/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FireFlyReward.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <array>

#include <spdlog/spdlog.h>

#include "Common/GameRandom.h"
#include "ECS/Components/Transform.h"
#include "ECS/FireFlies.h"
#include "ECS/Registry.h"
#include "ECS/Systems/WorshipStateInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/OneOffSpellSeed.h"
#include "Magic/MagicTables.h"

using namespace openblack;
using namespace openblack::worship;

namespace
{
constexpr size_t k_Magic = 42;

struct FireFlyRewardState
{
	std::array<float, k_Magic> probabilities {};
	std::array<float, k_Magic> sums {}; ///< Running sums; the last is the total
};

/// This module's state (Locator::worshipState)
FireFlyRewardState& Rewards()
{
	if (!Locator::worshipState::has_value())
	{
		std::fputs("worship::fire_fly: no worship state in the locator (Locator::worshipState)\n", stderr);
		std::abort();
	}
	return Locator::worshipState::value().Get<FireFlyRewardState>();
}
} // namespace

void fire_fly::SetRewardProbability(MagicType magic, float probability)
{
	const auto index = static_cast<size_t>(magic);
	if (static_cast<int>(magic) < 0 || index >= k_Magic)
	{
		return;
	}
	auto& rewards = Rewards();
	rewards.probabilities.at(index) = probability;
	float sum = 0.0f;
	for (size_t i = 0; i < k_Magic; ++i)
	{
		sum += rewards.probabilities.at(i);
		rewards.sums.at(i) = sum;
	}
}

void fire_fly::OnPlacedInMagicHand(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object) || !registry.AllOf<ecs::components::Transform>(object))
	{
		return;
	}
	const auto position = registry.Get<const ecs::components::Transform>(object).position;
	if (ecs::TakeFireFlyAt(position))
	{
		Reward(position);
	}
}

entt::entity fire_fly::Reward(const glm::vec3& position)
{
	// A game random float up to the sum
	const float r = game_random::GameFloatRand(Total());
	if (r == 0.0f)
	{
		return entt::null;
	}
	size_t magic = k_Magic;
	const auto& sums = Rewards().sums;
	for (size_t i = 0; i < k_Magic; ++i)
	{
		if (r <= sums.at(i))
		{
			magic = i;
			break;
		}
	}
	if (magic == k_Magic || magic == 0)
	{
		return entt::null;
	}
	const auto& tables = Locator::infoConstants::value();
	const auto type = static_cast<MagicType>(magic);
	// The magic's first seed, then its power-up level
	const auto seed = magic::GetFirstSpellSeedForMagicType(tables, type);
	if (static_cast<int>(seed) < 0)
	{
		return entt::null;
	}
	const auto& seedInfo = magic::GetSpellSeedInfo(tables, seed);
	if (seedInfo.exists == 0)
	{
		return entt::null;
	}
	const auto orb = magic::one_off::Create(position, seed, magic::GetPowerUpFromMagicType(seedInfo, type), 1.0f);
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Worship: a firefly leaves a one-shot {} at ({:.1f}, {:.1f})",
	                   seedInfo.debugString.data(), position.x, position.z);
	return orb;
}

void fire_fly::Reset()
{
	// Clears the probabilities only: the running sums (what Reward reads) keep the last land's until the next
	// FIRE_FLY_SPELL_REWARD_PROB (kept)
	Rewards().probabilities.fill(0.0f);
}

float fire_fly::Total()
{
	return Rewards().sums.back();
}
