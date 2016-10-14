local protobuf = require("protobuf")

function LoadProtocol()
    local curdir = Path.GetCurrentDir()
    Path.TraverseFolder(curdir.."/Protocol",0,function (filepath,filetype)
        if filetype == 1 then
            if Path.GetExtension(filepath) == ".pb" then
                Log.ConsoleTrace("LoadProtocol:%s",filepath)
                local p = io.open(filepath,"rb")
                local buffer = p:read "*a"
                p:close()
                protobuf.register(buffer)
            end
        end
        return true
    end)
end