/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandDemo.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <array>
#include <exception>
#include <span>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include "3D/ScreenFade.h"
#include "ECS/CreatureLoop.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/InputStateInterface.h"
#include "ECS/Systems/ScreenFadeSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Game.h"
#include "GameClock.h"
#include "Help/HelpSystem.h"
#include "Input/InterfaceActive.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "Worship/PlayerSpellIcons.h"

namespace openblack::hand_demo
{
namespace
{

/// One 124-byte record of a .hnd file (no header)
struct Record
{
	uint32_t message; ///< interface message: 0 MOUSE_MOVE, 1 GRAB_DOWN, 2 GRAB_UP, 3 ACTION_DOWN, 4 ACTION_UP
	/// The hand's throw block when recorded: velocity, angular velocity,
	/// the hand position, angles
	std::array<float, 12> throwBlock {};
	glm::vec2 mouse;  ///< the mouse, normalised 0..1
	glm::vec3 eye;    ///< the camera position
	glm::vec3 focus;  ///< the camera focus
	uint32_t trigger; ///< the trigger key state when it changed since the last record, else 0 ((inferred) the space key)
	uint32_t timeMs;  ///< the visual clock when recorded
};
constexpr size_t k_RecordSize = 0x7C;

struct HandDemoState
{
	std::vector<Record> records;
	size_t read {0}; ///< the records read
	bool playing {false};
	uint32_t task {0};        ///< the task that started the demo
	uint32_t firstTimeMs {0}; ///< the time of the first record
	uint32_t startMs {0};     ///< the visual clock at the start (re-based while waiting for the trigger)
	bool waitTrigger {false}; ///< the script's wait-for-trigger flag
	bool trigger {false};     ///< the script's pending trigger
	Frame frame;
};

/// This module's state (Locator::inputState)
HandDemoState& Get()
{
	if (!Locator::inputState::has_value())
	{
		std::fputs("hand_demo: no input state in the locator (Locator::inputState)\n", stderr);
		std::abort();
	}
	return Locator::inputState::value().Get<HandDemoState>();
}

/// OPENBLACK_HAND_DEMO_TRACE's frame count, in the debug hooks' store (Locator::debugHooks)
struct HandDemoDebugHooksState
{
	uint32_t frames {0};
};

HandDemoDebugHooksState& TraceState()
{
	if (!Locator::debugHooks::has_value())
	{
		std::fputs("hand_demo: no debug hooks in the locator (Locator::debugHooks)\n", stderr);
		std::abort();
	}
	return Locator::debugHooks::value().Get<HandDemoDebugHooksState>();
}

float ReadFloat(std::span<const uint8_t> data, size_t offset)
{
	float value = 0.0f;
	std::memcpy(&value, data.data() + offset, sizeof(value));
	return value;
}

uint32_t ReadU32(std::span<const uint8_t> data, size_t offset)
{
	uint32_t value = 0;
	std::memcpy(&value, data.data() + offset, sizeof(value));
	return value;
}

std::vector<Record> Load(std::string_view name)
{
	// ".\Data\HandDemo\<name>.hnd", opened for reading
	std::vector<Record> records;
	try
	{
		auto& fileSystem = Locator::filesystem::value();
		const auto& bytes = resources::LoadBlob(
		    Locator::resources::value().GetBlobs(),
		    fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Data>() / "HandDemo" / (std::string(name) + ".hnd")));
		// Only whole records are read: a short read ends the playback
		records.reserve(bytes.size() / k_RecordSize);
		for (size_t offset = 0; offset + k_RecordSize <= bytes.size(); offset += k_RecordSize)
		{
			const std::span<const uint8_t> data {bytes.data() + offset, k_RecordSize};
			std::array<float, 12> block {};
			for (size_t i = 0; i < block.size(); ++i)
			{
				block[i] = ReadFloat(data, 0x04 + i * 4);
			}
			records.push_back({ReadU32(data, 0x00),
			                   block,
			                   {ReadFloat(data, 0x34), ReadFloat(data, 0x38)},
			                   {ReadFloat(data, 0x3C), ReadFloat(data, 0x40), ReadFloat(data, 0x44)},
			                   {ReadFloat(data, 0x48), ReadFloat(data, 0x4C), ReadFloat(data, 0x50)},
			                   ReadU32(data, 0x5C),
			                   ReadU32(data, 0x60)});
		}
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Hand demo {}: {}", name, e.what());
		records.clear();
	}
	return records;
}

/// The records due, in order. With `first` the first record is taken whatever its time and
/// its time is returned.
uint32_t ProcessRecords(bool first, uint32_t nowMs)
{
	auto& state = Get();
	while (true)
	{
		// One record per read; a short read ends the playback. Every read sets the game speed to 1.
		if (state.read >= state.records.size())
		{
			End();
			return 0;
		}
		const auto& record = state.records[state.read];
		++state.read;
		game_clock::SetSpeed(1.0f);
		// Not due while (time - first time) > (now - start), unsigned: seek back one record
		if (!first && record.timeMs - state.firstTimeMs > nowMs - state.startMs)
		{
			--state.read;
			return 0;
		}
		// Waiting for the trigger with one pending, the start is re-based so that time stands still
		// (start = first time - time + now) and the record waits
		if (state.waitTrigger && state.trigger)
		{
			state.startMs = state.firstTimeMs - record.timeMs + nowMs;
			--state.read;
			return 0;
		}
		// A MOUSE_MOVE moves the mouse; any other message copies the recorded throw block into the render hand before
		// it is dispatched: a release throws with the recorded v and h
		if (record.message == 0)
		{
			state.frame.mouse = record.mouse;
		}
		else if (Locator::handSystem::has_value())
		{
			Locator::handSystem::value().SetThrowBlock(record.throwBlock);
		}
		// (not ported) the camera tricon flags (off for the first and the last 20 records): openblack has no camera
		// tricons
		// The record sets the camera's position and focus
		state.frame.eye = record.eye;
		state.frame.focus = record.focus;
		// A trigger in the record becomes the script's pending trigger
		if (record.trigger != 0)
		{
			state.trigger = true;
		}
		// The message goes through the interface's dispatcher (the button bits of the message). (approximate)
		// openblack's hand reads the buttons' state once a frame, so a press and a release due in the same frame are
		// not both seen. (pending) the object of ACTION messages found again within 3 m by its script type and subtype
		switch (record.message)
		{
		case 1:
			state.frame.grip = true;
			break;
		case 2:
			state.frame.grip = false;
			break;
		case 3:
			state.frame.action = true;
			break;
		case 4:
			state.frame.action = false;
			break;
		default:
			break;
		}
		if (first)
		{
			return record.timeMs;
		}
	}
}

} // namespace

