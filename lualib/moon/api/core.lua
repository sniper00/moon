---@meta

--- lightuserdata, cpp type `buffer*`
---@class buffer_ptr

---Represents a pointer to a C++ message object
---@class message_ptr

--- userdata buffer_shr_ptr
---@class buffer_shr_ptr

--- lightuserdata, cpp type `char*`
---@class cstring_ptr

---@class core
---@field public id integer @ service's id
---@field public name string @ service's name
---@field public timezone integer @ server's local timezone
local core = {}

--- Get high-precision time in seconds
---@return number @ Current time in seconds (floating-point)
---@nodiscard
function core.clock() end

--- Calculate MD5 hash of a string
---@param data string @ Input string to hash
---@return string @ MD5 hash as lowercase hexadecimal string
---@nodiscard
function core.md5(data) end

--- Convert C string pointer to Lua string
---@param sz cstring_ptr @ C string pointer (char*)
---@param len integer @ Length of the string in bytes
---@return string @ Converted Lua string
---@nodiscard
function core.tostring(sz, len) end

---@alias server_stats_options
---|>'service.count'      # return total services count
---| 'log.error'         # return log error count

--- Get server statistics information
---@param opt? server_stats_options @ Specific statistic to retrieve, nil for all
---@return string @ Server statistics in JSON format
---@nodiscard
function core.server_stats(opt) end

--- Trigger memory collection (if mimalloc is available)
---@param force boolean @ If true, forces immediate collection; if false, suggests collection
function core.collect(force) end

--- Print console log with specified level
---@param loglv string @ Log level: "DEBUG", "INFO", "WARN", or "ERROR"
---@param ... any @ Values to log (will be converted to strings)
function core.log(loglv,...) end

--- Set or get current log level
---@param lv? string @ Log level to set: "DEBUG", "INFO", "WARN", "ERROR"
---@return integer @ Current log level as numeric value
function core.loglevel(lv) end

--- Get this service's CPU usage time
---@return integer @ CPU time consumed by this service
---@nodiscard
function core.cpu() end

--- Remove/kill a service
---@param addr integer|string @ Service ID or name to terminate
function core.kill(addr) end

--- Query unique service address by name
---@param name string @ Unique service name to look up
---@return integer|nil @ Service ID if found, nil otherwise
---@nodiscard
function core.queryservice(name) end

--- Generate next sequence number
---@return integer @ Unique sequence number
---@nodiscard
function core.next_sequence() end

--- Set or get environment variable
---@param key string @ Environment variable name
---@param value? string @ Value to set (if provided)
---@return string @ Current value of the environment variable
function core.env(key, value) end

--- Shutdown the server with exit code
---@param exitcode integer @ Exit code (>=0 for graceful, <0 for immediate)
function core.exit(exitcode) end

--- Adjust server time offset
---@param milliseconds integer @ Time adjustment in milliseconds (can be negative)
function core.adjtime(milliseconds) end

--- Get server timestamp in milliseconds
---@return integer @ Current server time in milliseconds since Unix epoch
---@nodiscard
function core.now() end

--- Decode message fields based on format pattern
---@param msg message_ptr @ Message object pointer from C++
---@param pattern string @ Format string specifying which fields to extract
---@return ... @ number of values based on pattern length
---@nodiscard
function core.decode(msg, pattern) end

--- Redirect a message to another service
---@param msg message_ptr @ Message object to redirect
---@param receiver integer @ Target service ID to receive the message
---@param mtype? integer @ New message type (PTYPE), nil to keep original
---@param sender? integer @ New sender service ID, nil to keep original
---@param sessionid? integer @ New session ID for correlation, nil to keep original
function core.redirect(msg, receiver, mtype, sender, sessionid) end

--- Escape non-printable characters to hexadecimal
---@param str string @ Input string with potential non-printable characters
---@return string @ String with non-printable chars as hex codes
---@nodiscard
function core.escape_print(str) end

