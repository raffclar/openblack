/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <fstream>
#include <tuple>

#include <L3DFile.h>
#include <LNDFile.h>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/ObjectMatrix.h"
#include "Camera/Camera.h"
#include "Camera/CameraModel.h"
#include "Debug/DebugEnv.h"
#include "ECS/AnimalAI.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Archetypes/HandArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/HandDrawPose.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/NeedsSorting.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/FastExp.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/FishShoals.h"
#include "ECS/Life.h"
#include "ECS/LivingPhysics.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectResources.h"
#include "ECS/Physics/FromHand.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/WaterRings.h"
#include "FileSystem/FileSystemInterface.h"
#include "GameClock.h"
#include "Graphics/Texture2D.h"
#include "HandSystem.h"
#include "HandSystemDetail.h"
#include "Help/HelpProfile.h"
#include "InfoConstants.h"
#include "Input/HandDemo.h"
#include "Locator.h"
#include "Magic/Core/SpellSeed.h"
#include "Magic/Gestures/PowerUpSystem.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Windowing/WindowingInterface.h"
#include "Worship/Worship.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::hand_detail;

namespace
{
/// What the holding trace keeps between calls, in the debug hooks' store (Locator::debugHooks)
struct HandHoldingDebugHooksState
{
	int treeHoldFrame {0}; // OPENBLACK_HAND_TRACE: frames of a held tree, one line every 300
};

HandHoldingDebugHooksState& HandHoldingDebugHooksData()
{
	if (!Locator::debugHooks::has_value())
	{
		std::fputs("ecs::systems::HandSystem: no debug hooks in the locator (Locator::debugHooks)\n", stderr);
		std::abort();
	}
	return Locator::debugHooks::value().Get<HandHoldingDebugHooksState>();
}
} // namespace

