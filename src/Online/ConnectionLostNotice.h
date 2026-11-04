/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Online/NetworkSession.h"

namespace openblack::online
{

/// The facts of one frame that decide the box "Connection to network game has been lost. Cannot continue."
struct ConnectionLostFacts
{
	/// The computer has a network session
	bool hasSession {false};
	/// The game of the session started
	bool started {false};
	NetworkSession::State state {NetworkSession::State::Idle};
	NetworkSession::End end {NetworkSession::End::None};
	/// A run of the tools shows no box: its game stops when its network game ends
	bool toolRun {false};
};

/// The network game ended because the connection to the game server is gone, and the player did not choose it: the
/// tries to come back did not get through in time, the server refused the client, or the server sent more than the
/// client keeps. When the player leaves, or the world is out of step, the connection is not lost.
[[nodiscard]] constexpr bool LostTheConnection(NetworkSession::End end) noexcept
{
	switch (end)
	{
	case NetworkSession::End::Refused:
	case NetworkSession::End::Lost:
	case NetworkSession::End::NoConnection:
	case NetworkSession::End::ServerLimit:
		return true;
	case NetworkSession::End::None:
	case NetworkSession::End::Left:
	case NetworkSession::End::OutOfSync:
		break;
	}
	return false;
}

/// Tells when the box that the connection to the network game is lost shows: once for each network session, when a
/// started game ends because the connection is gone. The tries to come back come first, so the box shows only after
/// the last try. Before the start, the front end tells the player.
class ConnectionLostNotice
{
public:
	/// True at the frame when the box shows
	[[nodiscard]] bool Update(const ConnectionLostFacts& facts) noexcept
	{
		// A new network session can tell again
		if (!facts.hasSession)
		{
			_told = false;
			return false;
		}
		if (_told || facts.toolRun || !facts.started || facts.state != NetworkSession::State::Ended ||
		    !LostTheConnection(facts.end))
		{
			return false;
		}
		_told = true;
		return true;
	}

	/// The box showed for the network session of this computer
	[[nodiscard]] bool Told() const noexcept { return _told; }

private:
	bool _told {false};
};

} // namespace openblack::online
