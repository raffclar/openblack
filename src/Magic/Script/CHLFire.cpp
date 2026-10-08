/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CHLFire.h"

#include <LHVM.h>
#include <entt/entity/entity.hpp>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
/// The script's thing ("Thing not valid") that must be an object ("Thing not object"): the port's objects are the
/// entities with a transform
entt::entity ScriptObject(uint32_t value)
{
	const auto entity = static_cast<entt::entity>(value);
	auto& registry = Locator::entitiesRegistry::value();
	if (value == 0 || !registry.Valid(entity))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Thing not valid");
		return entt::null;
	}
	if (!registry.AllOf<components::Transform>(entity))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Thing not object");
		return entt::null;
	}
	return entity;
}
} // namespace

void magic::script::IsOnFire()
{
	auto& vm = Locator::vm::value();
	const auto object = ScriptObject(vm.Pop().uintVal);
	vm.Pushb(object != entt::null && fire::IsOnFire(object));
}

void magic::script::IsFireNear()
{
	auto& vm = Locator::vm::value();
	const auto radius = vm.Popf();
	const auto z = vm.Popf();
	const auto y = vm.Popf();
	const auto x = vm.Popf();
	// FindNearForScript over the cells of the square +-r, fixed then mobile. The predicate: on fire and the distance in
	// metres from the script's point to the object (a worship site: its totem) <= r. A fireball is not in the map
	// (it never inserts itself)
	const auto at = map_coords::FromWorld(glm::vec3(x, y, z));
	const auto onFireNear = [&at, radius](entt::entity object) {
		if (!fire::IsOnFire(object))
		{
			return false;
		}
		auto point = ecs::object::MapCoordsOf(object);
		if (const auto* site = Locator::entitiesRegistry::value().TryGet<const components::WorshipSite>(object);
		    site != nullptr && site->totem != entt::null && Locator::entitiesRegistry::value().Valid(site->totem))
		{
			point = ecs::object::MapCoordsOf(site->totem); // (inferred) openblack's totem entity
		}
		return gutils::GetDistanceInMetres(at, point) <= radius;
	};
	const bool found = ecs::map_cells::FindNearForScript(at, onFireNear, radius) != entt::null;
	vm.Pushb(found);
}

void magic::script::SetTemperature()
{
	auto& vm = Locator::vm::value();
	const auto temperature = vm.Popf();
	const auto object = ScriptObject(vm.Pop().uintVal);
	if (object != entt::null)
	{
		fire::SetTemperature(object, temperature, entt::null);
	}
}

void magic::script::SetOnFire()
{
	auto& vm = Locator::vm::value();
	const auto speed = vm.Popf();
	const auto object = ScriptObject(vm.Pop().uintVal);
	const auto enable = vm.Pop().intVal != 0;
	if (object == entt::null)
	{
		return;
	}
	if (enable)
	{
		fire::SetOnFire(object, speed);
	}
	else
	{
		const auto& transform = Locator::entitiesRegistry::value().Get<const components::Transform>(object);
		fire::SetTemperature(object, fire::AmbientTemperature(transform.position), entt::null);
	}
}

void magic::script::SetHurtByFire()
{
	auto& vm = Locator::vm::value();
	const auto object = ScriptObject(vm.Pop().uintVal);
	const auto enable = vm.Pop().intVal != 0;
	if (object != entt::null)
	{
		fire::traits::SetNotHurtByFire(object, !enable);
	}
}

void magic::script::SetSetOnFire()
{
	auto& vm = Locator::vm::value();
	const auto object = ScriptObject(vm.Pop().uintVal);
	const auto enable = vm.Pop().intVal != 0;
	if (object != entt::null)
	{
		fire::traits::SetCannotBeSetOnFire(object, !enable);
	}
}
