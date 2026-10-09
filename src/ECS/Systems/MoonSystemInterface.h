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

#include <chrono>
#include <optional>

#include "Graphics/Moon.h"

namespace openblack::ecs::systems
{

/// What the moon is worked out from each frame
struct MoonFrame
{
	/// The hour of script time, which places the moon
	float scriptHour {12.0f};
	/// The clouds over the camera, 0 to 1, and whether the fog option is on: an overcast dims the moon only with it
	float overcast {0.0f};
	bool fog {true};
	/// The real time since the game started, which paces how often the computer's date is read
	std::chrono::milliseconds realTime {0};
	/// The computer's date and time, in seconds since 1970
	int64_t wallClock {0};
};

/// The moon: where it stands from the camera at the hour, how strongly it shows, and its phase from the computer's date.
/// Scripts are told how full it is as it was when it last showed. The date can be overridden for testing the phases.
class MoonSystemInterface
{
public:
	virtual ~MoonSystemInterface() = default;

	/// Once a frame, before the sky is drawn
	virtual void Update(const MoonFrame& frame) = 0;

	/// Where it stands from the camera and its strength before any overcast; none while it is down
	[[nodiscard]] virtual std::optional<graphics::moon::Placement> GetPlacement() const = 0;
	/// Its strength through the overcast, 0 to 200: it is drawn only while this is above 0
	[[nodiscard]] virtual float GetStrength() const = 0;
	/// The phase of the date in use, 0 to 2 pi
	[[nodiscard]] virtual float GetPhase() const = 0;
	/// The phase as it was the last time the moon showed: 0 until it first shows
	[[nodiscard]] virtual float GetShownPhase() const = 0;
	/// What scripts are told of how full the moon is, from the phase it last showed with
	[[nodiscard]] virtual float GetScriptPercentage() const = 0;

	/// The date the phase is taken from, in seconds since 1970: the computer's, or the override
	[[nodiscard]] virtual int64_t GetDate() const = 0;
	/// Takes the phase from this date instead of the computer's, or from the computer's again with none
	virtual void SetDateOverride(std::optional<int64_t> date) = 0;
	[[nodiscard]] virtual std::optional<int64_t> GetDateOverride() const = 0;
};

} // namespace openblack::ecs::systems
