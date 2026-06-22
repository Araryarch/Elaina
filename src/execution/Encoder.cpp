#include "Encoder.h"
#include "core/Log.h"
#include <zstd.h>
#include <cstring>

#define XXH_INLINE_ALL
#include "xxhash.h"

extern "C" {
#include "blake3.h"
}

static constexpr uint8_t  SIG[4] = { 'R', 'S', 'B', '1' };
static constexpr uint32_t HASH_SEED = 42u;
static constexpr uint32_t MAGIC_A = 0x4C464F52; // "ROFL"
static constexpr uint32_t MAGIC_B = 0x946AC432;
static constexpr uint8_t  KEY[4] = { 0x52, 0x4F, 0x46, 0x4C };
static constexpr uint32_t FOOTER_SIZE = 40;

static uint8_t rotl8(uint8_t v, int s) {
    s &= 7;
    return (v << s) | (v >> (8 - s));
}

static std::string Sign(const std::string& bc) {
    if (bc.empty()) return {};

    uint8_t hash[32];
    {
        blake3_hasher h;
        blake3_hasher_init(&h);
        blake3_hasher_update(&h, bc.data(), bc.size());
        blake3_hasher_finalize(&h, hash, 32);
    }

    uint8_t thash[32];
    for (int i = 0; i < 32; i++) {
        uint8_t k = KEY[i & 3], hb = hash[i], c = k + i, r;
        int sft;
        switch (i & 3) {
            case 0: sft = ((c & 3) + 1); r = rotl8(hb ^ ~k, sft); break;
            case 1: sft = ((c & 3) + 2); r = rotl8(k ^ ~hb, sft); break;
            case 2: sft = ((c & 3) + 3); r = rotl8(hb ^ ~k, sft); break;
            case 3: sft = ((c & 3) + 4); r = rotl8(k ^ ~hb, sft); break;
        }
        thash[i] = r;
    }

    uint32_t fh;
    memcpy(&fh, thash, 4);
    uint8_t footer[FOOTER_SIZE]{};
    uint32_t v;
    v = fh ^ MAGIC_B; memcpy(footer, &v, 4);
    v = fh ^ MAGIC_A; memcpy(footer + 4, &v, 4);
    memcpy(footer + 8, thash, 32);

    return bc + std::string((char*)footer, FOOTER_SIZE);
}

static std::string Compress(const std::string& signedBc) {
    auto max = ZSTD_compressBound(signedBc.size());
    std::vector<char> buf(max + 8);

    memcpy(buf.data(), SIG, 4);
    uint32_t sz = (uint32_t)signedBc.size();
    memcpy(buf.data() + 4, &sz, 4);

    auto cs = ZSTD_compress(buf.data() + 8, max, signedBc.data(), signedBc.size(), ZSTD_maxCLevel());
    if (ZSTD_isError(cs)) {
        Log::Error("ZSTD compress failed: %s", ZSTD_getErrorName(cs));
        return {};
    }

    size_t final = cs + 8;
    buf.resize(final);

    auto xkey = XXH32(buf.data(), (uint32_t)final, HASH_SEED);
    auto bytes = (const uint8_t*)&xkey;
    for (auto i = 0u; i < final; i++)
        buf[i] ^= (bytes[i % 4] + i * 41) & 0xFF;

    Log::Info("RSB1: %zu -> %zu bytes", signedBc.size(), final);
    return std::string(buf.data(), final);
}

Result<std::string> Encoder::Encode(const std::string& bc) {
    auto signed_ = Sign(bc);
    if (signed_.empty()) return { Err::EncodeFailed, "Signing failed" };

    auto compressed = Compress(signed_);
    if (compressed.empty()) return { Err::EncodeFailed, "Compression failed" };

    return compressed;
}

std::vector<uint8_t> Encoder::EncodeVec(const std::vector<uint8_t>& bc) {
    auto s = Encode(std::string(bc.begin(), bc.end()));
    if (!s) return {};
    return std::vector<uint8_t>(s.value.begin(), s.value.end());
}
