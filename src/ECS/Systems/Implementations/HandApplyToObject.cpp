/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The held object applied to the object under the hand (the action press while holding an object that is not a spell
// seed, the ApplyToObject packet, its handler and HandleApplyResult), the per-class check and apply for the held
// classes that are ported (a villager dropped into a teleport stone; a tree given to a store, ecs::held_apply), and
// the pick-up of a spell seed or a teleport stone into the magic hand. Wiki: docs/bw1-notes/miracles.md, "Teleport"
// and "Forest"; docs/bw1-notes/hand-and-interface.md, "Giving a held object".

#define LOCATOR_IMPLEMENTATIONS

#include <cstdlib>

#include <spdlog/spdlog.h>

#include "Debug/DebugEnv.h"
#include "ECS/Components/HandDrawPose.h"
#include "ECS/Components/MagicTeleport.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/HeldApply.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "GameClock.h"
#include "HandSystem.h"
#include "HandSystemDetail.h"
#include "Help/HelpProfile.h"
#include "Input/GamePackets.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellSeed.h"
#include "Magic/Gestures/PowerUpSystem.h"
#include "Magic/Objects/MagicTeleport.h"
#include "Worship/InterfaceStatus.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
namespace teleport = magic::teleport;

/// The apply results (HandleApplyResult)
constexpr int k_ResultConsumed = 3;
constexpr int k_ResultNothing = 5;
constexpr int k_ResultRemoved = 0x16;
constexpr int k_ResultPlaced = 0x17;
constexpr int k_ResultRemovedOnly = 0x18;

bool ApplyTrace()
{
	static const bool trace = debug_env::HandTrace() || debug_env::TeleportTrace() || debug_env::SpellTrace();
	return trace;
}

/// A villager applies to a worship totem, or to a MagicTeleport that takes villagers directly; nothing else.
/// (pending) TODO(worship): the worship totem branch (the sacrifice) is not ported: here a totem does not take the
/// villager, and the press arms the put down as before
bool VillagerValidToApplyThisToObject(entt::entity villager, entt::entity target)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<MagicTeleport>(target))
	{
		return teleport::ValidToApplyVillagerDirectly(target, villager);
	}
	return false;
}

/// Whether the held object can be applied to the target. A plain object cannot; of the other held classes the
/// Villager is ported here, the classes a store takes in ecs::held_apply (the spell seed's is in HandSpellSeed.cpp)
bool ValidToApplyThisToObject(entt::entity held, entt::entity target)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<Villager>(held))
	{
		return VillagerValidToApplyThisToObject(held, target);
	}
	return ecs::held_apply::ValidToApplyThisToObject(held, target);
}
} // namespace

entt::entity HandSystem::SeedToPlaceInHand(entt::entity object) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!ecs::IsAvailable(object))
	{
		return entt::null;
	}
	if (registry.AllOf<SpellSeed>(object))
	{
		// the seed's own check, with the local hand's player
		return magic::seed::ValidForPlaceInHand(object, PlayerNames::PLAYER_ONE) ? object : entt::null;
	}
	if (registry.AllOf<MagicTeleport>(object))
	{
		// the stone's spell's seed, then the seed's own check; none without one
		const auto seed = teleport::SeedOf(object);
		return seed != entt::null && magic::seed::ValidForPlaceInHand(seed, PlayerNames::PLAYER_ONE) ? seed : entt::null;
	}
	return entt::null;
}

