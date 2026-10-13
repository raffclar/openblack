/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureHandSystem.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <memory>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/LandIslandInterface.h"
#include "Camera/Camera.h"
#include "Camera/CreatureCameraModel.h"
#include "Camera/CreatureInteractCameraModel.h"
#include "Camera/DefaultWorldCameraModel.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureHandRules.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureRig.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/HandOnCreature.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreatureScars.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using openblack::creature::CreatureRig;
namespace feedback = openblack::creature_feedback;

namespace
{
constexpr PlayerNames k_LocalPlayer = PlayerNames::PLAYER_ONE;

entt::entity PlayerHand()
{
	return Locator::handSystem::value().GetPlayerHands()[static_cast<size_t>(HandSystemInterface::Side::Left)];
}

const CreatureRig::ActionPoints* PointsOf(const Creature& creature)
{
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto id = creature::GetRigId(creature.species);
	if (!rigs.Contains(id))
	{
		return nullptr;
	}
	const auto& rig = *rigs.Handle(id);
	return rig.actionPoints.has_value() ? &*rig.actionPoints : nullptr;
}

/// The creature's body as the hand can touch it this frame: capsules round its bones, in the world
std::vector<feedback::Capsule> BodyOf(const Creature& creature, const CreatureAnimation& animation, const Transform& transform)
{
	return feedback::BodyCapsules(animation.skeleton.parents, animation.boneMatrices,
	                              creature::PlacementMatrix(transform.position, transform.rotation, transform.scale),
	                              feedback::k_BodyRadiusShare * feedback::k_HeightAtSizeOne * ShownSize(creature));
}

/// Where each part of the body a stroke can land on is this frame, in the world
std::optional<std::array<glm::vec3, feedback::k_BodyPartCount>>
PartsOf(const Creature& creature, const CreatureAnimation& animation, const Transform& transform)
{
	const auto* points = PointsOf(creature);
	if (points == nullptr)
	{
		return std::nullopt;
	}
	const auto placement = creature::PlacementMatrix(transform.position, transform.rotation, transform.scale);
	const auto at = [&](uint32_t bone) { return glm::vec3(creature::PosedBone(bone, animation.boneMatrices, placement)[3]); };
	const auto mirror = [&](uint32_t bone) { return bone < animation.mirror.size() ? animation.mirror[bone] : bone; };
	return std::array<glm::vec3, feedback::k_BodyPartCount> {
	    at(points->head),
	    at(points->rightArmpit),
	    at(mirror(points->rightArmpit)),
	    at(points->belly),
	    at(points->groin),
	    at(points->rightFoot),
	    at(mirror(points->rightFoot)),
	    at(points->rightHand),
	    at(mirror(points->rightHand)),
	};
}

/// The heading the creature's body is drawn at, or the one it is turning to while it turns
float HeadingOf(const ecs::Registry& registry, entt::entity creature, const Transform& transform)
{
	if (const auto* walking = registry.TryGet<const CreatureLocomotion>(creature); walking != nullptr && walking->started)
	{
		const bool turning =
		    walking->motion == CreatureLocomotion::Motion::Turning || walking->motion == CreatureLocomotion::Motion::Stepping;
		return turning ? walking->targetHeading : walking->heading;
	}
	const auto ahead = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
	return creature_locomotion::HeadingOf({ahead.x, ahead.z});
}

/// The camera's place for the creature as it now stands
std::optional<creature_interact::View> InteractViewOf(const ecs::Registry& registry, entt::entity creature)
{
	if (!registry.Valid(creature) || !registry.AllOf<Creature, Transform>(creature))
	{
		return std::nullopt;
	}
	const auto& transform = registry.Get<const Transform>(creature);
	const auto height = feedback::k_HeightAtSizeOne * ShownSize(registry.Get<const Creature>(creature));
	return creature_interact::ViewOf(transform.position, height, HeadingOf(registry, creature, transform));
}

/// Where a line of sight meets the upright plane through the creature that faces back along it
std::optional<glm::vec3> OnPlaneThrough(const glm::vec3& centre, const glm::vec3& origin, const glm::vec3& direction)
{
	auto normal = glm::vec3(direction.x, 0.0f, direction.z);
	if (glm::length(normal) < 1e-4f)
	{
		normal = glm::vec3(0.0f, 0.0f, 1.0f);
	}
	normal = glm::normalize(normal);
	const auto facing = glm::dot(direction, normal);
	if (std::abs(facing) < 1e-6f)
	{
		return std::nullopt;
	}
	const auto along = glm::dot(centre - origin, normal) / facing;
	return along > 0.0f ? std::optional(origin + (direction * along)) : std::nullopt;
}

/// What the hand needs to know about a creature to take hold of it; none for what isn't a creature
std::optional<creature_hand::Holdable> HoldableOf(const ecs::Registry& registry, entt::entity creature)
{
	const auto* body = registry.Valid(creature) ? registry.TryGet<const Creature>(creature) : nullptr;
	if (body == nullptr)
	{
		return std::nullopt;
	}
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	const auto* spells = registry.TryGet<const CreatureSpells>(creature);
	return creature_hand::Holdable {
	    .owner = body->owner,
	    .species = body->species,
	    .asleep = mind != nullptr && creature_mind::IsAsleep(mind->idle),
	    .frozen = spells != nullptr && spells->spells.IsActive(creature_spells::Spell::Freeze),
	    .scriptControlled = registry.AllOf<ScriptControlled>(creature),
	};
}
} // namespace

