/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "GameRewardOpeningWorld.h"

#include <algorithm>
#include <iterator>
#include <optional>
#include <vector>

#include <spdlog/spdlog.h>

#include "Common/GameRandom.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/PlayerMiracles.h"
#include "ECS/PlayerTowns.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/EffectRequestSystemInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/Systems/SpellIconSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
ecs::components::Player* PlayerNamed(PlayerNames name)
{
	ecs::components::Player* found = nullptr;
	Locator::entitiesRegistry::value().Each<ecs::components::Player>([&](ecs::components::Player& player) {
		if (found == nullptr && player.name == name)
		{
			found = &player;
		}
	});
	return found;
}

PlayerNames LocalPlayer()
{
	return Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
}

bool GameCoinIsHeads()
{
	return Locator::gameRandom::has_value() && Locator::gameRandom::value().GameRand(2) != 0;
}
} // namespace

GameRewardOpeningWorld::GameRewardOpeningWorld(entt::entity chest)
    : _chest(chest)
{
}

PlayerNames GameRewardOpeningWorld::Player() const
{
	const auto* chest = Locator::entitiesRegistry::value().TryGet<const Reward>(_chest);
	return chest != nullptr && chest->player.has_value() ? *chest->player : LocalPlayer();
}

entt::entity GameRewardOpeningWorld::Town() const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* chest = registry.TryGet<const Reward>(_chest);
	return chest != nullptr && chest->town != entt::null && registry.Valid(chest->town) &&
	               registry.AllOf<ecs::components::Town>(chest->town)
	           ? chest->town
	           : entt::null;
}

glm::vec3 GameRewardOpeningWorld::Position() const
{
	const auto* transform = Locator::entitiesRegistry::value().TryGet<const Transform>(_chest);
	return transform != nullptr ? transform->position : glm::vec3(0.0f);
}

void GameRewardOpeningWorld::Pour(ResourceType type, uint32_t amount, PlayerNames player)
{
	if (Locator::resourceStoreSystem::has_value())
	{
		Locator::resourceStoreSystem::value().PourAt(type, Position(), amount, false, player, false);
	}
}

bool GameRewardOpeningWorld::HasTown() const
{
	return Town() != entt::null;
}

void GameRewardOpeningWorld::GiveTownBelief(float belief)
{
	// The sparkle of a magic object made, on the chest for its time
	if (Locator::effectRequestSystem::has_value())
	{
		Locator::effectRequestSystem::value().Show(
		    {.type = SpotVisualType::MagicObjectCreated, .position = Position(), .turns = reward::k_BeliefSparkleTurns});
	}
	// Given with no thing to rise from, so no belief symbol shows
	if (Locator::reactionSystem::has_value())
	{
		Locator::reactionSystem::value().GiveTownBelief(Town(), Player(), belief, entt::null, GuidanceAlignment::Good);
	}
}

