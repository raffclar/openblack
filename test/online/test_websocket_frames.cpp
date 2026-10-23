/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Server/WebSocketFrames.h"

using namespace openblack::server::websocket_frames;

namespace
{

constexpr std::string_view k_Request = "GET /game/1 HTTP/1.1\r\nHost: 127.0.0.1\r\nUpgrade: websocket\r\n"
                                       "Connection: keep-alive, Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                                       "Sec-WebSocket-Version: 13\r\n\r\n";

/// A frame of a client: masked with a key
std::vector<uint8_t> ClientFrame(uint8_t first, std::span<const uint8_t> payload)
{
	constexpr std::array<uint8_t, 4> k_Mask {0x12, 0x34, 0x56, 0x78};
	std::vector<uint8_t> out {first};
	if (payload.size() <= 125)
	{
		out.push_back(static_cast<uint8_t>(0x80 | payload.size()));
	}
	else
	{
		out.push_back(0x80 | 126);
		out.push_back(static_cast<uint8_t>(payload.size() >> 8));
		out.push_back(static_cast<uint8_t>(payload.size()));
	}
	out.insert(out.end(), k_Mask.begin(), k_Mask.end());
	for (size_t i = 0; i < payload.size(); ++i)
	{
		out.push_back(static_cast<uint8_t>(payload[i] ^ k_Mask[i % 4]));
	}
	return out;
}

std::vector<uint8_t> Bytes(std::string_view text)
{
	return {text.begin(), text.end()};
}

} // namespace

TEST(WebSocketFrames, TheHandshakeOfAClientReads)
{
	Handshake handshake;
	size_t used = 0;
	const auto request = std::string(k_Request) + "rest";
	ASSERT_EQ(ReadHandshake(request, handshake, used), HandshakeResult::Ready);
	EXPECT_EQ(handshake.path, "/game/1");
	EXPECT_EQ(handshake.key, "dGhlIHNhbXBsZSBub25jZQ==");
	EXPECT_EQ(used, k_Request.size());
	EXPECT_EQ(AcceptSource(handshake.key), "dGhlIHNhbXBsZSBub25jZQ==258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
	EXPECT_NE(AcceptAnswer("abc").find("Sec-WebSocket-Accept: abc\r\n"), std::string::npos);

	EXPECT_EQ(ReadHandshake(k_Request.substr(0, 40), handshake, used), HandshakeResult::Incomplete);
	EXPECT_EQ(ReadHandshake("POST / HTTP/1.1\r\n\r\n", handshake, used), HandshakeResult::Bad);
	std::string oldVersion(k_Request);
	oldVersion.replace(oldVersion.find("13"), 2, "8");
	EXPECT_EQ(ReadHandshake(oldVersion, handshake, used), HandshakeResult::Bad);
	EXPECT_EQ(ReadHandshake(std::string(k_MostHandshakeBytes + 1, 'a'), handshake, used), HandshakeResult::Bad);
}

TEST(WebSocketFrames, TheServerFramesHaveTheirLengthAndNoMask)
{
	const auto small = EncodeFrame(Opcode::Text, Bytes("hi"));
	EXPECT_EQ(small, (std::vector<uint8_t> {0x81, 2, 'h', 'i'}));
	const std::vector<uint8_t> middle(300, 7);
	const auto frame = EncodeFrame(Opcode::Binary, middle);
	ASSERT_EQ(frame.size(), 304u);
	EXPECT_EQ(frame[1], 126);
	EXPECT_EQ((frame[2] << 8) | frame[3], 300);
	const std::vector<uint8_t> large(70000, 1);
	const auto big = EncodeFrame(Opcode::Binary, large);
	EXPECT_EQ(big[1], 127);
	EXPECT_EQ(big.size(), 70010u);
	EXPECT_EQ(EncodeClose(1000), (std::vector<uint8_t> {0x88, 2, 0x03, 0xE8}));
}

TEST(WebSocketFrames, TheReaderJoinsPartsAndUnmasks)
{
	FrameReader reader(1024);
	auto bytes = ClientFrame(0x01, Bytes("hel")); // text, not the last part
	auto ping = ClientFrame(0x89, Bytes("p"));    // a ping between the parts
	auto last = ClientFrame(0x80, Bytes("lo"));   // continuation, the last part
	bytes.insert(bytes.end(), ping.begin(), ping.end());
	bytes.insert(bytes.end(), last.begin(), last.end());
	// The bytes come one at a time
	std::vector<Message> messages;
	for (const auto byte : bytes)
	{
		for (auto& message : reader.Feed(std::span(&byte, 1)))
		{
			messages.push_back(std::move(message));
		}
	}
	ASSERT_EQ(messages.size(), 2u);
	EXPECT_EQ(messages[0].opcode, Opcode::Ping);
	EXPECT_EQ(messages[1].opcode, Opcode::Text);
	EXPECT_EQ(std::string(messages[1].payload.begin(), messages[1].payload.end()), "hello");
	EXPECT_EQ(reader.GetError(), FrameReader::Error::None);

	const std::vector<uint8_t> payload(500, 3);
	const auto binary = reader.Feed(ClientFrame(0x82, payload));
	ASSERT_EQ(binary.size(), 1u);
	EXPECT_EQ(binary[0].payload, payload);
}

TEST(WebSocketFrames, TheReaderStopsAtABrokenFrame)
{
	{
		FrameReader reader(1024);
		// No mask
		const std::vector<uint8_t> unmasked {0x81, 0x01, 'a'};
		EXPECT_TRUE(reader.Feed(unmasked).empty());
		EXPECT_EQ(reader.GetError(), FrameReader::Error::Protocol);
	}
	{
		FrameReader reader(1024);
		// A continuation with no message
		EXPECT_TRUE(reader.Feed(ClientFrame(0x80, Bytes("a"))).empty());
		EXPECT_EQ(reader.GetError(), FrameReader::Error::Protocol);
	}
	{
		FrameReader reader(1024);
		// A reserved bit
		EXPECT_TRUE(reader.Feed(ClientFrame(0xC1, Bytes("a"))).empty());
		EXPECT_EQ(reader.GetError(), FrameReader::Error::Protocol);
	}
	{
		FrameReader reader(16);
		EXPECT_TRUE(reader.Feed(ClientFrame(0x82, std::vector<uint8_t>(17, 0))).empty());
		EXPECT_EQ(reader.GetError(), FrameReader::Error::TooLarge);
	}
}
