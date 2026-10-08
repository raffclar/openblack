/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The hand's packets (sent by the interface, applied at the start of the next turn) and the hand's per-turn pass.

#define LOCATOR_IMPLEMENTATIONS

#include <utility>

#include <glm/gtc/constants.hpp>

#include "3D/LandMorph.h"
#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "Camera/Camera.h"
#include "Creature/CreatureHandRules.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/HandDrawPose.h"
#include "ECS/Components/MagicTeleport.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreatureHandPackets.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/HandCreature.h"
#include "ECS/Influence/Influence.h"
#include "ECS/ObjectFlags.h"
#include "ECS/ObjectResources.h"
#include "ECS/Physics/FromHand.h"
#include "ECS/Physics/ParticleCarriedObjects.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/HandTap.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "GameClock.h"
#include "HandSystem.h"
#include "HandSystemDetail.h"
#include "Help/HelpProfile.h"
#include "Input/GamePackets.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellSeed.h"
#include "Particles/PSysManager.h"
#include "Worship/InterfaceStatus.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// The player a creature belongs to, through the const registry; nobody for anything else
PlayerNames CreatureOwnerForHand(entt::entity object)
{
	const auto* creature = std::as_const(Locator::entitiesRegistry::value()).TryGet<const Creature>(object);
	return creature != nullptr ? creature->owner : PlayerNames::NEUTRAL;
}
} // namespace

HandSystem::~HandSystem()
{
	game_packets::Reset();
	game_packets::ClearHandlers();
}

void HandSystem::RegisterPacketHandlers() noexcept
{
	using game_packets::Packet;
	using game_packets::Type;
	// Tap: the object still interactable and valid to tap, then it is tapped
	game_packets::SetHandler(Type::Tap, [this](const Packet& packet) { ApplyTap(packet.object); });
	// The synced MapCoords and hand, the synced camera, and the ping of the local interface (ReceivePing). (not ported)
	// a few copies of the sync fields that are only saved (no reader found)
	game_packets::SetHandler(Type::HandAndCamera, [this](const Packet& packet) {
		ReceivePing(packet.value);
		_syncMapCoords = packet.coords;
		_syncHand = glm::vec3(packet.data[0], packet.data[1], packet.data[2]);
		_syncCameraPosition = glm::vec3(packet.data[3], packet.data[4], packet.data[5]);
		_syncCameraFocus = glm::vec3(packet.data[6], packet.data[7], packet.data[8]);
	});
	game_packets::SetHandler(Type::Hand, [this](const Packet& packet) {
		ReceivePing(packet.value);
		_syncMapCoords = packet.coords;
		_syncHand = glm::vec3(packet.data[0], packet.data[1], packet.data[2]);
	});
	game_packets::SetHandler(Type::Camera, [this](const Packet& packet) {
		ReceivePing(packet.value);
		_syncCameraPosition = glm::vec3(packet.data[3], packet.data[4], packet.data[5]);
		_syncCameraFocus = glm::vec3(packet.data[6], packet.data[7], packet.data[8]);
	});
	// The throw data: velocity, angular velocity, position and YXZ angles: the throw that the next apply-to-map-coord,
	// throw-held or apply-to-object packet uses
	game_packets::SetHandler(Type::ThrowData, [this](const Packet& packet) {
		_statusThrowVelocity = glm::vec3(packet.data[0], packet.data[1], packet.data[2]);
		_statusThrowAngular = glm::vec3(packet.data[3], packet.data[4], packet.data[5]);
		_statusThrowHandPosition = glm::vec3(packet.data[6], packet.data[7], packet.data[8]);
		_statusThrowAngles = glm::vec3(packet.data[9], packet.data[10], packet.data[11]);
	});
	// Apply to a map coord: an empty hand ends the action; the held object must be available and accept a map coord
	// (every holdable class but the seed does), else nothing; then it is applied at the packet's MapCoords (the throw
	// with the throw data) and the result is handled. (not identified) one more call on the status
	game_packets::SetHandler(Type::ApplyToMapCoord, [this](const Packet& packet) {
		_applyHandledTurn = game_clock::Turn();
		if (!_held)
		{
			EndAction();
			return;
		}
		if (!ecs::IsAvailable(*_held))
		{
			return;
		}
		if (IsHoldingSeed())
		{
			ApplySeedToMapCoord(packet);
			return;
		}
		const auto held = *_held;
		HandleApplyResult(Release(_statusThrowVelocity, packet.position, true), held, packet.position);
	});
	// Place in hand: the checks again, then the object goes into the hand; a refusal or no object ends the action
	game_packets::SetHandler(Type::PlaceInHand, [this](const Packet& packet) { ApplyPlaceInHand(packet.object); });
	// Apply to object: the held object is applied to the target (a seed's: HandSpellSeed.cpp); no target ends the
	// action
	game_packets::SetHandler(Type::ApplyToObject, [this](const Packet& packet) {
		_applyHandledTurn = game_clock::Turn();
		if (packet.object == entt::null)
		{
			EndAction();
		}
		else if (IsHoldingSeed())
		{
			ApplySeedToObject(packet);
		}
		else
		{
			ApplyHeldToObject(packet.object);
		}
	});
	// The locked select's start and end
	game_packets::SetHandler(Type::StartLockedSelect, [this](const Packet& packet) { ApplyStartLockedSelect(packet.object); });
	game_packets::SetHandler(Type::EndLockedSelect, [this](const Packet& packet) { ApplyEndLockedSelect(packet.object); });
	// The creature the hand let go of: how it was treated reaches its mind; and a click on the player's own creature
	// works their leash key
	game_packets::SetHandler(Type::CreatureFeedback, [](const Packet& packet) {
		ecs::creature_hand_packets::ApplyFeedback(
		    Locator::creatureMindSystem::has_value() ? &Locator::creatureMindSystem::value() : nullptr, packet);
	});
	game_packets::SetHandler(Type::CreatureLeashClick, [](const Packet&) {
		ecs::creature_hand_packets::ApplyLeashClick(
		    Locator::leashSystem::has_value() ? &Locator::leashSystem::value() : nullptr, PlayerNames::PLAYER_ONE);
	});
	// Throw held: the held object is thrown from the hand with the throw data's zero velocity
	game_packets::SetHandler(Type::ThrowHeld, [this](const Packet&) { ApplyForceDropHeld(); });
	// A particle spot visual at the position
	game_packets::SetHandler(Type::SpotVisual, [](const Packet& packet) {
		psys::manager::CreateSpotVisual(packet.value, packet.position, 0.0f, entt::null);
	});
	// Release impulse: the released object's body (none: nothing) spins with the hand's move after the release (the
	// spring velocity is not read)
	game_packets::SetHandler(Type::ReleaseImpulse, [](const Packet& packet) {
		physics::from_hand::ApplyReleaseSpin(packet.object, glm::vec3(packet.data[0], packet.data[1], packet.data[2]));
	});
}

