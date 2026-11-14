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
#include <string_view>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>

#include "Common/Zoomer.h"
#include "Dialog.h"
#include "LandThumbnail.h"
#include "MultiplayerFrontEnd.h"

namespace openblack::graphics
{
class Texture2D;
}

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

	/// The thumbnail of a land by the name of its file in localmaps.txt, such as "mpm_2p_1"; null when there is none
	using ThumbnailSource = std::function<std::shared_ptr<const land_thumbnail::LandThumbnail>(const std::string& file)>;

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
	/// The places of the pictures that the game room shows: the land's thumbnail and the clan's logo
	[[nodiscard]] std::vector<DialogRect> GetPictureRects() const;
	/// Where the thumbnails of the lands come from. Without a source the Landscape tab shows no thumbnail.
	void SetThumbnailSource(ThumbnailSource source) { _thumbnailSource = std::move(source); }
	/// The box shows the text of a message, for tools; empty when there is none
	[[nodiscard]] std::u16string_view GetMessageText() const;

private:
	struct MessageShown
	{
		multiplayer::Message message {multiplayer::Message::None};
		std::u16string text;
		Dialog answers;
		StaticText* textControl {nullptr};
		/// The box that takes the text of a question of the lobby sessions: a password or a code
		EditBox* edit {nullptr};
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
	void SyncPictures(bool landShown, bool clanShown);
	void DrawPictures(const DialogPainter& painter) const;
	void SendChat();
	void Join();

	// The lobby sessions (MultiplayerLobbyBoxes.cpp)
	void BuildLobbyList(Dialog& dialog);
	void BuildCreateLobby(Dialog& dialog);
	void BuildLobbyRoom(Dialog& dialog);
	void SyncLobbyList();
	void SyncCreateLobby();
	void SyncLobbyRoom();
	/// The controls of a message of the lobby sessions: the text, and an edit box for a question that takes a text
	void BuildLobbyMessage(MessageShown& shown);
	/// The keys of the boxes of the lobby sessions. True when the key did something.
	bool LobbyKeyDown(int key);
	/// The text that shows when the pointer is on a warning symbol, drawn over the box
	void DrawWarningTip(const DialogPainter& painter) const;
	[[nodiscard]] bool IsLobbyBox() const;
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

	// The boxes of the lobby sessions
	/// The sessions in the lines of the lobby list
	std::vector<lobby_api::LobbySummary> _shownLobbies;
	std::array<Button*, lobby_screens::k_ColumnCount> _headings {};
	EditBox* _search {nullptr};
	/// The panel of the main lobby room at the right of the list. The list and the edit box of its lines are _chat and
	/// _chatEdit.
	StaticText* _mainPeopleHeading {nullptr};
	List* _mainPeople {nullptr};
	/// The version of the room that the panel shows, none after a build
	std::optional<uint64_t> _mainRoomVersion;
	CheckBox* _showFull {nullptr};
	CheckBox* _showLocked {nullptr};
	CheckBox* _showInGame {nullptr};
	CheckBox* _showOtherVersions {nullptr};
	StaticText* _detail {nullptr};
	StaticText* _servers {nullptr};
	StaticText* _status {nullptr};
	StaticText* _mapLine {nullptr};
	StaticText* _countValue {nullptr};
	StaticText* _timeValue {nullptr};
	List* _slots {nullptr};
	List* _mapList {nullptr};
	std::vector<Button*> _hostButtons;
	Button* _takeSlot {nullptr};
	Button* _passwordButton {nullptr};
	Button* _closeButton {nullptr};
	Button* _settingsButton {nullptr};
	CheckBox* _privateBox {nullptr};
	EditBox* _sessionName {nullptr};
	/// The revision of the room that the controls show
	std::optional<uint32_t> _shownRevision;
	glm::ivec2 _pointer {-1, -1};
	/// The places of the warning symbols of the last draw, for the text under the pointer
	mutable std::vector<DialogRect> _warningRects;

	// The pictures of the game room
	ThumbnailSource _thumbnailSource;
	/// The land whose thumbnail the room has, and its thumbnail
	std::optional<size_t> _thumbnailLand;
	std::shared_ptr<const land_thumbnail::LandThumbnail> _thumbnail;
	bool _thumbnailShown {false};
	/// The texels of the clan's logo; empty when the clan has no logo
	land_thumbnail::Texels _logoSheet;
	bool _logoShown {false};
	/// The texture of the logo, made again in the next draw when the logo changes
	mutable std::unique_ptr<graphics::Texture2D> _logoTexture;
	mutable bool _logoChanged {false};
};

} // namespace openblack::gui
