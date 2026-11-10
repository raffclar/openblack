/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cctype>
#include <cmath>
#include <cstdint>
#include <ctime>

#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <iterator>
#include <limits>
#include <numbers>
#include <optional>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <LHVMTypes.h>
#include <SaveSlots.h>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/matrix.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "3D/CameraEdits.h"
#include "3D/DayNightClock.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/TempleInteriorInterface.h"
#include "CHLApi.h"
#include "ECS/GamePlayers.h"
#include "Sound/GameMusic.h"
#if defined(OPENBLACK_DEVELOPER_TOOLS)
#include "Common/DrawTrace.h"
#endif
#include "CHLApi/NativeHelpers.h"
#include "CHLApi/Natives.h"
#include "Common/GameRandom.h"
#include "Common/MachineClock.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureHandGlow.h"
#include "Creature/CreatureScriptPlay.h"
#include "Creature/LeashRules.h"
#include "ECS/Archetypes/AnimatedStaticArchetype.h"
#include "ECS/Archetypes/BallArchetype.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Archetypes/FeatureArchetype.h"
#include "ECS/Archetypes/MistArchetype.h"
#include "ECS/Archetypes/MobileObjectArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Archetypes/PuzzleTotemArchetype.h"
#include "ECS/Archetypes/ScriptMarkerArchetype.h"
#include "ECS/Archetypes/SharkArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Archetypes/VillagerArchetype.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureHelp.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/PuzzleGame.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/ScriptTimer.h"
#include "ECS/Components/Sky.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/CreatureFromFile.h"
#include "ECS/CreatureRemoval.h"
#include "ECS/DanceRules.h"
#include "ECS/Dances.h"
#include "ECS/DiscipleRules.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/PlayerTowns.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Registry.h"
#include "ECS/ScriptCreatureCounts.h"
#include "ECS/ScriptDelete.h"
#include "ECS/ScriptFind.h"
#include "ECS/ScriptFlocks.h"
#include "ECS/SpellCreator.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/BoatLaunchSystemInterface.h"
#include "ECS/Systems/CameraHelpSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/ComputerPlayerSystemInterface.h"
#include "ECS/Systems/CreatureAudioSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/DialogueControlSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/EffectRequestSystemInterface.h"
#include "ECS/Systems/FootballSystemInterface.h"
#include "ECS/Systems/HandDemoSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/HighDetailSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerScript.h"
#include "ECS/Systems/IntroSystemInterface.h"
#include "ECS/Systems/LandSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/PlaygroundTripSystemInterface.h"
#include "ECS/Systems/PuzzleGameSystemInterface.h"
#include "ECS/Systems/PuzzleTotemSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/RewardSystemInterface.h"
#include "ECS/Systems/SaveGameSystemInterface.h"
#include "ECS/Systems/ScriptControlSystemInterface.h"
#include "ECS/Systems/ScriptHighlightSystemInterface.h"
#include "ECS/Systems/ScriptMusicSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/Systems/SkyClockSystemInterface.h"
#include "ECS/Systems/TempleDestructionSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TurnCheerSystemInterface.h"
#include "ECS/Systems/TutorialSkipSystemInterface.h"
#include "ECS/Systems/VideoSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/Systems/VortexSystemInterface.h"
#include "ECS/Systems/WalkPathSystemInterface.h"
#include "ECS/TownHomes.h"
#include "ECS/VillagerSkeletons.h"
#include "ECS/WorkshopRules.h"
#include "ECS/WorldObjects.h"
#include "Enums.h"
#include "FileSystem/FileSystemInterface.h"
#include "Hand/HandGrabRules.h"
#include "Help/DialogueText.h"
#include "Help/HelpProfile.h"
#include "Help/Spirits.h"
#include "Input/GameActionMapInterface.h"
#include "LHScriptX/FeatureScriptCommands.h"
#include "LHScriptX/Script.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Magic/TownBelief.h"
#include "Physics/Body.h"
#include "ScriptHeaders/ScriptCreateRules.h"
#include "ScriptHeaders/ScriptDeadLiving.h"
#include "ScriptHeaders/ScriptEnums.h"
#include "ScriptHeaders/ScriptLessonNatives.h"
#include "ScriptHeaders/ScriptNameLists.h"
#include "ScriptHeaders/ScriptRandom.h"
#include "ScriptHeaders/ScriptRealTime.h"
#include "ScriptHeaders/ScriptTownHealth.h"
#include "Story/PlaygroundTrip.h"
#include "Temple/TempleEntrancePoint.h"

