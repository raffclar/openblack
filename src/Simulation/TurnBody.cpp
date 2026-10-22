/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TurnBody.h"

#include <cstdint>

#include <span>
#include <vector>

#include <LHVM.h>
#include <spdlog/spdlog.h>

#include "Audio/AudioManagerInterface.h"
#include "CHLApi.h"
#include "Camera/Camera.h"
#include "Camera/ScriptCameraModel.h"
#include "Common/GameRandom.h"
#include "Common/MachineClock.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/LivingListPlace.h"
#include "ECS/Components/Villager.h"
#include "ECS/Map.h"
#include "ECS/RealWeatherLand.h"
#include "ECS/Registry.h"
#include "ECS/Statistics.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/AppleBarrelSystemInterface.h"
#include "ECS/Systems/BuildingDamageSystemInterface.h"
#include "ECS/Systems/CameraBookmarkSystemInterface.h"
#include "ECS/Systems/ChimneySmokeSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/ComputerPlayerSystemInterface.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureCarryOverSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureFizzSystemInterface.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreaturePenSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/CreatureWebPageSystemInterface.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/FieldSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/FireflySystemInterface.h"
#include "ECS/Systems/FishFarmSystemInterface.h"
#include "ECS/Systems/FootpathSystemInterface.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/GuidanceSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HeartbeatSystemInterface.h"
#include "ECS/Systems/HelpMessageSystemInterface.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/MagicShieldSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/MusicMoodSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PathfindingSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/RealWeatherSystemInterface.h"
#include "ECS/Systems/RewardSystemInterface.h"
#include "ECS/Systems/SaveGameSystemInterface.h"
#include "ECS/Systems/ScriptControlSystemInterface.h"
#include "ECS/Systems/ScriptHighlightSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/Systems/SharkSystemInterface.h"
#include "ECS/Systems/SimulationSystemInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/SnowSystemInterface.h"
#include "ECS/Systems/SoundTagSystemInterface.h"
#include "ECS/Systems/SyncChecksumSystemInterface.h"
#include "ECS/Systems/TeleportSystemInterface.h"
#include "ECS/Systems/TempleDestructionSystemInterface.h"
#include "ECS/Systems/TempleExteriorSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TipBubbleSystemInterface.h"
#include "ECS/Systems/TornadoSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/Systems/VortexSystemInterface.h"
#include "ECS/Systems/WalkPathSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "LivingTurnOrder.h"
#include "Locator.h"
#include "Profiler.h"

