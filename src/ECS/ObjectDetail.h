/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

/// Which of the land's objects are drawn in less detail the further into the view they stand (see
/// graphics::mesh_detail), and which of those fade out and then go in the distance. Every object on the land is, by its
/// size: buildings, trees, features, fields, piles, animals, villagers. The creatures, the hand and the temple are always
/// drawn whole. Some things never fade: whatever a script made or keeps in view, the birds, the village centre, the ball,
/// the physical shield, a script's highlight, the vortex, and anything moving in the physics while it does.
namespace openblack::ecs
{
class Registry;
} // namespace openblack::ecs

namespace openblack::ecs::object_detail
{

/// Whether an object is drawn by the distance bands at all: what it is decides it, so this only changes when the
/// object's draw layout does (see k_ChangesDrawLayout)
[[nodiscard]] bool Banded(const Registry& registry, entt::entity entity);

/// Whether an object drawn by the bands is drawn by them this frame: not while a hand or a creature holds it, which
/// draws it whole, nor while a script draws it in high detail
[[nodiscard]] bool BandedNow(const Registry& registry, entt::entity entity);

/// Whether an object drawn by the bands fades out and then goes beyond them; one that doesn't is drawn as its low mesh
/// however far it is. `townCentre` says it is a village centre.
[[nodiscard]] bool Disappears(const Registry& registry, entt::entity entity, bool townCentre);

} // namespace openblack::ecs::object_detail