bool HandSystem::Interactable(entt::entity object) noexcept
{
	// Available; an abode also needs to be partly built
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !ecs::IsAvailable(object))
	{
		return false;
	}
	return !registry.AllOf<Abode>(object) || abodes::GetPercentBuilt(object) != 0.0f;
}

void HandSystem::PushThrowData(glm::vec3 velocity, bool predict) noexcept
{
	// The throw data: the velocity, the angular momentum, the position and the YXZ angles.
	// - predicting (state 12): from a prediction body set up at the held Transform (L = h M8, the velocity capped at
	//   124, adjusted to the ground, run for min(ping / 100, 5) turns): its velocity, L, origin and YXZ angles:
	//   from_hand::PredictRelease; no body (an openblack guard): the held Transform, the capped velocity, L 0
	// - ForceDropHeld (not predicting): the velocity and L zero, the held object's matrix translation and YXZ angles
	game_packets::Packet data {game_packets::Type::ThrowData};
	auto& registry = Locator::entitiesRegistry::value();
	if (!_held || !registry.Valid(*_held))
	{
		if (!predict)
		{
			game_packets::Push(data); // ForceDropHeld's own empty-hand path is in HandSpellSeed.cpp
		}
		return; // state 12: nothing without a held object
	}
	const auto held = *_held;
	// the held object's drawn matrix (its G3D), the frame's: HandDrawPose, else its Transform
	const auto& transform = registry.Get<const Transform>(held);
	const auto* pose = registry.TryGet<const HandDrawPose>(held);
	glm::vec3 angular(0.0f);
	glm::vec3 origin = pose != nullptr ? pose->position : transform.position;
	glm::mat3 rotation = pose != nullptr ? pose->rotation : transform.rotation;
	if (predict)
	{
		// the hand's angular velocity h (0 while the Holding spring works: an object, not a seed)
		const glm::vec3 handAngular = _handAngularVelocity;
		const auto turns = std::min(_pingMs / 100u, 5u); // unsigned division, signed min
		if (const auto prediction = physics::from_hand::PredictRelease(held, velocity, handAngular, turns))
		{
			velocity = prediction->velocity;
			angular = prediction->angularMomentum;
			origin = prediction->origin;
			rotation = prediction->rotation;
		}
		else
		{
			velocity = physics::from_hand::CapReleaseVelocity(velocity).velocity;
		}
	}
	float y = 0.0f;
	float x = 0.0f;
	float z = 0.0f;
	affine::DecomposeYXZ(rotation, y, x, z);
	data.data = {velocity.x, velocity.y, velocity.z, angular.x, angular.y, angular.z, origin.x, origin.y, origin.z, x, y, z};
	game_packets::Push(data);
}