namespace openblack::simulation
{
namespace
{

void PlayPlayerTurns()
{
	// Each player takes its turn in the order of the players, the neutral player last. First its temple: one whose heart
	// lost all its life moves on through its destruction, and a hurt heart beams. Then each of its towns, in the order
	// the player gained them, takes its turn. So the game's random numbers are drawn in the same order as the game draws
	// them.
	if (Locator::templeDestructionSystem::has_value())
	{
		Locator::templeDestructionSystem::value().StartTurn();
	}
	auto& towns = Locator::townSystem::value();
	for (uint8_t index = 0; index < static_cast<uint8_t>(PlayerNames::_COUNT); ++index)
	{
		const auto player = static_cast<PlayerNames>(index);
		if (Locator::templeDestructionSystem::has_value())
		{
			Locator::templeDestructionSystem::value().ProcessPlayerTurn(player);
		}
		towns.ProcessPlayerTowns(player);
	}
}

void PlayLivingTurn()
{
	using namespace ecs::components;
	using living_turn::Kind;

	auto& registry = Locator::entitiesRegistry::value();
	auto& profiler = Locator::profiler::value();
	// The list as the turn starts. A living thing made in the turn goes to the front of the list, which the walk has
	// passed, so its first turn is the next one.
	std::vector<living_turn::Living<entt::entity>> list;
	const auto add = [&registry, &list](entt::entity entity, Kind kind) {
		const auto* place = registry.TryGet<const LivingListPlace>(entity);
		list.push_back({.id = entity, .made = place != nullptr ? place->made : 0, .kind = kind});
	};
	registry.Each<const Villager>([&add](entt::entity entity, const Villager&) { add(entity, Kind::Villager); });
	registry.Each<const Animal>([&add](entt::entity entity, const Animal&) { add(entity, Kind::Animal); });
	// A creature that is not in the list (as the body that the tattoo menu shows) is not part of the walk
	registry.Each<const Creature, const LivingListPlace>(
	    [&add](entt::entity entity, const Creature&, const LivingListPlace&) { add(entity, Kind::Creature); });
	living_turn::NewestFirst(list);

	auto& villagers = Locator::livingActionSystem::value();
	auto& animals = Locator::animalSystem::value();
	bool animalsTurn = false;
	{
		auto animalsBegin = profiler.BeginScoped(Profiler::Stage::AnimalsUpdate);
		animalsTurn = animals.BeginTurn();
	}
	{
		auto actions = profiler.BeginScoped(Profiler::Stage::LivingActionUpdate);
		auto& reactions = Locator::reactionSystem::value();
		living_turn::Walk(
		    std::span<const living_turn::Living<entt::entity>>(list),
		    [&list, &villagers, &animals, &reactions, animalsTurn](const living_turn::Living<entt::entity>& living) {
			    // At the start of its turn, it lets go of a reaction that is over
			    reactions.EndReactionIfOver(living.id);
			    if (living.kind == Kind::Creature)
			    {
				    // A creature does each step of its own turn, in the game's order
				    [[maybe_unused]] const auto place = &living - list.data();
				    [[maybe_unused]] const auto drawsBefore =
				        Locator::gameRandom::has_value() ? Locator::gameRandom::value().GetSyncedDraws() : 0;
				    game_turn::PlayCreaturesTurn([&living](game_turn::Step step) { PlayCreatureTurnStep(step, living.id); });
				    SPDLOG_LOGGER_DEBUG(
				        spdlog::get("game"),
				        "Creature {} took its turn at place {} of {} in the list of the living: synced draws {} to {}",
				        entt::to_integral(living.id), place, list.size(), drawsBefore,
				        Locator::gameRandom::has_value() ? Locator::gameRandom::value().GetSyncedDraws() : 0);
			    }
			    else if (living.kind == Kind::Villager)
			    {
				    villagers.VillagerTurn(living.id);
			    }
			    else if (animalsTurn)
			    {
				    // The flocks fly and the wolves run and hunt
				    animals.AnimalTurn(living.id);
			    }
		    });
	}
	if (animalsTurn)
	{
		auto animalsEnd = profiler.BeginScoped(Profiler::Stage::AnimalsUpdate);
		animals.EndTurn();
	}
}

} // namespace

void PlayCreatureTurnStep(game_turn::Step step, entt::entity creature)
{
	using game_turn::Step;
	auto& profiler = Locator::profiler::value();
	// A creature that left the game in an earlier step of its turn, as one that fizzed out, does no more of its turn
	if (!Locator::entitiesRegistry::value().Valid(creature))
	{
		return;
	}
	switch (step)
	{
	case Step::CreaturePens:
		// Its home is its temple's pen, and in the pen it is shown smaller so that it fits
		Locator::creaturePenSystem::value().CreatureTurn(creature);
		break;
	case Step::CreatureBreath:
		Locator::creatureAnimationSystem::value().BreathTurn(creature);
		break;
	case Step::Leashes:
	{
		// A taut leash pulls the creature to the hand before its body lives the turn and its mind thinks
		auto creatureLeash = profiler.BeginScoped(Profiler::Stage::CreatureLeashUpdate);
		Locator::leashSystem::value().CreatureTurn(creature);
		break;
	}
	case Step::CreaturePhysiology:
	{
		// It ages, grows, gets hungry, tired and thirsty, and heals while it sleeps
		auto creaturePhysiology = profiler.BeginScoped(Profiler::Stage::CreaturePhysiologyUpdate);
		Locator::creaturePhysiologySystem::value().CreatureTurn(creature);
		break;
	}
	case Step::CreatureMinds:
	{
		auto creatureMind = profiler.BeginScoped(Profiler::Stage::CreatureMindUpdate);
		Locator::creatureMindSystem::value().MindTurn(creature);
		break;
	}
	case Step::CreatureHelp:
		// The advisor tells of the player's creature: the messages that wait, and what the help watches of it
		if (Locator::creatureHelpSystem::has_value())
		{
			Locator::creatureHelpSystem::value().CreatureTurn(creature);
		}
		break;
	case Step::CreatureDesires:
	{
		auto creatureMind = profiler.BeginScoped(Profiler::Stage::CreatureMindUpdate);
		Locator::creatureMindSystem::value().DesiresTurn(creature);
		break;
	}
	case Step::CreaturePlans:
	{
		// It weighs its desires against what it could do about them, and changes what it does for a pressing plan
		auto creaturePlanner = profiler.BeginScoped(Profiler::Stage::CreaturePlannerUpdate);
		Locator::creatureMindSystem::value().PlanCreatureTurn(creature);
		break;
	}
	case Step::CreatureAgendas:
	{
		auto creatureMind = profiler.BeginScoped(Profiler::Stage::CreatureMindUpdate);
		Locator::creatureMindSystem::value().AgendaTurn(creature);
		break;
	}
	case Step::CreatureLearning:
	{
		// It learns from what the leash shows it, and from what it found of other creatures
		auto creatureLearning = profiler.BeginScoped(Profiler::Stage::CreatureLearningUpdate);
		Locator::creatureMindSystem::value().LearnCreatureTurn(creature);
		break;
	}
	case Step::CreatureLocomotion:
	{
		// It plans its route and walks, runs and turns
		auto creatureLocomotion = profiler.BeginScoped(Profiler::Stage::CreatureLocomotionUpdate);
		Locator::creatureLocomotionSystem::value().CreatureTurn(creature);
		break;
	}
	case Step::CreatureObjectActions:
	{
		// It walks up to the thing it acts on, and what it carries makes it stronger
		auto creatureObjectActions = profiler.BeginScoped(Profiler::Stage::CreatureObjectActionUpdate);
		Locator::creatureObjectActionSystem::value().CreatureTurn(creature);
		break;
	}
	case Step::CreatureFights:
	{
		// A fight starts, moves to its next stage or ends, and a creature that is knocked out comes round
		auto creatureCombat = profiler.BeginScoped(Profiler::Stage::CreatureCombatUpdate);
		Locator::creatureFightSystem::value().FightTurn(creature);
		break;
	}
	case Step::CreatureBodies:
		// Its body follows its fatness
		Locator::creatureAnimationSystem::value().BodyTurn(creature);
		break;
	case Step::CreatureFizz:
	{
		// It fizzes on out of sight or back in, after it acted
		auto creatureCombat = profiler.BeginScoped(Profiler::Stage::CreatureCombatUpdate);
		Locator::creatureFizzSystem::value().CreatureTurn(creature);
		break;
	}
	case Step::CreatureSkin:
		// Its marks heal
		Locator::creatureSkinSystem::value().CreatureTurn(creature);
		break;
	case Step::CreatureMotes:
		Locator::creatureAnimationSystem::value().MotesTurn(creature);
		break;
	case Step::CreatureBlood:
	{
		auto creatureCombat = profiler.BeginScoped(Profiler::Stage::CreatureCombatUpdate);
		Locator::creatureFightSystem::value().BloodTurn(creature);
		break;
	}
	case Step::CreatureSway:
		Locator::creatureAnimationSystem::value().SwayTurn(creature);
		break;
	case Step::CreatureBodyActions:
	{
		// What it does with things plays on its body by the time of the turn
		auto creatureObjectActions = profiler.BeginScoped(Profiler::Stage::CreatureObjectActionUpdate);
		Locator::creatureObjectActionSystem::value().ActionTurn(creature);
		break;
	}
	case Step::CreatureFightMoves:
	{
		auto creatureCombat = profiler.BeginScoped(Profiler::Stage::CreatureCombatUpdate);
		Locator::creatureFightSystem::value().MovesTurn(creature);
		break;
	}
	case Step::CreaturePoses:
	{
		// At the end of its body's turn it takes its pose, and each frame draws its body between the last two
		auto turnPose = profiler.BeginScoped(Profiler::Stage::CreatureTurnPose);
		Locator::creatureAnimationSystem::value().EndPoseTurn(creature);
		break;
	}
	case Step::CreatureHandMoments:
	{
		// Its body knocks about what it hits, and its hands take hold of and let go of things where the pose put them
		auto creatureObjectActions = profiler.BeginScoped(Profiler::Stage::CreatureObjectActionUpdate);
		Locator::creatureObjectActionSystem::value().AfterPoseTurn(creature);
		break;
	}
	case Step::CreatureCarryOver:
		// It is written to its files as it lives, and a creature from what a last land kept sparkles into sight
		Locator::creatureCarryOverSystem::value().CreatureTurn(creature);
		break;
	case Step::CreatureWebPage:
		// The game can take a picture of the player's creature for its web page
		if (Locator::creatureWebPageSystem::has_value())
		{
			Locator::creatureWebPageSystem::value().CreatureTurn(creature);
		}
		break;
	case Step::CreatureFightStamina:
	{
		auto creatureCombat = profiler.BeginScoped(Profiler::Stage::CreatureCombatUpdate);
		Locator::creatureFightSystem::value().StaminaTurn(creature);
		break;
	}
	case Step::CreatureAlignment:
		// Its plan moves its alignment
		Locator::alignmentSystem::value().CreatureTurn(creature);
		break;
	default:
		// Not a step of a creature's own turn
		break;
	}
}

void PlayTurnStep(game_turn::Step step, TurnViewInterface& view)
{
	using game_turn::Step;
	auto& clock = Locator::time::value();
	// A script can move the camera in its turn, so the steps after the scripts hear and measure from where it is now
	const auto cameraPosition = Locator::camera::value().GetOrigin();
	auto& profiler = Locator::profiler::value();
	const auto turn = clock.GetTurn();

	switch (step)
	{
	case Step::DrawnPlaces:
		Locator::animalSystem::value().ShowTurnEnd();
		// The creatures that walk go back to where their last turn took them, with the clips of that turn's end, and what
		// they hold goes back into their hands as their last turn posed them
		Locator::creatureLocomotionSystem::value().Update(1.0f);
		Locator::creatureObjectActionSystem::value().ShowHeldAtTurnEnd();
		break;
	case Step::PlayerCommands:
		// The world does the commands that the players sent
		Locator::playerCommandSystem::value().RunSent();
		break;
	case Step::HandStillness:
		// The player's input comes first in the turn: the game counts the turns that the hand stays in one place
		Locator::handSystem::value().ProcessTurn();
		break;
	case Step::HandInterface:
		// The hand's own work of the turn, as of each frame. The commands that it sends go to a later turn.
		if (Locator::handGrabSystem::has_value())
		{
			Locator::handGrabSystem::value().InterfaceTurn(machine_clock::Ticks(), turn);
		}
		break;
	case Step::TurnCheering:
		// The view does this step
		view.PlayViewStep(step, cameraPosition);
		break;
	case Step::Clock:
		clock.StartTurn();
		break;
	case Step::ReactionClock:
		Locator::reactionSystem::value().StartTurn();
		break;
	case Step::TurnStartPlaces:
		// The villagers are drawn gliding through the turn from where they stand as it begins
		Locator::livingActionSystem::value().StartTurnPlaces();
		break;
	case Step::InfluenceTurnStart:
		// The influence asked during the turn is measured from where the hands were at it
		Locator::influenceSystem::value().SetInGameTurn(true);
		// The frames of the hand fix the reaches of the citadels. With no frames of the hand, the turn fixes them.
		Locator::influenceSystem::value().FixCitadelReaches();
		break;
	case Step::MapCells:
		// What moved since the last turn goes into its new map cell
		Locator::entitiesMap::value().Sync();
		break;
	case Step::Sharks:
		// Each shark's turn starts where it is
		Locator::sharkSystem::value().ProcessTurn();
		break;
	case Step::Storms:
		if (Locator::weatherSystem::has_value())
		{
			Locator::weatherSystem::value().ProcessStorms(turn);
		}
		break;
	case Step::Snow:
		// The storms that snow lay it on the land, and it melts
		if (Locator::weatherSystem::has_value())
		{
			Locator::snowSystem::value().ProcessTurn(Locator::weatherSystem::value().GetActiveStorms());
		}
		break;
	case Step::Players:
		// The players take their turns, each its temple and then its towns
		PlayPlayerTurns();
		break;
	case Step::ComputerPlayers:
		// In the turn of the players, each computer mind that a land script switched on starts on the land
		if (Locator::computerPlayerSystem::has_value())
		{
			Locator::computerPlayerSystem::value().ProcessTurn();
		}
		break;
	case Step::Influence:
		// How far the players' influence reaches, and its border
		Locator::influenceSystem::value().ProcessTurn(turn);
		break;
	case Step::PlayerStatistics:
		// The players' statistics take in their towns, influence and creature as the turn goes
		player_statistics::ProcessTurn(turn);
		break;
	case Step::Heartbeats:
		// The players' hearts beat on, quicker the more their people are in danger
		if (Locator::heartbeatSystem::has_value())
		{
			Locator::heartbeatSystem::value().ProcessTurn(turn);
		}
		break;
	case Step::TempleLooks:
		// The temples' outsides follow their players' alignments
		Locator::templeExteriorSystem::value().UpdateTurn();
		break;
	case Step::WorshipSiteLooks:
		// And their worship sites wear their looks
		if (Locator::worshipSiteSystem::has_value())
		{
			Locator::worshipSiteSystem::value().UpdateTurn();
		}
		break;
	case Step::PlayerAlignment:
		// Each player's waiting change moves its alignment. Then the camera takes the alignment of the player of most
		// influence at its eye.
		Locator::alignmentSystem::value().UpdatePlayersTurn(cameraPosition);
		break;
	case Step::VillageTotems:
		// A town totem that arrives at its share rings the village bell
		Locator::villageTotemSystem::value().ProcessTurn(static_cast<uint32_t>(clock.GetElapsedTime().count()));
		break;
	case Step::TownFlags:
		Locator::townDesireSystem::value().ProcessFlags();
		break;
	case Step::AppleBarrel:
		Locator::appleBarrelSystem::value().ProcessTurn();
		break;
	case Step::ChimneySmoke:
		Locator::chimneySmokeSystem::value().ProcessTurn();
		break;
	case Step::Dances:
		Locator::danceSystem::value().ProcessTurn();
		break;
	case Step::Fields:
		// The crops in the fields grow
		Locator::fieldSystem::value().ProcessTurn(turn);
		break;
	case Step::FishFarms:
		// The fish come back to the fish farms
		Locator::fishFarmSystem::value().ProcessTurn(turn);
		break;
	case Step::WalkPaths:
		// The things that the scripts walk along tracks move on
		Locator::walkPathSystem::value().ProcessTurn();
		break;
	case Step::Footpaths:
		// The footpaths that the game finds between things take a few more steps, and it lays those that it found
		if (Locator::footpathSystem::has_value())
		{
			Locator::footpathSystem::value().ProcessTurn();
		}
		break;
	case Step::ForestGrowth:
		// The forests' growing trees grow, faster in the rain, and the forests spread
		Locator::forestSystem::value().GrowForests();
		break;
	case Step::CreaturesTurnStart:
		// The parts of the creatures' turns that are not for one creature: the turns that the bodies live, the posts of
		// the leashes, the feedback that reaches the minds, and what creatures that are gone held
		Locator::creaturePhysiologySystem::value().BeginTurn();
		Locator::leashSystem::value().BeginTurn();
		Locator::creatureMindSystem::value().BeginTurn();
		Locator::creatureObjectActionSystem::value().BeginTurn();
		break;
	case Step::CreaturePhysiology:
	case Step::CreaturePens:
	case Step::CreatureBreath:
	case Step::CreatureBodies:
	case Step::CreatureSkin:
	case Step::Leashes:
	case Step::CreatureMinds:
	case Step::CreatureDesires:
	case Step::CreaturePlans:
	case Step::CreatureAgendas:
	case Step::CreatureLearning:
	case Step::CreatureLocomotion:
	case Step::CreatureObjectActions:
	case Step::CreatureFights:
	case Step::CreatureFizz:
	case Step::CreatureHelp:
	case Step::CreatureMotes:
	case Step::CreatureBlood:
	case Step::CreatureSway:
	case Step::CreatureBodyActions:
	case Step::CreatureFightMoves:
	case Step::CreatureFightStamina:
	case Step::CreaturePoses:
	case Step::CreatureHandMoments:
	case Step::CreatureWebPage:
	case Step::CreatureCarryOver:
	case Step::CreatureAlignment:
		// Each creature does these steps in its own turn (see PlayCreatureTurnStep)
		break;
	case Step::Living:
		PlayLivingTurn();
		break;
	case Step::MapCellsAfterLiving:
		// The living that walked this turn go into their new map cells before anything searches them
		Locator::entitiesMap::value().Sync();
		break;
	case Step::Fires:
		// The game burns its fires here, after the living and before the miracles and the scripts. A fire that a later
		// step lights in this turn waits for the next turn before it burns.
		if (Locator::fireSystem::has_value())
		{
			auto fire = profiler.BeginScoped(Profiler::Stage::FireUpdate);
			Locator::fireSystem::value().MarkBurnPoint();
			// What is hot burns, cools and spreads, and the villagers near it react
			Locator::fireSystem::value().ProcessTurn();
		}
		break;
	case Step::Reactions:
	{
		// What the miracles and other events made the living react to: belief for the villagers' towns, and the
		// creatures that the events impressed
		auto reactions = profiler.BeginScoped(Profiler::Stage::ReactionsUpdate);
		Locator::reactionSystem::value().ProcessTurn();
		break;
	}
	case Step::Teleports:
	{
		// The teleport stones: the passers-by turn aside into them and jump between them
		auto teleport = profiler.BeginScoped(Profiler::Stage::TeleportUpdate);
		Locator::teleportSystem::value().ProcessTurn();
		break;
	}
	case Step::WorshipChants:
		// The worship sites charge their icons and keep what their dancers chant, before the miracles pay their upkeep
		if (Locator::worshipSiteSystem::has_value())
		{
			Locator::worshipSiteSystem::value().ProcessChants();
		}
		break;
	case Step::Miracles:
	{
		// The dispensers, then each miracle's upkeep, its own particle effect and what that effect tells it
		auto magic = profiler.BeginScoped(Profiler::Stage::MagicUpdate);
		Locator::magicSystem::value().ProcessTurn();
		break;
	}
	case Step::Shields:
	{
		// The shields' objects: the spiritual shields' marks, the physical shields' domes, and the people who shelter
		auto shields = profiler.BeginScoped(Profiler::Stage::MagicShieldsUpdate);
		Locator::magicShieldSystem::value().ProcessTurn();
		break;
	}
	case Step::MiracleForests:
	{
		// The forest miracles' forests grow or wither
		auto forests = profiler.BeginScoped(Profiler::Stage::ForestsUpdate);
		Locator::forestSystem::value().ProcessTurn();
		break;
	}
	case Step::Particles:
	{
		// The particle effects that no miracle owns take a step, and the spot visuals count down
		auto particles = profiler.BeginScoped(Profiler::Stage::ParticlesUpdate);
		Locator::particleSystem::value().ProcessTurn();
		break;
	}
	case Step::Tornadoes:
	{
		// A tornado lets go of what it carried and threw once its particle has gone
		auto tornado = profiler.BeginScoped(Profiler::Stage::TornadoUpdate);
		Locator::tornadoSystem::value().ProcessTurn();
		break;
	}
	case Step::Fireflies:
		// The fireflies come out at nightfall and go home at dawn, by the time of day at the start of the turn
		if (Locator::fireflySystem::has_value())
		{
			Locator::fireflySystem::value().ProcessTurn();
		}
		break;
	case Step::Physics:
		// Then the physics, after the living, the fires, the reactions, the miracles and the particles. A body that any
		// of them sets in motion in this turn flies in this turn: it flies, collides and comes to rest.
		if (Locator::dynamicsSystem::has_value())
		{
			auto physics = profiler.BeginScoped(Profiler::Stage::PhysicsUpdate);
			Locator::dynamicsSystem::value().ProcessTurn();
		}
		break;
	case Step::Vortices:
		// The vortices between the lands open, close and make the ground level under them
		if (Locator::vortexSystem::has_value())
		{
			Locator::vortexSystem::value().ProcessTurn();
		}
		break;
	case Step::Scripts:
		// The scripts see the world after its turn
		Locator::vm::value().LookIn(lhvm::ScriptType::All);
		break;
	case Step::ScriptObjects:
		// Every object that the scripts do not hold in a variable now lets go of its place in their table
		Locator::scriptObjects::value().ReleaseUnreferenced();
		break;
	case Step::ScriptFade:
		// The scripts' fade moves on with their turn
		Locator::cinematicDirectorSystem::value().ProcessTurn();
		break;
	case Step::Advisors:
		// The advisors follow what they point at and look at
		Locator::advisorSystem::value().ProcessTurn();
		break;
	case Step::HandToolTips:
		// The view does this step
		view.PlayViewStep(step, cameraPosition);
		break;
	case Step::Banter:
		// The advisors banter when the player has had no help for a while
		if (Locator::helpMessageSystem::has_value())
		{
			Locator::helpMessageSystem::value().ProcessTurn();
		}
		break;
	case Step::HelpProfile:
		// The view does this step
		view.PlayViewStep(step, cameraPosition);
		break;
	case Step::TipBubble:
		// The help's tip bubble stays while its sign shows
		if (Locator::tipBubbleSystem::has_value())
		{
			Locator::tipBubbleSystem::value().ProcessTurn();
		}
		break;
	case Step::TimeOfDay:
		// The time of day moves on
		Locator::skySystem::value().ProcessTurn();
		break;
	case Step::WeatherThings:
		// The weather things follow their storms, which the wind blows along if it can
		if (Locator::weatherSystem::has_value())
		{
			Locator::weatherSystem::value().ProcessWeatherThings();
		}
		break;
	case Step::CameraBookmarks:
		// The camera's bookmarks on things move with their things, and the camera flies again to the bookmark that it follows
		Locator::cameraBookmarkSystem::value().ProcessTurn();
		break;
	case Step::ScriptHighlights:
		// The scrolls and signs of the scripts pulse and find what they stand on. The signs whose tips the player read
		// start.
		if (Locator::scriptHighlightSystem::has_value())
		{
			Locator::scriptHighlightSystem::value().ProcessTurn();
		}
		break;
	case Step::Climates:
		if (Locator::weatherSystem::has_value())
		{
			Locator::weatherSystem::value().ProcessClimates(turn);
		}
		break;
	case Step::HeldThing:
		// What the hand holds stays where the hand is for the game, and the hand lets go of it once it is gone
		if (Locator::handGrabSystem::has_value())
		{
			Locator::handGrabSystem::value().ProcessTurn();
		}
		break;
	case Step::Rewards:
		// A reward chest that fell to the ground goes into the map's cells
		if (Locator::rewardSystem::has_value())
		{
			Locator::rewardSystem::value().ProcessTurn();
		}
		break;
	case Step::Guidance:
		// The advisors' guidance: a new land's first stamps, and a look at the moon at night
		if (Locator::guidanceSystem::has_value())
		{
			Locator::guidanceSystem::value().ProcessTurn();
		}
		break;
	case Step::CameraChecks:
		// A script's two-thing camera lets go of a thing that has gone
		if (auto* scriptCamera = Locator::scriptControlSystem::value().GetScriptCamera(Locator::camera::value());
		    scriptCamera != nullptr)
		{
			scriptCamera->LetGoOfGoneThings();
		}
		// The camera of a dance that ended, or whose dance went, gives the camera back
		if (Locator::danceSystem::has_value())
		{
			Locator::danceSystem::value().CheckCamera();
		}
		break;
	case Step::BuildingPieces:
		// The pieces broken off buildings count down their time, and go when it runs out
		if (Locator::buildingDamageSystem::has_value())
		{
			Locator::buildingDamageSystem::value().ProcessTurn();
		}
		break;
	case Step::MusicMoods:
		// The music moods look at the custom music that plays
		if (Locator::musicMoodSystem::has_value())
		{
			Locator::musicMoodSystem::value().UpdateOnGameTurn(false);
		}
		break;
	case Step::SoundTags:
		// The objects' looping sounds start again where they stopped, within reach of where the game hears them from
		Locator::soundTagSystem::value().ProcessTurn(Locator::audio::value().GetListener().origin);
		break;
	case Step::Ambience:
		// The view does this step
		view.PlayViewStep(step, cameraPosition);
		break;
	case Step::Music:
		// The view does this step
		view.PlayViewStep(step, cameraPosition);
		break;
	case Step::Autosave:
		// At the end of the turn the game autosaves when an autosave is due
		if (Locator::saveGameSystem::has_value())
		{
			Locator::saveGameSystem::value().EndTurn();
		}
		break;
	case Step::GameOver:
		// Once the whole turn is over, the local player whose temple the enemy destroys has lost
		if (Locator::templeDestructionSystem::has_value())
		{
			Locator::templeDestructionSystem::value().EndTurn();
		}
		break;
	case Step::GlobalStatistics:
		player_statistics::ProcessGlobalTurn(turn, view.FrameRate());
		break;
	case Step::RealWeather:
		// After the turn of the game, the real-world weather that came changes the weather of the land
		if (Locator::realWeatherSystem::has_value())
		{
			ecs::RealWeatherLand land;
			Locator::realWeatherSystem::value().ProcessTurn(land);
		}
		break;
	case Step::WalkerLookahead:
		// Then one walker that came up against water, the land's edge or a fence tries its walk both ways round
		Locator::pathfindingSystem::value().ProcessLookahead();
		break;
	case Step::InfluenceTurnEnd:
		Locator::influenceSystem::value().SetInGameTurn(false);
		// The view's steps fix the reach of a citadel that the turn made. With no view, the end of the turn fixes it.
		Locator::influenceSystem::value().FixCitadelReaches();
		break;
	case Step::SyncChecksum:
		// When a network game asks for it, the checksum of the synced world as this turn left it
		Locator::syncChecksumSystem::value().ProcessTurn();
		break;
	case Step::RunRecord:
		// A tool that records the run reads the world as the turn left it. The turn count moved on at the start of the
		// turn, so the number is the turn after this one.
		Locator::simulation::value().TurnEnded(clock.GetTurn());
		break;
	case Step::_Count:
		break;
	}
}

void PlayTurn(TurnViewInterface& view)
{
	game_turn::PlayTurn([&view](game_turn::Step step) { PlayTurnStep(step, view); });
}

} // namespace openblack::simulation