bool CreatureHandSystem::Grab()
{
	const auto nearest = CreatureUnderCursor();
	if (!nearest.has_value() || !MayHold(*nearest))
	{
		return false;
	}
	auto& registry = Locator::entitiesRegistry::value();
	registry.AssignOrReplace<HandOnCreature>(PlayerHand(), HandOnCreature {.creature = *nearest});
	registry.Remove<HandLastFeedback>(PlayerHand());
	TakeCamera(*nearest);
	if (Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().HandTookHold(*nearest);
	}
	return true;
}

void CreatureHandSystem::TakeCamera(entt::entity creature)
{
	if (!Locator::camera::has_value())
	{
		return;
	}
	auto& camera = Locator::camera::value();
	auto& current = camera.GetModel();
	// Already on a creature it stays; only the player's own camera, or Creature Mode's, gives way: not a fight's, a
	// script's, the temple's or the editor's
	if (dynamic_cast<CreatureInteractCameraModel*>(&current) != nullptr)
	{
		return;
	}
	auto* world = dynamic_cast<DefaultWorldCameraModel*>(&current);
	if (world == nullptr && dynamic_cast<CreatureCameraModel*>(&current) == nullptr)
	{
		return;
	}
	_returnTo.reset();
	if (world != nullptr)
	{
		_returnTo = creature_interact::View {.origin = world->GetTargetOrigin(), .focus = world->GetTargetFocus()};
	}
	auto model = std::make_unique<CreatureInteractCameraModel>(camera.GetOrigin(), camera.GetFocus());
	model->SetTarget(InteractViewOf(Locator::entitiesRegistry::value(), creature));
	_cameraModel = model.get();
	_cameraModel->SetPlayerModel(camera.SetModel(std::move(model)));
}

void CreatureHandSystem::GiveCameraBack()
{
	auto* model = std::exchange(_cameraModel, nullptr);
	const auto returnTo = std::exchange(_returnTo, std::nullopt);
	if (model == nullptr || !Locator::camera::has_value())
	{
		return;
	}
	auto& camera = Locator::camera::value();
	// Something else may have taken the camera over meanwhile, taking ours with it
	if (&camera.GetModel() != model)
	{
		return;
	}
	auto player = model->TakePlayerModel();
	if (player == nullptr)
	{
		return;
	}
	const auto origin = camera.GetOrigin();
	const auto focus = camera.GetFocus();
	auto* world = dynamic_cast<DefaultWorldCameraModel*>(player.get());
	camera.SetModel(std::move(player));
	// The player's camera starts again from where the camera now is and flies back to where it was going
	if (world != nullptr && returnTo.has_value())
	{
		world->PlaceAt(origin, focus);
		world->FlyBack(returnTo->origin, returnTo->focus, creature_interact::WooshesFlyingBack(origin, returnTo->origin));
	}
}

bool CreatureHandSystem::MayHold(entt::entity creature) const
{
	const auto holdable = HoldableOf(Locator::entitiesRegistry::value(), creature);
	if (!holdable.has_value())
	{
		return false;
	}
	if (!creature_hand::MayHold(*holdable, k_LocalPlayer))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "The hand can't hold creature {}: it isn't the player's own, is an ogre, or is asleep, frozen "
		                   "or under a script's control",
		                   entt::to_integral(creature));
		return false;
	}
	return true;
}

