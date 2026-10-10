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

/// A temple's destruction in the game: the game's entities, clock, players, audio, spot visuals and beams, models, random draws
/// and scripts
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
	void SetSpotVisualMagnitude(uint32_t visual, float magnitude) override;
	[[nodiscard]] bool SpotVisualRunning(uint32_t visual) const override;
	void AddPlasma(uint32_t source, const particles::PlasmaCommand& command) override;
	[[nodiscard]] std::vector<model_surface::Triangle> DrawnTrianglesOf(entt::entity object) const override;
	[[nodiscard]] glm::mat4 PlacementOf(entt::entity object) const override;
	[[nodiscard]] GameRandomInterface* Random() override;
	[[nodiscard]] float RandomShare(float spread) override;
	void StartScript(std::string_view name) override;
	void Remove(entt::entity object) override;
};

} // namespace openblack::ecs::systems
