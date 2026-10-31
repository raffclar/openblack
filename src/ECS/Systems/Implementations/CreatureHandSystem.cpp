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
#include "Camera/DanceCameraModel.h"
#include "Camera/DefaultWorldCameraModel.h"
#include "Camera/FightCameraModel.h"
#include "Common/X87.h"
#include "Common/X87Vector.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureHandRules.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureSentReactions.h"
#include "Creature/CreatureShownFace.h"
#include "Creature/CreatureTouchStep.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/HandLockedCreature.h"
#include "ECS/Components/HandOnCreature.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/ShownCreatureFace.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreatureScars.h"
#include "ECS/PlayerRelations.h"
#include "ECS/PosedModel.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using openblack::creature::CreatureRig;
namespace feedback = openblack::creature_feedback;

namespace
{
/// The player whose hand this computer moves: the first player, unless a network game gave this computer another god
PlayerNames OwnPlayer()
{
	return Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer() : PlayerNames::PLAYER_ONE;
}

/// The left hand that this computer moves and draws. Only the frames keep their hold of a creature on it.
entt::entity PlayerHand()
{
	return Locator::handSystem::value().GetOwnHands()[static_cast<size_t>(HandSystemInterface::Side::Left)];
}

/// The left hand of a player in the world, none for a player with no hands
std::optional<entt::entity> HandOf(PlayerNames player)
{
	if (!Locator::handSystem::has_value())
	{
		return std::nullopt;
	}
	const auto hands = Locator::handSystem::value().GetHandsOf(player);
	if (!hands.has_value())
	{
		return std::nullopt;
	}
	return (*hands)[static_cast<size_t>(HandSystemInterface::Side::Left)];
}

/// This computer shows the face that a stroke of its hand pulls. The world does not have it, and the other computers do
/// not show it.
void PullShownFace(entt::entity creature, const creature_face::Request& face)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	if (animation == nullptr)
	{
		return;
	}
	if (auto* shown = registry.TryGet<ShownCreatureFace>(creature); shown != nullptr)
	{
		shown->face = creature_shown_face::Pull(shown->face, face);
		return;
	}
	registry.Assign<ShownCreatureFace>(creature, ShownCreatureFace {
	                                                 .face = creature_shown_face::Pull(animation->face, face),
	                                                 .worldFace = animation->face,
	                                                 .olderTurnPose = {},
	                                                 .newerTurnPose = {},
	                                             });
}

/// Whether the hand tells the mind how it treated the creature as it lets go, from how still the creature is now
bool ReleaseGivesFeedback(entt::entity creature)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	if (animation == nullptr)
	{
		return true;
	}
	const bool moving =
	    Locator::creatureLocomotionSystem::has_value() && Locator::creatureLocomotionSystem::value().IsMoving(creature);
	// The face that this computer shows, which can be the face of a stroke
	const auto* shown = registry.TryGet<const ShownCreatureFace>(creature);
	return feedback::ReleaseGivesFeedback({
	    .headStraight = animation->yaw.angle == 0.0f && animation->pitch.angle == 0.0f,
	    .bodyBusy = creature_layers::IsPlaying(animation->body) || moving,
	    .lyingDead = creature_layers::IsLooping(animation->body) && animation->body.holdLoop,
	    .faceShown = creature_shown_face::Shown(animation->face, shown != nullptr ? &shown->face : nullptr).current.has_value(),
	});
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

