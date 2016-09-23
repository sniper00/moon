
Log = {}

Log.Trace = function (fmt,...)
	local arg = { ... }
	LOGV(false,LogLevel.Trace,string.format(fmt,table.unpack(arg)) or "")
end

Log.Debug = function (fmt,...)
	local arg = { ... }
	LOGV(false,LogLevel.Debug,string.format(fmt,table.unpack(arg))or "")
end

Log.Info = function (fmt,...)
	local arg = { ... }
	LOGV(false,LogLevel.Info,string.format(fmt,table.unpack(arg))or "")
end

Log.Warn = function (fmt,...)
	local arg = { ... }
	LOGV(false,LogLevel.Warn,string.format(fmt,table.unpack(arg))or "")
end

Log.Error = function (fmt,...)
	local arg = { ... }
	LOGV(false,LogLevel.Error,string.format(fmt,table.unpack(arg))or "")
end


Log.ConsoleTrace = function (fmt,...)
	local arg = { ... }
	LOGV(true,LogLevel.Trace,string.format(fmt,table.unpack(arg))or "")
end

Log.ConsoleDebug = function (fmt,...)
	local arg = { ... }
	LOGV(true,LogLevel.Debug,string.format(fmt,table.unpack(arg))or "")
end

Log.ConsoleInfo = function (fmt,...)
	local arg = { ... }
	LOGV(true,LogLevel.Info,string.format(fmt,table.unpack(arg))or "")
end

Log.ConsoleWarn = function (fmt,...)
	local arg = { ... }
	LOGV(true,LogLevel.Warn,string.format(fmt,table.unpack(arg))or "")
end

Log.ConsoleError = function (fmt,...)
	local arg = { ... }
	LOGV(true,LogLevel.Error,string.format(fmt,table.unpack(arg))or "")
end
