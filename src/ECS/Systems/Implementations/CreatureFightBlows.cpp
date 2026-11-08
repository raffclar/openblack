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
#include <cstdint>

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "3D/CreatureBody.h"
#include "Common/GameRandom.h"
#include "Common/X87Vector.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureMarks.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureWalkPlanner.h"
#include "Creature/FightMarks.h"
#include "CreatureFightSystem.h"
#include "CreatureFightSystemDetail.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreatureHome.h"
#include "ECS/CreaturePlacement.h"
#include "ECS/CreatureScars.h"
#include "ECS/Map.h"
#include "ECS/PosedModel.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/FightTranscriptNote.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/LiquidParticleSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/ThingCircles.h"
#include "ECS/ThingMapPlace.h"
#include "ECS/ThingModel.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "MagicLiving.h"
#include "Resources/ResourcesInterface.h"
#include "Sound/SoundRequestInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using openblack::creature::CreatureRig;
namespace fight = openblack::creature_fight;
namespace posed_model = openblack::ecs::posed_model;
using namespace openblack::ecs::systems::fight_detail;

namespace
{
/// The strength that the creature's body shows, from -1 (weak) to 1 (strong). The blows compare these strengths.
float ShownStrength(const Creature& creature)
{
	return creature_morph::StrengthLook(creature.strength,
	                                    ecs::archetypes::CreatureArchetype::SpeciesStrength(creature.species));
}

const CreatureRig* RigOf(CreatureType species)
{
	auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto id = creature::GetRigId(species);
	return rigs.Contains(id) ? &*rigs.Handle(id) : nullptr;
}

/// How far one of the species' animations carries the root, in its mesh's units, ahead being -z
glm::vec2 DisplacementOf(CreatureType species, size_t animation)
{
	const auto* rig = RigOf(species);
	const auto* played = rig != nullptr ? rig->GetAnimation(CreatureRig::Mesh::Base, animation) : nullptr;
	return played != nullptr && !played->frames.empty() ? glm::vec2(played->displacement.x, played->displacement.z)
	                                                    : glm::vec2(0.0f);
}

/// The body plays a fight animation at the fighter's time, looping until the fight moves it on. The fight alone moves
/// the time on, at the move's own speed, so that the body shows exactly where the move is: blows land and sound as drawn,
/// whatever the frame rate.
void Show(CreatureAnimation& animation, const fight::Fighter& fighter, bool poweringUp, float durationMs)
{
	const auto played = fighter.state == fight::State::Stance && poweringUp ? fight::animations::k_PowerUp : fighter.animation;
	animation.body = {
	    .kind = creature_layers::BodyAction::Kind::Sequence,
	    .phase = creature_layers::BodyAction::Phase::Loop,
	    .animations = {played, played, played},
	    .timeMs = std::clamp(fighter.timeMs, 0.0f, std::max(durationMs - 1.0f, 0.0f)),
	    .mirrored = fighter.mirrored,
	    .holdLoop = fighter.state == fight::State::Lying,
	    .timedByPlayer = true,
	};
	animation.slots.clear();
}

/// The bones of a creature as the ellipsoids that fight blows strike, as its body is posed at the end of its newest
/// game turn
std::vector<physics::Ellipsoid> BonesOf(entt::entity creature)
{
	return Locator::dynamicsSystem::has_value() ? Locator::dynamicsSystem::value().CreatureSkeleton(creature)
	                                            : std::vector<physics::Ellipsoid> {};
}

/// How high a creature's head is over the land under it, as its body is posed at the end of its newest game turn; none
/// without a posed head
std::optional<float> HeadHeightOf(const ecs::Registry& registry, entt::entity creature)
{
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	const auto* rig = body != nullptr ? RigOf(body->species) : nullptr;
	if (animation == nullptr || transform == nullptr || rig == nullptr || !rig->actionPoints.has_value() ||
	    rig->actionPoints->head >= animation->boneMatrices.size())
	{
		return std::nullopt;
	}
	const auto pose = creature::TurnPose(animation->newerTurnPose, animation->boneMatrices,
	                                     creature::PlacementMatrix(transform->position, transform->rotation, transform->scale));
	const auto head = glm::vec3(creature::PosedBone(rig->actionPoints->head, pose.bones, pose.placement)[3]);
	return head.y - GroundAt(Flat(head));
}

/// Whether the creature of a player learns its lean from the blows and the blocks that the player sends. Each computer
/// gets the same answer.
bool LearnsFromPresses(PlayerNames player)
{
	const bool networkGame = Locator::time::has_value() && Locator::time::value().IsNetworkGame();
	return fight::LearnsFromPresses(networkGame, player == PlayerNames::PLAYER_ONE, IsHumanGod(player));
}

/// Whether a creature in its duel has a body to fight: the opponent fights on against it, or the opponent has its order to
/// start its duel. The blows test the body of an opponent that did not start its duel yet, and can hit it.
bool HasDuelPartner(const ecs::Registry& registry, entt::entity creature, const CreatureFighting& fighting)
{
	return FightsOn(registry, creature, fighting) || OpponentStartsDuel(registry, creature, fighting);
}

/// On each turn of a blow, whether it lands or not, the striker and its opponent move apart when their bodies overlap
void PushApartOnBlow(ecs::Registry& registry, entt::entity striker, entt::entity opponent)
{
	auto* self = registry.TryGet<CreatureLocomotion>(striker);
	auto* other = registry.Valid(opponent) ? registry.TryGet<CreatureLocomotion>(opponent) : nullptr;
	if (self == nullptr || other == nullptr)
	{
		return;
	}
	const auto at = Flat(self->toPosition);
	const auto otherAt = Flat(other->toPosition);
	const auto strikerExtent = ForwardExtentOf(registry, striker);
	const auto opponentExtent = ForwardExtentOf(registry, opponent);
	const auto pushed =
	    fight::BlowPushOf(at, otherAt, creature_locomotion::DirectionOf(self->heading), strikerExtent, opponentExtent);
	if (!pushed.first.has_value() || !pushed.second.has_value())
	{
		return;
	}
	Shift(*self, *pushed.first - at);
	Shift(*other, *pushed.second - otherAt);
	registry.SetDirty();
	fight_transcript::Note(striker, "blow_push", [&] {
		return fight_transcript::Fields {{"creature", fight_transcript::Id(striker)},
		                                 {"apart", glm::distance(at, otherAt)},
		                                 {"striker_extent", strikerExtent},
		                                 {"opponent_extent", opponentExtent}};
	});
}
} // namespace

