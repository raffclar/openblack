/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <glm/vec4.hpp>

#include "Common/Zoomer.h"
#include "ConditionsBoxRules.h"
#include "Dialog.h"
#include "EndGameBoxRules.h"
#include "StatisticsBox.h"

namespace openblack::gui
{

class ConditionsBox;
class GameFont;
class TextDatabase;

/// The end box of a skirmish or a network game: a see-through box of 780 by 500 with seven tabs along its top. The tabs
/// are Game Over, Multiplayer, Statistics and four empty tabs. Game Over is open.
///
/// The title stands in the middle of the box, in a band of the colour of the player's god at half opacity. Leave Game is
/// at the bottom right. Watch Game is at the bottom left when the player can watch. Back to Game is there for the
/// question to leave. Watch Game and Back to Game close the box. Escape does nothing.
///
/// The Statistics tab opens the Game Statistics box, and the Multiplayer tab the box of the winning conditions, each with
/// Game Over as its first tab. Their first tab and Back come back to the end box, which shows again as it opened. The
/// box fades in over half a second and out over a fifth.
class EndGameBox
{
public:
	enum class Action : uint8_t
	{
		None,
		/// The player leaves the game
		Leave,
	};

	/// What the box shows as it opens
	struct Contents
	{
		end_game_box::Box box;
		/// The title, from end_game_box::Title
		std::u16string title;
		/// The colour of the band under the title: the colour of the player's god
		glm::vec4 colour {1.0f};
	};

	/// What the Game Statistics box shows as it opens: the figures now and the player at this machine
	using StatisticsSource = std::function<std::pair<statistics::BoxFigures, PlayerNames>()>;

	EndGameBox(const TextDatabase& texts, const GameFont& font);
	~EndGameBox();
	EndGameBox(const EndGameBox&) = delete;
	EndGameBox& operator=(const EndGameBox&) = delete;
	EndGameBox(EndGameBox&&) = delete;
	EndGameBox& operator=(EndGameBox&&) = delete;

	/// What the box of the winning conditions shows as it opens
	using ConditionsSource = std::function<conditions_box::Figures()>;
	/// Called each time the box shows its contents: as it opens, and as it comes back from the box of a tab
	using ShownHandler = std::function<void(const Contents&)>;

	void SetStatisticsSource(StatisticsSource source) { _statisticsSource = std::move(source); }
	void SetConditionsSource(ConditionsSource source) { _conditionsSource = std::move(source); }
	void SetShownHandler(ShownHandler handler) { _onShown = std::move(handler); }
	/// Opens the box, or shows other contents in the open box, on its Game Over tab
	void Open(Contents contents);
	/// Starts to fade out
	void Close();
	/// Opens the box of a tab of the open box, as a click on the tab does: 1 for Multiplayer, 2 for Statistics. False
	/// when the box is not open or the tab has no box.
	bool OpenTab(size_t tab);
	/// Scrolls the window of gods of the box of a tab that has more than 4 gods, as notches of the mouse wheel over its
	/// god columns do: on with a positive count. False when no box of a tab shows or its view does not scroll.
	bool ScrollGodWindow(int notches);
	/// Open and taking the player's input, not fading out
	[[nodiscard]] bool IsOpen() const noexcept { return _open; }
	/// On the screen, fading out included
	[[nodiscard]] bool IsVisible() const noexcept { return _open || _fade.GetValue() > 0.0f; }
	/// The Game Statistics box shows in place of the end box
	[[nodiscard]] bool ShowsStatistics() const noexcept { return _showsStatistics; }
	/// The box of the winning conditions shows in place of the end box
	[[nodiscard]] bool ShowsConditions() const noexcept { return _showsConditions; }
	/// The end box is open and shows no box of a tab over it
	[[nodiscard]] bool ShowsItself() const noexcept { return _open && !_showsStatistics && !_showsConditions; }
	[[nodiscard]] const ConditionsBox& GetConditions() const noexcept { return *_conditions; }
	[[nodiscard]] const StatisticsBox& GetStatistics() const noexcept { return *_statistics; }
	[[nodiscard]] const Contents& GetContents() const noexcept { return _contents; }
	[[nodiscard]] const StaticText& GetTitle() const noexcept { return *_title; }

	void Update(float deltaSeconds);
	void MouseMove(glm::ivec2 point);
	void MouseDown(glm::ivec2 point);
	/// True when a control acted: the game clicks
	bool MouseUp(glm::ivec2 point);
	/// The mouse wheel turned over a point: steps away from the player, and across to the right on a horizontal wheel
	void Wheel(glm::ivec2 point, int steps, int across);
	/// What the player chose, once
	[[nodiscard]] Action TakeAction() noexcept { return std::exchange(_action, Action::None); }
	/// The controls that the player can press by name now
	[[nodiscard]] std::vector<Dialog::NamedControl> GetNamedControls() const;

	void Draw(const DialogPainter& painter) const;

	/// The box, 780 by 500 in the middle of the dialog space
	static constexpr DialogRect k_Box = TabbedBox({780, 500});
	/// The title and its band
	static constexpr DialogRect k_TitleRect {.min = {40, 250}, .max = {760, 350}};

private:
	void OpenStatistics();
	void OpenConditions();
	/// The box of a tab goes back to the end box
	void BackFromTab();
	[[nodiscard]] Dialog& CurrentDialog() const;

	StatisticsSource _statisticsSource;
	ConditionsSource _conditionsSource;
	ShownHandler _onShown;
	std::unique_ptr<Dialog> _dialog;
	std::unique_ptr<StatisticsBox> _statistics;
	std::unique_ptr<ConditionsBox> _conditions;
	StaticText* _title {nullptr};
	BigButton* _watchButton {nullptr};
	BigButton* _backButton {nullptr};
	BigButton* _leaveButton {nullptr};
	Contents _contents {.box = {.kind = end_game_box::Kind::Won, .watch = false}, .title = {}, .colour = glm::vec4(1.0f)};
	bool _open {false};
	bool _showsStatistics {false};
	bool _showsConditions {false};
	Zoomer _fade;
	Action _action {Action::None};
};

} // namespace openblack::gui
