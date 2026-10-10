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

#include <optional>
#include <vector>

#include <entt/fwd.hpp>

#include "Scenery/AnimatedStaticRules.h"

namespace openblack::ecs::systems
{

/// The animated scenery the land scripts place (the Norse gate of the creatures' glade, the gate stone plinth, the
/// piper's cave entrance and the phone box): opened and closed by the scripts, played on its clip as it is drawn, and
/// the plinth taking the gate stones the hand gives it
class AnimatedStaticSystemInterface
{
public:
	virtual ~AnimatedStaticSystemInterface() = default;

	/// A script opens (1) or closes (0) an animated static; false for anything else
	virtual bool SetOpenState(entt::entity object, int32_t openState) = 0;

	/// What the gate stones in an animated static are worth to the scripts; none for anything else
	[[nodiscard]] virtual std::optional<uint32_t> GateStoneValue(entt::entity object) const = 0;

	/// A gate stone given to the plinth by the hand: true when the plinth takes it, the stone's model then laid in its
	/// first empty slot (none when it is full). The giver uses the stone up.
	virtual bool LayGateStone(entt::entity plinth, entt::entity stone) = 0;

	/// The circles a creature plans its route round for a Norse gate, which stand in for its one bounding circle; none
	/// for anything else
	[[nodiscard]] virtual std::optional<std::vector<animated_static::RouteCircle>> RouteCircles(entt::entity gate) const = 0;
	/// Once a frame: each animated static's clip plays on by the milliseconds of the game's clock since the last frame
	/// and its model is posed, and the plinths' stones are placed
	virtual void Update(uint32_t turn, float turnFraction) = 0;

	/// The stone models drawn on the plinths go with the level
	virtual void Reset() = 0;
};

} // namespace openblack::ecs::systems