/// Where each part of the body a stroke can land on is, in the world, as the body is posed at the end of its newest game
/// turn
std::optional<std::array<glm::vec3, feedback::k_BodyPartCount>>
PartsOf(const Creature& creature, const CreatureAnimation& animation, const Transform& transform)
{
	const auto* points = PointsOf(creature);
	if (points == nullptr)
	{
		return std::nullopt;
	}
	const auto pose = creature::TurnPose(animation.newerTurnPose, animation.boneMatrices,
	                                     creature::PlacementMatrix(transform.position, transform.rotation, transform.scale));
	const auto at = [&](uint32_t bone) { return glm::vec3(creature::PosedBone(bone, pose.bones, pose.placement)[3]); };
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
GameHeading HeadingOf(const ecs::Registry& registry, entt::entity creature, const Transform& transform)
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
	// The camera looks at the body where the frame draws it
	const auto transform =
	    DrawnTransform(registry.Get<const Transform>(creature), registry.TryGet<const CreatureAnimation>(creature));
	const auto height = feedback::k_HeightAtSizeOne * ShownSize(registry.Get<const Creature>(creature));
	return creature_interact::ViewOf(transform.position, height, HeadingOf(registry, creature, transform));
}

/// Where a line first touches the creature's mesh as it is posed this frame
std::optional<screen_pick::MeshHit> SkinAlong(entt::entity creature, const Transform& transform, const glm::vec3& origin,
                                              const glm::vec3& direction)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* mesh = registry.TryGet<const Mesh>(creature);
	if (mesh == nullptr || !Locator::resources::has_value())
	{
		return std::nullopt;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(mesh->id))
	{
		return std::nullopt;
	}
	// The drawn bones go with the place where the frame draws the body
	const auto drawn = DrawnTransform(transform, registry.TryGet<const CreatureAnimation>(creature));
	const auto placement = creature::PlacementMatrix(drawn.position, drawn.rotation, drawn.scale);
	const auto hit =
	    ecs::posed_model::NearestSkinHit(registry, creature, *meshes.Handle(mesh->id), placement, origin, direction);
	return hit.has_value() ? std::optional(hit->hit) : std::nullopt;
}

/// The line of sight through a point of the screen, through the camera as it was after a step of the input
std::optional<glm::vec3> DirectionThroughAtStep(glm::vec2 cursor, uint32_t step)
{
	if (!Locator::camera::has_value() || !Locator::windowing::has_value())
	{
		return std::nullopt;
	}
	const auto screenSize = Locator::windowing::value().GetSize();
	if (screenSize.x <= 0 || screenSize.y <= 0)
	{
		return std::nullopt;
	}
	glm::vec3 origin;
	glm::vec3 direction;
	Locator::camera::value().DeprojectAtStep(cursor / glm::vec2(screenSize), step, origin, direction);
	return glm::length(direction) > 0.0f ? std::optional(direction) : std::nullopt;
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
	    .fainting = mind != nullptr && mind->idle.activity == creature_mind::Activity::Faint &&
	                creature_mind::IsUnconscious(mind->idle),
	};
}

/// The hand that holds the creature, none when no hand holds it. The hand of the creature's player can hold it, and
/// the hand of an allied god can hold it too.
std::optional<entt::entity> HandThatHolds(const ecs::Registry& registry, entt::entity creature)
{
	std::optional<entt::entity> holder;
	registry.Each<const HandLockedCreature>([&holder, creature](entt::entity hand, const HandLockedCreature& locked) {
		if (locked.creature == creature)
		{
			holder = hand;
		}
	});
	return holder;
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
	auto& contact = registry.AssignOrReplace<HandOnCreature>(PlayerHand(), HandOnCreature {.creature = *nearest});
	registry.Remove<HandLastFeedback>(PlayerHand());
	// The hand starts a little short of the creature, seen from where the camera is before it turns to the creature
	if (Locator::camera::has_value() && registry.AllOf<Creature, Transform>(*nearest))
	{
		const auto cameraToCreature = x87::Distance<x87::SumOrder::Zyx>(Locator::camera::value().GetOrigin(),
		                                                                registry.Get<const Transform>(*nearest).position);
		contact.distance.Reset(
		    feedback::HandStartDistance(cameraToCreature, ShownSize(registry.Get<const Creature>(*nearest))));
		contact.distanceStarted = true;
	}
	TakeCamera(*nearest);
	// The world takes the hold at the start of a later turn, as a command of the player
	if (Locator::playerCommandSystem::has_value())
	{
		Locator::playerCommandSystem::value().Submit(player_commands::HandLock {.object = *nearest});
	}
	else
	{
		DoLock(OwnPlayer(), *nearest);
	}
	return true;
}

