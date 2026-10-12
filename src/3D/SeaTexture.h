/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <string_view>

#include <entt/core/hashed_string.hpp>

/// The sea's texture: its colours in Sky.raw (oddly enough), its alpha in Skya.raw, 256 texels a side. The game keeps
/// it four bits a channel, as it keeps every texture it loads with an alpha.
namespace openblack::sea_texture
{
inline constexpr entt::hashed_string k_Id = entt::hashed_string("ocean/sea");
inline constexpr std::string_view k_ColourFile = "Sky.raw";
inline constexpr std::string_view k_AlphaFile = "Skya.raw";
inline constexpr uint16_t k_Side = 256;
} // namespace openblack::sea_texture
