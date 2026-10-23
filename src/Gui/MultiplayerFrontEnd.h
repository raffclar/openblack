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

#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <LobbyApi.h>

#include "MultiplayerFrontEndRules.h"

namespace openblack::ecs::systems
{
class LobbyClientSystemInterface;
class NetworkGameSystemInterface;
} // namespace openblack::ecs::systems

namespace openblack::online
{
class LanDiscoveryInterface;
class NetworkSession;
} // namespace openblack::online

namespace openblack::gui
{

/// The state of the multiplayer front end over the temple's future room, and its calls to the lobby client and the
/// game server. The boxes (MultiplayerBoxes) show this state and give it the player's clicks.
///
/// The player chooses a connection, then finds the games on the internet or on the local network. On the internet the
/// player logs in to the lobby, which gives the list of the game servers. A join asks the lobby for a ticket, then the
/// network game joins the server with it. The game room shows the server's players, its chat on the lobby's channel of
/// the server, its land, the player's clans and the winning conditions. When the server starts the game, the network
/// game keeps its session with the server, and the game loads the server's land.
///
/// The lobby gives a ticket for a full room only to an observer, so a join of a full room asks again for an observer's
/// ticket.
class MultiplayerFrontEnd
{
public:
	/// The services of the front end. A service that is missing makes its part unavailable.
	struct Services
	{
		ecs::systems::LobbyClientSystemInterface* lobby {nullptr};
		/// The network game, which joins the game server and keeps the session for the game
		ecs::systems::NetworkGameSystemInterface* network {nullptr};
		online::LanDiscoveryInterface* lan {nullptr};
		/// Opens a web page in the system's browser, or a new tab in the browser build
		std::function<void(std::string_view url)> openPage;
		/// Keeps the services above alive when the front end has services of its own, as the testbed's demonstration lobby
		std::shared_ptr<void> owner;
		/// Once a frame, for services of its own that the game does not move on
		std::function<void(MultiplayerFrontEnd& frontEnd)> poll;
	};

	struct Settings
	{
		/// The address of the registration page of the lobby's site
		std::string registerPage;
		/// The address of the account page of the lobby's site, where the player adds an email address
		std::string accountPage;
		/// The name of the internet connection in the connections' list: the lobby's host
		std::u16string internetName;
		/// The name of the player's profile, which the player has on the LAN
		std::u16string profileName;
		/// The version that the game servers must have, else the list shows "(Wrong Version)"
		std::string gameVersion;
		/// The browser build opens a game socket by its path on the page's own host
		bool browser {false};
		/// The text of "Online Maps\localmaps.txt"
		std::string localMaps;
		/// The text of "Online Maps\ConditionTemplate.txt"
		std::string conditionTemplate;
	};

	/// How the player answers a message box
	enum class Answer : uint8_t
	{
		Ok,
		Yes,
		No,
		Cancel,
	};

	MultiplayerFrontEnd(Services services, Settings settings);
	~MultiplayerFrontEnd();
	MultiplayerFrontEnd(const MultiplayerFrontEnd&) = delete;
	MultiplayerFrontEnd& operator=(const MultiplayerFrontEnd&) = delete;
	MultiplayerFrontEnd(MultiplayerFrontEnd&&) = delete;
	MultiplayerFrontEnd& operator=(MultiplayerFrontEnd&&) = delete;

	/// Once a frame
	void Update();

	[[nodiscard]] multiplayer::Box GetBox() const noexcept { return _flow.box; }
	[[nodiscard]] multiplayer::LobbyKind GetLobbyKind() const noexcept { return _flow.kind; }
	/// The message box over the box, None when there is none
	[[nodiscard]] multiplayer::Message GetMessage() const noexcept { return _message; }
	/// The front end ended since this was last asked: the player left it from the first box
	[[nodiscard]] bool TakeLeft();
	/// A call is in progress, so the box takes no other click
	[[nodiscard]] bool IsBusy() const noexcept { return _busy; }

	void Next();
	void Back();
	void AnswerMessage(Answer answer);

