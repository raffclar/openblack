/*******************************************************************************
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
#include <optional>
#include <string>
#include <utility>
#include <vector>

#if defined(OPENBLACK_DEVELOPER_TOOLS)
#include <filesystem>
#include <string_view>

#include "Debug/RowDump.h"
#include "Simulation/WorldStateAtTurn.h"
#endif

namespace openblack::synced_state
{
struct Hash;
} // namespace openblack::synced_state

namespace openblack::ecs::systems
{

/// The checksum of the synced state of the world at the end of each turn. A network game sends it after each turn, so
/// that the server finds the turn at which a computer's world parts from the others. It is off in a single player game.
class SyncChecksumSystemInterface
{
public:
	/// The checksum of one turn
	struct TurnChecksum
	{
		uint32_t turn {0};
		uint64_t checksum {0};
		/// Tools: the hash of each part of the synced world (see synced_state::Hash), when the system keeps them
		std::vector<std::pair<std::string, uint64_t>> parts;
		/// Tools: at the turn of a dump, each field of each row of the dumped components as "component entity field
		/// bytes", the bytes in hexadecimal
		std::vector<std::string> dump;
		/// The components that have an entity but no registration, so the checksum leaves them out. A correct game has none.
		std::vector<std::string> unregistered;
	};

	virtual ~SyncChecksumSystemInterface() = default;

	/// At the end of each turn: when it is on, it makes the checksum of the turn
	virtual void ProcessTurn() = 0;
	virtual void SetEnabled(bool enabled) = 0;
	[[nodiscard]] virtual bool IsEnabled() const = 0;
	/// Tools: keep the hash of each part of the synced world in each checksum, to find the part that parts two computers.
	/// With byField, the parts have the hash of each field of each component too.
	virtual void SetKeepParts(bool keep, bool byField) = 0;
	/// Tools: at this turn, keep each field of each row of these components ("*" for all) in the checksum
	/// (TurnChecksum::dump)
	virtual void SetDump(uint32_t turn, std::vector<std::string> components) = 0;
	/// The checksum of the last turn that it made one for
	[[nodiscard]] virtual std::optional<TurnChecksum> GetLast() const = 0;
	/// Tools: the hash of the synced world that the system made at a turn, with or without the hash of each field. None
	/// when the last checksum was for another turn or of the other kind. A tool that records the same world at the same
	/// turn uses it and does not hash the world again.
	[[nodiscard]] virtual const synced_state::Hash* GetHashOfTurn(uint32_t turn, bool byField) const = 0;
	/// A function that gets the checksum of each turn as the system makes it, as the dedicated server logs it. An empty
	/// function removes it.
	virtual void SetTurnListener(std::function<void(const TurnChecksum&)> listener) = 0;
	[[nodiscard]] virtual std::function<void(const TurnChecksum&)> GetTurnListener() const = 0;
	/// A function that gets the hash of the synced world of each turn when the system finishes it. A tool that records
	/// the turns uses it when the hash of a turn waits for the draw. An empty function removes it.
	virtual void SetHashListener(std::function<void(uint32_t turn, const synced_state::Hash& hash)> listener) = 0;

	/// The hash of the last turn of a frame can wait for the draw of the frame. Then the threads of the system hash the
	/// world while the draw reads it, and the checksum is the same. Only a frame that changes no synced part of the world
	/// after its turns can let the hash wait. A checksum that a tool needs in its turn does not wait.
	virtual void SetHashBesideDraw(bool beside) = 0;
	/// The hash of this turn waits for the draw: it has no checksum yet
	[[nodiscard]] virtual bool HashWaits(uint32_t turn) const = 0;
	/// Just before the draw reads the world: the threads start the hash that waits, if there is one
	virtual void StartHashBesideDraw() = 0;
	/// After the draw, and before anything changes the world: the hash that waits ends and makes its checksum. A hash that
	/// did not start, the system makes at once. Without a hash that waits, nothing happens.
	virtual void FinishHash() = 0;
#if defined(OPENBLACK_DEVELOPER_TOOLS)
	/// Tools: keep the bytes of each row of the synced world of the last turns, and write them to this file at a turn of
	/// the settings and at a part (DumpPart). Returns false when the file does not open.
	virtual bool StartRowDump(const std::filesystem::path& path, row_dump::Settings settings) = 0;
	/// Tools: the world of this computer parts from the world of the source at this turn: write the kept rows of the turn
	/// to the file of the row dump, when it started
	virtual void DumpPart(uint32_t turn, std::string_view reason, uint32_t source) = 0;
	/// Tools: at the end of each turn of the settings, after its checksum, write the world state of this computer
	/// (world_state_at_turn). It makes the checksum of each turn, also in a single player game.
	virtual void WriteWorldStates(world_state_at_turn::Settings settings) = 0;
#endif
};

} // namespace openblack::ecs::systems
