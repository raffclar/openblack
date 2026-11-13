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

#include <deque>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <spdlog/common.h>

#include "Online/WebSocketInterface.h"

/// The connection log of a network game: each step of the connection with the game server, with its time. The lines go
/// to the game log with a tag, and the log keeps the last lines for the "Copy diagnostics" report. No line holds a
/// ticket, a password, a token or the secret of a session.
namespace openblack::online
{

/// The tag at the start of each line of the connection log in the game log. The web page finds the lines by this tag.
inline constexpr std::string_view k_NetTag = "net: ";

/// The text with each secret in it replaced by a mark: a PASETO token, a bearer token, and the value of a field whose
/// name tells of a secret (ticket, password, token, session, secret).
[[nodiscard]] std::string MaskSecrets(std::string_view text);

/// The meaning of a WebSocket close code in a few words, such as "refused" for 4003. Empty for a code with no meaning.
[[nodiscard]] std::string_view CloseCodeMeaning(uint16_t code) noexcept;

/// Why a game socket ended, and which side ended it, in a few words
[[nodiscard]] std::string DescribeClosed(const WebSocketClosed& closed);

/// The hash of each part of the synced world at a turn, by the name of the part
using PartHashes = std::vector<std::pair<std::string, uint64_t>>;

/// The parts whose hashes are not the same in two worlds. A part that only one world has counts too, with the value 0
/// for the world that does not have it.
struct PartDifference
{
	std::string name;
	uint64_t local {0};
	uint64_t other {0};
};
[[nodiscard]] std::vector<PartDifference> DifferentParts(std::span<const std::pair<std::string, uint64_t>> local,
                                                         std::span<const std::pair<std::string, uint64_t>> other);

/// One line that names the parts that differ, with the two values of each under their labels, up to the limit
[[nodiscard]] std::string DescribeParts(std::span<const PartDifference> parts, std::string_view localLabel,
                                        std::string_view otherLabel, size_t most = 8);

class ConnectionLog
{
public:
	/// Microseconds of a steady clock
	using Clock = std::function<int64_t()>;
	/// Gets each line as it goes to the game log, without the tag. Tests use it in place of the game log.
	using Sink = std::function<void(spdlog::level::level_enum level, std::string_view line)>;

	/// The most lines that the log keeps for the report
	static constexpr size_t k_MostLines = 400;

	/// With no sink, the lines go to the "game" logger
	explicit ConnectionLog(Clock clock, Sink sink = {});

	/// Writes a line: the secrets in it are masked, the game log gets it with the tag, and the log keeps it with the time
	/// since the log started
	void Write(spdlog::level::level_enum level, std::string_view text);
	void Info(std::string_view text) { Write(spdlog::level::info, text); }
	void Warn(std::string_view text) { Write(spdlog::level::warn, text); }

	/// The kept lines, the oldest first. Each line starts with the seconds since the log started.
	[[nodiscard]] const std::deque<std::string>& Lines() const { return _lines; }
	/// The microseconds since the log started
	[[nodiscard]] int64_t Elapsed() const;

	/// The text of the "Copy diagnostics" report: the header lines, then the last kept lines up to the count
	[[nodiscard]] std::string Report(std::span<const std::string> header, size_t lines = k_MostLines) const;

private:
	Clock _clock;
	Sink _sink;
	int64_t _start {0};
	std::deque<std::string> _lines;
};

} // namespace openblack::online