	// Choose a connection
	[[nodiscard]] bool HasLanConnection() const;
	[[nodiscard]] bool HasInternetConnection() const;
	/// The kind of each line of the connections' list, in order
	[[nodiscard]] std::vector<multiplayer::LobbyKind> GetConnections() const;
	[[nodiscard]] std::optional<size_t> GetConnection() const noexcept { return _connection; }
	void SelectConnection(std::optional<size_t> line);
	[[nodiscard]] const std::u16string& GetInternetName() const noexcept { return _settings.internetName; }
	[[nodiscard]] const std::string& GetGameVersion() const noexcept { return _settings.gameVersion; }

	// Find games
	void SetLobbyKind(multiplayer::LobbyKind kind) noexcept { _flow.kind = kind; }

	// Login
	[[nodiscard]] const std::u16string& GetLoginName() const noexcept { return _loginName; }
	/// Next on the login box: the name and the password go to the lobby
	void Login(std::u16string name, std::u16string password);

	// The game list
	/// The games of the lobby or of the local network, in the order that they came
	[[nodiscard]] std::span<const lobby_api::ServerEntry> GetGames() const;
	/// The games that the list shows, by their places in GetGames
	[[nodiscard]] std::vector<size_t> GetShownGames() const;
	[[nodiscard]] std::optional<size_t> GetSelectedGame() const noexcept { return _selectedGame; }
	/// A click on a line of the list (by the place in GetGames), or on no line
	void SelectGame(std::optional<size_t> game);
	/// The filter of the game names, which the password's box holds while no game is picked
	void SetFilter(std::u16string filter) { _filter = std::move(filter); }
	[[nodiscard]] const std::u16string& GetFilter() const noexcept { return _filter; }
	void RefreshGames();
	/// Join game: a ticket from the lobby, then the game socket. The password is for a game that needs one.
	void Join(std::u16string password);

	// The game room
	[[nodiscard]] const lobby_api::ServerEntry* GetRoomServer() const;
	[[nodiscard]] std::span<const multiplayer::Seat> GetSeats() const noexcept { return _seats; }
	/// The player's own place in the room, none before the room knows the player
	[[nodiscard]] const multiplayer::Seat* GetOwnSeat() const;
	/// The player's name in the caption and in the chat
	[[nodiscard]] std::u16string GetPlayerName() const;
	[[nodiscard]] multiplayer::RoomTab GetTab() const noexcept { return _tab; }
	void SelectTab(multiplayer::RoomTab tab) noexcept { _tab = tab; }
	[[nodiscard]] const std::vector<multiplayer::ChatItem>& GetChat() const noexcept { return _chat; }
	/// Return in the chat's box: the line goes to the room's channel
	void SendChat(std::u16string_view text);
	/// Whether the game server let the player in the room
	[[nodiscard]] bool IsJoined() const noexcept { return _joined; }
	/// The player is in the room as an observer, who watches the game and has no god
	[[nodiscard]] bool IsObserver() const noexcept { return _observer; }
	/// The server started the game: the network game has the session now, and the front end leaves it open
	[[nodiscard]] bool IsStarted() const;

	// The Landscape tab
	[[nodiscard]] std::span<const multiplayer::LocalMap> GetLocalMaps() const noexcept { return _maps; }
	/// The server's land in the local maps, none when the player does not have it
	[[nodiscard]] std::optional<size_t> GetServerLand() const;
	[[nodiscard]] std::optional<size_t> GetShownLand() const noexcept { return _shownLand; }
	void ShowLand(std::optional<size_t> land) noexcept { _shownLand = land; }

	// The Clans tab
	[[nodiscard]] std::span<const lobby_api::ClanSummary> GetMyClans() const noexcept { return _myClans; }
	[[nodiscard]] const std::optional<lobby_api::ClanDetails>& GetShownClan() const noexcept { return _shownClan; }
	[[nodiscard]] const std::optional<multiplayer::LogoOpacities>& GetShownLogo() const noexcept { return _shownLogo; }
	/// The clan that the player plays for in this room, none for no clan
	[[nodiscard]] std::optional<int64_t> GetUsedClan() const;
	/// A click on a clan of the player's list: its details and its logo come from the lobby
	void ShowClan(std::optional<size_t> clan);
	/// Use This Clan, for the clan that the tab shows
	void UseShownClan();
	/// Don't use any Clan.
	void UseNoClan();
	/// A change of the logo's picture since this was last asked, for the texture of the boxes
	[[nodiscard]] bool TakeLogoChanged();

