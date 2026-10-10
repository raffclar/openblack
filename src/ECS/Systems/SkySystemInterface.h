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

#include "3D/DayNightClock.h"

namespace openblack::ecs::systems
{

/// The world's sky: the dome with its clock of day and night (components::SkyDome, components::DayNightCycle), the sun
/// (components::Sun) and the moon (components::Moon). The clock moves on a turn at a time; the sun, the moon and the
/// dome's blend are worked out once a frame, before it is drawn.
class SkySystemInterface
{
public:
	virtual ~SkySystemInterface() = default;

	/// Makes the sky's entities, with the sky kept from the last land when there is one
	virtual void Initialize() = 0;
	/// Keeps the sky as it is while the last land's entities go, for Initialize to make it again
	virtual void KeepForNextLand() = 0;

	/// One turn of game time
	virtual void ProcessTurn() = 0;
	/// Once a frame, just before it is drawn: the sun and moon for the hour and the date, and the rows of the dome to
	/// blend again while the sky is drawn
	virtual void UpdateFrame(bool skyDrawn) = 0;

	/// Jumps to an hour of script time, the dome built again for it at once
	virtual void SetTime(float time) = 0;
	/// The sky of the visual time: 0 at night, 1 at dusk and 2 by day, between them as it turns
	[[nodiscard]] virtual float GetCurrentSkyType() const = 0;
	/// The visual time, between 0 and 24 in hours
	[[nodiscard]] virtual float GetTime() const = 0;
	/// The hours of the visual time the sky turns at
	[[nodiscard]] virtual DayNightTimes GetDayNightTimes() const = 0;
	/// The clock of day and night the sky follows
	[[nodiscard]] virtual DayNightClock& GetClock() = 0;
	[[nodiscard]] virtual const DayNightClock& GetClock() const = 0;

	/// The sky's entities: the dome, which holds the clock, the sun and the moon
	[[nodiscard]] virtual entt::entity GetDome() const = 0;
	[[nodiscard]] virtual entt::entity GetSun() const = 0;
	[[nodiscard]] virtual entt::entity GetMoon() const = 0;
};

} // namespace openblack::ecs::systems
