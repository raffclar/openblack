/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "3D/LandMorph.h"

namespace openblack::ecs::components
{

/// Meshes with this component will have their vertices match terrain height maps: the melting of land_morph
/// (here vs_object_hm_instanced)
///
/// They correspond with the original's 3D type MORPHABLE or CITADEL
/// The following Objects should be created with this component:
/// * BigForest
/// * Graveyard
/// * PileFood (not wood)
/// * StoragePit x2 meshes
/// * Wonder (Depends on tribe type but defaults to morphable)
/// * Workshop
/// * CitadelHeart
/// * CitadelPart
/// * Creche
/// * Football
/// * PhysicalShield (3D type 1; the magic shield itself is a static object), Live
/// * TownCentre
/// * Field
/// * the ground marks (ecs/GroundMarks.h): an uprooted tree's crater, an explosion's mark
/// * DesignedWaterFall's ark and dinosaur (morphable, melted every frame)
struct MorphWithTerrain
{
	/// When the original takes the deltas (land_morph::Melting: Snapshot at creation, Live on every draw)
	land_morph::Melting mode {land_morph::Melting::Snapshot};
};

} // namespace openblack::ecs::components