	// The Player Details tab
	[[nodiscard]] std::optional<size_t> GetShownSeat() const noexcept { return _shownSeat; }
	void ShowSeat(std::optional<size_t> seat) noexcept { _shownSeat = seat; }

	// The conditions page
	/// The winning conditions of the server's land, from the template; none before a land is known
	[[nodiscard]] const std::optional<multiplayer::ConditionTemplate>& GetConditions() const noexcept { return _conditions; }
	[[nodiscard]] std::optional<uint32_t> GetTimeLimit() const noexcept { return _timeLimit; }
	[[nodiscard]] bool IsReady() const noexcept { return _ready; }
	/// The Ready box: the player is ready for the start, which the server makes
	void SetReady(bool ready);

	// For the testbed
	/// The name in the message box of a message with a name: the room's server for KickedFromGame
	[[nodiscard]] const std::u16string& GetMessageName() const noexcept { return _messageName; }

	/// Puts the front end on a box, as if the player had come to it
	void ShowBox(multiplayer::Box box, multiplayer::LobbyKind kind);

private:
	void Show(multiplayer::Message message) noexcept { _message = message; }
	void EnterGameList();
	void LeaveRoom();
	void HandleEvent(const lobby_api::PushEvent& event);
	void RequestTicket(const lobby_api::ServerEntry& server, game_socket::Role role, std::u16string password);
	void RoomOpened(std::string ticket, std::u16string password);
	/// Follows the session of the network game: the join, a refusal, a loss
	void FollowSession();
	/// Shows the steps of the start that the server makes
	void FollowStart();
	/// The server removed the player from the room: the box names the server
	void ShowKicked();
	void UpdateRoom();
	/// Puts the pick on the place of its game in a list that changed, or takes it away when its game went
	void FollowSelectedGame();
	[[nodiscard]] const online::NetworkSession* Session() const;

	Services _services;
	Settings _settings;
	multiplayer::Flow _flow;
	multiplayer::Message _message {multiplayer::Message::None};
	std::u16string _messageName;
	bool _left {false};
	bool _busy {false};
	std::optional<size_t> _connection;
	/// The calls that come back after the front end moved on are dropped: each move makes a new number
	uint32_t _generation {0};
	std::optional<uint32_t> _listener;

	std::u16string _loginName;
	std::optional<size_t> _selectedGame;
	/// The id of the picked game: the list changes on the push channel, and the pick stays on its game
	std::string _selectedId;
	std::u16string _filter;
	/// The game of the room, by its id, and a copy for the LAN
	std::optional<lobby_api::ServerEntry> _room;
	std::vector<multiplayer::Seat> _seats;
	std::vector<multiplayer::ChatItem> _chat;
	multiplayer::RoomTab _tab {multiplayer::RoomTab::Chat};
	/// The front end asked the network game to join the room's server
	bool _joining {false};
	bool _joined {false};
	bool _observer {false};
	/// The player was logged in to the lobby, so a loss of the session goes back to the login box
	bool _loggedIn {false};

	std::vector<multiplayer::LocalMap> _maps;
	std::optional<size_t> _shownLand;

	std::vector<lobby_api::ClanSummary> _myClans;
	std::optional<lobby_api::ClanDetails> _shownClan;
	std::optional<multiplayer::LogoOpacities> _shownLogo;
	bool _logoChanged {false};
	std::optional<int64_t> _usedClan;
	std::optional<size_t> _shownSeat;

	std::optional<multiplayer::ConditionTemplate> _conditions;
	std::optional<uint32_t> _timeLimit;
	bool _ready {false};
};

} // namespace openblack::gui