void HandSystem::PickUp(entt::entity entity, bool genericPickupSounds, std::optional<glm::vec3> statusPoint) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	_pickSource.reset();
	_pickFish = false;
	_pickField = false;
	_lastHeldPosition.reset();
	_handVelocity = glm::vec3(0.0f);
	// an object in physics leaves it
	const bool caught = physics::PhysicsObjects::Find(entity) != nullptr;
	physics::PhysicsObjects::RemoveObject(entity);
	// tested after RemoveObject has already cleared the in-physics flag, so the help event is PickUp, never Catch, as
	// in the original
	const bool catchEvent =
	    physics::PhysicsObjects::IsFlying(entity) &&
	    registry.Get<const Transform>(entity).position.y >=
	        (Locator::terrainSystem::has_value()
	             ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(registry.Get<const Transform>(entity).position.x,
	                                                                     registry.Get<const Transform>(entity).position.z))
	             : 0.0f);
	// a living object's own pick-up (ECS/LivingPhysics)
	ecs::living::InterfaceSetInMagicHand(entity);
	// a pot stops offering its reaction
	if (registry.AllOf<Pot>(entity))
	{
		ecs::animal_ai::RemovePotReaction(entity);
	}
	// Food / wood: the hand grabs a HandFood / HandWood pile and keeps pulling from the source while held over it
	// (GPotInfo.amountPickedUpInitially / PerTurn / PerTurnEnd / multiPickUpRampTime from info.dat).
	if (registry.AllOf<Pot>(entity))
	{
		const auto& pots = Locator::infoConstants::value().pot;
		const auto& sourceType = registry.Get<Mesh>(entity);
		PotInfo sourceInfo = PotInfo::FoodPot;
		for (size_t i = 0; i < pots.size(); ++i)
		{
			if (resources::HashIdentifier(pots[i].meshId) == sourceType.id)
			{
				sourceInfo = static_cast<PotInfo>(i);
				break;
			}
		}
		const bool isHandPile = sourceInfo == PotInfo::HandWood || sourceInfo == PotInfo::HandFood;
		if (!isHandPile)
		{
			const auto handType = pots[static_cast<size_t>(sourceInfo)].resourceType == ResourceType::Wood ? PotInfo::HandWood
			                                                                                               : PotInfo::HandFood;
			const auto& handInfo = pots[static_cast<size_t>(handType)];
			// n = min(the hand pot's amountPickedUpInitially, the source's resource) (a store pile offers the store's total),
			// removed for the hand; a new hand pot of n at the hand's point, poisoned when the source was
			const auto resource = pots[static_cast<size_t>(sourceInfo)].resourceType;
			const auto take =
			    std::min<uint32_t>(object_resources::GetResource(entity, resource), handInfo.amountPickedUpInitially);
			if (take == 0)
			{
				return;
			}
			const auto position = statusPoint.value_or(registry.Get<Transform>(entity).position);
			// (approximate) the poisoned flag is read just before the removal
			const bool poisoned = object_resources::IsPoisoned(entity);
			object_resources::RemoveResource(entity, resource, take, InterfaceStatus());
			const auto pile = archetypes::PotArchetype::Create(position, 0.0f, handType, static_cast<int32_t>(take));
			if (pile == entt::null)
			{
				return;
			}
			if (poisoned)
			{
				registry.Get<Pot>(pile).poisoned = true;
			}
			_pickSource = entity; // an emptied loose pile is gone: the first turn's IsAvailable ends the select
			_pickTurns = 0;
			_pickLock = _interactionPoint.value_or(position);
			entity = pile;
		}
	}
	// the object entering the hand leaves the map cells. (inferred) The early returns above put nothing in the hand and
	// take nothing out, the same end as the original's failure path (the object goes back into the map)
	if (ecs::map_cells::IsObjectInMap(entity))
	{
		ecs::map_cells::RemoveMapObject(entity);
	}
	// Carried objects must not be glued to the landscape by the height-map shader.
	if (registry.AllOf<MorphWithTerrain>(entity))
	{
		registry.Remove<MorphWithTerrain>(entity);
	}
	auto& transform = registry.Get<Transform>(entity);
	const float ground =
	    Locator::terrainSystem::has_value()
	        ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(transform.position.x, transform.position.z))
	        : 0.0f;
	_heldAltitude = transform.position.y - ground;
	_heldRotation = transform.rotation;
	_heldTop = 1.0f;
	_heldHeight = 0.0f;
	_holdRadius = 0.0f;
	auto& meshes = Locator::resources::value().GetMeshes();
	if (const auto* mesh = registry.TryGet<const Mesh>(entity); mesh != nullptr && meshes.Contains(mesh->id))
	{
		const auto& box = meshes.Handle(mesh->id)->GetBoundingBox();
		_heldTop = box.maxima.y * transform.scale.y;
		// A tree's hold radius = 0.2 * its 2D radius.
		_heldHeight = _heldTop;
		_holdRadius = 0.2f * 0.5f * std::max(box.Size().x * transform.scale.x, box.Size().z * transform.scale.z);
	}
	const auto pickupPoint = transform.position;
	if (registry.AllOf<Tree>(entity) && !caught)
	{
		// A rooted tree breaks with a random G_TreeBreak sound (3D, InGame, at once). Uprooting is evil: the alignment
		// update for the hand's player, -treePullPutAlignmentChange weighed by the alignment.
		ecs::effects::alignment::UpdateForTree(PlayerNames::PLAYER_ONE, false);
		audio::tags::Create(pickupPoint, audio::tags::RandomSample(32, 3), false, 3, 0, false, true, audio::SfxBank::InGame, 0);
		_heldAltitude = 0.0f;
	}
	if (genericPickupSounds)
	{
		// (the hand's pick-up message plays these when it is sent: GenericPickupSounds)
		GenericPickupSounds(entity, caught);
	}
	ComputeHoldParameters(entity);
	_held = entity;
	// the interface records who owned what it picked up: the most influential player where the object was (a locked
	// select's start then records its source's player instead)
	_sourceOwner = ecs::effects::alignment::MostInfluentialPlayer(pickupPoint);
	// the object's reactions that end while it is in a hand
	ecs::effects::reactions::SetUnavailableInHand(entity);
	// once the object is stored, the local hand's gesture buffer is cleared
	magic::gestures::ClearBuffer();
	// the Catch / Pick up help event, once the object is in the hand
	help_profile::Trigger(catchEvent ? help_profile::Event::Catch : help_profile::Event::PickUp);
	// the drawn hand holds the same object (a no-op when it already did)
	if (_renderHandHeld && *_renderHandHeld != entity)
	{
		// (approximate) the original swaps without a throw: it neither sets _lastReleased (which RenderHandRelease sets) nor
		// reads the world matrix
		RenderHandRelease();
	}
	if (!_renderHandHeld)
	{
		RenderHandSetHeldFlags(entity); // on the new object
	}
	_renderHandHeld = entity;
	if (Locator::entitiesRegistry::value().AllOf<HandDrawPose>(entity))
	{
		Locator::entitiesRegistry::value().Remove<HandDrawPose>(entity);
	}
	_hovered.reset();
	// a burning object leaves its group; held (not a villager) it is REACT_TO_BURNING_OBJECT_IN_HAND
	// (fire::StartedMoving, ECS/Fire)
	fire::StartedMoving(entity, !registry.AllOf<Villager>(entity));
	// the same tail: a firefly sleeping on the object is freed and may leave a one-shot miracle
	// (Worship/FireFlyReward.cpp, Land 1's FIRE_FLY_SPELL_REWARD_PROB)
	worship::OnPlacedInMagicHand(entity);
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Hand: picked up entity {}", static_cast<uint32_t>(entity));
}