namespace openblack::chlapi
{

using openblack::Locator;
using openblack::ecs::components::Transform;

void Random() // 028 RANDOM
{
	// A whole number from min to max, both included
	const auto max = Popf();
	const auto min = Popf();
	Pushf(script::random::NumberFrom(min, max, Locator::gameRandom::value()));
}

void DllGettime() // 029 DLL_GETTIME
{
	// The game leaves this one to the script machine: its own clock
	Locator::vm::value().PushElaspedTime();
}

void SetInterfaceInteraction() // 063 SET_INTERFACE_INTERACTION
{
	const auto level = Pop().intVal;
	if (!CHLApi::ApplyInterfaceLevel(level))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Unexpected interaction : {}", level);
	}
}

void Played() // 064 PLAYED
{
	const auto object = PopObject();
	if (!Locator::entitiesRegistry::value().Valid(object))
	{
		// Something gone has played whatever it was asked to
		ObjectNoLongerValid();
		Pushb(true);
		return;
	}
	if (IsDirectableVillager(object))
	{
		Pushb(Locator::livingActionSystem::value().VillagerHasPlayedScriptAnimation(object));
		return;
	}
	if (Locator::entitiesRegistry::value().AllOf<ecs::components::Creature>(object))
	{
		if (!Locator::creatureMindSystem::has_value())
		{
			Pushb(true);
			return;
		}
		// The answer yes also stops the remaining steps of the agenda, as an idle creature has steps that remain
		auto& minds = Locator::creatureMindSystem::value();
		const bool played = minds.HasPlayed(object);
		if (played)
		{
			minds.ClearAfterPlayed(object);
		}
		Pushb(played);
		return;
	}
	if (Locator::puzzleGameSystem::has_value())
	{
		// A puzzle game has played once the player solves it
		if (const auto played = Locator::puzzleGameSystem::value().Played(object))
		{
			Pushb(*played);
			return;
		}
	}
	if (const auto* animal = Locator::entitiesRegistry::value().TryGet<const ecs::components::Animal>(object))
	{
		// An animal has played once it is back in the script's hands, or on its way somewhere to end up there
		using ecs::components::AnimalState;
		const auto state = animal->state == AnimalState::MoveToPos ? animal->finalState : animal->state;
		Pushb(state == AnimalState::InScript);
		return;
	}
	// TODO(opening): the weather and dances
	NotImplemented();
	Pushb(true);
}

void RandomUlong() // 065 RANDOM_ULONG
{
	const auto high = Pop().uintVal;
	const auto low = Pop().uintVal;
	Pushi(static_cast<int32_t>(script::random::WholeNumberBetween(low, high, Locator::gameRandom::value())));
}

void SetGamespeed() // 066 SET_GAMESPEED
{
	const auto speed = Popf();
	if (Locator::scriptControlSystem::value().MaySetGameSpeed(CurrentTask()))
	{
		Locator::time::value().SetSpeed(speed);
	}
}

/// The scripts' countdown timer
script::timers::CountdownTimer& ScriptCountdown()
{
	return Locator::scriptControlSystem::value().GetCountdownTimer();
}

void StartCountdownTimer() // 084 START_COUNTDOWN_TIMER
{
	// The timer starts again for the seconds and shows. The game says nothing about a time of zero or less.
	ScriptCountdown().Start(Popf());
}

void RemoveCountdownTimer() // 087 REMOVE_COUNTDOWN_TIMER
{
	ScriptCountdown().Remove();
}

void GetCountdownTimer() // 092 GET_COUNTDOWN_TIMER
{
	// The whole seconds left, also after the timer stops or when it never started
	Pushf(ScriptCountdown().GetSecondsLeft());
}

void CountdownTimerExists() // 099 COUNTDOWN_TIMER_EXISTS
{
	// The timer runs: it started and did not get to zero, and no script removed it
	Pushb(ScriptCountdown().IsRunning());
}

void HideCountdownTimer() // 103 HIDE_COUNTDOWN_TIMER
{
	// Only the flag changes: the game does not draw the timer
	ScriptCountdown().Hide();
}

void GetMoonPercentage() // 108 GET_MOON_PERCENTAGE
{
	// How full the moon was the last time it showed: 0 at the full moon, 1 at a new moon
	const auto* moon =
	    Locator::entitiesRegistry::value().TryGet<ecs::components::Moon>(Locator::skyClockSystem::value().GetEntities().moon);
	Pushf(graphics::moon::ScriptPercentage(moon != nullptr ? moon->phase : 0.0f));
}

void SetGameTime() // 112 SET_GAME_TIME
{
	const auto time = Popf();
	Locator::skyClockSystem::value().SetTime(time);
}

void GetGameTime() // 113 GET_GAME_TIME
{
	Pushf(Locator::skyClockSystem::value().GetClock().GetScriptTime());
}

/// The local time of the player's computer now. In a game for more than one player, the game tells the script that
/// this is not safe there, and then gives the value.
static std::tm RealLocalTime()
{
	if (Locator::time::has_value() && Locator::time::value().IsNetworkGame())
	{
		ScriptMessage(script::real_time::k_NotMultiplayerFriendly);
	}
	return machine_clock::LocalTime();
}

