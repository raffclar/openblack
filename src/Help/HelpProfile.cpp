/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HelpProfile.h"

#include <cmath>

#include <utility>

#include "ECS/Systems/ScriptStateInterface.h"
#include "GameClock.h"
#include "Help/HelpSystem.h"
#include "Locator.h"

namespace openblack::help_profile
{
namespace
{
/// ProcessSpecialTriggers' constants
constexpr float k_TooCloseDistance = 15.0f;
constexpr float k_TooClosePitch = 0.55f;
constexpr float k_SkyPitch = -0.3f;
/// |yaw| is compared with the float 0.01 widened to double
constexpr double k_RotateThreshold = 0.0099999997764825821;

/// The camera help's own tables: the inputs of the 0x3nn reasons, the 0x2nn reasons and the 0x1nn reasons. Only the
/// camera help's debug page and its tooltips read them: kept so that the callback is whole
constexpr size_t k_InputCount = 5;
constexpr size_t k_MoveCount = 4;
constexpr size_t k_ExclusionCount = 3;

struct HelpProfileState
{
	std::array<Accumulator, k_EventCount> profile {};
	std::array<Accumulator, k_InputCount> inputs {};
	std::array<Accumulator, k_MoveCount> moves {};
	std::array<Accumulator, k_ExclusionCount> exclusions {};
	uint32_t accumulatedTime {0};
	Queries queries;
};

/// The HelpProfile state (Locator::scriptState)
HelpProfileState& HelpProfileData()
{
	return openblack::Locator::scriptState::value().Get<HelpProfileState>();
}

bool Paused()
{
	auto& state = HelpProfileData();
	return state.queries.paused ? state.queries.paused() : game_clock::IsPaused();
}

bool ScriptWideScreen()
{
	auto& state = HelpProfileData();
	if (state.queries.scriptWideScreen)
	{
		return state.queries.scriptWideScreen();
	}
	const auto* helpSystem = help::Get();
	return helpSystem != nullptr && helpSystem->IsScriptWideScreen();
}

bool Blocked()
{
	return Paused() || ScriptWideScreen();
}

Accumulator& At(Event type)
{
	return HelpProfileData().profile.at(static_cast<size_t>(type));
}

template <size_t N>
void EndTurn(std::array<Accumulator, N>& table)
{
	for (auto& accumulator : table)
	{
		accumulator.EndTurn();
	}
}

template <size_t N>
void ResetAll(std::array<Accumulator, N>& table)
{
	for (auto& accumulator : table)
	{
		accumulator.Reset();
	}
}

const Accumulator* Valid(int32_t type)
{
	// only 1..48
	if (type <= 0 || type >= k_EventCount)
	{
		return nullptr;
	}
	return &HelpProfileData().profile.at(static_cast<size_t>(type));
}
} // namespace

void Accumulator::Reset()
{
	totalTriggerCount = 0;
	used = 0;
	head = 0;
	rate = 0.0f;
	triggeredThisTurn = false;
}

void Accumulator::Trigger(uint32_t now)
{
	if (triggeredThisTurn)
	{
		return;
	}
	++totalTriggerCount;
	triggeredThisTurn = true;
	times.at(head) = now; // the head is below 64
	head = static_cast<uint8_t>((head + 1) & (k_RingSize - 1));
	if (used < static_cast<int8_t>(k_RingSize))
	{
		++used;
	}
}

void Accumulator::EndTurn()
{
	const auto flag = static_cast<float>(triggeredThisTurn ? 1 : 0);
	triggeredThisTurn = false;
	rate = (flag - rate) * k_RateSmoothing + rate;
}

void Accumulator::ClampTimes(uint32_t now)
{
	for (auto& time : times)
	{
		if (time > now)
		{
			time = now;
		}
	}
}

float Accumulator::TimeSinceLastUsed(uint32_t now)
{
	if (used == 0)
	{
		return 0.0f;
	}
	auto since = static_cast<int32_t>(now - times.at((head - 1u) & (k_RingSize - 1)));
	if (since < 0)
	{
		ClampTimes(now);
		since = 0;
	}
	return static_cast<float>(since) * game_clock::k_SecondsPerMs;
}

float Accumulator::TriggersPerSecond(uint32_t now)
{
	if (used < 2)
	{
		return 0.0f;
	}
	const auto oldest = static_cast<uint32_t>(head - static_cast<int32_t>(used)) & (k_RingSize - 1);
	const auto span = static_cast<int32_t>(now - times.at(oldest));
	if (span < 0)
	{
		ClampTimes(now);
		return 0.0f;
	}
	if (span == 0)
	{
		return 0.0f;
	}
	// (used x 1000 - 1000) / span
	return static_cast<float>(static_cast<int32_t>(used) * 1000 - 1000) / static_cast<float>(span);
}

void SetQueries(Queries queries)
{
	HelpProfileData().queries = std::move(queries);
}

void Trigger(Event type)
{
	if (Blocked())
	{
		return;
	}
	const auto value = static_cast<int32_t>(type);
	const auto now = HelpProfileData().accumulatedTime;
	At(type).Trigger(now);
	if (value >= 0x0E && value <= 0x17)
	{
		At(Event::GestureTotal).Trigger(now);
	}
	if (value >= 0x01 && value <= 0x2A)
	{
		At(Event::AllInterface).Trigger(now);
	}
	if (value >= 0x09 && value <= 0x0B)
	{
		At(Event::CastAll).Trigger(now);
	}
	At(Event::AllEvents).Trigger(now);
}

void ProcessSpecialTriggers()
{
	auto& state = HelpProfileData();
	// the camera's current mode, only when it is the player's; none -> nothing
	if (!state.queries.playerCamera)
	{
		return;
	}
	const auto view = state.queries.playerCamera();
	if (!view)
	{
		return;
	}
	if (view->screenCentreOnLand)
	{
		Trigger(Event::LookAtLand);
		// below 15 and above 0.55
		if (view->headingDistance < k_TooCloseDistance && view->pitch > k_TooClosePitch)
		{
			Trigger(Event::LookAtLandTooClose);
		}
		return;
	}
	if (view->pitch < k_SkyPitch)
	{
		Trigger(Event::LookAtSky);
	}
}

void Process()
{
	auto& state = HelpProfileData();
	// paused, a script's wide screen, or blocked
	if (Blocked() || (state.queries.processBlocked && state.queries.processBlocked()))
	{
		return;
	}
	ProcessSpecialTriggers();
	EndTurn(state.profile);
	state.accumulatedTime += k_MsPerProcess;
	// the camera help's three tables, the same step
	EndTurn(state.exclusions);
	EndTurn(state.moves);
	EndTurn(state.inputs);
}

void CameraHelpCallback(CameraReason reason, uint32_t inputs)
{
	auto& state = HelpProfileData();
	const auto value = static_cast<uint32_t>(reason);
	const auto entry = value & 0xFFu;
	const auto now = state.accumulatedTime;
	switch (value & 0xF00u)
	{
	case 0x300:
		Trigger(static_cast<Event>(entry + 0x19));
		for (size_t bit = 0; bit < k_InputCount; ++bit)
		{
			if ((inputs & (1u << bit)) != 0)
			{
				state.inputs.at(bit).Trigger(now); // the camera help's own: no pause test
			}
		}
		break;
	case 0x200:
		if (entry < k_MoveCount)
		{
			state.moves.at(entry).Trigger(now);
		}
		break;
	case 0x100:
		if (entry < k_ExclusionCount)
		{
			state.exclusions.at(entry).Trigger(now);
		}
		break;
	default:
		break;
	}
}

void OnPlayerCameraMove(float yaw, float pitch, float zoom, uint32_t inputs)
{
	if (zoom != 0.0f)
	{
		CameraHelpCallback(CameraReason::Zoom, inputs);
	}
	if (yaw != 0.0f && std::fabs(static_cast<double>(yaw)) > k_RotateThreshold)
	{
		CameraHelpCallback(CameraReason::Rotate, inputs);
		if (yaw > 0.0f)
		{
			CameraHelpCallback(CameraReason::RotateCW, inputs);
		}
		if (yaw < 0.0f)
		{
			CameraHelpCallback(CameraReason::RotateCCW, inputs);
		}
	}
	if (pitch != 0.0f)
	{
		CameraHelpCallback(CameraReason::Pitch, inputs);
	}
}

std::optional<float> TotalEvents(int32_t type)
{
	const auto* accumulator = Valid(type);
	if (accumulator == nullptr)
	{
		return std::nullopt;
	}
	return static_cast<float>(accumulator->totalTriggerCount);
}

std::optional<float> EventsPerSecond(int32_t type)
{
	auto& state = HelpProfileData();
	if (Valid(type) == nullptr)
	{
		return std::nullopt;
	}
	return state.profile.at(static_cast<size_t>(type)).TriggersPerSecond(state.accumulatedTime);
}

std::optional<float> TimeSince(int32_t type)
{
	auto& state = HelpProfileData();
	if (Valid(type) == nullptr)
	{
		return std::nullopt;
	}
	return state.profile.at(static_cast<size_t>(type)).TimeSinceLastUsed(state.accumulatedTime);
}

const Accumulator& Get(Event type)
{
	return At(type);
}

uint32_t AccumulatedTime()
{
	return HelpProfileData().accumulatedTime;
}

void SetToZero()
{
	auto& state = HelpProfileData();
	ResetAll(state.profile);
	// the camera help's tables: 0x1nn, 0x2nn, then the inputs
	ResetAll(state.exclusions);
	ResetAll(state.moves);
	ResetAll(state.inputs);
}

void Reset()
{
	HelpProfileData() = {};
}

} // namespace openblack::help_profile