std::optional<entt::entity> CreatureHandSystem::GetLockedCreature(PlayerNames player) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto hand = HandOf(player);
	if (!hand.has_value() || !registry.Valid(*hand))
	{
		return std::nullopt;
	}
	const auto* locked = registry.TryGet<const HandLockedCreature>(*hand);
	return locked != nullptr && registry.Valid(locked->creature) ? std::optional(locked->creature) : std::nullopt;
}

bool CreatureHandSystem::IsLocked(entt::entity creature) const
{
	// The hand of the creature's own player, or the hand of an allied god, holds it
	const auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(creature) && registry.AllOf<Creature>(creature) && HandThatHolds(registry, creature).has_value();
}

void CreatureHandSystem::DoLock(PlayerNames player, entt::entity creature)
{
	// The lock goes on the left hand of the player of the command
	auto& registry = Locator::entitiesRegistry::value();
	const auto hand = HandOf(player);
	if (!hand.has_value() || !registry.Valid(*hand) || !registry.Valid(creature) || !MayHoldFor(player, creature))
	{
		return;
	}
	registry.AssignOrReplace<HandLockedCreature>(*hand, HandLockedCreature {.creature = creature});
	if (const auto logger = spdlog::get("game"))
	{
		SPDLOG_LOGGER_INFO(logger, "The hand of player {} holds creature {}", static_cast<int>(player),
		                   entt::to_integral(creature));
	}
	if (Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().HandTookHold(creature);
	}
}

void CreatureHandSystem::DoUnlock(PlayerNames player, entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto hand = HandOf(player);
	if (!hand.has_value() || !registry.Valid(*hand))
	{
		return;
	}
	if (const auto* locked = registry.TryGet<const HandLockedCreature>(*hand);
	    locked != nullptr && locked->creature == creature)
	{
		registry.Remove<HandLockedCreature>(*hand);
		if (const auto logger = spdlog::get("game"))
		{
			SPDLOG_LOGGER_INFO(logger, "The hand of player {} lets go of creature {}", static_cast<int>(player),
			                   entt::to_integral(creature));
		}
	}
}

