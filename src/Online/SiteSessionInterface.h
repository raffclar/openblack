/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Online/HttpClientInterface.h"

/// The sign-in of the lobby site gives a session of the lobby to the game, with no name and no password. Only the
/// browser game has a source: the page of the site that holds the game asks the lobby with the sign-in of the site.
/// The answer is a session with an access token and no refresh token. The client asks again before the access token
/// expires, and the lobby refuses when the player signed out on the site.
namespace openblack::online
{

class SiteSessionInterface
{
public:
	virtual ~SiteSessionInterface() = default;

	/// Asks the page for a session. The callback runs in a later Poll with the answer of the lobby: 200 and the session,
	/// or the problem of the lobby. When the page cannot ask, the answer has the error Network.
	virtual void Request(HttpCallback callback) = 0;
	/// Once a frame: the callbacks of the answers that came run here
	virtual void Poll() = 0;
};

} // namespace openblack::online
