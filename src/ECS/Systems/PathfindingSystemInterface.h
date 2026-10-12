/*******************************************************************************
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
class PathfindingSystemInterface
{
public:
	/// Every walker takes its turn's walk
	virtual void Update() = 0;
	/// At the end of the turn, one walker that came up against water, the land's edge or a fence while going round
	/// tries its walk both ways round, and is turned round if the other way is better
	virtual void ProcessLookahead() = 0;
};
} // namespace openblack::ecs::systems
