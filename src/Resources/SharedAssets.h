/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <string_view>

#include <entt/core/hashed_string.hpp>

namespace openblack::resources::shared_assets
{

/// Power_Up_Band.L3d (Data/Spells/Meshes), the band mesh the hand's miracle FX and the worship spell seeds share. Game.cpp
/// does not load it: whoever needs it first loads it
inline constexpr auto k_PowerUpBandMesh = entt::hashed_string("Power_Up_Band");

/// S_SpriteSheet3a.raw (Data/Textures) as raw/S_SpriteSheet3a, the sprite sheet of the fireflies and the hand's grip dust
inline constexpr auto k_SpriteSheet3a = entt::hashed_string("raw/S_SpriteSheet3a");

enum class LoadResult
{
	AlreadyLoaded,
	LoadedNow,
	Failed,
};

/// The band mesh in the mesh cache unless it is there already, with its material: additive (SRCALPHA / ONE), two-sided,
/// no Z write. Failed without the resources or the file system; a read that fails is warned as
/// "<label>: cannot load Power_Up_Band.L3d: <error>"
LoadResult LoadPowerUpBand(std::string_view label);

/// The sprite sheet in the texture cache unless it is there already; a read that fails is warned as
/// "<label>: cannot load S_SpriteSheet3a.raw: <error>"
LoadResult LoadSpriteSheet3a(std::string_view label);

} // namespace openblack::resources::shared_assets