bool CreatureFightSystem::QueueMove(entt::entity creature, const fight::Move& move, bool replace)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* fighting = registry.Valid(creature) ? registry.TryGet<CreatureFighting>(creature) : nullptr;
	if (fighting == nullptr)
	{
		return false;
	}
	const auto controlBefore = fighting->fighter.control;
	const bool queued = fight::PlayerMove(fighting->fighter, fighting->marks, move, replace);
	const auto index = fight_transcript::Note(creature, "move_queued", [&] {
		return fight_transcript::Fields {{"creature", fight_transcript::Id(creature)},
		                                 {"move", fight::Name(move.kind)},
		                                 {"value", move.value},
		                                 {"replace", replace},
		                                 {"queued", queued},
		                                 {"source", _queueSource},
		                                 {"queue", static_cast<uint32_t>(fighting->fighter.queue.Size())},
		                                 {"blows_queued", fight::QueuedBlows(fighting->fighter.queue.Moves())}};
	});
	if (controlBefore != fighting->fighter.control)
	{
		fight_transcript::Note(
		    creature, "control",
		    [&] {
			    return fight_transcript::Fields {{"creature", fight_transcript::Id(creature)},
			                                     {"from", fight::Name(controlBefore)},
			                                     {"to", fight::Name(fighting->fighter.control)},
			                                     {"why", "move_given"},
			                                     {"computer_wait_ms", fighting->fighter.computerWaitMs}};
		    },
		    index);
	}
	if (fighting->stage == CreatureFighting::Stage::Duel)
	{
		CheckQueue(creature);
	}
	return queued;
}

void CreatureFightSystem::ReleaseCharge(entt::entity creature, float heldMs)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* fighting = registry.Valid(creature) ? registry.TryGet<CreatureFighting>(creature) : nullptr)
	{
		fighting->fighter.queue.Release(heldMs);
		if (fighting->stage == CreatureFighting::Stage::Duel)
		{
			CheckQueue(creature);
		}
	}
}

float CreatureFightSystem::GetFightHealth(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		return 1.0f;
	}
	if (const auto* fighting = registry.TryGet<const CreatureFighting>(creature))
	{
		return fighting->fighter.health;
	}
	const auto* record = registry.TryGet<const CreatureFightRecord>(creature);
	return record != nullptr ? record->health : 1.0f;
}

void CreatureFightSystem::SetFightHealth(entt::entity creature, float health)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		return;
	}
	if (auto* fighting = registry.TryGet<CreatureFighting>(creature))
	{
		const auto before = fighting->fighter.health;
		fighting->fighter.health = health;
		fight_transcript::Note(creature, "level", [&] { return LevelChange(creature, "health", before, health, "script"); });
	}
	auto& record = registry.AllOf<CreatureFightRecord>(creature) ? registry.Get<CreatureFightRecord>(creature)
	                                                             : registry.Assign<CreatureFightRecord>(creature);
	record.health = health;
}

bool CreatureFightSystem::IsBlocking(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* fighting = registry.Valid(creature) ? registry.TryGet<const CreatureFighting>(creature) : nullptr;
	return fighting != nullptr && fight::IsBlocking(fighting->fighter.state, fighting->fighter.timeMs,
	                                                DurationOf(creature, fight::animations::k_StartBlock));
}

void CreatureFightSystem::Recoil(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* fighting = registry.Valid(creature) ? registry.TryGet<CreatureFighting>(creature) : nullptr;
	if (fighting == nullptr)
	{
		return;
	}
	auto& fighter = fighting->fighter;
	// Reeling already from a blow, recoiling in its block or fainting, it takes no more notice
	using namespace fight::animations;
	const bool reeling = fighter.animation >= k_RecoilHigh && fighter.animation < k_AfterRecoils;
	if (reeling || fighter.state == fight::State::BlockRecoil || fighter.state == fight::State::Faint)
	{
		return;
	}
	const auto index = fight_transcript::Note(creature, "recoil", [&] {
		return fight_transcript::Fields {{"creature", fight_transcript::Id(creature)},
		                                 {"why", "miracle"},
		                                 {"blocking", IsBlocking(creature)},
		                                 {"state", fight::Name(fighter.state)}};
	});
	if (index.has_value())
	{
		_actionCause[creature] = *index;
	}
	if (IsBlocking(creature))
	{
		fight::Enter(fighter, fight::State::BlockRecoil);
	}
	else if (fighter.state == fight::State::Stance || fighter.state == fight::State::Action ||
	         fighter.state == fight::State::CastStart || fighter.state == fight::State::Cast ||
	         fighter.state == fight::State::CastEnd)
	{
		fight::Enter(fighter, fight::State::Action, k_RecoilMid);
	}
}

void CreatureFightSystem::BloodTurn(entt::entity creature)
{
	ChargeMarkTurn(creature);
	DripTurn(creature);
}

void CreatureFightSystem::ChargeMarkTurn(entt::entity creature)
{
	// While a press on a body is held, the sparkle added last charges by the milliseconds of the creature's turn. A
	// paused game has no turns, thus the charge stops.
	if (!_markHeld)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* fighting = registry.TryGet<CreatureFighting>(creature))
	{
		fighting->marks.Charge(static_cast<float>(TurnMsOf(registry.TryGet<const CreatureAnimation>(creature))));
	}
}

void CreatureFightSystem::MovesTurn(entt::entity creature)
{
	StepMoves(creature);
	NoteAction(creature);
}

void CreatureFightSystem::StepMoves(entt::entity entity)
{
	// One fighter after the other makes all of its turn: its animation moves on, then its move in the duel and its blow.
	// Then its body takes its pose for the turn. When the other fighter moves it after its turn, the pose does not change.
	auto& registry = Locator::entitiesRegistry::value();
	const auto* fighting = registry.Valid(entity) ? registry.TryGet<const CreatureFighting>(entity) : nullptr;
	if (fighting == nullptr)
	{
		return;
	}
	// Only an action that moves on in the step tests its blow. An action that the duel starts in this turn makes its
	// first test in the next turn.
	const auto step = StepAnimation(entity);
	fighting = registry.TryGet<const CreatureFighting>(entity);
	if (fighting == nullptr)
	{
		return;
	}
	if (fighting->stage == CreatureFighting::Stage::Duel)
	{
		DuelTurn(entity);
	}
	BlowTurn(entity, step);
	// The body takes its pose for the turn at the end of its own turn, before the other fighter makes its turn
	if (Locator::creatureAnimationSystem::has_value())
	{
		Locator::creatureAnimationSystem::value().PoseTurn(entity);
	}
}

