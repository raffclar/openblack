/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <span>
#include <string_view>

#include <entt/entity/fwd.hpp>

#include "Creature/CreatureAnimationLog.h"

namespace openblack::ecs::systems
{

/// The record of the clips that each creature's body plays and of the clips that did not play, for the debug inspector
/// (see creature_animation_log::Log). It is there only in builds with the inspector, after the inspector starts. Without
/// it, the bodies record nothing and pay nothing for it. The game turns never read it, so it does not change the world.
class CreatureAnimationLogSystemInterface
{
public:
	virtual ~CreatureAnimationLogSystemInterface() = default;

	/// The pose of a creature's turn used these clips
	virtual void Posed(entt::entity creature, uint32_t turn, std::span<const creature_animation_log::Clip> clips,
	                   const creature_animation_log::Cause& cause) = 0;
	/// A clip that something asked a creature's body for did not play
	virtual void Note(entt::entity creature, creature_animation_log::ProblemEntry problem) = 0;
	/// The body of a creature refused a clip, because it played another action. `by` is the part of the game that asked.
	virtual void Refused(entt::entity creature, creature_animation_log::Layer layer, size_t animation, std::string_view by,
	                     size_t playing) = 0;
	/// The log of a creature, or none when it has none yet
	[[nodiscard]] virtual const creature_animation_log::Log* Find(entt::entity creature) const = 0;
	/// Removes the logs of all the creatures
	virtual void Clear() = 0;
};

} // namespace openblack::ecs::systems
