/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <map>
#include <string>

#include <entt/fwd.hpp>

#include "Enums.h"

namespace openblack::creature
{
class CreatureBody
{
public:
	enum class Appearance : uint32_t
	{
		Unknown,
		Base,
		Good,
		Evil,
		Strong,
		Weak,
		Fat,
		Thin
	};
};

/**
 * Returns an ECS ID representing the mesh resource for the given creature mesh file name.
 * For example, "A_Tiger2_Evil" returns the "evil" variant mesh resource ID and
 * "A_Tiger2_Base" returns the "base" variant.
 * @param fileName The creature mesh file name
 * @return A unique ID pointing to the mesh
 */
entt::id_type GetIdFromMeshName(const std::string& fileName);
entt::id_type GetIdFromType(CreatureType species, CreatureBody::Appearance appearance);
/// The species a creature mesh file name is of, as GetIdFromMeshName reads it, or Unknown
[[nodiscard]] CreatureType GetSpeciesFromMeshName(const std::string& fileName);
/// The id of what moves the species' body (creature::CreatureRig)
[[nodiscard]] entt::id_type GetRigId(CreatureType species);

/// The species' row in the game's creature tables, which start with the Giant Ape and then follow the species in order
[[nodiscard]] constexpr size_t InfoRow(CreatureType species)
{
	if (species == CreatureType::UndeadCow)
	{
		// EXPERIMENT: the undead cow uses the cow's row of every species table
		return static_cast<size_t>(CreatureType::Cow);
	}
	return species == CreatureType::GiantApe ? 0 : static_cast<size_t>(species);
}

/// EXPERIMENT: whether a species' body changes with its alignment, fatness and strength. The undead cow is a skeleton
/// with one mesh: it always shows that mesh as it is.
[[nodiscard]] constexpr bool HasBodyMorph(CreatureType species)
{
	return species != CreatureType::UndeadCow;
}
} // namespace openblack::creature