void CreatureFightSystem::StaminaTurn(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* fighting = registry.Valid(entity) ? registry.TryGet<CreatureFighting>(entity) : nullptr;
	if (fighting == nullptr || fighting->stage != CreatureFighting::Stage::Duel)
	{
		return;
	}
	// At the end of its turn, a fighter in a duel with its opponent gets back a little stamina. It also does so while its
	// opponent is about to start its duel.
	const auto opponent = fighting->opponent;
	if (!OpponentStartsDuel(registry, entity, *fighting) &&
	    (!FightsOn(registry, entity, *fighting) || registry.Get<const CreatureFighting>(opponent).opponent != entity))
	{
		return;
	}
	auto& fighter = fighting->fighter;
	const auto staminaBefore = fighter.stamina;
	fighter.stamina = fight::RegainStamina(fighter.stamina);
	if (fighter.stamina != staminaBefore)
	{
		fight_transcript::Note(entity, "level",
		                       [&] { return LevelChange(entity, "stamina", staminaBefore, fighter.stamina, "regain"); });
	}
}

void CreatureFightSystem::MeasureBlows(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& fighting = registry.Get<CreatureFighting>(creature);
	if (fighting.measured || !Locator::creatureAnimationSystem::has_value())
	{
		return;
	}
	const auto& body = registry.Get<const Creature>(creature);
	const auto* rig = RigOf(body.species);
	if (rig == nullptr || !rig->actionPoints.has_value())
	{
		return;
	}
	if (!rig->blowStrikes.has_value())
	{
		return;
	}
	auto& animations = Locator::creatureAnimationSystem::value();
	const auto scale = std::abs(registry.Get<const Transform>(creature).scale.x);
	fighting.reaches.clear();
	for (size_t i = 0; i < fight::animations::k_AttackCount; ++i)
	{
		const auto animation = fight::animations::k_FirstAttack + i;
		const auto duration = DurationOf(creature, animation);
		const auto& strike = rig->blowStrikes->at(i);
		const auto landMs = static_cast<float>(strike.landMs);
		if (duration <= 0.0f)
		{
			continue;
		}
		fighting.hitTimesMs.at(i) = landMs;
		if (landMs < fight::k_LeastLandMs || strike.bone < 0)
		{
			// A blow that the creature has but does not measure stays at the origin of the mesh, and it can still be
			// chosen
			fighting.reaches.push_back({.animation = animation, .point = glm::vec3(0.0f), .height = 0.0f});
			continue;
		}
		// The striking bone as the blow lands, and the land time before
		const auto bone = static_cast<uint32_t>(strike.bone);
		const auto atLand = animations.BoneInAnimation(creature, animation, landMs, bone, false);
		const auto before = animations.BoneInAnimation(creature, animation, landMs - fight::k_LeastLandMs, bone, false);
		if (!atLand.has_value() || !before.has_value())
		{
			continue;
		}
		const auto shape = fight::ShapeOfBlow(*atLand, *before, landMs, duration, DisplacementOf(body.species, animation).y);
		fighting.reaches.push_back({
		    .animation = animation,
		    .point = shape.point,
		    .height = shape.point.y * scale,
		    .swing = shape.swing,
		});
	}
	fighting.measured = true;
}

void CreatureFightSystem::CheckQueue(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& fighting = registry.Get<CreatureFighting>(creature);
	if (fighting.stage != CreatureFighting::Stage::Duel || !registry.Valid(fighting.opponent))
	{
		return;
	}
	const auto opponentAt = Flat(registry.Get<const Transform>(fighting.opponent).position);
	auto& fighter = fighting.fighter;
	const auto front = fighter.queue.Front();
	const bool inRange = fight::WithinRange(fighting.arena, opponentAt);
	// The help record of this computer counts the moves of the creature of its own player
	if (fight::CountsHelpEvent(fighter.state, inRange, front.has_value()) &&
	    registry.Get<const Creature>(creature).owner == OwnPlayer() && Locator::helpProfileSystem::has_value())
	{
		Locator::helpProfileSystem::value().Trigger(fight::HelpEventOf(front->move.kind));
	}
	const auto order = fight::TakeOrder(fighter, fighting.marks, inRange);
	if (!order.has_value())
	{
		return;
	}
	const auto orderIndex = fight_transcript::Note(creature, "order", [&] {
		return fight_transcript::Fields {{"creature", fight_transcript::Id(creature)},
		                                 {"order", fight::Name(order->kind)},
		                                 {"band", fight::Name(order->band)},
		                                 {"value", order->value},
		                                 {"speed", order->speed},
		                                 {"in_range", inRange},
		                                 {"queue", static_cast<uint32_t>(fighter.queue.Size())}};
	});
	if (orderIndex.has_value())
	{
		_actionCause[creature] = *orderIndex;
	}
	// The creature learns from the moves it makes, whoever asked for them
	auto* record = registry.TryGet<CreatureFightRecord>(creature);
	if (order->kind != fight::Order::Kind::EndBlock && front.has_value() && record != nullptr &&
	    fight::LearnsFromMove(front->move.kind, LearnsFromPresses(registry.Get<const Creature>(creature).owner)))
	{
		const auto before = record->tendency;
		record->tendency = fight::LearnTendency(record->tendency, front->move.kind);
		fight_transcript::Note(
		    creature, "learning",
		    [&] {
			    return fight_transcript::Fields {{"creature", fight_transcript::Id(creature)},
			                                     {"what", "tendency"},
			                                     {"move", fight::Name(front->move.kind)},
			                                     {"before", before},
			                                     {"after", record->tendency}};
		    },
		    orderIndex);
	}
	switch (order->kind)
	{
	case fight::Order::Kind::EndBlock:
		fight::Enter(fighter, fight::State::BlockEnd);
		break;
	case fight::Order::Kind::Animation:
		if (const auto step = fight::StepOf(order->value))
		{
			if (fight::CanStep(fighting.arena, Flat(registry.Get<const Transform>(creature).position), *step))
			{
				fight::Enter(fighter, fight::State::Action, order->value);
			}
		}
		else if (Has(creature, order->value))
		{
			fight::Enter(fighter, fight::State::Action, order->value);
		}
		break;
	case fight::Order::Kind::Cast:
		// The spell is cast as the cast's start ends
		fight::Enter(fighter, fight::State::CastStart);
		fighter.spell = order->value;
		break;
	case fight::Order::Kind::Block:
		fight::Enter(fighter, fight::State::BlockStart);
		break;
	case fight::Order::Kind::Special:
		fight::Enter(fighter, fight::State::Action, fight::animations::k_AttackSpecial, order->speed);
		break;
	case fight::Order::Kind::Blow:
		AttemptBlow(creature, order->band, order->speed);
		break;
	}
}