void GameRewardOpeningWorld::MakeObject(MobileStaticInfo object)
{
	// Where the chest stands, unturned and at its own size
	ecs::archetypes::MobileStaticArchetype::Create(Position(), object, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
}

void GameRewardOpeningWorld::MakeScaffold()
{
	// TODO(reward-chest): a scaffold for the town's workshop, marked as a full one, once openblack has scaffolds
	SPDLOG_LOGGER_WARN(spdlog::get("game"), "A reward chest's scaffold isn't made: openblack has no scaffolds yet");
}

void GameRewardOpeningWorld::FindTownForMiracle()
{
	if (!Locator::spellIconSystem::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& icons = Locator::spellIconSystem::value();
	const auto towns = player_towns::InOrder(registry, Player());
	std::vector<std::optional<size_t>> shown;
	shown.reserve(towns.size());
	std::ranges::transform(towns, std::back_inserter(shown),
	                       [&icons](entt::entity town) { return icons.CentreMiraclesShown(town); });
	if (const auto first = reward::FirstTownForMiracle(shown))
	{
		registry.Get<Reward>(_chest).town = towns.at(*first);
	}
}

std::vector<reward::ProgressEntry> GameRewardOpeningWorld::ProgressList() const
{
	std::vector<reward::ProgressEntry> list;
	if (!Locator::infoConstants::has_value())
	{
		return list;
	}
	const auto& info = Locator::infoConstants::value();
	const float alignment =
	    Locator::alignmentSystem::has_value() ? Locator::alignmentSystem::value().GetPlayerAlignment(Player()) : 0.0f;
	const auto add = [&list](const auto& entries) {
		for (const auto& entry : entries)
		{
			list.push_back({.magic = entry.magicType, .onLand = entry.onLand});
		}
	};
	if (reward::TakesGoodList(alignment, GameCoinIsHeads))
	{
		add(info.rewardProgressGood);
	}
	else
	{
		add(info.rewardProgressEvil);
	}
	return list;
}

MagicType GameRewardOpeningWorld::NextMiracle()
{
	const auto land = Locator::entitiesRegistry::value().Context().mapScriptGlobals.landNumber;
	return reward::NextMiracle(ProgressList(), land, [this](MagicType type) { return PlayerMay(type); });
}

MagicType GameRewardOpeningWorld::RandomMiracle()
{
	const auto land = Locator::entitiesRegistry::value().Context().mapScriptGlobals.landNumber;
	return reward::RandomMiracle(ProgressList(), land, [](uint32_t n) {
		return Locator::gameRandom::has_value() ? Locator::gameRandom::value().GameRand(n) : 0u;
	});
}

SpellSeedType GameRewardOpeningWorld::FirstSeedOf(MagicType type) const
{
	if (!Locator::infoConstants::has_value())
	{
		return SpellSeedType::None;
	}
	return magic::FindFirstSpellSeedForMagicType(Locator::infoConstants::value(), type).value_or(SpellSeedType::None);
}

int GameRewardOpeningWorld::LevelCount(SpellSeedType seed) const
{
	if (!Locator::infoConstants::has_value() || seed == SpellSeedType::None)
	{
		return 0;
	}
	return magic::GetNumPowerUpLevels(magic::GetSpellSeedInfo(Locator::infoConstants::value(), seed));
}

MagicType GameRewardOpeningWorld::MagicAt(SpellSeedType seed, int level) const
{
	if (!Locator::infoConstants::has_value() || seed == SpellSeedType::None)
	{
		return MagicType::None;
	}
	const auto& row = magic::GetSpellSeedInfo(Locator::infoConstants::value(), seed);
	return reward::MagicAtLevel(row.magicTypes, row.powerUpGestures, level);
}

bool GameRewardOpeningWorld::PlayerMay(MagicType type) const
{
	const auto* player = PlayerNamed(Player());
	return player != nullptr && player_miracles::IsMagicTypeEnabled(player->miracles, type);
}

void GameRewardOpeningWorld::PlayerLearns(MagicType type)
{
	if (auto* player = PlayerNamed(Player()))
	{
		player_miracles::SetMagicTypeEnabled(player->miracles, type, true);
	}
}

bool GameRewardOpeningWorld::TownLearns(MagicType type)
{
	return Locator::spellIconSystem::has_value() && Locator::spellIconSystem::value().TownHolds(Town(), type);
}

void GameRewardOpeningWorld::ShowTownSeed(SpellSeedType seed)
{
	if (seed == SpellSeedType::None)
	{
		return;
	}
	// It goes with the chest. Only the model of the seed shows. The game makes the effect of the seed but never steps it,
	// so the effect has no particles and shows nothing. Thus no effect starts here.
	Locator::entitiesRegistry::value().AssignOrReplace<RewardSeed>(
	    _chest, RewardSeed {.seed = seed, .middle = Position() + glm::vec3(0.0f, reward::k_TownSeedAbove, 0.0f)});
}

void GameRewardOpeningWorld::OneShotGlobe(SpellSeedType seed)
{
	if (seed != SpellSeedType::None && Locator::magicSystem::has_value())
	{
		Locator::magicSystem::value().CreateOneOffSeed(Position() + glm::vec3(0.0f, reward::k_GlobeAbove, 0.0f), seed,
		                                               magic::k_BasePowerUpLevel, 1.0f);
	}
}

void GameRewardOpeningWorld::ShowGesture(MagicType type)
{
	if (type == MagicType::None || !Locator::infoConstants::has_value() || !Locator::gestureSystem::has_value())
	{
		return;
	}
	const auto gesture = magic::GetMagicInfo(Locator::infoConstants::value(), type).gestureType;
	if (gesture != GestureType::None)
	{
		Locator::gestureSystem::value().ShowGestureOnLand(gesture, Position(), reward::k_GestureSize);
	}
}