int32_t HandSystem::NextPingIndex() noexcept
{
	// ring[index] = GetTickCount, the old index into the packet, index + 1 (32 entries). (approximate) the engine
	// timer's ms for GetTickCount (the same wall clock, not its 15.6 ms steps; a restart of the engine timer would wrap
	// the difference, which GetTickCount never does)
	const auto index = _pingIndex;
	_pingRing[index] = static_cast<uint32_t>(game_clock::EngineMs());
	_pingIndex = static_cast<uint8_t>((index + 1) % _pingRing.size());
	return index;
}

void HandSystem::ReceivePing(int32_t index) noexcept
{
	// For the local interface: the ping = GetTickCount - ring[the packet's ping index]; openblack's packets are all the
	// local interface's
	_pingMs = static_cast<uint32_t>(game_clock::EngineMs()) - _pingRing[static_cast<size_t>(index) % _pingRing.size()];
}

void HandSystem::SendRelease(glm::vec3 velocity) noexcept
{
	// On the release (state 12): the action's MapCoords in bounds, an object in the hand that accepts a map coord (every
	// class this sends for) and influence there: the throw data packet. Otherwise no throw data and, in HOLDING, the
	// spring stops. Both go on to the drop on the map coord: nothing while an apply packet waits; the same three
	// tests: the apply packet, or FailApply: the fail spot and sound, the object stays in the hand. (approximate) no
	// land under the cursor counts as off the map
	const auto point = _interactionPoint.value_or(glm::vec3(0.0f));
	const bool valid = _held && _interactionPoint && map_coords::InBounds(point) && InInfluence();
	if (valid)
	{
		PushThrowData(velocity, true);
	}
	else
	{
		// HOLDING (hand state 4): the spring stops, then the drop on the map coord; no buffer clear on this path (that is
		// RemoveFirstFromHand's). (approximate) one _springActive for every state (the original's flag is HOLDING's own),
		// which HandPlacement also clears
		if (_renderHandState == 4)
		{
			_springActive = false;
		}
	}
	if (!_held || _applySentTurn != 0)
	{
		return;
	}
	if (!valid)
	{
		FailApply(point);
		return;
	}
	game_packets::Push({game_packets::Type::ApplyToMapCoord, entt::null, point});
	_applySentTurn = game_clock::Turn();
}

void HandSystem::GameTurnUpdate() noexcept
{
	// After the grain hand state's update (MagicLoop.cpp): the hand's object no longer available is thrown, else its
	// hold type is refreshed. (approximate) the hold type is ComputeHoldParameters' (the radius, lowering and height
	// stay those of the pick-up)
	auto& registry = Locator::entitiesRegistry::value();
	if (_renderHandHeld)
	{
		if (!registry.Valid(*_renderHandHeld) || !ecs::IsAvailable(*_renderHandHeld))
		{
			RenderHandRelease();
		}
		else
		{
			const auto radius = _holdRadius;
			const auto lowering = _loweringMultiplier;
			const auto height = _heldHeight;
			const auto rooted = _rooted;
			ComputeHoldParameters(*_renderHandHeld);
			_holdRadius = radius;
			_loweringMultiplier = lowering;
			_heldHeight = height;
			_rooted = rooted;
		}
	}
	// lastReleased no longer available: it and the countdown at 0. (not ported) the creature hand's fields and the
	// spell-in-hand particles
	if (_lastReleased && (!registry.Valid(*_lastReleased) || !ecs::IsAvailable(*_lastReleased)))
	{
		_lastReleased.reset();
		_releaseImpulseMs = 0;
	}
}

void HandSystem::UpdateReleaseImpulse() noexcept
{
	// Each frame, while preparing to draw. Inside the citadel nothing. With an object: HOLDING sets the countdown to 180;
	// the interface's hand state 0x19 (INVISIBLE), a locked pile, a seed (GRAIN) or the tug (TUG) leave it
	if (game_clock::IsInsideCitadel())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (const auto held = RenderHeld(); held)
	{
		const bool pile = _pickSource && ecs::object_resources::IsPileResource(*_pickSource);
		const bool seed = registry.Valid(*held) && registry.AllOf<SpellSeed>(*held);
		if (_interfaceHandState != 0x19 && !pile && !seed && !_tug)
		{
			_releaseImpulseMs = 180;
		}
		return;
	}
	// With nothing held, the countdown goes down by the frame's game ms; the frame it turns negative (exactly 0 stops it
	// with nothing sent) and lastReleased is set, the countdown = 0 and the release impulse packet: lastReleased, the
	// held ground point minus the required hand position, and the Holding spring velocity; then lastReleased is
	// cleared. Both points are the last HOLDING / GRAIN frame's: only the state updates write the ground point
	// (Holding, Camera), not the normal state's
	if (_releaseImpulseMs <= 0)
	{
		return;
	}
	_releaseImpulseMs -= static_cast<int32_t>(game_clock::FrameGameMs());
	if (_releaseImpulseMs >= 0 || !_lastReleased)
	{
		return;
	}
	_releaseImpulseMs = 0;
	const auto delta = _heldGroundPoint - _requiredHandPosition;
	game_packets::Packet packet {game_packets::Type::ReleaseImpulse, *_lastReleased};
	packet.data = {delta.x, delta.y, delta.z, _springVelocity.x, _springVelocity.y, _springVelocity.z};
	game_packets::Push(packet);
	_lastReleased.reset();
}

