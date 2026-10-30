/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ServerSnapshots.h"

#include <charconv>

#include <algorithm>
#include <array>
#include <iterator>
#include <ranges>
#include <system_error>
#include <utility>

#include <fmt/format.h>

namespace openblack::server::snapshots
{
namespace
{

constexpr std::array<uint8_t, 8> k_SnapshotMagic {'O', 'B', 'S', 'N', 'A', 'P', 'S', 'H'};
constexpr std::array<uint8_t, 8> k_JournalMagic {'O', 'B', 'J', 'O', 'U', 'R', 'N', 'L'};
/// Version 2 keeps a salted hash of each secret of a session. The files of version 1 kept the secrets, and do not load.
constexpr uint32_t k_FormatVersion = 2;
constexpr std::string_view k_SnapshotPrefix = "snapshot-";
constexpr std::string_view k_SnapshotExtension = ".obsnap";
constexpr std::string_view k_JournalPrefix = "journal-";
constexpr std::string_view k_JournalExtension = ".objournal";
constexpr std::string_view k_TemporaryExtension = ".tmp";
/// The bytes of a record before its contents: the kind and the size
constexpr size_t k_RecordHead = 1 + 4;
constexpr size_t k_ChecksumBytes = 8;

/// A 64-bit FNV-1a hash of the bytes
[[nodiscard]] uint64_t Checksum(std::span<const uint8_t> bytes) noexcept
{
	uint64_t hash = 14695981039346656037ull;
	for (const auto byte : bytes)
	{
		hash = (hash ^ byte) * 1099511628211ull;
	}
	return hash;
}

void PutU8(std::vector<uint8_t>& out, uint8_t value)
{
	out.push_back(value);
}

void PutU32(std::vector<uint8_t>& out, uint32_t value)
{
	for (int shift = 0; shift < 32; shift += 8)
	{
		out.push_back(static_cast<uint8_t>(value >> shift));
	}
}

void PutU64(std::vector<uint8_t>& out, uint64_t value)
{
	for (int shift = 0; shift < 64; shift += 8)
	{
		out.push_back(static_cast<uint8_t>(value >> shift));
	}
}

void PutBytes(std::vector<uint8_t>& out, std::span<const uint8_t> bytes)
{
	PutU32(out, static_cast<uint32_t>(bytes.size()));
	out.insert(out.end(), bytes.begin(), bytes.end());
}

void PutText(std::vector<uint8_t>& out, std::string_view text)
{
	PutU32(out, static_cast<uint32_t>(text.size()));
	out.insert(out.end(), text.begin(), text.end());
}

void PutHash(std::vector<uint8_t>& out, const SessionHash& hash)
{
	out.insert(out.end(), hash.salt.begin(), hash.salt.end());
	out.insert(out.end(), hash.digest.begin(), hash.digest.end());
}

/// Reads values from bytes. After a read past the end, every read gives zero and Good gives false.
class Reader
{
public:
	explicit Reader(std::span<const uint8_t> bytes)
	    : _bytes(bytes)
	{
	}

	[[nodiscard]] bool Good() const { return _good; }
	[[nodiscard]] bool AtEnd() const { return _at == _bytes.size(); }
	[[nodiscard]] size_t At() const { return _at; }

	[[nodiscard]] std::span<const uint8_t> Take(size_t count)
	{
		if (!_good || _bytes.size() - _at < count)
		{
			_good = false;
			return {};
		}
		const auto taken = _bytes.subspan(_at, count);
		_at += count;
		return taken;
	}

	[[nodiscard]] uint8_t U8()
	{
		const auto bytes = Take(1);
		return bytes.empty() ? 0 : bytes[0];
	}

	[[nodiscard]] uint32_t U32()
	{
		uint32_t value = 0;
		const auto bytes = Take(4);
		for (size_t i = 0; i < bytes.size(); ++i)
		{
			value |= static_cast<uint32_t>(bytes[i]) << (8 * i);
		}
		return value;
	}

	[[nodiscard]] uint64_t U64()
	{
		uint64_t value = 0;
		const auto bytes = Take(8);
		for (size_t i = 0; i < bytes.size(); ++i)
		{
			value |= static_cast<uint64_t>(bytes[i]) << (8 * i);
		}
		return value;
	}

	[[nodiscard]] std::vector<uint8_t> Bytes()
	{
		const auto count = U32();
		const auto bytes = Take(count);
		return {bytes.begin(), bytes.end()};
	}

	[[nodiscard]] std::string Text()
	{
		const auto count = U32();
		const auto bytes = Take(count);
		return {bytes.begin(), bytes.end()};
	}