void CreatureFightSystem::CastFightSpell(entt::entity creature, MagicType type, entt::entity opponent)
{
	if (!Locator::creatureMindSystem::has_value() || static_cast<size_t>(type) >= magic::k_MagicTypeCount)
	{
		return;
	}
	const auto& effect = magic::GetMagicEffectInfo(Locator::infoConstants::value(), type);
	const auto note = [&](std::string_view use, entt::entity target, std::optional<bool> cast) {
		fight_transcript::Note(
		    creature, "spell",
		    [&] {
			    return fight_transcript::Fields {{"creature", fight_transcript::Id(creature)},
			                                     {"magic_type", static_cast<int32_t>(type)},
			                                     {"use", use},
			                                     {"target", fight_transcript::Id(target)},
			                                     {"result", !cast.has_value() ? "not_cast" : (*cast ? "cast" : "failed")}};
		    },
		    fight_transcript::LastOf(creature, "action"));
	};
	// Without an opponent the creature casts no miracle, not even one that defends
	if (opponent == entt::null)
	{
		note("no_opponent", entt::null, std::nullopt);
		return;
	}
	if (effect.isAggressiveSpellWhichIsUsedInCreatureFightArena != 0)
	{
		note("attack", opponent, Locator::creatureMindSystem::value().TryMiracle(creature, type, opponent));
	}
	else if (effect.isDefensiveSpellWhichIsUsedInCreatureFightArena != 0)
	{
		note("defence", creature, Locator::creatureMindSystem::value().TryMiracle(creature, type, creature));
	}
	else
	{
		// A miracle that the fight cannot use does nothing
		note("not_for_fights", entt::null, std::nullopt);
	}
}

void CreatureFightSystem::AttemptBlow(entt::entity creature, fight::Band band, float speed)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& fighting = registry.Get<CreatureFighting>(creature);
	MeasureBlows(creature);
	const auto& body = registry.Get<const Creature>(creature);
	const auto opponent = fighting.opponent;
	const auto selfAt = Flat(registry.Get<const Transform>(creature).position);
	const auto& transform = registry.Get<const Transform>(creature);
	const auto& opponentTransform = registry.Get<const Transform>(opponent);
	const auto scale = std::abs(transform.scale.x);
	const auto headHeight = HeadHeightOf(registry, opponent);
	if (!headHeight.has_value())
	{
		return;
	}
	// Lines from where each blow lands, towards the opponent, find how far its body is ahead of the blow
	const auto bones = BonesOf(opponent);
	const auto towards = opponentTransform.position - transform.position;
	const auto unitTowards = glm::length(towards) > 0.0f ? glm::normalize(towards) : towards;
	// The points of the blows lean with the slope of the land, as the body does
	const auto placement = ecs::creature_placement::PlacementOf(registry, creature, transform);
	const auto opponentSize = ShownSize(registry.Get<const Creature>(opponent));
	std::vector<fight::BlowDepth> blows;
	blows.reserve(fighting.reaches.size());
	for (const auto& reach : fighting.reaches)
	{
		fight::BlowDepth blow {.animation = reach.animation, .height = reach.height, .depth = std::nullopt};
		if (reach.animation != fight::animations::k_AttackSpecial && reach.height <= *headHeight)
		{
			const auto point = glm::vec3(placement * glm::vec4(0.0f, reach.point.y, reach.point.z, 1.0f));
			blow.depth = fight::DepthToBody(bones, point, unitTowards, opponentSize);
		}
		blows.push_back(blow);
	}
	const auto strideOf = [&body, scale](size_t animation) {
		const auto* rig = RigOf(body.species);
		const auto* played = rig != nullptr ? rig->GetAnimation(CreatureRig::Mesh::Base, animation) : nullptr;
		return played != nullptr ? played->strideLength * scale : 0.0f;
	};
	const auto steps = fight::StepLengths {.forward = strideOf(fight::animations::k_StepForward),
	                                       .back = strideOf(fight::animations::k_StepBack)};
	const auto choice = fight::ChooseAttack(blows, band, *headHeight, steps, ShownSize(body));
	fight_transcript::Note(
	    creature, "attack_choice",
	    [&] {
		    return fight_transcript::Fields {
		        {"creature", fight_transcript::Id(creature)},
		        {"band", fight::Name(band)},
		        {"head_height", *headHeight},
		        {"nearest_depth",
		         [&blows] {
			         float nearest = fight::k_NoDepth;
			         for (const auto& blow : blows)
			         {
				         nearest = std::min(nearest, blow.depth.value_or(fight::k_NoDepth));
			         }
			         return nearest;
		         }()},
		        {"step_forward", steps.forward},
		        {"step_back", steps.back},
		        {"animation", choice.animation.has_value() ? static_cast<int64_t>(*choice.animation) : int64_t {-1}},
		        {"step", choice.step.has_value() ? fight::Name(*choice.step) : std::string_view("none")},
		        {"speed", speed}};
	    },
	    _actionCause.contains(creature) ? std::optional(_actionCause.at(creature)) : std::nullopt);
	if (choice.animation.has_value())
	{
		fight::Enter(fighting.fighter, fight::State::Action, *choice.animation, speed);
	}
	else if (choice.step.has_value() && fight::CanStep(fighting.arena, selfAt, *choice.step))
	{
		fight::Enter(fighting.fighter, fight::State::Action, fight::StepAnimation(*choice.step));
	}
}

