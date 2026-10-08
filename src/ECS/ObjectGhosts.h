/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <vector>

#include <entt/entity/fwd.hpp>

#include "Particles/Creators/Mesh.h"

// The ghost of an object a store took or a pot that was merged: a copy of its model drawn for 500 ms where it was, as
// it melts away (graphics::frame_anim::GoolooFrame). Draw state only: no entity, nothing the game logic reads. Wiki:
// docs/bw1-notes/rendering-objects.md, the Gooloo row.

namespace openblack::ecs::object_ghosts
{
/// A ghost of the object's model as it is drawn now (its mesh, its matrix), for 500 ms. Nothing for an object
/// without a mesh
void Add(entt::entity object);
/// Each ghost's time falls by the frame's ms; the ones that ran out go
void Update(float frameMs);
/// The number of ghosts
[[nodiscard]] size_t Count();
/// This frame's draws: each ghost twice, first with its moving texture offset, then at no offset in render mode 10
[[nodiscard]] std::vector<psys::mesh_atoms::Instance> Instances();
/// Every ghost goes (a new map)
void Clear();
} // namespace openblack::ecs::object_ghosts
