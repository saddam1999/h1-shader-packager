// SPDX-License-Identifier: BSL-1.0

#ifndef H1SP_CRYPT_HPP
#define H1SP_CRYPT_HPP

#include <cstddef>
#include <cstdint>

#include <bit>
#include <concepts>
#include <span>
#include <string>

namespace shader_packager
{
    /**
     * \brief A constraint for types that can encrypt chunks of fixed size.
     */
    template<typename Scheme>
    concept InPlaceEncryptionScheme = 
    requires (const Scheme& scheme, std::byte* chunk)
    {
        { scheme.chunk_size };
        { scheme.encrypt_chunk(chunk) } noexcept;
    };
    
    /**
     * \brief A constraint for types that can decrypt chunks of fixed size.
     */
    template<typename Scheme>
    concept InPlaceDecryptionScheme =
    requires (const Scheme& scheme, std::byte* chunk)
    {
        { scheme.chunk_size };
        { scheme.decrypt_chunk(chunk) } noexcept;
    };
    
    /**
     * \brief Implements the Tiny Encryption Algorithm, processing chunks in 
     *        little-endian order.
     *
     * See https://en.wikipedia.org/wiki/Tiny_Encryption_Algorithm.
     */
    struct tea
    {
        static constexpr std::size_t chunk_size = 2 * sizeof(std::uint32_t);
        
        std::endian   endian;
        std::uint32_t key[4];
        
        void encrypt_chunk(std::byte* chunk) const noexcept;
        
        void decrypt_chunk(std::byte* chunk) const noexcept;
    };
    static_assert(InPlaceEncryptionScheme<tea>);
    static_assert(InPlaceDecryptionScheme<tea>);
    
    /**
     * \brief The encryption scheme that Halo 1 (Retail/Custom Edition)
     *        uses for its shader archives.
     */
    inline constexpr tea h1_tea {
        .endian = std::endian::little,
        .key = {0x3FFFFFDD, 0x7FC3, 0xE5, 0x3FFFEF}
    };
    
    /**
     * \brief Applies \a scheme to encrypt `[buf .. buf + len)` in-place.
     *
     * This function does nothing if a chunk cannot fit into the buffer.
     *
     * \param [in]     scheme The encryption scheme to use.
     * \param [in,out] buf    The data to encrypt.
     */
    template<InPlaceEncryptionScheme Scheme>
    void encrypt_buffer(const Scheme& scheme, std::span<std::byte> buf)
    {
        const auto len = buf.size();
        const unsigned long chunk_size = scheme.chunk_size;
        if (len < chunk_size)
            return; // can't encrypt, do nothing
        
        // If len % chunk_size != 0, we encrypt the whole chunks first
        // then re-encrypt the tailing piece along with part of the last chunk
        // already encrypted
        
        for ( auto chunk = &(*buf.begin()), end = &(*buf.end()) - (len % chunk_size)
            ; chunk != end
            ; chunk += chunk_size)
        {
            scheme.encrypt_chunk(chunk);
        }
        
        if ((len % chunk_size) != 0)
        {
            scheme.encrypt_chunk(&(*buf.begin()) + len - chunk_size);
        }
    }
    
    /**
     * \brief Applies \a scheme to decrypt `[buf .. buf + len)` in-place.
     *
     * This function does nothing if a chunk cannot fit into the buffer.
     *
     * \param [in]     scheme The decryption scheme to use.
     * \param [in,out] buf    The data to decrypt.
     */
    template<InPlaceDecryptionScheme Scheme>
    void decrypt_buffer(const Scheme& scheme, std::span<std::byte> buf)
    {
        const auto len = buf.size();
        const unsigned long chunk_size = scheme.chunk_size;
        if (len < chunk_size || len <= 0)
            return; // can't decrypt, do nothing
        
        // If len % chunk_size != 0, we decrypt the tailing bit first along with
        // part of the last chunk, which will get re-decrypted when we decrypt 
        // the whole chunks
        
        if ((len % chunk_size) != 0)
        {
            scheme.decrypt_chunk(&(*buf.begin()) + len - chunk_size);
        }
        
        buf = buf.first(buf.size() - (len % chunk_size));
        for ( auto chunk = &(*buf.begin()), end = &(*buf.end())
            ; chunk != end
            ; chunk += chunk_size)
        {
            scheme.decrypt_chunk(chunk);
        }
    }
    
    /**
     * \brief Calculates the MD5 digest for some data.
     *
     * \param [in] buf The data to hash.
     * \return The MD5 digest of the data, as a string.
     */
    std::string compute_md5_digest(std::span<const std::byte> buf);
}

#endif // H1SP_CRYPT_HPP