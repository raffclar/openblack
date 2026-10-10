/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillageTotemSystem.h"

#include <optional>
#include <vector>

#include <glm/gtx/euler_angles.hpp>

#include "Common/GUtilsAngle.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Registry.h"
#include "ECS/VillageTotem.h"
#include "GameVillageTotemWorld.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

VillageTotemSystem::VillageTotemSystem()
    : VillageTotemSystem(std::make_unique<GameVillageTotemWorld>())
{
}

VillageTotemSystem::VillageTotemSystem(std::unique_ptr<village_totem::WorldInterface> world)
    : _world(std::move(world))
{
}

VillageTotemSystem::~VillageTotemSystem() = default;

entt::entity VillageTotemSystem::TownOf(entt::entity totem) const
{
	auto& registry = _world->Entities();
	const auto* totemComponent = registry.TryGet<const VillageTotem>(totem);
	if (totemComponent == nullptr || !registry.Valid(totemComponent->townCentre))
	{
		return entt::null;
	}
	const auto* abode = registry.TryGet<const Abode>(totemComponent->townCentre);
	if (abode == nullptr)
	{
		return entt::null;
	}
	const auto& towns = registry.Context().towns;
	const auto found = towns.find(abode->townId);
	return found != towns.end() && registry.Valid(found->second) ? found->second : entt::null;
}

void VillageTotemSystem::AddToPlayer(entt::entity totem)
{
	auto& registry = _world->Entities();
	auto* totemComponent = registry.TryGet<VillageTotem>(totem);
	if (totemComponent == nullptr)
	{
		return;
	}
	const auto town = TownOf(totem);
	if (town == entt::null)
	{
		SetTotemShare(totem, 0.0f);
		return;
	}
	const auto& townComponent = registry.Get<const Town>(town);
	// It turns to face the town's worship site, side on
	if (const auto* site =
	        registry.Valid(townComponent.worshipSite) ? registry.TryGet<const Transform>(townComponent.worshipSite) : nullptr;
	    site != nullptr)
	{
		auto& transform = registry.Get<Transform>(totem);
		const float angle = gutils::Get3DAngleFromXZ(glm::vec2(transform.position.x, transform.position.z),
		                                             glm::vec2(site->position.x, site->position.z)) +
		                    gutils::k_ScawenOffset;
		transform.rotation = glm::mat3(glm::eulerAngleY(-angle));
		if (auto* iconTransform = registry.TryGet<Transform>(totemComponent->icon))
		{
			iconTransform->rotation = transform.rotation;
		}
	}
	// The player's creature stands on top, or the hand
	if (auto* mesh = registry.TryGet<Mesh>(totemComponent->icon))
	{
		mesh->id = _world->IconMeshFor(townComponent.owner);
	}
	SetTownShare(town, 0.0f);
}

void VillageTotemSystem::SetTotemShare(entt::entity totem, float share)
{
	auto& totemComponent = _world->Entities().Get<VillageTotem>(totem);
	village_totem::SetShare(totemComponent.ease, share);
	_world->SetMovingSound(totem, true);
}

void VillageTotemSystem::SetTownShare(entt::entity town, float share)
{
	auto& registry = _world->Entities();
	auto* townComponent = registry.TryGet<Town>(town);
	if (townComponent == nullptr)
	{
		return;
	}
	// A town with nowhere to worship has no one at worship, and its totem is left as it was
	if (!registry.Valid(townComponent->worshipSite))
	{
		townComponent->worshipShare = 0.0f;
		return;
	}
	townComponent->worshipShare = share;
	registry.Each<VillageTotem>([this, town, share](entt::entity totem, VillageTotem& totemComponent) {
		if (TownOf(totem) == town)
		{
			SetTotemShare(totem, share);
			totemComponent.held = totemComponent.ease.target;
		}
	});
}

void VillageTotemSystem::Place(entt::entity totem)
{
	auto& registry = _world->Entities();
	const auto& totemComponent = registry.Get<const VillageTotem>(totem);
	// Held by the hand, it stands where the hand holds it; otherwise where it has eased to
	const float share = totemComponent.gripped ? totemComponent.held : totemComponent.ease.share;
	auto& transform = registry.Get<Transform>(totem);
	transform.position.y = totemComponent.restY + village_totem::RiseOf(share);
	if (auto* icon = registry.TryGet<Transform>(totemComponent.icon))
	{
		icon->position = transform.position + glm::vec3(0.0f, village_totem::k_IconAbovePlinth, 0.0f);
	}
}