void CreatureHandSystem::TakeCamera(entt::entity creature)
{
	if (!Locator::camera::has_value())
	{
		return;
	}
	auto& camera = Locator::camera::value();
	auto& current = camera.GetModel();
	// Already on a creature it stays. Only these cameras give way: the player's own camera, its fight view, Creature
	// Mode's camera and the camera of a dance. A script's camera, the temple's camera and the editor's camera do not.
	if (dynamic_cast<CreatureInteractCameraModel*>(&current) != nullptr)
	{
		return;
	}
	auto* world = dynamic_cast<DefaultWorldCameraModel*>(&current);
	auto* fight = dynamic_cast<FightCameraModel*>(&current);
	if (world == nullptr && fight == nullptr && dynamic_cast<CreatureCameraModel*>(&current) == nullptr &&
	    dynamic_cast<DanceCameraModel*>(&current) == nullptr)
	{
		return;
	}
	_returnTo.reset();
	if (world != nullptr)
	{
		_returnTo = creature_interact::View {.origin = world->GetTargetOrigin(), .focus = world->GetTargetFocus()};
	}
	// If the fight view stops while the hand holds the creature, the player's camera flies back to where the view was
	if (fight != nullptr)
	{
		_returnTo = creature_interact::View {.origin = fight->GetShown().origin, .focus = fight->GetShown().focus};
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
	const auto speed = fight_view::View {.origin = model->GetOriginSpeed(), .focus = model->GetFocusSpeed()};
	auto* world = dynamic_cast<DefaultWorldCameraModel*>(player.get());
	auto* fight = dynamic_cast<FightCameraModel*>(player.get());
	camera.SetModel(std::move(player));
	// The fight view starts again from where the camera now is and how fast it moves
	if (fight != nullptr)
	{
		fight->Return({.origin = origin, .focus = focus}, speed);
		return;
	}
	// The player's camera starts again from where the camera now is and flies back to where it was going
	if (world != nullptr && returnTo.has_value())
	{
		world->PlaceAt(origin, focus);
		world->FlyBack(returnTo->origin, returnTo->focus, creature_interact::WooshesFlyingBack(origin, returnTo->origin));
	}
}

bool CreatureHandSystem::MayHold(entt::entity creature) const
{
	return MayHoldFor(OwnPlayer(), creature);
}

bool CreatureHandSystem::MayHoldFor(PlayerNames player, entt::entity creature)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto holdable = HoldableOf(registry, creature);
	if (!holdable.has_value())
	{
		return false;
	}
	// The god that has the creature must be the ally of the player for the hand of the player to hold it
	const bool allied = holdable->owner != PlayerNames::NEUTRAL && Locator::playerSystem::has_value() &&
	                    player_relations::IsAlly(Locator::playerSystem::value().AllyStrength(holdable->owner, player));
	// The hand of one player only holds the creature at a time
	const auto holder = HandThatHolds(registry, creature);
	const bool heldByOther = holder.has_value() && holder != HandOf(player);
	if (heldByOther || !creature_hand::MayHold(*holdable, player, allied))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "The hand can't hold creature {}: it isn't the player's own or an ally's, another hand holds it, "
		                   "it is an ogre, or it is asleep, frozen or under a script's control",
		                   entt::to_integral(creature));
		return false;
	}
	return true;
}

creature_hand::InteractTip CreatureHandSystem::InteractTipOver(entt::entity creature, bool handInInfluence) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto holdable = HoldableOf(registry, creature);
	if (!holdable.has_value())
	{
		return creature_hand::InteractTip::None;
	}
	const auto* contact = Locator::handSystem::has_value() ? registry.TryGet<const HandOnCreature>(PlayerHand()) : nullptr;
	const bool held = contact != nullptr && contact->creature == creature;
	return creature_hand::InteractTipOf(OwnPlayer(), *holdable, held, handInInfluence);
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
	// The frame finds where the hand's line of sight meets the skin, and the world takes the bruise at the start of a
	// later turn, as a command of the player
	const player_commands::CreatureBruise bruise {
	    .creature = creature, .wound = creature_scars::WoundAlong(creature, rayOrigin, rayDirection, 0, column)};
	if (Locator::playerCommandSystem::has_value())
	{
		Locator::playerCommandSystem::value().Submit(bruise);
		return;
	}
	ApplyBruise(OwnPlayer(), bruise);
}

bool CreatureHandSystem::React(entt::entity creature, size_t animation, bool mirrored,
                               std::optional<creature_face::Request> face, double interruptsAfter)
{
	// The face of a stroke shows at once, also when the body refuses the stroke. Only this computer shows it.
	if (face.has_value())
	{
		PullShownFace(creature, *face);
	}
	auto& minds = Locator::creatureMindSystem::value();
	// The reaction is a command, which the next turn plays, the same at each frame rate and on every computer. The frame
	// only asks whether the creature can take it now, for what the hand counts.
	if (Locator::playerCommandSystem::has_value())
	{
		auto& commands = Locator::playerCommandSystem::value();
		// In a game alone, all commands wait in the queue until a turn does them. An empty queue has no reaction that waits.
		if (!commands.HasSource() && commands.PendingCount() == 0)
		{
			_sentReactions.Clear();
		}
		// A reaction that the hand sent before plays from its start at its turn. It stops a new reaction, as a reaction
		// that plays does.
		const bool takes = creature_hand::TakesReaction(_sentReactions.Waiting(creature), interruptsAfter,
		                                                minds.CanForceAction(creature, animation, mirrored, interruptsAfter));
		// A reaction that the creature refuses sends nothing
		if (!takes)
		{
			if (const auto logger = spdlog::get("game"); logger != nullptr && face.has_value())
			{
				SPDLOG_LOGGER_INFO(logger, "Creature {} refuses reaction {}: only this computer shows its face",
				                   entt::to_integral(creature), animation);
			}
			return false;
		}
		_sentReactions.Sent(creature, true);
		commands.Submit(player_commands::CreatureReaction {
		    .creature = creature,
		    .animation = static_cast<uint32_t>(animation),
		    .mirrored = mirrored,
		    .interruptsAfter = interruptsAfter,
		});
		return true;
	}
	// With no commands, the creature reacts at once
	return minds.ForceAction(creature, animation, mirrored, interruptsAfter);
}

