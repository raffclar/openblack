/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::ecs::systems
{

/// The creatures' pens at their players' temples (see temple_pen). Each game turn, a creature whose player has a
/// temple has its home moved to that temple's pen, and it is shown smaller the further it walks into the pen
/// (components::Creature::penSize), growing back as it walks out; its own size is kept.
class CreaturePenSystemInterface
{
public:
	virtual ~CreaturePenSystemInterface() = default;

	/// Once a game turn, after the creatures have grown
	virtual void ProcessTurn() = 0;
};

} // namespace openblack::ecs::systems