bool CreatureHandSystem::ShowsInteractTip(entt::entity creature, bool handInInfluence) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto holdable = HoldableOf(registry, creature);
	if (!holdable.has_value())
	{
		return false;
	}
	const auto* contact = Locator::handSystem::has_value() ? registry.TryGet<const HandOnCreature>(PlayerHand()) : nullptr;
	const bool held = contact != nullptr && contact->creature == creature;
	return creature_hand::ShowsInteractTip(k_LocalPlayer, *holdable, held, handInInfluence);
}

bool CreatureHandSystem::IsClick() const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* contact = Locator::handSystem::has_value() ? registry.TryGet<const HandOnCreature>(PlayerHand()) : nullptr;
	return contact != nullptr && !contact->byCommand && creature_hand::IsClick(contact->heldMs, contact->strokedOrSlapped);
}

std::optional<entt::entity> CreatureHandSystem::CreatureUnderCursor() const
{
	// The creature the interface picked under the cursor as the last frame was drawn
	if (!Locator::handSystem::has_value() || !Locator::pickingSystem::has_value())
	{
		return std::nullopt;
	}
	const auto& picked = Locator::pickingSystem::value().GetPick().object;
	if (!picked.has_value() || !Locator::entitiesRegistry::value().AllOf<Creature>(*picked))
	{
		return std::nullopt;
	}
	return picked;
}

void CreatureHandSystem::Bruise(entt::entity creature, const glm::vec3& rayOrigin, const glm::vec3& rayDirection,
                                uint8_t column)
{
	// The bruise and the lost life go to the player's own creature, where the hand's line of sight meets its skin
	if (!Locator::playerSystem::has_value())
	{
		return;
	}
	const auto own = Locator::playerSystem::value().GetPrimaryCreature(k_LocalPlayer);
	if (!own.has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* needs = registry.TryGet<CreatureNeeds>(*own))
	{
		needs->needs.life = feedback::LifeAfterBruise(needs->needs.life);
	}
	// TODO(creature): hard slaps on another god's creature bruise the player's own creature in the game, at the place
	// hit on the slapped one's skin; only a bruise on the slapped creature itself is drawn here (docs/vanilla-bugs.md)
	if (*own == creature)
	{
		creature_scars::MarkAlong(creature, rayOrigin, rayOrigin + rayDirection, 0, column);
	}
}

