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

#include <optional>
#include <string_view>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "3D/MapCoords.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureFizz.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureRestAndHome.h"
#include "CreatureFightSystem.h"
#include "CreatureFightSystemDetail.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreatureHome.h"
#include "ECS/CreaturePlacement.h"
#include "ECS/CreatureScars.h"
#include "ECS/Map.h"
#include "ECS/PosedModel.h"
#include "ECS/Registry.h"
#include "ECS/ShownCreatureFaces.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/FightTranscriptNote.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/ThingCircles.h"
#include "ECS/ThingMapPlace.h"
#include "ECS/ThingModel.h"
#include "ECS/WorldObjects.h"
#include "Locator.h"
#include "Sound/SoundRequestInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
namespace fight = openblack::creature_fight;
using namespace openblack::ecs::systems::fight_detail;

namespace
{
/// Whether the creature fainted and did not get up from that faint or have a turn with no plan since. Such a creature
/// does not faint again.
bool StillFainted(const ecs::Registry& registry, entt::entity creature)
{
	const auto* needs = registry.TryGet<const CreatureNeeds>(creature);
	return needs != nullptr && needs->fainted;
}

/// Marks that the creature fainted. A script's order to die is not a faint, and does not mark it.
void MarkFainted(ecs::Registry& registry, entt::entity creature)
{
	if (auto* needs = registry.TryGet<CreatureNeeds>(creature))
	{
		needs->fainted = true;
	}
}

void PlaceAt(CreatureLocomotion& locomotion, Transform& transform, glm::vec3 at)
{
	at.y = GroundAt(Flat(at));
	locomotion.fromPosition = at;
	locomotion.toPosition = at;
	transform.position = at;
}
} // namespace

void CreatureFightSystem::KnockOut(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !registry.AllOf<Creature, CreatureAnimation>(creature) ||
	    registry.AllOf<CreatureKnockedOut>(creature) || StillFainted(registry, creature))
	{
		return;
	}
	// In a duel, its opponent wins it
	if (auto* fighting = registry.TryGet<CreatureFighting>(creature))
	{
		if (fighting->stage == CreatureFighting::Stage::Duel && InDuel(registry, fighting->opponent))
		{
			Win(fighting->opponent, creature, false);
			return;
		}
		Leave(creature);
	}
	Faint(creature);
	MarkFainted(registry, creature);
}

void CreatureFightSystem::ForceFaint(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !registry.AllOf<Creature, CreatureAnimation>(creature) ||
	    registry.AllOf<CreatureKnockedOut>(creature) || StillFainted(registry, creature))
	{
		return;
	}
	if (registry.AllOf<CreatureFighting>(creature))
	{
		Leave(creature);
	}
	Faint(creature);
	MarkFainted(registry, creature);
}

void CreatureFightSystem::FaintForScript(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !registry.AllOf<Creature, CreatureAnimation>(creature) ||
	    registry.AllOf<CreatureKnockedOut>(creature))
	{
		return;
	}
	if (registry.AllOf<CreatureFighting>(creature))
	{
		Leave(creature);
	}
	Faint(creature, Leash::Keep);
}

void CreatureFightSystem::Faint(entt::entity creature, Leash leash)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (Locator::creatureLocomotionSystem::has_value())
	{
		Locator::creatureLocomotionSystem::value().StopAtOnce(creature);
	}
	// The action that its body plays ends at two times the usual speed. What its hands do goes on, and it keeps what it
	// holds: before the faint starts, it lets go of the thing as before any action that needs empty hands.
	if (Locator::creatureAnimationSystem::has_value())
	{
		Locator::creatureAnimationSystem::value().Hurry(creature);
	}
	// The faint stops what its mind does as a failure, an emergency too. Until it gets up, it looks at no emergency.
	if (Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().AbandonAction(creature);
	}
	// Fainting drops the leash, but not when a script orders the creature dead
	if (leash == Leash::TakeOff && Locator::leashSystem::has_value() && Locator::leashSystem::value().IsLeashed(creature))
	{
		Locator::leashSystem::value().TakeOff(creature);
	}
	// Whether it will rest to get better once it has lain long enough is decided now, by how it is as it faints
	bool rest = false;
	if (auto* needs = registry.TryGet<CreatureNeeds>(creature))
	{
		needs->rest = CreatureNeeds::Rest::Unconscious;
		rest = fight::NeedsRest(needs->needs.life, needs->needs.exhaustion);
	}
	// The spells on it are brought to their end
	if (auto* spells = registry.TryGet<CreatureSpells>(creature))
	{
		creature_spells::TryFinishAll(spells->spells);
	}
	// It is taken to its home: its temple's pen while the temple stands, else the home it was last given (where it was
	// made, unless a script moved it). Only a creature with no home at all stays where it lies.
	const auto here = registry.Get<const Transform>(creature).position;
	const auto home = creature_home::HomeOf(registry, creature).value_or(here);
	// The faint itself starts on the body on the next turn that the knock-out is looked after
	registry.AssignOrReplace<CreatureKnockedOut>(creature, CreatureKnockedOut {.rest = rest, .home = home});
	fight_transcript::Note(creature, "faint", [&] {
		auto fields = Levels(registry, creature);
		fields.emplace_back("rest_later", rest);
		fields.emplace_back("at_x", here.x);
		fields.emplace_back("at_z", here.z);
		fields.emplace_back("home_x", home.x);
		fields.emplace_back("home_z", home.z);
		fields.emplace_back("script_controlled", registry.AllOf<ScriptControlled>(creature));
		return fields;
	});
}

