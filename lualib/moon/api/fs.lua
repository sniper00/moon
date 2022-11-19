---@meta

error("DO NOT REQUIRE THIS FILE")

---@class fs
local fs = {}

--- Returns a table containing the names of the entries in the directory given by path recursive.
--- The table is in arbitrary order. It does not include the special entries '.' and '..'
--- even if they are present in the directory.
---@param dir string @path of the directory
---@param depth? integer @recursive depth, default 0
---@param ext? string @ filter by file extension
---@return table @ array table
function fs.listdir(dir, depth, ext) end

function fs.isdir(dir) end

---@param dir1 string
---@param dir2 string
---@return string
function fs.join(dir1, dir2) end

---@param dir string
function fs.exists(dir) end

---@param dir string
function fs.mkdir(dir) end

---@param fp string
---@param all? boolean
function fs.remove(fp, all) end

--- Returns a string with the current working directory
---@return string
function fs.cwd() end

---@param fp string
---@return string,string @ return dirname, basename
function fs.split(fp) end

---@param fp string
---@return string
function fs.ext(fp) end

---@param fp string
---@return string
function fs.root(fp) end

--- Get filename without extension
--- e. "/data/server/program1.log" return 'program1'
---@param fp string @ file path
---@return string
function fs.stem(fp) end

---@param fp string
---@return string
function fs.abspath(fp) end

return fs