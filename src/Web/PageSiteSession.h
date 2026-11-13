/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#if !defined(__EMSCRIPTEN__)
#error "The web platform code is only for the web build"
#endif

#include <cstdint>

#include <utility>
#include <vector>

#include "Online/SiteSessionInterface.h"

namespace openblack::web
{

/// The session of the lobby from the sign-in of the site, in the web build. The game thread asks the game page. The game
/// page asks the page of the site around it, which asks the lobby with the site session and its CSRF token. The answer
/// comes back the same way, in messages only: the access token goes into no address, log, file or store of the
/// browser. With no page of the site around the game page, the answer has the error Network.
class PageSiteSession final: public online::SiteSessionInterface
{
public:
	void Request(online::HttpCallback callback) override;
	void Poll() override;

private:
	/// The number of each question that waits, and its callback
	std::vector<std::pair<int32_t, online::HttpCallback>> _waiting;
};

} // namespace openblack::web
