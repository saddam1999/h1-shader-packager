// SPDX-License-Identifier: BSL-1.0

#include <h1sp/archive.hpp>

#include <cstdio>

#include <algorithm>
#include <numeric>
#include <utility>

#include <h1sp/crypt.hpp>
#include <h1sp/io.hpp>

namespace shader_packager
{
    byte_buffer::operator bool() const noexcept
    {
        return static_cast<bool>(buffer);
    }
    
    std::span<std::byte> byte_buffer::range() const noexcept
    {
        return {buffer.get(), nbytes};
    }
    
    byte_buffer read_file(const char* file)
    {
        byte_buffer result {};
        
        auto fp = std::fopen(file, "rb");
        if (fp == nullptr)
        {
            std::perror("failed to open file for reading\n");
            return result;
        }
        
        const auto size = [fp] {
            std::fseek(fp, 0, SEEK_END);
            const auto size = std::ftell(fp);
            if (size == -1)
                std::perror("failed to get file data size");
            std::fseek(fp, 0, SEEK_SET);
            return static_cast<std::size_t>(size != -1 ? size : 0);
        }();
        
        if (size <= 0)
        {
            std::fclose(fp);
            return result;
        }
        
        result.buffer = std::unique_ptr<std::byte[]>(new std::byte[size]);
        result.nbytes = std::fread(
            result.buffer.get(), 
            sizeof(std::byte), size, 
            fp);
        std::fclose(fp);
        
        if (result.nbytes != size) 
            result = byte_buffer{};
        
        return result;
    }
    
    bool write_file(const char* file, std::span<const std::byte> buf)
    {
        auto fp = std::fopen(file, "wb");
        if (fp == nullptr)
        {
            std::perror("failed to open file for writing\n");
            return false;
        }
        
        std::fwrite(buf.data(), sizeof(std::byte), buf.size(), fp);
        std::fclose(fp);
        fp = nullptr;
        
        return true;
    }
    
    archive::read_error archive::read_from_file(const char* file)
    {
        auto buf = read_file(file);
        if (!buf)
            return read_error::could_not_open_file;
        
        // Strictly speaking, this should be < 33, but Halo requires the 
        // archive to be non-empty, even if its just one byte.
        // The stored MD5 hash is null-terminated, and Halo checks for that
        // null-terminator.
        if (buf.nbytes < 34)
            return read_error::archive_data_is_corrupt;
        
        const auto archive_data = buf.range().first(buf.nbytes - 33);
        const auto archive_md5 = std::string_view(
            reinterpret_cast<const char*>(buf.range().last(33).data()), 
            33);
        
        // decrypt buffer and verify hash is correct
        decrypt_buffer(h1_tea, buf.range());
        {
            const auto md5_str = compute_md5_digest(archive_data);
            const std::string_view md5_sv(md5_str.c_str(), md5_str.size() + 1);
            // we need to ensure the null terminator is there, so we use sv's
            if (archive_md5 != md5_sv)
            {
                std::fprintf(stderr, 
                    "md5 did not match\n"
                    "\tcomputed %.32s\n"
                    "\tneeded %.32s\n",
                    md5_str.c_str(),
                    archive_md5.data());
                return read_error::archive_data_is_corrupt;
            }
        }
        
        // verify each archive entry is not corrupt
        {
            long member = 0;
            archive_enumerator e{archive_data};
            for (; e; e.advance()) { ++member; }
            if (e.has_error())
            {
                std::fprintf(stderr, "error at archive member %ld\n", member);
                return read_error::archive_data_is_corrupt;
            }
        }
        
        // everything checks out, assign the final values
        filebuf = std::move(buf);
        data    = archive_data;
        return read_error::success;
    }
    
    void archive::load_members_from(const std::span<const byte_buffer> member_bufs)
    {
        const std::size_t total_size = std::accumulate(
            member_bufs.begin(), member_bufs.end(),
            std::size_t{0},
            [] (auto size, auto& buf) { return size + buf.nbytes; }
        ) + sizeof(chunk_size_type) * member_bufs.size() // headers
          + std::size_t{33};                             // MD5 string + '\0'
        
        // Allocate a buffer of appropriate size
        byte_buffer buf {
            .buffer = std::unique_ptr<std::byte[]>(new std::byte[total_size]),
            .nbytes = total_size
        };
        
        // Populate the buffer
        {
            std::byte* cursor = buf.range().data();
            
            // Write the members/member headers
            for (auto& member : member_bufs)
            {
                cursor = serialize<std::endian::little>(
                    chunk_size_type{member.nbytes},
                    cursor);
                
                cursor = std::copy(
                    member.range().begin(), member.range().end(),
                    cursor
                );
            }
            
            // Write MD5 digest to the end of the buffer
            cursor = std::copy_n(
                reinterpret_cast<const std::byte*>(
                    compute_md5_digest(buf.range().first(buf.nbytes - 33)).c_str()
                ), 
                33,
                cursor
            );
        }
        
        // Do not encrypt; encrypt on write
        
        // Set members
        filebuf = std::move(buf);
        data    = filebuf.range().first(buf.nbytes - 33);
    }
    
    archive::write_error archive::flush_to_file(const char* file)
    {
        if (data.size() < sizeof(chunk_size_type))
            return write_error::no_data_to_write;
        
        // open file to see if we can even write
        auto fp = std::fopen(file, "wb");
        if (fp == nullptr)
            return write_error::could_not_open_file;
        
        // encrypt archive
        encrypt_buffer(h1_tea, filebuf.range());
        
        // write to the buffer to the file
        std::fwrite(
            filebuf.buffer.get(), 
            sizeof(std::byte), filebuf.nbytes, 
            fp);
        std::fclose(fp);
        
        // "flush" archive by emptying it
        *this = archive{};
        return write_error::success;
    }
    
    archive_enumerator archive::enumerate() const noexcept
    {
        return archive_enumerator{data};
    }
    
    archive_enumerator::archive_enumerator(std::span<std::byte> enumerable_range) 
        noexcept
        : range(enumerable_range)
        { }
    
    archive_enumerator& archive_enumerator::advance() noexcept
    {
        if (!finished())
        {
            const auto chunk_size = 
                deserialize<chunk_size_type, std::endian::little>(range.data());
            range = range.last(
                range.size() - chunk_size - sizeof(chunk_size_type));
        }
        
        return *this;
    }
    
    std::span<std::byte> archive_enumerator::data() const noexcept
    {
        if (finished())
            return {};
        
        const auto chunk_size = 
            deserialize<chunk_size_type, std::endian::little>(range.data());
        
        return range.subspan(sizeof(chunk_size_type), chunk_size);
    }
    
    bool archive_enumerator::is_at_end() const noexcept
    {
        return range.empty();
    }
    
    bool archive_enumerator::has_error() const noexcept
    {
        if (is_at_end())
            return false;
        
        if (range.size() < sizeof(chunk_size_type))
            return true;
        
        const auto chunk_size = 
            deserialize<chunk_size_type, std::endian::little>(range.data());
        
        if ((chunk_size + sizeof(chunk_size_type)) > range.size())
            return true;
        
        return false;
    }
    
    bool archive_enumerator::finished() const noexcept
    {
        return is_at_end() || has_error();
    }
}