/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ConditionsBoxRules.h"
#include "Dialog.h"
#include "GodWindow.h"

namespace openblack::gui
{

class GameFont;
class TextDatabase;

namespace statistics_box
{
class LineGraph;
} // namespace statistics_box

/// The box of the winning conditions of a network game: a see-through box of 780 by 500 with five tabs along its top.
/// The tabs are Main Menu (or Game Over, when the end box opened it), Multiplayer and three empty tabs. Multiplayer is
/// open. The first tab and Back go back to the box under it.
///
/// Under the heading "Multiplayer Winning Condition Status" is a line graph of the score history of each god, from 0 to
/// 1, then a check box for each of the first four players, then the list of the winning conditions of the player of
/// this computer (see conditions_box). The check boxes show only for the gods that play. A click on a check box shows
/// or hides the god in the graph and in the list.
///
/// A network game of more than 4 gods shows every god: the window of gods (GodWindow) on the line of the check boxes
/// picks the 4 gods of the check boxes (deviation 195).
///
/// The figures are taken as the box opens and stay as they were while it is open.
class ConditionsBox
{
public:
	ConditionsBox(const TextDatabase& texts, const GameFont& font, std::function<void()> back);
	~ConditionsBox();
	ConditionsBox(const ConditionsBox&) = delete;
	ConditionsBox& operator=(const ConditionsBox&) = delete;
	ConditionsBox(ConditionsBox&&) = delete;
	ConditionsBox& operator=(ConditionsBox&&) = delete;

	/// Opens on the Multiplayer tab with the figures as they are now, its first tab named for the box it was opened from
	void Open(conditions_box::Figures figures, bool fromGameOver);

	[[nodiscard]] Dialog& GetDialog() noexcept { return *_dialog; }
	[[nodiscard]] const Dialog& GetDialog() const noexcept { return *_dialog; }
	[[nodiscard]] const conditions_box::Figures& GetFigures() const noexcept { return _figures; }
	/// Whether each god is ticked, by its place in the figures
	[[nodiscard]] const std::vector<bool>& GetTicked() const noexcept { return _ticked; }
	[[nodiscard]] GodWindow& GetGodWindow() noexcept { return *_window; }

	/// The box, 780 by 500 in the middle of the dialog space
	static constexpr DialogRect k_Box = TabbedBox({780, 500});

private:
	/// The check boxes show the gods of the window, ticked as the player left them
	void ShowCheckBoxes();
	/// The graph shows the score history of each god whose check box is ticked
	void FillGraph();
	/// Draws the values of the gods under the name of a condition in the list
	void DrawValues(const DialogPainter& painter, const List::ItemPlace& place) const;

	std::u16string _mainMenuLabel;
	std::u16string _gameOverLabel;
	std::u16string _doneText;
	/// The names of the winning conditions, by their numbers
	std::array<std::u16string, ecs::components::GameStanding::k_ConditionCount> _names;
	std::unique_ptr<Dialog> _dialog;
	statistics_box::LineGraph* _graph {nullptr};
	List* _list {nullptr};
	std::array<CheckBox*, conditions_box::k_Gods> _checkBoxes {};
	std::unique_ptr<GodWindow> _window;
	conditions_box::CheckBoxGods _checkBoxGods {};
	std::vector<bool> _ticked;
	conditions_box::Figures _figures;
};

} // namespace openblack::gui
