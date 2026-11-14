/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ErrorReportsInterface.h"

/// The rules of the reports of a desync: a network game whose world parts from the world of the server. The rules have
/// no state of the program and no clock of their own, so that the tests can check them with no Sentry.
namespace openblack::error_reports
{

/// The most desync events of one game. After that, the desyncs of the game only count.
constexpr uint32_t k_MostDesyncEventsInGame = 5;
/// The shortest time between two desync events of one game, in milliseconds. A new kind of desync in this time waits.
constexpr int64_t k_DesyncEventGapMilliseconds = 60'000;
/// The most desync events of one run of the program, for all its games and the summaries together
constexpr uint32_t k_MostDesyncEventsInRun = 20;
/// The longest time, in milliseconds, that a desync waits for the names of the parts that differ. After that, its key
/// is k_UnknownDesyncPart.
constexpr int64_t k_DesyncPartsWaitMilliseconds = 15'000;
/// The longest time, in milliseconds, that the event of a desync waits for the detail of its capture, when the rule
/// waits for details. After that, the event goes with no detail.
constexpr int64_t k_DesyncDetailWaitMilliseconds = 75'000;
/// The most bytes of the file that an event of a desync attaches
constexpr size_t k_MostDesyncAttachmentBytes = 256 * 1024;
/// The key of a desync with no known part that differs
constexpr std::string_view k_UnknownDesyncPart = "unknown";
/// The most parts that an event names
constexpr size_t k_MostDesyncPartsInEvent = 8;
/// The most characters of the name of a part in an event
constexpr size_t k_MostDesyncPartLength = 64;

/// The facts of one desync, as the computer that compares the checksums finds them
struct DesyncFacts
{
	/// The first turn at which the two checksums differ
	uint32_t turn {0};
	/// The checksum of the world of this computer
	uint64_t ownChecksum {0};
	/// The checksum of the other world: the server's world on a client, the client's world on the server
	uint64_t otherChecksum {0};
	/// On the server: the god slot of the client. None on a client.
	std::optional<uint8_t> slot;
};

/// What the capture of a desync found, for its event
struct DesyncDetail
{
	/// The values of the event, such as the first fact that differs and the first entities that differ
	std::vector<std::pair<std::string, std::string>> extras;
	/// The name of the first fact that differs, or empty when no fact differs
	std::string fact;
	/// A file for the event, such as the facts of the capture, and its name. Empty: no file.
	std::string attachment;
	std::string attachmentName;
};

/// One event that goes to Sentry
struct DesyncReport
{
	enum class Kind : uint8_t
	{
		/// The first desync of a new key
		Desync,
		/// At the end of the game: the count of each key, when some desyncs did not go as their own event
		Summary,
	};
	Kind kind {Kind::Desync};
	/// The id of the game
	std::string game;
	/// The version of the format of the turns
	uint8_t turnCodec {0};
	/// A desync event: its key, its facts and the parts that differ, in the order of the checksum
	std::string key;
	DesyncFacts facts;
	std::vector<std::string> parts;
	/// The desyncs of the game so far, of all keys
	uint32_t desyncsInGame {0};
	/// A summary event: the count of each key in the order that the keys came, and the desyncs that did not go as their
	/// own event
	std::vector<std::pair<std::string, uint32_t>> counts;
	uint32_t notSent {0};
	/// A desync event: what its capture found, when it came in time
	std::optional<DesyncDetail> detail;
};

/// The events of desyncs that one run of the program can still send. It lives as long as the program runs.
class DesyncRunBudget
{
public:
	/// Takes one event. False when the run sent k_MostDesyncEventsInRun events.
	[[nodiscard]] bool Take() noexcept
	{
		if (_sent >= k_MostDesyncEventsInRun)
		{
			return false;
		}
		++_sent;
		return true;
	}
	[[nodiscard]] uint32_t Sent() const noexcept { return _sent; }

private:
	uint32_t _sent {0};
};

/// The key of a desync: the first part of the synced world that differs, in the order of the checksum. The checksum of
/// a turn is not in the key, because it changes at each turn of the same fault. Empty parts give k_UnknownDesyncPart.
[[nodiscard]] std::string DesyncKey(std::span<const std::string> parts);

/// Decides which desyncs of one game go to Sentry, so that a loop of desyncs does not fill Sentry:
/// - A desync waits for the names of the parts that differ, which give its key, up to k_DesyncPartsWaitMilliseconds.
/// - The first desync of a new key in the game gives one event. A desync of a known key only counts.
/// - An event goes only k_DesyncEventGapMilliseconds after the last event of the game. A new key in this time waits.
/// - The game sends k_MostDesyncEventsInGame events at most, and the run sends k_MostDesyncEventsInRun at most.
/// - At the end, the game sends one summary with the count of each key, only when some desyncs did not go.
class DesyncReportRule
{
public:
	DesyncReportRule(std::string game, uint8_t turnCodec);

