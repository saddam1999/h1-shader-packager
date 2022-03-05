// SPDX-License-Identifier: BSL-1.0

#ifndef H1SP_IO_HPP
#define H1SP_IO_HPP

#include <cstddef>

#include <array>
#include <bit>
#include <concepts>
#include <iterator>
#include <type_traits>

/**
 * Some of this stuff seems quite useful, so I will probably end up adding it 
 * to another library.
 */

static_assert(
    std::endian::native == std::endian::little ||
    std::endian::native == std::endian::big,
    "mixed endian is unsupported"
);

// =============================================================================
// =============================================================================
// SYNOPSIS
// The following function templates are provided to assist with serializing 
// integral (except bool) types from/to their byte representations. 
//
// They are written to produce minimal assembly, i.e. rather than multiple loads
// per byte, a single, unaligned load is used to read all the bytes at once when
// the iterator used is a pointer to `std::byte`.
//  -bswap will emit the instruction to swap a byte order.
//  -deserialize will emit a single load and bswap, if necessary.
//  -serialize will emit a bswap, if necessary, and a single store.
// The more generic versions that accept endian as a function argument will 
// generally be branchless.
//
// Note:
//  ARM64 GCC appears to have some minor trouble inlining bswap when 
//  std::byteswap is not available and opts to load into a SIMD/FP register
//  to reverse the bytes.

namespace shader_packager
{
    /**
     * \brief Loads an integral of type \a T from \a it, in native byte-order.
     *
     * \param [in] it The input iterator providing the bytes to load from.
     *                `[it, it + sizeof(T))` must be a valid range.
     *
     * \tparam T  The integral type to load.
     * \tparam It The input iterator type. `std::iter_value_t<It>` must be 
     *            (possibly `cv`-qualified) `std::byte`.
     *
     * \return The loaded integral value.
     */
    template<std::integral T, std::input_iterator It>
        requires (
            !std::same_as<bool, T> &&
            std::same_as<std::byte, std::remove_cv_t<std::iter_value_t<It>>>
        )
    [[nodiscard]] constexpr T emit_load(It it) noexcept;
    
    /**
     * \brief Stores the bytes of \a n into \a d_it in native byte-order.
     *
     * \param [in]  n    The integral to store.
     * \param [out] d_it The output iterator receiving the bytes of \a n.
     *                   Must be able to accept all `sizeof(n)` bytes.
     */
    template<std::integral T, std::output_iterator<const std::byte&> OutputIt>
        requires (!std::same_as<bool, T>)
    constexpr OutputIt emit_store(const T n, OutputIt d_it) noexcept;
    
    /**
     * \brief Reverses the order of the bytes of \a n.
     *
     * \return An integral of type \a T consisting of the bytes of \a n in 
     *         reverse order.
     */
    template<std::integral T>
        requires (!std::same_as<T, bool>)
    [[nodiscard]] constexpr T bswap(const T n) noexcept;
    
    /**
     * \brief Computes an integral that has a byte representation that is 
     *        equivalent in value under \a To endianness to the value of the 
     *        byte representation of \a n under \a From endianness.
     *
     * This function can be used to convert a value loaded (e.g. by #emit_load)
     * where the underlying data was known to be stored with \a From endianness
     * to a value that can be stored (e.g. by #emit_store) in \a To endianness.
     *
     * \param [in] n The integral to convert.
     *
     * \tparam From The endianness used to interpret the bytes of \a n.
     * \tparam To   The destination endianness.
     * 
     * \return The converted integral.
     */
    template<
        std::endian From, 
        std::endian To = std::endian::native, 
        std::integral T
    >
        requires (!std::same_as<T, bool>)
    [[nodiscard]] constexpr T convert_endian(const T n) noexcept;
    
    /**
     * \brief Computes an integral that has a byte representation that is 
     *        equivalent in value under \a to endianness to the value of the 
     *        byte representation of \a n under \a from endianness.
     *
     * This function can be used to convert a value loaded (e.g. by #emit_load)
     * where the underlying data was known to be stored with \a from endianness
     * to a value that can be stored (e.g. by #emit_store) in \a to endianness.
     *
     * \param [in] n    The integral to convert.
     * \param [in] from The endianness used to interpret the bytes of \a n.
     * \param [in] to   The destination endianness.
     * 
     * \return The converted integral.
     */
    template<std::integral T>
        requires (!std::same_as<T, bool>)
    [[nodiscard]] constexpr T convert_endian(
        const T n, 
        const std::endian from, 
        const std::endian to = std::endian::native) noexcept;
    
    /**
     * \brief Loads an integral of type \a To from the iterator \a it by 
     *        interpreting the bytes with \a Endian endianness.
     *
     * \param [in] it The input iterator providing the bytes to load from.
     *                `[it, it + sizeof(T))` must be a valid range.
     *
     * \tparam To     The integral type to deserialize.
     * \tparam Endian The endianness of the data loaded through \a it.
     * \tparam It     The input iterator type. `std::iter_value_t<It>` must be 
     *                (possibly `cv`-qualified) `std::byte`.
     *
     * \return The deserialized value.
     */
    template<std::integral To, std::endian Endian, std::input_iterator It>
        requires (
            !std::same_as<bool, To> &&
            std::same_as<std::remove_cv_t<std::iter_value_t<It>>, std::byte>
        )
    [[nodiscard]] constexpr To deserialize(It it) noexcept;
    
