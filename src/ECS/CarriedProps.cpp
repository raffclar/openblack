/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CarriedProps.h"

#include <array>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "3D/AllMeshes.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"

namespace openblack::ecs
{
using namespace components;

namespace
{
/// the mesh (AllMeshes.h index) of each CARRIED_OBJECT, 0 = none
constexpr std::array<int32_t, 16> k_PropMeshes = {0, 0, 342, 355, 354, 383, 343, 344, 367, 378, 384, 390, 406, 347, 348, 349};
constexpr uint32_t k_GripBone = 15;

bool Hidden(entt::entity villager)
{
	const auto* action = Locator::entitiesRegistry::value().TryGet<const LivingAction>(villager);
	if (action == nullptr)
	{
		return false;
	}
	const auto state = static_cast<VillagerStates>(action->states[static_cast<size_t>(LivingAction::Index::Top)]);
	// not in the hand, and not in a state that isn't drawn (info.dat clip -4)
	return state == VillagerStates::InHand ||
	       (static_cast<size_t>(state) < 255 &&
	        static_cast<int32_t>(Locator::infoConstants::value().villagerStateTable.at(static_cast<size_t>(state)).animation) ==
	            -4);
}
} // namespace

uint32_t CarriedObjectMesh(int32_t carriedObject)
{
	if (carriedObject < 0 || carriedObject >= static_cast<int32_t>(k_PropMeshes.size()))
	{
		return 0;
	}
	return static_cast<uint32_t>(k_PropMeshes.at(static_cast<size_t>(carriedObject)));
}

void UpdateCarriedProps()
{
	auto& registry = Locator::entitiesRegistry::value();
	// the props of villagers that stop carrying, or are gone
	std::vector<entt::entity> gone;
	registry.Each<const CarriedProp>([&](entt::entity prop, const CarriedProp& carried) {
		const auto* animation =
		    ecs::IsAvailable(carried.owner) ? registry.TryGet<const SkeletalAnimation>(carried.owner) : nullptr;
		if (animation == nullptr || animation->carriedObject != carried.type || animation->pose.size() <= k_GripBone ||
		    Hidden(carried.owner))
		{
			gone.push_back(prop);
		}
	});
	for (const auto prop : gone)
	{
		registry.Destroy(prop);
	}
	// new props
	std::vector<std::pair<entt::entity, int32_t>> wanted;
	registry.Each<const Villager, const SkeletalAnimation>([&](entt::entity villager, const Villager&,
	                                                           const SkeletalAnimation& animation) {
		const auto type = animation.carriedObject;
		if (type > 1 && type < static_cast<int32_t>(k_PropMeshes.size()) && k_PropMeshes.at(static_cast<size_t>(type)) != 0 &&
		    animation.pose.size() > k_GripBone && !Hidden(villager) && ecs::IsAvailable(villager))
		{
			wanted.emplace_back(villager, type);
		}
	});
	bool moved = false;
	for (const auto& [villager, type] : wanted)
	{
		entt::entity prop = entt::null;
		registry.Each<const CarriedProp>([&](entt::entity e, const CarriedProp& carried) {
			if (carried.owner == villager)
			{
				prop = e;
			}
		});
		if (prop == entt::null)
		{
			prop = registry.Create();
			registry.Assign<CarriedProp>(prop, villager, type);
			registry.Assign<Transform>(prop, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
			const auto mesh = static_cast<MeshId>(k_PropMeshes.at(static_cast<size_t>(type)));
			registry.Assign<Mesh>(prop, resources::HashIdentifier(mesh), static_cast<int8_t>(0), static_cast<int8_t>(0));
		}
		// the villager's model matrix (ecs::DrawnModel, as RenderingSystem draws it: in the physics its drawn pose between
		// its last two turns, with no slope shear) * bone 15 * the axis swap
		const auto model = DrawnModel(registry, villager);
		glm::mat4 swap(0.0f);
		swap[0] = glm::vec4(-1.0f, 0.0f, 0.0f, 0.0f);
		swap[1] = glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);
		swap[2] = glm::vec4(0.0f, -1.0f, 0.0f, 0.0f);
		swap[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		const auto world = model * registry.Get<const SkeletalAnimation>(villager).pose[k_GripBone] * swap;
		auto& transform = registry.Get<Transform>(prop);
		const float scale = glm::length(glm::vec3(world[0]));
		transform.position = glm::vec3(world[3]);
		transform.scale = glm::vec3(scale);
		transform.rotation = glm::mat3(world) / (scale > 0.0f ? scale : 1.0f);
		moved = true;
	}
	if (moved)
	{
		registry.SetDirty();
	}
}

} // namespace openblack::ecs
