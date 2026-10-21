/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerMagicTree.h"

#include <optional>
#include <vector>

#include <glm/gtx/vec_swizzle.hpp>

#include "3D/MapCoords.h"
#include "Common/GUtilsAngle.h"
#include "Common/GameRandom.h"
#include "ECS/Components/ForestMember.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/MagicForest.h"
#include "ECS/Components/MagicTreeWatch.h"
#include "ECS/Components/Transform.h"
#include "ECS/MagicTreeWatchRules.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Locator.h"
#include "ObjectMeasures.h"
#include "VillagerAnimate.h"
#include "VillagerHome.h"
#include "VillagerReactions.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace villager_magic_tree = openblack::ecs::villager_magic_tree;
namespace villager_reactions = openblack::ecs::systems::villager_reactions;
namespace watch = openblack::ecs::magic_tree_watch;

namespace
{
/// Game turns in a second of the game's clock
constexpr auto k_TurnsPerSecond = static_cast<uint32_t>(1000 / ecs::systems::TimeSystemInterface::k_TurnDuration.count());

ecs::Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

void SetTop(LivingAction& action, VillagerStates state)
{
	Locator::livingActionSystem::value().VillagerSetState(action, LivingAction::Index::Top, state, false);
}

uint32_t GameRand(uint32_t n)
{
	return Locator::gameRandom::has_value() ? Locator::gameRandom::value().GameRand(n) : 0;
}

/// The tree that the villager looks at now, while it is still there
std::optional<entt::entity> WatchedTree(entt::entity villager)
{
	const auto& registry = Entities();
	const auto* seen = registry.TryGet<const MagicTreeWatch>(villager);
	if (seen == nullptr || seen->tree == entt::null || !registry.Valid(seen->tree))
	{
		return std::nullopt;
	}
	return seen->tree;
}

/// The forest of a tree of a forest miracle. None for another tree, or for a tree in no forest.
std::optional<entt::entity> ForestOfMagicTree(entt::entity tree)
{
	const auto& registry = Entities();
	const auto* member = registry.TryGet<const ForestMember>(tree);
	if (!registry.AllOf<MagicTree>(tree) || member == nullptr || !Locator::forestSystem::has_value())
	{
		return std::nullopt;
	}
	return Locator::forestSystem::value().LandForestOf(member->forest);
}

/// The trees of the forest: its growing trees first, then its grown trees, each in the forest's own order
struct ForestTrees
{
	std::vector<entt::entity> growing;
	std::vector<entt::entity> grown;

	[[nodiscard]] uint32_t Count() const { return static_cast<uint32_t>(growing.size() + grown.size()); }
	[[nodiscard]] entt::entity At(uint32_t tree) const
	{
		const auto slot = watch::SlotOf(tree, static_cast<uint32_t>(growing.size()));
		const auto& list = slot.growing ? growing : grown;
		return slot.index < list.size() ? list.at(slot.index) : entt::null;
	}
};

ForestTrees TreesOf(entt::entity forest)
{
	const auto& forests = Locator::forestSystem::value();
	return {.growing = forests.TreesOf(forest, true), .grown = forests.TreesOf(forest, false)};
}

/// The point where a villager stands to come to a thing: from the thing towards the villager, as far as the two radii
/// on the ground together
glm::vec2 PointBeside(entt::entity thing, entt::entity villager)
{
	const auto& registry = Entities();
	const auto from = map_coords::FromMetres(glm::xz(registry.Get<const Transform>(thing).position));
	const auto to = map_coords::FromMetres(glm::xz(registry.Get<const Transform>(villager).position));
	const float angle = gutils::Get3DAngleFromXZ(from, to);
	const float villagerRadius = ecs::systems::object_measures::TwoDRadius(registry, villager);
	const float reach = ecs::systems::object_measures::TwoDRadius(registry, thing) + villagerRadius;
	auto point = from;
	const auto offset = gutils::GetPosFromAngle(angle, reach);
	point.x += offset.x;
	point.z += offset.z;
	return map_coords::ToMetres(point);
}
} // namespace

void villager_magic_tree::Setup(LivingAction& action, entt::entity tree)
{
	auto& registry = Entities();
	registry.AssignOrReplace<MagicTreeWatch>(registry.ToEntity(action), MagicTreeWatch {.tree = tree});
	SetTop(action, VillagerStates::InitialiseBewilderedByMagicTreeReaction);
}

uint32_t villager_magic_tree::InitialiseBewilderedByMagicTreeReaction(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto tree = WatchedTree(villager);
	const auto forest = tree.has_value() ? ForestOfMagicTree(*tree) : std::nullopt;
	if (!forest.has_value())
	{
		return villager_reactions::StopReacting(action);
	}
	// It starts at a tree of the forest that it picks at random
	auto& seen = Entities().Get<MagicTreeWatch>(villager);
	seen.next = GameRand(watch::FirstTreeRollRange(TreesOf(*forest).Count()));
	seen.looked = 0;
	SetTop(action, VillagerStates::PerformBewilderedByMagicTreeReaction);
	return 1;
}

uint32_t villager_magic_tree::PerformBewilderedByMagicTreeReaction(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto tree = WatchedTree(villager);
	const auto forest = tree.has_value() ? ForestOfMagicTree(*tree) : std::nullopt;
	if (!forest.has_value())
	{
		return villager_reactions::StopReacting(action);
	}
	const auto trees = TreesOf(*forest);
	const auto count = trees.Count();
	auto& seen = Entities().Get<MagicTreeWatch>(villager);
	if (!watch::LooksAtAnother(seen.looked, count, GameRand(watch::SkippedRollRange(count))))
	{
		return villager_reactions::StopReacting(action);
	}
	// It looks at the next tree, but it walks to the tree that it looked at last, as in the game
	seen.tree = trees.At(seen.next);
	++seen.looked;
	seen.next = watch::NextTree(seen.next, count);
	villager_home::SetupMoveTo(action, PointBeside(*tree, villager), VillagerStates::TurnToFaceMagicTree);
	const auto roll = GameRand(watch::LookRollRange(k_TurnsPerSecond));
	action.turnsUntilStateChange = watch::LookCounter(watch::LookTurns(roll, k_TurnsPerSecond));
	return 1;
}

uint32_t villager_magic_tree::TurnToFaceMagicTree(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto tree = WatchedTree(villager);
	if (!tree.has_value())
	{
		return villager_reactions::StopReacting(action);
	}
	if (villager_reactions::LookAt(villager, Entities().Get<const Transform>(*tree).position))
	{
		SetTop(action, VillagerStates::LookAtMagicTree);
	}
	return 1;
}

uint32_t villager_magic_tree::LookAtMagicTree(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	if (!WatchedTree(villager).has_value())
	{
		return villager_reactions::StopReacting(action);
	}
	// Half way through the look, it stops looking up at the tree and looks about
	if (watch::ChangesLookNow(action.turnsUntilStateChange))
	{
		ecs::villager_animate::SetStateAnim(villager);
	}
	action.turnsUntilStateChange = watch::CountDown(action.turnsUntilStateChange);
	if ((action.turnsUntilStateChange & 0xFFu) == 0)
	{
		SetTop(action, VillagerStates::PerformBewilderedByMagicTreeReaction);
	}
	return 1;
}
