#pragma once

#include <cstdint>

namespace moon
{
    namespace md5
    {
        /* Constants for MD5Transform routine. */
        static constexpr int S11 = 7;
        static constexpr int S12 = 12;
        static constexpr int S13 = 17;
        static constexpr int S14 = 22;
        static constexpr int S21 = 5;
        static constexpr int S22 = 9;
        static constexpr int S23 = 14;
        static constexpr int S24 = 20;
        static constexpr int S31 = 4;
        static constexpr int S32 = 11;
        static constexpr int S33 = 16;
        static constexpr int S34 = 23;
        static constexpr int S41 = 6;
        static constexpr int S42 = 10;
        static constexpr int S43 = 15;
        static constexpr int S44 = 21;

        inline static constexpr uint8_t PADDING[64] = { 0x80 };
        inline static constexpr char HEX[16] = {
            '0', '1', '2', '3',
            '4', '5', '6', '7',
            '8', '9', 'a', 'b',
            'c', 'd', 'e', 'f'
        };

        /* F, G, H and I are basic MD5 functions.
        */
        template<typename X, typename Y, typename Z>
        static inline auto BASE_F(X x, Y y, Z z)
        {
            return (((x) & (y)) | ((~x) & (z)));
        }

        template<typename X, typename Y, typename Z>
        static inline auto BASE_G(X x, Y y, Z z)
        {
            return (((x) & (z)) | ((y) & (~z)));
        }

        template<typename X, typename Y, typename Z>
        static inline auto BASE_H(X x, Y y, Z z)
        {
            return ((x) ^ (y) ^ (z));
        }

        template<typename X, typename Y, typename Z>
        static inline auto BASE_I(X x, Y y, Z z)
        {
            return ((y) ^ ((x) | (~z)));
        }

        /* ROTATE_LEFT rotates x left n bits.
        */
        template<typename X, typename N>
        static inline auto ROTATE_LEFT(X x, N n)
        {
            return (((x) << (n)) | ((x) >> (32 - (n))));
        }

        /* FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4.
            Rotation is separate from addition to prevent recomputation.
        */
        template<typename A, typename B, typename C, typename D, typename X, typename S, typename AC>
        static inline void FF(A& a, B b, C c, D d, X x, S s, AC ac)
        {
            (a) += BASE_F((b), (c), (d)) + (x)+ac;
            (a) = ROTATE_LEFT((a), (s));
            (a) += (b);
        }

        template<typename A, typename B, typename C, typename D, typename X, typename S, typename AC>
        static inline void GG(A& a, B b, C c, D d, X x, S s, AC ac)
        {
            (a) += BASE_G((b), (c), (d)) + (x)+ac;
            (a) = ROTATE_LEFT((a), (s));
            (a) += (b);
        }

        template<typename A, typename B, typename C, typename D, typename X, typename S, typename AC>
        static inline void HH(A& a, B b, C c, D d, X x, S s, AC ac)
        {
            (a) += BASE_H((b), (c), (d)) + (x)+ac;
            (a) = ROTATE_LEFT((a), (s));
            (a) += (b);
        }

        template<typename A, typename B, typename C, typename D, typename X, typename S, typename AC>
        static inline void II(A& a, B b, C c, D d, X x, S s, AC ac)
        {
            (a) += BASE_I((b), (c), (d)) + (x)+ac;
            (a) = ROTATE_LEFT((a), (s));
            (a) += (b);
        }

