/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The key of a game server on the local network (deviation 113, LAN-1 b): the server makes a certificate of its own, its
// answer to the search gives the pin of the public key, and the client accepts only that key.

#include <cstdint>

#include <chrono>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <LobbyApiJson.h>
#include <gtest/gtest.h>

#include "Online/Desktop/CurlWebSocketClient.h"
#include "Online/LanDiscovery.h"
#include "Online/ServerKeyPin.h"
#include "Server/Desktop/OwnCertificate.h"
#include "Server/Desktop/SocketWebSocketServer.h"
#include "Server/LanRoom.h"
#include "Server/LobbyStatus.h"

using namespace openblack;
using namespace std::chrono_literals;
namespace server_key = online::server_key;

namespace
{

constexpr std::string_view k_Key = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";

server_key::Pin Counting()
{
	server_key::Pin pin {};
	for (size_t index = 0; index < pin.size(); ++index)
	{
		pin.at(index) = static_cast<uint8_t>(index);
	}
	return pin;
}

/// Moves the server and the client on until the condition is true or the time is over
bool PollUntil(server::SocketWebSocketServer& sockets, online::CurlWebSocketClient& client,
               const std::function<bool()>& condition)
{
	const auto end = std::chrono::steady_clock::now() + 10s;
	while (std::chrono::steady_clock::now() < end)
	{
		sockets.Poll();
		client.Poll();
		if (condition())
		{
			return true;
		}
		std::this_thread::sleep_for(2ms);
	}
	return false;
}

/// The result of one connection of the client to the server
struct Outcome
{
	bool opened {false};
	std::optional<online::WebSocketClosed> closed;
};

Outcome Connect(server::SocketWebSocketServer& sockets, std::optional<server_key::Pin> pin)
{
	online::CurlWebSocketClient client;
	Outcome outcome;
	client.Connect({.url = "wss://127.0.0.1:" + std::to_string(sockets.Port()) + "/game",
	                .headers = {},
	                .subprotocols = {},
	                .serverKey = pin},
	               {.opened = [&outcome]() { outcome.opened = true; },
	                .text = {},
	                .binary = {},
	                .closed = [&outcome](const online::WebSocketClosed& closed) { outcome.closed = closed; }});
	EXPECT_TRUE(PollUntil(sockets, client, [&outcome]() { return outcome.opened || outcome.closed.has_value(); }));
	return outcome;
}

} // namespace

TEST(LanServerKey, APinReadsFromAndWritesTo64HexDigits)
{
	const auto pin = server_key::ParseHex(k_Key);
	ASSERT_TRUE(pin.has_value());
	EXPECT_EQ(*pin, Counting());
	EXPECT_EQ(server_key::HexOf(*pin), k_Key);
	// Upper case reads too, and the text comes back in lower case
	const auto upper = server_key::ParseHex("000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F");
	ASSERT_TRUE(upper.has_value());
	EXPECT_EQ(server_key::HexOf(*upper), k_Key);
	// Other text is no pin
	EXPECT_FALSE(server_key::ParseHex("").has_value());
	EXPECT_FALSE(server_key::ParseHex(k_Key.substr(1)).has_value());
	EXPECT_FALSE(server_key::ParseHex(std::string(k_Key) + "0").has_value());
	EXPECT_FALSE(server_key::ParseHex("g00102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f").has_value());
}

TEST(LanServerKey, ThePinOfLibcurlIsBase64OfThePin)
{
	EXPECT_EQ(server_key::CurlPinOf(Counting()), "sha256//AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8=");
	EXPECT_EQ(server_key::CurlPinOf(server_key::Pin {}), "sha256//AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=");
}

TEST(LanServerKey, TheAnswerOfTheServerCarriesTheKeyToTheLineOfTheList)
{
	const lobby_status::Room room {
	    .hostname = "Office game", .land = "Online Maps/mpm_2p_1.map", .maxGods = 2, .version = "0.1.0"};
	const auto answer = server::lan_room::AnswerOf(room, false, {}, 4433, "", std::string(k_Key));
	EXPECT_EQ(answer.key, k_Key);
	const auto read = lobby_api::ParseLanAnswer(lobby_api::ToJson(answer));
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(read->key, k_Key);
	const auto entry = online::lan_discovery::EntryOf(*read, 7, online::lan_discovery::V4(192, 168, 1, 20));
	EXPECT_EQ(entry.key, k_Key);
	EXPECT_EQ(entry.url, "wss://192.168.1.20:4433/game");
}

