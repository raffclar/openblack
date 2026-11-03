/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <glm/vec3.hpp>

#include "ECS/Components/ObserverInterfaces.h"

/// The rules of the observers of the god of the observers: each observer has its own hand and camera, and the other
/// computers show a sparkle at the camera of each observer
namespace openblack::observer_interfaces
{

/// An observer came into the game. The god takes the name of the observer. An observer with a number that is in the game
/// already keeps its place and its camera.
inline void Come(ecs::components::ObserverInterfaces& interfaces, uint8_t number, std::u16string name)
{
	interfaces.name = std::move(name);
	if (std::ranges::find(interfaces.observers, number, &ecs::components::ObserverInterface::number) ==
	    interfaces.observers.end())
	{
		interfaces.observers.push_back({.number = number});
	}
}

/// An observer left the game. The god keeps its name.
inline void Go(ecs::components::ObserverInterfaces& interfaces, uint8_t number)
{
	std::erase_if(interfaces.observers,
	              [number](const ecs::components::ObserverInterface& observer) { return observer.number == number; });
}

/// The last observer left: no observer is in the game. The god keeps its name.
inline void AllGone(ecs::components::ObserverInterfaces& interfaces)
{
	interfaces.observers.clear();
}

/// An observer reports its hand and its camera. A report of an observer that is not in the game changes nothing.
inline void Report(ecs::components::ObserverInterfaces& interfaces, uint8_t number, const glm::vec3& handPoint,
                   const glm::vec3& camera)
{
	const auto found = std::ranges::find(interfaces.observers, number, &ecs::components::ObserverInterface::number);
	if (found == interfaces.observers.end())
	{
		return;
	}
	found->reported = true;
	found->handPoint = handPoint;
	found->camera = camera;
}

/// The observers whose cameras a computer shows a sparkle at: each observer that reported its camera, except the
/// observer of this computer (none on the computer of a god), in the order that the observers came in
[[nodiscard]] inline std::vector<ecs::components::ObserverInterface>
ShownObservers(const ecs::components::ObserverInterfaces& interfaces, std::optional<uint8_t> ownObserver)
{
	std::vector<ecs::components::ObserverInterface> shown;
	for (const auto& observer : interfaces.observers)
	{
		if (observer.reported && observer.number != ownObserver)
		{
			shown.push_back(observer);
		}
	}
	return shown;
}

} // namespace openblack::observer_interfaces
