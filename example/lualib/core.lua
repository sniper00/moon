---@class core
local core = require("moon_core")
if core then
    return core
end

--------------------FOR EMMYLUA HINT---------------------

--- for luacheck warn: unused params
local ignore_param = print

core = {}

---Remove a timer.<br>
---@param timerid int
function core.remove_timer(timerid)
    ignore_param(timerid)
end

---Pause all timer.<br>
function core.pause_timer()
    -- body
end

---Start all timer.<br>
function core.start_all_timer()
    -- body
end

---Return current millsecond.<br>
---@return int
function core.millsecond()
    -- body
end

---Thread sleep millsecond.<br>
---@param milliseconds int
function core.sleep(milliseconds)
    ignore_param(milliseconds)
end

---@param data string
---@return string
function core.hash_string(data)
    ignore_param(data)
end

---@param data string
---@return string
function core.hex_string(data)
    ignore_param(data)
end

---@param dir string
---@param depth int
---@param callback function
function core.traverse_folder(dir,depth,callback)
    ignore_param(dir,depth,callback)
end

---@param dir string
function core.exists(dir)
    ignore_param(dir)
end

---@param dir string
function core.create_directory(dir)
    ignore_param(dir)
end

---@return string
function core.current_directory()
    -- body
end

---@return string
function core.name()
    -- body
end

---@return int
function core.id()
    -- body
end

---@param receiver int
---@param cacheid int
---@param header string
---@param responseid int
---@param type int
function core.send_prepare(receiver,cacheid,header,responseid,type)
    ignore_param(receiver,cacheid,header,responseid,type)
end

---@overload fun(buf:string)
---@param buf userdata
function core.prepare(buf)
    ignore_param(buf)
end

---@param protocol string
function core.get_tcp(protocol)
    ignore_param(protocol)
end

---@param name string
function core.remove_component(name)
    ignore_param(name)
end

---@param cmd string
---@param callback fun(params:table)
function core.register_command(cmd,callback)
    ignore_param(cmd,callback)
end

---@return int
function core.memory_use()
    -- body
end

---@return int
function core.workernum()
    -- body
end

---@param key string
---@param value string
function core.set_env(key,value)
    ignore_param(key,value)
end

function core.abort()
    -- body
end

---@type core.path
core.path = {}

---@class core.path
local path = {}
ignore_param(path)

---@param fp string
---@return string
function path.parent_path(fp)
    ignore_param(fp)
end

---@param fp string
---@return string
function path.filename(fp)
    ignore_param(fp)
end

---@param fp string
---@return string
function path.extension(fp)
    ignore_param(fp)
end

---@param fp string
---@return string
function path.root_path(fp)
    ignore_param(fp)
end

---@param fp string
---@return string
function path.stem(fp)
    ignore_param(fp)
end

---@type core.message
core.message = {}

---@class core.message
local message = {}
ignore_param(message)
---@return int
function message:sender()
    ignore_param(self)
end

---@return int
function message:responseid()
    ignore_param(self)
end

---@return int
function message:receiver()
    ignore_param(self)
end

---@return int
function message:type()
    ignore_param(self)
end

---@return int
function message:subtype()
    ignore_param(self)
end

---@return string
function message:header()
    ignore_param(self)
end

---@return string
function message:bytes()
    ignore_param(self)
end

---@return int
function message:size()
    ignore_param(self)
end

---@param pos int
---@param count int
---@return string
function message:substr(pos,count)
    ignore_param(self,pos,count)
end

---@return userdata
function message:buffer()
    ignore_param(self)
end

---@param header string
---@param receiver int
---@param mtype int
function message:redirect(header, receiver, mtype)
    ignore_param(self,header, receiver, mtype)
end

---@param sender int
---@param header string
---@param receiver int
---@param responseid int
---@param mtype int
function message:resend(sender, header, receiver, responseid, mtype)
    ignore_param(self,sender, header, receiver, responseid, mtype)
end

return core
