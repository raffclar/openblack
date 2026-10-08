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

// The weather's calls from the game loop: ProcessTurnStart from Magic/MagicLoop.cpp, ProcessWeatherThings and
// ProcessClimate from Game.cpp, each in its slot of the game turn.

namespace openblack::weather
{
/// A new land, before its script: no climate, weather thing or storm; the grid and the counters back to the start
void OnLoadMap();
/// Slot 1: the atmosphere's game update (VisualTime, 0.1), the first thing of a game turn
void ProcessTurnStart(uint32_t turn);
/// The weather things' turn, after the land alignment's time update
void ProcessWeatherThings();
/// The climates' turn, after the bookmarks and the script highlights (the turn of the last ProcessTurnStart); then
/// the test hooks
void ProcessClimate();
/// Every frame (magic::Update, game seconds): the atmosphere's 3D update, the rain streaks (Rain.cpp)
void UpdateFrame(float seconds);
/// OPENBLACK_TEST_WEATHER / OPENBLACK_WEATHER_TRACE (WeatherDebugHooks.cpp), every turn
void RunDebugHooks(uint32_t turn);
void ResetDebugHooks();
} // namespace openblack::weather