void GetRealTime() // 114 GET_REAL_TIME
{
	Pushf(script::real_time::TimeOfDay(RealLocalTime()));
}

void GetRealDay115() // 115 GET_REAL_DAY
{
	Pushf(script::real_time::DayOfMonth(RealLocalTime()));
}

void GetRealDay116() // 116 GET_REAL_DAY
{
	// The second native of this name gives the day of the week
	Pushf(script::real_time::DayOfWeek(RealLocalTime()));
}

void GetRealMonth() // 117 GET_REAL_MONTH
{
	Pushf(script::real_time::Month(RealLocalTime()));
}

void GetRealYear() // 118 GET_REAL_YEAR
{
	Pushf(script::real_time::Year(RealLocalTime()));
}

void StartGameSpeed() // 128 START_GAME_SPEED
{
	Locator::scriptControlSystem::value().StartGameSpeed(CurrentTask());
}

void EndGameSpeed() // 129 END_GAME_SPEED
{
	if (Locator::scriptControlSystem::value().EndGameSpeed(CurrentTask()))
	{
		Locator::time::value().SetSpeed(1.0f);
	}
}

void RevealCountdownTimer() // 144 REVEAL_COUNTDOWN_TIMER
{
	// Only the flag changes: the game does not draw the timer
	ScriptCountdown().Reveal();
}

/// The game's turn now, by which the scripts' timers count
static uint32_t TimerTurn()
{
	return Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
}

/// The timer a script names, none for anything else (which the script is told of)
static ecs::components::ScriptTimer* ScriptTimerOf(entt::entity object)
{
	auto* timer = Locator::entitiesRegistry::value().TryGet<ecs::components::ScriptTimer>(object);
	if (timer == nullptr && !Locator::entitiesRegistry::value().AllOf<ecs::components::SpellDispenser>(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Invalid script thing");
	}
	return timer;
}

void SetTimerTime() // 145 SET_TIMER_TIME
{
	const auto seconds = Popf();
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ObjectNoLongerValid();
		return;
	}
	if (auto* timer = ScriptTimerOf(object); timer != nullptr)
	{
		// It starts again from now
		timer->timer = script::timers::Set(TimerTurn(), seconds);
		return;
	}
	if (registry.AllOf<ecs::components::SpellDispenser>(object))
	{
		// The turns from the time that the hand takes its bubble to its next bubble
		if (const auto turns = script::timers::DispenserTurns(seconds); turns.has_value())
		{
			Locator::magicSystem::value().SetDispenserTurns(object, *turns);
		}
		return;
	}
}

void CreateTimer() // 146 CREATE_TIMER
{
	const auto seconds = Popf();
	auto& registry = Locator::entitiesRegistry::value();
	const auto timer = registry.Create();
	registry.Assign<ecs::components::ScriptTimer>(timer, script::timers::Set(TimerTurn(), seconds));
	// The script controls it from its first reference
	RegisterCreated(timer);
	PushObject(timer);
}

void GetTimerTimeRemaining() // 147 GET_TIMER_TIME_REMAINING
{
	const auto object = PopObject();
	if (object == entt::null || !Locator::entitiesRegistry::value().Valid(object))
	{
		ObjectNoLongerValid();
		Pushf(0.0f);
		return;
	}
	const auto* timer = ScriptTimerOf(object);
	Pushf(timer != nullptr ? script::timers::SecondsRemaining(timer->timer, TimerTurn()) : 0.0f);
}

void GetTimerTimeSinceSet() // 148 GET_TIMER_TIME_SINCE_SET
{
	const auto object = PopObject();
	if (object == entt::null || !Locator::entitiesRegistry::value().Valid(object))
	{
		ObjectNoLongerValid();
		Pushf(std::numeric_limits<float>::max());
		return;
	}
	// Anything but a timer has been set for ever
	const auto* timer = ScriptTimerOf(object);
	Pushf(timer != nullptr ? script::timers::SecondsSinceSet(timer->timer, TimerTurn()) : std::numeric_limits<float>::max());
}

void GetLandHeight() // 151 GET_LAND_HEIGHT
{
	const auto position = PopVec();
	Pushf(Locator::terrainSystem::value().GetScriptLandHeight(position));
}

void LoadMap() // 152 LOAD_MAP
{
	// The story moves on to its next land: the land is laid out afresh at once, while the scripts go on running, the
	// one that asked included
	const auto path = PopString();
	const auto& fileSystem = Locator::filesystem::value();
	try
	{
		Locator::landSystem::value().LoadMap(fileSystem.FindPath(filesystem::FileSystemInterface::FixPath(path)),
		                                     loading::LoadingClock::Mode::PleaseWait, land_change::Kind::Story);
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Could not load the map {}: {}", path, e.what());
	}
}

