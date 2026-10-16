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
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

/// The list of tracks of the custom music player: the MP3 files of the custom music folder in the player's profile
/// folder. Pure, tested with made-up values.
namespace openblack::audio::custom_music
{

/// A track of the custom music folder
struct Track
{
	std::filesystem::path path;
	/// The file name with its extension, as the play list shows it
	std::string fileName;
	/// The file name without its extension, as the player names the song that plays
	std::string songName;
};

/// The MP3 files of the folder, in the order of their file names. Other files are not tracks.
[[nodiscard]] std::vector<Track> ListTracks(std::span<const std::filesystem::path> files);

/// Gives a whole number below its argument
using Random = std::function<uint32_t(uint32_t)>;

/// The play list: which track is current, and which track comes next
class Playlist
{
public:
	/// A new list of tracks. The first track becomes current, or none when there is no track.
	void SetTracks(std::vector<Track> tracks);
	[[nodiscard]] const std::vector<Track>& GetTracks() const noexcept { return _tracks; }
	[[nodiscard]] std::optional<size_t> GetCurrent() const noexcept { return _current; }
	[[nodiscard]] const Track* GetCurrentTrack() const;
	/// Makes the track current, when the list has it
	void SetCurrent(size_t track);

	void SetShuffle(bool shuffle) noexcept { _shuffle = shuffle; }
	void SetRepeat(bool repeat) noexcept { _repeat = repeat; }
	[[nodiscard]] bool GetShuffle() const noexcept { return _shuffle; }
	[[nodiscard]] bool GetRepeat() const noexcept { return _repeat; }

	/// The next track, as the player asks: a random track when the list is shuffled, else the track after the current
	/// one, the first after the last
	void Next(const Random& random);
	/// The track before the current one, the last before the first; a random track when the list is shuffled
	void Previous(const Random& random);
	/// The track to play when the current one has played to its end: a random track when the list is shuffled, else the
	/// track after it. After the last track the list starts again only when it repeats. Returns false when the music
	/// stops.
	[[nodiscard]] bool AfterEnd(const Random& random);

private:
	std::vector<Track> _tracks;
	std::optional<size_t> _current;
	bool _shuffle {false};
	bool _repeat {false};
};

} // namespace openblack::audio::custom_music
