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

namespace openblack::ecs::systems
{

/// The sky that this computer shows: the dome (components::SkyDome), the sun (components::Sun) and the moon
/// (components::Moon). Once a frame, the system finds the sun, the moon and the blend of the dome from the clock of
/// SkyClockSystemInterface. The turns and the scripts do not read it, and the dedicated server does not make it.
class SkySystemInterface
{
public:
	virtual ~SkySystemInterface() = default;

	/// Once a frame, just before it is drawn: the sun and moon for the hour and the date, and the rows of the dome to
	/// blend again while the sky is drawn
	virtual void UpdateFrame(bool skyDrawn) = 0;
	/// Builds the whole dome again at once for the sky of the clock, after the clock jumped to a new hour
	virtual void JumpToClock() = 0;

	/// The sky's entities that this system shows: the dome, the sun and the moon
	[[nodiscard]] virtual entt::entity GetDome() const = 0;
	[[nodiscard]] virtual entt::entity GetSun() const = 0;
	[[nodiscard]] virtual entt::entity GetMoon() const = 0;
};

} // namespace openblack::ecs::systems
