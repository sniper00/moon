local strbyte       = string.byte
local strsub        = string.sub
local strlen        = string.len

--
-- Write content to a new file.
--
function io.writefile(filename, content)
    local file = io.open(filename, "w+b")
    if file then
        file:write(content)
        file:close()
        return true
    end
end

--
-- Read content from new file.
--
function io.readfile(filename)
    local file, err = io.open(filename, "rb")
    if file then
        local content = file:read("*a")
        file:close()
        return content
    end
    return false, err
end

function io.pathinfo(path)
    local pos = strlen(path)
    local extpos = pos + 1
    while pos > 0 do
        local b = strbyte(path, pos)
        if b == 46 then -- 46 = char "."
            extpos = pos
        elseif b == 47 then -- 47 = char "/"
            break
        end
        pos = pos - 1
    end

    local dirname  = strsub(path, 1, pos)
    local filename = strsub(path, pos + 1)

    extpos = extpos - pos
    local basename = strsub(filename, 1, extpos - 1)
    local extname  = strsub(filename, extpos)

    return {
        dirname  = dirname,
        filename = filename,
        basename = basename,
        extname  = extname
    }
end