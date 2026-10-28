/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <vector>

#include "Audio/MusicMood.h"
#include "Commands/PlayerCommands.h"

namespace openblack::ecs::systems
{

/// The music moods: the player plays MP3 files from the custom music folder in the profile folder. Each game turn the
/// system looks at the track that plays. When the track changes, the local player's creature reacts to what it felt
/// before about the new track, and the old track keeps what the creature feels now.
class MusicMoodSystemInterface
{
public:
	virtual ~MusicMoodSystemInterface() = default;

	/// Finds the custom music folder, makes it if it is missing, and gives its files to the custom music player
	virtual void LoadFolder() = 0;
	/// One game turn. A reset makes the system look at the music on this turn.
	virtual void UpdateOnGameTurn(bool reset) = 0;
	/// The feelings of the current track, none when no track record is current
	[[nodiscard]] virtual std::optional<music_mood::Emotion> GetCurrentEmotion() const = 0;
	/// Forgets all the track records, as a new game starts
	virtual void Clear() = 0;
	/// The track records of a creature mind file that was read. Only the local player's creature keeps them. Without
	/// records (a creature that is made), the creature is told about the current track again.
	virtual void ReadRecords(const std::optional<std::vector<music_mood::Record>>& records, bool own) = 0;
	/// The track records to write in the mind file of the local player's creature
	[[nodiscard]] virtual std::vector<music_mood::Record> WriteRecords() = 0;
	/// All the track records, the newest first
	[[nodiscard]] virtual std::vector<music_mood::Record> GetRecords() const = 0;
	/// The command of a change of the music, at its turn: when a track started, the creature reacts to the music
	virtual void DoMusicMood(const player_commands::MusicMood& command) = 0;
};

} // namespace openblack::ecs::systems
