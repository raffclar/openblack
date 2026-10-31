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

#include <cstdint>

#include <algorithm>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/gtx/euler_angles.hpp>

#include "Common/GUtilsAngle.h"
#include "Common/PortableRotations.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/DrawnHeight.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/SeeThrough.h"
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

void VillageTotemSystem::MakeParts(entt::entity totem)
{
	// The turn makes the see-through second plinth and icon with the totem, so that every computer has them. The frames
	// only move them, and show them while the totem's two shares stand apart.
	auto& registry = _world->Entities();
	const auto makeGhost = [&registry](entt::entity& ghost, entt::entity of) {
		if (registry.Valid(ghost) || !registry.Valid(of))
		{
			return;
		}
		// Copied out first: adding to a pool may move what is in it
		const Transform transform = registry.Get<const Transform>(of);
		const auto* mesh = registry.TryGet<const Mesh>(of);
		const std::optional<Mesh> drawn = mesh != nullptr ? std::optional(*mesh) : std::nullopt;
		ghost = registry.Create();
		registry.Assign<Transform>(ghost, transform);
		if (drawn.has_value())
		{
			registry.Assign<Mesh>(ghost, *drawn);
		}
		registry.Assign<SeeThrough>(ghost, village_totem::k_GhostAlpha);
		registry.Assign<VillageTotemGhost>(ghost);
		registry.Assign<VillageTotemGhostHidden>(ghost);
	};
	auto ghost = registry.Get<const VillageTotem>(totem).ghost;
	const auto icon = registry.Get<const VillageTotem>(totem).icon;
	makeGhost(ghost, totem);
	auto ghostIcon = registry.Get<const VillageTotem>(totem).ghostIcon;
	makeGhost(ghostIcon, icon);
	auto& totemComponent = registry.Get<VillageTotem>(totem);
	totemComponent.ghost = ghost;
	totemComponent.ghostIcon = ghostIcon;
	if (registry.Valid(icon) && !registry.AllOf<VillageTotemIcon>(icon))
	{
		registry.Assign<VillageTotemIcon>(icon);
	}
}

void VillageTotemSystem::AddToPlayer(entt::entity totem)
{
	auto& registry = _world->Entities();
	if (!registry.AllOf<VillageTotem>(totem))
	{
		return;
	}
	MakeParts(totem);
	auto* totemComponent = registry.TryGet<VillageTotem>(totem);
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
		const auto turned = glm::mat3(portable_math::EulerAngleY(-angle));
		const bool changed = turned != transform.rotation;
		transform.rotation = turned;
		if (auto* iconTransform = registry.TryGet<Transform>(totemComponent->icon))
		{
			iconTransform->rotation = transform.rotation;
		}
		// A totem that turns goes out of the map's cells and in again
		if (changed)
		{
			_world->Reinsert(totem);
		}
	}
	ShowIcon(*totemComponent, _world->IconMeshFor(townComponent.owner));
	// The new icon gives the totem a new outline, so it goes out of the map's cells and in again
	_world->Reinsert(totem);
	totemComponent->lastShownRise.reset();
	totemComponent->quietOnce = true;
	SetTownShare(town, 0.0f);
}

void VillageTotemSystem::ShowIcon(const VillageTotem& totem, entt::id_type iconMesh)
{
	// The player's creature stands on top, or the hand, on the second totem too. It is drawn afresh where it stands,
	// which shows no tooltip.
	auto& registry = _world->Entities();
	for (const auto icon : {totem.icon, totem.ghostIcon})
	{
		if (auto* mesh = registry.Valid(icon) ? registry.TryGet<Mesh>(icon) : nullptr)
		{
			mesh->id = iconMesh;
		}
	}
}

void VillageTotemSystem::RefreshIcons(PlayerNames player)
{
	auto& registry = _world->Entities();
	const auto iconMesh = _world->IconMeshFor(player);
	// The totems of the player's towns, in the order that the player gained the towns
	std::vector<std::pair<uint32_t, entt::entity>> totems;
	registry.Each<const VillageTotem>([&](entt::entity totem, const VillageTotem& /*unused*/) {
		const auto town = TownOf(totem);
		const auto* townComponent = town != entt::null ? registry.TryGet<const Town>(town) : nullptr;
		if (townComponent != nullptr && townComponent->owner == player)
		{
			totems.emplace_back(townComponent->gained, totem);
		}
	});
	std::ranges::stable_sort(totems, {}, &std::pair<uint32_t, entt::entity>::first);
	for (const auto& entry : totems)
	{
		ShowIcon(registry.Get<const VillageTotem>(entry.second), iconMesh);
		// The new icon gives the totem a new outline, so it goes out of the map's cells and in again
		_world->Reinsert(entry.second);
	}
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
	// The town sends people to worship, or calls them back, to match its new share
	_world->ShareSet(town);
}