void CreatureHandSystem::DoReaction(PlayerNames player, const player_commands::CreatureReaction& reaction)
{
	// The turn does the reactions of this computer in the order that the hand sent them
	if (Locator::playerCommandSystem::has_value() && Locator::playerCommandSystem::value().GetSender() == player)
	{
		_sentReactions.Done();
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(reaction.creature) || !registry.AllOf<Creature>(reaction.creature) ||
	    !Locator::creatureMindSystem::has_value())
	{
		return;
	}
	if (const auto logger = spdlog::get("game"))
	{
		SPDLOG_LOGGER_INFO(logger, "Creature {} gets reaction {} (mirrored {}) from player {}",
		                   entt::to_integral(reaction.creature), reaction.animation, reaction.mirrored,
		                   static_cast<int>(player));
	}
	Locator::creatureMindSystem::value().ForceAction(reaction.creature, reaction.animation, reaction.mirrored,
	                                                 reaction.interruptsAfter);
}

void CreatureHandSystem::DoBruise(PlayerNames player, const player_commands::CreatureBruise& bruise)
{
	ApplyBruise(player, bruise);
}

void CreatureHandSystem::ApplyBruise(PlayerNames player, const player_commands::CreatureBruise& bruise)
{
	// The bruise and the lost life go to the slapping player's own creature
	if (!Locator::playerSystem::has_value())
	{
		return;
	}
	const auto own = Locator::playerSystem::value().GetPrimaryCreature(player);
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
	if (*own == bruise.creature && bruise.wound.has_value() && Locator::creatureSkinSystem::has_value())
	{
		Locator::creatureSkinSystem::value().AddWound(bruise.creature, *bruise.wound);
	}
}