std::optional<CreatureFightSystem::ActionStep> CreatureFightSystem::StepAnimation(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* fighting = registry.TryGet<CreatureFighting>(entity);
	auto* locomotion = registry.TryGet<CreatureLocomotion>(entity);
	auto* animation = registry.TryGet<CreatureAnimation>(entity);
	const auto stepMs = TurnMsOf(animation);
	if (fighting == nullptr || locomotion == nullptr || animation == nullptr || fighting->fighter.state == fight::State::Idle)
	{
		return std::nullopt;
	}
	const auto& body = registry.Get<const Creature>(entity);
	const bool hasOpponent = fighting->stage == CreatureFighting::Stage::Duel && HasDuelPartner(registry, entity, *fighting);
	// While it turns on the spot in its start, the turn's own animation plays, and the start animation waits. When the
	// turn ends, the start animation plays from its beginning in the same turn. This also occurs when the fight ended
	// during the turn: then the start plays to its end, and the finish follows.
	if (fighting->fighter.state == fight::State::Start && fighting->fighter.turning)
	{
		if (Locator::creatureLocomotionSystem::has_value() && Locator::creatureLocomotionSystem::value().IsMoving(entity))
		{
			return std::nullopt;
		}
		fighting->fighter.turning = false;
		fighting->fighter.timeMs = 0.0f;
		fighting->fighter.stateMs = 0.0f;
	}
	auto duration = DurationOf(entity, fighting->fighter.animation);
	auto speed = fight::PlaybackSpeed(fighting->fighter.animation);

	// An action or a reel in a block whose animation reaches its end in this turn ends before it moves on: it makes no
	// root move. The reel goes back to the block. The action goes back to its stance, also when the fight is called off
	// (it finishes from its stance in the next turn). The next move that the player put in the queue starts at once and
	// moves on in this turn. The computer chooses its next move in its stance.
	const auto endingState = fighting->fighter.state;
	if ((endingState == fight::State::Action || endingState == fight::State::BlockRecoil) &&
	    fight::ActionEnds(fighting->fighter.timeMs, fight::TurnAdvance(speed, stepMs), duration))
	{
		fight::Enter(fighting->fighter, fight::AfterAnimation(endingState, hasOpponent));
		if (endingState == fight::State::Action && fighting->fighter.control != fight::Control::Computer &&
		    !fighting->fighter.queue.Empty())
		{
			CheckQueue(entity);
		}
		fighting = registry.TryGet<CreatureFighting>(entity);
		if (fighting == nullptr)
		{
			return std::nullopt;
		}
		if (fighting->fighter.state != fight::State::Action)
		{
			Show(*animation, fighting->fighter, fighting->fighter.queue.HasWaiting(),
			     DurationOf(entity, fighting->fighter.animation));
			return std::nullopt;
		}
		duration = DurationOf(entity, fighting->fighter.animation);
		speed = fight::PlaybackSpeed(fighting->fighter.animation);
	}

	// The start, the finish, and the start and end of a block or a cast test their end before their animation moves on.
	// A fighter that was called off moves on from its stance, its block or its cast at once. The next state then makes
	// its turn at once: its animation moves on in the same turn.
	// A hurried fighter doubles the span
	const auto spanMs = creature_layers::HurriedStep(
	    fight::TestSpanMs(static_cast<float>(stepMs), ShownSize(body), animation->realTime), fighting->fighter.hurried);
	bool entered = false;
	{
		auto& current = fighting->fighter;
		const auto ending = current.state;
		const bool ends = fight::EndsByOwnTest(ending) &&
		                  fight::OwnTestEnds(ending, current.timeMs, fight::EndTestMs(ending, speed, stepMs, spanMs), duration,
		                                     current.stateMs, stepMs);
		auto next = current.stopping ? fight::StopTransition(ending, ends) : std::nullopt;
		if (!next.has_value() && ends)
		{
			next = fight::AfterAnimation(ending, hasOpponent);
		}
		if (next.has_value())
		{
			// The casting pose over, the creature lets go of the miracle: one that lasts only while held stops
			if (ending == fight::State::Cast && Locator::magicSystem::has_value())
			{
				Locator::magicSystem::value().ReleaseCreatureCast(entity);
				const auto* why = current.stopping ? "called_off" : "cast_over";
				fight_transcript::Note(entity, "spell_release", [entity, why] {
					return fight_transcript::Fields {{"creature", fight_transcript::Id(entity)}, {"why", why}};
				});
			}
			// As the start of a cast ends, still facing its opponent, the spell is cast: an attacking one at the
			// opponent, a defending one on itself. A fighter that was called off casts nothing.
			if (current.stopping)
			{
				current.spell.reset();
			}
			else if (ending == fight::State::CastStart && current.spell.has_value() && hasOpponent)
			{
				CastFightSpell(entity, static_cast<MagicType>(*current.spell), fighting->opponent);
				fighting = registry.TryGet<CreatureFighting>(entity);
				if (fighting == nullptr)
				{
					return std::nullopt;
				}
				fighting->fighter.spell.reset();
			}
			fight::Enter(fighting->fighter, *next);
			if (*next == fight::State::Idle)
			{
				animation->body = {};
				return std::nullopt;
			}
			// At the end of a block or a cast, the next move that the player put in the queue starts at once
			if ((ending == fight::State::BlockEnd || ending == fight::State::CastEnd) && *next == fight::State::Stance &&
			    fighting->fighter.control != fight::Control::Computer && !fighting->fighter.queue.Empty())
			{
				CheckQueue(entity);
				fighting = registry.TryGet<CreatureFighting>(entity);
				if (fighting == nullptr)
				{
					return std::nullopt;
				}
			}
			entered = true;
			duration = DurationOf(entity, fighting->fighter.animation);
			speed = fight::PlaybackSpeed(fighting->fighter.animation);
		}
	}
	auto& fighter = fighting->fighter;
	const auto fromMs = static_cast<int32_t>(fighter.timeMs);

	// An action carries the creature as far as its animation's root moves in the turn, before the animation moves on.
	// Only the reel in a block stops at the range.
	if ((fighter.state == fight::State::Action || fighter.state == fight::State::BlockRecoil) && duration > 0.0f)
	{
		const auto scale = std::abs(registry.Get<const Transform>(entity).scale.x);
		auto by = fight::TurnRootMove(DisplacementOf(body.species, fighter.animation), speed, stepMs, scale,
		                              static_cast<int32_t>(duration), locomotion->heading.Radians());
		const auto at = Flat(locomotion->toPosition);
		const auto to =
		    fighter.state == fight::State::BlockRecoil ? fight::BlockRecoilTo(fighting->arena, at, at + by) : at + by;
		by = to - at;
		if (glm::length(by) > 0.0f)
		{
			// The body moves to its new place over the turn
			locomotion->toPosition.x += by.x;
			locomotion->toPosition.z += by.y;
			locomotion->toPosition.y = GroundAt(Flat(locomotion->toPosition));
		}
	}
	if (fight::EndsByOwnTest(fighter.state))
	{
		// Its animation goes round once at most and never stops at its end. The state ends only by its own test.
		const auto durationMs = static_cast<int32_t>(duration);
		fighter.timeMs = durationMs > 0 ? static_cast<float>(creature_walk_planner::AdvanceLoop(
		                                      static_cast<int32_t>(fighter.timeMs),
		                                      fight::StateAdvanceMs(fighter.state, speed, stepMs, spanMs), durationMs))
		                                : 0.0f;
		if (!entered)
		{
			fighter.stateMs += static_cast<float>(stepMs);
		}
	}
	else
	{
		fighter.timeMs += static_cast<float>(fight::TurnAdvance(speed, stepMs));
		fighter.stateMs += static_cast<float>(stepMs);
	}

	auto& current = fighting->fighter;
	// Another state moves on once its animation has played through
	if (!entered && !fight::EndsByOwnTest(current.state) &&
	    fight::StateDone(current.state, current.timeMs, duration, current.stateMs))
	{
		const auto next = fight::AfterAnimation(current.state, hasOpponent);
		if (next == current.state && fight::Loops(current.state))
		{
			current.timeMs =
			    current.state == fight::State::Lying || duration <= 0.0f ? duration : std::fmod(current.timeMs, duration);
		}
		else
		{
			fight::Enter(current, next);
			if (next == fight::State::Idle)
			{
				animation->body = {};
				return std::nullopt;
			}
		}
	}
	Show(*animation, current, current.queue.HasWaiting(), DurationOf(entity, current.animation));
	if (current.state != fight::State::Action)
	{
		return std::nullopt;
	}
	return ActionStep {.animation = current.animation, .fromMs = fromMs};
}