void HandSystem::GenericPickupSounds(entt::entity entity, bool inPhysics) noexcept
{
	// When the pick-up message is sent: every object but a rooted tree or forest (not flying) gets a G_PickUpObject (10)
	// point tag at its position (3D, InGame, plays at once); a DeadTree is not a tree here, so it sounds too
	auto& registry = Locator::entitiesRegistry::value();
	const auto pickupPoint = registry.Get<const Transform>(entity).position;
	const auto pickupTag = [&pickupPoint](int sample) {
		audio::tags::Create(pickupPoint, sample, false, 3, 0, false, true, audio::SfxBank::InGame, 0);
	};
	const bool rooted = registry.AnyOf<Tree, BigForest>(entity) && !inPhysics;
	if (!rooted)
	{
		pickupTag(10);
	}
	// A living villager screams with a second point tag of the same form: a child 180 + GetRandomSample(7)
	// G_PickUpChild_01.., else a woman 194 G_PickUpWoman_01.., else 187 G_PickUpMan_01..
	if (registry.AllOf<Villager>(entity) && ecs::life::LifeOf(entity) > 0.0f)
	{
		const int first = ecs::villager::IsChild(entity) ? 180 : ecs::villager::IsWoman(entity) ? 194 : 187;
		pickupTag(audio::tags::RandomSample(first, 7));
	}
}