void HandSystem::SendPlaceInHand(entt::entity entity) noexcept
{
	// Its sounds at once, the place-in-hand packet and the action state WAIT FOR PLACE IN HAND (the press is kept until
	// the object is in the hand)
	// IN_PHYSICS: flying, not a resting proxy (PhysicsObjects::IsFlying)
	GenericPickupSounds(entity, physics::PhysicsObjects::IsFlying(entity));
	game_packets::Push({game_packets::Type::PlaceInHand, entity});
	_pickPressHeld = true;
}

void HandSystem::ApplyPlaceInHand(entt::entity object) noexcept
{
	// The place-in-hand packet, re-entered from the big forest and the magic teleport (they put another object in the
	// hand)
	auto& registry = Locator::entitiesRegistry::value();
	// an object, interactable, space in the hands, ValidForPlaceInHand, not IsCannotBePickedUp, and not carried by a
	// particle system (particle_carried_objects::IsCarried). (pending) two flags whose names are unconfirmed:
	// UNAVAILABLE_FOR_STATE_CHANGE and LOCKED_SELECT
	if (!Interactable(object) || _held || !ValidForPlaceInHand(object) || ecs::object_flags::IsCannotBePickedUp(object) ||
	    physics::particle_carried_objects::IsCarried(object))
	{
		EndAction();
		_pickPressHeld = false;
		return;
	}
	// The object goes into the hand. The ones that return 0 put another object in the hand through this function again;
	// the outer call then only ends the action.
	// (pending) the other re-entrant classes (field, fish farm, field crop, pile, fireball); the object's fire noticing
	// that it started moving
	if (registry.AllOf<BigForest>(object))
	{
		// BigForest: a resource removed, a tree created and put in the hand, then 0
		if (const auto tree = CreateTreeFromForest(object); tree != entt::null)
		{
			ApplyPlaceInHand(tree);
		}
		EndAction();
		_pickPressHeld = _held.has_value();
		return;
	}
	if (registry.AllOf<MagicTeleport>(object))
	{
		// MagicTeleport: the spell's seed put in the hand, then 0
		if (const auto seed = SeedToPlaceInHand(object); seed != entt::null)
		{
			ApplyPlaceInHand(seed);
		}
		EndAction();
		_pickPressHeld = _held.has_value();
		return;
	}
	if (registry.AllOf<SpellSeed>(object))
	{
		// SpellSeed (the chants and age stored from the spell, the spell link cleared: its spell closes down): 1 the hand
		// takes it (worship's PlaceSeedInMagicHand does both); 3 ToBeDeleted -> the action ends
		if (worship::interface::PlaceSeedInMagicHand(PlayerNames::PLAYER_ONE, object) != 1)
		{
			EndAction();
			_pickPressHeld = false;
		}
		return;
	}
	// 1: the rest (PickUp). A rooted tree (not flying) leaves the ground here: PickUp plays the tree's break sound and
	// updates its alignment (its roots follow it per frame, HandHolding). (pending) the reaction 0x10; the help
	// profile triggers 2 / 3 in PickUp
	PickUp(object, false);
}

void HandSystem::SendStartLockedSelect(entt::entity object) noexcept
{
	// The object's network-unfriendly start (nothing for most objects, (not ported) the totems'), the start packet and
	// action state 3 with the object as the action's
	game_packets::Push({game_packets::Type::StartLockedSelect, object});
	_lockedSelectAction = object;
}

void HandSystem::SendEndLockedSelect() noexcept
{
	// State 3's end: with an action object, its network-unfriendly end (nothing for piles, fields and fish farms) and
	// the end packet; then the multi pickup stops at once. (not verified) the test of one object flag bit
	if (_lockedSelectAction)
	{
		game_packets::Push({game_packets::Type::EndLockedSelect, *_lockedSelectAction});
		_lockedSelectAction.reset();
		_lockedSelectStopped = true;
	}
}