std::optional<CreatureHandSystem::HandPose> CreatureHandSystem::Update(const glm::vec3& rayOrigin,
                                                                       const glm::vec3& rayDirection, glm::vec2 cursor,
                                                                       float screenWidth, float seconds)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* contact = Locator::handSystem::has_value() ? registry.TryGet<HandOnCreature>(PlayerHand()) : nullptr;
	if (contact == nullptr)
	{
		return std::nullopt;
	}
	const auto creatureEntity = contact->creature;
	if (!registry.Valid(creatureEntity) || !registry.AllOf<Creature, CreatureAnimation, Transform>(creatureEntity))
	{
		registry.Remove<HandOnCreature>(PlayerHand());
		GiveCameraBack();
		return std::nullopt;
	}
	const auto& creature = registry.Get<const Creature>(creatureEntity);
	const auto& animation = registry.Get<const CreatureAnimation>(creatureEntity);
	const auto& transform = registry.Get<const Transform>(creatureEntity);
	const auto ms = seconds * 1000.0f;
	const auto height = feedback::k_HeightAtSizeOne * ShownSize(creature);
	const auto centre = transform.position + glm::vec3(0.0f, height * 0.5f, 0.0f);
	// The hand is drawn larger on a bigger creature, by the square root of its size
	const auto handScale = std::sqrt(std::max(ShownSize(creature), 0.0f));

	// The camera follows the creature, until the player has no creature
	if (_cameraModel != nullptr)
	{
		const bool playerHasCreature =
		    Locator::playerSystem::has_value() && Locator::playerSystem::value().GetPrimaryCreature(k_LocalPlayer).has_value();
		_cameraModel->SetTarget(playerHasCreature ? InteractViewOf(registry, creatureEntity) : std::nullopt);
	}

	// Held by a command, the hand stays on the part it last stroked, or where it last slapped, whatever the cursor does
	if (contact->byCommand)
	{
		if (contact->lastPart.has_value())
		{
			if (const auto parts = PartsOf(creature, animation, transform))
			{
				contact->lastPoint = parts->at(static_cast<size_t>(*contact->lastPart));
			}
		}
		const auto position = contact->lastPoint.value_or(centre);
		return HandPose {.position = position,
		                 .onBody = true,
		                 .cycle = contact->cycle,
		                 .scale = handScale,
		                 .up = TurnHand(*contact, true, false, glm::vec3(0.0f), transform.position, seconds)};
	}

	// Where the hand is: on the body under the cursor, or beside it on the plane through the creature
	const auto onPlane = OnPlaneThrough(centre, rayOrigin, rayDirection);
	const auto body = BodyOf(creature, animation, transform);
	const auto hit = feedback::RayHit(rayOrigin, rayDirection, body);
	const auto touch = hit.has_value() ? std::optional(rayOrigin + (rayDirection * *hit)) : std::nullopt;
	const auto point = onPlane.value_or(touch.value_or(centre));
	const bool onBody = touch.has_value();
	contact->heldMs += ms;

	// Strokes and slaps are weighed in steps at the screen's refresh rate, as the game weighed them once a frame
	const auto step = feedback::Pace(contact->sinceStepMs, ms);
	if (step.has_value() && *step > 0.0f)
	{
		Step(*contact, {.point = point,
		                .touch = touch,
		                .cursor = cursor,
		                .screenWidth = screenWidth,
		                .stepMs = *step,
		                .rayOrigin = rayOrigin,
		                .rayDirection = rayDirection});
	}
	// The hand turns to the body where it touches it, as the cursor moves across the screen
	const bool cursorMovedAcross = contact->frameCursorX.has_value() && *contact->frameCursorX != cursor.x;
	contact->frameCursorX = cursor.x;
	const auto normal = touch.has_value() ? feedback::SurfaceNormal(*touch, body) : glm::vec3(0.0f, 1.0f, 0.0f);
	const auto up = TurnHand(*contact, onBody, cursorMovedAcross, normal, transform.position, seconds);
	const auto position = touch.value_or(point);

	// The creature looks at the hand, or at the camera with the hand near its middle
	if (const auto parts = PartsOf(creature, animation, transform); parts.has_value() && Locator::camera::has_value())
	{
		const auto middle = parts->at(static_cast<size_t>(feedback::BodyPart::Groin));
		const auto look = feedback::HeldLookPoint(position, middle, Locator::camera::value().GetOrigin(), rayDirection, height);
		registry.Get<CreatureAnimation>(creatureEntity).lookAt = look;
		if (auto* eyes = registry.TryGet<CreatureEyes>(creatureEntity))
		{
			eyes->lookAt = look;
		}
	}
	return HandPose {.position = position, .onBody = onBody, .cycle = contact->cycle, .scale = handScale, .up = up};
}

glm::vec3 CreatureHandSystem::TurnHand(HandOnCreature& contact, bool onBody, bool cursorMovedAcross, const glm::vec3& normal,
                                       const glm::vec3& creaturePosition, float seconds)
{
	const auto camera = Locator::camera::has_value() ? Locator::camera::value().GetOrigin() : creaturePosition;
	if (!contact.upStarted)
	{
		// Taking hold, the hand starts with its back to the camera
		const auto start = feedback::HandUpOnTakingHold(creaturePosition, camera);
		contact.up.Reset(start);
		contact.up.SetDestination(start, feedback::k_HandStartSeconds);
		contact.upStarted = true;
	}
	if (const auto glide = feedback::HandUpGlideTo(onBody, cursorMovedAcross, normal, creaturePosition, camera))
	{
		contact.up.SetDestination(glide->destination, glide->seconds);
	}
	contact.up.Update(seconds);
	const auto up = contact.up.GetValue();
	return glm::length(up) > 0.0f ? glm::normalize(up) : glm::vec3(0.0f, 1.0f, 0.0f);
}

