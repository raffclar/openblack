/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "PuzzleTotemSystem.h"

#include <glm/vec2.hpp>

#include "3D/LandIslandInterface.h"
#include "Commands/PlayerCommands.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/PuzzleGame.h"
#include "ECS/Components/Transform.h"
#include "ECS/PuzzleTotemRules.h"
#include "ECS/Registry.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/VillageTotem.h"
#include "ECS/WorldObjects.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{

/// How far the totem goes under the land at height 0, from its model and its scale
float DepthOf(const ecs::Registry& registry, entt::entity totem)
{
	const auto* mesh = registry.TryGet<const Mesh>(totem);
	const auto* transform = registry.TryGet<const Transform>(totem);
	if (mesh == nullptr || transform == nullptr)
	{
		return 0.0f;
	}
	const auto model = ecs::world_objects::SizeOfModel(mesh->id, 1.0f);
	if (!model.has_value())
	{
		return 0.0f;
	}
	return ecs::puzzle_totem::SinkDepth(model->height * 0.5f, transform->scale.x);
}

/// The height of the land under the totem
float LandUnder(const glm::vec3& position)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(position.x, position.z))
	                                           : 0.0f;
}

/// Only a totem that rises and sinks: a totem that only takes taps does not
bool Rises(const PuzzleTotem& totem)
{
	return totem.highest != ecs::puzzle_totem::k_TapOnly;
}

/// Where the totem stands drawn, and the hand above it, while the hand holds it
float DrawnY(const ecs::Registry& registry, entt::entity totem)
{
	return registry.Get<const Transform>(totem).position.y;
}

} // namespace

void PuzzleTotemSystem::SetHeight(entt::entity totem, int32_t height)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* component = registry.TryGet<PuzzleTotem>(totem);
	if (component == nullptr)
	{
		return;
	}
	component->height = height;
	Place(totem);
}

void PuzzleTotemSystem::Place(entt::entity totem)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* component = registry.TryGet<const PuzzleTotem>(totem);
	auto* transform = registry.TryGet<Transform>(totem);
	if (component == nullptr || transform == nullptr || !Rises(*component))
	{
		return;
	}
	const float depth = DepthOf(registry, totem);
	// It stands where its height puts it. A totem that the hand holds moves when the turn takes each new height that the
	// hand gives it, so that every computer has it at the same place.
	const float offset = ecs::puzzle_totem::OffsetFor(ecs::puzzle_totem::ShareOf(component->height, component->highest), depth);
	const float y = LandUnder(transform->position) + offset;
	if (transform->position.y != y)
	{
		transform->position.y = y;
		registry.SetDirty();
	}
}

bool PuzzleTotemSystem::Grip(entt::entity totem, PlayerNames player, glm::vec3 hand)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* component = registry.Valid(totem) ? registry.TryGet<const PuzzleTotem>(totem) : nullptr;
	if (component == nullptr || !Rises(*component) || component->held || _gripped != entt::null)
	{
		return false;
	}
	if (!Locator::influenceSystem::has_value() || !Locator::influenceSystem::value().IsHandInInfluence(player, hand))
	{
		return false;
	}
	// A totem with no model to measure cannot rise or sink
	const float depth = DepthOf(registry, totem);
	if (depth <= 0.0f)
	{
		return false;
	}
	_gripped = totem;
	// The hold starts where the totem stands now
	_hold = ecs::puzzle_totem::OffsetFor(ecs::puzzle_totem::ShareOf(component->height, component->highest), depth);
	_heldHeight = component->height;
	if (Locator::playerCommandSystem::has_value())
	{
		Locator::playerCommandSystem::value().Submit(player_commands::HandLock {.object = totem});
	}
	else
	{
		DoHeld(totem, true);
	}
	return true;
}

void PuzzleTotemSystem::Slide(float upPixels, float screenHeight)
{
	if (_gripped == entt::null || upPixels == 0.0f)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto* component = registry.Valid(_gripped) ? registry.TryGet<const PuzzleTotem>(_gripped) : nullptr;
	if (component == nullptr)
	{
		_gripped = entt::null;
		return;
	}
	// The totem moves only once the turn has seen the hand take hold of it
	if (!component->held)
	{
		return;
	}
	const float height = ecs::world_objects::SizeOf(_gripped).height;
	const auto& position = registry.Get<const Transform>(_gripped).position;
	const float handY = ecs::village_totem::GripY(DrawnY(registry, _gripped), height);
	const float slide = ecs::village_totem::HandSlide(upPixels, screenHeight, height, handY - LandUnder(position));
	const float depth = DepthOf(registry, _gripped);
	_hold = ecs::puzzle_totem::HoldAfterSlide(_hold, slide, depth);
	const auto before = ecs::puzzle_totem::HoldOffset(_heldHeight, component->highest, depth);
	_heldHeight = ecs::puzzle_totem::HeightForHold(_hold, depth, component->highest);
	// Each new place of the totem goes to the turn
	if (ecs::puzzle_totem::HoldOffset(_heldHeight, component->highest, depth) != before)
	{
		SendShare();
	}
}

void PuzzleTotemSystem::SendShare() const
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& component = registry.Get<const PuzzleTotem>(_gripped);
	const float depth = DepthOf(registry, _gripped);
	const float share =
	    ecs::puzzle_totem::ShareForOffset(ecs::puzzle_totem::HoldOffset(_heldHeight, component.highest, depth), depth);
	if (Locator::playerCommandSystem::has_value())
	{
		Locator::playerCommandSystem::value().Submit(player_commands::TotemShare {.totem = _gripped, .share = share});
	}
}

void PuzzleTotemSystem::LetGo()
{
	if (_gripped == entt::null)
	{
		return;
	}
	const auto totem = _gripped;
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.Valid(totem) && registry.AllOf<PuzzleTotem>(totem))
	{
		SendShare();
	}
	_gripped = entt::null;
	if (Locator::playerCommandSystem::has_value())
	{
		Locator::playerCommandSystem::value().Submit(player_commands::HandUnlock {.object = totem});
	}
	else
	{
		DoHeld(totem, false);
	}
}

std::optional<entt::entity> PuzzleTotemSystem::GetGripped() const
{
	return _gripped != entt::null ? std::optional(_gripped) : std::nullopt;
}

std::optional<VillageTotemSystemInterface::HandHold> PuzzleTotemSystem::GetHandHold() const
{
	if (_gripped == entt::null)
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(_gripped))
	{
		return std::nullopt;
	}
	const auto size = ecs::world_objects::SizeOf(_gripped);
	const auto& position = registry.Get<const Transform>(_gripped).position;
	const float y = ecs::village_totem::GripY(DrawnY(registry, _gripped), size.height);
	return VillageTotemSystemInterface::HandHold {
	    .position = {position.x, y, position.z},
	    .tilt = ecs::village_totem::HandTiltAt(y),
	    .closure = ecs::village_totem::ClosureForHoldRadius(size.radius),
	};
}

void PuzzleTotemSystem::DoHeld(entt::entity totem, bool held)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* component = registry.Valid(totem) ? registry.TryGet<PuzzleTotem>(totem) : nullptr; component != nullptr)
	{
		component->held = held;
	}
}

void PuzzleTotemSystem::DoShare(entt::entity totem, float share)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* component = registry.Valid(totem) ? registry.TryGet<const PuzzleTotem>(totem) : nullptr;
	if (component == nullptr)
	{
		return;
	}
	SetHeight(totem, ecs::puzzle_totem::HeightForShare(share, component->highest));
}
