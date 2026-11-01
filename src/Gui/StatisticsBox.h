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
#include <optional>
#include <string>
#include <vector>

#include <glm/vec4.hpp>

#include "Common/Zoomer.h"
#include "Dialog.h"
#include "GodWindow.h"
#include "StatisticsBoxRules.h"

namespace openblack::gui
{

class GameFont;
class TextDatabase;

namespace statistics_box
{
class BarGraph;
class LineGraph;
class CreatureViews;

/// Adds a line graph of the players' histories to a dialog: the box of the Player Stats tab, which other boxes use too.
/// A click on it changes it from shares to values, or back.
LineGraph& AddLineGraph(Dialog& dialog, DialogRect rect, std::u16string caption, bool shares);
/// Gives a line graph its lines, one for each player in its colour. With no scale, the scale goes from the lowest point
/// of the lines to the highest.
void SetLines(LineGraph& graph, std::vector<std::vector<float>> lines, std::vector<glm::vec4> colours,
              std::optional<LineScale> scale);
} // namespace statistics_box

/// The Game Statistics box: six tabs along its top, of which the first returns to the box it was opened from and the
/// others list the players' figures, a heading, three graph boxes, a list of the tab's figures, a Back arrow and the
/// Graphic and Numeric check boxes, which work as a pair.
///
/// The figures are taken as the box opens and stay as they were while it is open; a tab that the player picks lists the same
/// figures again. A figure's row shows each player's value in their colour and the total after them, or, while Graphic
/// is ticked, one bar split between the players. A click on a row shows it in the right hand graph box. The Player Stats
/// tab shows the influence and population over the whole game in two line graphs. A click on a line graph changes it
/// from shares to values, or back. The Creature Stats tab shows each player's creature.
///
/// A network game of more than 4 gods shows every god, 4 at a time: the window of gods (GodWindow) right of the check
/// boxes picks the gods of the rows, the graphs and the creatures (deviation 195).
class StatisticsBox
{
public:
	/// The box, 780 by 500 in the middle of the dialog space
	static constexpr DialogRect k_Box = TabbedBox({780, 500});

	StatisticsBox(const TextDatabase& texts, const GameFont& font, std::function<void()> back);
	~StatisticsBox();
	StatisticsBox(const StatisticsBox&) = delete;
	StatisticsBox& operator=(const StatisticsBox&) = delete;

	/// Opens on Player Stats with the figures as they are now, for the player at this machine, its first tab named for
	/// the box it was opened from
	void Open(statistics::BoxFigures figures, PlayerNames local, bool fromGameOver);
	/// Picks a tab: its rows are listed and its graph boxes filled
	void SetTab(statistics_box::Tab tab);
	/// A click on a row shows it in the right hand graph box, unless it is shown there already
	void PickRow(size_t row);
	void SetGraphic(bool graphic);

	/// Every frame
	void Update(float deltaSeconds);

	[[nodiscard]] Dialog& GetDialog() noexcept { return *_dialog; }
	[[nodiscard]] const Dialog& GetDialog() const noexcept { return *_dialog; }
	[[nodiscard]] statistics_box::Tab GetTab() const noexcept { return _tab; }
	[[nodiscard]] const statistics_box::Page& GetPage() const noexcept { return _page; }
	[[nodiscard]] bool IsGraphic() const noexcept { return _graphic; }
	[[nodiscard]] GodWindow& GetGodWindow() noexcept { return *_window; }

private:
	/// The window of gods moved: the tab shows its gods, and the row the player picked stays in the right hand graph box
	void ShowWindow();
	void Fill(statistics_box::Graph graph, std::optional<size_t> row);
	/// Draws a row's values or bar over its name in the list
	void DrawRow(const DialogPainter& painter, const List::ItemPlace& place);

	const TextDatabase& _texts;
	const GameFont& _font;
	std::u16string _mainMenuLabel;
	std::u16string _gameOverLabel;
	std::u16string _totalLabel;
	std::unique_ptr<Dialog> _dialog;
	List* _list {nullptr};
	CheckBox* _graphicBox {nullptr};
	CheckBox* _numericBox {nullptr};
	std::array<statistics_box::BarGraph*, statistics_box::k_GraphCount> _graphs {};
	statistics_box::LineGraph* _influence {nullptr};
	statistics_box::LineGraph* _population {nullptr};
	statistics_box::CreatureViews* _creatures {nullptr};
	std::unique_ptr<GodWindow> _window;

	statistics::BoxFigures _figures;
	PlayerNames _local {PlayerNames::PLAYER_ONE};
	statistics_box::Tab _tab {statistics_box::Tab::Player};
	statistics_box::Page _page;
	/// The row last shown in a graph box
	std::optional<size_t> _shownRow;
	bool _graphic {true};

	/// The rows' bars grow once, one for each number of players; each row drawn with them makes them grow more
	mutable std::array<Zoomer, statistics::k_BoxPlayers> _rowGrowth;
	float _frameSeconds {0.0f};
	/// Whether the Total heading has been drawn this frame
	mutable bool _totalHeadingDrawn {false};
};

} // namespace openblack::gui
