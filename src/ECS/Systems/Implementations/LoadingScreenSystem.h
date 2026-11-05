/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>
#include <optional>

#include "ECS/Systems/LoadingScreenSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::gui
{
class LoadingScreen;
}

namespace openblack::ecs::systems
{

class LoadingScreenSystem final: public LoadingScreenSystemInterface
{
public:
	explicit LoadingScreenSystem(LoadingScreenHooks hooks);
	~LoadingScreenSystem() override;
	LoadingScreenSystem(const LoadingScreenSystem&) = delete;
	LoadingScreenSystem& operator=(const LoadingScreenSystem&) = delete;
	LoadingScreenSystem(LoadingScreenSystem&&) = delete;
	LoadingScreenSystem& operator=(LoadingScreenSystem&&) = delete;

	void StartStartupTimer(LoadTimer::Clock::time_point launch) override;
	void StartupStep(std::string_view step) override;
	void FinishStartupTimer(std::string_view lastStep) override;
	void StartPrefetchTimer() override;
	[[nodiscard]] bool TimesPrefetch() const override { return _prefetchTimer.has_value(); }
	void FinishPrefetchTimer() override { _prefetchTimer.reset(); }
	[[nodiscard]] bool TimesStartup() const override { return _startupTimer.has_value(); }

	void PlayStartupScreens(const StartupScreenOptions& options) override;
	void PlayLogoEffect() override;
	void PresentStartupFrame(const Draw& draw) override;
	void BeginLoading(loading::LoadingClock::Mode look) override;
	bool RenderLoadingFrame(bool drawNow, const std::function<void()>& drawUnder) override;
	[[nodiscard]] bool IsLoading() const override;
	void EndLoading() override;
	void ShowSavingBox(const SavingBoxFrame& box) override;
	void ReleaseScreen() override;

	[[nodiscard]] bool HasScreen() const override { return _loadingScreen != nullptr; }
	[[nodiscard]] std::optional<loading::LoadingClock::Mode> GetLoadingLook() const override { return _loadingLook; }
	[[nodiscard]] bool TipFadedIn() const override { return _tipFadedIn; }
	[[nodiscard]] bool TipShowing() const override;

private:
	/// The logo film's two pictures. Each fades in and out, and a key or a mouse button makes it go faster.
	void PlayLogoScreens();
	/// The pre-intro film in its own loop with the trailer music, until its last frame or a key
	void PlayPreIntro();
	/// Picks the tip of the day and makes its picture. The first time, the tips screen fades in over a second.
	void StartTipOfTheDay();
	[[nodiscard]] bool QuitRequested() const { return _hooks.quitRequested(); }

	LoadingScreenHooks _hooks;
	/// Times the start of the game, from its launch to the first frame on the first land
	std::optional<LoadTimer> _startupTimer;
	/// Times the load of every resource that the start registered, on the loading threads or where the game first
	/// wants it
	std::optional<LoadTimer> _prefetchTimer;
	/// The screens that the game draws as it starts and while a land loads. There are none without a window.
	std::unique_ptr<gui::LoadingScreen> _loadingScreen;
	/// What the loading draws at its redraw points, nothing when no load occurs
	std::optional<loading::LoadingClock::Mode> _loadingLook;
	/// The tips screen faded in once. Later loads show it at once.
	bool _tipFadedIn {false};
};

} // namespace openblack::ecs::systems
