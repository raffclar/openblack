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
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>

#include "Common/Zoomer.h"
#include "Dialog.h"
#include "MultiplayerFrontEnd.h"

namespace openblack::gui
{

class BigButton;
class Button;
class CheckBox;
class EditBox;
class GameFont;
class List;
class StaticText;
class TextDatabase;

/// The boxes of the multiplayer front end, drawn over the temple's future room, which turns slowly behind them.
///
/// Each box is a see-through framed box of 780 by 500 in the middle of the dialog space, with Back at the bottom left
/// and Next at the bottom right. The frame goes 40 below the box, as the frame of a box with tabs does, so the buttons
/// at the bottom are in the frame. The game room has four tabs along its top. A message stands in a white opaque box of
/// 500 by 200 in front of the box, which dims to a quarter. The boxes and the messages fade in over half a second.
class MultiplayerBoxes
{
public:
	/// The frame of each box but the game room. It has the same size as the frame of the game room, which has tabs.
	static constexpr DialogRect k_Box = TabbedBox({780, 500});
	/// The game room's box, 40 deeper for its tabs
	static constexpr DialogRect k_RoomBox = TabbedBox({780, 500});
	/// The frame that the controls of a box are in
	[[nodiscard]] static constexpr DialogRect FrameOf(multiplayer::Box box) noexcept
	{
		return box == multiplayer::Box::Room || box == multiplayer::Box::Conditions ? k_RoomBox : k_Box;
	}
	static constexpr DialogRect k_MessageBox {.min = {150, 200}, .max = {650, 400}};
	static constexpr DialogRect k_MessageText {.min = {200, 220}, .max = {600, 350}};

	MultiplayerBoxes(const TextDatabase& texts, const GameFont& font);
	MultiplayerBoxes(const MultiplayerBoxes&) = delete;
	MultiplayerBoxes& operator=(const MultiplayerBoxes&) = delete;
	~MultiplayerBoxes();

	/// The front end opens on its first box
	void Open(std::unique_ptr<MultiplayerFrontEnd> frontEnd);
	void Close();
	[[nodiscard]] bool IsActive() const noexcept { return _frontEnd != nullptr; }
	[[nodiscard]] MultiplayerFrontEnd* GetFrontEnd() noexcept { return _frontEnd.get(); }
	[[nodiscard]] const MultiplayerFrontEnd* GetFrontEnd() const noexcept { return _frontEnd.get(); }
	/// The player left the front end from its first box, once
	[[nodiscard]] bool TakeLeft();

	void MouseMove(glm::ivec2 point);
	void MouseDown(glm::ivec2 point);
	/// True when a control was let go over: the game clicks
	bool MouseUp(glm::ivec2 point);
	void Wheel(glm::ivec2 point, int steps);
	void TextInput(std::u16string_view text);
	void KeyDown(int key, bool shift);
	void Update(float deltaSeconds);
	void Draw(const DialogPainter& painter) const;

	/// The controls that act, for tools that press them by name
	[[nodiscard]] std::vector<Dialog::NamedControl> GetNamedControls() const;
	/// The places of all the visible controls of the box, for tools
	[[nodiscard]] std::vector<DialogRect> GetVisibleRects() const;
	/// The box shows the text of a message, for tools; empty when there is none
	[[nodiscard]] std::u16string_view GetMessageText() const;

private:
	struct MessageShown
	{
		multiplayer::Message message {multiplayer::Message::None};
		std::u16string text;
		Dialog answers;
		StaticText* textControl {nullptr};
	};

	[[nodiscard]] std::u16string Text(std::string_view name) const;
	void Build();
	void BuildChooseConnection(Dialog& dialog);
	void BuildFindGames(Dialog& dialog);
	void BuildLogin(Dialog& dialog);
	void BuildGameList(Dialog& dialog);
	void BuildRoom();
	/// The controls follow the front end's state each frame: the lists, the texts, what shows
	void Sync();
	void SyncGameList();
	void SyncRoom();
	void ShowMessage(multiplayer::Message message);
	void SendChat();
	void Join();
	[[nodiscard]] Dialog* Active() const { return _dialog.get(); }

	const TextDatabase& _texts;
	const GameFont& _font;
	std::unique_ptr<MultiplayerFrontEnd> _frontEnd;
	std::unique_ptr<Dialog> _dialog;
	std::optional<multiplayer::Box> _built;
	multiplayer::LobbyKind _builtKind {multiplayer::LobbyKind::Internet};
	Zoomer _fade;
	std::optional<MessageShown> _message;
	Zoomer _messageFade;
	bool _left {false};

	// The controls of the boxes that change with the state, null when the box has none
	List* _list {nullptr};
	List* _players {nullptr};
	CheckBox* _lanBox {nullptr};
	CheckBox* _internetBox {nullptr};
	EditBox* _name {nullptr};
	EditBox* _password {nullptr};
	StaticText* _passwordLabel {nullptr};
	StaticText* _help {nullptr};
	StaticText* _playersLabel {nullptr};
	BigButton* _next {nullptr};
	std::vector<size_t> _shownGames;
	/// The game whose name the name's box shows
	std::optional<size_t> _shownPick;

	// The game room
	StaticText* _caption {nullptr};
	List* _roster {nullptr};
	std::vector<Control*> _chatControls;
	std::vector<Control*> _landControls;
	std::vector<Control*> _clanControls;
	std::vector<Control*> _playerControls;
	std::vector<Control*> _conditionControls;
	List* _chat {nullptr};
	EditBox* _chatEdit {nullptr};
	List* _lands {nullptr};
	List* _landInfo {nullptr};
	StaticText* _clansLabel {nullptr};
	StaticText* _clanInfoLabel {nullptr};
	List* _clans {nullptr};
	List* _clanInfo {nullptr};
	Button* _useClan {nullptr};
	Button* _noClan {nullptr};
	StaticText* _playerLabel {nullptr};
	List* _playerInfo {nullptr};
	List* _conditions {nullptr};
	StaticText* _selectMap {nullptr};
	StaticText* _timeLabel {nullptr};
	StaticText* _timeLimit {nullptr};
	Button* _viewChat {nullptr};
	CheckBox* _ready {nullptr};
	size_t _chatLines {0};
	std::optional<size_t> _shownTab;
};

} // namespace openblack::gui
