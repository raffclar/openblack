/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "FootprintSystem.h"

#include <numbers>
#include <optional>
#include <utility>

#include <glm/vec2.hpp>

#include "3D/CreatureBody.h"
#include "3D/LandIslandInterface.h"
#include "Creature/CreatureRig.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreaturePose.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

FootprintSystem::FootprintSystem(DateProvider today)
    : _today(std::move(today))
{
}

void FootprintSystem::Update(std::chrono::duration<float, std::milli> gameTime)
{
	creature_footprints::Fade(_trail, gameTime.count());
}

void FootprintSystem::Step(entt::entity creature)
{
	if (!Locator::entitiesRegistry::has_value() || !Locator::resources::has_value())
	{
		return;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (!registry.Valid(creature) || !registry.AllOf<Creature, CreatureAnimation, Transform>(creature) ||
	    !Locator::terrainSystem::has_value())
	{
		return;
	}
	const auto& component = registry.Get<const Creature>(creature);
	const auto& animation = registry.Get<const CreatureAnimation>(creature);
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto rigId = creature::GetRigId(component.species);
	if (!rigs.Contains(rigId))
	{
		return;
	}
	const auto& points = rigs.Handle(rigId)->actionPoints;
	if (!points.has_value() || points->rightFoot >= animation.boneMatrices.size())
	{
		return;
	}
	const std::optional<uint32_t> rightBone = points->rightFoot;
	const auto leftBone = *rightBone < animation.mirror.size() ? animation.mirror[*rightBone] : *rightBone;

	// the print is laid under the foot as the body is drawn this frame (its own drawn matrix: between its turns, smaller
	// in its pen)
	const auto placement = ecs::DrawnBodyModel(registry, creature);
	const auto right = creature_footprints::FootOf(creature::PosedBone(*rightBone, animation.boneMatrices, placement));
	const auto left = creature_footprints::FootOf(creature::PosedBone(leftBone, animation.boneMatrices, placement));
	const bool onLeft = creature_footprints::LeftIsLower(right, left);
	const auto& foot = onLeft ? left : right;

	const auto print = creature_footprints::PrintOf(component.species, IsAprilFools());
	const auto& land = Locator::terrainSystem::value();
	// as big as the foot is drawn: smaller in its pen
	const auto side = creature_footprints::Side(component.size, print) * ecs::creature_pose::DrawnSizeShare(registry, creature);
	const auto laid =
	    creature_footprints::MakeFootprint(foot.position, foot.yaw + std::numbers::pi_v<float>, side, print.cell, onLeft,
	                                       [&land](float x, float z) { return land.GetHeightAt(glm::vec2(x, z)); });
	if (!creature_footprints::Add(_trail, laid))
	{
		++_dropped;
	}
}

void FootprintSystem::Reset()
{
	_trail = {};
	_dropped = 0;
	_date = _today ? std::optional(_today()) : std::nullopt;
}

bool FootprintSystem::IsAprilFools() const
{
	if (_aprilFools.has_value())
	{
		return *_aprilFools;
	}
	return _date.has_value() && creature_footprints::IsAprilFools(_date->month, _date->day);
}