void StopAllScriptsExcluding() // 153 STOP_ALL_SCRIPTS_EXCLUDING
{
	const auto scriptNames = PopString();

	auto& lhvm = Locator::vm::value();
	lhvm.StopScripts([&scriptNames](const std::string& name, [[maybe_unused]] const std::string& filename) -> bool {
		return !script::name_lists::HoldsScript(scriptNames, name);
	});
}

void StopAllScriptsInFilesExcluding() // 154 STOP_ALL_SCRIPTS_IN_FILES_EXCLUDING
{
	const auto sourceFilenames = PopString();

	auto& lhvm = Locator::vm::value();
	lhvm.StopScripts([&sourceFilenames]([[maybe_unused]] const std::string& name, const std::string& filename) -> bool {
		return !script::name_lists::HoldsFile(sourceFilenames, filename);
	});
}

void StopScript() // 155 STOP_SCRIPT
{
	const auto scriptName = PopString();
	auto& lhvm = Locator::vm::value();
	// It may name several, as "ScriptA, ScriptB"
	lhvm.StopScripts([&scriptName](const std::string& name, [[maybe_unused]] const std::string& filename) -> bool {
		return script::name_lists::HoldsScript(scriptName, name);
	});
}

void StopScriptsInFiles() // 172 STOP_SCRIPTS_IN_FILES
{
	const auto sourceFilenames = PopString();

	auto& lhvm = Locator::vm::value();
	lhvm.StopScripts([&sourceFilenames]([[maybe_unused]] const std::string& name, const std::string& filename) -> bool {
		return script::name_lists::HoldsFile(sourceFilenames, filename);
	});
}

void SetAnimationModify() // 202 SET_ANIMATION_MODIFY
{
	// The creature comes off the stack first. Only the value 1 sets real time; all other values clear it.
	const auto thing = PopObject();
	const bool realTime = Pop().uintVal == 1;
	const auto creature = CreatureForNative(thing);
	if (!creature.has_value())
	{
		return;
	}
	if (auto* animation = Locator::entitiesRegistry::value().TryGet<ecs::components::CreatureAnimation>(*creature))
	{
		animation->realTime = realTime;
	}
}

void SetAviSequence() // 203 SET_AVI_SEQUENCE
{
	// Sequence 1 is the story's intro, 2 the falling spell's film
	const auto sequence = Pop().intVal;
	const auto enable = static_cast<bool>(Pop().intVal);
	auto& videos = Locator::videoSystem::value();
	auto& director = Locator::cinematicDirectorSystem::value();
	if (!enable)
	{
		if (sequence == 2)
		{
			videos.EndFallingSpell();
		}
		return;
	}
	if (sequence == 1)
	{
		// The intro is cut short: it fades from 58 s and ends at 60 s. The script's fade is lifted at once under it
		videos.Play(ecs::systems::k_IntroFilm);
		videos.ScheduleIntro();
		director.FadeBackToNormal(0);
	}
	else if (sequence == 2)
	{
		videos.StartFallingSpell();
		director.FadeBackToNormal(0);
	}
}

/// The developer function the GameOver script ends with
constexpr int32_t k_DevFunctionEnterSaveGameRoom = 12;

void DevFunction() // 205 DEV_FUNCTION
{
	const auto func = Pop().intVal;
	// Once the game is over the player is taken into the temple's save game room, and kept there
	if (func == k_DevFunctionEnterSaveGameRoom)
	{
		if (Locator::temple::has_value())
		{
			Locator::temple::value().EnterLockedRoom(TempleRoom::SaveGame);
		}
		return;
	}
	// Each lesson of the Gods' Playground tells the game that it runs, so that Escape ends it
	if (func == playground_trip::k_LessonStartsFunction)
	{
		if (Locator::playgroundTripSystem::has_value())
		{
			Locator::playgroundTripSystem::value().StartLesson();
		}
		return;
	}
	// The functions the tutorials use on the first player's creature: starting it growing up again kept at home, and
	// granting it the learning leash, then the aggression and compassion leashes
	if (!Locator::leashSystem::has_value())
	{
		return;
	}
	auto& leashes = Locator::leashSystem::value();
	const auto creature = leashes.PlayersCreature(PlayerNames::PLAYER_ONE);
	if (!creature.has_value())
	{
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "DEV_FUNCTION({}): the player has no creature they can lead", func);
		return;
	}
	switch (func)
	{
	case 1:
		MoveCreatureToStage(*creature, 0);
		leashes.ConfineToHome(*creature, creature_leash::k_HomeConfinement);
		break;
	case 2:
		leashes.SetKnown(*creature, LeashType::Rope, true);
		break;
	case 3:
		leashes.SetKnown(*creature, LeashType::Good, true);
		leashes.SetKnown(*creature, LeashType::Evil, true);
		break;
	case 6:
		// It points out the lesson highlight about it
		if (Locator::creatureMindSystem::has_value())
		{
			Locator::creatureMindSystem::value().PointOutHighlight(*creature);
		}
		break;
	case 7:
		// How it was last rewarded or punished is forgotten, so that a script can wait for the next time
		if (Locator::creatureMindSystem::has_value())
		{
			Locator::creatureMindSystem::value().ClearInteractionMagnitude(*creature);
		}
		break;
	case 8:
	case 9:
		// Whether the creature can die: a miracle that takes the last of its life knocks it out, or gives it all back
		if (auto* body = Locator::entitiesRegistry::value().TryGet<ecs::components::Creature>(*creature))
		{
			body->canDie = func == 8;
		}
		break;
	case 11:
		// The help forgets every message about the creature, waiting or told
		if (auto* help = Locator::entitiesRegistry::value().TryGet<ecs::components::CreatureHelpMemory>(*creature))
		{
			creature_help::ForgetMessages(help->memory);
		}
		break;
	default:
		// TODO(Daniels118): implement the other functions
		NotImplemented(func);
		break;
	}
}

