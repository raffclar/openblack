/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PageSiteSession.h"

#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <string_view>
#include <utility>
#include <vector>

#include <emscripten.h>

// clang-format off
// The game thread sends the question to the game page (post.js). The answer comes back to this thread in a message,
// and the code before the game (pre.js) keeps it under its number until the game takes it.
EM_JS(int, openblack_site_session_ask, (), {
	const id = (globalThis.openblackSiteSessionNext || 0) + 1;
	globalThis.openblackSiteSessionNext = id;
	postMessage({ openblackSiteSessionRequest: id });
	return id;
});

// The HTTP status of the answer, 0 when the page could not ask, or -1 while the answer has not come
EM_JS(int, openblack_site_session_status, (int id), {
	const replies = globalThis.openblackSiteSessionReplies;
	const reply = replies ? replies.get(id) : undefined;
	return reply ? (reply.status | 0) : -1;
});

// Gives the body of the answer to the game and forgets the answer
EM_JS(char*, openblack_site_session_take_body, (int id), {
	const replies = globalThis.openblackSiteSessionReplies;
	const reply = replies ? replies.get(id) : undefined;
	if (replies) {
		replies.delete(id);
	}
	return stringToNewUTF8(reply && typeof reply.body === 'string' ? reply.body : "");
});

EM_JS_DEPS(openblack_page_site_session, "$stringToNewUTF8");
// clang-format on

namespace openblack::web
{

void PageSiteSession::Request(online::HttpCallback callback)
{
	_waiting.emplace_back(static_cast<int32_t>(openblack_site_session_ask()), std::move(callback));
}

void PageSiteSession::Poll()
{
	std::vector<std::pair<int32_t, online::HttpCallback>> done;
	for (auto& entry : _waiting)
	{
		if (openblack_site_session_status(entry.first) >= 0)
		{
			done.push_back(std::move(entry));
			entry.first = 0;
		}
	}
	std::erase_if(_waiting, [](const auto& entry) { return entry.first == 0; });
	for (auto& [id, callback] : done)
	{
		online::HttpResponse response;
		const auto status = openblack_site_session_status(id);
		char* body = openblack_site_session_take_body(id);
		if (status == 0)
		{
			response.error = online::HttpError::Network;
		}
		else
		{
			response.status = status;
			const std::string_view text(body);
			response.body.assign(text.begin(), text.end());
		}
		std::free(body);
		if (callback)
		{
			callback(std::move(response));
		}
	}
}

} // namespace openblack::web