	[[nodiscard]] SessionHash Hash()
	{
		SessionHash hash;
		std::ranges::copy(Take(hash.salt.size()), hash.salt.begin());
		std::ranges::copy(Take(hash.digest.size()), hash.digest.begin());
		return hash;
	}

private:
	std::span<const uint8_t> _bytes;
	size_t _at {0};
	bool _good {true};
};

[[nodiscard]] std::optional<uint32_t> TurnOfName(std::string_view name, std::string_view prefix, std::string_view extension)
{
	if (!name.starts_with(prefix) || !name.ends_with(extension))
	{
		return std::nullopt;
	}
	const auto digits = name.substr(prefix.size(), name.size() - prefix.size() - extension.size());
	uint32_t turn = 0;
	const auto result = std::from_chars(digits.data(), digits.data() + digits.size(), turn);
	if (digits.empty() || result.ec != std::errc {} || result.ptr != digits.data() + digits.size())
	{
		return std::nullopt;
	}
	return turn;
}

[[nodiscard]] std::optional<std::vector<uint8_t>> ReadFile(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary);
	if (!file)
	{
		return std::nullopt;
	}
	std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	if (file.bad())
	{
		return std::nullopt;
	}
	return bytes;
}

/// The files of the folder whose names give a turn, by turn
template <typename TurnOf>
[[nodiscard]] std::map<uint32_t, std::filesystem::path> FilesByTurn(const std::filesystem::path& folder, TurnOf turnOf)
{
	std::map<uint32_t, std::filesystem::path> files;
	std::error_code error;
	for (const auto& entry : std::filesystem::directory_iterator(folder, error))
	{
		if (const auto turn = turnOf(entry.path().filename().string()); turn.has_value())
		{
			files.emplace(*turn, entry.path());
		}
	}
	return files;
}

/// Only the user of the server can read the files, because they keep the accounts of the players
void KeepPrivate(const std::filesystem::path& path)
{
	std::error_code error;
	std::filesystem::permissions(path, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
	                             std::filesystem::perm_options::replace, error);
}

} // namespace

std::vector<uint8_t> EncodeRoom(const Room& room)
{
	std::vector<uint8_t> out;
	PutText(out, room.start);
	PutU32(out, static_cast<uint32_t>(room.seats.size()));
	for (const auto& seat : room.seats)
	{
		PutU8(out, seat.state);
		PutU8(out, seat.session.has_value() ? 1 : 0);
		if (seat.session.has_value())
		{
			PutHash(out, *seat.session);
		}
		PutText(out, seat.account);
		PutText(out, seat.name);
	}
	PutU32(out, static_cast<uint32_t>(room.watchers.size()));
	for (const auto& watcher : room.watchers)
	{
		PutHash(out, watcher);
	}
	PutU8(out, room.stoppedAfter.has_value() ? 1 : 0);
	PutU32(out, room.stoppedAfter.value_or(0));
	PutBytes(out, room.leaving);
	PutBytes(out, room.joining);
	return out;
}

std::optional<Room> DecodeRoom(std::span<const uint8_t> bytes)
{
	Reader reader(bytes);
	Room room;
	room.start = reader.Text();
	const auto seats = reader.U32();
	for (uint32_t i = 0; i < seats && reader.Good(); ++i)
	{
		RoomSeat seat;
		seat.state = reader.U8();
		if (reader.U8() != 0)
		{
			seat.session = reader.Hash();
		}
		seat.account = reader.Text();
		seat.name = reader.Text();
		room.seats.push_back(std::move(seat));
	}
	const auto watchers = reader.U32();
	for (uint32_t i = 0; i < watchers && reader.Good(); ++i)
	{
		room.watchers.push_back(reader.Hash());
	}
	const bool stopped = reader.U8() != 0;
	const auto stoppedAfter = reader.U32();
	if (stopped)
	{
		room.stoppedAfter = stoppedAfter;
	}
	room.leaving = reader.Bytes();
	room.joining = reader.Bytes();
	if (!reader.Good() || !reader.AtEnd())
	{
		return std::nullopt;
	}
	return room;
}

bool SameGame(const game_socket::Start& saved, const game_socket::Start& now)
{
	return saved.land == now.land && saved.testbed == now.testbed && saved.seed == now.seed && saved.opening == now.opening &&
	       saved.godsRingMap == now.godsRingMap && saved.allComputerGods == now.allComputerGods && saved.aiSeed == now.aiSeed &&
	       saved.scenario == now.scenario;
}

std::vector<uint8_t> EncodeSnapshot(const Snapshot& snapshot)
{
	std::vector<uint8_t> out(k_SnapshotMagic.begin(), k_SnapshotMagic.end());
	PutU32(out, k_FormatVersion);
	PutU32(out, snapshot.turn);
	PutBytes(out, EncodeRoom(snapshot.room));
	out.reserve(out.size() + 4 + snapshot.world.size() + k_ChecksumBytes);
	PutBytes(out, snapshot.world);
	PutU64(out, Checksum(out));
	return out;
}

