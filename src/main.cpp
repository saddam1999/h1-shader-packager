// SPDX-License-Identifier: BSL-1.0

/*
USAGE
  h1sp {-h|--help}
  h1sp {-u|--unpack} {-pc|-ce} {-fx|-vsh} INPUT_FILE [PREFIX] 
    Unpack the shader archive INPUT_FILE by writing each member to files prefixed with PREFIX.
  h1sp {-p|--pack} {-pc|-ce} {-fx|-vsh} OUTPUT_FILE [PREFIX]
    Creates a shader archive OUTPUT_FILE by packing members located via PREFIX.
  
  OPTIONS
     -pc indicates that the shader archive is for the retail client.
     -ce indicates that the shader archive is for the Custom Edition client.
     -fx indicates that the shader archive is an effects archive.
        PREFIX defaults to "fx/".
     -vsh indicates that the shader archive is a vertex shaders archive.
        PREFIX defaults to "vsh/".
*/

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdlib>

#include <array>
#include <algorithm>
#include <memory>
#include <string_view>

#include <h1sp/archive.hpp>
#include <h1sp/crypt.hpp>
#include <h1sp/names.hpp>

namespace 
{
    enum class client_version {unspecified, pc, ce};
    enum class archive_type {unspecified, fx, vsh};
    enum class operation_mode {unspecified, unpack, pack};
    
    struct operation_context
    {
        operation_mode mode;   ///< The operation to perform.
        client_version client; ///< The client version of the archive.
        archive_type   type;   ///< The archive type.
        const char*    file;   ///< The file to operate on.
        const char*    prefix; ///< A file prefix for the operation.
    };
    
    enum class unpack_error
    {
        success,
        could_not_read,
        corrupted_data
    };
    
    const char* binpath = "h1sp";
    
    void print_usage();
    
    /**
     * \brief Performs an archive operation using the given parameters.
     *
     * \return `nullptr` on success, or a null-terminated string on error that
     *         indicates the reason for failure.
     */
    const char* perform_operation(const operation_context& op);
}