void VillageTotemSystem::Place(entt::entity totem)
{
	auto& registry = _world->Entities();
	const auto& totemComponent = registry.Get<const VillageTotem>(totem);
	// Held by the hand, it stands where the hand holds it; otherwise where it has eased to. It eases by the computer's
	// clock, so only its drawn height moves: the turns go by where it stands on the land.
	const float share = totemComponent.gripped ? totemComponent.held : totemComponent.ease.share;
	const float y = totemComponent.restY + village_totem::RiseOf(share);
	const auto setHeight = [&registry](entt::entity thing, float height) {
		if (auto* drawn = registry.TryGet<DrawnHeight>(thing); drawn != nullptr)
		{
			drawn->y = height;
		}
		else
		{
			registry.Assign<DrawnHeight>(thing, height);
		}
	};
	setHeight(totem, y);
	if (registry.Valid(totemComponent.icon) && registry.AllOf<Transform>(totemComponent.icon))
	{
		setHeight(totemComponent.icon, y + village_totem::k_IconAbovePlinth);
	}
}

void VillageTotemSystem::PlaceGhost(entt::entity totem, std::optional<float> share)
{
	// The frames only move and show the second plinth and its icon, which the turn made with the totem. They make and
	// remove no entity.
	auto& registry = _world->Entities();
	const auto& totemComponent = registry.Get<const VillageTotem>(totem);
	const auto show = [&registry, &share](entt::entity ghost) {
		if (!registry.Valid(ghost))
		{
			return false;
		}
		const bool hidden = registry.AllOf<VillageTotemGhostHidden>(ghost);
		if (share.has_value() && hidden)
		{
			registry.Remove<VillageTotemGhostHidden>(ghost);
		}
		else if (!share.has_value() && !hidden)
		{
			registry.Assign<VillageTotemGhostHidden>(ghost);
		}
		return share.has_value();
	};
	if (!show(totemComponent.ghost))
	{
		show(totemComponent.ghostIcon);
		return;
	}
	auto& transform = registry.Get<Transform>(totemComponent.ghost);
	transform = registry.Get<const Transform>(totem);
	transform.position.y = totemComponent.restY + village_totem::RiseOf(*share);
	if (show(totemComponent.ghostIcon))
	{
		auto& icon = registry.Get<Transform>(totemComponent.ghostIcon);
		const auto* solidIcon =
		    registry.Valid(totemComponent.icon) ? registry.TryGet<const Transform>(totemComponent.icon) : nullptr;
		if (solidIcon != nullptr)
		{
			icon = *solidIcon;
		}
		icon.position = transform.position + glm::vec3(0.0f, village_totem::k_IconAbovePlinth, 0.0f);
	}
}

void VillageTotemSystem::NoteToolTip(entt::entity totem, const village_totem::Shown& shown)
{
	auto& totemComponent = _world->Entities().Get<VillageTotem>(totem);
	const float rise = village_totem::RiseOf(shown.solid);
	if (totemComponent.lastShownRise == rise && !shown.ghost.has_value())
	{
		return;
	}
	// The town's player sees the share the totem is held at, unless it was only just given its icon
	if (const auto town = TownOf(totem); !totemComponent.quietOnce && town != entt::null)
	{
		_toolTips[_world->Entities().Get<const Town>(town).owner] =
		    ShareToolTip {.totem = totem, .percent = village_totem::AsPercentage(totemComponent.held)};
	}
	totemComponent.quietOnce = false;
	totemComponent.lastShownRise = rise;
}

void VillageTotemSystem::Remove(entt::entity totem)
{
	auto& registry = _world->Entities();
	_world->SetMovingSound(totem, false);
	const auto& totemComponent = registry.Get<const VillageTotem>(totem);
	for (const auto part : {totemComponent.icon, totemComponent.ghost, totemComponent.ghostIcon})
	{
		if (registry.Valid(part))
		{
			registry.Destroy(part);
		}
	}
	registry.Destroy(totem);
}