void GetArena() // 211 GET_ARENA
{
	// The nearest arena closer than the distance to the point, or else a new one there for the two creatures, as a fight
	// makes one: sized for the bigger of them, with room clear for the first
	const auto second = PopObject();
	const auto first = PopObject();
	const auto distance = Popf();
	const auto position = PopVec();
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(first) || !registry.Valid(second))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GET_ARENA: creature for arena not found!");
	}
	else if (Locator::creatureFightSystem::has_value())
	{
		// A thing that is not a creature gives a message, but the native still finds an arena near the point for it
		for (const auto thing : {first, second})
		{
			if (!registry.AllOf<ecs::components::Creature>(thing))
			{
				SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GET_ARENA: Thing not creature!");
			}
		}
		if (const auto found = Locator::creatureFightSystem::value().FindOrMakeArena(position, first, second, distance))
		{
			if (Locator::scriptObjects::has_value())
			{
				Locator::scriptObjects::value().Register(found->arena, found->made);
			}
			PushObject(found->arena);
			return;
		}
	}
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GET_ARENA: Arena not found or created!");
	Pusho(0);
}

/// The football pitch a native is given. A thing that is not a pitch gives a message, and the native does nothing.
entt::entity PitchOfScript(entt::entity thing)
{
	if (!Locator::footballSystem::has_value() || !Locator::footballSystem::value().IsPitch(thing))
	{
		ScriptMessage("Must be footy.");
		return entt::null;
	}
	return thing;
}

/// The villager a native is given for football. A thing that is not a villager gives a message.
bool IsFootballVillager(entt::entity thing)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(thing) || !registry.AllOf<ecs::components::Villager>(thing))
	{
		ScriptMessage("Must be villager.");
		return false;
	}
	return true;
}

void GetFootballPitch() // 212 GET_FOOTBALL_PITCH
{
	// The pitch that the town uses, or none with a message
	const auto town = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(town))
	{
		ScriptMessage("Object no longer valid.");
	}
	else
	{
		const bool isTown = registry.AllOf<ecs::components::Town>(town);
		if (!isTown)
		{
			ScriptMessage("Must look in a town.");
		}
		const auto pitch =
		    isTown && Locator::footballSystem::has_value() ? Locator::footballSystem::value().TownPitch(town) : entt::null;
		if (pitch != entt::null)
		{
			if (Locator::scriptObjects::has_value())
			{
				Locator::scriptObjects::value().Register(pitch, false);
			}
			PushObject(pitch);
			return;
		}
	}
	ScriptMessage("No pitch.");
	Pusho(0);
}

void StopAllGames() // 213 STOP_ALL_GAMES
{
	// The match of the pitch stops, as when its town stops it
	const auto thing = PopObject();
	if (!Locator::entitiesRegistry::value().Valid(thing))
	{
		ScriptMessage("Object no longer valid.");
		return;
	}
	if (const auto pitch = PitchOfScript(thing); pitch != entt::null)
	{
		Locator::footballSystem::value().StopAllGames(pitch);
	}
}

void AttachToGame() // 214 ATTACH_TO_GAME
{
	// The team, the football pitch and the villager joining it; the native takes control of both objects
	const auto team = Pop().intVal;
	const auto thing = PopObject();
	const auto villager = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(thing) || !registry.Valid(villager))
	{
		ScriptMessage("Object no longer valid.");
		return;
	}
	const auto pitch = PitchOfScript(thing);
	if (IsFootballVillager(villager) && pitch != entt::null &&
	    !Locator::footballSystem::value().AttachToGame(pitch, villager, team != 0))
	{
		ScriptMessage("Cannot add to team.");
	}
}