int main(int argc, char* argv[])
{
    using namespace std::literals::string_view_literals;
    
    operation_context op = {};
    
    if (argc >= 1)
        binpath = argv[0];
    
    if (argc >= 2 && (argv[1] == "-h"sv || argv[1] == "--help"))
    {
        print_usage();
        return EXIT_SUCCESS;
    } else if (
        (argc >= 5 && argc <= 6)                            && 
        (argv[1] == "-u"sv || argv[1] == "--unpack"sv ||
         argv[1] == "-p"sv || argv[1] == "--pack"sv)        &&
        (argv[2] == "-pc"sv || argv[2] == "-ce"sv)          &&
        (argv[3] == "-fx"sv || argv[3] == "-vsh"sv)) 
    {
        op.mode   = (argv[1] == "-u"sv || argv[1] == "--unpack"sv)
                  ? operation_mode::unpack : operation_mode::pack;
        op.client = argv[2] == "-pc"sv ? client_version::pc : client_version::ce;
        op.type   = argv[3] == "-fx"sv ? archive_type::fx : archive_type::vsh;
        op.file   = argv[4];
        if (argc >= 6)                        op.prefix = argv[5];
        else if (op.type == archive_type::fx) op.prefix = "fx/";
        else                                  op.prefix = "vsh/";
    } else 
    {
        std::puts("invalid use\n");
        print_usage();
        return EXIT_FAILURE;
    }
    
    const char* reason = perform_operation(op);
    if (reason != nullptr)
    {
        std::printf("operation failed: %s\n", reason);
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}

namespace 
{
    std::span<const char* const> get_names(
        client_version client, 
        archive_type   type)
    {
        using shader_packager::vs_names;
        using shader_packager::retail_fx_names;
        using shader_packager::custom_fx_names;
        
        switch (type)
        {
        case archive_type::vsh: return vs_names;
        case archive_type::fx:
            switch (client)
            {
            case client_version::pc: return retail_fx_names;
            case client_version::ce: return custom_fx_names;
            default: break;;
            }
        default: break;
        }
        
        return {};
    }
    
    const char* get_archive_member_extension(archive_type type)
    {
        switch (type)
        {
        case archive_type::vsh: return "vsh";
        case archive_type::fx:  return "fx";
        default:                return "";
        }
    }
    
    void print_usage()
    {
        std::printf(
R"(
USAGE
  %s {-h|--help}
  %s {-u|--unpack} {-pc|-ce} {-fx|-vsh} INPUT_FILE [PREFIX] 
    Unpack the shader archive INPUT_FILE by writing each member to files prefixed with PREFIX.
  %s {-p|--pack} {-pc|-ce} {-fx|-vsh} OUTPUT_FILE [PREFIX]
    Creates a shader archive OUTPUT_FILE by packing members located via PREFIX.
  
  OPTIONS
     -pc indicates that the shader archive is for the retail client.
     -ce indicates that the shader archive is for the Custom Edition client.
     -fx indicates that the shader archive is an effects archive.
        PREFIX defaults to "fx/".
     -vsh indicates that the shader archive is a vertex shaders archive.
        PREFIX defaults to "vsh/".
)",
            binpath,
            binpath,
            binpath
        );
    }
    
    // Returns nullptr on success, otherwise returns an error string
    const char* perform_unpack_operation(const operation_context& op)
    {
        using enum shader_packager::archive::read_error;
        
        // load the archive and check for errors
        shader_packager::archive archive;
        {
            const auto error = archive.read_from_file(op.file); 
            switch (error)
            {
            case success:
                break; // continue processing
            case could_not_open_file:
                return "could not open file";
            case archive_data_is_corrupt:
                return "archive is corrupt";
            default:
                return "unknown read error";
            }
        }
        
        // write each archive member to its own file
        {
            const char* prefix = op.prefix;
            const char* extension = get_archive_member_extension(op.type);
            const auto names = get_names(op.client, op.type);
            std::size_t i = 0;
            
            enum unpack_error
            {
                unpack_success,
                unpack_too_many_members,
                unpack_write_error
            };
            
            const unpack_error error = archive.for_each(
            [prefix, extension, names, &i] 
            (std::span<std::byte> data) -> unpack_error
            {
                if (i >= names.size())
                    return unpack_too_many_members;
                
                const char* name = names[i];
                char dstname[1024];
                std::snprintf(dstname, std::size(dstname), "%s%s.%s",
                    prefix, name, extension
                );

                if (!shader_packager::write_file(dstname, data))
                {
                    return unpack_write_error;
                }
                
                ++i;
                return unpack_success;
            }).value_or(unpack_success);
            
            if (error == unpack_success) {
                // DO NOTHING; success
            } else if (error == unpack_too_many_members) {
                // DO NOTHING; Halo does not treat this an error
            } else if (unpack_write_error)
            {
                std::printf("failed to write member %s\n", names[i]);
                return "failed to write member to corresponding file";
            } else if (i < names.size())
            {
                return "loaded archive has missing members";
            }
            
            std::printf("unpacked %d archive members prefixed with %s\n",
                (int)i, prefix);
        }
        
        return nullptr;
    }
    
    const char* perform_pack_operation(const operation_context& op)
    {
        namespace sp = shader_packager;
        
        const auto names = get_names(op.client, op.type);
        std::vector<sp::byte_buffer> filebufs;
        filebufs.reserve(names.size());
        
        // load the member files into memory
        {
            const char* extension = get_archive_member_extension(op.type);
            for (const char* name : names)
            {
                char filepath[1024];
                std::snprintf(filepath, std::size(filepath), "%s%s.%s",
                    op.prefix, name, extension);
                auto filebuf = sp::read_file(filepath);
                if (!filebuf)
                {
                    std::fprintf(stderr, "on file %s: \n", filepath);
                    return "failed to read member file";
                }
                filebufs.push_back(std::move(filebuf));
            }
        }
        
        // put the members into the archive
        sp::archive archive;
        archive.load_members_from(std::span{filebufs.cbegin(), filebufs.cend()});
        
        // write output file
        const auto result = archive.flush_to_file(op.file);
        
        switch (result)
        {
        case sp::archive::write_error::success:
            break;
        case sp::archive::write_error::no_data_to_write:
            return "no data to write";
        case sp::archive::write_error::could_not_open_file:
            return "could not open output file for writing";
        default:
            return "unknown archive::write_error";
        }
        
        return nullptr;
    }
    
    const char* perform_operation(const operation_context& op)
    {
        assert(op.mode != operation_mode::unspecified);
        assert(op.client != client_version::unspecified);
        assert(op.type != archive_type::unspecified);
        assert(op.file != nullptr);
        assert(op.prefix != nullptr);
        
        switch (op.mode)
        {
        case operation_mode::unpack:
            return perform_unpack_operation(op);
        case operation_mode::pack:
            return perform_pack_operation(op);
        default:
            return "unsupported operation mode";
        }
    }
}