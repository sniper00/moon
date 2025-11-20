---@meta

---@class crypt
local crypt = {}

---@enum crypt.padding
---@field iso7816_4 integer @ ISO 7816-4 padding mode (0)
---@field pkcs7 integer @ PKCS#7 padding mode (1)
crypt.padding = {
	iso7816_4 = 0,  -- ISO 7816-4 padding mode
	pkcs7 = 1,      -- PKCS#7 padding mode
}

---Convert a string to an 8-byte DES key
---Uses a combination of DJB hash and JS hash algorithms to generate the key
---@param key string @ Input string to hash
---@return string @ 8-byte DES key
function crypt.hashkey(key) end

---Generate an 8-byte random DES key
---Uses system random number generator, ensures key is not zero
---@return string @ 8-byte random key
function crypt.randomkey() end

---Encrypt data using DES algorithm
---Supports both ISO 7816-4 and PKCS#7 padding modes
---@param key string @ 8-byte DES key
---@param text string @ Plaintext data to encrypt
---@param padding crypt.padding? @ Padding mode, defaults to iso7816_4
---@return string @ Encrypted data (length is multiple of 8)
function crypt.desencode(key, text, padding) end

---Decrypt data using DES algorithm
---Supports both ISO 7816-4 and PKCS#7 padding modes
---@param key string @ 8-byte DES key
---@param text string @ Ciphertext data to decrypt (length must be multiple of 8)
---@param padding crypt.padding? @ Padding mode, defaults to iso7816_4
---@return string @ Decrypted plaintext data
function crypt.desdecode(key, text, padding) end

---Convert binary data to hexadecimal string
---Each byte is converted to two hexadecimal characters
---@param text string @ Binary data
---@return string @ Hexadecimal string
function crypt.hexencode(text) end

---Convert hexadecimal string to binary data
---Every two hexadecimal characters are converted to one byte
---@param hex string @ Hexadecimal string (length must be even)
---@return string @ Binary data
function crypt.hexdecode(hex) end

---Calculate HMAC64 hash of two 8-byte data blocks
---Uses simplified MD5 algorithm, result is 8 bytes long
---@param a string @ First 8-byte data block
---@param b string @ Second 8-byte data block
---@return string @ 8-byte HMAC64 hash value
function crypt.hmac64(a, b) end

---Calculate HMAC64-MD5 hash of two 8-byte data blocks
---Uses standard MD5 algorithm, result is 8 bytes long
---@param a string @ First 8-byte data block
---@param b string @ Second 8-byte data block
---@return string @ 8-byte HMAC64-MD5 hash value
function crypt.hmac64_md5(a, b) end

---Perform Diffie-Hellman key exchange
---Uses fixed generator g=5 and prime p=2^64-59
---@param text string @ 8-byte private key
---@return string @ 8-byte public key
function crypt.dhexchange(text) end

---Calculate Diffie-Hellman shared secret
---Uses fixed generator g=5 and prime p=2^64-59
---@param a string @ 8-byte private key
---@param b string @ 8-byte public key
---@return string @ 8-byte shared secret
function crypt.dhsecret(a, b) end

---Convert binary data to Base64 encoded string
---Uses standard Base64 character set with padding support
---@param text string @ Binary data
---@return string @ Base64 encoded string
function crypt.base64encode(text) end

---Convert Base64 encoded string to binary data
---Automatically handles padding characters
---@param base64 string @ Base64 encoded string
---@return string @ Binary data
function crypt.base64decode(base64) end

---Calculate SHA1 hash value
---Returns 20-byte SHA1 hash
---@param text string @ Data to hash
---@return string @ 20-byte SHA1 hash value
function crypt.sha1(text) end

---Calculate HMAC-SHA1 hash value
---Uses standard HMAC algorithm with SHA1 hash function
---@param key string @ Secret key
---@param text string @ Data to hash
---@return string @ 20-byte HMAC-SHA1 hash value
function crypt.hmac_sha1(key, text) end

---Calculate HMAC hash value
---Uses 8-byte key with custom hash algorithm
---@param key string @ 8-byte secret key
---@param text string @ Data to hash
---@return string @ 8-byte HMAC hash value
function crypt.hmac_hash(key, text) end

---Perform XOR operation on two strings
---If the second string is shorter, it will be reused cyclically
---@param a string @ First string
---@param b string @ Second string (cannot be empty)
---@return string @ XOR result string
function crypt.xor_str(a, b) end

return crypt