std::optional<Snapshot> DecodeSnapshot(std::span<const uint8_t> bytes)
{
	if (bytes.size() < k_SnapshotMagic.size() + k_ChecksumBytes)
	{
		return std::nullopt;
	}
	const auto body = bytes.first(bytes.size() - k_ChecksumBytes);
	Reader tail(bytes.last(k_ChecksumBytes));
	if (tail.U64() != Checksum(body))
	{
		return std::nullopt;
	}
	Reader reader(body);
	const auto magic = reader.Take(k_SnapshotMagic.size());
	if (!std::ranges::equal(magic, k_SnapshotMagic) || reader.U32() != k_FormatVersion)
	{
		return std::nullopt;
	}
	Snapshot snapshot;
	snapshot.turn = reader.U32();
	const auto room = DecodeRoom(reader.Bytes());
	snapshot.world = reader.Bytes();
	if (!reader.Good() || !reader.AtEnd() || !room.has_value())
	{
		return std::nullopt;
	}
	snapshot.room = *room;
	return snapshot;
}

std::vector<uint8_t> EncodeRecord(RecordKind kind, std::span<const uint8_t> bytes)
{
	std::vector<uint8_t> out;
	out.reserve(k_RecordHead + bytes.size() + k_ChecksumBytes);
	PutU8(out, static_cast<uint8_t>(kind));
	PutBytes(out, bytes);
	PutU64(out, Checksum(out));
	return out;
}

std::vector<uint8_t> TurnRecordBytes(uint32_t turn, std::span<const uint8_t> bytes)
{
	std::vector<uint8_t> out;
	out.reserve(4 + bytes.size());
	PutU32(out, turn);
	out.insert(out.end(), bytes.begin(), bytes.end());
	return out;
}

std::optional<std::pair<uint32_t, std::vector<uint8_t>>> ReadTurnRecord(std::span<const uint8_t> bytes)
{
	if (bytes.size() < 4)
	{
		return std::nullopt;
	}
	Reader reader(bytes);
	const auto turn = reader.U32();
	const auto rest = bytes.subspan(4);
	return std::pair {turn, std::vector<uint8_t>(rest.begin(), rest.end())};
}

std::vector<uint8_t> JournalHeader()
{
	std::vector<uint8_t> out(k_JournalMagic.begin(), k_JournalMagic.end());
	PutU32(out, k_FormatVersion);
	return out;
}

std::pair<std::vector<Record>, bool> ReadJournal(std::span<const uint8_t> bytes)
{
	std::vector<Record> records;
	Reader reader(bytes);
	const auto magic = reader.Take(k_JournalMagic.size());
	if (!std::ranges::equal(magic, k_JournalMagic) || reader.U32() != k_FormatVersion)
	{
		return {records, true};
	}
	while (!reader.AtEnd())
	{
		const auto start = reader.At();
		const auto kind = reader.U8();
		const auto contents = reader.Bytes();
		const auto end = reader.At();
		const auto checksum = reader.U64();
		if (!reader.Good() || checksum != Checksum(bytes.subspan(start, end - start)) ||
		    (kind != static_cast<uint8_t>(RecordKind::Turn) && kind != static_cast<uint8_t>(RecordKind::Room)))
		{
			return {records, true};
		}
		records.push_back({.kind = static_cast<RecordKind>(kind), .bytes = contents});
	}
	return {records, false};
}

std::string SnapshotFileName(uint32_t turn)
{
	return fmt::format("{}{:010}{}", k_SnapshotPrefix, turn, k_SnapshotExtension);
}

std::string JournalFileName(uint32_t turn)
{
	return fmt::format("{}{:010}{}", k_JournalPrefix, turn, k_JournalExtension);
}

std::optional<uint32_t> SnapshotTurnOf(std::string_view fileName)
{
	return TurnOfName(fileName, k_SnapshotPrefix, k_SnapshotExtension);
}

std::optional<uint32_t> JournalTurnOf(std::string_view fileName)
{
	return TurnOfName(fileName, k_JournalPrefix, k_JournalExtension);
}

Store::Store(std::filesystem::path folder, uint32_t keep)
    : _folder(std::move(folder))
    , _keep(std::max<uint32_t>(keep, 1))
{
}

Store::~Store() = default;

bool Store::Write(uint32_t turn, std::span<const uint8_t> bytes) const
{
	std::error_code error;
	std::filesystem::create_directories(_folder, error);
	const auto path = _folder / SnapshotFileName(turn);
	auto temporary = path;
	temporary += k_TemporaryExtension;
	{
		std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
		if (!file)
		{
			return false;
		}
		KeepPrivate(temporary);
		file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
		file.flush();
		if (!file)
		{
			file.close();
			std::filesystem::remove(temporary, error);
			return false;
		}
	}
	// Only a complete file gets the name of a snapshot
	std::filesystem::rename(temporary, path, error);
	if (error)
	{
		std::filesystem::remove(temporary, error);
		return false;
	}
	Rotate();
	return true;
}