TEST(LanServerKey, AnAnswerWithNoKeyOrABadKeyGivesALineWithNoKey)
{
	const lobby_status::Room room {
	    .hostname = "Office game", .land = "Online Maps/mpm_2p_1.map", .maxGods = 2, .version = "0.1.0"};
	auto answer = server::lan_room::AnswerOf(room, false, {}, 4433, "office.example", "");
	// An answer of an older server has no key field
	auto text = lobby_api::ToJson(answer);
	const auto field = text.find(",\"key\":\"\"");
	ASSERT_NE(field, std::string::npos);
	text.erase(field, std::string(",\"key\":\"\"").size());
	const auto old = lobby_api::ParseLanAnswer(text);
	ASSERT_TRUE(old.has_value());
	EXPECT_TRUE(online::lan_discovery::EntryOf(*old, 7, online::lan_discovery::V4(192, 168, 1, 20)).key.empty());
	// A key that is not 64 hex digits is no key
	answer.key = "not a key";
	EXPECT_TRUE(online::lan_discovery::EntryOf(answer, 7, online::lan_discovery::V4(192, 168, 1, 20)).key.empty());
	// Upper case comes to the list in lower case
	answer.key = "000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F";
	EXPECT_EQ(online::lan_discovery::EntryOf(answer, 7, online::lan_discovery::V4(192, 168, 1, 20)).key, k_Key);
}

TEST(LanServerKey, EachOwnCertificateHasANewKeyAndItsPin)
{
	const auto first = server::own_certificate::Make("openblack game server");
	const auto second = server::own_certificate::Make("openblack game server");
	ASSERT_TRUE(first.has_value());
	ASSERT_TRUE(second.has_value());
	EXPECT_NE(first->pin, second->pin);
	EXPECT_NE(first->keyPem, second->keyPem);
	EXPECT_EQ(server::own_certificate::PinOfCertificate(first->certificatePem), first->pin);
	EXPECT_EQ(server::own_certificate::PinOfCertificate(second->certificatePem), second->pin);
	EXPECT_FALSE(server::own_certificate::PinOfCertificate("no certificate").has_value());
}

TEST(LanServerKey, TheServerRefusesItsOwnCertificateWithACertificateFile)
{
	server::SocketWebSocketServer sockets;
	const auto error = sockets.Listen({.address = "127.0.0.1",
	                                   .port = 0,
	                                   .certificate = std::filesystem::path("server.pem"),
	                                   .key = std::filesystem::path("server.key"),
	                                   .ownCertificate = true});
	EXPECT_TRUE(error.has_value());
	EXPECT_FALSE(sockets.OwnKeyPin().has_value());
}

TEST(LanServerKey, AClientWithThePinJoinsAndAClientWithAnotherPinOrNoPinIsRefused)
{
	server::SocketWebSocketServer sockets;
	size_t opened = 0;
	sockets.SetHandlers({.opened = [&opened](server::ConnectionId) { ++opened; }, .text = {}, .binary = {}, .closed = {}});
	// A server on the local network address of a computer listens with its own certificate, also off the loopback
	// address. The test uses the loopback address.
	const auto error = sockets.Listen(
	    {.address = "127.0.0.1", .port = 0, .certificate = std::nullopt, .key = std::nullopt, .ownCertificate = true});
	ASSERT_FALSE(error.has_value()) << *error;
	const auto pin = sockets.OwnKeyPin();
	ASSERT_TRUE(pin.has_value());

	const auto joined = Connect(sockets, pin);
	EXPECT_TRUE(joined.opened);
	EXPECT_FALSE(joined.closed.has_value());

	auto other = *pin;
	other.at(0) ^= 0x01;
	const auto wrong = Connect(sockets, other);
	EXPECT_FALSE(wrong.opened);
	ASSERT_TRUE(wrong.closed.has_value());
	EXPECT_EQ(wrong.closed->end, online::WebSocketEnd::Tls);

	// With no pin, the certificate must come from an authority, so the client refuses it
	const auto none = Connect(sockets, std::nullopt);
	EXPECT_FALSE(none.opened);
	ASSERT_TRUE(none.closed.has_value());
	EXPECT_EQ(none.closed->end, online::WebSocketEnd::Tls);

	// Only the client with the pin got to the server's WebSocket handshake
	EXPECT_EQ(opened, 1u);
}
