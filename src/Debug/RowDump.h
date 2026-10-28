/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#if !defined(OPENBLACK_DEVELOPER_TOOLS)
#error "The row dump is a developer tool: a build without the developer tools does not include it"
#endif

#include <cstddef>
#include <cstdint>

#include <deque>
#include <map>
#include <optional>
#include <ostream>
#include <span>
#include <streambuf>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ECS/Reflection/WorldArchive.h"

namespace openblack::row_dump
{

struct Settings
{
	/// The count of the last turns of which the dump keeps the rows. A part that the other computer finds later than
	/// this many turns finds no rows.
	uint32_t keptTurns {64};
	/// The dump writes the rows of this turn as the turn ends, also when no part occurs
	std::optional<uint32_t> atTurn;
	/// The most turns that the dump writes, so that a world that stays parted does not fill the disk
	uint32_t mostWrites {8};
};

/// Tools: an output that writes each line of the text to the standard output after a tag. The page of the web build
/// keeps the lines that have the tag, as it keeps the checksum lines, so a test script can read the rows of the web.
class TaggedLines: public std::streambuf
{
public:
	explicit TaggedLines(std::string_view tag)
	    : _tag(tag)
	{
	}

protected:
	int_type overflow(int_type character) override;
	int sync() override;

private:
	std::string _tag;
	std::string _line;
};

/// Adds a value that is not in a component to the rows of a turn, such as the count of the synced random numbers. Its
/// entity is the null entity.
void AddValue(ecs::reflection::WorldRows& rows, std::string_view name, std::span<const uint8_t> bytes);

/// Tools: keeps the bytes of each field of each row of the synced world of the last turns, and writes the rows of a turn
/// as text. A network client and the server each keep one. When the server finds that a client's world parts at a
/// turn, both write the rows of that turn, and tools/sim/row_dump.py finds the first field that differs.
///
/// The text has one line "turn T reason" for each written turn, then one line for each field of each row:
/// "component.field entity bytes", the bytes in hexadecimal. A field that the checksum leaves out (a part that only
/// shows the world on this computer) has the word "shown" after its bytes.
class RowDump
{
public:
	RowDump(std::ostream& out, Settings settings);

	/// At the end of each turn: keeps the rows of the turn, and writes them when the turn is the turn of the settings
	void Keep(uint32_t turn, ecs::reflection::WorldRows rows);
	/// Writes the kept rows of a turn at which the world parts from another computer's world. The source is the other
	/// computer, as the server's god slot. A part that continues from the source's part at the turn before writes nothing,
	/// and a turn writes one time only. Returns false when the dump wrote nothing.
	bool Part(uint32_t turn, std::string_view reason, uint32_t source = 0);

	[[nodiscard]] size_t Writes() const { return _written.size(); }

private:
	bool Write(uint32_t turn, const ecs::reflection::WorldRows& rows, std::string_view reason);

	std::ostream& _out;
	Settings _settings;
	std::deque<std::pair<uint32_t, ecs::reflection::WorldRows>> _kept;
	/// The last turn of a part, by the source of the part
	std::map<uint32_t, uint32_t> _lastPart;
	std::vector<uint32_t> _written;
};

} // namespace openblack::row_dump