void DetachFromGame() // 215 DETACH_FROM_GAME
{
	// A value the game ignores, the football pitch and the villager leaving it; the native takes control of both objects
	[[maybe_unused]] const auto unused = Pop().intVal;
	const auto thing = PopObject();
	const auto villager = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(thing) || !registry.Valid(villager))
	{
		ScriptMessage("Object no longer valid.");
		return;
	}
	const auto pitch = PitchOfScript(thing);
	if (IsFootballVillager(villager) && pitch != entt::null &&
	    !Locator::footballSystem::value().DetachFromGame(pitch, villager))
	{
		ScriptMessage("Cannot detach.");
	}
}

void DetachUndefinedFromGame() // 216 DETACH_UNDEFINED_FROM_GAME
{
	// The team and the football pitch, of which the native takes control. The first player of the team leaves it.
	const auto team = Pop().intVal;
	const auto thing = PopObject();
	if (!Locator::entitiesRegistry::value().Valid(thing))
	{
		ScriptMessage("Object no longer valid.");
		return;
	}
	if (const auto pitch = PitchOfScript(thing);
	    pitch != entt::null && !Locator::footballSystem::value().DetachFirstPlayer(pitch, team != 0))
	{
		ScriptMessage("Cannot detach.");
	}
}

void SetOnlyForScripts() // 217 SET_ONLY_FOR_SCRIPTS
{
	// The football pitch, of which the native takes control, then whether only the scripts play on it
	const auto thing = PopObject();
	const auto onlyForScripts = Pop().intVal;
	if (!Locator::entitiesRegistry::value().Valid(thing))
	{
		ScriptMessage("Object no longer valid.");
		return;
	}
	if (const auto pitch = PitchOfScript(thing); pitch != entt::null)
	{
		Locator::footballSystem::value().SetOnlyForScripts(pitch, onlyForScripts != 0);
	}
}

void StartMatchWithReferee() // 218 START_MATCH_WITH_REFEREE
{
	// The villager refereeing and the football pitch; the native takes control of both
	const auto referee = PopObject();
	const auto thing = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(referee) || !registry.Valid(thing))
	{
		ScriptMessage("Object no longer valid.");
		return;
	}
	const auto pitch = PitchOfScript(thing);
	if (IsFootballVillager(referee) && pitch != entt::null &&
	    !Locator::footballSystem::value().StartWithReferee(pitch, referee))
	{
		ScriptMessage("Cannot start game with ref.");
	}
}

void GameTeamSize() // 219 GAME_TEAM_SIZE
{
	// The team and the football pitch. The game gives the size of the team as a number, although the function table
	// says that the native gives nothing.
	const auto team = Pop().intVal;
	const auto thing = PopObject();
	if (!Locator::entitiesRegistry::value().Valid(thing))
	{
		ScriptMessage("Object no longer valid.");
		Pushf(0.0f);
		return;
	}
	const auto pitch = PitchOfScript(thing);
	Pushf(pitch != entt::null ? static_cast<float>(Locator::footballSystem::value().TeamSize(pitch, team != 0)) : 0.0f);
}

void GameType() // 220 GAME_TYPE
{
	// The script type of the thing. A thing that has no script type gives 0.
	const auto object = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ObjectNoLongerValid();
		Pushi(0);
		return;
	}
	Pushi(static_cast<int32_t>(ecs::script_find::GameTypeOf(registry, object)));
}

void GameSubType() // 221 GAME_SUB_TYPE
{
	// The row of the thing's kind in its type's table, as a find or an is-of-type test matches it: a creature's species,
	// a vortex's kind. A thing whose kind the game can't tell gives a number no kind has
	const auto object = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ObjectNoLongerValid();
		Pushi(0);
		return;
	}
	const auto kind = ecs::script_find::KindOf(registry, object);
	if (!kind.has_value() || !kind->subtype.has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Unknown type for search");
		Pushi(static_cast<int32_t>(ecs::script_find::k_NoSubtype));
		return;
	}
	Pushi(static_cast<int32_t>(*kind->subtype));
}

/// The help profile's count of an event the script asks about, none (with an error) for a number it may not ask about
help::profile::EventCount* ScriptHelpEvent()
{
	const auto event = Pop().intVal;
	if (!help::profile::HelpProfile::ScriptMayAsk(event))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Invalid event");
		return nullptr;
	}
	return &Locator::helpProfileSystem::value().Get().Count(static_cast<uint32_t>(event));
}

void GetEventsPerSecond() // 235 GET_EVENTS_PER_SECOND
{
	auto* count = ScriptHelpEvent();
	Pushf(count != nullptr ? count->PerSecond(Locator::helpProfileSystem::value().Get().Clock()) : 0.0f);
}

