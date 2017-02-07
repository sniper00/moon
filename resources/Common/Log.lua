
Log = { }

local logTag = ""

Log.SetTag = function(tag)
    logTag = tag
end

Log.Trace = function(fmt, ...)
    local arg = { ...}
    LOGV(false, LogLevel.Trace, logTag, string.format(fmt, table.unpack(arg)) or "")
end

Log.Debug = function(fmt, ...)
    local arg = { ...}
    LOGV(false, LogLevel.Debug, logTag, string.format(fmt, table.unpack(arg)) or "")
end

Log.Info = function(fmt, ...)
    local arg = { ...}
    LOGV(false, LogLevel.Info, logTag, string.format(fmt, table.unpack(arg)) or "")
end

Log.Warn = function(fmt, ...)
    local arg = { ...}
    LOGV(false, LogLevel.Warn, logTag, string.format(fmt, table.unpack(arg)) or "")
end

Log.Error = function(fmt, ...)
    local arg = { ...}
    LOGV(false, LogLevel.Error, logTag, string.format(fmt, table.unpack(arg)) or "")
end

Log.ConsoleTrace = function(fmt, ...)
    local arg = { ...}
    LOGV(true, LogLevel.Trace, logTag, string.format(fmt, table.unpack(arg)) or "")
end

Log.ConsoleDebug = function(fmt, ...)
    local arg = { ...}
    LOGV(true, LogLevel.Debug, logTag, string.format(fmt, table.unpack(arg)) or "")
end

Log.ConsoleInfo = function(fmt, ...)
    local arg = { ...}
    LOGV(true, LogLevel.Info, logTag, string.format(fmt, table.unpack(arg)) or "")
end

Log.ConsoleWarn = function(fmt, ...)
    local arg = { ...}
    LOGV(true, LogLevel.Warn, logTag, string.format(fmt, table.unpack(arg)) or "")
end

Log.ConsoleError = function(fmt, ...)
    local arg = { ...}
    LOGV(true, LogLevel.Error, logTag, string.format(fmt, table.unpack(arg)) or "")
end

Log.TraceTag = function(tag, fmt, ...)
    local arg = { ...}
    LOGV(false, LogLevel.Trace, tag, string.format(fmt, table.unpack(arg)) or "")
end

Log.DebugTag = function(tag, fmt, ...)
    local arg = { ...}
    LOGV(false, LogLevel.Debug, tag, string.format(fmt, table.unpack(arg)) or "")
end

Log.InfoTag = function(tag, fmt, ...)
    local arg = { ...}
    LOGV(false, LogLevel.Info, tag, string.format(fmt, table.unpack(arg)) or "")
end

Log.WarnTag = function(tag, fmt, ...)
    local arg = { ...}
    LOGV(false, LogLevel.Warn, tag, string.format(fmt, table.unpack(arg)) or "")
end

Log.ErrorTag = function(tag, fmt, ...)
    local arg = { ...}
    LOGV(false, LogLevel.Error, tag, string.format(fmt, table.unpack(arg)) or "")
end

Log.ConsoleTraceTag = function(tag, fmt, ...)
    local arg = { ...}
    LOGV(true, LogLevel.Trace, tag, string.format(fmt, table.unpack(arg)) or "")
end

Log.ConsoleDebugTag = function(tag, fmt, ...)
    local arg = { ...}
    LOGV(true, LogLevel.Debug, tag, string.format(fmt, table.unpack(arg)) or "")
end

Log.ConsoleInfoTag = function(tag, fmt, ...)
    local arg = { ...}
    LOGV(true, LogLevel.Info, tag, string.format(fmt, table.unpack(arg)) or "")
end

Log.ConsoleWarnTag = function(tag, fmt, ...)
    local arg = { ...}
    LOGV(true, LogLevel.Warn, tag, string.format(fmt, table.unpack(arg)) or "")
end

Log.ConsoleErrorTag = function(tag, fmt, ...)
    local arg = { ...}
    LOGV(true, LogLevel.Error, tag, string.format(fmt, table.unpack(arg)) or "")
end