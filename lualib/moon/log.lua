require("base.string")
local c = require("mooncore")

local strfmt     = string.format
local dbgtrace   = debug.traceback
local strsplit   = string.split
local strtrim    = string.trim
local tbconcat   = table.concat

local error = error

local M = {}

---------------------------------------------------------------------------
-- log level
M.LOG_ERROR = 1
M.LOG_WARN = 2
M.LOG_INFO = 3
M.LOG_DEBUG = 4

-- 设置当前日志等级,低于这个等级的日志不会输出
M.LOG_LEVEL = c.get_loglevel

M.DEBUG = function ()
    return c.get_loglevel() == M.LOG_DEBUG
end

-----------------------------------------------------------------------
function M.throw(fmt, ...)
    local msg
    if #{...} == 0 then
        msg = fmt
    else
        msg = strfmt(fmt, ...)
    end
    if M.DEBUG then
        error(msg, 2)
    else
        error(msg, 0)
    end
end

local logV = c.LOGV

local function do_log(bstdout,level,fmt, ... )
    local str = strfmt(fmt, ...)
    local traceback = strsplit(dbgtrace("", 3), "\n")
    logV(bstdout, level, tbconcat({str,"(",strtrim(traceback[2]),")"}),c.id())
end

function M.error(fmt, ...)
    do_log(true, M.LOG_ERROR,fmt,...)
end

function M.warn(fmt, ...)
    do_log(true, M.LOG_WARN,fmt,...)
end

function M.debug(fmt, ...)
    if M.LOG_DEBUG <= M.LOG_LEVEL() then
        do_log(true, M.LOG_DEBUG,fmt,...)
    end
end

function M.info(fmt, ...)
    if M.LOG_INFO <= M.LOG_LEVEL() then
        do_log(true, M.LOG_INFO,fmt,...)
    end
end

return M
