error("DO NOT REQUIRE THIS FILE")

---@class fs
local fs = {}

---返回指定的文件夹包含的文件或文件夹的名字的列表
---@param dir string @路径
---@param depth integer @遍历子目录的深度，默认0
---@return table @返回数组
function fs.listdir(dir, depth)

end

function fs.isdir(dir)

end

function fs.join(dir1, dir2)

end

---@param dir string
function fs.exists(dir)

end

---@param dir string
function fs.mkdir(dir)

end

---@param fp string
---@param all boolean
function fs.remove(fp, all)

end

---@return string
function fs.cwd()
    -- body
end

---@param fp string
---@return string,string @ return dirname, basename
function fs.split(fp)

end

---@param fp string
---@return string
function fs.ext(fp)

end

---@param fp string
---@return string
function fs.root(fp)

end

---@param fp string
---@return string
function fs.stem(fp)

end

---@param fp string
---@return string
function fs.abspath(fp)

end

return fs