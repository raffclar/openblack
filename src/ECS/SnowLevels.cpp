/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SnowLevels.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "3D/FieldCrop.h"
#include "3D/SnowCover.h"
#include "Animals/BirdRules.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/HighDetail.h"
#include "ECS/Components/MapCellResident.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/SnowLevel.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Workshop.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "ECS/TownDesireFlags.h"

namespace openblack::ecs::snow_levels
{

using components::Animal;
using components::CarriedByTornado;
using components::Creature;
using components::Field;
using components::HeldByCreature;
using components::HighDetail;
using components::InHand;
using components::InPhysics;
using components::MapCellResident;
using components::PhysicsDrawPose;
using components::SnowLevel;
using components::TownDesireFlag;
using components::Transform;
using components::Tree;
using components::WorkshopFlag;
using components::WorshipSiteFlag;

std::optional<Hold> HoldOf(const Registry& registry, entt::entity entity)
{
	if (registry.AllOf<Creature>(entity))
	{
		return std::nullopt;
	}
	if (const auto* animal = registry.TryGet<const Animal>(entity); animal != nullptr && animals::birds::IsBird(animal->type))
	{
		return std::nullopt;
	}
	if (registry.AllOf<Field>(entity))
	{
		return Hold {.rate = field_crop::k_SnowCap, .cap = field_crop::k_SnowCap};
	}
	if (registry.AnyOf<TownDesireFlag, WorkshopFlag, WorshipSiteFlag>(entity))
	{
		return Hold {.rate = town_desire_flags::k_SnowCap, .cap = town_desire_flags::k_SnowCap};
	}
	if (const auto* snow = registry.TryGet<const SnowLevel>(entity); snow != nullptr && snow->drawnOutOfLand)
	{
		return Hold {.rate = snow_cover::k_OutOfLandTreeRate, .cap = snow_cover::k_OutOfLandTreeCap};
	}
	return Hold {.rate = snow_cover::k_ObjectRate, .cap = snow_cover::k_ObjectRate};
}

void Step(Registry& registry, std::span<const entt::entity> things, const std::function<float(glm::vec2)>& depthAt,
          int32_t step)
{
	if (step <= 0)
	{
		return;
	}
	for (const auto entity : things)
	{
		const auto* transform = registry.TryGet<const Transform>(entity);
		const auto hold = HoldOf(registry, entity);
		if (transform == nullptr || !hold.has_value())
		{
			continue;
		}
		const auto target =
		    snow_cover::ObjectLevel(depthAt({transform->position.x, transform->position.z}), hold->rate, hold->cap);
		if (auto* snow = registry.TryGet<SnowLevel>(entity); snow != nullptr)
		{
			snow->level = snow_cover::EaseLevel(snow->level, target, step);
		}
		else if (target != 0)
		{
			// A thing without snow gets its level the first time that it starts to show some
			registry.Assign<SnowLevel>(entity, snow_cover::EaseLevel(0, target, step));
		}
	}
}

int32_t LevelOf(const Registry& registry, entt::entity entity)
{
	const auto* snow = registry.TryGet<const SnowLevel>(entity);
	return snow != nullptr ? snow->level : 0;
}

int32_t ShownLevelOf(const Registry& registry, entt::entity entity)
{
	const auto* snow = registry.TryGet<const SnowLevel>(entity);
	return ShownLevel(snow, snow != nullptr && registry.AllOf<HighDetail>(entity));
}

int32_t ShownLevel(const SnowLevel* snow, bool highDetail)
{
	// A villager in high detail shows a different model, and the land never gives snow to that model
	if (snow == nullptr || highDetail)
	{
		return 0;
	}
	if (snow->broken.has_value())
	{
		return snow_cover::BrokenModelShownLevel(*snow->broken);
	}
	return snow->piece ? snow_cover::BrokenModelShownLevel(snow->level) : snow->level;
}

void FollowBreak(Registry& registry, entt::entity building, bool broken, uint32_t model)
{
	auto* snow = registry.TryGet<SnowLevel>(building);
	if (!broken)
	{
		if (snow != nullptr)
		{
			snow->broken.reset();
		}
		return;
	}
	if (snow == nullptr)
	{
		registry.Assign<SnowLevel>(building, SnowLevel {.broken = 0, .brokenModel = model});
	}
	else if (!snow->broken.has_value() || snow->brokenModel != model)
	{
		snow->broken = snow->level;
		snow->brokenModel = model;
	}
}

void StepScaffoldPhantoms(Registry& registry, const std::function<float(glm::vec2)>& depthAt, int32_t step)
{
	std::vector<std::pair<entt::entity, entt::entity>> shown;
	registry.Each<const components::ScaffoldGhost>([&shown](entt::entity phantom, const components::ScaffoldGhost& ghost) {
		if (ghost.model.has_value())
		{
			shown.emplace_back(phantom, ghost.scaffold);
		}
	});
	for (const auto& [phantom, scaffold] : shown)
	{
		auto* snow = registry.TryGet<SnowLevel>(phantom);
		if (!registry.Valid(scaffold) || !registry.AllOf<InHand>(scaffold))
		{
			// The site shows it: no snow from now on
			if (snow == nullptr)
			{
				registry.Assign<SnowLevel>(phantom, SnowLevel {.noSnow = true});
			}
			else
			{
				snow->level = 0;
				snow->noSnow = true;
			}
			continue;
		}
		if ((snow != nullptr && snow->noSnow) || step <= 0)
		{
			continue;
		}
		const auto* transform = registry.TryGet<const Transform>(phantom);
		if (transform == nullptr)
		{
			continue;
		}
		const auto target = snow_cover::ObjectLevel(depthAt({transform->position.x, transform->position.z}),
		                                            snow_cover::k_ObjectRate, snow_cover::k_ObjectRate);
		if (snow != nullptr)
		{
			snow->level = snow_cover::EaseLevel(snow->level, target, step);
		}
		else if (target != 0)
		{
			registry.Assign<SnowLevel>(phantom, snow_cover::EaseLevel(0, target, step));
		}
	}
}

void TakePieceSnow(Registry& registry, entt::entity piece, entt::entity building)
{
	if (registry.AllOf<SnowLevel>(piece))
	{
		return;
	}
	int32_t level = 0;
	if (building != entt::null && registry.Valid(building))
	{
		// The broken model has the snow of the building until the frame notes the break
		if (const auto* snow = registry.TryGet<const SnowLevel>(building); snow != nullptr)
		{
			level = snow->broken.value_or(snow->level);
		}
	}
	registry.Assign<SnowLevel>(piece, SnowLevel {.level = level, .piece = true});
}

void NoteTreesOutOfLand(Registry& registry)
{
	const auto note = [&registry](entt::entity tree) {
		if (registry.AllOf<MapCellResident>(tree))
		{
			return;
		}
		// The game does not draw a body sunk under the sea
		if (const auto* drawn = registry.TryGet<const PhysicsDrawPose>(tree); drawn != nullptr && drawn->underSea)
		{
			return;
		}
		if (auto* snow = registry.TryGet<SnowLevel>(tree); snow != nullptr)
		{
			snow->drawnOutOfLand = true;
		}
		else
		{
			registry.Assign<SnowLevel>(tree, SnowLevel {.drawnOutOfLand = true});
		}
	};
	std::vector<entt::entity> trees;
	registry.Each<const Tree, const InHand>([&trees](entt::entity tree, const Tree&, const InHand&) { trees.push_back(tree); });
	registry.Each<const Tree, const InPhysics>([&trees](entt::entity tree, const Tree&) { trees.push_back(tree); });
	registry.Each<const Tree, const HeldByCreature>(
	    [&trees](entt::entity tree, const Tree&, const HeldByCreature&) { trees.push_back(tree); });
	registry.Each<const Tree, const CarriedByTornado>(
	    [&trees](entt::entity tree, const Tree&, const CarriedByTornado&) { trees.push_back(tree); });
	std::ranges::for_each(trees, note);
}

void StepBroken(Registry& registry, std::span<const entt::entity> buildings, const std::function<float(glm::vec2)>& depthAt,
                int32_t step)
{
	if (step <= 0)
	{
		return;
	}
	for (const auto entity : buildings)
	{
		auto* snow = registry.TryGet<SnowLevel>(entity);
		const auto* transform = registry.TryGet<const Transform>(entity);
		if (snow == nullptr || !snow->broken.has_value() || transform == nullptr)
		{
			continue;
		}
		const auto target = snow_cover::BrokenModelLevel(depthAt({transform->position.x, transform->position.z}));
		snow->broken = snow_cover::EaseLevel(*snow->broken, target, step);
	}
}

} // namespace openblack::ecs::snow_levels
