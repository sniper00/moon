---@meta

error("DO NOT REQUIRE THIS FILE")

---@class crypt
local crypt = {}

---@enum crypt.padding
crypt.padding = {
	iso7816_4 = 0,
	pkcs7 = 1,
}

---@param key string
---@return string
function crypt.hashkey(key) end

---@return string
function crypt.randomkey() end

---@param key string
---@param text string
---@param padding crypt.padding?
---@return string
function crypt.desencode(key, text, padding) end

---@param key string
---@param text string
---@param padding crypt.padding?
---@return string
function crypt.desdecode(key, text, padding) end

---@param text string
---@return string
function crypt.hexencode(text) end

---@param hex string
---@return string
function crypt.hexdecode(hex) end

---@param a string
---@param b string
---@return string
function crypt.hmac64(a, b) end

---@param a string
---@param b string
---@return string
function crypt.hmac64_md5(a, b) end

---@param text string
---@return string
function crypt.dhexchange(text) end

---@param a string
---@param b string
---@return string
function crypt.dhsecret(a, b) end

---@param text string
---@return string
function crypt.base64encode(text) end

---@param base64 string
---@return string
function crypt.base64decode(base64) end

---@param text string
---@return string
function crypt.sha1(text) end

---@param key string
---@param text string
---@return string
function crypt.hmac_sha1(key, text) end

---@param key string
---@param text string
---@return string
function crypt.hmac_hash(key, text) end

---@param a string
---@param b string
---@return string
function crypt.xor_str(a, b) end

return crypt
