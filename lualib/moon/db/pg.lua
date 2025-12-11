local moon         = require("moon")
local buffer       = require("buffer")
local scram_sha256 = require("crypto.scram")
local socket       = require("moon.socket")
local json         = require "json"

local concat       = buffer.concat
local bsize        = buffer.size
local wfront       = buffer.write_front
local wback        = buffer.write_back

local socket_error = setmetatable({}, { __tostring = function() return "[Error: socket]" end }) -- alias for error object

local md5          = moon.md5

local NULL         = "\000"

local insert       = table.insert

local strpack      = string.pack
local strunpack    = string.unpack
local strsub       = string.sub

local type         = type
local assert       = assert

local tointeger    = math.tointeger
local tonumber     = tonumber

local convert_null = true

local flipped
flipped = function(t)
    local flipped_t = {}
    for k, v in pairs(t) do
        flipped_t[v] = k
        flipped_t[k] = v
    end
    return flipped_t
end

local MSG_TYPE     =
    flipped(
        {
            status = "S",
            auth = "R",
            backend_key = "K",
            ready_for_query = "Z",
            query = "Q",
            notice = "N",
            notification = "A",
            password = "p",
            row_description = "T",
            data_row = "D",
            command_complete = "C",
            error = "E"
        }
    )

local ERROR_TYPES  =
    flipped(
        {
            severity = "S",
            code = "C",
            message = "M",
            position = "P",
            detail = "D",
            schema = "s",
            table = "t",
            constraint = "n"
        }
    )

local PG_TYPES     = {
    ---"boolean"
    [16] = function(value)
        return value == "t"
    end,
    -- [17] = "bytea",
    [20] = tointeger,
    [21] = tointeger,
    [23] = tointeger,
    [700] = tonumber,
    [701] = tonumber,
    [1700] = tonumber,
    -- [114] = "json",
    -- [3802] = "json",
    -- [1000] = "array_boolean",
    -- [1005] = "array_number",
    -- [1007] = "array_number",
    -- [1016] = "array_number",
    -- [1021] = "array_number",
    -- [1022] = "array_number",
    -- [1231] = "array_number",
    -- [1009] = "array_string",
    -- [1015] = "array_string",
    -- [1002] = "array_string",
    -- [1014] = "array_string",
    -- [2951] = "array_string",
    -- [199] = "array_json",
    -- [3807] = "array_json"
}

local function encode_int(n)
    return strpack(">i", n)
end

local function decode_int(s)
    return strunpack(">i", s)
end

local function decode_short(s)
    return strunpack(">H", s)
end

local function disconnect(self)
    if self.sock then
        socket.close(self.sock)
        self.sock = nil
    end
end

local function receive_message(self)
    if not self.sock then
        return socket_error, "not connect"
    end
    socket.settimeout(self.sock, 10)
    local header, err, content
    ---header: 1byte type + 4byte len
    header, err = socket.read(self.sock, 5)
    if not header then
        disconnect(self)
        return socket_error, "receive_message: failed to get header: " .. tostring(err)
    end
    local t, len = strunpack(">c1i", header)
    content, err = socket.read(self.sock, len - 4)
    if not content then
        disconnect(self)
        return socket_error, err
    end
    socket.settimeout(self.sock, 0)
    return t, content
end

local function send_message(self, t, data)
    local buf = concat(data)
    wfront(buf, t, strpack(">I", bsize(buf) + 4))
    socket.write(self.sock, buf)
end

