// SPDX-License-Identifier: BSL-1.0

#ifndef H1SP_NAMES_HPP
#define H1SP_NAMES_HPP

#include <array>

namespace shader_packager
{
    /**
     * \brief The names (in order) of the effects for PC (retail).
     */
    extern const std::array<const char*, 122> retail_fx_names;
    
    /**
     * \brief The names (in order) of the effects for Custom Edition.
     */
    extern const std::array<const char*, 120> custom_fx_names;
    
    /**
     * \brief The names (in order) of the vertex shaders.
     */
    extern const std::array<const char*, 64>  vs_names;
}

#endif // H1SP_NAMES_HPP