/*
    PBKDF2-HMAC-SHA256 implementation
    Based on RFC 2898 (PKCS #5: Password-Based Cryptography Specification Version 2.0)
*/
#pragma once

#include "hmac_sha256.hpp"
#include <cmath> // For ceil
#include <cstdint>
#include <limits> // For numeric_limits
#include <stdexcept> // For std::runtime_error
#include <string>
#include <vector>

namespace moon {
namespace crypto {

    // PBKDF2-HMAC-SHA256 function
    inline std::vector<uint8_t> pbkdf2_hmac_sha256(
        const uint8_t* password,
        size_t password_len,
        const uint8_t* salt,
        size_t salt_len,
        uint32_t iterations,
        size_t dkLen // Desired key length in bytes
    ) {
        if (iterations == 0) {
            throw std::runtime_error("PBKDF2 iterations count cannot be zero.");
        }
        if (dkLen == 0) {
            throw std::runtime_error("PBKDF2 desired key length cannot be zero.");
        }

        size_t hLen = moon::crypto::detail::SHA256_DIGEST_SIZE; // Output length of HMAC-SHA256
        size_t l = static_cast<size_t>(std::ceil(static_cast<double>(dkLen) / hLen)
        ); // Number of blocks needed
        size_t r = dkLen - (l - 1) * hLen; // Length of the last block

        if (l > std::numeric_limits<uint32_t>::max()) {
            throw std::runtime_error("PBKDF2 desired key length is too large.");
        }

        std::vector<uint8_t> derived_key(dkLen);
        std::vector<uint8_t> salt_with_counter = std::vector<uint8_t>(salt, salt + salt_len);
        salt_with_counter.resize(salt_len + 4); // Reserve space for the 4-byte counter (big-endian)

        size_t dk_offset = 0;
        for (uint32_t i = 1; i <= l; ++i) {
            // Prepare salt || INT(i)
            salt_with_counter[salt_len] = static_cast<uint8_t>((i >> 24) & 0xFF);
            salt_with_counter[salt_len + 1] = static_cast<uint8_t>((i >> 16) & 0xFF);
            salt_with_counter[salt_len + 2] = static_cast<uint8_t>((i >> 8) & 0xFF);
            salt_with_counter[salt_len + 3] = static_cast<uint8_t>(i & 0xFF);

            // Calculate F(Password, Salt, c, i) = U1 ^ U2 ^ ... ^ Uc
            // U1 = PRF(Password, Salt || INT(i))
            std::array<uint8_t, detail::SHA256_DIGEST_SIZE> u = hmac_sha256(
                password,
                password_len,
                salt_with_counter.data(),
                salt_with_counter.size()
            );
            std::array<uint8_t, detail::SHA256_DIGEST_SIZE> f = u; // Initialize F with U1

            // Calculate U2..Uc and XOR into F
            for (uint32_t j = 1; j < iterations; ++j) {
                // Uj = PRF(Password, Uj-1)
                u = hmac_sha256(password, password_len, u.data(), u.size());
                // F = F ^ Uj
                for (size_t k = 0; k < hLen; ++k) {
                    f[k] ^= u[k];
                }
            }

            // Copy the result F into the derived key buffer
            size_t copy_len = (i == l) ? r : hLen; // Length to copy for this block
            std::memcpy(derived_key.data() + dk_offset, f.data(), copy_len);
            dk_offset += copy_len;
        }

        return derived_key;
    }

    // Overloads for convenience
    inline std::vector<uint8_t> pbkdf2_hmac_sha256(
        const std::string& password,
        const std::vector<uint8_t>& salt,
        uint32_t iterations,
        size_t dkLen
    ) {
        return pbkdf2_hmac_sha256(
            reinterpret_cast<const uint8_t*>(password.data()),
            password.size(),
            salt.data(),
            salt.size(),
            iterations,
            dkLen
        );
    }

    inline std::vector<uint8_t> pbkdf2_hmac_sha256(
        const std::string& password,
        const std::string& salt, // Assuming salt is raw bytes in a string
        uint32_t iterations,
        size_t dkLen
    ) {
        return pbkdf2_hmac_sha256(
            reinterpret_cast<const uint8_t*>(password.data()),
            password.size(),
            reinterpret_cast<const uint8_t*>(salt.data()),
            salt.size(),
            iterations,
            dkLen
        );
    }

} // namespace crypto
} // namespace moon