void VillageTotemSystem::Update(float gameMilliseconds)
{
	auto& registry = _world->Entities();
	std::vector<entt::entity> totems;
	registry.Each<VillageTotem>([&totems](entt::entity totem, const VillageTotem& /*unused*/) { totems.push_back(totem); });
	for (const auto totem : totems)
	{
		auto& totemComponent = registry.Get<VillageTotem>(totem);
		// It goes with its town centre
		if (!registry.Valid(totemComponent.townCentre))
		{
			_world->SetMovingSound(totem, false);
			if (registry.Valid(totemComponent.icon))
			{
				registry.Destroy(totemComponent.icon);
			}
			registry.Destroy(totem);
			continue;
		}
		village_totem::Step(totemComponent.ease, gameMilliseconds);
		// Come to its share, its moving sound stops and the village bell rings
		if (village_totem::TakeArrival(totemComponent.ease))
		{
			_world->SetMovingSound(totem, false);
			_world->RingBell(registry.Get<const Transform>(totem).position);
		}
		Place(totem);
	}
	if (_gripped != entt::null && !registry.Valid(_gripped))
	{
		_gripped = entt::null;
	}
}

std::optional<entt::entity> VillageTotemSystem::TotemOf(entt::entity picked) const
{
	auto& registry = _world->Entities();
	if (!registry.Valid(picked))
	{
		return std::nullopt;
	}
	if (registry.AllOf<VillageTotem>(picked))
	{
		return picked;
	}
	std::optional<entt::entity> found;
	registry.Each<const VillageTotem>([picked, &found](entt::entity totem, const VillageTotem& totemComponent) {
		if (totemComponent.icon == picked)
		{
			found = totem;
		}
	});
	return found;
}

bool VillageTotemSystem::Grip(entt::entity totem, PlayerNames player)
{
	auto& registry = _world->Entities();
	auto* totemComponent = registry.TryGet<VillageTotem>(totem);
	const auto town = TownOf(totem);
	if (totemComponent == nullptr || town == entt::null)
	{
		return false;
	}
	const auto& townComponent = registry.Get<const Town>(town);
	// Only a player's own town's, standing and working, with their temple and the town's worship site built
	if (townComponent.owner != player || !_world->Built(totemComponent->townCentre) || !_world->TempleBuilt(player) ||
	    !registry.Valid(townComponent.worshipSite) || !_world->Built(townComponent.worshipSite))
	{
		return false;
	}
	// The hand takes it at the share it was last set to
	totemComponent->held = totemComponent->ease.target;
	totemComponent->gripped = true;
	_gripped = totem;
	Place(totem);
	return true;
}

float VillageTotemSystem::GripY(entt::entity totem) const
{
	const auto& registry = _world->Entities();
	const auto& totemComponent = registry.Get<const VillageTotem>(totem);
	const auto* icon = registry.TryGet<const Transform>(totemComponent.icon);
	const float iconY = icon != nullptr ? icon->position.y : registry.Get<const Transform>(totem).position.y;
	return village_totem::GripY(iconY, _world->SizeOf(totemComponent.icon).height);
}

void VillageTotemSystem::Slide(float upPixels, float screenHeight)
{
	if (_gripped == entt::null)
	{
		return;
	}
	auto& registry = _world->Entities();
	auto& totemComponent = registry.Get<VillageTotem>(_gripped);
	const auto& position = registry.Get<const Transform>(_gripped).position;
	const float handAboveLand = GripY(_gripped) - _world->LandHeightAt(glm::vec2(position.x, position.z));
	const float slide =
	    village_totem::HandSlide(upPixels, screenHeight, _world->SizeOf(totemComponent.icon).height, handAboveLand);
	totemComponent.held = village_totem::SlideShare(totemComponent.held, slide);
	Place(_gripped);
}

void VillageTotemSystem::LetGo()
{
	if (_gripped == entt::null)
	{
		return;
	}
	const auto totem = _gripped;
	_gripped = entt::null;
	auto& registry = _world->Entities();
	auto& totemComponent = registry.Get<VillageTotem>(totem);
	totemComponent.gripped = false;
	const float share = totemComponent.held;
	// Where it was left becomes the town's share, which a town without a worship site refuses
	if (const auto town = TownOf(totem); town != entt::null)
	{
		SetTownShare(town, share);
	}
	else
	{
		SetTotemShare(totem, share);
	}
	totemComponent.held = totemComponent.ease.target;
	Place(totem);
}

std::optional<entt::entity> VillageTotemSystem::GetGripped() const
{
	return _gripped != entt::null ? std::optional(_gripped) : std::nullopt;
}

std::optional<VillageTotemSystemInterface::HandHold> VillageTotemSystem::GetHandHold() const
{
	if (_gripped == entt::null)
	{
		return std::nullopt;
	}
	const auto& registry = _world->Entities();
	const auto& totemComponent = registry.Get<const VillageTotem>(_gripped);
	const auto& position = registry.Get<const Transform>(_gripped).position;
	const float y = GripY(_gripped);
	return HandHold {
	    .position = {position.x, y, position.z},
	    .tilt = village_totem::HandTiltAt(y),
	    .closure = village_totem::GripClosure(_world->SizeOf(totemComponent.icon).radius),
	};
}
