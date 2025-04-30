/*
    HMAC-SHA256 implementation
    Based on RFC 2104 (HMAC: Keyed-Hashing for Message Authentication)
*/
#pragma once

#include "sha256.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <numeric> // For std::iota
#include <string>
#include <vector>

namespace moon {
namespace crypto {

    namespace detail {

        // Helper to XOR data with a constant byte
        inline void
        xor_pad(const std::vector<uint8_t>& key, uint8_t pad_byte, std::vector<uint8_t>& result) {
            result.resize(SHA256_BLOCK_SIZE);
            size_t key_len = key.size();
            size_t copy_len = (key_len < SHA256_BLOCK_SIZE) ? key_len : SHA256_BLOCK_SIZE;

            // Copy key and XOR with pad_byte
            for (size_t i = 0; i < copy_len; ++i) {
                result[i] = key[i] ^ pad_byte;
            }
            // Fill remaining space with pad_byte
            for (size_t i = copy_len; i < SHA256_BLOCK_SIZE; ++i) {
                result[i] = pad_byte;
            }
        }

    } // namespace detail

    // Compute HMAC-SHA256
    inline std::array<uint8_t, detail::SHA256_DIGEST_SIZE>
    hmac_sha256(const uint8_t* key, size_t key_len, const uint8_t* msg, size_t msg_len) {
        std::vector<uint8_t> actual_key;

        // If key is longer than block size, hash it
        if (key_len > detail::SHA256_BLOCK_SIZE) {
            auto hashed_key = sha256(key, key_len);
            actual_key.assign(hashed_key.begin(), hashed_key.end());
        } else {
            actual_key.assign(key, key + key_len);
        }

        // Pad key to block size if shorter
        if (actual_key.size() < detail::SHA256_BLOCK_SIZE) {
            actual_key.resize(detail::SHA256_BLOCK_SIZE, 0x00);
        }

        std::vector<uint8_t> o_key_pad;
        std::vector<uint8_t> i_key_pad;

        detail::xor_pad(actual_key, 0x5c, o_key_pad); // Outer pad
        detail::xor_pad(actual_key, 0x36, i_key_pad); // Inner pad

        // Inner hash: H(i_key_pad || message)
        detail::sha256_ctx inner_ctx;
        detail::update(inner_ctx, i_key_pad.data(), i_key_pad.size());
        detail::update(inner_ctx, msg, msg_len);
        std::array<uint8_t, detail::SHA256_DIGEST_SIZE> inner_hash;
        detail::final(inner_ctx, inner_hash.data());

        // Outer hash: H(o_key_pad || inner_hash)
        detail::sha256_ctx outer_ctx;
        detail::update(outer_ctx, o_key_pad.data(), o_key_pad.size());
        detail::update(outer_ctx, inner_hash.data(), inner_hash.size());
        std::array<uint8_t, detail::SHA256_DIGEST_SIZE> final_hash;
        detail::final(outer_ctx, final_hash.data());

        return final_hash;
    }

    // Overloads for convenience
    inline std::array<uint8_t, detail::SHA256_DIGEST_SIZE>
    hmac_sha256(const std::vector<uint8_t>& key, const std::vector<uint8_t>& msg) {
        return hmac_sha256(key.data(), key.size(), msg.data(), msg.size());
    }

    inline std::array<uint8_t, detail::SHA256_DIGEST_SIZE>
    hmac_sha256(const std::string& key, const std::string& msg) {
        return hmac_sha256(
            reinterpret_cast<const uint8_t*>(key.data()),
            key.size(),
            reinterpret_cast<const uint8_t*>(msg.data()),
            msg.size()
        );
    }

} // namespace crypto
} // namespace moon