    /**
     * \brief Loads an integral of type \a To from the iterator \a it by 
     *        interpreting the bytes with \a endian endianness.
     *
     * \param [in] it     The input iterator providing the bytes to load from.
     *                    `[it, it + sizeof(T))` must be a valid range.
     * \param [in] endian The endianness of the data loaded through \a it.
     *
     * \tparam To The integral type to deserialize.
     * \tparam It The input iterator type. `std::iter_value_t<It>` must be 
     *            (possibly `cv`-qualified) `std::byte`.
     *
     * \return The deserialized value.
     */
    template<std::integral To, std::input_iterator It>
        requires (
            !std::same_as<bool, To> &&
            std::same_as<std::remove_cv_t<std::iter_value_t<It>>, std::byte>
        )
    [[nodiscard]] constexpr To deserialize(
        It it, 
        const std::endian endian) noexcept;
    
    /**
     * \brief Stores the bytes of \a n at \a d_it, in \a Endian byte-order.
     *
     * \param [in]  n    The integral to serialize.
     * \param [out] d_it The output iterator receiving the bytes of \a n.
     *                   Must be able to accept all `sizeof(n)` bytes.
     * 
     * \tparam Endian The endianness that determines the order in which the 
     *                bytes of \a n are stored.
     */
    template<
        std::endian Endian, 
        std::output_iterator<const std::byte&> OutputIt, 
        std::integral T
    >
        requires (!std::same_as<bool, T>)
    constexpr OutputIt serialize(const T n, OutputIt d_it) noexcept;
    
    /**
     * \brief Stores the bytes of \a n at \a d_it, in \a endian byte-order.
     *
     * \param [in]  n      The integral to serialize.
     * \param [out] d_it   The output iterator receiving the bytes of \a n.
     *                     Must be able to accept all `sizeof(n)` bytes.
     * \param [in]  endian The endianness that determines the order in which the
     *                     bytes of \a n are stored.
     */
    template<std::output_iterator<const std::byte&> OutputIt, std::integral T>
        requires (!std::same_as<bool, T>)
    constexpr OutputIt serialize(const T n, OutputIt d_it, const std::endian endian)
        noexcept;
}

// =============================================================================
// =============================================================================
// IMPLEMENTATION
//

namespace shader_packager
{
    template<std::integral T, std::input_iterator It>
        requires (
            !std::same_as<bool, T> &&
            std::same_as<std::byte, std::remove_cv_t<std::iter_value_t<It>>>
        )
    [[nodiscard]] constexpr T emit_load(It it) noexcept
    {
        // unrolls just fine, gcc/clang recognize what we want
        const auto bytes = [&it] {
            std::array<std::byte, sizeof(T)> bytes{};
            for (auto& b : bytes)
                b = *it++;
            return bytes;
        }();
        return std::bit_cast<T>(bytes);
    }

    template<std::integral T, std::output_iterator<const std::byte&> OutputIt>
        requires (!std::same_as<bool, T>)
    constexpr OutputIt emit_store(const T n, OutputIt d_it) noexcept
    {
        // unrolls just fine, gcc/clang recognize what we want
        using byte_array = std::array<std::byte, sizeof(n)>;
        const auto bytes = std::bit_cast<byte_array>(n);
        for (const auto& b : bytes)
            *d_it++ = b;
        
        return d_it;
    }
    
    template<std::integral T>
        requires (!std::same_as<T, bool>)
    [[nodiscard]] constexpr T bswap(const T n) noexcept
    {
#if __cpp_lib_byteswap >= 202110L
        return std::byteswap(n);
#else
        const auto bytes = std::bit_cast<std::array<std::byte, sizeof(n)>>(n);
        return emit_load<T>(bytes.rbegin());
#endif
    }
    
    template<std::endian From, std::endian To, std::integral T>
        requires (!std::same_as<T, bool>)
    [[nodiscard]] constexpr T convert_endian(const T n) noexcept
    {
        // Assuming From/To are only little/big
        // then we just need to bswap if From != To
        if constexpr (From == To)
            return n;
        else // From != To
            return bswap(n);
    }
    
    template<std::integral T>
        requires (!std::same_as<T, bool>)
    [[nodiscard]] constexpr T convert_endian(
        const T n, 
        const std::endian from, 
        const std::endian to) noexcept
    {
        return from != to ? bswap(n) : n;
    }
    
    template<std::integral To, std::endian Endian, std::input_iterator It>
        requires (
            !std::same_as<bool, To> &&
            std::same_as<std::remove_cv_t<std::iter_value_t<It>>, std::byte>
        )
    [[nodiscard]] constexpr To deserialize(It it) noexcept
    {
        return convert_endian<Endian, std::endian::native>(emit_load<To>(it));
    }
    
    template<std::integral To, std::input_iterator It>
        requires (
            !std::same_as<bool, To> &&
            std::same_as<std::remove_cv_t<std::iter_value_t<It>>, std::byte>
        )
    [[nodiscard]] constexpr To deserialize(
        It it, 
        const std::endian endian) noexcept
    {
        // produces branchless code on gcc 11.2 (x86, x86-64, ARM64)
        return convert_endian(emit_load<To>(it), endian, std::endian::native);
    }
    
    template<
        std::endian Endian, 
        std::output_iterator<const std::byte&> OutputIt, 
        std::integral T
    >
        requires (!std::same_as<bool, T>)
    constexpr OutputIt serialize(const T n, OutputIt d_it) noexcept
    {
        return emit_store(convert_endian<std::endian::native, Endian>(n), d_it);
    }
    
    template<std::output_iterator<const std::byte&> OutputIt, std::integral T>
        requires (!std::same_as<bool, T>)
    constexpr OutputIt serialize(const T n, OutputIt d_it, const std::endian endian)
        noexcept
    {
        // produces branchless code on gcc 11.2 (x86, x86-64, ARM64)
        return emit_store(
            convert_endian(n, std::endian::native, endian),
            d_it
        );
    }
}

#endif // H1SP_IO_HPP