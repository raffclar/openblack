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

#include <string>
#include <vector>

#include <glm/vec3.hpp>

namespace openblack::ecs::components
{

/// One observer of a network game: a human of the god of the observers, with its own hand and camera
struct ObserverInterface
{
	/// The number that the game server gave the observer, from 1
	uint8_t number {0};
	/// The observer reported its camera at least once
	bool reported {false};
	/// The place of the hand and the place of the camera, as the last report of the observer that a turn did tells them
	glm::vec3 handPoint {0.0f};
	glm::vec3 camera {0.0f};
};

/// On the player of the god of the observers: the observers that are in the network game, in the order that they came
/// in. Every computer changes it at the same turn, from the commands of the game server and of the observers.
struct ObserverInterfaces
{
	std::vector<ObserverInterface> observers;
	/// The name of the god: the name of the last observer that came in. The chat names the lines of each observer by it.
	std::u16string name;
};

} // namespace openblack::ecs::components
