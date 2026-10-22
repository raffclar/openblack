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
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Audio/ScriptSoundEffect.h"
#include "ECS/Systems/SoundTagSystemInterface.h"

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

	/// The sounds at points that wait or play, the oldest first
	[[nodiscard]] std::span<const PointSound> PointSounds() const { return _points; }

private:
	/// Whether a tag's sound is heard if it starts now
	[[nodiscard]] bool Heard(entt::id_type sound) const;
	/// Starts a point's sound once, if it is switched on, heard and the camera is within the sound's reach. With no
	/// camera the caller already did the test of the reach. The tag owns its sound, so the voice rules never find the
	/// voice of another sound for it.
	void StartPointSound(PointSound& point, std::optional<glm::vec3> camera) const;
	/// One turn of each sound at a point: a delayed sound starts when it reaches the camera, and a sound goes when it
	/// no longer plays
	void ProcessPointSounds(const glm::vec3& camera);

	ConditionsSource _conditions;
	std::vector<PointSound> _points;
};

} // namespace openblack::ecs::systems