int HandSystem::Release(glm::vec3 velocity, [[maybe_unused]] std::optional<glm::vec3> mapPoint, bool statusPose) noexcept
{
	// The hand opens: the held object is applied to the map point whatever the speed, then ThrowObjectFromHand
	// (dontReplant false) -> InitialisePhysicsFromHand(the throw velocity, ...)
	if (!_held)
	{
		return 0;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = *_held;
	if (registry.Valid(entity))
	{
		// a tree let go is put down or thrown: a store takes it only on the press (HandApplyToObject.cpp) or when its
		// body hits one (HandPhysics.cpp)
		// a pot put down offers its reaction again (the hand's HandWood / HandFood become a pile in PutDownHandPot, which
		// offers the pile's)
		if (const auto type = PotInfoOf(entity);
		    registry.AllOf<Pot>(entity) && type != PotInfo::HandWood && type != PotInfo::HandFood)
		{
			ecs::animal_ai::SetupPotReaction(entity);
		}
	}
	return ThrowObjectFromHand(velocity, false, statusPose); // a mobile object's result: 0x16
}

int HandSystem::ThrowObjectFromHand(glm::vec3 velocity, bool dontReplant, bool statusPose) noexcept
{
	// The physics' part (ECS/Physics/FromHand: the hand pot put down, InitialisePhysicsFromHand) with the object still
	// in the hand: no RemoveFromHand here. It returns 0x16 and the caller's HandleApplyResult takes it out of the hand
	// (fire::SetOutMagicHand, RenderHandRelease)
	if (!_held)
	{
		return 0;
	}
	const auto entity = *_held;
	// the matrix from the status's hand angles and position, the object's scale kept, before InitialisePhysicsFromHand.
	// The Throw help event for the local player
	auto& registry = Locator::entitiesRegistry::value();
	std::optional<glm::vec3> angular;
	if (statusPose && registry.Valid(entity))
	{
		// the YXZ rotation of the hand angles and the hand position, the scale kept apart; set right before the throw,
		// whose SetUpBody reads it. The angular momentum as it is (the throw's angular velocity)
		auto& transform = registry.Get<Transform>(entity);
		transform.position = _statusThrowHandPosition;
		transform.rotation = affine::RotationYXZ(_statusThrowAngles.y, _statusThrowAngles.x, _statusThrowAngles.z);
		angular = _statusThrowAngular;
	}
	else if (registry.Valid(entity))
	{
		// (inferred) with no status pose the throw starts from the matrix the hand drew it with (its G3D, which the
		// physics' SetUpBody reads), not from the turn's place
		if (const auto* pose = registry.TryGet<const HandDrawPose>(entity); pose != nullptr)
		{
			auto& transform = registry.Get<Transform>(entity);
			transform.position = pose->position;
			transform.rotation = pose->rotation;
		}
	}
	if (registry.Valid(entity) && registry.AllOf<HandDrawPose>(entity))
	{
		registry.Remove<HandDrawPose>(entity); // out of the hand: drawn by the physics from now on
	}
	help_profile::Trigger(help_profile::Event::Throw);
	physics::from_hand::Throw(entity, velocity, dontReplant, angular);
	return 0x16;
}

void HandSystem::UpdateThrowBlock(glm::vec3 oldHand, glm::vec3 newHand) noexcept
{
	_recordedHandPose.reset(); // this frame's Holding / Grain write HandPos and the angles again
	if (!IsHoldingSeed())
	{
		// h = 0 only while the spring works (the velocity is the spring's, HandPlacement); otherwise h keeps its last value
		if (_springActive)
		{
			_handAngularVelocity = glm::vec3(0.0f);
		}
		_grainEntered = false;
		return;
	}
	if (!_grainEntered)
	{
		// entering the grain state: d = v_s = 0, a_s = 0
		_grainEntered = true;
		_grainDisplacement = glm::vec3(0.0f);
		_grainVelocity = glm::vec3(0.0f);
		_grainSideAcceleration = 0.0f;
	}
	// The grain state, each frame with the hand's dt (the camera time step x 0.001, the real frame time inside the
	// citadel): v_s smooths the hand's per-frame velocity with k = -10 ln(0.2), a_s the sideways displacement over dt^2
	// with k = -10 ln(0.8); the hand velocity = v_s (no 124 cap), the angular velocity = (0, -(a_s / |v_s|), 0)
	// (|v_s| > 0.0001). (not ported) an acos branch behind a flag that is never set. The camera time step follows the
	// game time while a hand demo plays. The exps are evaluated at 24-bit precision (gutils::ExpSinglePrecision)
	const auto ms =
	    game_clock::IsInsideCitadel() ? game_clock::FrameRealMs() : game_clock::CameraFrameMs(hand_demo::IsPlaying());
	const float dt = static_cast<float>(ms) * 0.001f; // dt >= 1 ms
	// (approximate) the original rounds kV / kA to float once; here the double is cast at the end
	const auto kV = static_cast<float>(-10.0 * std::log(1.0 - static_cast<double>(0.8f)));
	const auto kA = static_cast<float>(-10.0 * std::log(1.0 - static_cast<double>(0.2f)));
	const glm::vec3 previous = _grainDisplacement;
	_grainDisplacement = newHand - oldHand; // this frame's move
	const float invDt = 1.0f / dt;
	const glm::vec3 instant = _grainDisplacement * invDt;
	_grainVelocity += (instant - _grainVelocity) * (1.0f - gutils::ExpSinglePrecision(-(dt * kV)));
	_handVelocity = _grainVelocity;
	glm::vec3 normal(previous.z, 0.0f, -previous.x);
	float side = 0.0f;
	if (normal.x != 0.0f || normal.z != 0.0f)
	{
		if (const float length = std::sqrt(normal.z * normal.z + normal.x * normal.x); length != 0.0f)
		{
			const float inv = 1.0f / length;
			normal.x *= inv;
			normal.z *= inv;
			side = (normal.z * _grainDisplacement.z + 0.0f * _grainDisplacement.y) + normal.x * _grainDisplacement.x;
		}
	}
	_grainSideAcceleration = (side * invDt * invDt - _grainSideAcceleration) * (1.0f - gutils::ExpSinglePrecision(-(dt * kA))) +
	                         _grainSideAcceleration;
	const float speed = std::sqrt((_grainVelocity.z * _grainVelocity.z + _grainVelocity.y * _grainVelocity.y) +
	                              _grainVelocity.x * _grainVelocity.x);
	_handAngularVelocity = glm::vec3(0.0f, speed > 0.0001f ? -(_grainSideAcceleration / speed) : 0.0f, 0.0f);
}

void HandSystem::SetThrowBlock(const std::array<float, 12>& block) noexcept
{
	// The recorded block: the velocity, h, the hand position and the angles; the next per-frame write of the hand's state
	// (Holding / Grain, here Place) replaces them
	_handVelocity = glm::vec3(block[0], block[1], block[2]);
	_handAngularVelocity = glm::vec3(block[3], block[4], block[5]);
	_recordedHandPose = std::make_pair(glm::vec3(block[6], block[7], block[8]), glm::vec3(block[9], block[10], block[11]));
}

void HandSystem::RenderHandPickUp(entt::entity entity) noexcept
{
	// The object is held: its model attached to the hand when it is drawn in the hand, and its hold type / lowering from
	// it; the need-sorting flag (RenderHandSetHeldFlags). The dynamic shadow off is already the hand's attach (the object in
	// the hand's projector), which openblack has (ShadowList / RendererShadows); the specular reset is rewritten by every
	// draw (inferred)
	if (_renderHandHeld && *_renderHandHeld != entity)
	{
		RenderHandRelease();
	}
	if (!_renderHandHeld)
	{
		RenderHandSetHeldFlags(entity);
	}
	_renderHandHeld = entity;
	if (!_held)
	{
		ComputeHoldParameters(entity);
	}
}

void HandSystem::RenderHandSetHeldFlags(entt::entity entity) noexcept
{
	// only an object with a model: remember its need-sorting flag, then set it (NeedsSorting, the Z-sorter's list)
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity) || !registry.AllOf<Mesh>(entity))
	{
		_renderHandHadNeedsSorting = true; // nothing to put back
		return;
	}
	_renderHandHadNeedsSorting = registry.AllOf<NeedsSorting>(entity);
	if (!_renderHandHadNeedsSorting)
	{
		registry.Assign<NeedsSorting>(entity);
		registry.SetDirty();
	}
}

