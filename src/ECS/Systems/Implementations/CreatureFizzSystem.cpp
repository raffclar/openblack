/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureFizzSystem.h"

#include <cmath>

#include <chrono>
#include <vector>

#include <glm/geometric.hpp>

#include "3D/L3DMesh.h"
#include "Audio/Sound.h"
#include "Creature/CreatureFizzLook.h"
#include "ECS/Components/CreatureFizz.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreatureRemoval.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "ECS/Systems/SoundTagSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Graphics/MeshDetail.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Scenery/ObjectDrawList.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
constexpr auto k_TurnMilliseconds =
    static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(TimeSystemInterface::k_TurnDuration).count());

/// The creature is drawn as fizzed as its fizz says
void Show(ecs::Registry& registry, entt::entity creature, float fizz)
{
	auto* spells = registry.TryGet<CreatureSpells>(creature);
	if (spells == nullptr)
	{
		spells = &registry.Assign<CreatureSpells>(creature);
	}
	spells->fizz = fizz;
}
} // namespace

void CreatureFizzSystem::SetFizz(entt::entity creature, float target, float seconds, bool goesForGood)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		return;
	}
	auto* current = registry.TryGet<CreatureFizz>(creature);
	const auto set =
	    creature_fizz::SetFizz(current != nullptr ? current->fizz : creature_fizz::Fizz {}, target, seconds, goesForGood);
	if (set.sound && Locator::soundTagSystem::has_value())
	{
		if (const auto* transform = registry.TryGet<const Transform>(creature))
		{
			Locator::soundTagSystem::value().CreatePointSound(
			    static_cast<entt::id_type>(audio::SoundId::G_SpellTeleportEnergiseGo), transform->position, false);
		}
	}
	registry.AssignOrReplace<CreatureFizz>(creature, CreatureFizz {.fizz = set.fizz});
	Show(registry, creature, set.fizz.now);
}

void CreatureFizzSystem::ProcessTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> settled;
	std::vector<entt::entity> gone;
	registry.Each<CreatureFizz>([&](entt::entity entity, CreatureFizz& component) {
		component.fizz = creature_fizz::Step(component.fizz, k_TurnMilliseconds);
		if (creature_fizz::Gone(component.fizz))
		{
			gone.push_back(entity);
		}
		else if (creature_fizz::Settled(component.fizz))
		{
			settled.push_back(entity);
		}
	});
	for (const auto entity : settled)
	{
		Show(registry, entity, 0.0f);
		registry.Remove<CreatureFizz>(entity);
	}
	registry.Each<const CreatureFizz>([&registry](entt::entity entity, const CreatureFizz& component) {
		if (auto* spells = registry.TryGet<CreatureSpells>(entity))
		{
			spells->fizz = component.fizz.now;
		}
	});
	// Fizzed right out for good, it leaves the game the way any deleted creature does
	for (const auto entity : gone)
	{
		if (!ecs::creature_removal::RemoveFromGame(entity))
		{
			registry.Destroy(entity);
		}
	}
	if (!gone.empty())
	{
		registry.SetDirty();
	}
}

void CreatureFizzSystem::UpdateFrame(float seconds)
{
	_eyeStaticScroll = creature_fizz_look::Scroll(_eyeStaticScroll, seconds);
}

void CreatureFizzSystem::SlideDrawnStatic(float seconds)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* drawList = Locator::objectDrawListSystem::has_value() ? &Locator::objectDrawListSystem::value() : nullptr;
	const auto* view = drawList != nullptr && drawList->GetView().has_value() ? &*drawList->GetView() : nullptr;
	const auto* meshes = Locator::resources::has_value() ? &Locator::resources::value().GetMeshes() : nullptr;
	registry.Each<CreatureSpells, const Transform, const Mesh>(
	    [&](entt::entity entity, CreatureSpells& spells, const Transform& transform, const Mesh& mesh) {
		    if (!creature_fizz_look::Fizzing(spells.fizz))
		    {
			    return;
		    }
		    // With no camera or land, the frame draws the body as it is
		    bool onScreen = true;
		    if (view != nullptr)
		    {
			    onScreen = drawList->Drawn(registry, entity);
			    if (onScreen && meshes != nullptr && meshes->Contains(mesh.id))
			    {
				    // The sphere round the box of the body's model, at the place and size that the frame draws the body
				    const auto box = meshes->Handle(mesh.id)->GetBoundingBox();
				    const auto centre = transform.position + (transform.rotation * (transform.scale * box.Center()));
				    const float radius = std::abs(transform.scale.x) * graphics::mesh_detail::HalfDiagonal(box.Size());
				    onScreen = object_draw_list::SphereOnScreen(*view, transform.position, centre, radius);
			    }
		    }
		    if (creature_fizz_look::StaticSlides(spells.fizz, onScreen))
		    {
			    spells.staticScroll = creature_fizz_look::Scroll(spells.staticScroll, seconds);
		    }
	    });
}

glm::vec2 CreatureFizzSystem::EyeStaticScroll() const
{
	return _eyeStaticScroll;
}

float CreatureFizzSystem::FizzOf(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		return 0.0f;
	}
	if (const auto* component = registry.TryGet<const CreatureFizz>(creature))
	{
		return component->fizz.now;
	}
	return 0.0f;
}
