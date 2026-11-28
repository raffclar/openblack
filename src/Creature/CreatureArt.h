/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_set>
#include <vector>

#include <LHVMTypes.h>
#include <entt/core/fwd.hpp>

#include "3D/CreatureBody.h"
#include "Enums.h"

/// The art of a creature species: its animations (the rig), its seven meshes and the files of the meshes that it paints
/// its skins from. The web game gets the art of a species only when the game asks for it (see
/// ecs::systems::CreatureArtSystemInterface).
namespace openblack::creature_art
{

/// The looks of the seven meshes of a species
constexpr std::array<creature::CreatureBody::Appearance, 7> k_MeshLooks = {
    creature::CreatureBody::Appearance::Base,   creature::CreatureBody::Appearance::Evil,
    creature::CreatureBody::Appearance::Good,   creature::CreatureBody::Appearance::Thin,
    creature::CreatureBody::Appearance::Fat,    creature::CreatureBody::Appearance::Weak,
    creature::CreatureBody::Appearance::Strong,
};

/// The ids of the seven meshes of the species, in the order of k_MeshLooks
[[nodiscard]] std::array<entt::id_type, k_MeshLooks.size()> MeshIds(CreatureType species);

/// The species whose animations a file of the folder Data/CTR keeps, from the name of the file without its extension,
/// in upper or lower case. Unknown for a name that is not the name of a species' file.
[[nodiscard]] CreatureType SpeciesOfAnimationFile(std::string_view name);

/// Whether a creature paints its skins from the file of the mesh of this look. A creature paints them from its base,
/// evil and good meshes.
[[nodiscard]] constexpr bool PaintsSkinsFrom(creature::CreatureBody::Appearance look)
{
	using enum creature::CreatureBody::Appearance;
	return look == Base || look == Evil || look == Good;
}

/// The code of a script: the addresses of its first instruction and of the instruction after its last
struct ScriptCode
{
	uint32_t begin {0};
	uint32_t end {0};
};

/// The numbers of the natives that make a thing of a type and a subtype that the script pushes first, such as CREATE
using MakeNatives = std::span<const uint32_t>;

/// The code of the script of a number, or nothing when there is no such script
using CodeOfScript = std::function<std::optional<ScriptCode>(uint32_t scriptId)>;

/// The species of the creatures that the code makes with a fixed species, and that the scripts it runs make, and so on.
/// A creature that the code makes is a push of the creature type of the scripts, then a push of a fixed species, then a
/// call to a native of `makeNatives` on the same line of the script. The code does not scan a script again whose first
/// address is in `scanned`, and it adds the first address of each script that it scans. With `followRuns` false, it
/// scans only the code of the script.
[[nodiscard]] std::vector<CreatureType> SpeciesMade(std::span<const lhvm::VMInstruction> code, ScriptCode script,
                                                    MakeNatives makeNatives, const CodeOfScript& codeOf, bool followRuns,
                                                    std::unordered_set<uint32_t>& scanned);

} // namespace openblack::creature_art
