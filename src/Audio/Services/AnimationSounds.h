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

#include <array>

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

namespace openblack::audio
{

/// The sounds of the animation clips (docs/bw1-notes/animation.md): footsteps, axe chops, screams...
/// Data\SmallSounds.SAS gives each clip its sound group and events {ms, soundId, action}. When the clip time crosses
/// an event the key {voice, 2, group, surface, soundId} goes to audio::PlayAnimationEffect with the
/// camera's distance to the animated thing: one sample of the editor.sad row on one of the 16 sample channels,
/// following its owner. Banter (0x92-0x94) comes from VillagersBanter.sad, 0x92 at the villager's house; action 1
/// stops the list's samples playing for the owner (the saw).
class AnimationSounds
{
public:
	/// Plays the events of the clip (an ANM_ index) with from <= time < to, for that villager or animal (what
	/// it reads of the thing: GameQueries::animatedThing; the clips' names: GameQueries::animationClipName).
	static void Fire(entt::entity entity, int32_t clip, int32_t from, int32_t to);
	/// PlayAnimationEffect with no animation behind it (trees rustling, a tree bent by the hand): one sample of the
	/// editor.sad row the key {voice, 2, group, surface, soundId} picks (-1 = only rows with a wildcard there), with
	/// the camera's distance to `position`, owned by `owner` (its point is the owner's). Nothing when the row has no
	/// samples or the camera is out of range.
	static void PlayFromTable(entt::entity owner, glm::vec3 position, const std::array<int32_t, 5>& key);
	/// Nothing (kept for its caller): the channels follow their owner once a turn, from the audio game turn, not
	/// every frame.
	static void Update();
	AnimationSounds() = delete;
};

} // namespace openblack::audio
