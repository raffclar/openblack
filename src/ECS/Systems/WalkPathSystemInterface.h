/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <entt/entity/fwd.hpp>

namespace openblack::ecs::systems
{

/// The things the scripts walk along the camera editor's tracks
class WalkPathSystemInterface
{
public:
	virtual ~WalkPathSystemInterface() = default;

	/// A thing starts walking track `number` from `from` to `to`, shares of the track's way, along the track or from its
	/// end back. A thing already walking one walks the new one instead. False when there is no such track.
	virtual bool Start(entt::entity thing, int32_t number, bool forward, float from, float to) = 0;
	/// Each turn: each thing walking goes on along its track, and stops once it has gone its share of the way
	virtual void ProcessTurn() = 0;
};

} // namespace openblack::ecs::systems
