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

#include <algorithm>
#include <array>
#include <optional>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include "Commands/PlayerCommands.h"
#include "Common/ModelInverseSquareRoot.h"
#include "Physics/Body.h"

/// The lead of a throw. The computer of the player that throws knows how late the turns do the reports of its hand.
/// It moves the start of the throw along the flight by as many turns, and the command sends the moved start to all the
/// computers. Thus the thing flies from the place where the player sees it, and every computer starts the same flight.
namespace openblack::hand_grab::throw_lead
{

/// One turn of lead for each full count of these milliseconds of report latency
inline constexpr uint32_t k_LeadTurnMs = 100;
/// The most turns of lead
inline constexpr int k_MostLeadTurns = 5;
/// The most reports that wait for their turns with their send times
inline constexpr size_t k_ReportStamps = 32;

/// The turns of lead for a report latency in milliseconds
[[nodiscard]] constexpr int LeadTurns(uint32_t latencyMs)
{
	return static_cast<int>(std::min(latencyMs / k_LeadTurnMs, static_cast<uint32_t>(k_MostLeadTurns)));
}

/// The latency of the reports of this computer's hand: the milliseconds from the send of a report to the turn that does
/// it. The last report that a turn did gives the value. The value stays until a turn does the next report.
class ReportLatency
{
public:
	/// This computer sends a report at this tick count
	void Sent(const player_commands::HandReport& report, uint32_t ticks)
	{
		// A new send takes the place of the oldest when all the places are full
		if (_waiting == k_ReportStamps)
		{
			_oldest = (_oldest + 1) % k_ReportStamps;
			--_waiting;
		}
		_stamps.at((_oldest + _waiting) % k_ReportStamps) = {.report = report, .ticks = ticks};
		++_waiting;
	}
	/// A turn does a report of this computer at this tick count. The tick count wraps as the clock does. The reports
	/// that waited before it do not come to a turn.
	void Done(const player_commands::HandReport& report, uint32_t ticks)
	{
		for (size_t i = 0; i < _waiting; ++i)
		{
			const auto& stamp = _stamps.at((_oldest + i) % k_ReportStamps);
			if (stamp.report == report)
			{
				_latencyMs = ticks - stamp.ticks;
				_oldest = (_oldest + i + 1) % k_ReportStamps;
				_waiting -= i + 1;
				return;
			}
		}
		// A report that this computer did not send keeps the last latency
	}
	/// A tool run gives a fixed latency, so that its throws are the same each run. None measures the latency again.
	void Fix(std::optional<uint32_t> latencyMs) { _fixedMs = latencyMs; }

	[[nodiscard]] uint32_t Milliseconds() const { return _fixedMs.value_or(_latencyMs); }

private:
	struct Stamp
	{
		player_commands::HandReport report {};
		uint32_t ticks {0};
	};
	std::array<Stamp, k_ReportStamps> _stamps {};
	size_t _oldest {0};
	size_t _waiting {0};
	uint32_t _latencyMs {0};
	std::optional<uint32_t> _fixedMs;
};

/// The flight that the computer of the player that throws predicts for the turns of lead. The frames of this computer
/// draw the thing along it, from the send of the throw to the turn that does the command.
struct Flight
{
	/// The places of the body at the start of each turn of lead, then the start that the command sends
	std::vector<physics::FlightPlace> places;
	/// The size of the body, and its centre of mass in its own axes
	float scale {1.0f};
	glm::vec3 centreOfMass {0.0f};
	/// The turn that the frames were in at the send
	uint32_t sentTurn {0};
};

/// A throw after its lead: the start and the velocity that the command sends, and the flight that this computer draws
/// until the turn of the command. A thing with no body has no flight.
struct LedThrow
{
	player_commands::ThrowStart start {};
	glm::vec3 velocity {0.0f};
	std::optional<Flight> flight;
};

/// Where the frames draw the thing of a predicted flight: a turn after the send, a part of the way through it. The axes
/// have no size.
[[nodiscard]] inline physics::Pose DrawnPose(const Flight& flight, uint32_t turn, float turnFraction)
{
	if (flight.places.empty())
	{
		return {};
	}
	const size_t last = flight.places.size() - 1;
	const size_t index = turn - flight.sentTurn;
	physics::FlightPlace place = flight.places.back();
	// Until the last place, the thing moves from the place of a turn to the place of the next turn
	if (turn >= flight.sentTurn && index < last)
	{
		const auto& from = flight.places[index];
		const auto& to = flight.places[index + 1];
		for (int row = 0; row < 3; ++row)
		{
			place.axes[row] = (to.axes[row] - from.axes[row]) * turnFraction + from.axes[row];
		}
		place.centre = (to.centre - from.centre) * turnFraction + from.centre;
	}
	// Each axis becomes a unit axis again
	for (int row = 0; row < 3; ++row)
	{
		auto& axis = place.axes[row];
		axis *= gutils::ModelInverseSquareRoot((axis.x * axis.x + axis.y * axis.y) + axis.z * axis.z);
	}
	return {.axes = place.axes, .origin = place.centre - (place.axes * flight.scale) * flight.centreOfMass};
}

} // namespace openblack::hand_grab::throw_lead
