// SPDX-License-Identifier: BSL-1.0

#ifndef H1SP_ARCHIVE_HPP
#define H1SP_ARCHIVE_HPP

#include <cstddef>
#include <cstdint>

#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>

/* HALO 1 SHADER ARCHIVE FILES:
 * These files are encrypted using the Tiny Encryption Algorithm
 * (https://en.wikipedia.org/wiki/Tiny_Encryption_Algorithm)
 * using the key 0x3FFFFFDD, 0x7FC3, 0xE5, 0x3FFFEF.
 *
 * When unencrypted, the last 33 octets compose the null-terminated MD5 digest 
 * in lowercase hex form, with 0s for padding. Halo uses the stored hash
 * to verify archive integrity.
 *
 * The first archive member is located at the start of the file contents.
 * Each member follows a simple scheme:
 *   - a uint32 indicating the member data size in bytes (excluding this field)
 *   - the member data itself
 *
 * Thus, if `data` is a pointer to the current member data and `size` is the 
 * size of data, the next member is located by `data + size` (assuming a byte 
 * is an octet, of course).
 */

namespace shader_packager
{   
    /**
     * \brief A dynamically-sized buffer of bytes with size information.
     */
    struct byte_buffer;
    
    /**
     * \brief Delegates access to a shader file archive.
     */
    class archive;
    
    /**
     * \brief An interface for enumerating over the files in an #archive.
     */
    class archive_enumerator;
    
    /**
     * \brief Reads the entire contents of a file in binary mode into a buffer.
     *
     * \param [in] file The name of the file to read.
     * \return The buffer containing the file's contents.
     */
    byte_buffer read_file(const char* file);
    
    /**
     * \brief Writes a buffer of bytes to a file in binary mode.
     *
     * The file is truncated if opened.
     *
     * \param [in] file The name of the file to write.
     * \param [in] buf  The contents to write.
     * \return \c true on success, otherwise \c false.
     */
    bool write_file(const char* file, std::span<const std::byte> buf);

    struct byte_buffer
    {
        std::unique_ptr<std::byte[]> buffer; ///< The buffer for the bytes.
        std::size_t                  nbytes; ///< Number of bytes in buffer.
        
        /**
         * \brief Returns the data, as a `std::span`.
         */
        std::span<std::byte> range() const noexcept;
        
        /**
         * \brief Checks whether this buffer holds any data.
         *
         * \return \c true if the buffer holds data, otherwise \c false.
         */
        explicit operator bool() const noexcept;
    };
    
    class archive
    {
        using chunk_size_type = std::uint32_t;
        
        byte_buffer          filebuf; ///< Takes ownership of the data buffer.
        std::span<std::byte> data;    ///< The subrange of the buffer that 
                                      ///< contains the chunk data.
    
    public:
        /**
         * \brief Indicates the result of a #read_from_file operation.
         */
        enum class read_error
        {
            success,
            could_not_open_file,
            archive_data_is_corrupt
        };
        
        /**
         * \brief Indicates the result of a #write_to_file operation.
         */
        enum class write_error
        {
            success,
            no_data_to_write,
            could_not_open_file
        };
        
        /**
         * \brief Loads an archive from \a file.
         *
         * If this function returns a value that is not \c read_error::success,
         * the state of this object is unchanged.
         *
         * \param [in] The filepath of the archive to load.
         * \return `read_error::success` on success, 
         *         or an appropriate value from \c read_error on failure.
         */
        read_error read_from_file(const char* file);
        
        /**
         * \brief Loads an archive from members supplied by individual buffers.
         */
        void load_members_from(std::span<const byte_buffer> member_bufs);
        
        /**
         * \brief Writes an archive to a \a file.
         *
         * If this function returns `write_error::success`, then the archive 
         * is emptied.
         *
         * \param [in] file The filepath of the archive to write.
         * \return `write_error::success` on success, 
         *         or an appropriate value from \c write_error on failure.
         */
        write_error flush_to_file(const char* file);
        