	/// The checksums of a turn differ, and the world of this source was in step before. The source is the god slot on
	/// the server, and 0 on a client. A desync of the same source that still waits for its parts takes the key
	/// k_UnknownDesyncPart first.
	void Detected(uint8_t source, const DesyncFacts& facts, int64_t nowMilliseconds);
	/// The names of the parts that differ for the desync of the source that waits. Nothing when no desync waits.
	void Parted(uint8_t source, std::span<const std::string> parts, int64_t nowMilliseconds);
	/// The event of a new key waits up to k_DesyncDetailWaitMilliseconds for the detail of its capture. Off by default.
	void SetDetailWait(bool wait) noexcept { _detailWait = wait; }
	/// The detail of the capture of the last desync of the source. It goes with the event of that desync when the event
	/// did not go yet, else nothing happens.
	void Detailed(uint8_t source, DesyncDetail detail);
	/// Once a frame: the events that go now
	[[nodiscard]] std::vector<DesyncReport> Update(int64_t nowMilliseconds, DesyncRunBudget& budget);
	/// The game ended, or the program stops: the summary, once, when some desyncs did not go and the run can send it.
	/// The rule then sends nothing more.
	[[nodiscard]] std::optional<DesyncReport> Finish(DesyncRunBudget& budget);

	[[nodiscard]] const std::string& Game() const noexcept { return _game; }
	[[nodiscard]] uint32_t DesyncsInGame() const noexcept { return _desyncs; }
	[[nodiscard]] uint32_t EventsSent() const noexcept { return _eventsSent; }
	/// The count of the desyncs of a key, 0 for a key that did not come
	[[nodiscard]] uint32_t CountOf(std::string_view key) const;

private:
	struct Waiting
	{
		DesyncFacts facts;
		int64_t detectedAt {0};
		std::optional<DesyncDetail> detail;
	};
	struct Ready
	{
		uint8_t source {0};
		std::string key;
		DesyncFacts facts;
		std::vector<std::string> parts;
		uint32_t desyncsInGame {0};
		int64_t readyAt {0};
		std::optional<DesyncDetail> detail;
	};
	void Resolve(uint8_t source, std::span<const std::string> parts, int64_t nowMilliseconds);

	std::string _game;
	uint8_t _turnCodec {0};
	std::map<uint8_t, Waiting> _waiting;
	/// The count of each key, in the order that the keys came
	std::vector<std::pair<std::string, uint32_t>> _counts;
	/// The first desync of each new key that did not go yet
	std::vector<Ready> _ready;
	std::optional<int64_t> _lastEventAt;
	uint32_t _desyncs {0};
	uint32_t _eventsSent {0};
	bool _finished {false};
	bool _detailWait {false};
};

/// What a desync event tells Sentry, with no player name, account, address or chat
struct DesyncEventFields
{
	std::string message;
	/// "warning" for a desync, "info" for a summary
	std::string level;
	/// Sentry puts the events with the same fingerprint into one issue: one issue for each key, and one for the summaries
	std::vector<std::string> fingerprint;
	std::vector<std::pair<std::string, std::string>> tags;
	std::vector<std::pair<std::string, std::string>> extras;
};

/// The fields of the event of a report. The role tag comes from the service.
[[nodiscard]] DesyncEventFields DesyncEventFieldsOf(const DesyncReport& report);

/// The id of a game on a client: 16 hexadecimal digits of a random number
[[nodiscard]] std::string ClientGameId(uint64_t random);

} // namespace openblack::error_reports