void HandSystem::RenderHandRelease() noexcept
{
	// the flags restored, no model in the hand, lastReleased = held, held = none
	if (_renderHandHeld && Locator::entitiesRegistry::value().Valid(*_renderHandHeld) &&
	    Locator::entitiesRegistry::value().AllOf<HandDrawPose>(*_renderHandHeld))
	{
		Locator::entitiesRegistry::value().Remove<HandDrawPose>(*_renderHandHeld);
	}
	// the need-sorting flag back as it was before the hand
	if (_renderHandHeld && !_renderHandHadNeedsSorting && Locator::entitiesRegistry::value().Valid(*_renderHandHeld) &&
	    Locator::entitiesRegistry::value().AllOf<NeedsSorting>(*_renderHandHeld))
	{
		Locator::entitiesRegistry::value().Remove<NeedsSorting>(*_renderHandHeld);
		Locator::entitiesRegistry::value().SetDirty();
	}
	_renderHandHadNeedsSorting = false;
	if (_renderHandHeld)
	{
		_lastReleased = _renderHandHeld; // for the hand's required-state message
	}
	_renderHandHeld.reset();
}

void HandSystem::UpdateRenderHandHeldPose() noexcept
{
	// The hand's state draws its object (Tug, Holding) where the state put it, while the object is still in the map:
	// here the same pose as UpdateHeldObject, for the draw only. A held object (the pick-up applied) is posed by
	// UpdateHeldObject; a tugged tree by the tug
	auto& registry = Locator::entitiesRegistry::value();
	if (!_renderHandHeld || !registry.Valid(*_renderHandHeld))
	{
		_renderHandHeld.reset();
		return;
	}
	const auto entity = *_renderHandHeld;
	if (_held && *_held == entity)
	{
		return; // UpdateHeldObject's pose
	}
	if (_tug && *_tug == entity)
	{
		return; // UpdateTug's pose (the tug matrix)
	}
	// the prediction's object is drawn by the physics, not by the hand
	if (_holdType == HoldType::None || physics::PhysicsObjects::IsPredictionObject(entity))
	{
		if (registry.AllOf<HandDrawPose>(entity))
		{
			registry.Remove<HandDrawPose>(entity);
		}
		return;
	}
	const auto& hand = registry.Get<const Transform>(_hands[static_cast<size_t>(Side::Left)]);
	const auto up = glm::normalize(-hand.rotation[2]);
	const auto side = glm::normalize(hand.rotation[0]);
	auto& pose = registry.AssignOrReplace<HandDrawPose>(entity);
	pose.rotation = glm::mat3(glm::cross(up, side), up, side); // (approximate) with no up' yet: the hand's own axes
	if (_heldUp)
	{
		// the held object's own matrix, as UpdateHeldObject
		auto x = glm::cross(_handHeadingBack, *_heldUp);
		if (x.x != 0.0f || x.y != 0.0f || x.z != 0.0f)
		{
			x *= 1.0f / std::sqrt((x.x * x.x + x.y * x.y) + x.z * x.z);
		}
		pose.rotation = glm::mat3(x, *_heldUp, glm::cross(x, *_heldUp));
	}
	pose.position = hand.position - up * (_loweringMultiplier * _heldHeight);
}

