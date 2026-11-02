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

#include <optional>
#include <span>
#include <string>
#include <vector>

#include <glm/vec2.hpp>

#include "ChatControlsRules.h"
#include "Dialog.h"
#include "ECS/ChatRules.h"

namespace openblack::graphics
{
class Texture2D;
} // namespace openblack::graphics

namespace openblack::gui
{

class GameFont;

/// The chat of a network game at the top right of the screen (see chat_controls):
/// - A click on the speech button opens the chat, with the focus in the line of the chat.
/// - With Shift, the press on the speech button opens the chat and the quick chat list at once. The chat closes after
///   the player lets go.
/// - Return sends the line. The chat closes when the line was empty. Escape closes the chat.
/// - The quick chat button opens the quick chat list while the mouse button is held. The pointer picks a taunt, and the
///   taunt goes when the player lets go over the list.
/// - The list of players selects the players that get the lines.
/// - Under the list, the friend button makes the selected people friends, or no longer friends.
class ChatControls
{
public:
	/// What the player sent: a typed line or a taunt
	struct Sent
	{
		std::optional<std::u16string> line;
		std::optional<uint32_t> taunt;
		bool operator==(const Sent&) const = default;
	};

	/// What each frame gives the controls
	struct Frame
	{
		chat_controls::ShowFacts show;
		/// Another box of the game is over the world
		bool otherBox {false};
		/// Shift is down with no other modifier key
		bool onlyShift {false};
		glm::ivec2 layoutSize {DialogPainter::k_Size};
	};

	/// The texts of the controls
	struct Texts
	{
		/// The quick chat list
		std::vector<std::u16string> taunts;
		/// The headings of the groups of the list of people
		std::u16string players;
		std::u16string friends;
		std::u16string others;
		/// The labels of the friend button
		std::u16string makeFriends;
		std::u16string makeNotFriends;
	};

	/// The atmosphere texture gives the envelope of an online friend. With none, the envelope does not show.
	ChatControls(const GameFont& font, const graphics::Texture2D* atmosphere);
	ChatControls(const ChatControls&) = delete;
	ChatControls& operator=(const ChatControls&) = delete;

	void SetTexts(Texts texts);
	/// The other players of the game. A new set of players makes the list again, with each player selected.
	void SetPlayers(std::vector<chat::people::Person> players);
	/// The friend button shows only when the game knows the lasting numbers of the people
	void SetFriendsOn(bool on);
	/// The friends of the profile, as the game starts
	void SetFriends(std::span<const chat::people::FriendRecord> records);
	/// The friends that the profile must keep, after the friend button changed them
	[[nodiscard]] std::optional<std::vector<chat::people::FriendRecord>> TakeChangedFriends();
	/// A person that is not a player and not a friend sent a line, and goes to the others
	void AddOther(std::u16string_view name, chat::people::UserId user);
	/// The friends that were seen now, at this time of the online clock (see people::UpdateOnline)
	void UpdateOnline(std::span<const chat::people::UserId> seen, uint32_t now);
	/// The chat opens with the focus in its line, as a click on the speech button opens it
	void Open() { OpenChat(false); }
	/// Every frame, before the input. True when the controls came on the screen in this frame.
	bool Update(const Frame& frame);

	/// The controls show and take input
	[[nodiscard]] bool IsActive() const noexcept { return _boxShown; }
	[[nodiscard]] bool IsOpen() const noexcept { return _open; }
	[[nodiscard]] bool IsQuickChatOpen() const noexcept { return _quickOpen; }
	/// The keys go to the line of the chat: the chat is open and the line has the focus, with no cinema bars
	[[nodiscard]] bool WantsKeys(bool wideScreen) const;
	/// The controls take the mouse: the mouse button is held on a control, or, with no mouse button down, the pointer is
	/// over a control
	[[nodiscard]] bool WantsMouse(bool mouseButtonDown) const;
	void MouseMove(glm::ivec2 point);
	void MouseDown(glm::ivec2 point);
	/// True when a control acted, which clicks. The modifier keys are for the list of players.
	bool MouseUp(glm::ivec2 point, bool shift, bool ctrl, bool otherModifier);
	void Wheel(glm::ivec2 point, int steps);
	bool TextInput(std::u16string_view text);
	/// A key while the keys go to the line. True when the controls took the key.
	bool KeyDown(int key, bool shift);

	/// What the player sent since the last call
	[[nodiscard]] std::vector<Sent> TakeSent();
	/// The players that get what the player sends: the selected players, or all of them when none is selected
	[[nodiscard]] std::vector<PlayerNames> Recipients() const;

	/// Draws the controls in the layout space. The caller puts the layout's top left corner on the screen's top left
	/// corner (chat_controls::Origin).
	void Draw(const DialogPainter& painter) const;

	[[nodiscard]] const chat::people::List& GetPeople() const noexcept { return _people; }
	[[nodiscard]] std::u16string_view GetLine() const noexcept;
	/// The controls that show, for tools that press them by name
	[[nodiscard]] std::vector<Dialog::NamedControl> GetNamedControls() const;

private:
	class PictureButton;
	enum class Clicked : uint8_t
	{
		None,
		Open,
		Send,
		QuickList,
		Players,
		Friend,
	};

	void OpenChat(bool closeAfterSend);
	void Send(std::optional<uint32_t> taunt);
	void StartQuickChat(glm::ivec2 point);
	void ClickPlayers(glm::ivec2 point, bool shift, bool ctrl, bool otherModifier);
	void RebuildRows();
	void MakeFriends();
	[[nodiscard]] std::u16string HeadingOf(chat::people::Group group) const;
	void Layout();
	void FitQuickList();

	const GameFont& _font;
	Dialog _dialog;
	PictureButton* _openButton {nullptr};
	PictureButton* _sendButton {nullptr};
	PictureButton* _quickButton {nullptr};
	EditBox* _edit {nullptr};
	List* _players {nullptr};
	List* _quickList {nullptr};
	Button* _friendButton {nullptr};
	const graphics::Texture2D* _atmosphere {nullptr};

	std::vector<std::u16string> _taunts;
	Texts _texts;
	chat::people::List _people;
	/// The row of each item of the list of people
	std::vector<chat::people::Row> _rows;
	std::optional<std::vector<chat::people::FriendRecord>> _changedFriends;
	bool _friendsOn {false};
	chat::people::FriendButton _friendState;
	std::vector<Sent> _sent;
	Clicked _clicked {Clicked::None};
	glm::ivec2 _pointer {-1, -1};
	glm::ivec2 _layoutSize {DialogPainter::k_Size};
	bool _shown {true};
	bool _boxShown {false};
	bool _open {false};
	bool _closeAfterSend {false};
	bool _quickOpen {false};
	bool _onlyShift {false};
};

} // namespace openblack::gui
