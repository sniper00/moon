local moon = require("moon")

local M= {
}

local clusterd

function M.call( rnode,rservice,...)
    if not clusterd then
        clusterd = moon.unique_service("clusterd")
        if 0 == clusterd then
            error("clusterd service not start")
        end
    end
    --print("cluster call ",clusterd)
    return moon.co_call('lua',clusterd,'CALL',rnode,rservice,...)
end

return M