void CreatureHandSystem::Step(HandOnCreature& contact, const StepInput& input)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto creatureEntity = contact.creature;
	const auto& creature = registry.Get<const Creature>(creatureEntity);
	const auto& animation = registry.Get<const CreatureAnimation>(creatureEntity);
	const auto& transform = registry.Get<const Transform>(creatureEntity);
	const auto height = feedback::k_HeightAtSizeOne * ShownSize(creature);
	const bool onBody = input.touch.has_value();

	auto& clocks = contact.clocks;
	clocks.sinceTouch += input.stepMs;
	clocks.ready += input.stepMs;
	contact.slapReadyMs += input.stepMs;
	// How fast the hand moves across the land, up and down not counting
	const auto moved = contact.lastPoint.has_value() ? glm::xz(input.point) - glm::xz(*contact.lastPoint) : glm::vec2(0.0f);
	contact.speed = glm::length(moved) / (input.stepMs / 1000.0f);
	const bool cursorMoved = input.cursor.x != contact.lastCursor.x;
	bool stroking = false;

	auto& minds = Locator::creatureMindSystem::value();
	const auto ground = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::xz(input.point))
	                                                        : transform.position.y;
	std::optional<feedback::Slap> slap;
	if (feedback::SlapConsidered(onBody, contact.wasOnBody, input.cursor.x, contact.lastCursor.x, input.screenWidth * 0.5f) &&
	    contact.slapReadyMs > feedback::k_SlapReadyMs)
	{
		slap = feedback::ClassifySlap(input.point.y - ground, contact.speed, height, input.cursor.x < contact.lastCursor.x);
	}
	if (slap.has_value())
	{
		if (feedback::CountSlap(contact.hardSlaps, *slap, onBody))
		{
			Bruise(creatureEntity, input.rayOrigin, input.rayDirection, feedback::BruiseColumn(contact.speed, height));
		}
		// The slap only counts when the creature can reel from it
		if (minds.ForceAction(creatureEntity, slap->animation, slap->mirrored, std::nullopt, feedback::k_SlapInterruptsAfter))
		{
			clocks.sinceTouch = 0.0f;
			clocks.ready = feedback::k_ReadyAfterTouchMs;
			contact.sum = feedback::AfterSlap(contact.sum, slap->gentle);
			contact.strokedOrSlapped = true;
		}
		clocks.contact = 0.0f;
	}
	else if (!onBody)
	{
		contact.lastPart.reset();
	}
	else
	{
		// Resting on the body long enough, the moving hand caresses it, and strokes the part it is nearest when one is due
		clocks.contact += input.stepMs;
		const auto parts =
		    clocks.contact > feedback::k_StrokeHoldMs && cursorMoved ? PartsOf(creature, animation, transform) : std::nullopt;
		if (parts.has_value())
		{
			stroking = true;
			const auto part = feedback::NearestPart(*input.touch, *parts);
			const auto index = static_cast<size_t>(part);
			if (feedback::StrokeDue(clocks, contact.lastPart, part, contact.sum) &&
			    minds.ForceAction(creatureEntity, feedback::k_RewardAnimations.at(index), feedback::k_RewardMirrored.at(index),
			                      feedback::RewardFace(part), feedback::k_StrokeInterruptsAfter))
			{
				contact.lastPart = part;
				clocks.sinceTouch = 0.0f;
				clocks.ready = feedback::k_ReadyAfterTouchMs;
				contact.slapReadyMs = 0.0f;
				contact.sum = feedback::AfterStroke(contact.sum);
				contact.strokedOrSlapped = true;
			}
		}
	}
	const bool fast = contact.speed > feedback::k_SlapSpeed * height;
	contact.cycle = feedback::NextCycle(contact.cycle, fast, stroking, onBody);
	contact.wasOnBody = onBody;
	contact.lastPoint = input.point;
	contact.lastCursor = input.cursor;
}

void CreatureHandSystem::Release()
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!Locator::handSystem::has_value())
	{
		return;
	}
	const auto hand = PlayerHand();
	const auto* contact = registry.TryGet<const HandOnCreature>(hand);
	if (contact == nullptr)
	{
		GiveCameraBack();
		return;
	}
	const auto creature = contact->creature;
	const auto sum = contact->sum;
	const auto delivered = feedback::Delivered(sum);
	registry.Remove<HandOnCreature>(hand);
	registry.AssignOrReplace<HandLastFeedback>(hand, HandLastFeedback {.sum = sum});
	GiveCameraBack();
	if (registry.Valid(creature) && Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().ReceiveFeedback(creature, delivered);
		// Too slight to count, as a click is, the hand offers to focus on the creature
		_focusTip = std::abs(delivered) <= feedback::k_SlightFeedback;
	}
}