std::optional<CreatureHandSystem::HandPose> CreatureHandSystem::Update(const glm::vec3& rayOrigin,
                                                                       const glm::vec3& rayDirection, glm::vec2 cursor,
                                                                       float screenWidth, const input_steps::Steps& steps)
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
	const auto height = feedback::k_HeightAtSizeOne * ShownSize(creature);
	const auto centre = transform.position + glm::vec3(0.0f, height * 0.5f, 0.0f);
	// The hand is drawn larger on a bigger creature, by its size to the power of a half, as the game's power makes it
	const auto shownSize = ShownSize(creature);
	const auto handScale = shownSize > 0.0f ? x87::Pow(shownSize, 0.5) : 0.0f;

	// The camera follows the creature, until the player has no creature
	if (_cameraModel != nullptr)
	{
		const bool playerHasCreature =
		    Locator::playerSystem::has_value() && Locator::playerSystem::value().GetPrimaryCreature(OwnPlayer()).has_value();
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
		auto up = UpOf(*contact);
		for (uint32_t i = 0; i < steps.count; ++i)
		{
			up = TurnHand(*contact, true, false, glm::vec3(0.0f), transform.position, input_steps::k_StepSeconds);
		}
		return HandPose {.position = position, .onBody = true, .cycle = contact->cycle, .scale = handScale, .up = up};
	}

	// The hand stays on the line of sight through the cursor, at a distance from the camera that eases to the body
	const auto cameraNow = Locator::camera::has_value() ? Locator::camera::value().GetOrigin() : rayOrigin;
	const auto parts = PartsOf(creature, animation, transform);
	const auto middle = parts.has_value() ? parts->at(static_cast<size_t>(feedback::BodyPart::Groin)) : centre;
	if (!contact->distanceStarted)
	{
		contact->distance.Reset(
		    feedback::HandStartDistance(x87::Distance<x87::SumOrder::Zyx>(cameraNow, transform.position), ShownSize(creature)));
		contact->distanceStarted = true;
	}

	// The hand, its strokes and its slaps step at the fixed steps of the input. Each step takes the cursor at its place
	// between the last frame and this frame.
	const auto cursorPixel = glm::ivec2(cursor);
	const auto frameCursorBefore = contact->frameCursor.value_or(cursorPixel);
	auto stepCursorBefore = frameCursorBefore;
	auto onBody = contact->wasOnBody;
	auto up = UpOf(*contact);
	for (uint32_t i = 0; i < steps.count; ++i)
	{
		// Each step looks through the camera as it was after the same step of the camera
		const auto stepCursor = creature_hand::StepCursor(frameCursorBefore, cursorPixel, steps, i);
		const bool now = i + 1 == steps.count && stepCursor == cursorPixel;
		const auto camera = now || !Locator::camera::has_value() ? cameraNow : Locator::camera::value().GetStepOrigin(i);
		const auto direction = now ? rayDirection : DirectionThroughAtStep(glm::vec2(stepCursor), i).value_or(rayDirection);
		const auto sight = feedback::SightWay(camera, direction);
		feedback::KeepHandBack(contact->distance, x87::Distance<x87::SumOrder::Zyx>(camera, middle), height);
		auto handDistance = feedback::HandDistanceFromCamera(contact->distance);

		// The line through the cursor looks for the creature's posed mesh. When it does not touch the mesh, a second line
		// looks through the middle of the cursor's places at this step and at the last step.
		auto hit = SkinAlong(creatureEntity, transform, camera, sight);
		if (!hit.has_value())
		{
			const auto between = feedback::SecondTryCursor(stepCursor, stepCursorBefore);
			if (const auto second = DirectionThroughAtStep(glm::vec2(between), now ? steps.count : i))
			{
				hit = SkinAlong(creatureEntity, transform, camera, feedback::SightWay(camera, *second));
			}
		}
		onBody = hit.has_value();
		const auto touch = onBody ? std::optional(hit->point) : std::nullopt;
		contact->heldMs += input_steps::k_StepMs;

		// Strokes and slaps are weighed at each step, as the game weighed them once a frame at the screen's refresh
		const auto lastDirection = DirectionThroughAtStep(contact->lastCursor, now ? steps.count : i).value_or(direction);
		const bool slapped =
		    Step(*contact, {.shownAt = contact->shownAt.value_or(feedback::PointOnSight(camera, direction, handDistance)),
		                    .here = feedback::PointOnSight(camera, direction, handDistance),
		                    .before = feedback::PointOnSight(camera, lastDirection, handDistance),
		                    .touch = touch,
		                    .cursor = glm::vec2(stepCursor),
		                    .screenWidth = screenWidth,
		                    .stepMs = input_steps::k_StepMs,
		                    .rayOrigin = rayOrigin,
		                    .rayDirection = direction});
		// On the body, the hand eases to a place a little short of the mesh. A slap leaves the distance as it is.
		// The mesh's way out at the touch comes from the order of the triangle's corners. A slap leaves it straight up.
		auto normal = glm::vec3(0.0f, 1.0f, 0.0f);
		if (onBody && !slapped)
		{
			feedback::RestHandOnBody(contact->distance, x87::Distance<x87::SumOrder::Xzy>(camera, hit->point), height);
			handDistance = feedback::HandDistanceFromCamera(contact->distance);
			normal = hit->windingNormal;
		}
		contact->distance.UpdateInline(input_steps::k_StepSeconds);
		contact->shownAt = feedback::PointOnSight(camera, direction, handDistance);
		contact->shownDistance = handDistance;

		// The hand turns to the body where it touches it, as the cursor moves across the screen
		const bool cursorMovedAcross = stepCursorBefore.x != stepCursor.x;
		up = TurnHand(*contact, onBody, cursorMovedAcross, normal, transform.position, input_steps::k_StepSeconds);
		stepCursorBefore = stepCursor;
	}
	contact->frameCursor = cursorPixel;
	// The hand shows on the line of sight through the cursor, at its distance at the last step
	const auto shownDistance = contact->shownDistance.value_or(feedback::HandDistanceFromCamera(contact->distance));
	const auto position = feedback::PointOnSight(cameraNow, rayDirection, shownDistance);

	// The creature looks at the hand, or at the camera with the hand near its middle. The turn sets the look from the
	// hand that the player reported, in a game alone and in a network game.
	return HandPose {.position = position, .onBody = onBody, .cycle = contact->cycle, .scale = handScale, .up = up};
}

