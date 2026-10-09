/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

#include "ECS/TempleDestructionWorld.h"

namespace openblack::ecs::systems
{

/// A temple's destruction in the game: the game's entities, clock, players, audio, spot visuals, random draws and scripts
class GameTempleDestructionWorld final: public temple_world::World
{
public:
	[[nodiscard]] Registry* Entities() override;
	[[nodiscard]] uint32_t MillisecondsPerTurn() const override;
	[[nodiscard]] std::optional<PlayerNames> LocalPlayer() const override;
	entt::entity StartLoop(entt::id_type sound, glm::vec3 position, entt::entity owner) override;
	void PlayOnce(entt::id_type sound, glm::vec3 position, entt::entity owner) override;
	void StopSound(entt::entity emitter) override;
	std::optional<uint32_t> StartSpotVisual(SpotVisualType type, glm::vec3 position, int32_t turns,
	                                        PlayerNames player) override;
	void FollowWithSpotVisual(uint32_t visual, entt::entity target) override;
	[[nodiscard]] float RandomShare(float spread) override;
	void StartScript(std::string_view name) override;
	void Remove(entt::entity object) override;
};

} // namespace openblack::ecs::systems