void HandSystem::ApplyStartLockedSelect(entt::entity object) noexcept
{
	// The start packet: an interactable object not in a locked select already ((inferred) openblack has the one hand,
	// so its locked object), then the network-friendly start, the status's locked object and the locked-select flag.
	// Otherwise the action ends (state 3's end: the end packet).
	// (inferred) a hand already holding something refuses it: state 3 begins only on a press with an empty hand
	auto& registry = Locator::entitiesRegistry::value();
	if (_pickSource == object || _held || !Interactable(object))
	{
		SendEndLockedSelect();
		return;
	}
	if (std::as_const(registry).AllOf<Creature>(object))
	{
		// a creature: now locked for the hand, which takes hold of it (the creature hand). (not ported) it first finishes
		// what it was doing
		_creatureLocked = _creatureLock == object && Locator::creatureHandSystem::has_value() &&
		                  Locator::creatureHandSystem::value().Grab(object);
		if (!_creatureLocked)
		{
			SendEndLockedSelect();
		}
		return;
	}
	// The network-friendly start: Field, FishFarm, pile (the first amount into a hand pot put in the hand, and the multi
	// pickup starts)
	// the hand pot at the status's MapCoords
	const auto point = SyncMapPoint();
	if (registry.AllOf<Field>(object))
	{
		TryPickUpField(object, point);
	}
	else if (registry.AllOf<FishFarm>(object))
	{
		TryPickUpFish(object, point);
	}
	else if (registry.AllOf<Pot>(object))
	{
		// the pile's player is read before the first scoop, which may empty and delete a loose pile; once the hand pot is
		// in the hand the interface records it
		const auto pilePlayer = ecs::object_resources::PlayerOfPile(object);
		PickUp(object, false, point);
		if (_pickSource == object)
		{
			_sourceOwner = pilePlayer;
		}
	}
	if (!_pickSource)
	{
		// (inferred) nothing was taken: the same turn's status pass finds no hand pot to fill, its interaction returns 0
		// and the select ends (EndAction)
		SendEndLockedSelect();
		return;
	}
	_lockedSelectStopped = false;
}

void HandSystem::ApplyEndLockedSelect(entt::entity object) noexcept
{
	// no object (gone): EndAction and nothing else; the turn's pass then ends the select (IsAvailable)
	if (object == entt::null)
	{
		SendEndLockedSelect();
		return;
	}
	if (std::as_const(Locator::entitiesRegistry::value()).AllOf<Creature>(object))
	{
		// a creature: no longer locked for the hand. (not ported) its body goes back to the game
		if (_creatureLock == object)
		{
			_creatureLocked = false;
		}
		return;
	}
	// The end packet: the object's network-friendly end (a pile: the multi pickup stops and the amount tooltip,
	// in HandSystem::Update), the status's locked object cleared and the locked-select flag cleared
	_pickSource.reset();
	_pickFish = false;
	_pickField = false;
}

bool HandSystem::CreatureTakesPress(entt::entity object) const noexcept
{
	const auto* creature = std::as_const(Locator::entitiesRegistry::value()).TryGet<const Creature>(object);
	if (creature == nullptr || !Locator::creatureHandSystem::has_value())
	{
		return false;
	}
	return creature_hand::TakesPress({
	    .isCreature = true,
	    .mayHold = Locator::creatureHandSystem::value().MayHold(object),
	    .cannotBePickedUp = ecs::object_flags::IsCannotBePickedUp(object),
	    .locked = _creatureLock.has_value(),
	    .friendly = creature_hand::IsFriendlyTo(creature->owner, PlayerNames::PLAYER_ONE),
	});
}

void HandSystem::UpdateCreatureLock(bool actionReleased) noexcept
{
	if (!_creatureLock)
	{
		return;
	}
	const auto creature = *_creatureLock;
	const hand_creature::LockFacts facts {
	    .available = ecs::IsAvailable(creature),
	    .locked = _creatureLocked,
	    .stillSelected = _lockedSelectAction == creature,
	    .actionReleased = actionReleased,
	};
	const auto step = hand_creature::StepLock(_creatureLockState, facts);
	if (step.sendEnd)
	{
		SendEndLockedSelect();
	}
	if (!step.letGo)
	{
		_creatureLockState = step.next;
		return;
	}
	// (openblack guard) a creature the hand took hold of without reaching the CREATURE state is let go without
	// feedback: the original sends it only from that state
	if (_renderHandState != hand_creature::k_CreatureHandState && Locator::creatureHandSystem::has_value() &&
	    Locator::creatureHandSystem::value().GetCreature().has_value())
	{
		(void)Locator::creatureHandSystem::value().Release();
	}
	_creatureLock.reset();
	_creatureLocked = false;
	_creatureLockState = {};
}

