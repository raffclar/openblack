/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include "ECS/Physics/Dust.h"
#include "ECS/Physics/FromHand.h"
#include "ECS/Physics/PhysicsBody.h"
#include "ECS/Physics/PhysicsObjects.h"

namespace openblack::ecs::physics
{
constexpr size_t k_NumConstants = 24;
// Data\PhysicsConstants.txt of the base game, used when the file cannot be read
constexpr std::array<PhysicsData, k_NumConstants> k_DefaultConstants = {{
    {0.8f, 78.75f, 0.4f, 2.65625f, 0.2f, 0.0f},
    {0.8f, 35.0f, 0.09f, 1.2f, 0.8f, 1.0f},
    {0.3f, 80.0f, 0.3f, 1.2f, 0.8f, 1.0f},
    {2.0f, 21.5625f, 1.328125f, 1.3125f, 0.709375f, 1.0f},
    {0.603125f, 31.640625f, 0.4f, 1.9375f, 0.575f, 1.875f},
    {0.914258f, 4.0625f, 0.195313f, 1.484375f, 0.628125f, 1.078125f},
    {0.741406f, 5.625f, 0.273438f, 1.796875f, 0.728125f, 1.28125f},
    {0.994922f, 26.25f, 3.046875f, 3.0f, 0.434375f, 2.078125f},
    {0.9f, 22.1875f, 0.4f, 1.46875f, 0.503125f, 1.8125f},
    {0.3f, 8.4375f, 0.028125f, 1.375f, 0.8f, 1.9375f},
    {0.8f, 54.0625f, 0.4f, 0.0f, 0.2f, 0.0f},
    {0.8f, 17.8125f, 4.296875f, 1.3f, 0.76875f, 1.0f},
    {0.8f, 12.1875f, 1.328125f, 1.2f, 0.371875f, 1.0f},
    {0.8f, 1.0f, 0.006f, 1.2f, 0.8f, 1.0f},
    {0.096094f, 239.0625f, 0.01875f, 1.09375f, 0.925f, 1.28125f},
    {0.8f, 41.25f, 1.210938f, 1.296875f, 0.996875f, 0.0f},
    {0.8f, 8.125f, 0.39375f, 1.171875f, 0.8f, 1.0625f},
    {0.8f, 32.5f, 0.328125f, 2.125f, 0.790625f, 1.78125f},
    {0.75293f, 19.6875f, 0.820313f, 1.328125f, 0.2625f, 1.65625f},
    {0.8f, 20.0f, 0.271875f, 1.21875f, 0.63125f, 0.890625f},
    {2.043555f, 101.25f, 0.140625f, 0.5f, 0.921875f, 0.8125f},
    {0.8f, 19.6875f, 0.4f, 1.09375f, 0.69375f, 0.0f},
    {0.8f, 20.3125f, 0.4f, 1.0f, 0.55625f, 1.046875f},
    {0.8f, 18.75f, 0.28125f, 0.984375f, 0.390625f, 1.015625f},
}};

/// The hand's release prediction: whether it is active, its object, its body, the history (N + 1 entries) and the
/// turns since it started
struct Prediction
{
	bool active {false};
	entt::entity object {entt::null};
	PhysicsBody body;
	std::vector<std::pair<glm::mat3, glm::vec3>> history;
	uint32_t turns {0};
};

/// A dust puff thrown up by a landing or a slide
struct DustPuff
{
	glm::vec3 velocity;
	float size;
	float age;
	uint32_t seed;         ///< rand % 16 of the cell
	DustParticleDraw draw; ///< what is drawn, from the last Update (the Sprite + Transform of the puff's entity before)
};

/// Two objects whose collision sound played, kept for some turns so that it does not repeat
struct SoundPair
{
	entt::entity a;
	entt::entity b;
	int turns;
};

/// What the physics objects keep between turns (ecs::systems::PhysicsObjectsSystemInterface owns it), as the game
/// starts: the constants are the base game's until PhysicsConstants.txt is read
struct State
{
	std::array<PhysicsData, k_NumConstants> constants = k_DefaultConstants;
	std::vector<std::unique_ptr<PhysicsObject>> objects;
	Prediction prediction;
	/// The allocated slots of the list: grown by 16 when it is full, never shrunk, no upper limit; Clear sets it to 0
	size_t capacity {0};
	std::array<PhysicsObjects::ClassHandlers, static_cast<size_t>(PhysicsClass::_Count)> classHandlers;
	/// The creature part of the catching check (PhysicsObjects::SetCreatureCatchHook)
	std::function<void(entt::entity, PhysicsObject&)> creatureCatchHook;
	std::vector<DustPuff> dustPuffs;
	std::vector<SoundPair> soundPairs; ///< at most 128
	from_hand::HandHooks handHooks;
};
} // namespace openblack::ecs::physics
