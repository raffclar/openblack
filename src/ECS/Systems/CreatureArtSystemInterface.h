/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/core/fwd.hpp>

#include "Enums.h"

namespace openblack::ecs::systems
{

/// Asks the loading threads for the art of a creature species: its animations, its seven meshes and the files that it
/// paints its skins from (see creature_art). The web game holds this art back from the start (see RegisterWorldData),
/// because the game gets each file from the site when it first reads it. The system asks for the art of a species
/// before the first frame of a creature of the species, when the game knows the species:
/// - the creatures in the world when a land or a saved game loads, behind the loading screen;
/// - the species that the scripts which start can make with a fixed species, while the game plays;
/// - the species of each new creature, as the game makes it.
/// The desktop game loads all the art at the start, and the system does nothing there.
class CreatureArtSystemInterface
{
public:
	virtual ~CreatureArtSystemInterface() = default;

	/// Records that the game holds back the animations and the meshes of the species
	virtual void HoldSpecies(CreatureType species) = 0;
	/// Records a held file of a mesh that the species paints its skins from
	virtual void HoldSkinFile(CreatureType species, entt::id_type file) = 0;

	/// Asks for the held art of the species, one time
	virtual void AskFor(CreatureType species) = 0;
	/// Asks for the art of the species of each creature in the world. A new land or a saved game starts the scan of
	/// the scripts again.
	virtual void AskForWorld() = 0;
	/// Once a frame: asks for the species that the scripts which started since the last frame can make
	virtual void Update() = 0;
};

} // namespace openblack::ecs::systems