bool HandSystem::SendSeedOrStonePickup(entt::entity object, bool inInfluence) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!ecs::IsAvailable(object) || !registry.AnyOf<SpellSeed, MagicTeleport>(object))
	{
		return false;
	}
	// valid to place in the hand, and in the influence: both must be in the influence to interact
	const auto seed = SeedToPlaceInHand(object);
	const bool stone = registry.AllOf<MagicTeleport>(object);
	if (seed == entt::null || !inInfluence)
	{
		return true;
	}
	// (after the packet) neither a tree nor a forest, so the sound tag 10 G_PickUpObject at the object's MapCoords (not
	// tracked, mode 3, no loops, 3D, InGame, no delay), as HandHolding.cpp PickUp does. The point is taken before the
	// stone goes. (approximate) a seed's MapCoords altitude is not moved by its draw (only its 3D object's matrix);
	// openblack keeps only the drawn point in its Transform: that one
	if (const auto* transform = registry.TryGet<const Transform>(object); transform != nullptr)
	{
		const auto at = stone ? teleport::MapPositionOf(object) : magic::ToMap(transform->position);
		audio::tags::CreateAtMapCoords(at.x, at.z, at.y, 10, false, 3, 0, false, true, audio::SfxBank::InGame, 0);
	}
	// the PlaceInHand packet with the object itself (the stone, not its seed: the handler finds the seed,
	// ApplyPlaceInHand), then action state 7
	game_packets::Push({game_packets::Type::PlaceInHand, object});
	_pickPressHeld = true;
	if (ApplyTrace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand: pick-up of {} {} sent (seed {})",
		                   stone ? "teleport stone" : "spell seed", static_cast<uint32_t>(object), static_cast<uint32_t>(seed));
	}
	return true;
}

int HandSystem::HandleApplyResult(int result, entt::entity held, std::optional<glm::vec3> position) noexcept
{
	// 5 -> 0, 3 -> 3; any other but 1 only while `held` is still the hand's first object, else the result as it is
	if (result == k_ResultNothing)
	{
		return 0;
	}
	if (result == k_ResultConsumed || (result != 1 && !(_held && *_held == held)))
	{
		return result;
	}
	// its reactions available again
	ecs::effects::reactions::SetAvailable(held, true);
	auto& registry = Locator::entitiesRegistry::value();
	if (result == k_ResultRemoved)
	{
		// 0 when the object it let go is not `held`
		const auto removed = RemoveFirstFromHand();
		return removed && *removed == held ? result : 0;
	}
	if (result == k_ResultPlaced)
	{
		if (!position)
		{
			return 0; // no position
		}
		// RemoveFirstFromHand; when that was `held`, it is put at pos and back in the map
		if (const auto removed = RemoveFirstFromHand(); removed && *removed == held && registry.Valid(held))
		{
			if (auto* transform = registry.TryGet<Transform>(held); transform != nullptr)
			{
				transform->position = *position;
				registry.SetDirty();
			}
			if (registry.AllOf<HandDrawPose>(held))
			{
				registry.Remove<HandDrawPose>(held); // in the map again: drawn where it is
			}
		}
		return result;
	}
	if (result == k_ResultRemovedOnly && _held)
	{
		// only the interface status's magic hand lets it go (the gesture buffer cleared), not the drawn hand (no
		// RenderHandRelease, RemoveFirstFromHand's part), which goes on drawing it. (inferred) no ported class returns
		// 0x18
		const auto entity = *_held;
		_held.reset();
		_pickSource.reset();
		_releaseArmed = false;
		magic::gestures::ClearBuffer();
		if (registry.Valid(entity))
		{
			ecs::fire::SetOutMagicHand(entity);
		}
	}
	return result;
}

std::optional<entt::entity> HandSystem::RemoveFirstFromHand() noexcept
{
	// the interface status's magic hand lets the object go (fire::SetOutMagicHand), and the local hand empties
	// (RenderHandRelease: no physics)
	if (!_held)
	{
		return std::nullopt;
	}
	const auto entity = *_held;
	_held.reset();
	_pickSource.reset();
	_releaseArmed = false;
	// the local interface's gesture buffer cleared (count, head, stationary and the samples)
	magic::gestures::ClearBuffer();
	RenderHandRelease();
	if (Locator::entitiesRegistry::value().Valid(entity))
	{
		ecs::fire::SetOutMagicHand(entity);
	}
	return entity;
}

