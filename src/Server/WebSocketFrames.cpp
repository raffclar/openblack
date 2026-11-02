/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WebSocketFrames.h"

#include <cctype>

#include <algorithm>
#include <array>

namespace openblack::server::websocket_frames
{
namespace
{

constexpr std::string_view k_Guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
constexpr std::string_view k_EndOfHeaders = "\r\n\r\n";
constexpr uint8_t k_Fin = 0x80;
constexpr uint8_t k_Mask = 0x80;
constexpr uint8_t k_MostShortLength = 125;
constexpr uint8_t k_Length16 = 126;
constexpr uint8_t k_Length64 = 127;

std::string Lower(std::string_view text)
{
	std::string out(text);
	std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return out;
}

std::string_view Trim(std::string_view text)
{
	while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
	{
		text.remove_prefix(1);
	}
	while (!text.empty() && (text.back() == ' ' || text.back() == '\t'))
	{
		text.remove_suffix(1);
	}
	return text;
}

/// True when a comma list of tokens has the token, ignoring case
bool ListHas(std::string_view list, std::string_view token)
{
	const auto wanted = Lower(token);
	while (!list.empty())
	{
		const auto comma = list.find(',');
		const auto item = Trim(list.substr(0, comma));
		if (Lower(item) == wanted)
		{
			return true;
		}
		if (comma == std::string_view::npos)
		{
			break;
		}
		list.remove_prefix(comma + 1);
	}
	return false;
}

bool IsControl(Opcode opcode)
{
	return (static_cast<uint8_t>(opcode) & 0x8u) != 0;
}

bool IsKnown(uint8_t opcode)
{
	return opcode <= 0x2 || (opcode >= 0x8 && opcode <= 0xA);
}

} // namespace

HandshakeResult ReadHandshake(std::string_view bytes, Handshake& out, size_t& used)
{
	const auto end = bytes.find(k_EndOfHeaders);
	if (end == std::string_view::npos)
	{
		return bytes.size() > k_MostHandshakeBytes ? HandshakeResult::Bad : HandshakeResult::Incomplete;
	}
	if (end > k_MostHandshakeBytes)
	{
		return HandshakeResult::Bad;
	}
	auto head = bytes.substr(0, end);
	const auto firstEnd = head.find("\r\n");
	const auto requestLine = head.substr(0, firstEnd);
	// GET <path> HTTP/1.1
	if (!requestLine.starts_with("GET "))
	{
		return HandshakeResult::Bad;
	}
	const auto pathEnd = requestLine.find(' ', 4);
	if (pathEnd == std::string_view::npos || !requestLine.substr(pathEnd + 1).starts_with("HTTP/1.1"))
	{
		return HandshakeResult::Bad;
	}
	Handshake handshake {.path = std::string(requestLine.substr(4, pathEnd - 4)), .key = {}, .forwardedFor = {}};
	bool upgrade = false;
	bool connection = false;
	bool version = false;
	head = firstEnd == std::string_view::npos ? std::string_view {} : head.substr(firstEnd + 2);
	while (!head.empty())
	{
		const auto lineEnd = head.find("\r\n");
		const auto line = head.substr(0, lineEnd);
		head = lineEnd == std::string_view::npos ? std::string_view {} : head.substr(lineEnd + 2);
		const auto colon = line.find(':');
		if (colon == std::string_view::npos)
		{
			return HandshakeResult::Bad;
		}
		const auto name = Lower(Trim(line.substr(0, colon)));
		const auto value = Trim(line.substr(colon + 1));
		if (name == "upgrade")
		{
			upgrade = ListHas(value, "websocket");
		}
		else if (name == "connection")
		{
			connection = ListHas(value, "upgrade");
		}
		else if (name == "sec-websocket-version")
		{
			version = value == "13";
		}
		else if (name == "sec-websocket-key")
		{
			handshake.key = std::string(value);
		}
		else if (name == "x-forwarded-for")
		{
			handshake.forwardedFor = std::string(value);
		}
	}
	// The key is the base64 text of 16 bytes
	constexpr size_t k_KeyLength = 24;
	if (!upgrade || !connection || !version || handshake.key.size() != k_KeyLength)
	{
		return HandshakeResult::Bad;
	}
	out = std::move(handshake);
	used = end + k_EndOfHeaders.size();
	return HandshakeResult::Ready;
}

std::string AcceptSource(std::string_view key)
{
	std::string source(key);
	source += k_Guid;
	return source;
}

std::string AcceptAnswer(std::string_view acceptValue)
{
	std::string answer = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
	                     "Sec-WebSocket-Accept: ";
	answer += acceptValue;
	answer += "\r\n\r\n";
	return answer;
}

std::string RefuseAnswer()
{
	return "HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Length: 0\r\n\r\n";
}

std::vector<uint8_t> EncodeFrame(Opcode opcode, std::span<const uint8_t> payload)
{
	std::vector<uint8_t> out;
	out.reserve(payload.size() + 10);
	out.push_back(static_cast<uint8_t>(k_Fin | static_cast<uint8_t>(opcode)));
	const auto size = payload.size();
	if (size <= k_MostShortLength)
	{
		out.push_back(static_cast<uint8_t>(size));
	}
	else if (size <= 0xFFFF)
	{
		out.push_back(k_Length16);
		out.push_back(static_cast<uint8_t>(size >> 8));
		out.push_back(static_cast<uint8_t>(size));
	}
	else
	{
		out.push_back(k_Length64);
		for (int shift = 56; shift >= 0; shift -= 8)
		{
			out.push_back(static_cast<uint8_t>(static_cast<uint64_t>(size) >> shift));
		}
	}
	out.insert(out.end(), payload.begin(), payload.end());
	return out;
}

std::vector<uint8_t> EncodeClose(uint16_t code)
{
	const std::array<uint8_t, 2> payload {static_cast<uint8_t>(code >> 8), static_cast<uint8_t>(code)};
	return EncodeFrame(Opcode::Close, payload);
}

FrameReader::FrameReader(size_t mostMessageBytes)
    : _most(mostMessageBytes)
{
}

std::vector<Message> FrameReader::Feed(std::span<const uint8_t> bytes)
{
	std::vector<Message> out;
	if (_error != Error::None)
	{
		return out;
	}
	_buffer.insert(_buffer.end(), bytes.begin(), bytes.end());
	size_t at = 0;
	while (true)
	{
		const auto available = _buffer.size() - at;
		if (available < 2)
		{
			break;
		}
		const auto first = _buffer[at];
		const auto second = _buffer[at + 1];
		const bool fin = (first & k_Fin) != 0;
		const auto opcodeBits = static_cast<uint8_t>(first & 0x0Fu);
		// A client's frames are always masked, and the reserved bits are clear because no extension is agreed
		if ((first & 0x70u) != 0 || (second & k_Mask) == 0 || !IsKnown(opcodeBits))
		{
			_error = Error::Protocol;
			return out;
		}
		const auto opcode = static_cast<Opcode>(opcodeBits);
		uint64_t length = second & 0x7Fu;
		size_t header = 2;
		if (length == k_Length16)
		{
			if (available < 4)
			{
				break;
			}
			length = (static_cast<uint64_t>(_buffer[at + 2]) << 8) | _buffer[at + 3];
			header = 4;
		}
		else if (length == k_Length64)
		{
			if (available < 10)
			{
				break;
			}
			length = 0;
			for (size_t i = 0; i < 8; ++i)
			{
				length = (length << 8) | _buffer[at + 2 + i];
			}
			header = 10;
		}
		if (IsControl(opcode) && (length > k_MostShortLength || !fin))
		{
			_error = Error::Protocol;
			return out;
		}
		if (length > _most || (!IsControl(opcode) && _message.size() + length > _most))
		{
			_error = Error::TooLarge;
			return out;
		}
		constexpr size_t k_MaskBytes = 4;
		if (available < header + k_MaskBytes + length)
		{
			break;
		}
		const auto* mask = &_buffer[at + header];
		const auto* data = mask + k_MaskBytes;
		std::vector<uint8_t> payload(static_cast<size_t>(length));
		for (size_t i = 0; i < payload.size(); ++i)
		{
			payload[i] = static_cast<uint8_t>(data[i] ^ mask[i % k_MaskBytes]);
		}
		at += header + k_MaskBytes + static_cast<size_t>(length);

		if (IsControl(opcode))
		{
			out.push_back(Message {.opcode = opcode, .payload = std::move(payload)});
			continue;
		}
		if (opcode == Opcode::Continuation)
		{
			if (!_messageOpcode.has_value())
			{
				_error = Error::Protocol;
				return out;
			}
		}
		else
		{
			if (_messageOpcode.has_value())
			{
				_error = Error::Protocol;
				return out;
			}
			_messageOpcode = opcode;
		}
		_message.insert(_message.end(), payload.begin(), payload.end());
		if (fin)
		{
			out.push_back(Message {.opcode = *_messageOpcode, .payload = std::move(_message)});
			_message = {};
			_messageOpcode.reset();
		}
	}
	_buffer.erase(_buffer.begin(), _buffer.begin() + static_cast<std::ptrdiff_t>(at));
	return out;
}

} // namespace openblack::server::websocket_frames
