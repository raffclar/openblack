/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "3D/DayNightClock.h"
#include "ECS/Archetypes/SkyArchetype.h"

namespace openblack::ecs::systems
{

/// The sky's clock of day and night (components::DayNightCycle), which the turns, the scripts and the saved world read.
/// The clock moves on a turn at a time, on every computer and on the dedicated server.
///
/// The system also keeps the sky's entities: the dome, the sun and the moon. They are entities of the world on every
/// computer, so that each later entity gets the same id. The presentation (SkySystemInterface) draws them.
class SkyClockSystemInterface
{
public:
	virtual ~SkyClockSystemInterface() = default;

	/// Makes the sky's entities, with the sky kept from the last land when there is one
	virtual void Initialize() = 0;
	/// Keeps the sky as it is while the last land's entities go, for Initialize to make it again
	virtual void KeepForNextLand() = 0;
	/// After the load of a saved world: the system finds the sky's entities among the loaded entities
	virtual void FindAfterLoad() = 0;

	/// One turn of game time
	virtual void ProcessTurn() = 0;

	/// Jumps to an hour of script time. When the presentation shows the sky, it builds the dome again for it at once.
	virtual void SetTime(float time) = 0;
	/// The sky of the visual time: 0 at night, 1 at dusk and 2 by day, between them as it turns
	[[nodiscard]] virtual float GetCurrentSkyType() const = 0;
	/// The visual time, between 0 and 24 in hours
	[[nodiscard]] virtual float GetTime() const = 0;
	/// The hours of the visual time the sky turns at
	[[nodiscard]] virtual DayNightTimes GetDayNightTimes() const = 0;
	/// The clock of day and night
	[[nodiscard]] virtual DayNightClock& GetClock() = 0;
	[[nodiscard]] virtual const DayNightClock& GetClock() const = 0;

	/// The sky's entities: the dome, which holds the clock, the sun and the moon
	[[nodiscard]] virtual const archetypes::SkyArchetype::Entities& GetEntities() const = 0;
};

} // namespace openblack::ecs::systems