void GetTimeSince() // 236 GET_TIME_SINCE
{
	auto* count = ScriptHelpEvent();
	Pushf(count != nullptr ? count->SecondsSince(Locator::helpProfileSystem::value().Get().Clock()) : 0.0f);
}

void GetTotalEvents() // 237 GET_TOTAL_EVENTS
{
	const auto* count = ScriptHelpEvent();
	Pushf(count != nullptr ? static_cast<float>(count->Total()) : 0.0f);
}

void PlayedPercentage() // 260 PLAYED_PERCENTAGE
{
	// How far a creature's body is through what it plays, 0 to 1: the time of its action over the length of the
	// animation that the game keeps for it. Anything else gives 1.
	const auto object = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		ObjectNoLongerValid();
		Pushf(1.0f);
		return;
	}
	if (!registry.AllOf<ecs::components::Creature>(object))
	{
		ScriptMessage("Not coded for non creature");
		Pushf(1.0f);
		return;
	}
	const auto share =
	    Locator::creatureMindSystem::has_value() ? Locator::creatureMindSystem::value().BodyShare(object) : std::nullopt;
	Pushf(share.value_or(0.0f));
}

void GetInteractionMagnitude() // 270 GET_INTERACTION_MAGNITUDE
{
	// How strongly the player last rewarded (above 0) or punished (below 0) the creature
	const auto creature = CreatureForNative(PopObject());
	Pushf(creature.has_value() && Locator::creatureMindSystem::has_value()
	          ? Locator::creatureMindSystem::value().GetInteractionMagnitude(*creature)
	          : 0.0f);
}

void GameTimeOnOff() // 288 GAME_TIME_ON_OFF
{
	const auto enable = Pop().intVal != 0;
	Locator::skyClockSystem::value().GetClock().SetRunning(enable);
}

void MoveGameTime() // 289 MOVE_GAME_TIME
{
	const auto duration = Popf();
	const auto hourOfTheDay = Popf();
	Locator::skyClockSystem::value().GetClock().MoveScriptTime(hourOfTheDay, duration);
}

void SetHighGraphicsDetail() // 290 SET_HIGH_GRAPHICS_DETAIL
{
	const auto object = PopObject();
	const auto enable = Pop().intVal != 0;
	if (object == entt::null || !Locator::entitiesRegistry::value().Valid(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Thing not found");
		return;
	}
	// The thing is drawn in high detail for the script's cinema, or as usual again
	auto& highDetail = Locator::highDetailSystem::value();
	if (enable)
	{
		highDetail.Make(object);
	}
	else
	{
		highDetail.Release(object);
	}
	RemakeDrawList();
}

void ImmersionExists() // 304 IMMERSION_EXISTS
{
	// openblack drives no force-feedback mouse, so there is never a device for the scripts' effects
	Pushb(false);
}

void IsLockedInteraction() // 342 IS_LOCKED_INTERACTION
{
	// Whether the player's hand is locked onto the thing: held to a creature with the button, gripping a totem, or
	// scooping from a field, a fish farm or a pile
	const auto object = PopObject();
	if (object == entt::null || !Locator::entitiesRegistry::value().Valid(object))
	{
		ObjectNoLongerValid();
		Pushb(false);
		return;
	}
	const bool heldCreature = Locator::creatureHandSystem::has_value() && Locator::creatureHandSystem::value().IsLocked(object);
	const bool grippedTotem =
	    Locator::villageTotemSystem::has_value() && Locator::villageTotemSystem::value().GetGripped() == object;
	// A puzzle totem that a hand holds, as the turn last saw it
	const auto* puzzleTotem = Locator::entitiesRegistry::value().TryGet<const ecs::components::PuzzleTotem>(object);
	const bool heldPuzzleTotem = puzzleTotem != nullptr && puzzleTotem->held;
	const auto player =
	    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
	const bool scooped =
	    Locator::handGrabSystem::has_value() && Locator::handGrabSystem::value().GetLockedOnOf(player) == std::optional(object);
	Pushb(heldCreature || grippedTotem || heldPuzzleTotem || scooped);
}

void EnterExitCitadel() // 347 ENTER_EXIT_CITADEL
{
	const auto enter = static_cast<bool>(Pop().intVal);
	if (Locator::temple::has_value())
	{
		auto& temple = Locator::temple::value();
		const auto active = temple.Active();
		if (enter != active)
		{
			if (enter)
			{
				temple.Activate();
			}
			else
			{
				temple.Deactivate();
			}
		}
	}
	else
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "No temple");
	}
}

void StopScriptsInFilesExcluding() // 352 STOP_SCRIPTS_IN_FILES_EXCLUDING
{
	const auto scriptNames = PopString();
	const auto sourceFilenames = PopString();

	auto& lhvm = Locator::vm::value();
	lhvm.StopScripts([&scriptNames, &sourceFilenames](const std::string& name, const std::string& filename) -> bool {
		return script::name_lists::HoldsFile(sourceFilenames, filename) && !script::name_lists::HoldsScript(scriptNames, name);
	});
}

