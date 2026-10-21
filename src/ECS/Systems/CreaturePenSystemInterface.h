/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/fwd.hpp>

namespace openblack::ecs::systems
{

/// The creatures' pens at their players' temples (see temple_pen). Each game turn, a creature whose player has a
/// temple has its home moved to that temple's pen, even while the temple is still going up. Once the temple is built,
/// the creature is shown smaller the further it walks into the pen (components::Creature::penSize), growing back as it
/// walks out; its own size is kept.
class CreaturePenSystemInterface
{
public:
	virtual ~CreaturePenSystemInterface() = default;

	/// Once a game turn, for each creature
	virtual void ProcessTurn() = 0;
	/// In the creature's own turn, before its leash and its body
	virtual void CreatureTurn(entt::entity /*creature*/) {}
};

} // namespace openblack::ecs::systems