void CreatureHandSystem::LookAtReportedHand()
{
	if (IsHeldByCommand())
	{
		return;
	}
	// Each player who holds a creature makes it look at the hand of that player
	for (auto player = static_cast<size_t>(PlayerNames::PLAYER_ONE); player <= static_cast<size_t>(PlayerNames::PLAYER_SEVEN);
	     ++player)
	{
		LookAtReportedHandOf(static_cast<PlayerNames>(player));
	}
}

void CreatureHandSystem::LookAtReportedHandOf(PlayerNames player)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto creatureEntity = GetLockedCreature(player);
	if (!creatureEntity.has_value() || !registry.AllOf<Creature, CreatureAnimation, Transform>(*creatureEntity))
	{
		return;
	}
	const auto& handSystem = Locator::handSystem::value();
	const auto hand = handSystem.GetHandPositionsOf(player).at(static_cast<size_t>(HandSystemInterface::Side::Left));
	const auto camera = handSystem.GetReportedCamera(player);
	if (!hand.has_value() || !camera.has_value())
	{
		return;
	}
	const auto& creature = registry.Get<const Creature>(*creatureEntity);
	const auto& transform = registry.Get<const Transform>(*creatureEntity);
	const auto parts = PartsOf(creature, registry.Get<const CreatureAnimation>(*creatureEntity), transform);
	if (!parts.has_value())
	{
		return;
	}
	const auto middle = parts->at(static_cast<size_t>(feedback::BodyPart::Groin));
	const auto height = feedback::k_HeightAtSizeOne * ShownSize(creature);
	const auto look = feedback::ReportedLookPoint(*hand, middle, camera->origin, height);
	if (!look.has_value())
	{
		return;
	}
	registry.Get<CreatureAnimation>(*creatureEntity).lookAt = *look;
	if (auto* eyes = registry.TryGet<CreatureEyes>(*creatureEntity))
	{
		eyes->lookAt = *look;
	}
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
	return UpOf(contact);
}

glm::vec3 CreatureHandSystem::UpOf(const HandOnCreature& contact)
{
	const auto up = contact.up.GetValue();
	return glm::length(up) > 0.0f ? x87::Scaled<x87::SumOrder::Zyx>(up) : glm::vec3(0.0f, 1.0f, 0.0f);
}