bool Play(std::string_view name, uint32_t task, bool waitTrigger, bool keepHand)
{
	auto& state = Get();
	// Without keepHand: an object in the hand is dropped (ForceDropHeld), the player's spells stop charging
	// (CancelAllSpellsCharging) and a leash held in the hand comes off
	if (!keepHand)
	{
		if (Locator::handSystem::has_value() && Locator::handSystem::value().GetHeldObject().has_value())
		{
			Locator::handSystem::value().ForceDropHeld();
		}
		worship::player::CancelAllSpellsCharging(PlayerNames::PLAYER_ONE);
		ecs::creature_loop::ReleaseLeashHeldInHand(PlayerNames::PLAYER_ONE);
	}
	if (state.playing)
	{
		End();
	}
	game_clock::SetSpeed(1.0f);
	state.task = task;
	// The wide screen bars are turned on when they are off, then snapped in at once every time
	if (auto* helpSystem = help::Get(); helpSystem != nullptr && helpSystem->GetWideScreen() == 0)
	{
		helpSystem->SetWideScreen(1, 0);
	}
	if (Locator::screenFade::has_value())
	{
		Locator::screenFade::value().Fade().SnapWideScreen();
	}
	// The interface is made active, so that the recorded input moves the hand
	interface_active::SetActive(true);
	state.records = Load(name);
	state.read = 0;
	state.frame = Frame {};
	state.playing = false;
	if (!state.records.empty())
	{
		// The first record at once, and its time; the start is now; the real mouse is turned off
		state.playing = true;
		const uint32_t now = game_clock::VisualMs();
		state.firstTimeMs = ProcessRecords(true, now);
		state.startMs = now;
	}
	// The script's flags are set whatever the playback start did
	state.waitTrigger = waitTrigger;
	state.trigger = false;
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand demo {}: {} records, task {}, wait trigger {}, keep hand {}", name,
	                   state.records.size(), task, waitTrigger, keepHand);
	return state.playing;
}

bool IsPlaying(uint32_t task)
{
	const auto& state = Get();
	if (task != 0 && task != state.task)
	{
		return false;
	}
	return state.playing;
}

bool ConsumeTrigger()
{
	auto& state = Get();
	const bool trigger = state.trigger;
	state.trigger = false;
	return trigger;
}

void End()
{
	auto& state = Get();
	// Only while playing: the file is closed, the playing flag and task are cleared, the real mouse is back, the
	// script's wait-for-trigger flag is cleared, and the interface is made inactive when the wide screen has an owner.
	// The bars stay and the pending trigger stays.
	if (!state.playing)
	{
		return;
	}
	if (const auto* helpSystem = help::Get(); helpSystem != nullptr && helpSystem->GetWideScreenOwner() != 0)
	{
		interface_active::SetActive(false);
	}
	state.playing = false;
	state.task = 0;
	state.records.clear();
	state.read = 0;
	state.waitTrigger = false;
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand demo ended");
}

void EndIfTask(uint32_t task)
{
	if (Get().playing && Get().task == task)
	{
		End();
	}
}

std::optional<Frame> Update(uint32_t nowMs)
{
	auto& state = Get();
	if (!state.playing)
	{
		return std::nullopt;
	}
	ProcessRecords(false, nowMs);
	// OPENBLACK_HAND_DEMO_TRACE=1: every 60 frames the record reached and the clocks
	if (static const bool trace = std::getenv("OPENBLACK_HAND_DEMO_TRACE") != nullptr; trace)
	{
		if (++TraceState().frames % 60 == 0)
		{
			SPDLOG_LOGGER_INFO(
			    spdlog::get("game"),
			    "Hand demo trace: record {}/{}, now {} start {} first {}, mouse ({:.3f}, {:.3f}) grip {} action {}", state.read,
			    state.records.size(), nowMs, state.startMs, state.firstTimeMs, state.frame.mouse.x, state.frame.mouse.y,
			    state.frame.grip, state.frame.action);
		}
	}
	// the frame of the last record due; on the frame the file ends the last record still counts
	return state.frame;
}

} // namespace openblack::hand_demo