void CreatureFightSystem::StartFaint(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	// The faint also stops what the hands do, such as a catch or a let-go. The hand keeps what it holds.
	if (Locator::creatureObjectActionSystem::has_value())
	{
		Locator::creatureObjectActionSystem::value().Cancel(creature);
	}
	// The faint also stops a walk at once. Between the knock-out and this turn, an action of the hands can start a new
	// walk to its thing. If the walk stays, the creature gets up to follow it and the faint does not end.
	if (Locator::creatureLocomotionSystem::has_value())
	{
		Locator::creatureLocomotionSystem::value().StopAtOnce(creature);
	}
	// The faint cuts what the body plays at once. The game's random numbers choose its side, one time in two the left
	// side.
	auto& animation = registry.Get<CreatureAnimation>(creature);
	const bool mirrored = Locator::gameRandom::has_value() && Locator::gameRandom::value().GameRand(2) == 1;
	animation.body = {
	    .kind = creature_layers::BodyAction::Kind::Sequence,
	    .phase = creature_layers::BodyAction::Phase::Start,
	    .animations = {fight::animations::k_Faint, fight::animations::k_Faint, fight::animations::k_GetUp},
	    .mirrored = mirrored,
	    .holdLoop = true,
	};
	shown_creature_faces::ChangeFace(registry, creature, animation.face,
	                                 [](const creature_layers::FaceLayer& face) { return creature_layers::RelaxFace(face); });
	// The eyes close later, when the body first lies still
	// The faint on the body marks it as fainted, also when a script orders it to die
	MarkFainted(registry, creature);
	// The people and the creatures near it react to the faint as to a death. The reaction belongs to no player.
	if (Locator::reactionSystem::has_value())
	{
		Locator::reactionSystem::value().Create({.initiator = creature,
		                                         .type = Reaction::ReactToDeath,
		                                         .position = registry.Get<const Transform>(creature).position,
		                                         .playerless = true});
	}
	fight_transcript::Note(creature, "faint_starts", [&] {
		return fight_transcript::Fields {{"creature", fight_transcript::Id(creature)}, {"mirrored", mirrored}};
	});
}

void CreatureFightSystem::KillPermanently(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		return;
	}
	if (registry.AllOf<CreatureFighting>(creature))
	{
		AbortFight(creature);
		Leave(creature);
	}
	// A creature killed for good lies dead, also when it got up from a faint a short time before
	if (auto* needs = registry.TryGet<CreatureNeeds>(creature))
	{
		needs->fainted = false;
	}
	KnockOut(creature);
	if (auto* knockedOut = registry.TryGet<CreatureKnockedOut>(creature))
	{
		StopCarryingHome(creature, *knockedOut);
		knockedOut->permanent = true;
		knockedOut->stage = CreatureKnockedOut::Stage::Lying;
	}
	if (auto* needs = registry.TryGet<CreatureNeeds>(creature))
	{
		needs->needs.life = 0.0f;
	}
}

void CreatureFightSystem::Resurrect(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* knockedOut = registry.Valid(creature) ? registry.TryGet<CreatureKnockedOut>(creature) : nullptr;
	if (knockedOut == nullptr)
	{
		return;
	}
	// The get-up of the resurrection lets it faint again
	if (auto* needs = registry.TryGet<CreatureNeeds>(creature))
	{
		needs->needs.life = fight::k_GetUpLife;
		needs->fainted = false;
	}
	StopCarryingHome(creature, *knockedOut);
	knockedOut->permanent = false;
	knockedOut->fainting = false;
	knockedOut->handsChecked = false;
	knockedOut->stage = CreatureKnockedOut::Stage::GettingUp;
	knockedOut->seconds = 0.0f;
	auto& animation = registry.Get<CreatureAnimation>(creature);
	animation.body = creature_layers::EndLoop(animation.body);
	SetEyes(registry, creature, creature_eyes::Mode::Calm);
}