bool HandSystem::HeldValidToApplyTo(entt::entity target) const noexcept
{
	// whether the held object can be applied to `target`, for the tooltips
	return _held && *_held != target && ValidToApplyThisToObject(*_held, target);
}

bool HandSystem::HeldActionPressedOnObject(bool inInfluence) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!_held || !Interactable(*_held))
	{
		return false;
	}
	const auto held = *_held;
	// the creature to give to (no creature yet), else the object under the hand
	const auto target = _cursorObject && Interactable(*_cursorObject) ? *_cursorObject : entt::null;
	// any object is a valid interface target. Giving an object needs a creature. Out of the influence, for an
	// object that must be in the influence to interact (all of them): the branch without a target
	if (target == entt::null || target == held || !inInfluence)
	{
		return false;
	}
	// the held object must be valid to apply to the target; else (not a seed) apply only after release / fail apply,
	// the put down that the press arms here
	if (!ValidToApplyThisToObject(held, target))
	{
		return false;
	}
	// a villager neither waits for the gesture system nor locks the apply, so the apply is sent: the hand holds
	// something, a valid target, the validity again, not a seed: the ApplyToObject packet and action state 0x12. The
	// next turn's start applies it (ApplyHeldToObject)
	// with an apply packet still waiting, no new packet (only the action state); else the send turn is noted after it
	if (_applySentTurn == 0)
	{
		game_packets::Push({game_packets::Type::ApplyToObject, target, registry.Get<const Transform>(target).position});
		_applySentTurn = game_clock::Turn();
	}
	_releaseArmed = false;
	return true;
}

void HandSystem::ApplyHeldToObject(entt::entity target) noexcept
{
	// the ApplyToObject packet: the target interactable, the hand holding, its first object interactable and valid to
	// apply to the target; then the held object's apply. A target gone or the hand empty: the action ends (the action
	// state is already reset here). (not verified) the first step of the handler
	if (!_held || !Interactable(*_held) || !Interactable(target))
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto held = *_held;
	if (held == target || !ValidToApplyThisToObject(held, target))
	{
		return;
	}
	int result = 0;
	const glm::vec3 targetPosition = registry.Get<const Transform>(target).position;
	if (registry.AllOf<Villager>(held) && registry.AllOf<MagicTeleport>(target))
	{
		// a villager onto a MagicTeleport that takes villagers directly: FLYING, out of the hand and put at the stone's
		// MapCoords, LANDED, decide what to do, its destination registered and a forced teleport. 1 when it jumped, else
		// 0x17. Not valid any more -> 0.
		if (teleport::ValidToApplyVillagerDirectly(target, held))
		{
			RemoveFirstFromHand(); // ApplyVillagerDirectly puts it at the stone (LandAt)
			result = teleport::ApplyVillagerDirectly(target, held);
		}
	}
	else if (ecs::held_apply::KindOfHeld(held) != ecs::held_apply::HeldKind::Other)
	{
		// a class a store takes: the store under the hand takes it (3), else nothing (0, still held). Once it is sure
		// to go it leaves the hand as a release does, then an uprooted tree drops its roots
		result = ecs::held_apply::ApplyThisToObject(held, target, InterfaceStatus(), [this, held]() {
			_held.reset();
			_pickSource.reset();
			_releaseArmed = false;
			RenderHandRelease();
			ecs::fire::SetOutMagicHand(held);
			DropRoots(held, false);
		});
	}
	// the help event 7 for a sacrifice altar, else 6, for the local interface. (pending) the altar test: no sacrifice
	// altar in openblack, so 6
	help_profile::Trigger(help_profile::Event::Supply);
	result = HandleApplyResult(result, held, targetPosition);
	if (ApplyTrace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand: applied {} to object {}: result {:#x}, still held {}",
		                   static_cast<uint32_t>(held), static_cast<uint32_t>(target), result, _held.has_value());
	}
}
