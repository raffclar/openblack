/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>

namespace openblack::ecs::physics
{
class FragMesh;
}

namespace openblack::ecs::components
{
/// An abode's destruction mesh: a building a rock knocked pieces out of, drawn from its FragMesh.
struct BuildingDamage
{
	/// never changed once set (openblack, for the draw snapshot): an impact or a merge makes a new FragMesh and puts it
	/// here (Buildings.cpp), so a FragMeshDraw taken earlier keeps the old one
	std::shared_ptr<const physics::FragMesh> mesh;
	/// the FragMesh's last hitter (the pass-through pair of an abode's physics impact; setting up the physics object
	/// clears it). Kept here so that the FragMesh stays unchanged; it goes with the FragMesh (RemoveDamage)
	entt::entity lastHitter {entt::null};
	entt::id_type intactMesh {0}; ///< the building's own mesh: its Mesh component, which the damage never changes
	/// the FragMesh's model (FragMesh::BuildMesh, with the partly built draw over it), drawn as the building's
	/// components::DrawMesh while that DrawMesh holds this id. Kept here as well because the DrawMesh can be taken away
	/// without erasing it (abodes::RedrawConstruction's Remove before its on_destroy sink is connected): physics erases
	/// it again (a no-op once OnDrawMeshDestroyed has done it)
	entt::id_type generatedMesh {0};
	bool morphed {false}; ///< it had MorphWithTerrain (the FragMesh bakes the morph in)
};

/// Fragment (a kind of rock): a piece knocked off a building. It only hits the landscape, cannot be picked
/// up, and vanishes after 100 turns per triangle.
struct Fragment
{
	std::shared_ptr<const physics::FragMesh> mesh; ///< never changed once the fragment is made
	entt::entity parent {entt::null};
	entt::id_type generatedMesh {0};
	int turnsLeft {0};
	float area {0.0f};
};
} // namespace openblack::ecs::components