local function send_startup_message(self)
    assert(self.user, "missing user for connect")
    assert(self.database, "missing database for connect")
    local startup_message = buffer.concat_string(
        encode_int(196608),
        "user", NULL, self.user, NULL,
        "database", NULL, self.database, NULL,
        "application_name", NULL, "moon", NULL,
        NULL
    )
    socket.write(self.sock, buffer.concat_string(encode_int(#startup_message + 4), startup_message))
end

local function parse_error(err_msg)
    local db_error = {}
    local offset = 1
    local err_len = #err_msg
    while offset < err_len do                                     -- Stop before the final null terminator
        local t = strsub(err_msg, offset, offset)                 -- Get the type code byte
        offset = offset + 1                                       -- Move past the type code

        local null_pos = string.find(err_msg, NULL, offset, true) -- Find the next null terminator
        if not null_pos then
            -- Malformed error message, stop parsing
            break
        end

        local str = strsub(err_msg, offset, null_pos - 1) -- Extract the value string
        offset = null_pos + 1                             -- Move past the null terminator for the next iteration

        local field = ERROR_TYPES[t]
        if field then
            db_error[field] = str
        end
    end
    return db_error
end

local function check_auth(self)
    local t, msg = receive_message(self)
    if t == socket_error then
        return socket_error, msg
    end

    if MSG_TYPE.error == t then
        return false, parse_error(msg)
    elseif MSG_TYPE.auth == t then
        -- Authentication message 'R' received. Check the status code.
        if #msg < 4 then
            return false, "Received truncated Authentication message (R)"
        end
        local auth_status = decode_int(msg:sub(1, 4))
        if auth_status == 0 then
            -- AuthenticationOk
            return true
        else
            -- Authentication failed (specific status code indicates reason, but we treat any non-zero as failure here)
            -- Attempt to parse the rest of the message as an error, although format isn't guaranteed
            local err_detail = "Authentication failed with status code: " .. auth_status
            -- Try parsing remaining msg as error fields if possible, otherwise just return the code
            if #msg > 4 then
                local potential_error = parse_error(msg:sub(5))
                if next(potential_error) then  -- Check if parse_error found anything
                    potential_error.message = potential_error.message or err_detail
                    return false, potential_error
                end
            end
            return false, { code = "AUTH", message = err_detail }
        end
    else
        -- Unexpected message type during final auth check
        return false, "Unknown or unexpected response during final authentication check: " .. t
    end
end

local function cleartext_auth(self)
    assert(self.password, "missing password, required for connect")
    send_message(
        self,
        MSG_TYPE.password,
        {
            self.password,
            NULL
        }
    )
    return check_auth(self)
end

local function md5_auth(self, msg)
    local salt = msg:sub(5, 8)
    assert(self.password, "missing password, required for connect")
    send_message(
        self,
        MSG_TYPE.password,
        {
            "md5",
            md5(md5(self.password .. self.user) .. salt),
            NULL
        }
    )
    return check_auth(self)
end

local function scram_sha_256_auth(self, msg)
    assert(self.password, "the database is requesting a password for authentication but you did not provide a password")

    if msg:match("SCRAM%-SHA%-256%-PLUS") then
        error("unsupported SCRAM mechanism name: " .. tostring(msg))
    elseif msg:match("SCRAM%-SHA%-256") then
    else
        error("unsupported SCRAM mechanism name: " .. tostring(msg))
    end

    -- 1. Create SCRAM client instance
    local scram_client = scram_sha256.new(self.user, self.password)

    -- 2. Prepare and send client-first-message
    local gs2_header = "n,,"
    local client_first_message = gs2_header .. scram_client:prepare_first_message()
    local mechanism_name = "SCRAM-SHA-256" .. NULL
    send_message(
        self,
        MSG_TYPE.password, -- SASLInitial message uses 'p' type
        {
            mechanism_name,
            encode_int(#client_first_message), -- Length of client_first_message
            client_first_message
        }
    )

    -- 3. Receive server-first-message (AuthenticationSASLContinue)
    local t, server_msg = receive_message(self)
    if t == socket_error then
        return socket_error, server_msg
    elseif t == MSG_TYPE.error then
        return false, parse_error(server_msg)
    elseif t ~= MSG_TYPE.auth then
        return false, "Unexpected message type during SCRAM auth (expected Auth/R): " .. t
    end

    -- Check AuthenticationSASLContinue indicator (11)
    local auth_indicator = strunpack(">i", server_msg)
    if auth_indicator ~= 11 then
        return false, "Unexpected SASL auth indicator (expected 11): " .. tostring(auth_indicator)
    end
    local server_first_message = server_msg:sub(5) -- Extract server-first data

    -- 4. Process server-first-message
    scram_client:process_server_first(server_first_message)

    -- 5. Prepare and send client-final-message (SASLResponse)
    local client_final_message = scram_client:prepare_final_message()
    -- Log the exact message being sent
    -- print(string.format("Sending ClientFinal Payload: [%s]", client_final_message))
    -- Wrap client_final_message in a table for send_message
    send_message(
        self,
        MSG_TYPE.password, -- SASLResponse message also uses 'p' type
        {
            client_final_message
        }
    )

    -- 6. Receive server-final-message (AuthenticationSASLFinal)
    t, server_msg = receive_message(self)
    if t == socket_error then
        return socket_error, server_msg
    elseif t == MSG_TYPE.error then
        return false, parse_error(server_msg)
    elseif t ~= MSG_TYPE.auth then
        return false, "Unexpected message type during SCRAM auth (expected Auth/R): " .. t
    end

    ---@cast server_msg string

    -- Check AuthenticationSASLFinal indicator (12)
    auth_indicator = strunpack(">i", server_msg)
    if auth_indicator ~= 12 then
        return false, "Unexpected SASL auth indicator (expected 12): " .. tostring(auth_indicator)
    end
    local server_final_message = server_msg:sub(5) -- Extract server-final data

    -- 7. Process server-final-message (verifies server signature)
    scram_client:process_server_final(server_final_message)

    -- 8. Check authentication status
    if not scram_client:is_authenticated() then
        return false, "SCRAM-SHA-256 authentication failed (client check)"
    end

    -- 9. Wait for AuthenticationOk (R message with 0)
    return check_auth(self) -- check_auth expects the final R(0) message
end

local function auth(self)
    local t, msg = receive_message(self)
    if t == socket_error then
        return socket_error, msg
    end
    if not (MSG_TYPE.auth == t) then
        disconnect(self)
        if MSG_TYPE.error == t then
            return false, parse_error(msg)
        end
        error("unexpected message during auth: " .. tostring(t))
    end
    local auth_type = strunpack(">i", msg)
    local _exp_0 = auth_type
    if 0 == _exp_0 then
        return true
    elseif 3 == _exp_0 then
        return cleartext_auth(self)
    elseif 5 == _exp_0 then
        return md5_auth(self, msg)
    elseif 10 == _exp_0 then
        return scram_sha_256_auth(self, msg)
    else
        return error(string.format("don't know how to auth, auth_type: %s.", auth_type))
    end
end

local function wait_until_ready(self)
    while true do
        local t, msg = receive_message(self)
        if t == socket_error then
            return t, msg
        end
        if MSG_TYPE.error == t then
            disconnect(self)
            return false, parse_error(msg)
        end
        if MSG_TYPE.ready_for_query == t then
            break
        end
    end
    return true
end

---@class pg_result
---@field public code? integer
---@field public data table @ table rows
---@field public num_queries integer
---@field public notifications integer
---@field public message string @ error message
---@field public errdata table @ error detail info

---@class pg
---@field public sock integer @ socket fd
---@field public code? integer
---@field public message? string
local pg = {}

pg.disconnect = disconnect

pg.__index = pg

pg.__gc = function(o)
    if o.sock then
        socket.close(o.sock)
    end
end

---@param opts table @{database = "", user = "", password = ""}
---@return pg
function pg.connect(opts)
    local sock, err = socket.connect(opts.host, opts.port, moon.PTYPE_SOCKET_TCP, opts.connect_timeout)
    if not sock then
        return { code = "SOCKET", message = err }
    end

    -- Shallow copy opts and add sock
    local obj = {}
    for k, v in pairs(opts) do
        obj[k] = v
    end
    obj.sock = sock

    send_startup_message(obj)

    local success

    success, err = auth(obj)
    if success == socket_error then
        return { code = "SOCKET", message = err }
    elseif not success then
        disconnect(obj) -- Ensure socket is closed on auth failure
        return type(err) == "table" and err or { code = "AUTH", message = err }
    end

    success, err = wait_until_ready(obj)
    if success == socket_error then
        return { code = "SOCKET", message = err }
    elseif not success then
        -- disconnect() is called within wait_until_ready on its error path already
        return type(err) == "table" and err or { code = "AUTH", message = err }
    end

    return setmetatable(obj, pg)
end

-- local json = require("json")


-- local type_deserializers = {
--     json = json.decode,
--     bytea = function(val)
--         return self:decode_bytea(val)
--     end,
--     array_boolean = function(self, val, name)
--         local decode_array
--         decode_array = require("pgmoon.arrays").decode_array
--         return decode_array(val, tobool)
--     end,
--     array_number = function(self, val, name)
--         local decode_array
--         decode_array = require("pgmoon.arrays").decode_array
--         return decode_array(val, tonumber)
--     end,
--     array_string = function(self, val, name)
--         local decode_array
--         decode_array = require("pgmoon.arrays").decode_array
--         return decode_array(val)
--     end,
--     array_json = function(self, val, name)
--         local decode_array
--         decode_array = require("pgmoon.arrays").decode_array
--         local decode_json
--         decode_json = require("pgmoon.json").decode_json
--         return decode_array(val, decode_json)
--     end,
--     hstore = function(self, val, name)
--         local decode_hstore
--         decode_hstore = require("pgmoon.hstore").decode_hstore
--         return decode_hstore(val)
--     end
-- }

local function parse_row_desc(row_desc)
    local num_fields = decode_short(row_desc:sub(1, 2))
    local offset = 3
    local fields = {}
    local row_desc_len = #row_desc
    for i = 1, num_fields do
        local null_pos = string.find(row_desc, NULL, offset, true) -- Find null terminator
        if not null_pos then
            error("Invalid row description format: missing null terminator for field name at offset " .. offset)
        end
        local name = strsub(row_desc, offset, null_pos - 1)
        offset = null_pos + 1 -- Move past the null terminator

        -- Ensure enough bytes remain for the rest of the field info (18 bytes)
        if offset + 17 > row_desc_len then
            error("Invalid row description format: not enough data for field info for field '" .. name .. "'")
        end

        -- table_id = decode_int(row_desc:sub(offset, offset + 3)) -- Not used
        -- column_id = decode_short(row_desc:sub(offset + 4, offset + 5)) -- Not used
        local data_type = decode_int(row_desc:sub(offset + 6, offset + 9))
        -- data_type_size = decode_short(row_desc:sub(offset + 10, offset + 11)) -- Not used
        -- type_modifier = decode_int(row_desc:sub(offset + 12, offset + 15)) -- Not used
        local format = decode_short(row_desc:sub(offset + 16, offset + 17))

        if 0 ~= format then
            -- Only text format (0) is handled currently
            error("don't know how to handle non-text format code: " .. format)
        end
        offset = offset + 18 -- Move past the 18-byte field info block
        -- Store name, type OID, and the pre-looked-up converter function
        local info = {
            name = name,
            type_oid = data_type,
            converter = PG_TYPES[data_type]
        }
        table.insert(fields, info)
    end
    return fields
end

local function parse_row_data(data_row, fields)
    local tupnfields = decode_short(data_row:sub(1, 2))
    if tupnfields ~= #fields then
        error('unexpected field count in \"D\" message')
    end

    local out = {}
    local offset = 3
    for i = 1, tupnfields do
        local field = fields[i] -- field contains {name=..., type_oid=..., converter=...}
        local field_name = field.name
        local len = decode_int(strsub(data_row, offset, offset + 3))
        offset = offset + 4 -- Move past length
        if len < 0 then     -- Handle NULL value
            if convert_null then
                out[field_name] = NULL
            end
            -- No data follows for NULL, so loop continues
        else
            local value = strsub(data_row, offset, offset + len - 1)
            offset = offset + len      -- Move past value data
            local fn = field.converter -- Use pre-looked-up converter
            if fn then
                value = fn(value)      -- Apply conversion if available
            end
            out[field_name] = value
        end
    end
    return out
end

local function parse_notification(msg)
    local pid = decode_int(msg:sub(1, 4))
    local offset = 4
    local channel, payload = msg:match("^([^%z]+)%z([^%z]*)%z$", offset + 1)
    if not (channel) then
        error("parse_notification: failed to parse notification")
    end
    return {
        operation = "notification",
        pid = pid,
        channel = channel,
        payload = payload
    }
end

local function format_query_result(row_desc, data_rows, command_complete)
    local command, affected_rows
    if command_complete then
        command = command_complete:match("^%w+")
        affected_rows = tointeger(command_complete:match("(%d+)%z$"))
    end
    if row_desc then
        if not (data_rows) then
            return {}
        end
        local fields = parse_row_desc(row_desc)
        local num_rows = #data_rows
        for i = 1, num_rows do
            data_rows[i] = parse_row_data(data_rows[i], fields)
        end
        if affected_rows and command ~= "SELECT" then
            data_rows.affected_rows = affected_rows
        end
        return data_rows
    end
    if affected_rows then
        return {
            affected_rows = affected_rows
        }
    else
        return true
    end
end

function pg.pack_query_buffer(buf)
    wfront(buf, MSG_TYPE.query, strpack(">I", bsize(buf) + 4 + #NULL))
    wback(buf, NULL)
end

local function read_result(self)
    local row_desc, data_rows, err_msg
    local result, notifications
    local num_queries = 0
    while true do
        local t, msg = receive_message(self)
        if t == socket_error then
            return { code = "SOCKET", message = msg }
        end
        local _exp_0 = t
        if MSG_TYPE.data_row == _exp_0 then
            data_rows = data_rows or {}
            insert(data_rows, msg)
        elseif MSG_TYPE.row_description == _exp_0 then
            row_desc = msg
        elseif MSG_TYPE.error == _exp_0 then
            err_msg = msg
        elseif MSG_TYPE.command_complete == _exp_0 then
            local next_result = format_query_result(row_desc, data_rows, msg)
            num_queries = num_queries + 1
            if num_queries == 1 then
                result = next_result
            elseif num_queries == 2 then
                result = {
                    result,
                    next_result
                }
            else
                insert(result, next_result)
            end
            row_desc = nil
            data_rows = nil
        elseif MSG_TYPE.ready_for_query == _exp_0 then
            break
        elseif MSG_TYPE.notification == _exp_0 then
            if not (notifications) then
                notifications = {}
            end
            insert(notifications, parse_notification(msg))
        end
    end
    if err_msg then
        local db_error = parse_error(err_msg)
        db_error.data = result
        db_error.num_queries = num_queries
        db_error.notifications = notifications
        return db_error
    end

    return {
        data = result,
        num_queries = num_queries,
        notifications = notifications
    }
end

function pg.pipe(self, req)
    if type(req) == "userdata" then
        -- assume it's a buffer_shr_ptr
        socket.write(self.sock, req)
    else
        socket.write(self.sock, json.pq_pipe(req))
    end
    return read_result(self)
end

function pg.query_params(self, sql, ...)
    local tt = type(sql)
    if tt == "userdata" then
        socket.write(self.sock, sql)
    elseif tt == "table" then
        socket.write(self.sock, json.pq_query(sql))
    else
        socket.write(self.sock, json.pq_query({ sql, ... }))
    end
    return read_result(self)
end

---@param sql buffer_shr_ptr|string
---@return pg_result
function pg.query(self, sql)
    if type(sql) == "string" then
        send_message(self, MSG_TYPE.query, { sql, NULL })
    else
        socket.write(self.sock, sql)
    end
    return read_result(self)
end

return pg
