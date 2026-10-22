/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// These tests connect to servers on this computer. They run only when the environment gives the addresses, and they
// skip in the usual test run:
// - OPENBLACK_TEST_WSS_ECHO: a wss:// address of a server that sends back each message, and closes with the code 4000
//   when it gets the text "close".
// - OPENBLACK_TEST_PUSH_URL and OPENBLACK_TEST_PUSH_TOKEN: the push channel of a bwgame-service and an access token.
// - OPENBLACK_TEST_WSS_CA: a file of the certificate authority of the test servers.

#if !defined(__EMSCRIPTEN__)

#include <cstdlib>

#include <algorithm>
#include <chrono>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "Online/Desktop/CurlWebSocketClient.h"
#include "Online/WebSocketPushChannel.h"

using namespace openblack;
using namespace std::chrono_literals;

namespace
{

std::optional<std::string> Environment(const char* name)
{
#if defined(_WIN32)
	char* value = nullptr;
	size_t size = 0;
	if (_dupenv_s(&value, &size, name) != 0 || value == nullptr)
	{
		return std::nullopt;
	}
	std::string text(value);
	std::free(value);
	return text.empty() ? std::nullopt : std::optional<std::string>(text);
#else
	const char* value = std::getenv(name);
	return value != nullptr && *value != '\0' ? std::optional<std::string>(value) : std::nullopt;
#endif
}

/// Moves the client on until the condition is true or the time ends
template <class Condition>
bool PollUntil(online::WebSocketClientInterface& client, Condition condition, std::chrono::milliseconds most = 10s)
{
	const auto end = std::chrono::steady_clock::now() + most;
	while (std::chrono::steady_clock::now() < end)
	{
		client.Poll();
		if (condition())
		{
			return true;
		}
		std::this_thread::sleep_for(5ms);
	}
	return false;
}

} // namespace

TEST(CurlWebSocketClient, AnAddressThatIsNotWssIsRefused)
{
	online::CurlWebSocketClient client;
	std::optional<online::WebSocketClosed> closed;
	client.Connect({.url = "ws://127.0.0.1:1/x", .headers = {}, .subprotocols = {}},
	               {.opened = {}, .text = {}, .binary = {}, .closed = [&](const auto& end) { closed = end; }});
	client.Poll();
	ASSERT_TRUE(closed.has_value());
	EXPECT_EQ(closed->end, online::WebSocketEnd::AddressRefused);
	EXPECT_EQ(client.Count(), 0u);
}

TEST(CurlWebSocketClient, TheEchoServerSendsBackTextAndLargeBinaryMessages)
{
	const auto url = Environment("OPENBLACK_TEST_WSS_ECHO");
	if (!url.has_value())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_WSS_ECHO is not set";
	}
	online::CurlWebSocketClient client(Environment("OPENBLACK_TEST_WSS_CA"));
	bool opened = false;
	std::vector<std::string> texts;
	std::vector<std::vector<uint8_t>> binaries;
	std::optional<online::WebSocketClosed> closed;
	const auto id =
	    client.Connect({.url = *url, .headers = {}, .subprotocols = {"bwgame.v1"}},
	                   {
	                       .opened = [&]() { opened = true; },
	                       .text = [&](std::string_view text) { texts.emplace_back(text); },
	                       .binary = [&](std::span<const uint8_t> bytes) { binaries.emplace_back(bytes.begin(), bytes.end()); },
	                       .closed = [&](const auto& end) { closed = end; },
	                   });
	ASSERT_TRUE(PollUntil(client, [&]() { return opened || closed.has_value(); }));
	ASSERT_TRUE(opened) << "the connection ended: " << static_cast<int>(closed->end) << " " << closed->httpStatus;

	std::vector<uint8_t> large(300'000);
	for (size_t i = 0; i < large.size(); ++i)
	{
		large[i] = static_cast<uint8_t>(i * 7);
	}
	EXPECT_TRUE(client.SendText(id, "hello"));
	EXPECT_TRUE(client.SendBinary(id, large));
	ASSERT_TRUE(PollUntil(client, [&]() { return !texts.empty() && !binaries.empty(); }));
	EXPECT_EQ(texts[0], "hello");
	EXPECT_EQ(binaries[0], large);

	EXPECT_TRUE(client.SendText(id, "close"));
	ASSERT_TRUE(PollUntil(client, [&]() { return closed.has_value(); }));
	EXPECT_EQ(closed->end, online::WebSocketEnd::ClosedByServer);
	EXPECT_EQ(closed->code, 4000);
}

TEST(CurlWebSocketClient, TheEchoServerCertificateMustMatchAnAuthority)
{
	const auto url = Environment("OPENBLACK_TEST_WSS_ECHO");
	if (!url.has_value())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_WSS_ECHO is not set";
	}
	// With no file of the test authority, the certificate of the test server is not trusted
	online::CurlWebSocketClient client;
	std::optional<online::WebSocketClosed> closed;
	client.Connect({.url = *url, .headers = {}, .subprotocols = {}},
	               {.opened = {}, .text = {}, .binary = {}, .closed = [&](const auto& end) { closed = end; }});
	ASSERT_TRUE(PollUntil(client, [&]() { return closed.has_value(); }));
	EXPECT_EQ(closed->end, online::WebSocketEnd::Tls);
}

TEST(CurlWebSocketClient, ThePushChannelOfTheLobbySaysHelloOrRefusesABadToken)
{
	const auto url = Environment("OPENBLACK_TEST_PUSH_URL");
	const auto token = Environment("OPENBLACK_TEST_PUSH_TOKEN");
	if (!url.has_value() || !token.has_value())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_PUSH_URL or OPENBLACK_TEST_PUSH_TOKEN is not set";
	}
	online::CurlWebSocketClient client(Environment("OPENBLACK_TEST_WSS_CA"));
	{
		online::WebSocketPushChannel channel(client, online::PushAuth::Header);
		std::vector<std::string> messages;
		std::optional<online::PushClose> closed;
		channel.Open(*url, *token,
		             {.opened = {},
		              .message = [&](std::string_view text) { messages.emplace_back(text); },
		              .closed = [&](online::PushClose reason) { closed = reason; }});
		ASSERT_TRUE(PollUntil(client, [&]() { return !messages.empty() || closed.has_value(); }));
		ASSERT_FALSE(messages.empty()) << "closed: " << static_cast<int>(*closed);
		EXPECT_NE(messages[0].find(R"("type":"hello")"), std::string::npos) << messages[0];
		channel.Send(R"({"type":"subscribe","channel":"lobby"})");
		// The lobby answers "subscribed", and can send the events of the channel at the same time
		const auto subscribed = [&]() {
			return std::ranges::any_of(
			    messages, [](const auto& text) { return text.find(R"("type":"subscribed")") != std::string::npos; });
		};
		EXPECT_TRUE(PollUntil(client, [&]() { return subscribed() || closed.has_value(); }));
		EXPECT_TRUE(subscribed());
		EXPECT_FALSE(closed.has_value());
	}
	{
		online::WebSocketPushChannel channel(client, online::PushAuth::Header);
		std::optional<online::PushClose> closed;
		channel.Open(*url, "not-a-token",
		             {.opened = {}, .message = {}, .closed = [&](online::PushClose reason) { closed = reason; }});
		ASSERT_TRUE(PollUntil(client, [&]() { return closed.has_value(); }));
		EXPECT_EQ(closed, online::PushClose::Refused);
	}
}

#endif
