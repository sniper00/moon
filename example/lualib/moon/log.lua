local c = require("moon_core")
local string_format     = string.format
local debug_traceback   = debug.traceback
local string_split      = string.split
local string_trim       = string.trim
local M = {}

---------------------------------------------------------------------------
-- log level
M.LOG_ERROR = 1
M.LOG_WARN = 2
M.LOG_INFO = 3
M.LOG_DEBUG = 4

-- 设置当前日志等级,低于这个等级的日志不会输出
M.LOG_LEVEL = M.LOG_DEBUG

-----------------------------------------------------------------------
function M.throw(fmt, ...)
    local msg
    if #{...} == 0 then
        msg = fmt
    else
        msg = string_format(fmt, ...)
    end
    if M.DEBUG then
        error(msg, 2)
    else
        error(msg, 0)
    end
end

local logV = c.LOGV

local function do_log(bcle,level,fmt, ... )
    local str = string_format(fmt, ...)
    local traceback = string_split(debug_traceback("", 3), "\n")
    logV(bcle, level, table.concat({str,"(",string_trim(traceback[2]),")"}),c.id())
end

function M.error(fmt, ...)
    do_log(true, M.LOG_ERROR,fmt,...)
end

function M.warn(fmt, ...)
    do_log(true, M.LOG_WARN,fmt,...)
end

function M.debug(fmt, ...)
    if M.LOG_DEBUG <= M.LOG_LEVEL then
        do_log(false, M.LOG_DEBUG,fmt,...)
    end
end

function M.info(fmt, ...)
    if M.LOG_INFO <= M.LOG_LEVEL then
        do_log(true, M.LOG_INFO,fmt,...)
    end
end

return M