bool CreatureFightSystem::IsKnockedOut(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(creature) && registry.AllOf<CreatureKnockedOut>(creature);
}

void CreatureFightSystem::ProcessKnockedOut()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> lying;
	registry.Each<CreatureKnockedOut>(
	    [&lying](entt::entity entity, CreatureKnockedOut& /*knockedOut*/) { lying.push_back(entity); });
	for (const auto entity : lying)
	{
		KnockedOutTurn(entity);
	}
}

void CreatureFightSystem::KnockedOutTurn(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity) || !registry.AllOf<CreatureKnockedOut>(entity))
	{
		return;
	}
	// The first turn after the knock-out starts the faint on the body. First the creature checks its hands: with a
	// thing in the hand it starts to let go, and the faint starts on the next turn and cuts the let-go. While its
	// body or hands are busy it cannot start to let go, and the faint waits.
	auto& knockedOut = registry.Get<CreatureKnockedOut>(entity);
	if (knockedOut.fainting)
	{
		const auto check = knockedOut.handsChecked || !Locator::creatureMindSystem::has_value()
		                       ? creature_mind::HandsCheck::Free
		                       : Locator::creatureMindSystem::value().CheckHandsBeforeAction(entity);
		knockedOut.handsChecked = check == creature_mind::HandsCheck::LetGo;
		if (check == creature_mind::HandsCheck::Free)
		{
			knockedOut.fainting = false;
			StartFaint(entity);
		}
	}
	auto* needs = registry.TryGet<CreatureNeeds>(entity);
	auto& animation = registry.Get<CreatureAnimation>(entity);
	const auto& body = registry.Get<const Creature>(entity);
	const fight::FaintBody faintBody {.faintStarted = !knockedOut.fainting,
	                                  .acts = creature_layers::IsPlaying(animation.body),
	                                  .lies = creature_layers::IsLooping(animation.body)};
	if (knockedOut.stage == CreatureKnockedOut::Stage::Lying)
	{
		// It counts the time that it lies only from when it lies still for the first time. A script that moves it
		// after that stands its body, but the time goes on.
		const auto wait = fight::LyingTurn({.seconds = knockedOut.seconds, .started = knockedOut.layStill}, faintBody,
		                                   k_TurnSeconds, -creature_mind::k_FaintWaitTurnsLate * k_TurnSeconds);
		// The eyes close when the body first lies still
		if (wait.started && !knockedOut.layStill)
		{
			SetEyes(registry, entity, creature_eyes::Mode::Closed);
		}
		knockedOut.seconds = wait.seconds;
		knockedOut.layStill = wait.started;
	}
	else if (knockedOut.stage != CreatureKnockedOut::Stage::Waiting || fight::WaitTurnCounts(faintBody))
	{
		knockedOut.seconds += k_TurnSeconds;
	}
	const auto next = [&knockedOut, entity](CreatureKnockedOut::Stage stage) {
		fight_transcript::Note(entity, "knocked_out", [&] {
			return fight_transcript::Fields {{"creature", fight_transcript::Id(entity)},
			                                 {"from", Name(knockedOut.stage)},
			                                 {"to", Name(stage)},
			                                 {"seconds", knockedOut.seconds}};
		});
		knockedOut.stage = stage;
		knockedOut.seconds = 0.0f;
	};
	const auto getUp = [&] {
		next(CreatureKnockedOut::Stage::GettingUp);
		// When it starts to get up, it can faint again
		if (needs != nullptr)
		{
			needs->fainted = false;
		}
		animation.body = creature_layers::EndLoop(animation.body);
		SetEyes(registry, entity, creature_eyes::Mode::Calm);
	};

	switch (knockedOut.stage)
	{
	case CreatureKnockedOut::Stage::Lying:
		if (!knockedOut.permanent && fight::TurnsCounted(knockedOut.seconds, k_TurnSeconds) >=
		                                 fight::FaintWaitTurns(ShownSize(body), k_TurnMilliseconds))
		{
			// A player's creature is taken home; any other comes round where it lies
			if (body.owner != PlayerNames::NEUTRAL)
			{
				// It fizzes out of sight where it lies, with the teleport's sound
				next(CreatureKnockedOut::Stage::FadingOut);
				knockedOut.fizzTurns = creature_fizz::TurnsOf(creature_fizz::k_CarryHomeSeconds, k_TurnMilliseconds);
				SetFizz(entity, 1.0f, creature_fizz::k_CarryHomeSeconds);
			}
			else
			{
				next(CreatureKnockedOut::Stage::Waiting);
			}
		}
		break;
	case CreatureKnockedOut::Stage::FadingOut:
		knockedOut.fizzTurns = knockedOut.fizzTurns > 0 ? knockedOut.fizzTurns - 1 : 0;
		if (knockedOut.fizzTurns == 0)
		{
			// Out of sight, it is moved home and fizzes back into sight there
			auto* locomotion = registry.TryGet<CreatureLocomotion>(entity);
			if (knockedOut.home.has_value() && locomotion != nullptr)
			{
				// The temple's pen is taken again now, as the home follows the temple every turn
				const auto from = registry.Get<const Transform>(entity).position;
				// The creature goes to the exact map position of its home, which the leash keeps
				const auto* leash = registry.TryGet<const CreatureLeash>(entity);
				const auto homeOnMap = leash != nullptr ? leash->homeOnMap : std::nullopt;
				auto to = creature_home::HomeOf(registry, entity).value_or(*knockedOut.home);
				if (homeOnMap.has_value())
				{
					to.x = map_coords::ToMetres(homeOnMap->x);
					to.z = map_coords::ToMetres(homeOnMap->z);
				}
				PlaceAt(*locomotion, registry.Get<Transform>(entity), to);
				ecs::thing_map_place::Keep(registry, entity, homeOnMap.value_or(map_coords::FromMetres({to.x, to.z})));
				registry.SetDirty();
				fight_transcript::Note(entity, "taken_home", [&] {
					return fight_transcript::Fields {{"creature", fight_transcript::Id(entity)},
					                                 {"from_x", from.x},
					                                 {"from_z", from.z},
					                                 {"to_x", to.x},
					                                 {"to_z", to.z},
					                                 {"distance", glm::distance(Flat(from), Flat(to))},
					                                 {"script_controlled", registry.AllOf<ScriptControlled>(entity)}};
				});
			}
			SetFizz(entity, 0.0f, creature_fizz::k_CarryHomeSeconds);
			// Home, it is no more exhausted or thirsty than it can bear, with a little energy
			if (Locator::creaturePhysiologySystem::has_value())
			{
				Locator::creaturePhysiologySystem::value().WakeFromFaint(entity);
			}
			next(CreatureKnockedOut::Stage::FadingIn);
			knockedOut.fizzTurns = creature_fizz::TurnsOf(creature_fizz::k_CarryHomeSeconds, k_TurnMilliseconds);
		}
		break;
	case CreatureKnockedOut::Stage::FadingIn:
		knockedOut.fizzTurns = knockedOut.fizzTurns > 0 ? knockedOut.fizzTurns - 1 : 0;
		if (knockedOut.fizzTurns == 0)
		{
			next(CreatureKnockedOut::Stage::Waiting);
		}
		break;
	case CreatureKnockedOut::Stage::Waiting:
		if (fight::TurnsCounted(knockedOut.seconds, k_TurnSeconds) >=
		    creature_fizz::TurnsOf(fight::k_WaitAfterHomeSeconds, k_TurnMilliseconds))
		{
			next(CreatureKnockedOut::Stage::WaitingForSpells);
		}
		break;
	case CreatureKnockedOut::Stage::WaitingForSpells:
	{
		// The spells on it are brought to their end each turn until none is left
		auto* spells = registry.TryGet<CreatureSpells>(entity);
		if (spells == nullptr || creature_spells::TryFinishAll(spells->spells))
		{
			if (knockedOut.rest)
			{
				next(CreatureKnockedOut::Stage::Resting);
			}
			else
			{
				getUp();
			}
		}
		break;
	}
	case CreatureKnockedOut::Stage::Resting:
	{
		// It rests until its life is full, it is not tired at all, and the rest has lasted long enough
		const auto turns = static_cast<uint32_t>(std::lround(knockedOut.seconds / k_TurnSeconds));
		if (needs == nullptr ||
		    (creature_mind::IsBetter(needs->needs.life, needs->needs.exhaustion) && turns > creature_mind::k_FewestRestTurns))
		{
			getUp();
		}
		break;
	}
	case CreatureKnockedOut::Stage::GettingUp:
		if (!creature_layers::IsPlaying(animation.body))
		{
			if (needs != nullptr)
			{
				needs->rest = CreatureNeeds::Rest::Awake;
			}
			fight_transcript::Note(entity, "got_up", [&] { return Levels(registry, entity); });
			registry.Remove<CreatureKnockedOut>(entity);
			return;
		}
		break;
	}
	if (needs != nullptr)
	{
		// Resting, its body heals; otherwise it is out cold until it gets up
		needs->rest = knockedOut.stage == CreatureKnockedOut::Stage::Resting     ? CreatureNeeds::Rest::Resting
		              : knockedOut.stage == CreatureKnockedOut::Stage::GettingUp ? CreatureNeeds::Rest::Awake
		                                                                         : CreatureNeeds::Rest::Unconscious;
	}
}
