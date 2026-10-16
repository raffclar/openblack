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

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "CustomMusicPlaylist.h"

namespace openblack::audio
{

/// The custom music player: it plays the MP3 files of the custom music folder. It does what the game asked of an
/// external music player: a play list, play, pause, stop, the next and last track, the position in the track, the
/// volume, shuffle and repeat. It has no fast forward or rewind.
class CustomMusicPlayerInterface
{
public:
	enum class PlayState : uint8_t
	{
		Stopped,
		Playing,
		Paused,
	};

	virtual ~CustomMusicPlayerInterface() = default;

	/// Gives the player the files of the custom music folder. The MP3 files become the play list.
	virtual void SetFiles(const std::vector<std::filesystem::path>& files) = 0;
	/// The player works when the folder has a track
	[[nodiscard]] virtual bool IsRunning() const = 0;
	[[nodiscard]] virtual PlayState GetPlayState() const = 0;
	/// The name of the song of the current track, empty when there is no track
	[[nodiscard]] virtual std::string GetSongName() const = 0;
	[[nodiscard]] virtual const std::vector<custom_music::Track>& GetTracks() const = 0;
	[[nodiscard]] virtual std::optional<size_t> GetCurrentTrack() const = 0;
	/// Makes a track current. A track that plays stops, and the new track plays.
	virtual void SetCurrentTrack(size_t track) = 0;

	virtual void Play() = 0;
	virtual void Pause() = 0;
	virtual void Stop() = 0;
	virtual void NextTrack() = 0;
	virtual void PreviousTrack() = 0;

	/// The time played of the current track and its length, in milliseconds. The length is 0 until it is known.
	[[nodiscard]] virtual uint32_t GetPosition() const = 0;
	[[nodiscard]] virtual uint32_t GetLength() const = 0;
	virtual void SetPosition(uint32_t milliseconds) = 0;

	/// The volume, 0 to 255
	virtual void SetVolume(uint32_t volume) = 0;
	[[nodiscard]] virtual uint32_t GetVolume() const = 0;
	virtual void SetShuffle(bool shuffle) = 0;
	virtual void SetRepeat(bool repeat) = 0;
	[[nodiscard]] virtual bool GetShuffle() const = 0;
	[[nodiscard]] virtual bool GetRepeat() const = 0;

	/// Keeps the music going; called every frame
	virtual void Update() = 0;
};

} // namespace openblack::audio
