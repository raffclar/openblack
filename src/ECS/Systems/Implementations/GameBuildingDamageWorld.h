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

#include "ECS/BuildingDamageWorld.h"

namespace openblack::ecs::systems
{

/// Breaking buildings in the game: the game's entities, land, models, physics, snow, random draws, audio, magic and
/// creature minds
class GameBuildingDamageWorld final: public building_world::World
{
public:
	[[nodiscard]] Registry& Entities() override;
	[[nodiscard]] const Registry& Entities() const override;
	[[nodiscard]] float LandHeight(glm::vec2 point) const override;
	[[nodiscard]] std::vector<building_world::ModelPart> ModelParts(entt::id_type mesh) const override;
	[[nodiscard]] entt::id_type MakeModel(const std::string& name, entt::id_type source,
	                                      std::span<const building_world::DrawnPart> parts) override;
	void EraseModel(entt::id_type mesh) override;
	[[nodiscard]] DynamicsSystemInterface* Dynamics() override;
	[[nodiscard]] GameRandomInterface* Random() override;
	[[nodiscard]] int32_t SnowAt(glm::vec3 point) const override;
	[[nodiscard]] uint32_t DustTint(uint32_t argb, int32_t snow) const override;
	void PlaySound(std::span<const int32_t> keys, entt::entity building, glm::vec3 position) override;
	[[nodiscard]] float LifeOf(entt::entity object) const override;
	[[nodiscard]] const GObjectInfo* InfoOf(entt::entity object) const override;
	[[nodiscard]] std::optional<PlayerNames> PlayerOf(entt::entity object) const override;
	void Remove(entt::entity object) override;
	[[nodiscard]] std::optional<magic::EffectValues> Crush() const override;
	void ApplyEffect(entt::entity object, const magic::EffectValues& values, const magic::EffectSource& source) override;
	void PlayerDid(size_t deed, glm::vec3 point, entt::entity object, PlayerNames player) override;
};

} // namespace openblack::ecs::systems
