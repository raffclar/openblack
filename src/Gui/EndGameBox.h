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
#include "Dialog.h"
#include "EndGameBoxRules.h"
#include "StatisticsBox.h"

namespace openblack::gui
{

class GameFont;
class TextDatabase;

/// The end box of a skirmish or a network game: a see-through box of 780 by 500 with seven tabs along its top. The tabs
/// are Game Over, Multiplayer, Statistics and four empty tabs. Game Over is open.
///
/// The title stands in the middle of the box, in a band of the colour of the player's god at half opacity. Leave Game is
/// at the bottom right. Watch Game is at the bottom left when the player can watch. Back to Game is there for the
/// question to leave. Watch Game and Back to Game close the box. Escape does nothing.
///
/// The Statistics tab opens the Game Statistics box, with Game Over as its first tab. Its first tab and Back come back
/// to the end box. The box fades in over half a second and out over a fifth.
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

	void SetStatisticsSource(StatisticsSource source) { _statisticsSource = std::move(source); }
	/// Opens the box, or shows other contents in the open box, on its Game Over tab
	void Open(Contents contents);
	/// Starts to fade out
	void Close();
	/// Open and taking the player's input, not fading out
	[[nodiscard]] bool IsOpen() const noexcept { return _open; }
	/// On the screen, fading out included
	[[nodiscard]] bool IsVisible() const noexcept { return _open || _fade.GetValue() > 0.0f; }
	/// The Game Statistics box shows in place of the end box
	[[nodiscard]] bool ShowsStatistics() const noexcept { return _showsStatistics; }
	[[nodiscard]] const Contents& GetContents() const noexcept { return _contents; }
	[[nodiscard]] const StaticText& GetTitle() const noexcept { return *_title; }

	void Update(float deltaSeconds);
	void MouseMove(glm::ivec2 point);
	void MouseDown(glm::ivec2 point);
	/// True when a control acted: the game clicks
	bool MouseUp(glm::ivec2 point);
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
	void BackFromStatistics();
	[[nodiscard]] Dialog& CurrentDialog() const;

	StatisticsSource _statisticsSource;
	std::unique_ptr<Dialog> _dialog;
	std::unique_ptr<StatisticsBox> _statistics;
	StaticText* _title {nullptr};
	BigButton* _watchButton {nullptr};
	BigButton* _backButton {nullptr};
	BigButton* _leaveButton {nullptr};
	Contents _contents {.box = {.kind = end_game_box::Kind::Won, .watch = false}, .title = {}, .colour = glm::vec4(1.0f)};
	bool _open {false};
	bool _showsStatistics {false};
	Zoomer _fade;
	Action _action {Action::None};
};

} // namespace openblack::gui
