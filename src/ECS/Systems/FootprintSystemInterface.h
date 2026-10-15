/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <chrono>
#include <optional>
#include <span>

#include <entt/entity/fwd.hpp>

#include "Creature/CreatureFootprints.h"

namespace openblack::ecs::systems
{

/// The prints the creatures leave on the land as they walk, laid as their footsteps fall and fading away over about
/// five seconds (see creature_footprints). They show and fade only on frames that draw the land. A change of land
/// keeps them, as the game does. A save does not keep them.
class FootprintSystemInterface
{
public:
	virtual ~FootprintSystemInterface() = default;

	/// Fades the prints as the game time passes, only on a frame that draws the land
	virtual void Update(std::chrono::duration<float, std::milli> gameTime, bool landDrawn) = 0;
	/// Lays a print under a creature's lower foot, as one of its footsteps falls, if there is room for it
	virtual void Step(entt::entity creature) = 0;
	/// Takes every print away, for a new testbed scenario. The game never takes them away all at once.
	virtual void Reset() = 0;

	/// The prints showing, the oldest first
	[[nodiscard]] virtual std::span<const creature_footprints::Footprint> GetPrints() const = 0;
	/// How many prints were dropped because there were already as many as there can be
	[[nodiscard]] virtual size_t GetDroppedCount() const = 0;

	/// Whether the prints are drawn, for trying things out
	[[nodiscard]] virtual bool IsShown() const = 0;
	virtual void SetShown(bool shown) = 0;
	/// Whether every creature leaves smiley faces: by the date when unset, as the game does on the first of April
	[[nodiscard]] virtual std::optional<bool> GetAprilFoolsOverride() const = 0;
	virtual void SetAprilFoolsOverride(std::optional<bool> aprilFools) = 0;
	/// Whether the prints laid now are smiley faces
	[[nodiscard]] virtual bool IsAprilFools() const = 0;
};

} // namespace openblack::ecs::systems
