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
#include <span>
#include <string_view>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "3D/AllMeshes.h"
#include "Creature/CreatureAudio.h"

namespace openblack
{
class L3DAnim;
namespace sas
{
struct ClipSounds;
}
namespace ecs
{
class Registry;
}
} // namespace openblack

/// The sounds placed on a living thing's clip, played as the clip it is drawn with plays on
namespace openblack::ecs::clip_sound_player
{

/// How a clip plays: how long it lasts, in milliseconds, and whether it plays over again
struct ClipTiming
{
	uint32_t duration {0};
	bool looping {false};
};

/// What playing a clip's sounds needs of the rest of the game: the entities, the sounds placed on the clips, the ground,
/// whether the player is inside the temple, the things' life and the sounds themselves. The game's own world works
/// through the game's resources and audio; tests give a fake.
class World
{
public:
	virtual ~World() = default;

	[[nodiscard]] virtual const Registry& Entities() const = 0;
	/// The sounds placed on a clip of the animation pack, none when it has none or there is no audio to play them
	[[nodiscard]] virtual const sas::ClipSounds* SoundsOf(AnimId clip) const = 0;
	/// What lies on the ground at a place, none where it can't be told
	[[nodiscard]] virtual std::optional<creature_audio::Ground> GroundAt(const glm::vec3& position) const = 0;
	/// Whether the player is inside the temple
	[[nodiscard]] virtual bool InsideTemple() const = 0;
	[[nodiscard]] virtual float LifeOf(entt::entity object) const = 0;
	/// A sound of a bank, as an animation's sound effect is played, belonging to an owner at a place; by the sample
	/// rules, only when the sample picked may be heard now
	virtual void PlaySound(std::string_view bank, std::span<const int32_t> keys, entt::entity owner, const glm::vec3& position,
	                       bool bySampleRules) = 0;
};

/// Plays the sounds a living thing's clip passes as it plays on from a place by so many milliseconds, from where the
/// thing is: a person's sounds by its size and only while it is alive, the banter from the villagers' bank (the first
/// from its home), a thrown person's scream only early in its flight, anything else from the editor's bank, the ones
/// played the ordinary way only as their samples allow. Nothing for a clip played once that has already finished.
void Play(World& world, entt::entity entity, AnimId clipId, ClipTiming clip, uint32_t place, uint32_t played,
          const glm::vec3& position);

/// The same in the game's own world
void Play(entt::entity entity, AnimId clipId, const L3DAnim& clip, uint32_t place, uint32_t played, const glm::vec3& position);

} // namespace openblack::ecs::clip_sound_player