void CreatureFightSystem::BlowTurn(entt::entity entity, std::optional<ActionStep> step)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& fighting = registry.Get<const CreatureFighting>(entity);
	const auto& fighter = fighting.fighter;
	const bool striking = step.has_value() && fighter.animation == step->animation &&
	                      fighting.stage == CreatureFighting::Stage::Duel && fighter.state == fight::State::Action &&
	                      fight::IsAttack(fighter.animation) && HasDuelPartner(registry, entity, fighting);
	if (striking)
	{
		// The tests start where the animation was before it moved on in this turn
		TestBlow(entity, step->fromMs);
		if (registry.Valid(entity) && registry.AllOf<CreatureFighting>(entity))
		{
			PushApartOnBlow(registry, entity, registry.Get<const CreatureFighting>(entity).opponent);
		}
	}
}

void CreatureFightSystem::TestBlow(entt::entity creature, int32_t fromMs)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!Locator::creatureAnimationSystem::has_value())
	{
		return;
	}
	const auto& fighting = registry.Get<const CreatureFighting>(creature);
	const auto& attacker = fighting.fighter;
	const auto opponent = fighting.opponent;
	// An earlier blow this turn may have made it reel already
	if (attacker.state != fight::State::Action || !fight::IsAttack(attacker.animation) ||
	    !HasDuelPartner(registry, creature, fighting))
	{
		return;
	}
	const auto& body = registry.Get<const Creature>(creature);
	const auto* rig = RigOf(body.species);
	const auto blow = attacker.animation - fight::animations::k_FirstAttack;
	if (rig == nullptr || !rig->blowStrikes.has_value() || !rig->actionPoints.has_value() ||
	    rig->blowStrikes->at(blow).bone < 0)
	{
		return;
	}
	const auto bone = static_cast<uint32_t>(rig->blowStrikes->at(blow).bone);
	const auto groin = rig->actionPoints->groin;
	const auto size = ShownSize(body);
	const bool special = attacker.animation == fight::animations::k_AttackSpecial;
	const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	const auto spanMs = creature_layers::HurriedStep(
	    fight::TestSpanMs(static_cast<float>(TurnMsOf(animation)), size, animation != nullptr && animation->realTime),
	    attacker.hurried);
	const auto landMs = static_cast<int32_t>(fighting.hitTimesMs.at(blow));
	const auto times = fight::BlowTestTimes(fromMs, spanMs, landMs);
	if (times.empty())
	{
		return;
	}
	auto& animations = Locator::creatureAnimationSystem::value();
	const auto& transform = registry.Get<const Transform>(creature);
	// The bone that strikes leans with the slope of the land, as the body does
	const auto placement = ecs::creature_placement::PlacementOf(registry, creature, transform);
	const auto toWorld = [&placement](const std::optional<glm::vec3>& point) -> std::optional<glm::vec3> {
		return point.has_value() ? std::optional(glm::vec3(placement * glm::vec4(*point, 1.0f))) : std::nullopt;
	};
	const auto worldBone = [&](uint32_t which, int32_t timeMs) {
		return toWorld(
		    animations.BoneInAnimation(creature, attacker.animation, static_cast<float>(timeMs), which, attacker.mirrored));
	};
	auto before = worldBone(bone, fromMs);
	if (!before.has_value())
	{
		return;
	}
	// A victim that reels from a blow already, recoils in its block or faints takes no blow
	const auto& victim = registry.Get<const CreatureFighting>(opponent).fighter;
	const bool reeling = victim.state == fight::State::Action && victim.animation >= fight::animations::k_RecoilHigh &&
	                     victim.animation < fight::animations::k_AfterRecoils;
	const bool open = !reeling && victim.state != fight::State::BlockRecoil && victim.state != fight::State::Faint;
	const auto bones = BonesOf(opponent);
	const auto reach = fight::BlowTestReach(size, special);
	std::optional<std::pair<glm::vec3, glm::vec3>> landed;
	for (const auto time : times)
	{
		// The bone that strikes and the groin come from one sample of the animation at the time
		const auto [strikes, groinAt] = animations.TwoBonesInAnimation(creature, attacker.animation, static_cast<float>(time),
		                                                               bone, groin, attacker.mirrored);
		const auto at = toWorld(strikes);
		if (!at.has_value())
		{
			continue;
		}
		if (Locator::liquidParticleSystem::has_value())
		{
			Locator::liquidParticleSystem::value().Add(*at, glm::vec3(0.0f), fight::SwingPuffSize(size, special),
			                                           fight::k_SwingPuffColour, liquid_particles::Kind::Dust);
		}
		const auto from = toWorld(groinAt);
		if (open && !landed.has_value() && from.has_value() && !bones.empty())
		{
			// All five lines are tested, and the last line that meets the body within its length gives the point
			for (const auto& target : fight::BlowTestPoints(*at, *before, reach))
			{
				const auto direction = target - *from;
				const auto hit = physics::EllipsoidHit(bones, *from, direction);
				if (hit.has_value() && fight::WithinBlowLine(hit->first.point, *from, direction))
				{
					landed = std::pair {hit->first.point, (*at - *before) * fight::k_BlowVectorScale};
				}
			}
		}
		before = at;
	}
	if (landed.has_value())
	{
		LandBlow(creature, landed->first, landed->second);
	}
}