        /* MD5 basic transformation. Transforms _state based on block. */
        template<class = void>
        void transform(const uint8_t* block, uint32_t* state) {

            uint32_t a = state[0], b = state[1], c = state[2], d = state[3], x[16];

            decode(block, x, 64);

            /* Round 1 */
            FF(a, b, c, d, x[0], S11, 0xd76aa478); /* 1 */
            FF(d, a, b, c, x[1], S12, 0xe8c7b756); /* 2 */
            FF(c, d, a, b, x[2], S13, 0x242070db); /* 3 */
            FF(b, c, d, a, x[3], S14, 0xc1bdceee); /* 4 */
            FF(a, b, c, d, x[4], S11, 0xf57c0faf); /* 5 */
            FF(d, a, b, c, x[5], S12, 0x4787c62a); /* 6 */
            FF(c, d, a, b, x[6], S13, 0xa8304613); /* 7 */
            FF(b, c, d, a, x[7], S14, 0xfd469501); /* 8 */
            FF(a, b, c, d, x[8], S11, 0x698098d8); /* 9 */
            FF(d, a, b, c, x[9], S12, 0x8b44f7af); /* 10 */
            FF(c, d, a, b, x[10], S13, 0xffff5bb1); /* 11 */
            FF(b, c, d, a, x[11], S14, 0x895cd7be); /* 12 */
            FF(a, b, c, d, x[12], S11, 0x6b901122); /* 13 */
            FF(d, a, b, c, x[13], S12, 0xfd987193); /* 14 */
            FF(c, d, a, b, x[14], S13, 0xa679438e); /* 15 */
            FF(b, c, d, a, x[15], S14, 0x49b40821); /* 16 */

                                                    /* Round 2 */
            GG(a, b, c, d, x[1], S21, 0xf61e2562); /* 17 */
            GG(d, a, b, c, x[6], S22, 0xc040b340); /* 18 */
            GG(c, d, a, b, x[11], S23, 0x265e5a51); /* 19 */
            GG(b, c, d, a, x[0], S24, 0xe9b6c7aa); /* 20 */
            GG(a, b, c, d, x[5], S21, 0xd62f105d); /* 21 */
            GG(d, a, b, c, x[10], S22, 0x2441453); /* 22 */
            GG(c, d, a, b, x[15], S23, 0xd8a1e681); /* 23 */
            GG(b, c, d, a, x[4], S24, 0xe7d3fbc8); /* 24 */
            GG(a, b, c, d, x[9], S21, 0x21e1cde6); /* 25 */
            GG(d, a, b, c, x[14], S22, 0xc33707d6); /* 26 */
            GG(c, d, a, b, x[3], S23, 0xf4d50d87); /* 27 */
            GG(b, c, d, a, x[8], S24, 0x455a14ed); /* 28 */
            GG(a, b, c, d, x[13], S21, 0xa9e3e905); /* 29 */
            GG(d, a, b, c, x[2], S22, 0xfcefa3f8); /* 30 */
            GG(c, d, a, b, x[7], S23, 0x676f02d9); /* 31 */
            GG(b, c, d, a, x[12], S24, 0x8d2a4c8a); /* 32 */

                                                    /* Round 3 */
            HH(a, b, c, d, x[5], S31, 0xfffa3942); /* 33 */
            HH(d, a, b, c, x[8], S32, 0x8771f681); /* 34 */
            HH(c, d, a, b, x[11], S33, 0x6d9d6122); /* 35 */
            HH(b, c, d, a, x[14], S34, 0xfde5380c); /* 36 */
            HH(a, b, c, d, x[1], S31, 0xa4beea44); /* 37 */
            HH(d, a, b, c, x[4], S32, 0x4bdecfa9); /* 38 */
            HH(c, d, a, b, x[7], S33, 0xf6bb4b60); /* 39 */
            HH(b, c, d, a, x[10], S34, 0xbebfbc70); /* 40 */
            HH(a, b, c, d, x[13], S31, 0x289b7ec6); /* 41 */
            HH(d, a, b, c, x[0], S32, 0xeaa127fa); /* 42 */
            HH(c, d, a, b, x[3], S33, 0xd4ef3085); /* 43 */
            HH(b, c, d, a, x[6], S34, 0x4881d05); /* 44 */
            HH(a, b, c, d, x[9], S31, 0xd9d4d039); /* 45 */
            HH(d, a, b, c, x[12], S32, 0xe6db99e5); /* 46 */
            HH(c, d, a, b, x[15], S33, 0x1fa27cf8); /* 47 */
            HH(b, c, d, a, x[2], S34, 0xc4ac5665); /* 48 */

                                                   /* Round 4 */
            II(a, b, c, d, x[0], S41, 0xf4292244); /* 49 */
            II(d, a, b, c, x[7], S42, 0x432aff97); /* 50 */
            II(c, d, a, b, x[14], S43, 0xab9423a7); /* 51 */
            II(b, c, d, a, x[5], S44, 0xfc93a039); /* 52 */
            II(a, b, c, d, x[12], S41, 0x655b59c3); /* 53 */
            II(d, a, b, c, x[3], S42, 0x8f0ccc92); /* 54 */
            II(c, d, a, b, x[10], S43, 0xffeff47d); /* 55 */
            II(b, c, d, a, x[1], S44, 0x85845dd1); /* 56 */
            II(a, b, c, d, x[8], S41, 0x6fa87e4f); /* 57 */
            II(d, a, b, c, x[15], S42, 0xfe2ce6e0); /* 58 */
            II(c, d, a, b, x[6], S43, 0xa3014314); /* 59 */
            II(b, c, d, a, x[13], S44, 0x4e0811a1); /* 60 */
            II(a, b, c, d, x[4], S41, 0xf7537e82); /* 61 */
            II(d, a, b, c, x[11], S42, 0xbd3af235); /* 62 */
            II(c, d, a, b, x[2], S43, 0x2ad7d2bb); /* 63 */
            II(b, c, d, a, x[9], S44, 0xeb86d391); /* 64 */

            state[0] += a;
            state[1] += b;
            state[2] += c;
            state[3] += d;
        }


