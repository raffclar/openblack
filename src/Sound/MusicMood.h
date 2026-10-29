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

#include <functional>
#include <list>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

/// The music moods. The game keeps a record for each music track that the player plays from the custom music folder. A
/// record keeps what the creature felt each time the track stopped. When a track starts, the creature reacts to the
/// feelings that the record keeps. Pure, tested with made-up values.
namespace openblack::creaturemind
{
struct MindDatabaseEntry;
}

namespace openblack::music_mood
{

/// What the creature felt about a track: the mood (above 0 when it likes the track) and the fear
struct Emotion
{
	float mood {0.0f};
	float fear {0.0f};
};

/// The record of a track
struct Record
{
	/// The number made from the song name (see TrackId)
	uint32_t id {0};
	/// When the track last started, in seconds since the start of 1970
	int64_t time {0};
	/// The feelings, numbered from 1. A record in use has one.
	std::vector<Emotion> emotions;
};

/// What the controller tells the creature when the track changes
struct Packet
{
	/// The number of the current feelings in the record, 0 for none
	uint32_t index {0};
	/// How many feelings the record keeps, 0 when there is no record
	uint32_t count {0};
	/// The track, 0 when no track plays
	uint32_t id {0};
	Emotion emotion;
};

/// Seconds of a game turn
constexpr float k_TurnSeconds = 0.1f;
/// Seconds until the controller looks at the music again: when the music feature is off, while a track plays, and when
/// no track plays
constexpr float k_InactiveSeconds = 3600.0f;
constexpr float k_PlayingSeconds = 5.0f;
constexpr float k_SilentSeconds = 15.0f;
/// The countdown that a reset sets, so that the controller looks at the music on the next turn
constexpr float k_ResetCountdown = -0.1f;
/// The most records kept. A new track then takes the place of the record of the track that started longest ago.
constexpr size_t k_MostRecords = 256;
/// The colours that the music button blends to, as 0xAARRGGBB: green for a track that the creature likes, red for one it
/// does not like
constexpr uint32_t k_GoodMoodColour = 0xff00ff00;
constexpr uint32_t k_BadMoodColour = 0xffff0000;

/// The number of a track, made from its song name: each byte as a signed character, with a count from 1 that goes back
/// to 0 after 255. The lowest bit is always set, so a track never has the number 0.
[[nodiscard]] uint32_t TrackId(std::string_view songName);

/// Adds the creature's feelings now to the feelings of a record. Each part of the feelings now is first limited to -1
/// to 1.
[[nodiscard]] Emotion AddFeelings(Emotion record, Emotion now);

/// How the music button shows the mood of the current track: the colour it blends to and by how much, 0 to 255. There
/// is no blend when there is no current track record.
struct Tint
{
	uint32_t colour {k_GoodMoodColour};
	uint32_t amount {0};
};
[[nodiscard]] std::optional<Tint> ButtonTint(std::optional<Emotion> current);

/// The records as a creature mind file keeps them: only the feelings that are not 0, each with its number from 1
[[nodiscard]] std::vector<creaturemind::MindDatabaseEntry> ToMindFile(std::span<const Record> records);
/// The records that a creature mind file keeps. A feeling numbered outside its record is left out.
[[nodiscard]] std::vector<Record> FromMindFile(std::span<const creaturemind::MindDatabaseEntry> entries);

/// What the controller asks of the music player on a turn that it looks at the music: none when the music moods cannot
/// work (no player, or the feature is off), else the track that plays (see TrackId), 0 when no track plays
using Listen = std::function<std::optional<uint32_t>()>;

/// The music moods work only in a game alone. In a network game the controller hears no music player, so the current
/// track ends and no creature reacts to the music of one computer.
[[nodiscard]] Listen ForGame(bool networkGame, Listen player);

/// The music mood controller
class Controller
{
public:
	Controller() = default;
	Controller(const Controller&) = delete;
	Controller& operator=(const Controller&) = delete;
	Controller(Controller&&) noexcept = default;
	Controller& operator=(Controller&&) noexcept = default;
	~Controller() = default;

	/// One game turn. A reset makes the controller look at the music on this turn. The feelings are those of the local
	/// player's creature now, none when it has no creature: then no record gets feelings. Returns what the creature must
	/// be told when the track changed.
	[[nodiscard]] std::optional<Packet> Turn(float seconds, bool reset, const Listen& listen, std::optional<Emotion> feelings,
	                                         int64_t now);

	/// The feelings of the current track: none when no track record is current
	[[nodiscard]] std::optional<Emotion> Current() const;
	[[nodiscard]] const std::list<Record>& GetRecords() const noexcept { return _records; }
	[[nodiscard]] float GetCountdown() const noexcept { return _countdown; }

	/// Forgets all the records and the current track
	void Clear();

	/// Reads the records of a creature mind file. The current record first gets the feelings now. Only the local player's
	/// creature keeps the records it reads; they take the place of all the records. Returns what the creature must be
	/// told.
	[[nodiscard]] Packet ReadRecords(const std::optional<std::vector<Record>>& records, bool own,
	                                 std::optional<Emotion> feelings);
	/// The records to write in a creature mind file, the newest first. The current record first gets the feelings now.
	[[nodiscard]] std::vector<Record> WriteRecords(std::optional<Emotion> feelings);

private:
	/// The current record gets the feelings now
	void UpdateCurrent(std::optional<Emotion> feelings);
	/// No track is current
	[[nodiscard]] std::optional<Packet> SetNoTrack(std::optional<Emotion> feelings);
	/// A record for a new track
	Record& NewRecord();
	[[nodiscard]] Packet MakePacket() const;
	/// The current feelings to add to. Without a current record of that number, the spare feelings, which start at 0.
	[[nodiscard]] Emotion& CurrentEmotion();

	/// The newest record first
	std::list<Record> _records;
	Record* _current {nullptr};
	/// The feelings used when the current record has none of the current number
	Emotion _spare;
	uint32_t _index {0};
	uint32_t _id {0};
	float _countdown {0.0f};
};

} // namespace openblack::music_mood
