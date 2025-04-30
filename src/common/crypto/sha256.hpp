#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace moon {
namespace crypto {

    namespace detail {

        constexpr size_t SHA256_BLOCK_SIZE = 64; // 512 bits
        constexpr size_t SHA256_DIGEST_SIZE = 32; // 256 bits

        class sha256_ctx {
        public:
            uint8_t data[SHA256_BLOCK_SIZE];
            uint32_t datalen;
            uint64_t bitlen;
            uint32_t state[8];

            sha256_ctx() {
                reset();
            }

            void reset() {
                datalen = 0;
                bitlen = 0;
                state[0] = 0x6a09e667;
                state[1] = 0xbb67ae85;
                state[2] = 0x3c6ef372;
                state[3] = 0xa54ff53a;
                state[4] = 0x510e527f;
                state[5] = 0x9b05688c;
                state[6] = 0x1f83d9ab;
                state[7] = 0x5be0cd19;
                std::memset(data, 0, SHA256_BLOCK_SIZE);
            }
        };

        inline uint32_t rotl(uint32_t x, uint32_t n) {
            return (x << n) | (x >> (32 - n));
        }

        inline uint32_t rotr(uint32_t x, uint32_t n) {
            return (x >> n) | (x << (32 - n));
        }

        inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z) {
            return (x & y) ^ (~x & z);
        }

        inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) {
            return (x & y) ^ (x & z) ^ (y & z);
        }

        inline uint32_t sigma0(uint32_t x) {
            return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
        }

        inline uint32_t sigma1(uint32_t x) {
            return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
        }

        inline uint32_t gamma0(uint32_t x) {
            return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
        }

        inline uint32_t gamma1(uint32_t x) {
            return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
        }

        inline void transform(sha256_ctx& ctx, const uint8_t* data) {
            uint32_t a, b, c, d, e, f, g, h, i, j, t1, t2, m[64];

            for (i = 0, j = 0; i < 16; ++i, j += 4)
                m[i] = (static_cast<uint32_t>(data[j]) << 24)
                    | (static_cast<uint32_t>(data[j + 1]) << 16)
                    | (static_cast<uint32_t>(data[j + 2]) << 8)
                    | (static_cast<uint32_t>(data[j + 3]));
            for (; i < 64; ++i)
                m[i] = gamma1(m[i - 2]) + m[i - 7] + gamma0(m[i - 15]) + m[i - 16];

            a = ctx.state[0];
            b = ctx.state[1];
            c = ctx.state[2];
            d = ctx.state[3];
            e = ctx.state[4];
            f = ctx.state[5];
            g = ctx.state[6];
            h = ctx.state[7];

            static const uint32_t k[64] = {
                0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
                0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
                0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
                0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
                0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
                0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
                0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
                0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
                0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
                0xc67178f2
            };

            for (i = 0; i < 64; ++i) {
                t1 = h + sigma1(e) + ch(e, f, g) + k[i] + m[i];
                t2 = sigma0(a) + maj(a, b, c);
                h = g;
                g = f;
                f = e;
                e = d + t1;
                d = c;
                c = b;
                b = a;
                a = t1 + t2;
            }

            ctx.state[0] += a;
            ctx.state[1] += b;
            ctx.state[2] += c;
            ctx.state[3] += d;
            ctx.state[4] += e;
            ctx.state[5] += f;
            ctx.state[6] += g;
            ctx.state[7] += h;
        }

        inline void update(sha256_ctx& ctx, const uint8_t* data, size_t len) {
            for (size_t i = 0; i < len; ++i) {
                ctx.data[ctx.datalen] = data[i];
                ctx.datalen++;
                if (ctx.datalen == SHA256_BLOCK_SIZE) {
                    transform(ctx, ctx.data);
                    ctx.bitlen += SHA256_BLOCK_SIZE * 8;
                    ctx.datalen = 0;
                }
            }
        }

        inline void final(sha256_ctx& ctx, uint8_t* hash) {
            uint32_t i = ctx.datalen;

            // Pad whatever data is left in the buffer.
            if (ctx.datalen < 56) {
                ctx.data[i++] = 0x80;
                while (i < 56)
                    ctx.data[i++] = 0x00;
            } else {
                ctx.data[i++] = 0x80;
                while (i < SHA256_BLOCK_SIZE)
                    ctx.data[i++] = 0x00;
                transform(ctx, ctx.data);
                memset(ctx.data, 0, 56);
            }

            // Append to the padding the total message's length in bits and transform.
            ctx.bitlen += ctx.datalen * 8;
            ctx.data[63] = static_cast<uint8_t>(ctx.bitlen);
            ctx.data[62] = static_cast<uint8_t>(ctx.bitlen >> 8);
            ctx.data[61] = static_cast<uint8_t>(ctx.bitlen >> 16);
            ctx.data[60] = static_cast<uint8_t>(ctx.bitlen >> 24);
            ctx.data[59] = static_cast<uint8_t>(ctx.bitlen >> 32);
            ctx.data[58] = static_cast<uint8_t>(ctx.bitlen >> 40);
            ctx.data[57] = static_cast<uint8_t>(ctx.bitlen >> 48);
            ctx.data[56] = static_cast<uint8_t>(ctx.bitlen >> 56);
            transform(ctx, ctx.data);

            // Since this implementation uses little endian byte ordering and SHA uses big endian,
            // reverse all the bytes when copying the final state to the output hash.
            for (i = 0; i < 4; ++i) {
                hash[i] = static_cast<uint8_t>((ctx.state[0] >> (24 - i * 8)) & 0xff);
                hash[i + 4] = static_cast<uint8_t>((ctx.state[1] >> (24 - i * 8)) & 0xff);
                hash[i + 8] = static_cast<uint8_t>((ctx.state[2] >> (24 - i * 8)) & 0xff);
                hash[i + 12] = static_cast<uint8_t>((ctx.state[3] >> (24 - i * 8)) & 0xff);
                hash[i + 16] = static_cast<uint8_t>((ctx.state[4] >> (24 - i * 8)) & 0xff);
                hash[i + 20] = static_cast<uint8_t>((ctx.state[5] >> (24 - i * 8)) & 0xff);
                hash[i + 24] = static_cast<uint8_t>((ctx.state[6] >> (24 - i * 8)) & 0xff);
                hash[i + 28] = static_cast<uint8_t>((ctx.state[7] >> (24 - i * 8)) & 0xff);
            }
        }

    } // namespace detail

    // Convenience function to compute SHA-256 hash directly
    inline std::array<uint8_t, detail::SHA256_DIGEST_SIZE> sha256(const uint8_t* data, size_t len) {
        detail::sha256_ctx ctx;
        detail::update(ctx, data, len);
        std::array<uint8_t, detail::SHA256_DIGEST_SIZE> hash;
        detail::final(ctx, hash.data());
        return hash;
    }

    inline std::array<uint8_t, detail::SHA256_DIGEST_SIZE>
    sha256(const std::vector<uint8_t>& data) {
        return sha256(data.data(), data.size());
    }

    inline std::array<uint8_t, detail::SHA256_DIGEST_SIZE> sha256(const std::string& data) {
        return sha256(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    }

} // namespace crypto
} // namespace moon
