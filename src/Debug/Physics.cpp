/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Physics.h"

#include <algorithm>
#include <limits>

#include <imgui.h>

#include "Camera/Camera.h"
#include "Common/DebugThingActRuns.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::debug::gui;

Physics::Physics() noexcept
    : Window("Physics", ImVec2(320.0f, 260.0f))
{
}

void Physics::Draw() noexcept
{
	if (!Locator::dynamicsSystem::has_value() || !Locator::entitiesRegistry::has_value())
	{
		ImGui::Text("There is no physics");
		return;
	}
	auto& dynamics = Locator::dynamicsSystem::value();
	int moving = 0;
	int resting = 0;
	dynamics.ForEachEntry([&moving, &resting](const ecs::PhysicsEntry& entry) { (entry.IsFlying() ? moving : resting) += 1; });
	ImGui::Text("Bodies: %d moving, %d resting", moving, resting);

	ImGui::InputInt("Static type", &_type);
	ImGui::InputFloat("Height", &_height);
	ImGui::InputFloat("Scale", &_scale);
	ImGui::InputFloat3("Velocity", _velocity.data());
	ImGui::InputInt("Object (-1: a new one)", &_entity);
	if (ImGui::Button("Throw at the camera's focus") && Locator::camera::has_value())
	{
		// Each computer finds the height of the land under the focus at the turn. A type below 0 is not a type, thus each
		// computer refuses the act.
		const auto focus = Locator::camera::value().GetFocus();
		const player_commands::DebugPhysics act {
		    .object = _entity < 0 ? entt::entity {entt::null} : static_cast<entt::entity>(_entity),
		    .type = static_cast<uint16_t>(_type < 0 ? std::numeric_limits<uint16_t>::max()
		                                            : std::min(_type, static_cast<int>(std::numeric_limits<uint16_t>::max()))),
		    .height = _height,
		    .scale = _scale,
		    .place = glm::vec2(focus.x, focus.z),
		    .velocity = glm::vec3(_velocity[0], _velocity[1], _velocity[2]),
		};
		const auto result = debug_thing_acts::Do(act);
		_result = !result.has_value() ? "Sent as a command of the turns" : result->done ? "Thrown" : "Refused";
	}
	if (!_result.empty())
	{
		ImGui::TextUnformatted(_result.c_str());
	}
}

void Physics::Update() noexcept {}

void Physics::ProcessEventOpen([[maybe_unused]] const SDL_Event& event) noexcept {}

void Physics::ProcessEventAlways([[maybe_unused]] const SDL_Event& event) noexcept {}