int32_t HandSystem::UpdateCreatureFrame(uint32_t cameraMs) noexcept
{
	const bool creatureInteraction = _interfaceHandState == 0x13;
	if (creatureInteraction)
	{
		// (approximate) the original takes the creature under the hand again every frame; the locked one stands for it.
		// (not ported) the interaction camera starts with the interaction
		_creatureInHand = _creatureLock;
	}
	const hand_creature::FrameFacts facts {
	    .creatureInteraction = creatureInteraction,
	    .hasCreature = _creatureInHand.has_value(),
	    .cameraMs = cameraMs,
	    .requiredState = RequiredHandState(),
	    .renderState = _renderHandState,
	    .creatureLocked = _creatureLocked,
	};
	const auto ownCreature = [this]() { return CreatureOwnerForHand(*_creatureInHand) == PlayerNames::PLAYER_ONE; };
	const auto step = hand_creature::StepFrame(_creatureInteraction, facts, ownCreature);
	_creatureInteraction = step.interaction;
	for (const auto send : step.Sends())
	{
		switch (send)
		{
		case hand_creature::Send::Click:
			// let go of the player's own creature within a click's time: the click packet, the player's leash key
			game_packets::Push({game_packets::Type::CreatureLeashClick});
			break;
		case hand_creature::Send::Feedback:
			LeaveCreatureState();
			break;
		}
	}
	return step.renderState;
}

void HandSystem::LeaveCreatureState() noexcept
{
	// how the creature was treated goes to its mind at the next turn. (approximate) the original sends nothing when
	// the creature is ready to be let go already
	if (_creatureInHand && Locator::creatureHandSystem::has_value())
	{
		if (const auto given = Locator::creatureHandSystem::value().Release())
		{
			game_packets::Packet packet {game_packets::Type::CreatureFeedback, given->creature};
			packet.data[0] = given->feedback;
			game_packets::Push(packet);
		}
	}
	_creatureInHand.reset();
}

void HandSystem::ProcessLockedSelect() noexcept
{
	// The locked select: its turn count goes up; it lasts while the object is available, has influence > 0 at the
	// synced hand and its interaction (pile, field, fish farm) returns 1; otherwise it is cleared and the action ends
	// (state 3's end: the end packet)
	if (!_pickSource)
	{
		return;
	}
	++_pickTurns;
	bool keep = ecs::IsAvailable(*_pickSource) && _held && ecs::IsAvailable(*_held) &&
	            influence::CalculatePlayerInfluence(PlayerNames::PLAYER_ONE, _turnHand) > 0.0f;
	if (keep)
	{
		keep = _pickFish ? ProcessInInteractFish() : _pickField ? ProcessInInteractField() : ProcessInInteractPile();
	}
	if (!keep)
	{
		_pickSource.reset();
		_pickFish = false;
		_pickField = false;
		_pickTurns = 0;
		SendEndLockedSelect();
	}
}

void HandSystem::ApplyTap(entt::entity object) noexcept
{
	// The tap packet: the object interactable and valid to tap, then tapped at the status's synced hand (abode, rock,
	// scaffold, one-off spell seed). The help profile's Tap trigger; (not ported) a rock making the creature mimic
	// the player
	if (!Interactable(object))
	{
		return;
	}
	const pot_resource::Dropper is {true, PlayerNames::PLAYER_ONE, true};
	if (!hand_tap::ValidToTap(object, is))
	{
		return;
	}
	help_profile::Trigger(help_profile::Event::Tap);
	hand_tap::Tap(object, is, _turnHand);
}

void HandSystem::ResetTurnState() noexcept
{
	// The status's reset: the synced and turn fields; with them the hand sync's last-sent values and countdown, the
	// status's throw and the action's object. (inferred) on a new land, with game_packets::Reset (the callers of the
	// interface's reset were not traced)
	_syncMapCoords = {};
	_syncHand = glm::vec3(0.0f);
	_syncCameraPosition = glm::vec3(0.0f);
	_syncCameraFocus = glm::vec3(0.0f);
	_turnHand = glm::vec3(0.0f);
	_turnDelta = glm::vec3(0.0f);
	_stillMotion = glm::vec3(0.0f);
	_stillTurns = 0;
	_turnSpeed = 0.0f;
	_turnHeading = 0.0f;
	_turnRate = 0.0f;
	_turnVelocity = glm::vec3(0.0f);
	_turnSideAcceleration = 0.0f;
	_sentMapCoords = {};
	_sentHand = glm::vec3(0.0f);
	_sentCameraFocus = glm::vec3(0.0f);
	_sentCameraPosition = glm::vec3(0.0f);
	_syncCountdown = 0;
	_statusThrowVelocity = glm::vec3(0.0f);
	_statusThrowHandPosition = glm::vec3(0.0f);
	_statusThrowAngular = glm::vec3(0.0f);
	_statusThrowAngles = glm::vec3(0.0f);
	_lockedSelectAction.reset();
	_lockedSelectStopped = false;
	// (inferred) the apply gate with the packets that are gone; the hand's clear-map: lastReleased and the countdown
	_applySentTurn = 0;
	_applyHandledTurn = 0;
	_lastReleased.reset();
	_releaseImpulseMs = 0;
}