        /**
         * \brief Creates an object that can be used to enumerate over the 
         *        chunks in the archive.
         */
        archive_enumerator enumerate() const noexcept;
        
        /**
         * \brief Invokes \a f on the byte ranges of this archive's members.
         *
         * The invocation is performed in order of the members.
         */
        template<std::invocable<std::span<std::byte>> F>
            requires (std::is_invocable_r_v<void, F&&,  std::span<std::byte>>)
        void for_each(F&& f) const;
        
        /**
         * \brief Invokes \a f on the byte ranges of this archive's members,
         *        possibly halting early depending on the invocation result.
         *
         * The invocation is performed in order of the members.
         *
         * If invocation of \a f results in a value that tests \c true when 
         * contextually converted to \c bool, this function returns early 
         * with `std::make_optional(std::move(r))` where `r` is the result.
         * Otherwise, if the function does not return early in this way, 
         * `std::nullopt` is returned.
         */
        template<std::invocable<std::span<std::byte>> F>
        std::optional<std::invoke_result_t<F&&, std::span<std::byte>>> 
        for_each(F&& f);
    };
    
    class archive_enumerator
    {
        using chunk_size_type = std::uint32_t;
        
        std::span<std::byte> range; ///< The span of enumerable bytes.
    public:
        archive_enumerator() = default;
        archive_enumerator(std::span<std::byte> enumerable_range) noexcept;
        archive_enumerator(const archive_enumerator&) = default;
        archive_enumerator& operator=(const archive_enumerator&) = default;
        
        /**
         * \brief Moves the enumerator to the next chunk in the archive.
         *
         * This function does nothing if the enumerator is #finished.
         *
         * \return `*this`
         */
        archive_enumerator& advance() noexcept;
        
        /**
         * \brief Gets the data of the current chunk.
         *
         * \return A span containing the range of bytes of the chunk data,
         *         or an empty span if this enumerator is #finished.
         */
        std::span<std::byte> data() const noexcept;
        
        /**
         * \brief Checks if the enumerator is at the end of the archive.
         *
         * \return \c true if the enumerator is at the end, otherwise \c false.
         */
        bool is_at_end() const noexcept;
        
        /**
         * \brief Checks if the enumerator has an error that prevents it from 
         *        reading the chunk or advancing.
         *
         * The enumerator does not have an error if it #is_at_end.
         *
         * \return \c true if the enumerator has an error, otherwise \c false.
         */
        bool has_error() const noexcept;
        
        
        /**
         * \brief Determines if the enumerator can advance.
         *
         * If this function returns \c false, one can use #is_at_end() or 
         * #has_error() to inspect the state of the enumerator.
         *
         * \return \c true if the enumerator can advance, otherwise \c false.
         */
        bool finished() const noexcept;
        
        /**
         * \brief A convenience conversion operator for `!finished()`.
         *
         * Can be used as in the following snippet:
         * \code {.cpp}
         * archive_enumerator e = ...;
         * for (archive_enumerator e = ...; e; e.advance())
         * {
         *     auto data = e.data();
         *     // ...
         * }
         * \endcode
         */
        explicit operator bool() const noexcept { return !finished(); }
    };
    
    template<std::invocable<std::span<std::byte>> F>
        requires (std::is_invocable_r_v<void, F&&,  std::span<std::byte>>)
    inline void archive::for_each(F&& f) const
    {
        for (auto e = enumerate(); e; e.advance())
        {
            std::forward<F>(f)(e.data());
        }
    }
    
    template<std::invocable<std::span<std::byte>> F>
    inline std::optional<std::invoke_result_t<F&&, std::span<std::byte>>> 
    archive::for_each(F&& f)
    {
        for (auto e = enumerate(); e; e.advance())
        {
            auto result = std::forward<F>(f)(e.data());
            if (result)
                return std::make_optional(std::move(result));
        }
        
        return std::nullopt;
    }
}

#endif // H1SP_ARCHIVE_HPP