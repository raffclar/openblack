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

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include <glm/vec2.hpp>

#include "Common/LoadTimer.h"
#include "Gui/LoadingScreenRules.h"

namespace openblack::ecs::systems
{

/// What the screens of the start and of a load need from the game that shows them
struct LoadingScreenHooks
{
	/// Gives true when the game must stop, for example after the player closed the window
	std::function<bool()> quitRequested;
	/// Tells the game to stop. A screen calls it when the player closes the window.
	std::function<void()> requestQuit;
	/// Draws at the size of the window from this frame on, if the size changed
	std::function<void()> followWindowSize;
};

/// The choices of the command line for the screens that the game shows as it starts
struct StartupScreenOptions
{
	/// The short version of this build, which the tips screen shows
	std::string build;
	/// The command line removed the logo pictures
	bool skipLogos {false};
	/// The command line asked for the pre-intro film
	bool preIntro {false};
	/// The command line started a testbed scenario
	bool scenario {false};
};

/// A save box that the game shows as a frame of its own
struct SavingBoxFrame
{
	/// The box of a load, not of a save
	bool loading {false};
	/// The box darkens the last picture under it
	bool darken {false};
	/// The number of the frame of the game
	uint32_t frame {0};
	/// The file of a screenshot that the game wants of this frame, if it wants one
	std::optional<std::filesystem::path> screenshot;
};

/// The screens that the game shows as it starts and while a land loads: the logos, the pre-intro film, the tips screen
/// and the banner of a load. It also keeps the timers of the start of the game.
class LoadingScreenSystemInterface
{
public:
	/// Draws the contents of a frame at the given resolution
	using Draw = std::function<void(glm::u16vec2 resolution)>;

	virtual ~LoadingScreenSystemInterface() = default;

	/// Starts the timer of the start of the game, from the launch to the first frame on the first land
	virtual void StartStartupTimer(LoadTimer::Clock::time_point launch) = 0;
	/// Logs the time of a step of the start, while the timer of the start runs
	virtual void StartupStep(std::string_view step) = 0;
	/// Logs the last step of the start and stops the timer of the start, if it runs
	virtual void FinishStartupTimer(std::string_view lastStep) = 0;
	/// Starts the timer of the load of every resource that the start registered
	virtual void StartPrefetchTimer() = 0;
	/// Gives true while the timer of the resources runs
	[[nodiscard]] virtual bool TimesPrefetch() const = 0;
	/// Stops the timer of the resources
	virtual void FinishPrefetchTimer() = 0;
	/// Gives true while the timer of the start runs
	[[nodiscard]] virtual bool TimesStartup() const = 0;

	/// Shows the screens before the game loads the rest: the logo pictures, the pre-intro film on a first run and the
	/// tips screen. The tips screen fades in and stays up while the game loads.
	virtual void PlayStartupScreens(const StartupScreenOptions& options) = 0;
	/// Plays the logo effect and its sound, before the menu shows
	virtual void PlayLogoEffect() = 0;
	/// Draws one frame outside the loop of the game over a black screen and shows it
	virtual void PresentStartupFrame(const Draw& draw) = 0;
	/// Shows the tips screen or the banner while a land loads. The loading draws it again at its redraw points.
	virtual void BeginLoading(loading::LoadingClock::Mode look) = 0;
	/// A redraw point inside the loading. It draws and shows the tips screen if it is due, or at once when drawNow is
	/// true. drawUnder, if it is given, draws first in a frame that shows. Gives true when a frame showed.
	virtual bool RenderLoadingFrame(bool drawNow, const std::function<void()>& drawUnder) = 0;
	/// Gives true while a load shows its screen
	[[nodiscard]] virtual bool IsLoading() const = 0;
	/// Ends the screen of the load. After the tips screen, its tip goes before the first frame of the game.
	virtual void EndLoading() = 0;
	/// Draws the box of a save or a load and shows it at once as a frame of its own over the last picture
	virtual void ShowSavingBox(const SavingBoxFrame& box) = 0;
	/// Removes the textures of the screens, before the renderer stops
	virtual void ReleaseScreen() = 0;

	/// Gives true when the screens have their textures. Without a window there are none.
	[[nodiscard]] virtual bool HasScreen() const = 0;
	/// What the loading draws at its redraw points, nothing when no load occurs
	[[nodiscard]] virtual std::optional<loading::LoadingClock::Mode> GetLoadingLook() const = 0;
	/// Gives true after the tips screen faded in once
	[[nodiscard]] virtual bool TipFadedIn() const = 0;
	/// Gives true while the tips screen has a tip
	[[nodiscard]] virtual bool TipShowing() const = 0;
};

} // namespace openblack::ecs::systems
