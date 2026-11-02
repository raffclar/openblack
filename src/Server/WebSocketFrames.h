/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// The server side of the WebSocket protocol (RFC 6455): the opening handshake and the frames. It has no sockets, so the
/// tests give it bytes.
namespace openblack::server::websocket_frames
{

/// The most bytes of a handshake request
inline constexpr size_t k_MostHandshakeBytes = 8 * 1024;

struct Handshake
{
	std::string path;
	/// The value of Sec-WebSocket-Key
	std::string key;
	/// The value of X-Forwarded-For, which a reverse proxy in front of the server adds. Empty when the request has none.
	std::string forwardedFor;
};

enum class HandshakeResult : uint8_t
{
	/// The request is not complete yet
	Incomplete,
	Ready,
	/// The request is not a WebSocket handshake of version 13, or it is too large
	Bad,
};

/// Reads the handshake request at the start of the bytes. With Ready, the size of the request goes to `used`.
[[nodiscard]] HandshakeResult ReadHandshake(std::string_view bytes, Handshake& out, size_t& used);
/// The answer that accepts a handshake. The accept value is base64(SHA-1(key + the protocol's GUID)), which the caller
/// gives.
[[nodiscard]] std::string AcceptAnswer(std::string_view acceptValue);
/// The answer that refuses a handshake
[[nodiscard]] std::string RefuseAnswer();
/// The text whose SHA-1 the accept value has
[[nodiscard]] std::string AcceptSource(std::string_view key);

enum class Opcode : uint8_t
{
	Continuation = 0x0,
	Text = 0x1,
	Binary = 0x2,
	Close = 0x8,
	Ping = 0x9,
	Pong = 0xA,
};

/// A frame of the server to a client: not masked, the FIN bit on
[[nodiscard]] std::vector<uint8_t> EncodeFrame(Opcode opcode, std::span<const uint8_t> payload);
/// A close frame with a code
[[nodiscard]] std::vector<uint8_t> EncodeClose(uint16_t code);

/// A whole message, or a control frame, from a client
struct Message
{
	Opcode opcode {Opcode::Binary};
	std::vector<uint8_t> payload;
};

/// Reads the frames of a client from the bytes as they come. It joins the parts of a message.
class FrameReader
{
public:
	enum class Error : uint8_t
	{
		None,
		/// A frame breaks the protocol: no mask, a reserved bit, a bad opcode, a bad continuation, a long control frame
		Protocol,
		/// A message is larger than the limit
		TooLarge,
	};

	explicit FrameReader(size_t mostMessageBytes);

	/// Adds the bytes. Gives the whole messages and control frames that they finish.
	[[nodiscard]] std::vector<Message> Feed(std::span<const uint8_t> bytes);
	[[nodiscard]] Error GetError() const { return _error; }

private:
	size_t _most;
	std::vector<uint8_t> _buffer;
	std::optional<Opcode> _messageOpcode;
	std::vector<uint8_t> _message;
	Error _error {Error::None};
};

} // namespace openblack::server::websocket_frames
