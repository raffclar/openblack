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

/// EXPERIMENT: the undead cow, a creature with a cow's mind and settings and a body imported from a glTF file. It is no
/// part of the original game and must not be merged.
namespace openblack::experiment_creature
{

/// The files the undead cow is read from, in the folder the build names (OPENBLACK_EXPERIMENT_CREATURE_DIR)
constexpr std::string_view k_ModelFile = "undead_cow.glb";
/// The texture painted over the model's UV layout. Any square PNG; the game keeps it at 256 by 256 texels.
constexpr std::string_view k_TextureFile = "undead_cow_texture.png";

/// Loads the undead cow's mesh into the mesh cache and its rig into the rig cache, made from the cow's rig and the
/// model's animations. Does nothing when the model's file is not there. Call it after the creature rigs are registered.
void Load();

} // namespace openblack::experiment_creature