        /* Encodes input (ulong) into output (uint8_t). Assumes length is
        a multiple of 4.
        */
        void encode(const uint32_t *input, uint8_t *output, size_t length) {

            for (size_t i = 0, j = 0; j < length; i++, j += 4) {
                output[j] = (uint8_t)(input[i] & 0xff);
                output[j + 1] = (uint8_t)((input[i] >> 8) & 0xff);
                output[j + 2] = (uint8_t)((input[i] >> 16) & 0xff);
                output[j + 3] = (uint8_t)((input[i] >> 24) & 0xff);
            }
        }

        /* Decodes input (uint8_t) into output (ulong). Assumes length is
        a multiple of 4.
        */
        void decode(const uint8_t *input, uint32_t *output, size_t length) {

            for (size_t i = 0, j = 0; j < length; i++, j += 4) {
                output[i] = ((uint32_t)input[j]) | (((uint32_t)input[j + 1]) << 8) |
                    (((uint32_t)input[j + 2]) << 16) | (((uint32_t)input[j + 3]) << 24);
            }
        }

        static std::size_t constexpr BLOCK_BYTES = 64;
        static std::size_t constexpr DIGEST_BYTES = 16;

        struct md5_context
        {
            uint32_t state[4];	/* state (ABCD) */
            uint32_t count[2];	/* number of bits, modulo 2^64 (low-order word first) */
            uint8_t buffer[BLOCK_BYTES];	/* input buffer */
        };

        template<class = void>
        void
            init(md5_context& ctx) noexcept
        {
            ctx.count[0] = ctx.count[1] = 0;
            ctx.state[0] = 0x67452301;
            ctx.state[1] = 0xefcdab89;
            ctx.state[2] = 0x98badcfe;
            ctx.state[3] = 0x10325476;
        }

        /* MD5 block update operation. Continues an MD5 message-digest
        operation, processing another message block, and updating the
        context.
        */
        template<class = void>
        void update(md5_context& ctx, void const* data, size_t length) {
            const uint8_t* input = (const uint8_t*)data;
            uint32_t i, index, partLen;

            /* Compute number of bytes mod 64 */
            index = (uint32_t)((ctx.count[0] >> 3) & 0x3f);

            /* update number of bits */
            if ((ctx.count[0] += ((uint32_t)length << 3)) < ((uint32_t)length << 3))
                ctx.count[1]++;
            ctx.count[1] += ((uint32_t)length >> 29);

            partLen = 64 - index;

            /* transform as many times as possible. */
            if (length >= partLen) {

                memcpy(&ctx.buffer[index], input, partLen);
                transform(ctx.buffer,ctx.state);

                for (i = partLen; i + 63 < length; i += 64)
                    transform(&input[i], ctx.state);
                index = 0;

            }
            else {
                i = 0;
            }

            /* Buffer remaining input */
            memcpy(&ctx.buffer[index], &input[i], length - i);
        }

        /* MD5 finalization. Ends an MD5 message-digest operation, writing the
        the message digest and zeroizing the context.
        */
        template<class = void>
        void finish(md5_context& ctx, void* digest) {
            uint8_t* _digest = (uint8_t*)digest;
            uint8_t bits[8];
            uint32_t oldState[4];
            uint32_t oldCount[2];
            uint32_t index, padLen;

            /* Save current state and count. */
            memcpy(oldState, ctx.state, 16);
            memcpy(oldCount, ctx.count, 8);

            /* Save number of bits */
            encode(ctx.count, bits, 8);

            /* Pad out to 56 mod 64. */
            index = (uint32_t)((ctx.count[0] >> 3) & 0x3f);
            padLen = (index < 56) ? (56 - index) : (120 - index);
            update(ctx, PADDING, padLen);

            /* Append length (before padding) */
            update(ctx, bits, 8);

            /* Store state in digest */
            encode(ctx.state, _digest, 16);

            /* Restore current state and count. */
            memcpy(ctx.state, oldState, 16);
            memcpy(ctx.count, oldCount, 8);
        }
    };
}