void HandSystem::ValidateHands() noexcept
{
	// A held object that is no longer interactable (deleted: ToBeDeleted marks it, the deletion comes later in the turn)
	// leaves the hand with no physics (the fire's SetOutMagicHand; a seed's own out-of-hand hook); then the hands are
	// tidied and, for the local interface, the hand's own hold is thrown (here the same _held)
	if (!_held || Interactable(*_held))
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = *_held;
	_held.reset();
	_pickSource.reset();
	_releaseArmed = false;
	_seedAction = SeedAction::None;
	RenderHandRelease();
	if (registry.Valid(entity))
	{
		// Out of the hand: the in-hand flag cleared, the object's out-of-hand hook, the fire's SetOutMagicHand.
		// (pending) the out-of-hand hook of the other classes (the villager's), as in RemoveFirstFromHand
		if (registry.AllOf<SpellSeed>(entity))
		{
			SeedLeftHand(entity);
		}
		ecs::fire::SetOutMagicHand(entity);
	}
}

glm::vec3 HandSystem::SyncMapPoint() const noexcept
{
	return map_coords::ToWorld(_syncMapCoords);
}

bool HandSystem::HandCastNeedsContinualPackets() noexcept
{
	// The spells, until one needs continual packets: cast from the hand, not closed down, cast by a human player and
	// the status's player.
	// (pending) the flock spell's override (one more case first)
	bool needs = false;
	Locator::entitiesRegistry::value().Each<const Spell>([&needs](entt::entity spell, const Spell& data) {
		needs = needs || (magic::IsCastFromHand(spell) && !data.closedDown && data.isHumanPlayerCasting &&
		                  data.player == PlayerNames::PLAYER_ONE);
	});
	return needs;
}

