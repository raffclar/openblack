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

#include <vector>

#include <glm/vec3.hpp>

#include "Audio/Game/BankTables.h"
#include "Audio/GameQueries.h"

// The music attached to script objects (ATTACH_MUSIC): a list of ThingMusicInfo, head first. This is the list and its
// plain operations; what plays is in GameMusic.

namespace openblack::audio
{

struct ThingMusicInfo
{
	/// MUSIC_TYPE (ATTACH_MUSIC only reports a type outside 1..84 and attaches it anyway)
	int type {0};
	ThingId thing {0};
	int enabled {1};  ///< 1 when attached
	int finished {0}; ///< Nothing in the thing music code sets it to 1
	int started {0};
	int hasPlayPosition {0};       ///< SET_MUSIC_PLAY_POSITION
	glm::vec3 playPosition {0.0f}; ///< Map coordinates, as a world point (see SetPlayPosition)
};

class ThingMusicList
{
public:
	/// The first info that has that thing (nullptr if none)
	[[nodiscard]] ThingMusicInfo* Get(ThingId thing);
	/// A new info (enabled 1, the rest 0), put at the head of the list; the caller has checked that the thing has no
	/// info yet
	ThingMusicInfo& AddFront(int type, ThingId thing);
	/// Every node of that thing's info goes and the info is deleted (also used by the purges). Nothing is stopped.
	void Remove(ThingId thing);
	/// Every info released
	void Clear() { _infos.clear(); }

	/// MOVE_MUSIC: the info of from now belongs to to
	void Move(ThingId from, ThingId to);
	/// ENABLE_DISABLE_MUSIC: enabled = on, finished = started = 0
	void Enable(ThingId thing, int on);
	/// SET_MUSIC_PLAY_POSITION: hasPlayPosition = 1, the point stored as map coordinates. Those keep x and z as
	/// trunc(v * 6553.6) and the height above the land; the music player adds the altitude back and multiplies x and z by
	/// 1 / 6553.6 (exactly 10 / 65536), so the point comes back with x and z truncated to the 16.16 grid and the same y
	/// (map_coords::Quantise).
	void SetPlayPosition(ThingId thing, glm::vec3 point);
	/// finished, or 1 without an info
	[[nodiscard]] int IsFinished(ThingId thing);

	[[nodiscard]] std::vector<ThingMusicInfo>& GetInfos() { return _infos; }
	[[nodiscard]] const std::vector<ThingMusicInfo>& GetInfos() const { return _infos; }
	[[nodiscard]] size_t GetCount() const { return _infos.size(); }

private:
	/// In the order of the linked list (head first)
	std::vector<ThingMusicInfo> _infos;
};

} // namespace openblack::audio