void HandSystem::Drop() noexcept
{
	// The original's forced drop is a zero-velocity release with dontReplant; the test hook OPENBLACK_HAND_TEST_DROP uses
	// this as a gentle release instead (dontReplant false)
	if (_held)
	{
		const auto held = *_held;
		HandleApplyResult(Release(glm::vec3(0.0f)), held, std::nullopt);
	}
}

void HandSystem::UpdateHeldObject() noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	// the object that left the hand is drawn where its logic is again
	if (_heldPosed && (!_held || *_heldPosed != *_held))
	{
		if (registry.Valid(*_heldPosed) && registry.AllOf<HandDrawPose>(*_heldPosed) &&
		    !(_renderHandHeld && *_renderHandHeld == *_heldPosed))
		{
			registry.Remove<HandDrawPose>(*_heldPosed);
		}
		_heldPosed.reset();
	}
	if (!_held)
	{
		return;
	}
	if (!registry.Valid(*_held))
	{
		_held.reset();
		return;
	}
	const auto& hand = registry.Get<Transform>(_hands[static_cast<size_t>(Side::Left)]);
	const auto& logic = registry.Get<const Transform>(*_held);
	glm::vec3 drawn = logic.position;
	// Holding skips the draw while the held object is the prediction's: the physics draws it at the predicted
	// pose (PhysicsDrawPose). (approximate) with no PhysicsDrawPose (the physics draw's openblack guards) it is drawn
	// at its logic Transform; the original draws it nowhere
	if (_holdType != HoldType::None && !physics::PhysicsObjects::IsPredictionObject(*_held))
	{
		// Holding draws the held object out of the map with the matrix below, every frame; the logic Transform is the
		// turn's (ProcessTurn). The pose's scale is the Transform's
		auto& transform = registry.AssignOrReplace<HandDrawPose>(*_held);
		_heldPosed = *_held;
		// The grip point is the hand position itself (the model origin): obj.pos = hand->pos - lowering * up'.
		const auto centre = hand.position;
		const auto up = glm::normalize(-hand.rotation[2]);
		const auto side = glm::normalize(hand.rotation[0]);
		transform.position = centre - up * (_loweringMultiplier * _heldHeight);
		drawn = transform.position;
		if (_heldUp)
		{
			// rows X = norm(d x up'), Y = up', Z = X x up' (the third row negated in one hand mode; (pending) the game
			// runs {-side, up', up' x side}; d = (-sin H, 0, cos H)), as glm's columns; then the hold Y rotation (0 for
			// generic objects, so no turn; (pending) a seed's info value) on rows 0 and 2. (pending) the hand's scale factor
			// in front
			auto x = glm::cross(_handHeadingBack, *_heldUp);
			if (x.x != 0.0f || x.y != 0.0f || x.z != 0.0f)
			{
				x *= 1.0f / std::sqrt((x.x * x.x + x.y * x.y) + x.z * x.z);
			}
			transform.rotation = glm::mat3(x, *_heldUp, glm::cross(x, *_heldUp));
		}
		else
		{
			transform.rotation = glm::mat3(glm::cross(up, side), up, side); // (approximate) no up' yet: the hand's axes
		}
		UpdateRoots(*_held);
		if (debug_env::HandTrace())
		{
			auto& frame = HandHoldingDebugHooksData().treeHoldFrame;
			if (++frame % 300 == 0)
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"),
				                   "Tree hold: hand ({:.1f},{:.1f},{:.1f}) grip ({:.1f},{:.1f},{:.1f}) tree "
				                   "({:.1f},{:.1f},{:.1f}) h={:.1f} r={:.2f} point ({:.1f},{:.1f})",
				                   hand.position.x, hand.position.y, hand.position.z, centre.x, centre.y, centre.z,
				                   transform.position.x, transform.position.y, transform.position.z, _heldHeight, _holdRadius,
				                   _interactionPoint ? _interactionPoint->x : 0.0f,
				                   _interactionPoint ? _interactionPoint->z : 0.0f);
			}
		}
	}
	else if (registry.AllOf<HandDrawPose>(*_held))
	{
		registry.Remove<HandDrawPose>(*_held);
		_heldPosed.reset();
	}
	if (_lastHeldPosition && _lastDt > 0.0f && !_springActive)
	{
		const auto velocity = (drawn - *_lastHeldPosition) / _lastDt;
		_handVelocity += (velocity - _handVelocity) * 0.35f;
	}
	_lastHeldPosition = drawn;
}