void VillageTotemSystem::Update(uint32_t realMs)
{
	auto& registry = _world->Entities();
	std::vector<entt::entity> totems;
	registry.Each<VillageTotem>([&totems](entt::entity totem, const VillageTotem& /*unused*/) { totems.push_back(totem); });
	for (const auto totem : totems)
	{
		auto& totemComponent = registry.Get<VillageTotem>(totem);
		// A totem whose town centre went goes at the next turn
		if (!registry.Valid(totemComponent.townCentre))
		{
			continue;
		}
		village_totem::StepTo(totemComponent.ease, realMs);
		Place(totem);
		// The second totem shows the other share, and the player sees the share as it moves
		const auto shown = village_totem::ShownShares(totemComponent.ease.share, totemComponent.held, totemComponent.gripped);
		PlaceGhost(totem, shown.ghost);
		NoteToolTip(totem, shown);
	}
	if (_gripped != entt::null && !registry.Valid(_gripped))
	{
		_gripped = entt::null;
	}
}

void VillageTotemSystem::ProcessTurn(uint32_t realMs)
{
	auto& registry = _world->Entities();
	std::vector<entt::entity> arrived;
	std::vector<entt::entity> gone;
	registry.Each<VillageTotem>([&registry, &arrived, &gone, realMs](entt::entity totem, VillageTotem& totemComponent) {
		// It goes with its town centre
		if (!registry.Valid(totemComponent.townCentre))
		{
			gone.push_back(totem);
			return;
		}
		village_totem::StepTo(totemComponent.ease, realMs);
		if (village_totem::TakeArrival(totemComponent.ease))
		{
			arrived.push_back(totem);
		}
	});
	// A totem at its share stops the sound of its movement and rings the village bell
	for (const auto totem : arrived)
	{
		_world->SetMovingSound(totem, false);
		_world->RingBell(registry.Get<const Transform>(totem).position);
	}
	for (const auto totem : gone)
	{
		Remove(totem);
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

std::string_view VillageTotemSystem::WhyNotGrip(entt::entity totem, PlayerNames player) const
{
	const auto& registry = _world->Entities();
	const auto* totemComponent = registry.Valid(totem) ? registry.TryGet<const VillageTotem>(totem) : nullptr;
	const auto town = totemComponent != nullptr ? TownOf(totem) : entt::null;
	if (town == entt::null)
	{
		return "the totem has no town";
	}
	const auto& townComponent = registry.Get<const Town>(town);
	if (townComponent.owner != player)
	{
		return "the totem's town is not the player's";
	}
	// The totem stands built when its town centre stands built
	if (!village_totem::Functional(_world->Built(totemComponent->townCentre), _world->LifeOf(totem)))
	{
		return "the totem does not work: it is not built, or it has three quarters of its life or less";
	}
	if (!_world->HeartBuilt(player))
	{
		return "the heart of the player's temple is not built";
	}
	if (!registry.Valid(townComponent.worshipSite) || !_world->Built(townComponent.worshipSite))
	{
		return "the town's worship site is not built";
	}
	return {};
}

bool VillageTotemSystem::Grip(entt::entity totem, PlayerNames player)
{
	// Only a totem of the player's own town that works, with the heart of their temple and the town's worship site
	// built
	if (!WhyNotGrip(totem, player).empty())
	{
		return false;
	}
	auto& totemComponent = _world->Entities().Get<VillageTotem>(totem);
	// The hand takes it at the share it was last set to
	totemComponent.held = totemComponent.ease.target;
	totemComponent.gripped = true;
	_gripped = totem;
	Place(totem);
	// The world sees the hand take hold at the start of a later turn
	if (!_world->SendCommand(player_commands::HandLock {.object = totem}))
	{
		DoGrip(player, totem);
	}
	return true;
}

void VillageTotemSystem::DoGrip(PlayerNames player, entt::entity totem)
{
	if (!_world->Entities().Valid(totem) || !_world->Entities().AllOf<VillageTotem>(totem))
	{
		return;
	}
	// The player's creature sees the hand take the totem first, then the living near it react to the hand using it
	_world->SeeHandTakeTotem(player);
	_world->ReactToHandUsingTotem(totem, player, StandingAt(totem));
}

glm::vec3 VillageTotemSystem::StandingAt(entt::entity totem) const
{
	const auto& registry = _world->Entities();
	const auto& position = registry.Get<const Transform>(totem).position;
	return {position.x, registry.Get<const VillageTotem>(totem).restY, position.z};
}

float VillageTotemSystem::GripY(entt::entity totem) const
{
	const auto& registry = _world->Entities();
	const auto& totemComponent = registry.Get<const VillageTotem>(totem);
	// The hand holds the icon where the frame draws it
	const auto heightOf = [&registry](entt::entity thing) -> std::optional<float> {
		if (const auto* drawn = registry.Valid(thing) ? registry.TryGet<const DrawnHeight>(thing) : nullptr)
		{
			return drawn->y;
		}
		const auto* transform = registry.Valid(thing) ? registry.TryGet<const Transform>(thing) : nullptr;
		return transform != nullptr ? std::optional(transform->position.y) : std::nullopt;
	};
	const float iconY = heightOf(totemComponent.icon).value_or(heightOf(totem).value_or(totemComponent.restY));
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
	auto& totemComponent = _world->Entities().Get<VillageTotem>(totem);
	totemComponent.gripped = false;
	const float share = totemComponent.held;
	// The town takes the share at the start of a later turn; until then the totem eases to the share it had
	totemComponent.held = totemComponent.ease.target;
	Place(totem);
	const player_commands::TotemShare command {.totem = totem, .share = share};
	if (!_world->SendCommand(command))
	{
		DoShare(PlayerNames::NEUTRAL, totem, share);
		return;
	}
	_world->SendCommand(player_commands::HandUnlock {.object = totem});
}

void VillageTotemSystem::RaiseByGod(PlayerNames player, entt::entity totem, float share)
{
	auto& registry = _world->Entities();
	if (!registry.Valid(totem) || !registry.AllOf<VillageTotem>(totem))
	{
		return;
	}
	DoGrip(player, totem);
	// The creature of the town's player feels the raise also when the share is none
	if (const auto town = TownOf(totem); town != entt::null)
	{
		_world->EmpathiseWithPlayer(registry.Get<const Town>(town).owner, StandingAt(totem));
		SetTownShare(town, share);
	}
	else
	{
		SetTotemShare(totem, share);
	}
	KeepShareShown(totem);
}

void VillageTotemSystem::KeepShareShown(entt::entity totem)
{
	auto& totemComponent = _world->Entities().Get<VillageTotem>(totem);
	// The hand of this computer can hold the totem meanwhile: then the totem stays at the share of the hand
	if (!totemComponent.gripped)
	{
		totemComponent.held = totemComponent.ease.target;
	}
	Place(totem);
}

void VillageTotemSystem::DoShare([[maybe_unused]] PlayerNames player, entt::entity totem, float share)
{
	auto& registry = _world->Entities();
	if (!registry.Valid(totem) || !registry.AllOf<VillageTotem>(totem))
	{
		return;
	}
	auto& totemComponent = registry.Get<VillageTotem>(totem);
	const auto at = StandingAt(totem);
	// Where it was left becomes the town's share, which a town without a worship site refuses. Raised from none at all,
	// the town's player's creature takes it the player wants to impress.
	if (const auto town = TownOf(totem); town != entt::null)
	{
		if (share > 0.0f)
		{
			_world->EmpathiseWithPlayer(registry.Get<const Town>(town).owner, at);
		}
		SetTownShare(town, share);
	}
	else
	{
		SetTotemShare(totem, share);
	}
	// The hand of this computer can hold it again meanwhile
	if (!totemComponent.gripped)
	{
		totemComponent.held = totemComponent.ease.target;
	}
	Place(totem);
	// The share it was left at floats up from it in grey
	_world->FloatNumber(at, village_totem::AsPercentage(share), village_totem::k_LetGoNumberColour);
}

std::optional<VillageTotemSystemInterface::ShareToolTip> VillageTotemSystem::TakeShareToolTip(PlayerNames player)
{
	const auto found = _toolTips.find(player);
	if (found == _toolTips.end())
	{
		return std::nullopt;
	}
	const auto toolTip = found->second;
	_toolTips.erase(found);
	return toolTip;
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
	    .closure = village_totem::GripClosure(_world->SizeOf(totemComponent.icon).height),
	};
}
