/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <string>
#include <string_view>

#include <entt/entity/fwd.hpp>

#include "CreatureMindTrees.h"
#include "Window.h"

namespace openblack::debug::gui
{

/// The developer view of a creature's mind: its active desires and the sources that drive them, the plans of the
/// planner with a short history, and a graph of the desire values over the last turns. A second tab shows the decision
/// trees that the creature learnt, for each desire. The view only reads the mind.
/// While it is open, the mind log records the chosen creature at the end of each turn.
class CreatureMindWindow final: public Window
{
public:
	CreatureMindWindow() noexcept;

	/// Shows the mind of this creature
	void Select(entt::entity creature) noexcept;

protected:
	void Draw() noexcept override;
	void Update() noexcept override;
	void UpdateAlways() noexcept override;
	void ShowTab(std::string_view tab) noexcept override { _showTab = std::string(tab); }
	void ProcessEventOpen(const SDL_Event&) noexcept override {}
	void ProcessEventAlways(const SDL_Event&) noexcept override {}

private:
	/// Starts the record of the chosen creature, and stops the record that this window started before
	void WatchChosen() noexcept;
	void StopWatching() noexcept;
	void DrawPicker() noexcept;
	void DrawDesires(entt::entity creature) noexcept;
	void DrawPlans(entt::entity creature) noexcept;
	void DrawTimeline(entt::entity creature) noexcept;
	void DrawLabel(entt::entity creature) noexcept;

	std::optional<entt::entity> _chosen;
	/// The creature that this window asked the mind log to record. Another tool can record the same creature: the window
	/// then does not stop that record.
	std::optional<entt::entity> _watching;
	bool _followHand {false};
	bool _showLabel {true};
	CreatureMindTrees _trees;
	/// The tab to show at the next draw, by its name
	std::optional<std::string> _showTab;
};

} // namespace openblack::debug::gui
