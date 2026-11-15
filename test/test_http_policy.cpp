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

// The test authorities of the tools trust only this computer. A name that starts with "127." is a host of the internet.
TEST(HttpPolicy, OnlyLocalhostAndAnAddressOf127AreThisComputer)
{
	EXPECT_TRUE(http_policy::IsLoopbackHost("localhost"));
	EXPECT_TRUE(http_policy::IsLoopbackHost("127.0.0.1"));
	EXPECT_TRUE(http_policy::IsLoopbackHost("127.255.0.9"));
	EXPECT_FALSE(http_policy::IsLoopbackHost("127.evil.com"));
	EXPECT_FALSE(http_policy::IsLoopbackHost("127.0.0.1.evil.com"));
	EXPECT_FALSE(http_policy::IsLoopbackHost("127.0.0"));
	EXPECT_FALSE(http_policy::IsLoopbackHost("127.0.0.256"));
	EXPECT_FALSE(http_policy::IsLoopbackHost("127.0.0.0001"));
	EXPECT_FALSE(http_policy::IsLoopbackHost("127..0.1"));
	EXPECT_FALSE(http_policy::IsLoopbackHost("127.0.0.1."));
	EXPECT_FALSE(http_policy::IsLoopbackHost("128.0.0.1"));
	EXPECT_FALSE(http_policy::IsLoopbackHost(""));
	EXPECT_TRUE(http_policy::IsLoopbackAddress("wss://127.0.0.1:7650/game/a"));
	EXPECT_TRUE(http_policy::IsLoopbackAddress("https://localhost/api"));
	EXPECT_FALSE(http_policy::IsLoopbackAddress("wss://127.evil.com/game/a"));
	EXPECT_FALSE(http_policy::IsLoopbackAddress("https://127.0.0.1.evil.com:443/api"));
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

TEST(HttpPolicy, TheHeaderLinesOfTheBrowserGiveNamesAndValuesWithNoSpaceAndNoLineEnd)
{
	// The text of a range answer as the browser gives it: CR LF after each line, a space after each colon
	const auto headers = http_policy::ParseHeaderLines("Accept-Ranges: bytes\r\n"
	                                                   "Content-Range: bytes 0-262143/2176738\r\n"
	                                                   "content-length:262144\n"
	                                                   "no colon\r\n"
	                                                   ": no name\r\n"
	                                                   "ETag: \"a:b\"\r\n");
	ASSERT_EQ(headers.size(), 4u);
	EXPECT_EQ(headers[0], (std::pair<std::string, std::string> {"accept-ranges", "bytes"}));
	EXPECT_EQ(headers[1], (std::pair<std::string, std::string> {"content-range", "bytes 0-262143/2176738"}));
	EXPECT_EQ(headers[2], (std::pair<std::string, std::string> {"content-length", "262144"}));
	EXPECT_EQ(headers[3], (std::pair<std::string, std::string> {"etag", "\"a:b\""}));
	EXPECT_TRUE(http_policy::ParseHeaderLines("").empty());
}
