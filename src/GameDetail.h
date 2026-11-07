/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// The helpers that the game's source files share. Only those files include this header.

#include <cstdint>

#include <chrono>
#include <memory>
#include <optional>

#include "Common/MainThreadQueue.h"
#include "Common/SideThread.h"
#include "Debug/FrameStatsLog.h"
#include "Enums.h"
#include "Game.h"
#include "Graphics/RendererInterface.h"

namespace openblack
{
struct EngineConfig;

/// The state that the frames of the game loop keep from one frame to the next
struct Game::FrameLoop
{
	/// The game's clock when the loop started. The clock follows a fixed frame time in a tool's run, so the draw's time
	/// does not change with the speed of the machine.
	uint32_t startTicks {0};
	std::optional<debug::FrameStatsLog> frameStats;
	std::chrono::steady_clock::time_point frameStart;
	/// The thread of the turns that play beside the draw (deviation row 153), made the first time it is used
	std::unique_ptr<SideThread> turnThread;
	/// The turns that play beside the draw: the turn count before them and their start
	uint32_t turnBefore {0};
	std::chrono::steady_clock::time_point turnsStart;
	std::chrono::steady_clock::time_point turnsEnd;
	/// The clock of the next frame moved on already, and its turns played beside the draw of this frame
	bool turnsPlayedBeside {false};
	/// The clock of the machine for the statistics of this frame, read before the clock of the next frame moved on
	std::optional<uint32_t> statisticsTicks;
	/// Open while the turns play beside the draw: the main thread then does not use the services that the turns hold
	std::optional<MainThreadQueue::TurnElsewhere> turnsElsewhere;
};
} // namespace openblack

namespace openblack::game_detail
{

/// The player that the mouse, the keys and the gestures of this computer act for
PlayerNames LocalInputPlayer();

/// How the game draws its scene into the window, from its camera
graphics::RendererInterface::DrawSceneDesc SceneDrawDesc(const EngineConfig& config, uint32_t time, bool drawHand);

/// A network game with a session: the menu has Exit Online Game and the Multiplayer tab
bool InNetworkGame();

} // namespace openblack::game_detail
