local protobuf = require("protobuf")
local fs = require("fs")
local reg_file      = protobuf.register_file

local M={}

function M.load(pbfile)
    local filepath = fs.working_directory()
    filepath = filepath.."/protocol/"..pbfile
    reg_file(filepath)
end

function M.loadall()
    local curdir = fs.working_directory()
    fs.traverse_folder(curdir .. "/protocol", 0, function(filepath, filetype)
        if filetype == 1 then
            if fs.extension(filepath) == ".pb" then
                --printf("LoadProtocol:%s", filepath)
                reg_file(filepath)
            end
        end
        return true
    end)
end

return M
