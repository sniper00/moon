local log = require("moon.log")

do
    local ok = pcall(log.throw, "log throw")
    if not ok then
        print("log.throw")
    end
end

do
    local tb = {a=1,b=100,c="dadadad"}
    log.dump(tb)
end

do
    log.error("log error %s","balaba")
end

do
    log.warn("log warn")
end

do
    log.debug("log debug")
end

do
    log.info("log info")
end

log.LOG_LEVEL = log.LOG_WARN
log.info("this line will not print")
log.debug("this line will not print")
log.warn("this line will  print")
log.error("this line will  print")

log.LOG_LEVEL = log.LOG_TRACE
log.DEBUG = false
log.debug("this line will not print or log file")
