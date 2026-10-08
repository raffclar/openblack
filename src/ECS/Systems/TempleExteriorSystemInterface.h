/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/entity/fwd.hpp>

#include "3D/TempleExteriorMorph.h"

namespace openblack::ecs::systems
{

/// The temples' outsides. A temple's heart starts neutral and small, and its mesh is blended for that as it is made.
/// Then each turn, from the citadel's process, its look moves toward its player's alignment and share of influence,
/// and its mesh and texture are blended afresh once the look has moved far enough (components::TempleExterior)
class TempleExteriorSystemInterface
{
public:
	virtual ~TempleExteriorSystemInterface() = default;

	/// The heart is made (CitadelArchetype::CreateHeart): its outside's start, and the first blend
	virtual void Create(entt::entity heart) = 0;
	/// The turn's targets taken and stepped toward; true when the mesh is now to be blended again. The caller takes
	/// the heart out of the map cells around that Blend, as the game does
	[[nodiscard]] virtual bool Step(entt::entity heart, float alignmentTarget, float sizeTarget) = 0;
	/// The heart's own mesh blended for its look and laid on the land, and its texture blended, when a blend is due
	virtual void Blend(entt::entity heart) = 0;

	/// Where the heart's outside is, where it heads and what its mesh was last blended for; none when it has no outside
	[[nodiscard]] virtual std::optional<TempleExteriorMorph::State> GetLook(entt::entity heart) const = 0;
	/// For the debug window only: the outside's alignment taken to the target at once, instead of a step a turn, and
	/// its mesh blended again when that is due, out of the map cells around the blend. The turns go on from there
	virtual void SnapAlignment(entt::entity heart, float alignmentTarget) = 0;
};

} // namespace openblack::ecs::systems
