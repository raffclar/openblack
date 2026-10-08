/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <limits>
#include <optional>
#include <vector>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "Common/GUtilsDistance.h"
#include "Debug/DebugEnv.h"
#include "ECS/AnimalAIDetail.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Trees.h"
#include "Locator.h"

/// The predators' lairs (tiger, wolf and lion). The forest list, centres and grown trees come from ECS/Trees.h (read
/// only).
namespace openblack::ecs::animal_ai::detail
{
namespace
{
using components::BigForest;
using components::Flock;
using components::Transform;
using components::Tree;

/// The distance of two positions in metres (each truncated to a MapCoords first): whole 16.16 units, through the
/// shared hypotenuse (Common/GUtilsDistance)
int32_t MapDistance(glm::vec2 a, glm::vec2 b)
{
	return gutils::GetDistance(map_coords::FromMetres(a), map_coords::FromMetres(b));
}

/// The forest's score: SigmoidThreshold(-0.9, (grown + growing) / 20) is computed and discarded, the score
/// is SigmoidThreshold(-0.9, -(d / scale))
float ForestScore(uint32_t forestId, float distance, float scale)
{
	[[maybe_unused]] const float discarded =
	    gutils::SigmoidThreshold(-0.9f, static_cast<float>(static_cast<uint32_t>(ForestTreeCount(forestId)) / 20u));
	return gutils::SigmoidThreshold(-0.9f, -(distance / scale));
}

/// The forest loop shared by the tiger and the wolf: the forest list newest first, the first forest taken unscored, a
/// later one replaces it only if it scores strictly more. The tiger also calls FindNearestDrinkingWater(me, 500) per
/// forest; it only depends on its own position and never changes the choice, so it is not ported. Returns the chosen forest's
/// first grown tree (the one nearest its centre), none when there is no forest or it has no grown tree.
std::optional<glm::vec3> BestForestLair(glm::vec2 me, uint32_t& chosen)
{
	std::optional<uint32_t> best;
	float bestScore = 0.0f;
	for (const auto id : ForestsNewestFirst())
	{
		if (!best)
		{
			best = id;
			continue;
		}
		const auto centre = ForestCentre(id);
		const float score = ForestScore(id, static_cast<float>(MapDistance(me, {centre.x, centre.z})), 1000.0f);
		if (bestScore < score)
		{
			bestScore = score;
			best = id;
		}
	}
	chosen = best.value_or(0);
	if (!best)
	{
		return std::nullopt;
	}
	const auto grown = GrownTreesByDistance(*best);
	if (grown.empty())
	{
		return std::nullopt;
	}
	return Locator::entitiesRegistry::value().Get<const Transform>(grown.front()).position;
}

/// The nearest entity by GetDistance over a list with head insertion (the newest first): a later one replaces only if
/// strictly nearer, so ties keep the newest (the object creation index stands in for the list order)
template <typename Component>
std::optional<glm::vec3> NearestNewestFirst(glm::vec2 from)
{
	auto& registry = Locator::entitiesRegistry::value();
	std::optional<glm::vec3> best;
	int32_t bestDistance = std::numeric_limits<int32_t>::max();
	int64_t bestIndex = -1;
	registry.Each<const Component, const Transform>([&](entt::entity entity, const Component&, const Transform& transform) {
		if (!ecs::IsAvailable(entity))
		{
			return;
		}
		const int32_t d = MapDistance(from, Xz(transform));
		const int64_t index = object_index::Of(entity);
		if (!best || d < bestDistance || (d == bestDistance && index > bestIndex))
		{
			bestDistance = d;
			bestIndex = index;
			best = transform.position;
		}
	});
	return best;
}

void TraceLair(const Context& ctx, const char* rule, uint32_t forestId, glm::vec3 lair)
{
	static const bool trace = debug_env::AnimalTrace();
	if (trace)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Animal {}: lair ({}, forest {}) at ({:.1f}, {:.1f}) from ({:.1f}, {:.1f})",
		                   static_cast<uint32_t>(ctx.entity), rule, forestId, lair.x, lair.z, ctx.transform.position.x,
		                   ctx.transform.position.z);
	}
}
} // namespace

void CalculateLairPosition(Context& ctx)
{
	auto* flock = FlockOf(ctx.animal);
	if (flock == nullptr || !IsLeader(ctx))
	{
		return;
	}
	const glm::vec2 me = Xz(ctx.transform);
	uint32_t forestId = 0;
	switch (HunterOf(ctx.animal.type))
	{
	case Hunter::Tiger:
	{
		// the tiger: the best forest's grown tree nearest its centre, else where it is
		const auto lair = BestForestLair(me, forestId);
		const auto at = lair.value_or(ctx.transform.position);
		TraceLair(ctx, lair ? "tiger forest" : "tiger self", forestId, at);
		SetDomainCentre(*flock, at);
		break;
	}
	case Hunter::Wolf:
	{
		// the wolf. Not ported: a flock flag set -> nothing (the original only ever zeroes it).
		// A leader in a script: where it is
		if (script_held::IsInScript(ctx.entity))
		{
			TraceLair(ctx, "wolf in script", 0, ctx.transform.position);
			SetDomainCentre(*flock, ctx.transform.position);
			break;
		}
		// The nearest big forest
		if (const auto big = NearestNewestFirst<BigForest>(me); big)
		{
			TraceLair(ctx, "wolf big forest", 0, *big);
			SetDomainCentre(*flock, *big);
			break;
		}
		// else the tiger's forest rule (without the water search); a forest with no grown tree falls through
		if (const auto lair = BestForestLair(me, forestId); lair)
		{
			TraceLair(ctx, "wolf forest", forestId, *lair);
			SetDomainCentre(*flock, *lair);
			break;
		}
		// else the nearest tree of the tree list (every tree), else where it is
		const auto tree = NearestNewestFirst<Tree>(me);
		const auto at = tree.value_or(ctx.transform.position);
		TraceLair(ctx, tree ? "wolf tree" : "wolf self", 0, at);
		SetDomainCentre(*flock, at);
		break;
	}
	default:
		// the lion: where it is
		SetDomainCentre(*flock, ctx.transform.position);
		break;
	}
}

void TestLairs()
{
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto id : ForestsNewestFirst())
	{
		const auto centre = ForestCentre(id);
		const auto grown = GrownTreesByDistance(id);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Lair test: forest {} centre ({:.1f}, {:.1f}) trees {} grown {}", id, centre.x,
		                   centre.z, ForestTreeCount(id), grown.size());
	}
	std::vector<entt::entity> leaders;
	registry.Each<const components::Animal, const components::AnimalBrain>(
	    [&](entt::entity e, const components::Animal& animal, const components::AnimalBrain&) {
		    if (HunterOf(animal.type) != Hunter::None)
		    {
			    leaders.push_back(e);
		    }
	    });
	for (const auto e : leaders)
	{
		auto& animal = registry.Get<components::Animal>(e);
		Context ctx {e, animal, registry.Get<components::AnimalBrain>(e), registry.Get<Transform>(e), InfoOf(animal)};
		if (IsLeader(ctx))
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Lair test: animal {} type {}", static_cast<uint32_t>(e),
			                   static_cast<int>(animal.type));
			CalculateLairPosition(ctx);
		}
	}
}

} // namespace openblack::ecs::animal_ai::detail
