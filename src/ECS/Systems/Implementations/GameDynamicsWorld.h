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

#include "ECS/DynamicsWorld.h"

namespace openblack::ecs::systems
{

/// The physics' world in the game: the game's entities, land, models, map and the systems its bodies reach
class GameDynamicsWorld final: public dynamics::World
{
public:
	[[nodiscard]] Registry& Entities() override;
	[[nodiscard]] const Registry& Entities() const override;
	[[nodiscard]] const LandIslandInterface* Land() const override;
	[[nodiscard]] const InfoConstants* Info() const override;
	[[nodiscard]] const GObjectInfo* InfoOf(entt::entity object) const override;
	[[nodiscard]] float LifeOf(entt::entity object) const override;
	[[nodiscard]] float HeightOf(entt::entity object) const override;
	void Remove(entt::entity object) override;
	[[nodiscard]] std::optional<dynamics::ModelSize> SizeOfModel(entt::id_type mesh) const override;
	[[nodiscard]] std::optional<dynamics::Model> ModelOf(entt::id_type mesh) const override;
	[[nodiscard]] physics::Material MaterialOf(physics::MaterialRow row) const override;

	[[nodiscard]] bool HasMap() const override;
	[[nodiscard]] std::vector<entt::entity> FixedThenMobileInCell(glm::ivec2 cell) const override;
	[[nodiscard]] std::vector<entt::entity> AllInCell(glm::ivec2 cell) const override;
	void Refile(entt::entity object) override;

	[[nodiscard]] std::optional<glm::vec3> CameraOrigin() const override;
	[[nodiscard]] GameRandomInterface* Random() override;
	void PlaySound(audio::SoundId sound) override;
	std::optional<audio::AnimEffectPlay> PlayCollisionSound(std::span<const int32_t> keys, entt::entity owner,
	                                                        glm::vec3 position) override;
	void MoveSound(entt::entity emitter, glm::vec3 position) override;
	void AddWaterRing(const water_rings::Ring& ring) override;
	void ScareFish(glm::vec3 point) override;
	[[nodiscard]] int32_t SnowAt(glm::vec3 point) const override;

	void StartedMoving(entt::entity object) override;
	[[nodiscard]] bool IsOnFire(entt::entity object) const override;
	void CreateReaction(const ReactionSystemInterface::Source& source) override;
	void RemoveReactions(entt::entity initiator, Reaction type) override;
	void UntieLeashesTiedTo(entt::entity object) override;
	[[nodiscard]] bool IsComputerPlayer(PlayerNames player) const override;
	void FitDeadTreeObstacle(entt::entity deadTree) override;
};

} // namespace openblack::ecs::systems
