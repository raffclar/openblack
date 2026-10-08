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

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Audio/Audio.h"

// Sound tags: the public part is audio::tags in Audio.h; this header has what the rest of src/Audio
// calls (the turn, the map change, the channels' 3D function) and the older sound_tags names.
// See docs/bw1-notes/audio.md.

namespace openblack::audio::tags
{

/// Once a game turn, at the end of the turn: every tag, newest first (the original's list is pushed at its head)
void ProcessSoundTags();
/// Every tag stopped and forgotten (the map change deletes them with the map's objects; openblack also stops their
/// samples, which the audio reset does right after)
void Clear();
/// The tag's sound position for the channels: its thing's (nullopt: no thing, or the thing is gone in openblack, so
/// the channel keeps its point)
[[nodiscard]] std::optional<glm::vec3> TagSoundPoint(TagId tag);
/// The tag's own point, nullopt for an unknown tag
[[nodiscard]] std::optional<glm::vec3> Point(TagId tag);

} // namespace openblack::audio::tags

namespace openblack::audio::sound_tags
{

/// The water code's names (DesignedScenery's waterfall), kept until their caller moves to audio::tags. A
/// desc without a thing is a tag of a ScriptMarker at `point` (the waterfall tags its marker, a thing that never goes
/// away): it replays like a tag of a thing, not like a point tag.
using TagId = tags::TagId;
inline constexpr TagId k_NoTag = tags::k_NoTag;

struct TagDesc
{
	/// The sample (an InGame.sad SoundId)
	entt::id_type sample {0};
	/// The tag's thing. entt::null: a ScriptMarker at `point`
	entt::entity thing {entt::null};
	/// With a thing: its offset. Without: the marker's point ((x, altitude + y above the land, z); a marker made from a
	/// point keeps y - altitude, so it sounds at the point's y itself.)
	glm::vec3 point {0.0f};
	/// loops: -1 = for ever, else 0
	bool loop {true};
	/// tags::SetActive
	bool active {true};
};

/// tags::Create with track 0, mode 2, extra3DFlag 0, is3D 1, InGame, delay 0
TagId Create(const TagDesc& desc);
/// tags::SetActive
void SetActive(TagId tag, bool active);
/// tags::Delete
void Delete(TagId tag);

} // namespace openblack::audio::sound_tags