glm::mat3 HandSystem::HeldSway(glm::vec3 at, float extraRoll) const noexcept
{
	// The required hand position (HOLDING and GRAIN; with D all zero it stays (0, 0, 0), so R1 = cos a I and R2 = cos b I
	// and up' = (0, cos a cos b, 0), not unit: affine::AxisAngle has no zero guard either; (pending) the distance
	// from the view with |camera - grip| in HOLDING / GRAIN):
	// - tiltX = clamp(smoothed mouse x - mouse x, -80, 80) x (0.3 / 80);
	// - D = normalize(camera - grip) (0 when the two meet);
	// - R1 = AxisAngle(D, (extraRoll + roll) + tiltX): extraRoll is the state's (Grain's tilt, 0 for Holding), roll the
	//   Zoomer's (0 unless a creature takes the object: (not ported));
	// - tiltY = clamp(mouse y - smoothed mouse y, -80, 80) x (0.3 / 80);
	// - R2 = AxisAngle(normalize(0 - D.z, 0, D.x - 0), tiltY), the axis left as it is when zero;
	// - M = R1 R2, and the hand's up row (0, 1, 0) M.
	// As column vectors: up' = AxisAngle(axis2, tiltY) AxisAngle(D, ...) (0, 1, 0).
	// (approximate) `at` (the grip) stands for the hand's position
	if (!Locator::camera::has_value())
	{
		return glm::mat3(1.0f);
	}
	const auto clampLag = [](float v) { return v > -80.0f ? (v < 80.0f ? v : 80.0f) : -80.0f; };
	const float tiltX = (0.3f / 80.0f) * clampLag(_smoothMouse.x - _mouse.x);
	const float tiltY = (0.3f / 80.0f) * clampLag(_mouse.y - _smoothMouse.y);
	auto d = Locator::camera::value().GetOrigin() - at;
	if (d.x != 0.0f || d.y != 0.0f || d.z != 0.0f)
	{
		d *= 1.0f / std::sqrt((d.x * d.x + d.y * d.y) + d.z * d.z);
	}
	const glm::mat3 r1 = affine::AxisAngle(d, (extraRoll + 0.0f) + tiltX);
	glm::vec3 axis(0.0f - d.z, 0.0f, d.x - 0.0f);
	if (axis.x != 0.0f || axis.y != 0.0f || axis.z != 0.0f)
	{
		axis *= 1.0f / std::sqrt((axis.x * axis.x + axis.z * axis.z) + axis.y * axis.y);
	}
	return affine::AxisAngle(axis, tiltY) * r1;
}

