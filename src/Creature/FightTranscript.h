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

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

/// The transcript of creature fights, for tools that check the fight rules. Each fight keeps a list of records in the
/// order that they occur. A record has a kind, the frame, the game turn, the game time and named fields. A record can
/// refer to an earlier record of the same fight as its cause. The names of the kinds and the fields are stable for one
/// version of the transcript.
namespace openblack::fight_transcript
{

/// Change this number when a kind or a field changes its name or its meaning
inline constexpr uint32_t k_Version = 1;

using Value = std::variant<bool, int64_t, double, std::string>;

/// A named value of a record. Each constructor keeps a text as a text and a number as a number.
struct Field
{
	Field(std::string_view fieldName, bool flag);
	Field(std::string_view fieldName, int32_t number);
	Field(std::string_view fieldName, uint32_t number);
	Field(std::string_view fieldName, int64_t number);
	Field(std::string_view fieldName, uint64_t number);
	Field(std::string_view fieldName, float number);
	Field(std::string_view fieldName, double number);
	Field(std::string_view fieldName, std::string_view text);
	Field(std::string_view fieldName, const char* text);
	Field(std::string_view fieldName, std::string text);

	std::string name;
	Value value;

	bool operator==(const Field& other) const = default;
};
using Fields = std::vector<Field>;

/// When a record occurs
struct Stamp
{
	uint64_t frame {0};
	uint32_t turn {0};
	/// The game time in milliseconds since the transcript started
	double gameMs {0.0};

	bool operator==(const Stamp& other) const = default;
};

struct Entry
{
	/// The place of the record in its fight, from 0
	uint64_t index {0};
	Stamp stamp;
	std::string kind;
	/// The earlier record of the same fight that causes this record, if there is one
	std::optional<uint64_t> cause;
	Fields fields;
};

struct Fight
{
	uint32_t id {0};
	/// The creature that starts the fight and its opponent, by their entity numbers
	uint32_t first {0};
	uint32_t second {0};
	Stamp started;
	std::optional<Stamp> ended;
	std::vector<Entry> entries;
};

/// Keeps the fights and their records. A creature belongs to the last fight that it started or joined, so the records
/// after a fight (a forced plan, a creature that goes home) go to that fight.
class Recorder
{
public:
	/// Starts a fight with a "fight_start" record, and gives the number of the fight
	uint32_t Begin(uint32_t first, uint32_t second, const Stamp& stamp, Fields fields);
	/// Adds a record to the last fight of a creature. The index of the record, or none when the creature has no fight.
	std::optional<uint64_t> Add(uint32_t creature, std::string_view kind, const Stamp& stamp, Fields fields,
	                            std::optional<uint64_t> cause = std::nullopt);
	/// Adds a record to a fight by its number. The index of the record, or none when there is no such fight.
	std::optional<uint64_t> AddTo(uint32_t fight, std::string_view kind, const Stamp& stamp, Fields fields,
	                              std::optional<uint64_t> cause = std::nullopt);
	/// Marks a fight as ended. Records added after this time still go to it.
	void End(uint32_t fight, const Stamp& stamp);

	/// The number of the last fight of a creature
	[[nodiscard]] std::optional<uint32_t> FightOf(uint32_t creature) const;
	/// The index of the last record of a kind for a creature, in its last fight
	[[nodiscard]] std::optional<uint64_t> LastOf(uint32_t creature, std::string_view kind) const;
	[[nodiscard]] std::span<const Fight> Fights() const { return _fights; }
	[[nodiscard]] const Fight* Find(uint32_t fight) const;
	/// Removes all fights. The numbers of new fights continue from the last number.
	void Clear();

private:
	Fight* FindMutable(uint32_t fight);

	std::vector<Fight> _fights;
	std::unordered_map<uint32_t, uint32_t> _lastFight;
	uint32_t _nextId {1};
};

/// One record as one line of JSON, with the version and the number of its fight
[[nodiscard]] std::string ToJsonLine(const Fight& fight, const Entry& entry);
/// A summary of a fight as one line of JSON: its number, the two creatures, when it started and ended, and the number
/// of records
[[nodiscard]] std::string SummaryJson(const Fight& fight);

} // namespace openblack::fight_transcript
