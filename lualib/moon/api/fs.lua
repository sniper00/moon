---@meta

---@class fs
local fs = {}

--- Returns a table containing the names of the entries in the directory given by path recursive.
--- The table is in arbitrary order. It does not include the special entries '.' and '..'
--- even if they are present in the directory.
---@param dir string @ path of the directory
---@param depth? integer @ recursive depth, default 0
---@param ext? string @ filter by file extension
---@return table @ array table containing file/directory paths
function fs.listdir(dir, depth, ext) end

--- Check if the given path is a directory
---@param dir string @ path to check
---@return boolean @ true if path is a directory, false otherwise
function fs.isdir(dir) end

--- Join one or more path components into a single path
---@param ... string @ path components to join
---@return string @ joined path string
function fs.join(...) end

--- Check if the given path exists (file or directory)
---@param dir string @ path to check
---@return boolean @ true if path exists, false otherwise
function fs.exists(dir) end

--- Create directory and all parent directories if they don't exist
---@param dir string @ directory path to create
---@return boolean @ true if directory was created successfully, false otherwise
function fs.mkdir(dir) end

--- Remove file or directory
---@param fp string @ file or directory path to remove
---@param all? boolean @ if true, remove directory and all contents recursively
---@return boolean @ true if removal was successful, false otherwise
function fs.remove(fp, all) end

--- Returns a string with the current working directory
---@return string @ current working directory path
function fs.cwd() end

--- Split a file path into directory name and base name
---@param fp string @ file path to split
---@return string,string @ dirname, basename
function fs.split(fp) end

--- Get the file extension from a file path
---@param fp string @ file path
---@return string @ file extension (including the dot)
function fs.ext(fp) end

--- Get the root path component from a file path
---@param fp string @ file path
---@return string @ root path component
function fs.root(fp) end

--- Get filename without extension
--- e.g. "/data/server/program1.log" returns 'program1'
---@param fp string @ file path
---@return string @ filename without extension
function fs.stem(fp) end

--- Get the absolute path of a file or directory
---@param fp string @ file or directory path
---@return string @ absolute path
function fs.abspath(fp) end

return fs