void HandSystem::ComputeHoldParameters(entt::entity entity) noexcept
{
	// The hold class table:
	//   Object (default)          ABOVE     R = 0.75 * height   lowering 0
	//   Tree / DeadTree           TREE      R = 0.2 * R2D       lowering 0.1   (Tree is rooted)
	//   MobileObject, Pot         SIDE      R = R2D             lowering 0.7
	//   MobileStatic gate totems, weeping stones: SIDE 0.7; singing stone 1: SIDE 0.4; others ABOVE
	//   Villager                  VILLAGER  R = R2D             lowering 0.65
	auto& registry = Locator::entitiesRegistry::value();
	const auto& transform = registry.Get<Transform>(entity);
	// The hold radius asks the 2D radius, with its overrides: a food pile (PileFood, MagicFood, PuzzleGrain and the
	// hand's HandFood) is x its GetProportionRaised, so the hand opens as the food in it grows. No mesh: 0 for both
	const float radius2D = ecs::object::Get2DRadius(entity);
	_heldHeight = ecs::object::GetHeight(entity);
	_holdType = HoldType::Above;
	_holdRadius = 0.75f * _heldHeight;
	_loweringMultiplier = 0.0f;
	_rooted = registry.AllOf<Tree>(entity);
	if (const auto* seed = registry.TryGet<const SpellSeed>(entity); seed != nullptr)
	{
		// MAGIC until ready, then the seed info's hold type; R = holdRadius x scale. The height is that of the seed info's
		// mesh, even when the seed is not drawn in the hand
		const auto& info = magic::seed::InfoOf(*seed);
		if (const auto half = ecs::object::MeshHalfExtents(resources::HashIdentifier(info.mesh)); half)
		{
			_heldHeight = ecs::object::Height(*half, ecs::object::GetScaleField(entity));
		}
		_holdType = seed->ready ? static_cast<HoldType>(info.holdType) : HoldType::Magic;
		_holdRadius = info.holdRadius * transform.scale.x;
		_loweringMultiplier = info.holdLoweringMultiplier;
		_rooted = false;
	}
	else if (registry.AnyOf<Tree, DeadTree>(entity))
	{
		_holdType = HoldType::Tree;
		_holdRadius = 0.2f * radius2D;
		_loweringMultiplier = 0.1f;
	}
	else if (registry.AnyOf<MobileObject, Pot, OneOffSpellSeed>(entity))
	{
		// a one-shot orb is a mobile object: SIDE, with its lowering and the 2D radius
		_holdType = HoldType::Side;
		_holdRadius = radius2D;
		_loweringMultiplier = 0.7f;
	}
	else if (registry.AnyOf<Villager, Animal>(entity))
	{
		// the Living hold class: villagers and animals alike
		_holdType = HoldType::Villager;
		_holdRadius = radius2D;
		_loweringMultiplier = 0.65f;
	}
	else if (const auto* statics = registry.TryGet<const MobileStatic>(entity); statics != nullptr)
	{
		switch (statics->type)
		{
		case MobileStaticInfo::GateTotemApe:
		case MobileStaticInfo::GateTotemBlank:
		case MobileStaticInfo::GateTotemCow:
		case MobileStaticInfo::GateTotemTiger:
		case MobileStaticInfo::WeepingStone:
		case MobileStaticInfo::WeepingStoneReward:
			_holdType = HoldType::Side;
			_holdRadius = radius2D;
			_loweringMultiplier = 0.7f;
			break;
		case MobileStaticInfo::SingingStone_1:
			_holdType = HoldType::Side;
			_holdRadius = radius2D;
			_loweringMultiplier = 0.4f;
			break;
		case MobileStaticInfo::ToyCuddly:
		case MobileStaticInfo::ToySkittle:
			// a toy whose info's mesh is ObjectToyCuddly or ObjectToySkittle is held from the
			// side, with the default lowering 0
			_holdType = HoldType::Side;
			_holdRadius = radius2D;
			_loweringMultiplier = 0.0f;
			break;
		default:
			break;
		}
	}
}
