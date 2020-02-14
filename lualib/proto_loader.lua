local protobuf = require("protobuf")
local fs = require("fs")
local reg_file      = protobuf.register_file

local M={}

function M.load(pbfile)
    local filepath = fs.working_directory()
    reg_file(filepath..pbfile)
end

function M.loadall(path)
    local curdir = fs.working_directory()
    fs.traverse_folder(curdir .. path, 0, function(filepath, isdir)
        if not isdir then
            if fs.extension(filepath) == ".pb" then
                print("LoadProtocol", filepath)
                reg_file(filepath)
            end
        end
        return true
    end)
end

return M
