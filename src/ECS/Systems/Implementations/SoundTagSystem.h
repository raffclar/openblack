/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

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

class SoundTagSystem final: public SoundTagSystemInterface
{
public:
	/// Where it asks what the game is doing when a sound is about to start, which decides whether it is heard
	using ConditionsSource = audio::SoundEffectConditions (*)();

	SoundTagSystem();
	explicit SoundTagSystem(ConditionsSource conditions);

	void ProcessTurn(const glm::vec3& camera) override;
	void SetActive(entt::entity entity, bool active) override;
	entt::entity CreatePointSound(entt::id_type sound, const glm::vec3& position, bool delayed) override;

private:
	/// Whether a tag's sound is heard if it starts now
	[[nodiscard]] bool Heard(entt::id_type sound) const;
	/// Starts a point's sound once, if it is switched on, heard and the camera is within the sound's reach
	void StartPointSound(components::SoundTag& tag, const glm::vec3& position, const glm::vec3& camera) const;

	ConditionsSource _conditions;
};

} // namespace openblack::ecs::systems
