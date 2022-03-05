// SPDX-License-Identifier: BSL-1.0

#include <h1sp/crypt.hpp>
#include <h1sp/io.hpp>

#include <climits>
#include <cstdio>

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

#include <openssl/md5.h>

static_assert(
    CHAR_BIT == 8,
    "I pity whoever has to fix this project when this assertion fails."
);

namespace shader_packager
{

// Taken almost exactly from 
// https://en.wikipedia.org/wiki/Tiny_Encryption_Algorithm
void tea::encrypt_chunk(std::byte* chunk) const noexcept
{
    using std::uint32_t;
    
    uint32_t v0 = deserialize<uint32_t>(chunk, endian);
    uint32_t v1 = deserialize<uint32_t>(chunk + sizeof(v0), endian);
    
    const uint32_t delta = 0x9E3779B9;
    
    const uint32_t k0 = key[0];
    const uint32_t k1 = key[1];
    const uint32_t k2 = key[2];
    const uint32_t k3 = key[3];
    
    uint32_t sum = 0;
    for (int i = 0; i < 32; ++i)
    {
        sum += delta;
        v0 += ((v1 << 4u) + k0) ^ (v1 + sum) ^ ((v1 >> 5u) + k1);
        v1 += ((v0 << 4u) + k2) ^ (v0 + sum) ^ ((v0 >> 5u) + k3);
    }
    
    serialize(v0, chunk, endian);
    serialize(v1, chunk + sizeof(v0), endian);
}

// Taken almost exactly from 
// https://en.wikipedia.org/wiki/Tiny_Encryption_Algorithm
void tea::decrypt_chunk(std::byte* chunk) const noexcept
{
    using std::uint32_t;
    
    uint32_t v0 = deserialize<uint32_t>(chunk, endian);
    uint32_t v1 = deserialize<uint32_t>(chunk + sizeof(v0), endian);
    
    const uint32_t delta = 0x9E3779B9;
    
    const uint32_t k0 = key[0];
    const uint32_t k1 = key[1];
    const uint32_t k2 = key[2];
    const uint32_t k3 = key[3];
    
    uint32_t sum = 0xC6EF3720;
    for (int i = 0; i < 32; ++i)
    {
        v1 -= ((v0 << 4u) + k2) ^ (v0 + sum) ^ ((v0 >> 5u) + k3);
        v0 -= ((v1 << 4u) + k0) ^ (v1 + sum) ^ ((v1 >> 5u) + k1);
        sum -= delta;
    }
    
    serialize(v0, chunk, endian);
    serialize(v1, chunk + sizeof(v0), endian);
}

std::string compute_md5_digest(std::span<const std::byte> buf)
{
    const auto len = buf.size();
    if (len > std::numeric_limits<unsigned long>::max())
        throw std::range_error("buffer length could not be represented as ulong");
    
    unsigned char digest[MD5_DIGEST_LENGTH] = {};
    (void)MD5(
        reinterpret_cast<const unsigned char*>(buf.data()), 
        static_cast<unsigned long>(len), 
        digest);
    
    std::string result = "";
    result.reserve(sizeof("abcdefghijklmnopqrstuvwxyz012345"));
    
    for (const auto& d : digest)
    {
        char strbuf[3] = {};
        std::snprintf(strbuf, std::size(strbuf), "%02hhx", d);
        result += std::string_view{strbuf, 2};
    }
    
    return result;
}

} // namespace shader_packager
