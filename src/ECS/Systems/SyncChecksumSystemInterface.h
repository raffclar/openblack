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
	/// A function that gets the checksum of each turn as the system makes it, as the dedicated server logs it. An empty
	/// function removes it.
	virtual void SetTurnListener(std::function<void(const TurnChecksum&)> listener) = 0;
	[[nodiscard]] virtual std::function<void(const TurnChecksum&)> GetTurnListener() const = 0;
};

} // namespace openblack::ecs::systems
