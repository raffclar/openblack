/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Online/NetworkLand.h"
#include "Server/ConnectionLimits.h"
#include "Server/WebSocketFrames.h"

using namespace openblack;
using namespace openblack::server::connection_limits;

TEST(ConnectionLimits, AnAddressHasAtMostItsConnections)
{
	AddressCounts counts;
	for (size_t i = 0; i < k_MostConnectionsPerAddress; ++i)
	{
		EXPECT_TRUE(counts.Add("203.0.113.7"));
	}
	EXPECT_FALSE(counts.Add("203.0.113.7"));
	EXPECT_EQ(counts.Of("203.0.113.7"), k_MostConnectionsPerAddress);
	// Another address has its own count
	EXPECT_TRUE(counts.Add("203.0.113.8"));
	// A connection that ends frees a place
	counts.Remove("203.0.113.7");
	EXPECT_TRUE(counts.Add("203.0.113.7"));
	// The tools on this computer have no limit
	for (size_t i = 0; i < 3 * k_MostConnectionsPerAddress; ++i)
	{
		EXPECT_TRUE(counts.Add("127.0.0.1"));
	}
}

TEST(ConnectionLimits, BehindTheProxyTheClientIsTheLastForwardedAddress)
{
	EXPECT_EQ(ClientAddress("127.0.0.1", "198.51.100.1"), "198.51.100.1");
	// A client can put any address at the start of the header. The proxy adds the address that it saw at the end.
	EXPECT_EQ(ClientAddress("127.0.0.1", "10.0.0.1, 198.51.100.2"), "198.51.100.2");
	EXPECT_EQ(ClientAddress("127.0.0.1", ""), "127.0.0.1");
	// A client that connects from another computer cannot give its own address in the header
	EXPECT_EQ(ClientAddress("203.0.113.9", "198.51.100.3"), "203.0.113.9");
}

TEST(ConnectionLimits, TheHandshakeKeepsTheForwardedAddress)
{
	const std::string request = "GET /game/1 HTTP/1.1\r\nHost: 127.0.0.1\r\nUpgrade: websocket\r\n"
	                            "Connection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
	                            "X-Forwarded-For: 198.51.100.4\r\nSec-WebSocket-Version: 13\r\n\r\n";
	server::websocket_frames::Handshake handshake;
	size_t used = 0;
	ASSERT_EQ(server::websocket_frames::ReadHandshake(request, handshake, used),
	          server::websocket_frames::HandshakeResult::Ready);
	EXPECT_EQ(handshake.forwardedFor, "198.51.100.4");
}

TEST(ConnectionLimits, TheQueueRefusesBytesOverItsLimitAndKeepsTheOrder)
{
	OutQueue queue;
	const std::vector<uint8_t> bytes {1, 2, 3, 4, 5};
	ASSERT_TRUE(queue.Add(bytes.data(), bytes.size(), 8));
	EXPECT_FALSE(queue.Add(bytes.data(), bytes.size(), 8));
	EXPECT_EQ(queue.Size(), 5u);
	queue.Sent(2);
	EXPECT_EQ(queue.Size(), 3u);
	EXPECT_EQ(queue.Data()[0], 3);
	ASSERT_TRUE(queue.Add(bytes.data(), bytes.size(), 8));
	EXPECT_EQ(queue.Size(), 8u);
	queue.Sent(8);
	EXPECT_TRUE(queue.Empty());
	// A large queue that goes out in small steps keeps its order
	std::vector<uint8_t> large(300 * 1024);
	for (size_t i = 0; i < large.size(); ++i)
	{
		large[i] = static_cast<uint8_t>(i * 7);
	}
	ASSERT_TRUE(queue.Add(large.data(), large.size()));
	for (size_t sent = 0; sent < large.size(); sent += 1000)
	{
		ASSERT_EQ(queue.Data()[0], large[sent]);
		queue.Sent(1000);
	}
	EXPECT_TRUE(queue.Empty());
}

TEST(NetworkLandNames, OnlyPlainNamesAreLandsOfAServer)
{
	using network_land::IsPlainWireName;
	EXPECT_TRUE(IsPlainWireName("Land1.txt"));
	EXPECT_TRUE(IsPlainWireName("Playgrounds/construct.txt"));
	EXPECT_TRUE(IsPlainWireName("Online Maps/mpm_2p_1.map"));
	EXPECT_TRUE(IsPlainWireName("a..b/c.txt"));
	for (const std::string name : {"",
	                               "/etc/passwd",
	                               "//host/share/x.txt",
	                               "\\\\host\\share\\x.txt",
	                               "\\\\?\\C:\\x.txt",
	                               "C:/x.txt",
	                               "C:x.txt",
	                               "../x.txt",
	                               "a/../x.txt",
	                               "a/./x.txt",
	                               "a//x.txt",
	                               "x.txt/",
	                               "NUL",
	                               "nul.txt",
	                               "Con",
	                               "aux.map",
	                               "COM1",
	                               "lpt9.txt",
	                               "conin$",
	                               "x.txt:stream",
	                               "x.txt.",
	                               "x.txt ",
	                               "x?.txt",
	                               "x*.txt",
	                               "x<y.txt",
	                               "a\nb.txt"})
	{
		EXPECT_FALSE(IsPlainWireName(name)) << name;
	}
	EXPECT_FALSE(IsPlainWireName(std::string(network_land::k_MostWireNameBytes + 1, 'a')));
	// A name that only starts like a device is a plain name
	EXPECT_TRUE(IsPlainWireName("console.txt"));
	EXPECT_TRUE(IsPlainWireName("com10.txt"));
	// The path of a land of a server is never a path to a file of this computer
	const std::filesystem::path scripts("game/Scripts");
	EXPECT_FALSE(network_land::PathOf("\\\\host\\share\\x.txt", scripts).has_value());
	EXPECT_FALSE(network_land::PathOf("NUL", scripts).has_value());
	EXPECT_EQ(network_land::PathOf("Land1.txt", scripts), scripts / "Land1.txt");
}

TEST(NetworkLandNames, AWorldStateOfAServerMustNameTheOpenLand)
{
	using network_land::NamesOpenLand;
	const std::filesystem::path land("D:/games/black/Scripts/Land1.txt");
	const std::filesystem::path landscape("D:/games/black/Data/Landscape/Land1.lnd");
	// The server's own paths name the same files
	EXPECT_TRUE(NamesOpenLand("C:/server/black/Scripts/Land1.txt", "C:\\server\\Data\\Landscape\\land1.lnd", land, landscape));
	EXPECT_TRUE(NamesOpenLand("Land1.txt", "Land1.lnd", land, landscape));
	// Another land, another landscape or a share
	EXPECT_FALSE(NamesOpenLand("Land2.txt", "Land1.lnd", land, landscape));
	EXPECT_FALSE(NamesOpenLand("Land1.txt", "Land2.lnd", land, landscape));
	EXPECT_FALSE(NamesOpenLand("Land1.txt", "", land, landscape));
	EXPECT_FALSE(NamesOpenLand("\\\\host\\share\\Land1.txt:x", "Land1.lnd", land, landscape));
	EXPECT_FALSE(NamesOpenLand("Land1.txt", "Land1.lnd", {}, landscape));
	// A land that the game builds has no landscape file
	EXPECT_TRUE(NamesOpenLand("testbed", "", std::filesystem::path("testbed"), {}));
	EXPECT_FALSE(NamesOpenLand("testbed", "\\\\host\\share\\x.lnd", std::filesystem::path("testbed"), {}));
}
