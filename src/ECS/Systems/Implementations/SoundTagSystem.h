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

#include <list>
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "ECS/Systems/SoundTagSystemInterface.h"
#include "Sound/ScriptSoundEffect.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::components
{
struct SoundTag;
}

namespace openblack::ecs::systems
{

/// A sound at a point. Only this computer hears it, and it lasts as long as this computer plays it. Thus it is not an
/// entity of the world: two computers with other cameras or other audio keep the same world.
struct PointSound
{
	entt::id_type sound;
	glm::vec3 position;
	/// The sound as this computer plays it, none when it does not play
	entt::entity emitter {entt::null};
	/// It waits until it has travelled to the camera at the speed of sound
	bool delayed {false};
	/// The game turns since it was made
	uint16_t turns {0};
};

class SoundTagSystem final: public SoundTagSystemInterface
{
public:
	/// Where it asks what the game is doing when a sound is about to start, which decides whether it is heard
	using ConditionsSource = audio::SoundEffectConditions (*)();

	SoundTagSystem();
	explicit SoundTagSystem(ConditionsSource conditions);

	void ProcessTurn(const glm::vec3& camera) override;
	void SetActive(entt::entity entity, bool active) override;
	void CreatePointSound(entt::id_type sound, const glm::vec3& position, bool delayed) override;
	entt::entity AttachObjectSound(entt::entity entity, entt::id_type sound, bool loops) override;
	void DetachObjectSound(entt::entity entity, entt::id_type sound) override;
	void ForgetSavedSounds() override;

	/// The sounds at points that wait or play, the newest first
	[[nodiscard]] std::vector<PointSound> PointSounds() const;
	/// The tags of objects and the sounds at points in the order of their turn, the newest first. A tag of an object is
	/// its entity; a sound at a point is entt::null.
	[[nodiscard]] std::vector<entt::entity> TurnOrder() const;

private:
	/// Whether a tag's sound is heard if it starts now
	[[nodiscard]] bool Heard(entt::id_type sound) const;
	/// Starts a point's sound once, if it is switched on, heard and the camera is within the sound's reach. With no
	/// camera the caller already did the test of the reach. The tag owns its sound, so the voice rules never find the
	/// voice of another sound for it.
	void StartPointSound(PointSound& point, std::optional<glm::vec3> camera) const;
	/// One turn of a sound at a point: a delayed sound starts when it reaches the camera. True when the sound goes,
	/// because it no longer plays.
	[[nodiscard]] bool ProcessPointSound(PointSound& point, const glm::vec3& camera) const;
	/// One turn of the tag of an object: its sound follows the object, or starts. True when the tag goes, because its
	/// object is gone.
	[[nodiscard]] bool ProcessObjectTag(entt::entity entity, components::SoundTag& tag, const glm::vec3& camera) const;
	/// Puts the tags that came into the world with no call to this system (the tags of new objects and of a loaded
	/// world) at the start of the list, and removes the tags that left it
	void FindTags();

	/// A tag in the list: the tag of an object, or a sound at a point
	struct Entry
	{
		entt::entity tag {entt::null};
		std::optional<PointSound> point;
	};

	ConditionsSource _conditions;
	/// The one list of the tags of objects and the sounds at points. A new tag goes at the start, and each turn does the
	/// tags from the start of the list, the newest first.
	std::list<Entry> _tags;
};

} // namespace openblack::ecs::systems