bool CreatureHandSystem::Step(HandOnCreature& contact, const StepInput& input)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto creatureEntity = contact.creature;
	const auto& creature = registry.Get<const Creature>(creatureEntity);
	const auto& animation = registry.Get<const CreatureAnimation>(creatureEntity);
	const auto& transform = registry.Get<const Transform>(creatureEntity);
	const auto ground = Locator::terrainSystem::has_value()
	                        ? Locator::terrainSystem::value().GetHeightAt(glm::xz(input.shownAt))
	                        : transform.position.y;
	const creature_hand::TouchAnswers answers {
	    .slap =
	        [&](const feedback::Slap& slap) {
		        return React(creatureEntity, slap.animation, slap.mirrored, std::nullopt, feedback::k_SlapInterruptsAfter);
	        },
	    .stroke =
	        [&](feedback::BodyPart part) {
		        const auto index = static_cast<size_t>(part);
		        return React(creatureEntity, feedback::k_RewardAnimations.at(index), feedback::k_RewardMirrored.at(index),
		                     feedback::RewardFace(part), feedback::k_StrokeInterruptsAfter);
	        },
	    .nearestPart = [&]() -> std::optional<feedback::BodyPart> {
		    const auto parts = PartsOf(creature, animation, transform);
		    if (!parts.has_value() || !input.touch.has_value())
		    {
			    return std::nullopt;
		    }
		    return feedback::NearestPart(*input.touch, *parts);
	    },
	    .bruise = [&](uint8_t column) { Bruise(creatureEntity, input.rayOrigin, input.rayDirection, column); },
	};
	return creature_hand::WeighTouchStep(contact,
	                                     {
	                                         .stepMs = input.stepMs,
	                                         .here = input.here,
	                                         .before = input.before,
	                                         .onBody = input.touch.has_value(),
	                                         .handHeight = input.shownAt.y - ground,
	                                         .cursor = input.cursor,
	                                         .screenWidth = input.screenWidth,
	                                         .creatureHeight = feedback::k_HeightAtSizeOne * ShownSize(creature),
	                                     },
	                                     answers);
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
	const bool byCommand = contact->byCommand;
	registry.Remove<HandOnCreature>(hand);
	registry.AssignOrReplace<HandLastFeedback>(hand, HandLastFeedback {.sum = sum});
	GiveCameraBack();
	if (registry.Valid(creature) && Locator::creatureMindSystem::has_value() && ReleaseGivesFeedback(creature))
	{
		// The creature feels the strokes and the slaps at the start of the next turn, as a command of the player
		if (Locator::playerCommandSystem::has_value())
		{
			Locator::playerCommandSystem::value().Submit(
			    player_commands::CreatureFeedback {.creature = creature, .amount = delivered});
		}
		else
		{
			Locator::creatureMindSystem::value().SendReleaseFeedback(creature, delivered);
		}
		// Too slight to count, as a click is, the hand offers to focus on the creature
		_focusTip = std::abs(delivered) <= feedback::k_SlightFeedback;
	}
	// The world lets go of the creature at the start of a later turn, as a command of the player. A hold that a command
	// started ends at once.
	if (!byCommand && Locator::playerCommandSystem::has_value())
	{
		Locator::playerCommandSystem::value().Submit(player_commands::HandUnlock {.object = creature});
	}
	else
	{
		DoUnlock(OwnPlayer(), creature);
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
	// A command holds it in the world at once
	registry.AssignOrReplace<HandLockedCreature>(hand, HandLockedCreature {.creature = creature});
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
	// The face of the stroke shows at once on this computer, also when the body refuses the stroke
	PullShownFace(creature, feedback::RewardFace(part));
	if (!Locator::creatureMindSystem::value().ForceAction(creature, feedback::k_RewardAnimations.at(index),
	                                                      feedback::k_RewardMirrored.at(index),
	                                                      feedback::k_StrokeInterruptsAfter))
	{
		return false;
	}
	contact->sum = feedback::AfterStroke(contact->sum);
	feedback::Touched(contact->clocks);
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
	if (!Locator::creatureMindSystem::value().ForceAction(creature, slap->animation, slap->mirrored,
	                                                      feedback::k_SlapInterruptsAfter))
	{
		return false;
	}
	contact->sum = feedback::AfterSlap(contact->sum, slap->gentle);
	feedback::Touched(contact->clocks);
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
