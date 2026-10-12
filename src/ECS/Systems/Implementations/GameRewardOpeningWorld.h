/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "ECS/RewardOpening.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

/// A chest opening into the game: its contents go to the stores, the towns, the player's miracles and the land through the
/// game's systems
class GameRewardOpeningWorld final: public reward::OpeningWorld
{
public:
	explicit GameRewardOpeningWorld(entt::entity chest);

	void Pour(ResourceType type, uint32_t amount) override;
	[[nodiscard]] bool HasTown() const override;
	void GiveTownBelief(float belief) override;
	void MakeObject(MobileStaticInfo object) override;
	void MakeScaffold() override;
	void FindTownForMiracle() override;
	[[nodiscard]] MagicType NextMiracle() override;
	[[nodiscard]] MagicType RandomMiracle() override;
	[[nodiscard]] SpellSeedType FirstSeedOf(MagicType type) const override;
	[[nodiscard]] int LevelCount(SpellSeedType seed) const override;
	[[nodiscard]] MagicType MagicAt(SpellSeedType seed, int level) const override;
	[[nodiscard]] bool PlayerMay(MagicType type) const override;
	void PlayerLearns(MagicType type) override;
	bool TownLearns(MagicType type) override;
	void ShowTownSeed(SpellSeedType seed) override;
	void OneShotGlobe(SpellSeedType seed) override;
	void ShowGesture(MagicType type) override;

private:
	[[nodiscard]] PlayerNames Player() const;
	[[nodiscard]] entt::entity Town() const;
	[[nodiscard]] glm::vec3 Position() const;
	/// The player's reward progress list for their alignment, the coin tossed at exactly nothing
	[[nodiscard]] std::vector<reward::ProgressEntry> ProgressList() const;

	entt::entity _chest;
};

} // namespace openblack::ecs::systems