void SetBookmarkPosition() // 362 SET_BOOKMARK_POSITION
{
	// The game takes the four values and does nothing with them. The bookmarks do not change.
	for (int i = 0; i < 4; ++i)
	{
		static_cast<void>(Pop());
	}
}

void SetScaffoldProperties() // 363 SET_SCAFFOLD_PROPERTIES
{
	const bool destroys = (Pop().intVal & 1) != 0;
	const auto size = Popf();
	const auto building = Pop().intVal;
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		ObjectNoLongerValid();
		return;
	}
	auto* scaffold = registry.TryGet<ecs::components::Scaffold>(object);
	if (scaffold == nullptr)
	{
		ScriptMessage("Thing must be scaffold");
		return;
	}
	// The scaffold offers only a building of the abode number, and it becomes worth the size
	scaffold->scriptBuilding = ecs::workshop::ScriptBuildingOf(building);
	registry.Get<Transform>(object).scale = glm::vec3(ecs::workshop::ScaleOfScriptSize(size));
	scaffold->destroysThingsInWay = destroys;
	registry.SetDirty();
}

void InsideTemple() // 405 INSIDE_TEMPLE
{
	Pushb(PlayerInsideTemple());
}

void SetGameTimeProperties() // 407 SET_GAME_TIME_PROPERTIES
{
	const auto percentageChange = Popf();
	const auto percentageNight = Popf();
	const auto duration = Popf();
	Locator::skyClockSystem::value().SetCycle({.duration = duration, .night = percentageNight, .change = percentageChange});
}

void ResetGameTimeProperties() // 408 RESET_GAME_TIME_PROPERTIES
{
	Locator::skyClockSystem::value().SetCycle({
	    .duration = DayNightClock::k_DefaultDuration,
	    .night = DayNightClock::k_DefaultNight,
	    .change = DayNightClock::k_DefaultChange,
	});
}

void SetInterfaceCitadel() // 414 SET_INTERFACE_CITADEL
{
	// Whether tapping the temple's entrance takes the player inside
	Locator::entitiesRegistry::value().Context().scriptLetsTempleBeEntered = Pop().intVal != 0;
}

void MapScriptFunction() // 415 MAP_SCRIPT_FUNCTION
{
	// A line of a land's script, carried out as the land's script would
	const auto line = PopString();
	try
	{
		lhscriptx::Script script;
		script.Load(line);
	}
	catch (const std::runtime_error& error)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Map script line \"{}\" failed: {}", line, error.what());
	}
}

void WithinRotation() // 416 WITHIN_ROTATION
{
	// The view report of the player tells which camera icons the hand showed or offered
	const auto view = ScriptView();
	Pushb(view.has_value() && script::lesson_natives::WithinRotation(view->cameraIcons));
}

void StartImmersion() // 429 START_IMMERSION
{
	// A force-feedback effect, which does nothing without the device (see IMMERSION_EXISTS)
	Pop();
}

void StopImmersion() // 430 STOP_IMMERSION
{
	// Stopping a force-feedback effect does nothing without the device either
	Pop();
}

void StopAllImmersion() // 431 STOP_ALL_IMMERSION
{
	// Stopping every force-feedback effect does nothing without the device either
}

void SaveGameInSlot() // 439 SAVE_GAME_IN_SLOT
{
	// The game takes the whole part of the number as the slot, and saves into it as it does by itself. The game does
	// not check the slot; openblack does not save into a slot that is not there.
	const auto number = Popf();
	if (!(number > -1.0f && number < static_cast<float>(openblack::gamesave::k_SlotCount)) ||
	    !Locator::saveGameSystem::has_value())
	{
		return;
	}
	Locator::saveGameSystem::value().InstantSave(static_cast<uint32_t>(static_cast<int32_t>(number)));
}

void CanSkipTutorial() // 460 CAN_SKIP_TUTORIAL
{
	Pushb(Locator::tutorialSkipSystem::value().Get().skipTutorial);
}

void CanSkipCreatureTraining() // 461 CAN_SKIP_CREATURE_TRAINING
{
	Pushb(Locator::tutorialSkipSystem::value().Get().skipCreatureTraining);
}

void IsKeepingOldCreature() // 462 IS_KEEPING_OLD_CREATURE
{
	Pushb(Locator::tutorialSkipSystem::value().Get().keepOldCreature);
}

void CurrentProfileHasCreature() // 463 CURRENT_PROFILE_HAS_CREATURE
{
	// The debug quickstart's Cow stands in for a kept creature
	Pushb(Locator::tutorialSkipSystem::value().Get().startWithCow ||
	      Locator::playerProfileSystem::value().CurrentProfileHasCreature());
}

} // namespace openblack::chlapi
