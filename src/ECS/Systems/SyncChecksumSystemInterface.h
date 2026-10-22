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

#include <optional>

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
	};

	virtual ~SyncChecksumSystemInterface() = default;

	/// At the end of each turn: when it is on, it makes the checksum of the turn
	virtual void ProcessTurn() = 0;
	virtual void SetEnabled(bool enabled) = 0;
	[[nodiscard]] virtual bool IsEnabled() const = 0;
	/// The checksum of the last turn that it made one for
	[[nodiscard]] virtual std::optional<TurnChecksum> GetLast() const = 0;
};

} // namespace openblack::ecs::systems