--- Send signal to worker thread
---@param wid integer @ Worker thread ID [1, THREAD_NUM]
---@param val integer @ Signal value (0 = break loop, >0 = custom signal)
function core.signal(wid, val) end

---@class asio
local asio = {}

--- Check if a port is bindable or connectable
---@param host string @ IP address or hostname to test
---@param port integer @ Port number to test (1-65535)
---@param is_connect? boolean @ If true, test connection; if false/nil, test binding (default: false)
---@return boolean @ True if port is bindable/connectable, false otherwise
---@nodiscard
function asio.try_open(host, port, is_connect) end

--- Create a network listener
---@param host string @ IP address to bind to ("0.0.0.0" for all interfaces)
---@param port integer @ Port number to listen on (1-65535)
---@param protocol integer @ Protocol type (moon.PTYPE_SOCKET_*)
---@return integer @ File descriptor of the listening socket
function asio.listen(host, port, protocol) end

--- Send data to a socket
---@param fd integer @ Socket file descriptor
---@param data string|buffer_ptr|buffer_shr_ptr @ Data to send
---@param mask? integer @ Optional mask for send options (protocol-dependent)
---@return boolean @ True if data was queued successfully, false otherwise
function asio.write(fd, data, mask) end

--- Send a message object to a socket
---@param fd integer @ Socket file descriptor
---@param m message_ptr @ Message object to send
---@return boolean @ True if message was queued successfully, false otherwise
function asio.write_message(fd, m) end

--- Set read timeout for a socket
---@param fd integer @ Socket file descriptor
---@param t integer @ Timeout in seconds (0 = no timeout)
---@return boolean @ True if timeout was set successfully
function asio.settimeout(fd, t) end

--- Disable Nagle's algorithm for a socket
---@param fd integer @ Socket file descriptor
---@return boolean @ True if TCP_NODELAY was set successfully
function asio.setnodelay(fd) end

---@alias chunkmode
---| 'r' # read.
---| 'w' # write.
---| 'rw'
---| 'wr'

--- Enable chunked mode for Moon protocol sockets
---@param fd integer @ Socket file descriptor
---@param mode chunkmode @ Chunk mode
---@return boolean @ True if chunked mode was enabled
function asio.set_enable_chunked(fd, mode) end

--- Set send queue warning/error limits
---@param fd integer @ Socket file descriptor
---@param warnsize integer @ Warning threshold (bytes)
---@param errorsize integer @ Error threshold (bytes)
---@return boolean @ True if limits were set successfully
function asio.set_send_queue_limit(fd, warnsize, errorsize) end

--- Get socket address as string
---@param fd integer @ Socket file descriptor
---@return string @ Address string
function asio.getaddress(fd) end

--- Open UDP socket
---@param address string @ IP address to bind to
---@param port integer @ Port number
---@return integer @ UDP socket file descriptor
function asio.udp(address, port) end

--- Connect UDP socket to remote address
---@param fd integer @ UDP socket file descriptor
---@param address string @ Remote IP address
---@param port integer @ Remote port number
---@return boolean @ True if connection was successful
function asio.udp_connect(fd, address, port) end

--- Send data to specific address via UDP socket
---@param fd integer @ UDP socket file descriptor
---@param address string @ Destination address
---@param data string|buffer_ptr|buffer_shr_ptr @ Data to send
---@return boolean @ True if data was sent successfully
function asio.send_to(fd, address, data) end

--- Make endpoint from address and port
---@param address string @ IP address
---@param port integer @ Port number
---@return string @ Encoded endpoint string
function asio.make_endpoint(address, port) end

--- Unpack UDP packet
---@param data string|buffer_ptr @ UDP packet data
---@param size? integer @ Data size (if using buffer_ptr)
---@return string @ Address
---@return string @ Payload data
function asio.unpack_udp(data, size) end

return core