void CreatureFightSystem::LandBlow(entt::entity creature, const glm::vec3& point, const glm::vec3& vector)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& fighting = registry.Get<CreatureFighting>(creature);
	const auto opponent = fighting.opponent;
	if (!HasDuelPartner(registry, creature, fighting))
	{
		return;
	}
	const auto& body = registry.Get<const Creature>(creature);
	const auto& opponentBody = registry.Get<const Creature>(opponent);
	auto& opponentFighting = registry.Get<CreatureFighting>(opponent);
	auto& victim = opponentFighting.fighter;
	auto& attacker = fighting.fighter;
	const auto found = std::ranges::find_if(
	    fighting.reaches, [&attacker](const fight::Reach& reach) { return reach.animation == attacker.animation; });
	if (found == fighting.reaches.end())
	{
		return;
	}
	const bool special = attacker.animation == fight::animations::k_AttackSpecial;
	// The blow follows from the action of the attacker that strikes it
	const auto blowCause = fight_transcript::LastOf(creature, "action");
	// The victim remembers when it was struck, which the villagers watching cheer or despair at
	auto& struck = registry.AllOf<CreatureFightRecord>(opponent) ? registry.Get<CreatureFightRecord>(opponent)
	                                                             : registry.Assign<CreatureFightRecord>(opponent);
	struck.struckTurn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0u;
	// A blow that hits is the moment of the attacker's action, which a picture for its web page waits for
	if (auto* moment = registry.TryGet<CreatureAnimation>(creature))
	{
		moment->actionMoment = true;
	}
	// A blow that lands, blocked or not, is a small hit from the attacker: it changes how nice the victim finds the
	// attacker, no more often than once a minute. It takes nothing from the victim's life.
	{
		magic::EffectValues blowEffect {};
		blowEffect[magic::EffectKind::Hit] = fight::k_BlowHitEffect;
		magic_living::AfterEffect(opponent, blowEffect, {.player = body.owner, .casterCreature = creature});
	}

	// Every blow that lands wounds the skin, blocked or not: the column is drawn first, then the kind by the wound type
	// that the attacker's species gives the blow
	{
		const auto random = [this](uint32_t range) { return _random.GameRand(range); };
		const auto column = static_cast<uint8_t>(random(creature_marks::k_CellsPerRow));
		const auto* attackerRig = RigOf(body.species);
		const auto woundType = attackerRig != nullptr && attackerRig->blowWoundTypes.has_value()
		                           ? fight::BlowWoundType(attacker.animation, *attackerRig->blowWoundTypes)
		                           : -1;
		const auto kind = fight::WoundKind(woundType, random);
		WoundSkin(opponent, point, vector, kind, column);
	}
	const bool blocked = fight::IsBlocking(victim.state, victim.timeMs, DurationOf(opponent, fight::animations::k_StartBlock));
	// Blood flies unless the victim blocks, then dust whether it blocks or not
	if (Locator::liquidParticleSystem::has_value())
	{
		auto& liquids = Locator::liquidParticleSystem::value();
		const auto draw = [this](int32_t range) { return static_cast<int32_t>(_random.LocalRand(range)); };
		// Each axis of the spread is drawn z first, then y, then x
		const auto spray = [&draw](const glm::vec3& along, float step) {
			const auto z = draw(51);
			const auto y = draw(51);
			const auto x = draw(51);
			return fight::SprayVelocity(along, step, {x, y, z});
		};
		const auto step = fight::BloodSpreadStep(vector);
		if (!blocked)
		{
			for (int32_t i = 0; i < fight::k_BloodDrops; ++i)
			{
				const auto velocity = spray(vector, step);
				const auto colour = fight::k_BloodColours.at(static_cast<size_t>(draw(3)));
				liquids.Add(point, velocity, ShownSize(opponentBody) * fight::k_BloodSizePerSize, colour,
				            liquid_particles::Kind::Liquid);
			}
		}
		for (int32_t i = 0; i < fight::k_BlowDustPuffs; ++i)
		{
			const auto velocity = spray(vector * fight::k_DustVectorShare, step * fight::k_DustSpreadShare);
			liquids.Add(point, velocity, ShownSize(body) * fight::k_BlowDustSizePerSize, fight::k_BlowDustColour,
			            liquid_particles::Kind::Dust);
		}
	}

	// How high it lands: how high the blow reaches over how high the victim's head is over the land under it
	const auto heightShare =
	    fight::HeightShareOf(found->height, HeadHeightOf(registry, opponent).value_or(HeightOf(ShownSize(opponentBody))));
	const auto direction = fight::RecoilDirectionOf(found->swing);
	// The blows compare the strengths that the two bodies show
	const auto bodyLook = ShownStrength(body);
	const auto opponentLook = ShownStrength(opponentBody);
	const auto result = fight::ResolveBlow(
	    {
	        .attackerSize = ShownSize(body),
	        .defenderSize = ShownSize(opponentBody),
	        .attackerStrength = bodyLook,
	        .defenderStrength = opponentLook,
	        .speed = attacker.speed,
	        .special = special,
	        .heightShare = heightShare,
	        .blocked = blocked,
	    },
	    direction);
	const auto healthBefore = victim.health;
	victim.health = fight::HealthAfterBlow(victim.health, result.damage);
	const auto blowIndex = fight_transcript::Note(
	    creature, "blow",
	    [&] {
		    return fight_transcript::Fields {{"attacker", fight_transcript::Id(creature)},
		                                     {"defender", fight_transcript::Id(opponent)},
		                                     {"animation", static_cast<uint64_t>(attacker.animation)},
		                                     {"outcome", blocked ? "blocked" : "hit"},
		                                     {"part", fight::Name(result.band)},
		                                     {"height_share", heightShare},
		                                     {"point_x", point.x},
		                                     {"point_y", point.y},
		                                     {"point_z", point.z},
		                                     {"vector_x", vector.x},
		                                     {"vector_y", vector.y},
		                                     {"vector_z", vector.z},
		                                     {"attacker_size", ShownSize(body)},
		                                     {"defender_size", ShownSize(opponentBody)},
		                                     {"attacker_strength", bodyLook},
		                                     {"defender_strength", opponentLook},
		                                     {"speed", attacker.speed},
		                                     {"special", special},
		                                     {"blocked", blocked},
		                                     {"damage_factor", fight::DamageFactor({.attackerSize = ShownSize(body),
		                                                                            .defenderSize = ShownSize(opponentBody),
		                                                                            .attackerStrength = bodyLook,
		                                                                            .defenderStrength = opponentLook,
		                                                                            .speed = attacker.speed,
		                                                                            .special = special,
		                                                                            .heightShare = heightShare,
		                                                                            .blocked = blocked})},
		                                     {"damage", result.damage},
		                                     {"direction", fight::Name(direction)},
		                                     {"recoil", static_cast<uint64_t>(result.recoil)},
		                                     {"plain_recoil", static_cast<uint64_t>(result.plainRecoil)},
		                                     {"health_before", healthBefore},
		                                     {"health_after", victim.health}};
	    },
	    blowCause);
	if (blowIndex.has_value())
	{
		fight_transcript::Note(
		    opponent, "level",
		    [&] { return LevelChange(opponent, "health", healthBefore, victim.health, blocked ? "blow_blocked" : "blow"); },
		    blowIndex);
		// The defender reels or recoils in its block because of the blow
		_actionCause[opponent] = *blowIndex;
	}
	// Getting hit takes away a charged blow still waiting, and the sparkle of the move added last
	const auto queuedBefore = victim.queue.Size();
	victim.queue.CancelWaiting();
	if (victim.queue.Size() != queuedBefore)
	{
		fight_transcript::Note(
		    opponent, "queue_cancelled",
		    [&] {
			    return fight_transcript::Fields {{"creature", fight_transcript::Id(opponent)},
			                                     {"why", "struck_while_charging"},
			                                     {"before", static_cast<uint32_t>(queuedBefore)},
			                                     {"after", static_cast<uint32_t>(victim.queue.Size())}};
		    },
		    blowIndex);
	}
	if (victim.queue.Size() != queuedBefore && opponentFighting.marks.Size() > 0)
	{
		opponentFighting.marks.DropNewest();
		_markHeld = false;
	}

	// Too close, the attacker is pushed back by half of how far they overlap
	const auto* opponentMoves = registry.TryGet<const CreatureLocomotion>(opponent);
	if (auto* moves = registry.TryGet<CreatureLocomotion>(creature); moves != nullptr && opponentMoves != nullptr)
	{
		const auto between = Flat(moves->toPosition) - Flat(opponentMoves->toPosition);
		const auto distance = glm::length(between);
		const auto closest = (moves->radius * 0.5f) + opponentMoves->radius;
		if (distance < closest && distance > 0.0f)
		{
			const auto by = between * (((closest - distance) * 0.5f) / distance);
			Shift(*moves, by);
			fight_transcript::Note(
			    creature, "pushed_back",
			    [&] {
				    return fight_transcript::Fields {{"creature", fight_transcript::Id(creature)},
				                                     {"by_x", by.x},
				                                     {"by_z", by.y},
				                                     {"distance", distance},
				                                     {"closest", closest}};
			    },
			    blowIndex);
		}
	}

	if (victim.health <= 0.0f)
	{
		Win(creature, opponent, true);
		return;
	}
	if (blocked)
	{
		fight::Enter(victim, fight::State::BlockRecoil);
	}
	else if (victim.state == fight::State::Stance || victim.state == fight::State::Action ||
	         victim.state == fight::State::CastStart || victim.state == fight::State::Cast ||
	         victim.state == fight::State::CastEnd)
	{
		fight::Enter(victim, fight::State::Action, Has(opponent, result.recoil) ? result.recoil : result.plainRecoil);
	}
}