void HandSystem::SendHandSync() noexcept
{
	// While the countdown is not 0 and no hand cast needs continual packets, it goes down by one and nothing is
	// compared
	if (_syncCountdown != 0 && !HandCastNeedsContinualPackets())
	{
		--_syncCountdown;
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	// the action collide's MapCoords, the hand, the camera position and ((inferred) the focus)
	if (_interactionPoint)
	{
		_actionCoords = map_coords::FromWorld(*_interactionPoint);
	}
	const auto hand = registry.Get<const Transform>(_hands[static_cast<size_t>(Side::Left)]).position;
	glm::vec3 cameraPosition = _sentCameraPosition;
	glm::vec3 cameraFocus = _sentCameraFocus;
	if (Locator::camera::has_value())
	{
		cameraPosition = Locator::camera::value().GetOrigin();
		cameraFocus = Locator::camera::value().GetFocus();
	}
	// a component differs by more than 0.01 (the float difference against the double)
	const auto moved = [](glm::vec3 a, glm::vec3 b) {
		constexpr double k_Epsilon = 0.0099999997764825821;
		return static_cast<double>(std::fabs(a.x - b.x)) > k_Epsilon || static_cast<double>(std::fabs(a.y - b.y)) > k_Epsilon ||
		       static_cast<double>(std::fabs(a.z - b.z)) > k_Epsilon;
	};
	// another map cell (CellX / CellZ, the high words)
	const bool handChanged = map_coords::CellX(_sentMapCoords) != map_coords::CellX(_actionCoords) ||
	                         map_coords::CellZ(_sentMapCoords) != map_coords::CellZ(_actionCoords) || moved(_sentHand, hand);
	const bool cameraChanged = moved(_sentCameraFocus, cameraFocus) || moved(_sentCameraPosition, cameraPosition);
	game_packets::Packet packet {game_packets::Type::HandAndCamera};
	packet.coords = _actionCoords;
	packet.data = {hand.x,           hand.y,        hand.z,        cameraPosition.x, cameraPosition.y,
	               cameraPosition.z, cameraFocus.x, cameraFocus.y, cameraFocus.z};
	if (handChanged && cameraChanged)
	{
		packet.type = game_packets::Type::HandAndCamera;
	}
	else if (handChanged)
	{
		packet.type = game_packets::Type::Hand;
	}
	else if (cameraChanged)
	{
		packet.type = game_packets::Type::Camera;
	}
	else
	{
		return; // nothing sent, the countdown as it is
	}
	if (handChanged)
	{
		_sentMapCoords = _actionCoords;
		_sentHand = hand;
	}
	if (cameraChanged)
	{
		_sentCameraFocus = cameraFocus;
		_sentCameraPosition = cameraPosition;
	}
	packet.value = NextPingIndex();
	game_packets::Push(packet);
	// 3 in the internet lobby, else 1 ((inferred) single player)
	_syncCountdown = 1;
}

void HandSystem::UpdateTurnMovement() noexcept
{
	// The hand's movement per turn. k = 1000 / the turn's ms, the turns per second. (approximate) float steps in the
	// original's order; the 24-bit FPU's intermediate roundings are not checked one by one
	const float k = 1000.0f / static_cast<float>(game_clock::MsPerTurn());
	const glm::vec3 previous = _turnDelta;
	const glm::vec3 delta = _syncHand - _turnHand; // 1: the hand's move this turn
	_turnDelta = delta;
	_stillMotion += delta; // 2
	if (glm::length(_stillMotion) < 1.0f)
	{
		++_stillTurns; // 3: the hand stayed within one unit
	}
	else
	{
		_stillTurns = 0;
		_stillMotion = glm::vec3(0.0f);
	}
	_turnVelocity += 0.6f * (k * delta - _turnVelocity); // 4
	// 5: the sideways part of this turn's delta, against the last turn's delta turned 90 degrees in XZ
	float side = 0.0f;
	const glm::vec3 normal(previous.z, 0.0f, -previous.x);
	if (normal.x != 0.0f || normal.z != 0.0f)
	{
		if (const float length = glm::length(normal); length != 0.0f)
		{
			side = glm::dot(delta, normal / length);
		}
	}
	_turnSideAcceleration += 0.6f * (side * k * k - _turnSideAcceleration); // 6
	_turnSpeed = glm::length(_turnVelocity);                                // 7
	_turnHeading = std::atan2(_turnVelocity.z, _turnVelocity.x);            // 8: 0..2 pi
	if (_turnHeading < 0.0f)
	{
		_turnHeading += glm::two_pi<float>();
	}
	_turnRate = _turnSpeed > 0.0001f ? -(_turnSideAcceleration / _turnSpeed) : 0.0f; // 9
	_turnHand = _syncHand;                                                           // 10
}

void HandSystem::ProcessTurn() noexcept
{
	// The interface's turn (after the superpackets), the hand's part: the hand sync packets, then the status pass: the
	// movement, (elsewhere) the heart beat, the locked select and the hands: the held object takes the synced hand as
	// its place, then its in-hand process
	SendHandSync();
	UpdateTurnMovement();
	ProcessLockedSelect();
	ValidateHands();
	// After the status pass: the apply gate opens once its packet's handler has run
	if (_applySentTurn <= _applyHandledTurn)
	{
		_applySentTurn = 0;
		_applyHandledTurn = 0;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (!_held || !registry.Valid(*_held))
	{
		return;
	}
	const auto held = *_held;
	// MapCoords x, z = the synced hand in fixed point (map_coords::ToFixed), the altitude over the land there = the
	// hand's height - the altitude (so the place's height is the hand's). Every turn, before the in-hand process: the
	// turn's logic reads it there; the frames between draw it in the hand (UpdateHeldObject, DrawOutOfMap). The land
	// under it is looked up at a second conversion, x times 65536 times 0.1, and the height over it = y - the altitude
	// there (a float); the place's height is then the altitude at the MapCoords plus that
	if (auto* transform = registry.TryGet<Transform>(held); transform != nullptr)
	{
		const float x = map_coords::Quantise(_turnHand.x);
		const float z = map_coords::Quantise(_turnHand.z);
		float y = _turnHand.y;
		if (Locator::terrainSystem::has_value())
		{
			const auto ground = land_morph::CurrentAltitude();
			// (inferred) CurrentAltitude (GetHeightAt) stands for the original's altitude
			const auto lookup = [](float m) { return map_coords::ToMetres(map_coords::MetresToFixedForHandLookup(m)); };
			const float altitude = _turnHand.y - ground(glm::vec2(lookup(_turnHand.x), lookup(_turnHand.z)));
			y = ground(glm::vec2(x, z)) + altitude;
		}
		transform->position = glm::vec3(x, y, z);
		// (inferred) the drawn matrix's rotation too: the original's turn reads the drawn mesh (FireCentre's mesh centre)
		if (const auto* pose = registry.TryGet<const HandDrawPose>(held); pose != nullptr)
		{
			transform->rotation = pose->rotation;
		}
	}
	if (registry.AllOf<SpellSeed>(held))
	{
		magic::seed::ProcessInHand(held);
	}
	else if (influence::CalculatePlayerInfluence(PlayerNames::PLAYER_ONE, ecs::fire::traits::FireCentre(held)) > 0.0f)
	{
		// Inside the holder's influence it catches the fires it is held over
		// (inferred: openblack has only the local player's hand, taken as PLAYER_ONE)
		ecs::fire::CheckToSeeIfObjectIsNearOnFireObject(held);
	}
}