void Store::Rotate() const
{
	std::error_code error;
	const auto snapshots = FilesByTurn(_folder, SnapshotTurnOf);
	if (snapshots.empty())
	{
		return;
	}
	auto kept = snapshots.rbegin();
	std::advance(kept, std::min<size_t>(_keep, snapshots.size()) - 1);
	const auto oldestKept = kept->first;
	for (const auto& [turn, path] : snapshots)
	{
		if (turn < oldestKept)
		{
			std::filesystem::remove(path, error);
		}
	}
	for (const auto& [turn, path] : FilesByTurn(_folder, JournalTurnOf))
	{
		if (turn < oldestKept)
		{
			std::filesystem::remove(path, error);
		}
	}
}

bool Store::OpenJournal(uint32_t turn, const Room& room)
{
	if (_journal.is_open())
	{
		_journal.close();
	}
	std::error_code error;
	std::filesystem::create_directories(_folder, error);
	const auto path = _folder / JournalFileName(turn);
	_journal.open(path, std::ios::binary | std::ios::trunc);
	if (!_journal)
	{
		_journal.close();
		return false;
	}
	KeepPrivate(path);
	const auto header = JournalHeader();
	_journal.write(reinterpret_cast<const char*>(header.data()), static_cast<std::streamsize>(header.size()));
	AddRoom(room);
	return static_cast<bool>(_journal);
}

void Store::AddRecord(RecordKind kind, std::span<const uint8_t> bytes)
{
	if (!_journal.is_open())
	{
		return;
	}
	const auto record = EncodeRecord(kind, bytes);
	_journal.write(reinterpret_cast<const char*>(record.data()), static_cast<std::streamsize>(record.size()));
	// The system gets each record at once, so that the record stays when the server stops with no warning
	_journal.flush();
}

void Store::AddTurn(uint32_t turn, std::span<const uint8_t> bytes)
{
	AddRecord(RecordKind::Turn, TurnRecordBytes(turn, bytes));
}

void Store::AddRoom(const Room& room)
{
	AddRecord(RecordKind::Room, EncodeRoom(room));
}

std::optional<Restored> Store::LoadNewest() const
{
	const auto snapshots = FilesByTurn(_folder, SnapshotTurnOf);
	const auto journals = FilesByTurn(_folder, JournalTurnOf);
	std::vector<std::string> skipped;
	for (const auto& [turn, path] : snapshots | std::views::reverse)
	{
		const auto bytes = ReadFile(path);
		auto snapshot = bytes.has_value() ? DecodeSnapshot(*bytes) : std::nullopt;
		if (!snapshot.has_value() || snapshot->turn != turn)
		{
			skipped.push_back(path.filename().string());
			continue;
		}
		Restored restored {.snapshot = std::move(*snapshot), .room = {}, .turns = {}, .skipped = skipped, .tornJournal = false};
		restored.room = restored.snapshot.room;
		// The turns after the snapshot follow one after the other through the journals. A gap or a torn record ends them.
		auto next = turn;
		bool ended = false;
		for (auto journal = journals.lower_bound(turn); journal != journals.end() && !ended; ++journal)
		{
			if (journal->first > next)
			{
				break;
			}
			const auto journalBytes = ReadFile(journal->second);
			if (!journalBytes.has_value())
			{
				restored.tornJournal = true;
				break;
			}
			const auto [records, torn] = ReadJournal(*journalBytes);
			for (const auto& record : records)
			{
				if (record.kind == RecordKind::Room)
				{
					if (auto room = DecodeRoom(record.bytes); room.has_value())
					{
						restored.room = std::move(*room);
					}
					continue;
				}
				auto turnRecord = ReadTurnRecord(record.bytes);
				if (!turnRecord.has_value() || turnRecord->first > next)
				{
					ended = true;
					break;
				}
				if (turnRecord->first == next)
				{
					restored.turns.emplace(next, std::move(turnRecord->second));
					++next;
				}
			}
			if (torn)
			{
				restored.tornJournal = true;
				ended = true;
			}
		}
		return restored;
	}
	return std::nullopt;
}

void Store::Clear()
{
	if (_journal.is_open())
	{
		_journal.close();
	}
	std::error_code error;
	for (const auto& entry : std::filesystem::directory_iterator(_folder, error))
	{
		const auto name = entry.path().filename().string();
		const bool temporary = name.starts_with(k_SnapshotPrefix) && name.ends_with(k_TemporaryExtension);
		if (SnapshotTurnOf(name).has_value() || JournalTurnOf(name).has_value() || temporary)
		{
			std::filesystem::remove(entry.path(), error);
		}
	}
}

} // namespace openblack::server::snapshots