std::optional<entt::entity> CreatureHandSystem::GetCreature() const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* contact = Locator::handSystem::has_value() ? registry.TryGet<const HandOnCreature>(PlayerHand()) : nullptr;
	return contact != nullptr ? std::optional(contact->creature) : std::nullopt;
}

float CreatureHandSystem::GetFeedbackSum() const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* contact = Locator::handSystem::has_value() ? registry.TryGet<const HandOnCreature>(PlayerHand()) : nullptr;
	return contact != nullptr ? contact->sum : 0.0f;
}

HandOnCreature* CreatureHandSystem::HoldByCommand(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!Locator::handSystem::has_value() || !registry.Valid(creature) ||
	    !registry.AllOf<Creature, CreatureAnimation, Transform>(creature) || !MayHold(creature))
	{
		return nullptr;
	}
	const auto hand = PlayerHand();
	if (auto* contact = registry.TryGet<HandOnCreature>(hand); contact != nullptr && contact->creature == creature)
	{
		contact->byCommand = true;
		return contact;
	}
	// Held to another creature, the hand lets go of it first, which tells it how it was treated
	Release();
	registry.Remove<HandLastFeedback>(hand);
	return &registry.AssignOrReplace<HandOnCreature>(hand, HandOnCreature {.creature = creature, .byCommand = true});
}

bool CreatureHandSystem::Stroke(entt::entity creature, feedback::BodyPart part)
{
	auto* contact = HoldByCommand(creature);
	if (contact == nullptr || !Locator::creatureMindSystem::has_value())
	{
		return false;
	}
	const auto index = static_cast<size_t>(part);
	contact->lastPart = part;
	contact->cycle = feedback::TouchCycle::Tickle;
	if (!Locator::creatureMindSystem::value().ForceAction(creature, feedback::k_RewardAnimations.at(index),
	                                                      feedback::k_RewardMirrored.at(index), feedback::RewardFace(part),
	                                                      feedback::k_StrokeInterruptsAfter))
	{
		return false;
	}
	contact->sum = feedback::AfterStroke(contact->sum);
	contact->clocks.sinceTouch = 0.0f;
	contact->clocks.ready = feedback::k_ReadyAfterTouchMs;
	contact->slapReadyMs = 0.0f;
	return true;
}

bool CreatureHandSystem::Slap(entt::entity creature, float heightShare, bool gentle, bool sweepsRight)
{
	auto* contact = HoldByCommand(creature);
	if (contact == nullptr || !Locator::creatureMindSystem::has_value())
	{
		return false;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto height = feedback::k_HeightAtSizeOne * ShownSize(registry.Get<const Creature>(creature));
	// As fast across the body as a gentle slap or a hard one is
	const auto speed =
	    (gentle ? (feedback::k_SlapSpeed + feedback::k_HardSlapSpeed) * 0.5f : feedback::k_HardSlapSpeed * 1.5f) * height;
	const auto slap = feedback::ClassifySlap(heightShare * height, speed, height, !sweepsRight);
	if (!slap.has_value())
	{
		return false;
	}
	contact->lastPart.reset();
	contact->lastPoint = registry.Get<const Transform>(creature).position + glm::vec3(0.0f, heightShare * height, 0.0f);
	contact->cycle = feedback::TouchCycle::Slap;
	if (!Locator::creatureMindSystem::value().ForceAction(creature, slap->animation, slap->mirrored, std::nullopt,
	                                                      feedback::k_SlapInterruptsAfter))
	{
		return false;
	}
	contact->sum = feedback::AfterSlap(contact->sum, slap->gentle);
	contact->clocks.sinceTouch = 0.0f;
	contact->clocks.ready = feedback::k_ReadyAfterTouchMs;
	return true;
}

bool CreatureHandSystem::IsHeldByCommand() const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* contact = Locator::handSystem::has_value() ? registry.TryGet<const HandOnCreature>(PlayerHand()) : nullptr;
	return contact != nullptr && contact->byCommand;
}

float CreatureHandSystem::GetLastFeedbackSum() const
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!Locator::handSystem::has_value())
	{
		return 0.0f;
	}
	if (const auto* contact = registry.TryGet<const HandOnCreature>(PlayerHand()))
	{
		return contact->sum;
	}
	const auto* last = registry.TryGet<const HandLastFeedback>(PlayerHand());
	return last != nullptr ? last->sum : 0.0f;
}
