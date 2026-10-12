/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/ForestSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class ForestSystem final: public ForestSystemInterface
{
public:
	uint32_t Plant(entt::entity spell, components::Spell& miracle, float tribalPower) override;
	[[nodiscard]] bool CanGrowAt(glm::vec3 point) const override;
	[[nodiscard]] bool HasTrees(entt::entity spell) const override;
	[[nodiscard]] std::optional<float> TallestTreeOf(entt::entity spell) const override;
	std::optional<entt::entity> AddTreeNear(entt::entity tree) override;
	uint32_t MakeLandForest(std::optional<uint32_t> id, glm::vec3 position, entt::entity bigForest, bool scenic) override;
	[[nodiscard]] std::optional<entt::entity> LandForestOf(uint32_t id) const override;
	[[nodiscard]] std::vector<entt::entity> LandForests() const override;
	[[nodiscard]] std::vector<entt::entity> TreesOf(entt::entity forest, bool growing) const override;
	[[nodiscard]] float WoodOf(entt::entity forest) const override;
	[[nodiscard]] glm::vec3 NearestPointOf(entt::entity forest, glm::vec3 to) const override;
	uint32_t TakeBigForestWood(entt::entity bigForest, uint32_t amount) override;
	std::optional<entt::entity> TreeFromBigForest(entt::entity bigForest) override;
	void AssignForestsToTowns() override;
	[[nodiscard]] std::optional<entt::entity> NearestTownForest(entt::entity town, glm::vec3 point) const override;
	[[nodiscard]] std::optional<entt::entity> NearestForestWithTrees(glm::vec3 point, float reach) const override;
	bool RemoveEmptyForestNear(glm::vec3 point, float reach) override;
	[[nodiscard]] std::optional<entt::entity> CentreTreeOf(entt::entity forest) const override;
	[[nodiscard]] std::optional<glm::vec3> ForestLair(AnimalInfo kind, glm::vec3 from) const override;
	void MakeScenicForests() override;
	void JoinForest(entt::entity tree, uint32_t forest) override;
	void GrowForests() override;
	void ProcessTurn() override;
	void Reset() override;

private:
	/// The forest miracles' forests' turn: growing while their miracles last, withering once they have gone
	void ProcessForests();
	/// A young tree of a tree's kind is planted at the first free spot of up to 160 tried round it, in its forest; none
	/// when no spot is free
	std::optional<entt::entity> PlantBeside(entt::entity tree);
	/// A shrunk big forest plants a young pine in its forest, at the first of ten random spots round its edge that is on
	/// land and clear of what stands there; none when no spot is
	std::optional<entt::entity> PlantPineAround(entt::entity bigForest);
	/// The number of a big forest's own forest, 0 when it has none
	[[nodiscard]] uint32_t ForestNumberOf(entt::entity bigForest) const;

	/// The turn a forest last gained a tree planted near another, 0 on a new land
	uint32_t _lastTreeAddedTurn {0};
	/// The number the next forest of the land takes when none is given
	uint32_t _nextLandForestId {1};
	/// How many forests have been made, the forest miracles' included: the newest is met first
	uint32_t _forestsMade {0};
	/// How many times a tree has joined a forest's growing or grown trees
	uint32_t _treesListed {0};
};

} // namespace openblack::ecs::systems
