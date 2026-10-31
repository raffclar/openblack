/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CustomMusicPlaylist.h"

#include <cctype>

#include <algorithm>

using namespace openblack::audio;
using namespace openblack::audio::custom_music;

namespace
{
[[nodiscard]] std::string Lower(std::string text)
{
	std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return text;
}
} // namespace

std::vector<Track> custom_music::ListTracks(std::span<const std::filesystem::path> files)
{
	std::vector<Track> tracks;
	for (const auto& file : files)
	{
		if (Lower(file.extension().string()) != ".mp3")
		{
			continue;
		}
		tracks.push_back({
		    .path = file,
		    .fileName = file.filename().string(),
		    .songName = file.stem().string(),
		});
	}
	std::ranges::sort(tracks, [](const Track& a, const Track& b) { return Lower(a.fileName) < Lower(b.fileName); });
	return tracks;
}

void Playlist::SetTracks(std::vector<Track> tracks)
{
	_tracks = std::move(tracks);
	_current = _tracks.empty() ? std::nullopt : std::optional<size_t>(0);
}

const Track* Playlist::GetCurrentTrack() const
{
	return _current.has_value() && *_current < _tracks.size() ? &_tracks.at(*_current) : nullptr;
}

void Playlist::SetCurrent(size_t track)
{
	if (track < _tracks.size())
	{
		_current = track;
	}
}

void Playlist::Next(const Random& random)
{
	if (_tracks.empty())
	{
		return;
	}
	if (_shuffle)
	{
		_current = random(static_cast<uint32_t>(_tracks.size()));
		return;
	}
	_current = (_current.value_or(_tracks.size() - 1) + 1) % _tracks.size();
}

void Playlist::Previous(const Random& random)
{
	if (_tracks.empty())
	{
		return;
	}
	if (_shuffle)
	{
		_current = random(static_cast<uint32_t>(_tracks.size()));
		return;
	}
	const auto current = _current.value_or(0);
	_current = current == 0 ? _tracks.size() - 1 : current - 1;
}

bool Playlist::AfterEnd(const Random& random)
{
	if (_tracks.empty() || !_current.has_value())
	{
		return false;
	}
	if (_shuffle)
	{
		_current = random(static_cast<uint32_t>(_tracks.size()));
		return true;
	}
	if (*_current + 1 < _tracks.size())
	{
		++*_current;
		return true;
	}
	_current = 0;
	return _repeat;
}