void CreatureFightSystem::WoundSkin(entt::entity victim, const glm::vec3& point, const glm::vec3& vector, uint8_t kind,
                                    uint8_t column)
{
	auto& registry = Locator::entitiesRegistry::value();
	// A blow with no movement points nowhere, and meets no skin
	if (vector == glm::vec3(0.0f) || !Locator::creatureSkinSystem::has_value())
	{
		return;
	}
	const auto drawn = DrawnMeshOf(registry, victim);
	if (!drawn.has_value())
	{
		return;
	}
	// The whole line along the blow counts: the wound goes where it first meets the skin, as the body is posed at the end
	// of its newest game turn
	const auto turnBones = creature_scars::TurnBones(registry, victim, *drawn->first, drawn->second);
	const auto hit = posed_model::NearestSkinHit(registry, victim, *drawn->first, drawn->second, turnBones, point,
	                                             x87::Scaled<x87::SumOrder::Zyx>(vector), true);
	if (!hit.has_value())
	{
		return;
	}
	const auto texel = creature_marks::scar::TexelAt(hit->uvs, hit->hit.s, hit->hit.t);
	Locator::creatureSkinSystem::value().AddWound(
	    victim,
	    {.u = texel.x, .v = texel.y, .skin = static_cast<uint8_t>(hit->skin & 3u), .age = 0, .type = kind, .column = column});
	if (!fight::Bleeds(kind))
	{
		return;
	}
	// A wound that bleeds starts a drip in the first free place; with none free it starts none
	auto& drips =
	    registry.AllOf<CreatureDrips>(victim) ? registry.Get<CreatureDrips>(victim) : registry.Assign<CreatureDrips>(victim);
	const auto free =
	    std::ranges::find_if(drips.drips, [](const CreatureDrips::Drip& drip) { return drip.ms >= CreatureDrips::k_RunMs; });
	if (free != drips.drips.end())
	{
		*free = {.subMesh = hit->subMesh, .firstIndex = hit->hit.firstIndex, .s = hit->hit.s, .t = hit->hit.t, .ms = 0};
	}
}

void CreatureFightSystem::ProcessDrips()
{
	std::vector<entt::entity> bleeding;
	Locator::entitiesRegistry::value().Each<CreatureDrips>(
	    [&bleeding](entt::entity creature, CreatureDrips& /*drips*/) { bleeding.push_back(creature); });
	for (const auto creature : bleeding)
	{
		DripTurn(creature);
	}
}

void CreatureFightSystem::DripTurn(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!Locator::creatureSkinSystem::has_value())
	{
		return;
	}
	auto& skins = Locator::creatureSkinSystem::value();
	auto* found = registry.Valid(creature) ? registry.TryGet<CreatureDrips>(creature) : nullptr;
	if (found == nullptr)
	{
		return;
	}
	auto& drips = *found;
	const auto drawn = DrawnMeshOf(registry, creature);
	// The blood runs over the body as it is posed at the end of its newest game turn
	const auto turnBones = drawn.has_value() ? creature_scars::TurnBones(registry, creature, *drawn->first, drawn->second)
	                                         : std::vector<glm::mat4> {};
	for (auto& drip : drips.drips)
	{
		if (drip.ms >= CreatureDrips::k_RunMs)
		{
			continue;
		}
		drip.ms += TurnMsOf(registry.TryGet<const CreatureAnimation>(creature));
		const auto corners =
		    drawn.has_value()
		        ? posed_model::CornersOnSkin(registry, creature, *drawn->first, drawn->second, turnBones,
		                                     {.subMesh = drip.subMesh, .firstIndex = drip.firstIndex, .s = drip.s, .t = drip.t})
		        : std::nullopt;
		if (!corners.has_value())
		{
			drip.ms = CreatureDrips::k_RunMs;
			continue;
		}
		// The drip moves a little down, then back onto the skin across its triangle; it stops where it finds none
		auto from = posed_model::SkinPointPosition(*corners, drip.s, drip.t);
		from.y -= fight::k_DripFallPerTurn;
		// Across the triangle, as long as twice its area: a triangle too small finds no skin
		const auto across = glm::cross((*corners)[2] - (*corners)[0], (*corners)[1] - (*corners)[0]);
		const auto hit =
		    posed_model::NearestSkinHit(registry, creature, *drawn->first, drawn->second, turnBones, from, across, true);
		if (!hit.has_value())
		{
			drip.ms = CreatureDrips::k_RunMs;
			continue;
		}
		drip = {.subMesh = hit->subMesh, .firstIndex = hit->hit.firstIndex, .s = hit->hit.s, .t = hit->hit.t, .ms = drip.ms};
		const auto texel = creature_marks::scar::TexelAt(hit->uvs, hit->hit.s, hit->hit.t);
		skins.AddBlood(creature, {.u = texel.x, .v = texel.y, .skin = static_cast<uint8_t>(hit->skin & 3u), .age = 0});
	}
}

void CreatureFightSystem::LoseFightHealth(entt::entity creature, float damage)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!IsDuelling(creature) || IsKnockedOut(creature))
	{
		return;
	}
	auto& fighter = registry.Get<CreatureFighting>(creature).fighter;
	const auto before = fighter.health;
	fighter.health = fight::HealthAfterLifeLost(before, damage);
	fight_transcript::Note(creature, "level", [&] { return LevelChange(creature, "health", before, fighter.health, "life"); });
	if (fighter.health <= 0.0f)
	{
		KnockOut(creature);
	}
}
