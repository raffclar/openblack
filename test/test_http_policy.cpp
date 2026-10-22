/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Online/HttpClientInterface.h"
#include "Online/PushChannelInterface.h"

using namespace openblack::online;

TEST(HttpPolicy, TheDesktopCallsHttpsAddressesOnly)
{
	EXPECT_TRUE(http_policy::IsHttpsAddress("https://bw.example/api/v1/me"));
	EXPECT_TRUE(http_policy::IsHttpsAddress("HTTPS://bw.example"));
	EXPECT_TRUE(http_policy::IsHttpsAddress("https://127.0.0.1:8443/api"));
	EXPECT_FALSE(http_policy::IsHttpsAddress("http://bw.example/api/v1/me"));
	EXPECT_FALSE(http_policy::IsHttpsAddress("https://"));
	EXPECT_FALSE(http_policy::IsHttpsAddress("https:///path"));
	EXPECT_FALSE(http_policy::IsHttpsAddress("https://user:pw@bw.example/"));
	EXPECT_FALSE(http_policy::IsHttpsAddress("https://bw.example/a b"));
	EXPECT_FALSE(http_policy::IsHttpsAddress("ftp://bw.example"));
	EXPECT_FALSE(http_policy::IsHttpsAddress("/api/v1/me"));
}

TEST(HttpPolicy, TheBrowserCallsPathsOfTheOriginOnly)
{
	EXPECT_TRUE(http_policy::IsSameOriginPath("/api/v1/me"));
	EXPECT_TRUE(http_policy::IsSameOriginPath("/"));
	EXPECT_FALSE(http_policy::IsSameOriginPath("//other.example/api"));
	EXPECT_FALSE(http_policy::IsSameOriginPath("https://bw.example/api"));
	EXPECT_FALSE(http_policy::IsSameOriginPath("api/v1/me"));
	EXPECT_FALSE(http_policy::IsSameOriginPath("/\\other.example"));
	EXPECT_FALSE(http_policy::IsSameOriginPath(""));
}

TEST(HttpPolicy, GameSocketsAreWssOnly)
{
	EXPECT_TRUE(http_policy::IsWssAddress("wss://bw.example/game/5"));
	EXPECT_FALSE(http_policy::IsWssAddress("ws://bw.example/game/5"));
	EXPECT_FALSE(http_policy::IsWssAddress("https://bw.example/game/5"));
}

TEST(HttpPolicy, AResponseFindsItsHeadersByLowerCaseName)
{
	const HttpResponse response {.status = 200, .headers = {{"etag", "\"1\""}}, .body = {'o', 'k'}};
	EXPECT_EQ(response.Header("etag"), "\"1\"");
	EXPECT_FALSE(response.Header("ETag").has_value());
	EXPECT_EQ(response.Text(), "ok");
}

TEST(HttpPolicy, AChannelWithNoTransportClosesAtTheNextPoll)
{
	NoPushChannel channel;
	std::optional<PushClose> reason;
	channel.Open("wss://bw.example/api/v1/events", "a", PushHandlers {.closed = [&](PushClose why) { reason = why; }});
	EXPECT_FALSE(reason.has_value());
	channel.Poll();
	EXPECT_EQ(reason, PushClose::Unavailable);
	EXPECT_FALSE(channel.IsOpen());
}
