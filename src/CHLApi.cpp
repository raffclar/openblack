/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CHLApi.h"

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
#include "3D/MapCoords.h"
#include "3D/Mists.h"
#include "3D/ScreenPick.h"
#include "3D/TempleInteriorInterface.h"
#include "Animals/AnimalMove.h"
#include "Blast/CameraShake.h"
#include "Camera/Camera.h"
#include "Camera/ForcedTurnPoint.h"
#include "Camera/ScriptCameraModel.h"
#include "ECS/GamePlayers.h"
#include "Sound/GameMusic.h"
#include "Sound/GameSoundEffects.h"
#include "Sound/ScriptSoundEffect.h"
#include "Sound/SoundId.h"
#include "Sound/SoundRequestInterface.h"
#if defined(OPENBLACK_DEVELOPER_TOOLS)
#include "Common/DrawTrace.h"
#endif
#include "CHLApi/NativeHelpers.h"
#include "CHLApi/Natives.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "Common/MachineClock.h"
#include "Common/PortableRotations.h"
#include "Creature/CreatureActivity.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureDevelopment.h"
#include "Creature/CreatureHandGlow.h"
#include "Creature/CreatureRoute.h"
#include "Creature/CreatureScriptAgendas.h"
#include "Creature/CreatureScriptPlay.h"
#include "Creature/CreatureSwap.h"
#include "Creature/LeashRules.h"
#include "Creature/TemplePen.h"
#include "ECS/AllyInfluence.h"
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
#include "ECS/ChallengePictures.h"
#include "ECS/ChallengeRecords.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/ChallengePicture.h"
#include "ECS/Components/ChallengeRecord.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureFriends.h"
#include "ECS/Components/CreatureHelp.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/GroundPrint.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/HandClicked.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/HighDetail.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/Influence.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingListPlace.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/MapCellResident.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MiracleImpression.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/NeverFades.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/PuzzleGame.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/ScriptRemind.h"
#include "ECS/Components/ScriptTimer.h"
#include "ECS/Components/Shark.h"
#include "ECS/Components/Sky.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownAggression.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerDeath.h"
#include "ECS/Components/VillagerPose.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipChants.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/CreatureCarried.h"
#include "ECS/CreatureFromFile.h"
#include "ECS/CreatureNaming.h"
#include "ECS/CreatureReactionNotice.h"
#include "ECS/CreatureRemoval.h"
#include "ECS/CreatureScars.h"
#include "ECS/DanceRules.h"
#include "ECS/Dances.h"
#include "ECS/DiscipleRules.h"
#include "ECS/FollowHeadingThing.h"
#include "ECS/Map.h"
#include "ECS/MazePuzzleRules.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/PlacedTurn.h"
#include "ECS/PlayerTowns.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Registry.h"
#include "ECS/ReportedCamera.h"
#include "ECS/ScriptContainerWalk.h"
#include "ECS/ScriptCreatureCounts.h"
#include "ECS/ScriptDelete.h"
#include "ECS/ScriptDestination.h"
#include "ECS/ScriptFind.h"
#include "ECS/ScriptFlocks.h"
#include "ECS/ScriptObject.h"
#include "ECS/ScriptPoison.h"
#include "ECS/ScriptPopulate.h"
#include "ECS/ScriptSpotVisuals.h"
#include "ECS/ScriptWeatherRules.h"
#include "ECS/SpellCreator.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/AnimatedStaticSystemInterface.h"
#include "ECS/Systems/BoatLaunchSystemInterface.h"
#include "ECS/Systems/CameraBookmarkSystemInterface.h"
#include "ECS/Systems/CameraHelpSystemInterface.h"
#include "ECS/Systems/CameraZoneSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/ComputerPlayerSystemInterface.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureAudioSystemInterface.h"
#include "ECS/Systems/CreatureCarryOverSystemInterface.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureFizzSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/DialogueControlSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/EffectRequestSystemInterface.h"
#include "ECS/Systems/ExplosionSystemInterface.h"
#include "ECS/Systems/FightTranscriptSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/FootballSystemInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/HandDemoSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HelpMessageSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/HelpSpeechSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/HighDetailSystemInterface.h"
#include "ECS/Systems/Implementations/ObjectMeasures.h"
#include "ECS/Systems/Implementations/VillagerDance.h"
#include "ECS/Systems/Implementations/VillagerDisciples.h"
#include "ECS/Systems/Implementations/VillagerScript.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/IntroSystemInterface.h"
#include "ECS/Systems/LandSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/MagicShieldSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/MistSystemInterface.h"
#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/PlaygroundTripSystemInterface.h"
#include "ECS/Systems/PuzzleGameSystemInterface.h"
#include "ECS/Systems/PuzzleTotemSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/Systems/RewardSystemInterface.h"
#include "ECS/Systems/SaveGameSystemInterface.h"
#include "ECS/Systems/ScriptControlSystemInterface.h"
#include "ECS/Systems/ScriptHighlightSystemInterface.h"
#include "ECS/Systems/ScriptMusicSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/Systems/SkyClockSystemInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/SoundTagSystemInterface.h"
#include "ECS/Systems/SpellIconSystemInterface.h"
#include "ECS/Systems/TempleDestructionSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/Systems/TurnCheerSystemInterface.h"
#include "ECS/Systems/TutorialSkipSystemInterface.h"
#include "ECS/Systems/VideoSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/Systems/VortexSystemInterface.h"
#include "ECS/Systems/WalkPathSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/Systems/WorkshopSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/TempleConstruction.h"
#include "ECS/ThingMapPlace.h"
#include "ECS/ThingOnScreen.h"
#include "ECS/TownDesire.h"
#include "ECS/TownHomes.h"
#include "ECS/TownPlanning.h"
#include "ECS/TownPlaythings.h"
#include "ECS/VillagerAfterDeath.h"
#include "ECS/VillagerAge.h"
#include "ECS/VillagerScriptRules.h"
#include "ECS/VillagerSkeletons.h"
#include "ECS/WalkerPlacement.h"
#include "ECS/WallHugRules.h"
#include "ECS/WorkshopRules.h"
#include "ECS/WorldObjects.h"
#include "Enums.h"
#include "FileSystem/FileSystemInterface.h"
#include "Gui/ScriptTexts.h"
#include "Hand/HandClickRules.h"
#include "Hand/HandGrabRules.h"
#include "Hand/ScriptHandState.h"
#include "Help/DialogueText.h"
#include "Help/HelpProfile.h"
#include "Help/ScriptHelpText.h"
#include "Help/ScriptSpirits.h"
#include "Help/Spirits.h"
#include "InfoConstants.h"
#include "Input/GameActionMapInterface.h"
#include "LHScriptX/FeatureScriptCommands.h"
#include "LHScriptX/Script.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Magic/ScriptCast.h"
#include "Magic/TownBelief.h"
#include "MainThreadWork.h"
#include "Physics/Body.h"
#include "Resources/ResourcesInterface.h"
#include "ScriptHeaders/ScriptCameraLookups.h"
#include "ScriptHeaders/ScriptCastHistory.h"
#include "ScriptHeaders/ScriptChallengeSnapshots.h"
#include "ScriptHeaders/ScriptCreateRules.h"
#include "ScriptHeaders/ScriptCreatureRules.h"
#include "ScriptHeaders/ScriptDeadLiving.h"
#include "ScriptHeaders/ScriptEnums.h"
#include "ScriptHeaders/ScriptInfluence.h"
#include "ScriptHeaders/ScriptInterfaceLevels.h"
#include "ScriptHeaders/ScriptLessonNatives.h"
#include "ScriptHeaders/ScriptNameLists.h"
#include "ScriptHeaders/ScriptPlayers.h"
#include "ScriptHeaders/ScriptPropertyRules.h"
#include "ScriptHeaders/ScriptRandom.h"
#include "ScriptHeaders/ScriptRealTime.h"
#include "ScriptHeaders/ScriptStart.h"
#include "ScriptHeaders/ScriptSwitchChanges.h"
#include "ScriptHeaders/ScriptTownHealth.h"
#include "Story/PlaygroundTrip.h"
#include "Temple/TempleEntrancePoint.h"
#include "Visual/RendererRequestsInterface.h"
#include "Windowing/WindowingInterface.h"

namespace openblack::chlapi
{

using namespace openblack::ecs::archetypes;

ecs::components::Player* ScriptPlayer(float number);

/// Whether a player that a script names is a player of this game. The players of a land are the 8 of the game. A
/// network game with more gods has the gods after the seventh too: the script numbers 9 to 33 name them (deviation 165).
[[nodiscard]] static bool ScriptNamesPlayer(PlayerNames player)
{
	return static_cast<size_t>(player) < openblack::ecs::game_players::Count();
}

using openblack::Locator;
using openblack::MobileStaticInfo;
using openblack::ecs::components::CreatureMindState;
using openblack::ecs::components::Transform;
using openblack::ecs::systems::HandSystemInterface;
using openblack::lhvm::DataType;
using openblack::lhvm::VMValue;
using openblack::script::ObjectType;

#define CREATE_FUNCTION_BINDING(NAME, STACKIN, STACKOUT, FUNCTION)       \
	{                                                                    \
		_functionsTable.emplace_back(FUNCTION, STACKIN, STACKOUT, NAME); \
	}

const std::vector<lhvm::NativeFunction>& CHLApi::GetFunctionsTable()
{
	return _functionsTable;
}

std::unordered_set<std::string> GetUniqueWords(const std::string& strings)
{
	std::unordered_set<std::string> result;
	std::istringstream iss(strings);
	std::string word;
	while (std::getline(iss, word, ' '))
	{
		result.insert(word);
	}
	return result;
}

/// The task running the native now, 0 between tasks
uint32_t CurrentTask()
{
	return Locator::vm::value().GetCurrentTaskNumber();
}

/// The kind of script a task runs, none for a task that has gone
lhvm::ScriptType TaskType(uint32_t task)
{
	const auto& tasks = Locator::vm::value().GetTasks();
	const auto found = tasks.find(task);
	return found != tasks.end() ? found->second.type : lhvm::ScriptType::None;
}

/// A script's mistake the game tells of, and carries on
void ScriptMessage(std::string_view message)
{
	if (const auto logger = spdlog::get("scripting"))
	{
		SPDLOG_LOGGER_WARN(logger, "{}", message);
	}
}

/// A native was given a thing that is gone. The game says nothing and the native carries on. A script can ask about a
/// gone thing on each turn, thus the note is only for a trace log.
void ObjectNoLongerValid()
{
	SPDLOG_LOGGER_TRACE(spdlog::get("scripting"), "Object no longer valid");
}

/// Nothing outside is in view while the player is inside the temple
bool PlayerInsideTemple()
{
	return Locator::temple::has_value() && Locator::temple::value().Active();
}

/// The scripts' switch for the leashes: on again once a script gives the camera back, off while it has it
void DrawLeashes(bool drawn)
{
	if (Locator::leashSystem::has_value())
	{
		Locator::leashSystem::value().SetDrawn(drawn);
	}
}

/// The scripts' switches a change of control sets
void ApplySwitchChanges(const script::switches::SwitchChanges& changes)
{
	if (changes.leashesDrawn.has_value())
	{
		DrawLeashes(*changes.leashesDrawn);
	}
	if (changes.highlightsDrawn.has_value() && Locator::chlapi::has_value())
	{
		Locator::chlapi::value().SetHighlightDrawOn(*changes.highlightsDrawn);
	}
	if (changes.otherCreatureVoices.has_value() && Locator::creatureAudioSystem::has_value())
	{
		Locator::creatureAudioSystem::value().SetOtherVoicesEnabled(*changes.otherCreatureVoices);
	}
}

glm::vec3 PopVec()
{
	auto& lhvm = Locator::vm::value();
	const auto z = lhvm.Popf();
	const auto y = lhvm.Popf();
	const auto x = lhvm.Popf();
	return {x, y, z};
}

void PushVec(const glm::vec3& vec)
{
	auto& lhvm = Locator::vm::value();
	lhvm.Pushv(vec.x);
	lhvm.Pushv(vec.y);
	lhvm.Pushv(vec.z);
}

std::string PopString()
{
	auto& lhvm = Locator::vm::value();
	return lhvm.GetString(lhvm.Pop().intVal);
}

/// Whether a thing is one of the world's objects, which a miracle can be cast on, rather than something with only a place,
/// such as a town or a miracle
bool IsScriptObject(const ecs::Registry& registry, entt::entity thing)
{
	return ecs::script_object::IsObject({.model = registry.AnyOf<ecs::components::Mesh>(thing),
	                                     .hand = registry.AnyOf<ecs::components::Hand>(thing),
	                                     .creature = registry.AnyOf<ecs::components::Creature>(thing),
	                                     .field = registry.AnyOf<ecs::components::Field>(thing),
	                                     .globe = registry.AnyOf<ecs::components::OneOffSpellSeed>(thing),
	                                     .seed = registry.AnyOf<ecs::components::SpellSeed>(thing)});
}

/// A football made by a script goes to the nearest town, however far, for its people to play with; a town that already
/// has a football still about keeps that one
entt::entity CreateScriptBall(const glm::vec3& position)
{
	const auto ball = BallArchetype::Create(position);
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<ecs::town_playthings::Candidate> towns;
	registry.Each<const ecs::components::Town, const ecs::components::Transform>(
	    [&towns](entt::entity entity, const ecs::components::Town& town, const ecs::components::Transform& transform) {
		    towns.push_back({.town = entity, .owner = town.owner, .id = town.id, .position = transform.position});
	    });
	const auto nearest = ecs::town_playthings::Nearest(towns, position);
	if (!nearest.has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Ball Created but unable to add it to a town.");
		return ball;
	}
	auto& town = registry.Get<ecs::components::Town>(*nearest);
	ecs::town_playthings::Add(town.playthings, ball, [&registry](entt::entity thing) {
		return registry.Valid(thing) && registry.AllOf<ecs::components::Ball>(thing);
	});
	return ball;
}

/// A villager a script makes: a grown-up just past growing up, or a child of ten, in no town, standing waiting for the
/// script
entt::entity CreateScriptVillager(bool child, uint32_t subtype, const glm::vec3& position)
{
	const auto& infos = Locator::infoConstants::value().villager;
	if (subtype >= infos.size())
	{
		ScriptMessage("Thing not created");
		return entt::null;
	}
	const auto made = script::create_rules::ScriptVillagerOf(child, infos.at(subtype).grownUpAge);
	const auto villager =
	    VillagerArchetype::Create(position, position, static_cast<VillagerInfo>(subtype), made.age,
	                              made.joinsTown ? VillagerArchetype::Joins::NearestTown : VillagerArchetype::Joins::Nobody);
	if (Locator::livingActionSystem::has_value())
	{
		// Then it is set to the script's age, as big as that age and in its models, even when it was made a named
		// villager of another age
		Locator::livingActionSystem::value().VillagerSetAge(villager, made.age);
		Locator::livingActionSystem::value().VillagerSetScriptState(villager, VillagerStates::InScript);
	}
	return villager;
}

/// Makes a thing show only as a print on the land, the newest of the prints
void MarkGroundPrint(entt::entity thing)
{
	auto& registry = Locator::entitiesRegistry::value();
	uint32_t made = 0;
	registry.Each<const ecs::components::GroundPrint>(
	    [&made](const ecs::components::GroundPrint& print) { made = std::max(made, print.made + 1); });
	registry.AssignOrReplace<ecs::components::GroundPrint>(thing, ecs::components::GroundPrint {.made = made});
}

/// The point on the ground under where a script asks for a thing
glm::vec3 OnGround(const glm::vec3& position)
{
	return {position.x, Locator::terrainSystem::value().GetHeightAt(glm::vec2(position.x, position.z)), position.z};
}

/// Whether the info table puts a row of the mobile statics in the rock kind
bool IsRockKindRow(uint32_t subtype)
{
	if (!Locator::infoConstants::has_value())
	{
		return false;
	}
	const auto& rows = Locator::infoConstants::value().mobileStatic;
	return script::create_rules::IsRow(subtype, rows.size()) && rows.at(subtype).mobileType == MobileStaticInfo::Rock;
}

/// A rock that a script made starts in the physics at rest. The physics puts it on the land when the start asks for
/// it, and then lifts it clear of the things under it. A rock above the land falls in the next turns.
void StartScriptRock(entt::entity rock, const script::create_rules::ScriptRockStart& start)
{
	if (rock == entt::null || !Locator::dynamicsSystem::has_value())
	{
		return;
	}
	auto& dynamics = Locator::dynamicsSystem::value();
	const auto started = dynamics.InitialisePhysics(rock, {.add = true});
	if (started.entry == nullptr)
	{
		return;
	}
	if (start.settlesOnLand)
	{
		dynamics.SettleOnLand(*started.entry, false, true);
	}
	dynamics.RaiseClearOfWhatIsUnder(*started.entry);
}

/// A one-shot miracle globe a script puts straight into the first player's hand, charged in full and ready, if the hand
/// is free to take it: the seed, or none
entt::entity CreateScriptSeedInHand(uint32_t subtype)
{
	const auto player =
	    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
	const auto seedType = static_cast<SpellSeedType>(subtype);
	// The hand takes the miracle only when it is ready for an object
	const bool holdsSomething =
	    (Locator::handGrabSystem::has_value() && Locator::handGrabSystem::value().GetHeldOf(player).has_value()) ||
	    Locator::magicSystem::value().IsHandBusyOf(player);
	const bool holdsCreature =
	    Locator::creatureHandSystem::has_value() && Locator::creatureHandSystem::value().GetLockedCreature(player).has_value();
	if (!hand_grab::ReadyForObject(holdsSomething, holdsCreature))
	{
		return entt::null;
	}
	const auto seed = Locator::magicSystem::value().GiveOneShotSeedToHand(player, seedType, magic::k_BasePowerUpLevel, 1.0f);
	if (seed == entt::null)
	{
		return entt::null;
	}
	// The player has now had the miracle that it casts
	auto& registry = Locator::entitiesRegistry::value();
	const auto type = magic::GetMagicTypeFromPowerUpLevel(magic::GetSpellSeedInfo(Locator::infoConstants::value(), seedType),
	                                                      magic::k_BasePowerUpLevel);
	if (const auto playerEntity = Locator::playerSystem::value().FindPlayer(player).value_or(entt::null);
	    registry.Valid(playerEntity) && static_cast<size_t>(type) < ecs::components::Player::k_MagicTypeCount)
	{
		if (auto* component = registry.TryGet<ecs::components::Player>(playerEntity))
		{
			component->miracles.everEnabled.at(static_cast<size_t>(type)) = true;
		}
	}
	return seed;
}

entt::entity CreateScriptObject(const ObjectType type, uint32_t subtype, const glm::vec3& position, float altitude,
                                float xAngleRadians, float yAngleRadians, const float zAngleRadians, const float scale)
{
	// TODO(Daniels118): handle all types
	switch (type)
	{
	// Asking for a mobile static or for a rock makes the same thing: whether it is a rock is up to its kind. The thing
	// goes onto the land under the place asked for. A holder of a singing stone, a lantern and a bonfire keep the
	// height that the script gave, because they can not move in the physics. A mobile static of the rock kind also
	// keeps the height, and the physics lets it fall.
	case ObjectType::MobileStatic:
	case ObjectType::Rock:
	{
		const bool rockKind = IsRockKindRow(subtype);
		const auto made = MobileStaticArchetype::Create(
		    script::create_rules::KeepsScriptHeight(type, subtype, rockKind) ? position : OnGround(position),
		    static_cast<MobileStaticInfo>(subtype), altitude, xAngleRadians, yAngleRadians, zAngleRadians, scale);
		// The holder of a singing stone shows only as a print of its model on the land
		if (static_cast<MobileStaticInfo>(subtype) == MobileStaticInfo::SingingStoneBase)
		{
			MarkGroundPrint(made);
		}
		if (const auto start = script::create_rules::RockStartOf(type, subtype, rockKind))
		{
			StartScriptRock(made, *start);
		}
		return made;
	}
	case ObjectType::MobileObject:
	case ObjectType::Poo:
	case ObjectType::Ark:
	{
		// A mobile object of the row that the type gives, on the land under the place asked for. It belongs to nobody.
		const auto row = script::create_rules::MobileObjectRowOf(type, subtype);
		if (!row.has_value())
		{
			return entt::null;
		}
		return MobileObjectArchetype::Create(OnGround(position), *row, yAngleRadians, scale);
	}
	case ObjectType::Ball:
		// On the land under the place asked for, even when that is the hand up in the air
		return CreateScriptBall(OnGround(position));
	case ObjectType::Marker:
	{
		// A marker stands at the map position of the point and keeps the height of the point above the land
		const auto& island = Locator::terrainSystem::value();
		return ScriptMarkerArchetype::Create(map_coords::ToWorld(island, map_coords::FromWorld(island, position)));
	}
	case ObjectType::PuzzleGame:
	{
		// The system makes a puzzle game of each kind. Only the beach temple puzzle and the tree puzzles have pieces now.
		const auto kind = static_cast<script::PuzzleGameType>(subtype);
		if (subtype < static_cast<uint32_t>(script::PuzzleGameType::Last) && Locator::puzzleGameSystem::has_value())
		{
			const auto puzzle = Locator::puzzleGameSystem::value().Create(kind, position, yAngleRadians, scale);
			if (puzzle != entt::null)
			{
				return puzzle;
			}
		}
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "CreateScriptObject not implemented for puzzle game kind {}", subtype);
		return entt::null;
	}
	// A scaffold of no town and no workshop, worth what its scale gives, on the land under the place asked for
	case ObjectType::Scaffold:
		if (!Locator::workshopSystem::has_value())
		{
			break;
		}
		return Locator::workshopSystem::value().CreateScaffold(OnGround(position), entt::null, entt::null, yAngleRadians,
		                                                       scale);
	case ObjectType::Villager:
	case ObjectType::VillagerChild:
		// Made standing on the land under the place asked for, whatever height the script gave
		return CreateScriptVillager(type == ObjectType::VillagerChild, subtype,
		                            script::create_rules::KeepsScriptHeight(type) ? position : OnGround(position));
	// The game's whale object type: the opening's sharks
	case ObjectType::Whale:
		return SharkArchetype::Create(position, scale);
	case ObjectType::Creature:
	{
		// A creature of the species the scripts number, which belongs to no one and has its home where it is made. It
		// stands on the ground, as big as its species starts, named from the four creature names of the help texts.
		const auto species = script::property_rules::CreatureTypeFromScript(subtype);
		if (!species.has_value())
		{
			break;
		}
		const auto ground = Locator::terrainSystem::value().GetHeightAt(glm::vec2(position.x, position.z));
		const auto created = ecs::archetypes::CreatureArchetype::Create(
		    {position.x, ground, position.z}, PlayerNames::NEUTRAL, *species, 0, yAngleRadians,
		    ecs::archetypes::CreatureArchetype::StartScale(*species), ecs::archetypes::CreatureArchetype::StartBody(*species));
		// It takes no notice of reactions until a script tells it to, or until it gets a player's mind in a swap. This
		// stays when it becomes the player's creature, so the creature that the player picks in the glade does not react
		// on the first land.
		if (script::create_rules::IgnoresReactions(type))
		{
			ecs::creature_reaction_notice::SetTakesNotice(Locator::entitiesRegistry::value(), created, false);
		}
		return created;
	}
	case ObjectType::Vortex:
	{
		// A vortex of the three kinds; the game makes nothing for any other
		if (subtype > static_cast<uint32_t>(VortexType::Volcano))
		{
			break;
		}
		const auto vortex = Locator::vortexSystem::value().Create(position, static_cast<VortexType>(subtype), altitude);
		return vortex != entt::null ? vortex : static_cast<entt::entity>(0);
	}
	case ObjectType::Feature:
	{
		const auto& info = Locator::infoConstants::value();
		if (!script::create_rules::IsRow(subtype, info.feature.size()))
		{
			break;
		}
		return FeatureArchetype::Create(OnGround(position), static_cast<FeatureInfo>(subtype), yAngleRadians, scale);
	}
	case ObjectType::Totem:
	{
		// A puzzle totem: the building of the row, of no town, on the land under the place asked for
		const auto& info = Locator::infoConstants::value();
		if (!script::create_rules::IsRow(subtype, info.abode.size()))
		{
			break;
		}
		const auto made =
		    PuzzleTotemArchetype::Create(OnGround(position), static_cast<AbodeInfo>(subtype), yAngleRadians, scale);
		// It starts at height 0, down in the land
		if (Locator::puzzleTotemSystem::has_value())
		{
			Locator::puzzleTotemSystem::value().Place(made);
		}
		if (Locator::entitiesMap::has_value())
		{
			Locator::entitiesMap::value().Refile(made);
		}
		return made;
	}
	case ObjectType::Tree:
	{
		// A tree of the land's forests' kinds, in no forest, already as big as it will grow
		const auto& info = Locator::infoConstants::value();
		if (!script::create_rules::IsRow(subtype, info.tree.size()))
		{
			break;
		}
		return TreeArchetype::Create(0, OnGround(position), static_cast<TreeInfo>(subtype), false,
		                             script::create_rules::TreeAngle(yAngleRadians), scale, scale);
	}
	case ObjectType::AnimatedStatic:
	{
		const auto& info = Locator::infoConstants::value();
		if (!script::create_rules::IsRow(subtype, info.animatedStatic.size()))
		{
			break;
		}
		return AnimatedStaticArchetype::Create(OnGround(position), static_cast<AnimatedStaticInfo>(subtype), yAngleRadians,
		                                       scale);
	}
	case ObjectType::WeatherThing:
	{
		// A weather thing of a kind of weather, bringing a small storm of that weather to the place
		const auto& info = Locator::infoConstants::value();
		if (!script::create_rules::IsRow(subtype, info.weather.size()) || !Locator::weatherSystem::has_value())
		{
			break;
		}
		return Locator::weatherSystem::value().CreateWeatherThing(
		    script::create_rules::WeatherThingStorm(info.weather.at(subtype), OnGround(position)));
	}
	case ObjectType::Store:
	{
		// A new pile of food or wood on the land, holding one of it and belonging to nobody. It thuds as it lands. The
		// script then fills it.
		const auto resource = script::create_rules::StoreResource(subtype);
		if (!resource.has_value() || !Locator::resourceStoreSystem::has_value())
		{
			break;
		}
		return Locator::resourceStoreSystem::value().MakePile(*resource, position, 1);
	}
	case ObjectType::OneShotSpell:
		// A globe of a seed at its plain miracle, lying on the ground
		return Locator::magicSystem::value().CreateOneOffSeed(OnGround(position), static_cast<SpellSeedType>(subtype),
		                                                      magic::k_BasePowerUpLevel, 1.0f);
	case ObjectType::OneShotSpellInHand:
		return CreateScriptSeedInHand(subtype);
	case ObjectType::SpellDispenser:
		// A dispenser building of the buildings' kinds, with no miracle, off until a script gives it a miracle. The script
		// gives it no town, so it belongs to the first town that the first player gained.
		return Locator::magicSystem::value().CreateScriptDispenser(position, static_cast<AbodeInfo>(subtype), yAngleRadians,
		                                                           scale, entt::null);
	case ObjectType::Animal:
	case ObjectType::Bird:
		// Made on its own and held still for the script: the land's birds, grazers and hunters
		if (Locator::animalSystem::has_value())
		{
			const auto animal = Locator::animalSystem::value().CreateScriptAnimal(static_cast<AnimalInfo>(subtype),
			                                                                      glm::vec2(position.x, position.z));
			if (animal != entt::null)
			{
				return animal;
			}
		}
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "CreateScriptObject not implemented for animal kind {}", subtype);
		return static_cast<entt::entity>(0);
	default:
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "CreateScriptObject not implemented for type {}", static_cast<int>(type));
	}
	return entt::null;
}

VMValue Pop(DataType& type)
{
	auto& lhvm = Locator::vm::value();
	return lhvm.Pop(type);
}

VMValue Pop()
{
	auto& lhvm = Locator::vm::value();
	return lhvm.Pop();
}

float Popf()
{
	auto& lhvm = Locator::vm::value();
	return lhvm.Popf();
}

/// An object a script made takes a place in the scripts' table as made by a script, so the script controls it from
/// its first reference
void RegisterCreated(entt::entity object)
{
	if (object != entt::null && Locator::entitiesRegistry::value().Valid(object))
	{
		Locator::scriptObjects::value().Register(object, true);
	}
}

/// An object given to a native: a native that takes control of what it is given takes control of it
entt::entity PopObject()
{
	// A script's object 0 is none: the native was given nothing, or a creation that failed
	const auto id = Pop().uintVal;
	if (id == 0)
	{
		return entt::null;
	}
	return Locator::scriptObjects::value().Fetch(static_cast<entt::entity>(id));
}

/// The villager a native is given, if it is one the living actions direct
bool IsDirectableVillager(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(object) && registry.AllOf<ecs::components::Villager, ecs::components::LivingAction>(object) &&
	       Locator::livingActionSystem::has_value();
}

/// A script effect's seconds as game turns: a whole number of turns a second, as the game's turn length gives it
int SpecialEffectTurns(float seconds)
{
	constexpr auto k_TurnsPerSecond = 1000 / static_cast<int>(ecs::systems::TimeSystemInterface::k_TurnDuration.count());
	return static_cast<int>(seconds * static_cast<float>(k_TurnsPerSecond));
}

/// One of the spot visuals started by a script, at a point or on an object that ends it when it goes, for the first
/// player; numbers past the table's last start nothing
uint32_t StartScriptSpotVisual(int32_t effect, glm::vec3 position, int turns, entt::entity owner)
{
	constexpr int32_t k_LastSpotVisual = 49;
	if (effect < 0 || effect > k_LastSpotVisual || !Locator::effectRequestSystem::has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Spell not created");
		return 0;
	}
	auto& effects = Locator::effectRequestSystem::value();
	// The visual shows the colour of the first player, the same on every computer (as the script player 0)
	const auto handle = effects.Start({
	    .type = static_cast<SpotVisualType>(effect),
	    .position = position,
	    .turns = turns,
	    .owner = owner,
	    .player = Locator::playerSystem::has_value()
	                  ? std::optional(static_cast<int>(Locator::playerSystem::value().GetLocalPlayer()))
	                  : std::nullopt,
	});
	if (!effects.IsRunning(handle))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Spell not created");
		return 0;
	}
	return handle;
}

/// The thing a script holds for a visual it started, taking its place in the scripts' table as made by a script; none
/// when no visual was started. The script holds the thing, never the visual's own number.
static entt::entity ScriptSpotVisualThing(uint32_t effect, glm::vec3 position)
{
	if (effect == ecs::systems::k_NoEffectHandle)
	{
		return entt::null;
	}
	const auto thing = ecs::script_spot_visuals::MakeThing(Locator::entitiesRegistry::value(), effect, position);
	RegisterCreated(thing);
	return thing;
}

void Push(VMValue value, DataType type)
{
	auto& lhvm = Locator::vm::value();
	lhvm.Push(value, type);
}

void Pushf(float value)
{
	auto& lhvm = Locator::vm::value();
	lhvm.Pushf(value);
}

void Pushv(float value)
{
	auto& lhvm = Locator::vm::value();
	lhvm.Pushv(value);
}

void Pushi(int32_t value)
{
	auto& lhvm = Locator::vm::value();
	lhvm.Pushi(value);
}

void Pusho(uint32_t value)
{
	auto& lhvm = Locator::vm::value();
	lhvm.Pusho(value);
}

void Pushb(bool value)
{
	auto& lhvm = Locator::vm::value();
	lhvm.Pushb(value);
}

void CHLApi::TaskStopped(uint32_t task)
{
#if defined(OPENBLACK_DEVELOPER_TOOLS)
	// The draw trace keeps the turn of each stop: a stop changes the waits of the tasks before it
	if (auto* trace = Locator::gameRandom::has_value() ? Locator::gameRandom::value().GetDrawTrace() : nullptr;
	    trace != nullptr)
	{
		const auto& tasks = Locator::vm::value().GetTasks();
		const auto found = tasks.find(task);
		trace->TaskStopped(found == tasks.end() ? fmt::format("{}", task)
		                                        : fmt::format("{} {} {}", task, found->second.filename, found->second.name));
	}
#endif
	// What the task held goes back: the dialogue, its cinema bars, the camera and the game's speed
	ReleaseDialogue(task);
	auto& director = Locator::cinematicDirectorSystem::value();
	if (director.IsWideScreenOn() && director.GetWideScreenOwner() == task)
	{
		director.SetWideScreen(false, 0);
	}
	// A hand demonstration it started ends with it
	if (Locator::handDemoSystem::has_value())
	{
		Locator::handDemoSystem::value().TaskStopped(task);
	}
	const auto released = Locator::scriptControlSystem::value().TaskStopped(Locator::camera::value(), task);
	if (released.camera)
	{
		CountCameraOrder();
		ApplySwitchChanges(script::switches::CameraReleased());
	}
	if (released.gameSpeed)
	{
		Locator::time::value().SetSpeed(1.0f);
	}
}

bool CHLApi::HelpScriptHoldsDialogue()
{
	const auto owner = Locator::dialogueControlSystem::value().GetOwner();
	return owner != 0 && TaskType(owner) == lhvm::ScriptType::Help;
}

void CHLApi::StopHelpScriptsByEscape()
{
	StopHelpScripts();
	if (Locator::helpTextSystem::has_value())
	{
		Locator::helpTextSystem::value().InterruptAdvisorsByEscape();
	}
}

bool CHLApi::ApplyInterfaceLevel(int32_t level)
{
	// What the camera lets the player do at this level of the interface, as the tutorials step through them
	if (!Locator::cameraHelpSystem::value().Get().SetInterfaceLevel(level))
	{
		return false;
	}
	// The level also limits what the Action button of the player's hand can do
	if (Locator::playerSystem::has_value())
	{
		script::interface_levels::Apply(Locator::entitiesRegistry::value(),
		                                Locator::playerSystem::value().FindPlayer(PlayerNames::PLAYER_ONE).value_or(entt::null),
		                                level);
	}
	return true;
}

bool CHLApi::StartHelpScript(std::string_view name)
{
	auto& dialogue = Locator::dialogueControlSystem::value();
	if (const auto owner = dialogue.GetOwner(); owner != 0)
	{
		// A help script holding the dialogue gives way; its tasks give it back as they stop
		if (TaskType(owner) == lhvm::ScriptType::Help)
		{
			StopHelpScripts();
		}
		if (dialogue.GetOwner() != 0)
		{
			return false;
		}
	}
	Locator::vm::value().StartScript(std::string(name), script::start::StartableScripts());
	return true;
}

bool CHLApi::StartChallengeReminder(uint32_t challenge)
{
	auto& dialogue = Locator::dialogueControlSystem::value();
	if (const auto owner = dialogue.GetOwner(); owner != 0 && TaskType(owner) == lhvm::ScriptType::Help)
	{
		StopHelpScripts();
	}
	if (dialogue.GetOwner() != 0)
	{
		return false;
	}
	return StartChallengeScript(challenge);
}

bool CHLApi::StartChallengeScript(uint32_t challenge)
{
	if (!Locator::vm::has_value())
	{
		return false;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto found = ecs::challenge_records::Find(registry, challenge);
	if (!found.has_value())
	{
		return false;
	}
	const auto& record = registry.Get<ecs::components::ChallengeRecord>(*found);
	auto& lhvm = Locator::vm::value();
	for (const auto& argument : ecs::challenge_records::ReminderArguments(record))
	{
		lhvm.Push(argument.value, argument.type);
	}
	lhvm.StartScript(record.reminderScript, script::start::StartableScripts());
	return true;
}

/// An object handed to a script: none is the script's object 0
void PushObject(entt::entity object)
{
	Pusho(object == entt::null ? 0u : static_cast<uint32_t>(object));
}

CHLApi::CHLApi()
{
	_functionsTable.reserve(464);
	InitFunctionsTable0();
	InitFunctionsTable1();
	InitFunctionsTable2();
	InitFunctionsTable3();
	InitFunctionsTable4();
}

void CHLApi::SaveState(ecs::reflection::BinaryWriter& out) const
{
	ecs::reflection::Save(out, _gameSoundOn);
	ecs::reflection::Save(out, _highlightDrawOn);
	ecs::reflection::Save(out, _scriptHelpOn);
}

bool CHLApi::LoadState(ecs::reflection::BinaryReader& in)
{
	ecs::reflection::Load(in, _gameSoundOn);
	ecs::reflection::Load(in, _highlightDrawOn);
	ecs::reflection::Load(in, _scriptHelpOn);
	return !in.Failed();
}

void CHLApi::ResetSwitches()
{
	_gameSoundOn = true;
	_highlightDrawOn = true;
	_scriptHelpOn = true;
	// The sun that a script hid shows again
	if (Locator::skySystem::has_value())
	{
		Locator::skySystem::value().SetSunHidden(false);
	}
	if (Locator::creatureAudioSystem::has_value())
	{
		Locator::creatureAudioSystem::value().SetOtherVoicesEnabled(true);
	}
	// No lesson of growing up runs any more for the first player's creature, so the game's help about it speaks again
	if (Locator::playerSystem::has_value() && Locator::entitiesRegistry::has_value())
	{
		auto& players = Locator::playerSystem::value();
		if (const auto creature = players.GetPrimaryCreature(players.GetLocalPlayer()))
		{
			if (auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(*creature))
			{
				mind->inDevelopmentScript = false;
			}
		}
	}
}

void CHLApi::NotImplemented(std::optional<int32_t> detail)
{
	if (!_stubCalls.Record(_currentNative, detail))
	{
		return;
	}
	const auto name = _currentNative < _functionsTable.size() ? _functionsTable[_currentNative].name : "?";
	if (detail.has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Script native {} ({}) not implemented for {}; later calls are counted",
		                    name, _currentNative, *detail);
	}
	else
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Script native {} ({}) not implemented; later calls are counted", name,
		                    _currentNative);
	}
}

void CHLApi::LogStubCalls() const
{
	const auto entries = _stubCalls.Entries();
	if (entries.empty())
	{
		return;
	}
	SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "Unimplemented script natives called: {} calls of {} natives",
	                   _stubCalls.Total(), entries.size());
	for (const auto& entry : entries)
	{
		const auto name = entry.native < _functionsTable.size() ? _functionsTable[entry.native].name : "?";
		if (entry.detail.has_value())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "  {} calls of {} ({}) for {}", entry.calls, name, entry.native,
			                   *entry.detail);
		}
		else
		{
			SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "  {} calls of {} ({})", entry.calls, name, entry.native);
		}
	}
}

/// The native being run isn't written yet, or not for this case of it
void NotImplemented(std::optional<int32_t> detail)
{
	Locator::chlapi::value().NotImplemented(detail);
}

/// What the player last clicked, kept on their hand; none without a hand. A game on one computer has the click of its
/// own player, the first player. In a network game the turn writes the clicks, and the scripts read the clicks of the
/// first player on every computer.
ecs::components::HandClicked* LocalHandClicked()
{
	if (!Locator::handSystem::has_value())
	{
		return nullptr;
	}
	const auto hand = Locator::handSystem::value().GetPlayerHands()[static_cast<size_t>(HandSystemInterface::Side::Left)];
	auto& registry = Locator::entitiesRegistry::value();
	if (hand == entt::null || !registry.Valid(hand))
	{
		return nullptr;
	}
	if (auto* clicked = registry.TryGet<ecs::components::HandClicked>(hand))
	{
		return clicked;
	}
	return &registry.Assign<ecs::components::HandClicked>(hand);
}

/// Make the land's draw list again on the next frame. The commands that move the camera or change the detail of a
/// thing use this.
void RemakeDrawList()
{
	if (Locator::objectDrawListSystem::has_value())
	{
		Locator::objectDrawListSystem::value().RequestRebuild(1);
	}
}

void None() {} // 000 NONE

/// The scripts read the camera, the hand and the view of the player from the last reports that a turn did, in a game
/// alone too, so that every computer and every frame rate gets the same answer. Before the first report they read the
/// middle of the world's axes.
std::optional<HandSystemInterface::ReportedView> ScriptView()
{
	return Locator::handSystem::has_value() ? Locator::handSystem::value().GetReportedView(PlayerNames::PLAYER_ONE)
	                                        : std::nullopt;
}

void GameThingClicked() // 016 GAME_THING_CLICKED
{
	const auto object = PopObject();
	if (object == entt::null || !Locator::entitiesRegistry::value().Valid(object))
	{
		ObjectNoLongerValid();
		Pushb(false);
		return;
	}
	auto* clicked = LocalHandClicked();
	const bool isClicked = clicked != nullptr && hand_click::IsThingClicked(*clicked, object);
	// A clicked scroll of leaving the land through its vortex first saves the land into the previous land's slot. The
	// click is forgotten while the game saves, so the save does not keep it, and then the scroll is clicked again on
	// this turn.
	const auto* highlight = Locator::entitiesRegistry::value().TryGet<const ecs::components::ScriptHighlight>(object);
	if (isClicked && highlight != nullptr &&
	    ecs::script_highlights::SavesPreviousLandOnClick(highlight->kind, highlight->scriptId) &&
	    Locator::saveGameSystem::has_value())
	{
		hand_click::ClearThing(*clicked);
		Locator::saveGameSystem::value().InstantSave(gamesave::k_PreviousLandSlot);
		hand_click::ClickThing(*clicked, object, Locator::time::value().GetTurn());
	}
	Pushb(isClicked);
}

/// A native that takes control gives each member of a container that it goes through to the script, before it does
/// its work on the member. The member stays under the script's control until a script releases it.
static void TakeControlInLoop(ecs::Registry& registry, entt::entity member)
{
	if (registry.Valid(member))
	{
		registry.AssignOrReplace<ecs::components::ScriptControlled>(member);
	}
}

/// A creature given a state plays what the script last gave it to play
void CreatureCarriesOutScriptPlay(entt::entity creature)
{
	auto& mind = Locator::entitiesRegistry::value().Get<ecs::components::CreatureMindState>(creature);
	if (Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().CarryOutForScript(creature, creature_script_play::Agenda(mind.scriptPlay));
	}
}

void SetScriptState() // 017 SET_SCRIPT_STATE
{
	const auto state = Pop().intVal;
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		ObjectNoLongerValid();
		return;
	}
	// The script takes control of each member of a group. Each living member takes the state as one would on its own,
	// with no word of those that can't.
	if (IsScriptContainer(registry, object))
	{
		for (const auto member : ContainerMembers(registry, object))
		{
			TakeControlInLoop(registry, member);
			if (IsDirectableVillager(member))
			{
				auto& living = Locator::livingActionSystem::value();
				if (state >= 0 && state < static_cast<int32_t>(VillagerStates::_COUNT))
				{
					living.VillagerSetScriptState(member, static_cast<VillagerStates>(state));
				}
			}
			else if (registry.Valid(member) && registry.AllOf<ecs::components::CreatureMindState>(member))
			{
				CreatureCarriesOutScriptPlay(member);
			}
			else if (registry.Valid(member) && registry.AllOf<ecs::components::Animal>(member) &&
			         Locator::animalSystem::has_value() && state >= 0)
			{
				static_cast<void>(Locator::animalSystem::value().SetScriptState(member, static_cast<LivingStates>(state)));
			}
		}
		return;
	}
	if (IsDirectableVillager(object))
	{
		auto& living = Locator::livingActionSystem::value();
		if (state < 0 || state >= static_cast<int32_t>(VillagerStates::_COUNT) || !living.VillagerCanBeDirected(object))
		{
			ScriptMessage("Object not living for set state");
			return;
		}
		living.VillagerSetScriptState(object, static_cast<VillagerStates>(state));
		return;
	}
	if (registry.AllOf<ecs::components::CreatureMindState>(object))
	{
		// A creature takes no state: it starts playing what the script last gave it to play
		CreatureCarriesOutScriptPlay(object);
		return;
	}
	// An animal takes the state of its own table
	if (registry.AllOf<ecs::components::Animal>(object) && Locator::animalSystem::has_value())
	{
		if (state < 0 || !Locator::animalSystem::value().SetScriptState(object, static_cast<LivingStates>(state)))
		{
			if (const auto logger = spdlog::get("scripting"))
			{
				SPDLOG_LOGGER_WARN(logger, "SET_SCRIPT_STATE: animal state {} is not done", state);
			}
		}
		return;
	}
	ScriptMessage("Object not living for set state");
}

void SetScriptStatePos() // 018 SET_SCRIPT_STATE_POS
{
	const auto position = PopVec();
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		ObjectNoLongerValid();
		return;
	}
	const auto place = glm::vec2(position.x, position.z);
	if (IsScriptContainer(registry, object))
	{
		// The script takes control of each member. Only a villager gets the place.
		for (const auto member : ContainerMembers(registry, object))
		{
			TakeControlInLoop(registry, member);
			if (IsDirectableVillager(member))
			{
				Locator::livingActionSystem::value().VillagerSetScriptPlace(member, place);
			}
		}
		return;
	}
	if (!IsDirectableVillager(object))
	{
		ScriptMessage("Object not villager for state data");
		return;
	}
	Locator::livingActionSystem::value().VillagerSetScriptPlace(object, place);
}

void SetScriptFloat() // 019 SET_SCRIPT_FLOAT
{
	const auto value = Popf();
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		ObjectNoLongerValid();
		return;
	}
	if (IsScriptContainer(registry, object))
	{
		// The script takes control of each member. Only a villager gets the number.
		for (const auto member : ContainerMembers(registry, object))
		{
			TakeControlInLoop(registry, member);
			if (IsDirectableVillager(member))
			{
				Locator::livingActionSystem::value().VillagerSetScriptNumber(member, value);
			}
		}
		return;
	}
	if (!IsDirectableVillager(object))
	{
		ScriptMessage("Object not villager for state data");
		return;
	}
	Locator::livingActionSystem::value().VillagerSetScriptNumber(object, value);
}

void SetScriptUlong() // 020 SET_SCRIPT_ULONG
{
	// A count of -1 plays the clip without end
	const auto plays = Pop().uintVal;
	const auto animation = Pop().intVal;
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		ObjectNoLongerValid();
		return;
	}
	// The two numbers serve the script state that the villager goes into next. They are a clip and its plays for a
	// clip, or the fewest and most turns at each stop for a walk round a place.
	const auto giveVillager = [animation, plays](entt::entity villager) {
		auto& living = Locator::livingActionSystem::value();
		living.VillagerSetScriptNumbers(villager, static_cast<uint32_t>(animation), plays);
		living.VillagerSetScriptAnimation(villager, static_cast<AnimId>(animation), plays);
	};
	if (IsScriptContainer(registry, object))
	{
		// The script takes control of each member. Only a villager gets the two numbers. A creature or an animal in the
		// group keeps the numbers that it had.
		for (const auto member : ContainerMembers(registry, object))
		{
			TakeControlInLoop(registry, member);
			if (IsDirectableVillager(member))
			{
				giveVillager(member);
			}
		}
		return;
	}
	if (IsDirectableVillager(object))
	{
		giveVillager(object);
		return;
	}
	if (auto* mind = registry.TryGet<ecs::components::CreatureMindState>(object))
	{
		// Kept for the creature until the script tells it to start
		mind->scriptPlay = {.animation = animation, .plays = plays};
		return;
	}
	ScriptMessage("setting the state of something neither a creature nor a villager");
}

/// Whether something is drowning, as the scripts ask: a villager while it is in its drowning state, anything else
/// while it is in the physics with its body's centre under the sea's level
static bool IsDrowning(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return false;
	}
	if (registry.AllOf<ecs::components::Villager>(object))
	{
		const auto* action = registry.TryGet<const ecs::components::LivingAction>(object);
		return action != nullptr && Locator::livingActionSystem::has_value() &&
		       Locator::livingActionSystem::value().VillagerGetState(*action, ecs::components::LivingAction::Index::Top) ==
		           VillagerStates::Drowning;
	}
	if (!registry.AllOf<ecs::components::InPhysics>(object) || !Locator::dynamicsSystem::has_value())
	{
		return false;
	}
	const auto* entry = Locator::dynamicsSystem::value().Find(object);
	return entry != nullptr && entry->body != nullptr && entry->body->Centre().y < 0.0f;
}

/// Whether a town is wholly destroyed: none of its buildings stands
static bool TownCompletelyDestroyed(const ecs::components::Town& town)
{
	const auto& registry = Locator::entitiesRegistry::value();
	std::vector<script::property_rules::TownBuilding> buildings;
	buildings.reserve(town.abodes.size());
	for (const auto abode : town.abodes)
	{
		if (!registry.Valid(abode))
		{
			continue;
		}
		const auto* progress = registry.TryGet<const ecs::components::BuildProgress>(abode);
		buildings.push_back({
		    .life = ecs::world_objects::LifeOf(abode),
		    .field = registry.AllOf<ecs::components::Field>(abode),
		    .built = progress != nullptr ? progress->built : 1.0f,
		});
	}
	return script::property_rules::TownCompletelyDestroyed(buildings);
}

/// A creature has its body from the time that it is made. A script that reads or sets a value of the body before the
/// first turn of the creature works on the start values of its species, and the value that it sets stays.
static void StartCreatureBody(entt::entity object)
{
	if (Locator::entitiesRegistry::value().AllOf<ecs::components::Creature>(object) &&
	    Locator::creaturePhysiologySystem::has_value())
	{
		Locator::creaturePhysiologySystem::value().StartBody(object);
	}
}

/// A creature's body as the scripts read and set it, none for anything else
static creature_physiology::Needs* CreatureNeedsOf(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.AllOf<ecs::components::Creature>(object))
	{
		return nullptr;
	}
	StartCreatureBody(object);
	auto* needs = registry.TryGet<ecs::components::CreatureNeeds>(object);
	return needs != nullptr ? &needs->needs : nullptr;
}

/// The need of a creature a property is, none for the properties that aren't
static std::optional<script::property_rules::CreatureNeed> NeedOf(script::ObjectPropertyType prop)
{
	using script::ObjectPropertyType;
	using script::property_rules::CreatureNeed;
	switch (prop)
	{
	case ObjectPropertyType::CreatureWarmth:
		return CreatureNeed::Warmth;
	case ObjectPropertyType::CreatureEnergy:
		return CreatureNeed::Energy;
	case ObjectPropertyType::CreatureItchiness:
		return CreatureNeed::Itchiness;
	case ObjectPropertyType::CreatureAmountOfPoo:
		return CreatureNeed::Poo;
	case ObjectPropertyType::CreatureExhaustion:
		return CreatureNeed::Exhaustion;
	case ObjectPropertyType::CreatureDehydration:
		return CreatureNeed::Dehydration;
	default:
		return std::nullopt;
	}
}

static float& NeedValue(creature_physiology::Needs& needs, script::property_rules::CreatureNeed need)
{
	using script::property_rules::CreatureNeed;
	switch (need)
	{
	case CreatureNeed::Warmth:
		return needs.warmth;
	case CreatureNeed::Energy:
		return needs.energy;
	case CreatureNeed::Itchiness:
		return needs.itchiness;
	case CreatureNeed::Poo:
		return needs.poo;
	case CreatureNeed::Exhaustion:
		return needs.exhaustion;
	case CreatureNeed::Dehydration:
	default:
		return needs.dehydration;
	}
}

/// Whether a thing is one of the world's objects, which have a life, angles and a height; a town isn't
bool IsWorldObject(entt::entity object)
{
	return !Locator::entitiesRegistry::value().AllOf<ecs::components::Town>(object);
}

/// Whether a living thing: its angle is kept by its walking, not by how it was placed
bool IsLivingThing(entt::entity object)
{
	return Locator::entitiesRegistry::value()
	    .AnyOf<ecs::components::Villager, ecs::components::Creature, ecs::components::Animal>(object);
}

/// The way a living thing faces, as the scripts take it: radians from +x towards +z
float LivingAngleOf(entt::entity object, const Transform& transform)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (const auto* walk = registry.TryGet<const ecs::components::WallHug>(object); walk != nullptr)
	{
		return walk->yAngle;
	}
	if (const auto* animal = registry.TryGet<const ecs::components::Animal>(object); animal != nullptr)
	{
		return animal->heading;
	}
	if (const auto* locomotion = registry.TryGet<const ecs::components::CreatureLocomotion>(object);
	    locomotion != nullptr && locomotion->started)
	{
		return script::property_rules::CreatureHeadingToLivingAngle(locomotion->heading);
	}
	// Standing still, a living thing is turned as it is drawn: its model faces the other way round, a quarter turn on
	return script::property_rules::CreatureHeadingToLivingAngle(
	    GameHeading(script::property_rules::PlacedAngles(transform.rotation).y));
}

/// Turns a living thing to face a way, given as the scripts take it: its walk, if it has one, faces that way from now
void TurnLivingThing(entt::entity object, Transform& transform, float angle)
{
	auto& registry = Locator::entitiesRegistry::value();
	const bool turned = angle != LivingAngleOf(object, transform);
	const auto heading = script::property_rules::LivingAngleToCreatureHeading(angle);
	if (auto* walk = registry.TryGet<ecs::components::WallHug>(object); walk != nullptr)
	{
		walk->yAngle = angle;
		walk->gameAngle = static_cast<uint16_t>(gutils::ConvertAngle3DToGame(angle));
	}
	if (auto* animal = registry.TryGet<ecs::components::Animal>(object); animal != nullptr)
	{
		animal->heading = angle;
		animal->previousHeading = angle;
	}
	if (auto* locomotion = registry.TryGet<ecs::components::CreatureLocomotion>(object); locomotion != nullptr)
	{
		locomotion->heading = heading;
		locomotion->targetHeading = heading;
		locomotion->fromHeading = heading;
		locomotion->toHeading = heading;
	}
	transform.rotation = glm::mat3(portable_math::EulerAngleY(heading.Yaw()));
	registry.SetDirty();
	// A thing that turns comes out of the map's cells and goes in again, at the front of the list of its cell
	if (turned)
	{
		ecs::thing_map_place::Reinsert(object);
	}
}

/// Turns a thing that is not living to the given angles and keeps them. A thing in the map's cells comes out of them and
/// goes in again: always for a turn of a lean, and for a turn about the upright only when the angle about y changes.
void TurnPlacedThing(entt::entity object, const glm::vec3& angles, bool lean)
{
	auto& registry = Locator::entitiesRegistry::value();
	const bool turned = lean || angles.y != ecs::placed_turn::AnglesOf(registry, object).y;
	ecs::placed_turn::SetAngles(registry, object, angles);
	if (turned)
	{
		ecs::thing_map_place::Reinsert(object);
	}
}

/// Whether a thing may lean, so that its angles across and forward are its own; the others always stand upright
static bool CanLean(entt::entity object)
{
	return ecs::placed_turn::CanLean(Locator::entitiesRegistry::value(), object);
}

/// How tall an object stands to the scripts: a creature by its size, anything else by its model as it is scaled, none
/// without a model
float ScriptHeightOf(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (const auto* creature = registry.TryGet<const ecs::components::Creature>(object); creature != nullptr)
	{
		// As tall as its body is shown, kept within the sizes it can be drawn at
		return script::property_rules::CreatureHeight(ecs::components::ShownSize(*creature));
	}
	const auto* transform = registry.TryGet<const Transform>(object);
	const auto* mesh = registry.TryGet<const ecs::components::Mesh>(object);
	if (transform == nullptr || mesh == nullptr || !Locator::resources::has_value())
	{
		return 0.0f;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(mesh->id))
	{
		return 0.0f;
	}
	return meshes.Handle(mesh->id)->GetBoundingBox().Size().y * transform->scale.y;
}

/// The town a totem stands in: the town of the town centre it stands on
static entt::entity TownOfTotem(entt::entity totem)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* totemComponent = registry.TryGet<const ecs::components::VillageTotem>(totem);
	const auto* abode = totemComponent != nullptr && registry.Valid(totemComponent->townCentre)
	                        ? registry.TryGet<const ecs::components::Abode>(totemComponent->townCentre)
	                        : nullptr;
	if (abode == nullptr)
	{
		return entt::null;
	}
	const auto& towns = registry.Context().towns;
	const auto found = towns.find(abode->townId);
	return found != towns.end() && registry.Valid(found->second) ? found->second : entt::null;
}

/// The script asked for a property the thing doesn't have
static void CannotGetProperty(script::ObjectPropertyType prop)
{
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Cannot Get Property {}", static_cast<int>(prop));
	Pushf(0.0f);
}

static void CannotSetProperty(script::ObjectPropertyType prop)
{
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Cannot Set Property {}", static_cast<int>(prop));
}

void GetProperty() // 021 GET_PROPERTY
{
	const auto object = PopObject();
	const auto prop = static_cast<script::ObjectPropertyType>(Pop().intVal);
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ObjectNoLongerValid();
		Pushf(0.0f);
		return;
	}
	// A town, a flock or a dance gives 0 for each property but its player. A script reads its members one by one.
	if (IsScriptContainer(registry, object) && prop != script::ObjectPropertyType::Player)
	{
		Pushf(0.0f);
		return;
	}
	if (const auto need = NeedOf(prop); need.has_value())
	{
		auto* needs = CreatureNeedsOf(object);
		if (needs == nullptr)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object not a creature");
			Pushf(0.0f);
			return;
		}
		Pushf(NeedValue(*needs, *need));
		return;
	}
	switch (prop)
	{
	case script::ObjectPropertyType::CreatureFightHealth:
		if (!registry.AllOf<ecs::components::Creature>(object))
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object not a creature");
			Pushf(0.0f);
			return;
		}
		{
			const auto health =
			    Locator::creatureFightSystem::has_value() ? Locator::creatureFightSystem::value().GetFightHealth(object) : 1.0f;
			if (Locator::fightTranscript::has_value())
			{
				Locator::fightTranscript::value().ScriptRead(object, "FIGHTHEALTH", health);
			}
			Pushf(health);
			return;
		}
	case script::ObjectPropertyType::Flying:
		// In the physics, thrown, dropped or knocked and not yet at rest
		Pushb(registry.AllOf<ecs::components::InPhysics>(object));
		return;
	case script::ObjectPropertyType::Drowning:
		Pushb(IsDrowning(object));
		return;
	case script::ObjectPropertyType::Scale:
		// A creature answers the size its body is shown at
		if (const auto* creature = registry.TryGet<const ecs::components::Creature>(object); creature != nullptr)
		{
			Pushf(ecs::components::ShownSize(*creature));
			return;
		}
		if (const auto* transform = Locator::entitiesRegistry::value().TryGet<const Transform>(object); transform != nullptr)
		{
			Pushf(transform->scale.x);
			return;
		}
		Pushf(0.0f);
		return;
	case script::ObjectPropertyType::Speed:
		// A thing in the physics gives the speed of its flight
		if (registry.AllOf<ecs::components::InPhysics>(object) && Locator::dynamicsSystem::has_value())
		{
			if (const auto* entry = Locator::dynamicsSystem::value().Find(object); entry != nullptr && entry->body != nullptr)
			{
				Pushf(glm::length(entry->body->velocity));
				return;
			}
		}
		// A weather thing gives the speed of its storm
		if (Locator::weatherSystem::has_value())
		{
			if (const auto stormSpeed = Locator::weatherSystem::value().WeatherThingSpeed(object))
			{
				Pushf(*stormSpeed);
				return;
			}
		}
		// An animal gives its walk speed, metres a turn
		if (registry.AllOf<ecs::components::Animal>(object))
		{
			Pushf(Locator::animalSystem::has_value() ? Locator::animalSystem::value().SpeedForScript(object) : 0.0f);
			return;
		}
		if (const auto* wallHug = Locator::entitiesRegistry::value().TryGet<const ecs::components::WallHug>(object);
		    wallHug != nullptr)
		{
			Pushf(ecs::villager_script_rules::WalkSpeedToScriptSpeed(wallHug->speed));
			return;
		}
		Pushf(0.0f);
		return;
	case script::ObjectPropertyType::Age:
		// Its age in years; only living things have one
		if (const auto* person = registry.TryGet<const ecs::components::Villager>(object); person != nullptr)
		{
			Pushf(static_cast<float>(ecs::villager_age::AgeNow(*person)));
			return;
		}
		// A creature has the age of its body
		if (const auto* needs = CreatureNeedsOf(object); needs != nullptr)
		{
			Pushf(static_cast<float>(needs->age));
			return;
		}
		// TODO(opening): the ages of animals; other things have none ("not used on non living objects")
		NotImplemented(static_cast<int32_t>(prop));
		Pushf(0.0f);
		return;
	case script::ObjectPropertyType::InHand:
		// Held in a hand, as a number
		Pushf(registry.AllOf<ecs::components::InHand>(object) ? 1.0f : 0.0f);
		return;
	case script::ObjectPropertyType::Player:
	{
		const auto* town = registry.TryGet<const ecs::components::Town>(object);
		Pushf(script::property_rules::PlayerProperty(ecs::world_objects::PlayerOf(object),
		                                             town != nullptr && TownCompletelyDestroyed(*town)));
		return;
	}
	case script::ObjectPropertyType::BuiltPercentage:
		// How much of it is built, 0 to 1; anything that isn't built is whole
		if (const auto* progress = registry.TryGet<const ecs::components::BuildProgress>(object); progress != nullptr)
		{
			Pushf(progress->built);
			return;
		}
		Pushf(1.0f);
		return;
	case script::ObjectPropertyType::XPos:
	case script::ObjectPropertyType::YPos:
	case script::ObjectPropertyType::ZPos:
	{
		const auto* transform = registry.TryGet<const Transform>(object);
		if (transform == nullptr)
		{
			Pushf(0.0f);
			return;
		}
		const auto& at = transform->position;
		if (prop == script::ObjectPropertyType::YPos)
		{
			// How high above the land it is
			const float land =
			    Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(at.x, at.z)) : 0.0f;
			Pushf(at.y - land);
			return;
		}
		// Across the land, as a map position holds it
		Pushf(map_coords::Quantise(prop == script::ObjectPropertyType::XPos ? at.x : at.z));
		return;
	}
	case script::ObjectPropertyType::Health:
		if (!IsWorldObject(object))
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object not an object");
			CannotGetProperty(prop);
			return;
		}
		StartCreatureBody(object);
		Pushf(ecs::world_objects::LifeOf(object));
		return;
	case script::ObjectPropertyType::Moving:
	{
		// A creature moves while it is on its way somewhere, from being given where to go until it stops
		if (const auto* locomotion = registry.TryGet<const ecs::components::CreatureLocomotion>(object); locomotion != nullptr)
		{
			Pushb(locomotion->destination.has_value() && !locomotion->facingOnly &&
			      locomotion->motion != ecs::components::CreatureLocomotion::Motion::Standing);
			return;
		}
		// An animal moved across the land in its last turn
		if (const auto* animal = registry.TryGet<const ecs::components::Animal>(object); animal != nullptr)
		{
			Pushb(script::property_rules::MovedAcross(animal->previousPosition, animal->position));
			return;
		}
		// Things that aren't in the map's cells never move
		if (!IsWorldObject(object))
		{
			Pushb(false);
			return;
		}
		// TODO(script-natives): a villager moved in its last turn, and any other thing has moved from where it was made or
		// came to rest from the physics: openblack keeps neither position
		NotImplemented(static_cast<int32_t>(prop));
		Pushb(false);
		return;
	}
	case script::ObjectPropertyType::Death:
	{
		// What a dead villager died of; every other thing (and a living villager) answers none
		const auto* death = registry.TryGet<const ecs::components::VillagerDeath>(object);
		Pushf(death != nullptr && death->reason.has_value() ? static_cast<float>(*death->reason) : 0.0f);
		return;
	}
	case script::ObjectPropertyType::Angle:
	case script::ObjectPropertyType::XAngle:
	case script::ObjectPropertyType::ZAngle:
	{
		const auto* transform = registry.TryGet<const Transform>(object);
		if (!IsWorldObject(object) || transform == nullptr)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object not an object");
			CannotGetProperty(prop);
			return;
		}
		if (prop != script::ObjectPropertyType::Angle)
		{
			// The thing's kept angles, as it got them. A thing that does not lean has no lean.
			const auto angles = ecs::placed_turn::AnglesOf(registry, object);
			Pushf(script::property_rules::AngleToScript(prop == script::ObjectPropertyType::XAngle ? angles.x : angles.z));
			return;
		}
		if (IsLivingThing(object))
		{
			Pushf(script::property_rules::AngleToScript(LivingAngleOf(object, *transform)));
			return;
		}
		Pushf(script::property_rules::AngleToScript(ecs::placed_turn::AnglesOf(registry, object).y));
		return;
	}
	case script::ObjectPropertyType::Strength:
	{
		if (const auto* creature = registry.TryGet<const ecs::components::Creature>(object); creature != nullptr)
		{
			Pushf(creature->strength);
			return;
		}
		// A miracle's globe or seed: what scales the prayer power and the time of what it casts
		if (const auto* globe = registry.TryGet<const ecs::components::OneOffSpellSeed>(object); globe != nullptr)
		{
			Pushf(globe->multiplier);
			return;
		}
		if (const auto* seed = registry.TryGet<const ecs::components::SpellSeed>(object); seed != nullptr)
		{
			Pushf(seed->castMultiplier);
			return;
		}
		// A miracle that is cast: what multiplies the strength its prayer power gives
		if (const auto* spell = registry.TryGet<const ecs::components::Spell>(object); spell != nullptr)
		{
			Pushf(spell->chants.strengthMultiplier);
			return;
		}
		// Anything else answers its life
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Script-Did you want the strength of this?");
		Pushf(ecs::world_objects::LifeOf(object));
		return;
	}
	case script::ObjectPropertyType::Alignment:
	{
		if (const auto* creature = registry.TryGet<const ecs::components::Creature>(object); creature != nullptr)
		{
			Pushf(creature->alignment);
			return;
		}
		// Anything else its player's, none without a player
		const auto player = ecs::world_objects::PlayerOf(object);
		Pushf(player.has_value() && Locator::alignmentSystem::has_value()
		          ? Locator::alignmentSystem::value().GetPlayerAlignment(*player)
		          : 0.0f);
		return;
	}
	case script::ObjectPropertyType::Height:
		// A puzzle totem answers its height, a whole number
		if (const auto* totem = registry.TryGet<const ecs::components::PuzzleTotem>(object); totem != nullptr)
		{
			Pushf(static_cast<float>(totem->height));
			return;
		}
		// A town's totem answers the share of its people it was last set to send to worship
		if (const auto* totem = registry.TryGet<const ecs::components::VillageTotem>(object); totem != nullptr)
		{
			Pushf(totem->ease.target);
			return;
		}
		Pushf(IsWorldObject(object) ? ScriptHeightOf(object) : 0.0f);
		return;
	case script::ObjectPropertyType::MaxHeight:
		// A puzzle totem keeps a greatest height. Nothing else keeps one.
		if (const auto* totem = registry.TryGet<const ecs::components::PuzzleTotem>(object); totem != nullptr)
		{
			Pushf(static_cast<float>(totem->highest));
			return;
		}
		Pushf(0.0f);
		return;
	case script::ObjectPropertyType::CreatureMinSize:
	case script::ObjectPropertyType::CreatureMaxSize:
	{
		const auto* spells = registry.TryGet<const ecs::components::CreatureSpells>(object);
		const auto* creature = registry.TryGet<const ecs::components::Creature>(object);
		if (spells == nullptr || creature == nullptr)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object not a creature");
			CannotGetProperty(prop);
			return;
		}
		// What the size spells would take it to now: in a fight they go by its size
		const auto inFight =
		    registry.AllOf<ecs::components::CreatureFighting>(object) ? std::optional(creature->size) : std::nullopt;
		Pushf(prop == script::ObjectPropertyType::CreatureMinSize
		          ? creature_spells::SmallestSizeNow(spells->smallestSize, inFight)
		          : creature_spells::LargestSizeNow(spells->largestSize, inFight));
		return;
	}
	case script::ObjectPropertyType::CreatureFatness:
		if (const auto* creature = registry.TryGet<const ecs::components::Creature>(object); creature != nullptr)
		{
			Pushf(creature->fatness);
			return;
		}
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object not a creature");
		CannotGetProperty(prop);
		return;
	case script::ObjectPropertyType::CreatureCanGoThroughVortex:
		// 1 when a script has let it through, else 0
		if (const auto* creature = registry.TryGet<const ecs::components::Creature>(object); creature != nullptr)
		{
			Pushf(creature->mayGoThroughVortex ? 1.0f : 0.0f);
			return;
		}
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object not a creature");
		CannotGetProperty(prop);
		return;
	default:
		// TODO(Daniels118): the other properties
		NotImplemented(static_cast<int32_t>(prop));
		Pushf(0.0f);
		return;
	}
}

static void SetPropertyOf(entt::entity object, script::ObjectPropertyType prop, float value);

void SetProperty() // 022 SET_PROPERTY
{
	const auto value = Popf();
	const auto object = PopObject();
	const auto prop = static_cast<script::ObjectPropertyType>(Pop().intVal);
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ObjectNoLongerValid();
		return;
	}
	// A town, a flock or a dance gives the property to each of its members. Each member takes it as if the script set it
	// on that member. The script does not take control of the members. When a script sets a health of 0 on a town, each
	// villager of the town has no life. Then each villager that no script controls dies at its next check.
	if (IsScriptContainer(registry, object))
	{
		for (const auto member : ContainerMembers(registry, object))
		{
			if (registry.Valid(member))
			{
				SetPropertyOf(member, prop, value);
			}
		}
		return;
	}
	SetPropertyOf(object, prop, value);
}

/// One property of one thing, as a script sets it on that thing
static void SetPropertyOf(entt::entity object, script::ObjectPropertyType prop, float value)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (const auto need = NeedOf(prop); need.has_value())
	{
		auto* needs = CreatureNeedsOf(object);
		if (needs == nullptr)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object not a creature");
			return;
		}
		NeedValue(*needs, *need) = script::property_rules::SetNeed(*need, value);
		return;
	}
	if (prop == script::ObjectPropertyType::YPos)
	{
		// A highlight keeps the height it is given, whatever stands under it
		if (registry.AllOf<ecs::components::ScriptHighlight>(object))
		{
			Locator::scriptHighlightSystem::value().SetDrawHeight(object, value);
		}
		// How high above the land it is: it is drawn there at once
		if (auto* transform = registry.TryGet<Transform>(object); transform != nullptr)
		{
			const auto& at = transform->position;
			const float land =
			    Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(at.x, at.z)) : 0.0f;
			transform->position.y = land + value;
		}
		return;
	}
	switch (prop)
	{
	case script::ObjectPropertyType::CreatureFightHealth:
		// Kept as given, even outside 0 to 1, until a fight starts it afresh
		if (!registry.AllOf<ecs::components::Creature>(object))
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object not a creature");
			CannotSetProperty(prop);
			return;
		}
		if (Locator::creatureFightSystem::has_value())
		{
			Locator::creatureFightSystem::value().SetFightHealth(object, value);
		}
		return;
	case script::ObjectPropertyType::Health:
		if (!IsWorldObject(object))
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object not an object");
			CannotSetProperty(prop);
			return;
		}
		StartCreatureBody(object);
		// A thing a script holds in a cutscene, or made indestructible, can't be brought to nearly no life
		ecs::world_objects::SetLife(
		    object, value,
		    script::property_rules::CanSetLife(value, ecs::world_objects::HeldDuringCutscene(object),
		                                       registry.AllOf<ecs::components::Indestructible>(object)));
		return;
	case script::ObjectPropertyType::Angle:
	case script::ObjectPropertyType::XAngle:
	case script::ObjectPropertyType::ZAngle:
	{
		auto* transform = registry.TryGet<Transform>(object);
		if (!IsWorldObject(object) || transform == nullptr)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object not an object");
			CannotSetProperty(prop);
			return;
		}
		const float radians = script::property_rules::AngleFromScript(value);
		// Only its turn is its own: a lean asked for changes nothing
		if (!CanLean(object) && prop != script::ObjectPropertyType::Angle)
		{
			return;
		}
		if (IsLivingThing(object))
		{
			// Unless it already faces that way
			if (radians != LivingAngleOf(object, *transform))
			{
				TurnLivingThing(object, *transform, radians);
			}
			return;
		}
		// The other angles stay as the thing has them
		auto angles = ecs::placed_turn::AnglesOf(registry, object);
		(prop == script::ObjectPropertyType::Angle    ? angles.y
		 : prop == script::ObjectPropertyType::XAngle ? angles.x
		                                              : angles.z) = radians;
		TurnPlacedThing(object, angles, prop != script::ObjectPropertyType::Angle);
		return;
	}
	case script::ObjectPropertyType::Strength:
		if (auto* creature = registry.TryGet<ecs::components::Creature>(object); creature != nullptr)
		{
			// Its body shows it
			creature->strength = value;
			ecs::archetypes::CreatureArchetype::ShowStrength(object);
			return;
		}
		if (auto* globe = registry.TryGet<ecs::components::OneOffSpellSeed>(object); globe != nullptr)
		{
			globe->multiplier = value;
			return;
		}
		if (auto* seed = registry.TryGet<ecs::components::SpellSeed>(object); seed != nullptr)
		{
			seed->castMultiplier = value;
			return;
		}
		// A miracle that is cast: the value multiplies each strength that it gets after this
		if (auto* spell = registry.TryGet<ecs::components::Spell>(object); spell != nullptr)
		{
			spell->chants.strengthMultiplier = value;
			return;
		}
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Cannot set strength of this");
		CannotSetProperty(prop);
		return;
	case script::ObjectPropertyType::Alignment:
		if (auto* creature = registry.TryGet<ecs::components::Creature>(object); creature != nullptr)
		{
			// Its body takes its looks at once
			creature->alignment = value;
			ecs::archetypes::CreatureArchetype::TakeLooks(registry, object);
			return;
		}
		// Only a creature (or a reward, which openblack doesn't keep an alignment for) takes one
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "UNEXPECTED Alignment change");
		return;
	case script::ObjectPropertyType::Height:
		if (registry.AllOf<ecs::components::VillageTotem>(object))
		{
			// A totem sets its town's share of people at worship
			if (const auto town = TownOfTotem(object); town != entt::null && Locator::villageTotemSystem::has_value())
			{
				Locator::villageTotemSystem::value().SetTownShare(town, value);
			}
			// TODO(script-natives): a totem with no town takes the share itself
			return;
		}
		if (auto* totem = registry.TryGet<ecs::components::PuzzleTotem>(object); totem != nullptr)
		{
			// A puzzle totem takes the height, made whole towards zero, up to its greatest height, and stands where that
			// height puts it
			const auto height = ecs::maze_puzzle::TotemHeight(value, totem->highest);
			if (Locator::puzzleTotemSystem::has_value())
			{
				Locator::puzzleTotemSystem::value().SetHeight(object, height);
			}
			else
			{
				totem->height = height;
			}
			return;
		}
		if (auto* creature = registry.TryGet<ecs::components::Creature>(object); creature != nullptr)
		{
			// It grows or shrinks to that height at once
			creature->size = script::property_rules::CreatureSizeForHeight(value);
			if (auto* transform = registry.TryGet<Transform>(object); transform != nullptr)
			{
				transform->scale = glm::vec3(ecs::archetypes::CreatureArchetype::DrawnScale(creature->species, creature->size));
				registry.SetDirty();
			}
		}
		// Nothing else takes a height
		return;
	case script::ObjectPropertyType::MaxHeight:
		// Nothing keeps a greatest height
		return;
	case script::ObjectPropertyType::CreatureMinSize:
	case script::ObjectPropertyType::CreatureMaxSize:
		if (auto* spells = registry.TryGet<ecs::components::CreatureSpells>(object); spells != nullptr)
		{
			// The size spells take it no further
			(prop == script::ObjectPropertyType::CreatureMinSize ? spells->smallestSize : spells->largestSize) = value;
			return;
		}
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object not a creature");
		CannotSetProperty(prop);
		return;
	case script::ObjectPropertyType::CreatureCanGoThroughVortex:
		// Any value but 0 lets it through
		if (auto* creature = registry.TryGet<ecs::components::Creature>(object); creature != nullptr)
		{
			creature->mayGoThroughVortex = value != 0.0f;
			return;
		}
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object not a creature");
		CannotSetProperty(prop);
		return;
	case script::ObjectPropertyType::CreatureFatness:
		if (auto* creature = registry.TryGet<ecs::components::Creature>(object); creature != nullptr)
		{
			creature->fatness = value;
			return;
		}
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object not a creature");
		CannotSetProperty(prop);
		return;
	case script::ObjectPropertyType::Scale:
		// A creature grows or shrinks to that size at once, its body shown at it (within the sizes a creature can be
		// drawn at) unless it is in its pen
		if (auto* creature = registry.TryGet<ecs::components::Creature>(object); creature != nullptr)
		{
			creature->size = value;
			if (auto* transform = registry.TryGet<Transform>(object); transform != nullptr)
			{
				transform->scale = glm::vec3(
				    ecs::archetypes::CreatureArchetype::DrawnScale(creature->species, ecs::components::ShownSize(*creature)));
				registry.SetDirty();
			}
			return;
		}
		// A thing that holds a visual of a script gives the size to its visual and keeps its own size
		if (const auto effect = ecs::script_spot_visuals::EffectOf(registry, object); effect.has_value())
		{
			if (Locator::effectRequestSystem::has_value())
			{
				Locator::effectRequestSystem::value().SetMagnitude(*effect, value);
			}
			return;
		}
		// The thing is drawn at this size, the same along every axis. A thing in the map's cells that changes its size
		// comes out of them and goes in again.
		if (auto* transform = registry.TryGet<Transform>(object); transform != nullptr)
		{
			const bool resized = transform->scale.x != value;
			transform->scale = glm::vec3(value);
			registry.SetDirty();
			if (resized)
			{
				ecs::thing_map_place::Reinsert(object);
			}
		}
		return;
	case script::ObjectPropertyType::Age:
		// Only living things take an age, in whole years
		if (registry.AllOf<ecs::components::Villager>(object))
		{
			Locator::livingActionSystem::value().VillagerSetAge(object, static_cast<uint32_t>(static_cast<int32_t>(value)));
			return;
		}
		// A creature's body takes the age as it is, and nothing else changes
		if (auto* needs = CreatureNeedsOf(object); needs != nullptr)
		{
			needs->age = static_cast<uint32_t>(static_cast<int32_t>(value));
			return;
		}
		// TODO(opening): the ages of animals; other things take none
		NotImplemented(static_cast<int32_t>(prop));
		return;
	case script::ObjectPropertyType::Speed:
		// A creature is asked to go at that share of its top speed, from none to all of it, until it is next sent
		// somewhere at a pace of its own
		if (auto* locomotion = registry.TryGet<ecs::components::CreatureLocomotion>(object); locomotion != nullptr)
		{
			locomotion->fraction = script::property_rules::CreatureSpeedFraction(value);
			return;
		}
		// A weather thing's storm travels at it, metres a second
		if (Locator::weatherSystem::has_value() && Locator::weatherSystem::value().SetWeatherThingSpeed(object, value))
		{
			return;
		}
		// An animal walks at it, metres a turn
		if (registry.AllOf<ecs::components::Animal>(object))
		{
			if (Locator::animalSystem::has_value())
			{
				Locator::animalSystem::value().SetSpeedFromScript(object, value);
			}
			return;
		}
		// Metres a turn
		if (auto* wallHug = registry.TryGet<ecs::components::WallHug>(object); wallHug != nullptr)
		{
			wallHug->speed = ecs::villager_script_rules::ScriptSpeedToWalkSpeed(value);
		}
		return;
	case script::ObjectPropertyType::BuiltPercentage:
		// A building or a feature is built that far, finished at all of it
		ecs::construction::SetBuilt(object, value);
		return;
	default:
		// TODO(Daniels118): the other properties
		NotImplemented(static_cast<int32_t>(prop));
		return;
	}
}

void GetPosition() // 023 GET_POSITION
{
	// No object, or one that has gone, finds no transform: it is at the origin
	const auto objId = entt::to_integral(PopObject());

	glm::vec3 position(0.0f);
	if (objId != 0)
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto* transform = registry.TryGet<Transform>(static_cast<entt::entity>(objId));
		const auto object = static_cast<entt::entity>(objId);
		if (ecs::script_flocks::IsFlock(registry, object))
		{
			// Where its leader is, or its own place on the ground when it has nobody
			const auto leader = ecs::script_flocks::Leader(registry, object);
			const auto* leaderTransform = leader != entt::null ? registry.TryGet<Transform>(leader) : nullptr;
			if (leaderTransform != nullptr)
			{
				position = leaderTransform->position;
			}
			else
			{
				const auto& place = registry.Get<const ecs::components::Flock>(object).place;
				const auto xz = map_coords::ToMetres(place);
				position = {xz.x, Locator::terrainSystem::value().GetHeightAt(xz) + place.altitude, xz.y};
			}
		}
		else if (const auto* shark = registry.TryGet<const ecs::components::Shark>(static_cast<entt::entity>(objId)))
		{
			// Where it is this turn, not where it is drawn on the way there
			position = shark->position;
		}
		else if (const auto* wallHug = registry.TryGet<const ecs::components::WallHug>(object);
		         wallHug != nullptr && transform != nullptr && !registry.AllOf<ecs::components::Creature>(object) &&
		         ecs::villager_script_rules::WithinAStepOfGoalPlace(glm::vec2(transform->position.x, transform->position.z),
		                                                            wallHug->goalPlace, wallHug->speed))
		{
			// A walker within a turn's step of where it is walking to is seen there, at the height of its goal above the
			// land. An animal keeps the height of its goal with its move.
			const auto goal = ecs::wall_hug::ToPoint(wallHug->goalPlace);
			const auto* animal = registry.TryGet<const ecs::components::Animal>(object);
			const float height = animal != nullptr ? animal->goalHeight : wallHug->goalHeight;
			position = {goal.x, Locator::terrainSystem::value().GetHeightAt(goal) + height, goal.y};
		}
		else if (transform != nullptr)
		{
			position = transform->position;
		}
	}

	PushVec(position);
}

void SetPosition() // 024 SET_POSITION
{
	auto position = PopVec();
	// No object, or one that has gone, finds no transform: nothing moves
	const auto objId = entt::to_integral(PopObject());

	if (objId != 0)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto object = static_cast<entt::entity>(objId);
		// Any thing that is in the hand or flying stays where it is
		const bool inHand = registry.Valid(object) && registry.AllOf<ecs::components::InHand>(object);
		const bool flying = registry.Valid(object) && registry.AllOf<ecs::components::InPhysics>(object);
		if (inHand)
		{
			ScriptMessage("Setting object in hand to a position");
		}
		if (flying)
		{
			ScriptMessage("Trying to set position - Object is flying");
		}
		if (inHand || flying)
		{
			return;
		}
		// A marker is not in the map. It goes to the map position of the point and keeps the height of the point above
		// the land.
		if (registry.Valid(object) && registry.AllOf<ecs::components::ScriptMarker>(object))
		{
			const auto& island = Locator::terrainSystem::value();
			registry.Get<Transform>(object).position = map_coords::ToWorld(island, map_coords::FromWorld(island, position));
			registry.SetDirty();
			return;
		}
		// A thing out of the map's cells stays where it is, as a thing that a creature holds
		if (registry.Valid(object) && Locator::entitiesMap::has_value() && !Locator::entitiesMap::value().InMap(object))
		{
			ScriptMessage("Trying to set position - Object not in map");
			return;
		}
		// The thing goes to the map position of the point, on the land there
		const auto& island = Locator::terrainSystem::value();
		const auto place = map_coords::FromMetres({position.x, position.z});
		const auto placed = map_coords::ToMetres(place);
		position = {placed.x, island.GetHeightAt(placed), placed.y};
		if (IsDirectableVillager(object))
		{
			// Its walk takes up the new place, and one under the script's control stops there and waits for it
			ecs::walker_placement::Place(registry, object, position);
			ecs::thing_map_place::Keep(registry, object, place);
			if (Locator::entitiesMap::has_value())
			{
				Locator::entitiesMap::value().Moved(object);
			}
			if (registry.AllOf<ecs::components::ScriptControlled>(object))
			{
				ecs::walker_placement::Stop(registry, object);
				Locator::livingActionSystem::value().VillagerSetScriptState(object, VillagerStates::InScript);
			}
			registry.SetDirty();
			return;
		}
		// A creature goes there at once, without a walk. Its walk stops, and the plan continues from the new place.
		if (registry.AllOf<ecs::components::Creature>(object) && Locator::creatureLocomotionSystem::has_value())
		{
			Locator::creatureLocomotionSystem::value().PlaceWithoutWalking(object, position);
			ecs::thing_map_place::Keep(registry, object, place);
			if (Locator::entitiesMap::has_value())
			{
				Locator::entitiesMap::value().Moved(object);
			}
			return;
		}
		ecs::thing_map_place::Put(registry, object, place, position.y);
		// The place where the thing was before is now the new place. An animal under the script's control goes back to
		// its script state.
		if (registry.AllOf<ecs::components::Animal>(object))
		{
			ecs::walker_placement::Place(registry, object, position);
			if (registry.AllOf<ecs::components::ScriptControlled>(object) && Locator::animalSystem::has_value())
			{
				Locator::animalSystem::value().SetScriptState(object, LivingStates::LivingInScript);
			}
			registry.SetDirty();
		}
	}
}

void GetDistance() // 025 GET_DISTANCE
{
	const auto p1 = PopVec();
	const auto p0 = PopVec();
	Pushf(ecs::villager_script_rules::ScriptDistance(p0, p1));
}

/// What a "get ... at" (no reach) or "get ... at ... radius" (a reach) asks for
struct ScriptFindRequest
{
	ObjectType type;
	uint32_t subtype;
	map_coords::MapCoords at;
	std::optional<float> radius;
	bool excludingScripted;
	/// Only livings in this state for themselves (the game's number), whatever their distance in the cells looked at.
	/// A living's state is a byte, so a number past 255 is never matched.
	std::optional<uint32_t> finalState;
};

/// The game's number for the state a living thing is in for itself, none for what isn't a villager or an animal
static std::optional<uint8_t> FinalGameStateOf(const ecs::Registry& registry, entt::entity thing)
{
	if (const auto* action = registry.TryGet<const ecs::components::LivingAction>(thing);
	    action != nullptr && registry.AllOf<ecs::components::Villager>(thing) && Locator::livingActionSystem::has_value())
	{
		return static_cast<uint8_t>(Locator::livingActionSystem::value().VillagerGetFinalState(*action));
	}
	if (registry.AllOf<ecs::components::Animal>(thing) && Locator::animalSystem::has_value())
	{
		return Locator::animalSystem::value().FinalGameStateOf(thing);
	}
	return std::nullopt;
}

/// The place from which the scripts' searches measure a thing: the altar of a worship site, else the thing itself
static map_coords::MapCoords ScriptFindPosition(const ecs::Registry& registry, entt::entity entity)
{
	const auto measured = ecs::script_find::MeasuredFrom(registry, entity);
	const auto* transform = registry.TryGet<const Transform>(measured);
	const auto& position = (transform != nullptr ? transform : &registry.Get<const Transform>(entity))->position;
	return map_coords::FromMetres({position.x, position.z});
}

/// The things in a cell of the map that a search asks for
static std::vector<ecs::script_find::Candidate> ScriptFindCandidates(const ScriptFindRequest& request, glm::ivec2 cell)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const std::vector<entt::entity> things = Locator::entitiesMap::value().GetAllInCell(cell);
	std::vector<ecs::script_find::Candidate> found;
	for (const auto thing : things)
	{
		if (!registry.Valid(thing) || !registry.AllOf<Transform>(thing))
		{
			continue;
		}
		// The excluding variants pass over what a script holds already
		if (request.excludingScripted && registry.AllOf<ecs::components::InScript>(thing))
		{
			continue;
		}
		const auto kind = ecs::script_find::KindOf(registry, thing);
		if (!kind.has_value() || !ecs::script_find::Matches(*kind, request.type, request.subtype))
		{
			continue;
		}
		if (request.finalState.has_value())
		{
			const auto state = FinalGameStateOf(registry, thing);
			if (!state.has_value() || static_cast<uint32_t>(*state) != *request.finalState)
			{
				continue;
			}
		}
		const auto at = ScriptFindPosition(registry, thing);
		if (request.radius.has_value() && gutils::GetDistanceInMetres(request.at, at) > *request.radius)
		{
			continue;
		}
		found.push_back({.entity = thing, .at = at});
	}
	return found;
}

/// The town nearest a place strictly within the reach, each player's towns in turn and the neutral ones last
static entt::entity FindScriptTown(const map_coords::MapCoords& at, float reach)
{
	const auto& registry = Locator::entitiesRegistry::value();
	std::vector<ecs::script_find::Candidate> ordered;
	for (const auto town : ecs::player_towns::AllInOrder(registry))
	{
		if (registry.AllOf<Transform>(town))
		{
			ordered.push_back({.entity = town, .at = ScriptFindPosition(registry, town)});
		}
	}
	return ecs::script_find::FindNearestTown(at, reach, ordered);
}

/// The creature a script's search finds, taken into the scripts' table, or none. The search looks at the creatures, the
/// newest first.
static entt::entity FindCreatureForScript(uint32_t subtype, const glm::vec3& position, std::optional<float> radius)
{
	const auto& registry = Locator::entitiesRegistry::value();
	std::vector<std::pair<uint64_t, ecs::script_find::CreatureCandidate>> made;
	registry.Each<const ecs::components::Creature, const ecs::components::LivingListPlace, const Transform>(
	    [&registry, &made](entt::entity entity, const ecs::components::Creature& /*creature*/,
	                       const ecs::components::LivingListPlace& place, const Transform& transform) {
		    const auto kind = ecs::script_find::KindOf(registry, entity);
		    made.emplace_back(place.made, ecs::script_find::CreatureCandidate {
		                                      .entity = entity,
		                                      .at = map_coords::FromMetres({transform.position.x, transform.position.z}),
		                                      .subtype = kind.has_value() ? kind->subtype : std::nullopt,
		                                  });
	    });
	std::ranges::sort(made, std::ranges::greater {}, &decltype(made)::value_type::first);
	std::vector<ecs::script_find::CreatureCandidate> newestFirst;
	newestFirst.reserve(made.size());
	std::ranges::transform(made, std::back_inserter(newestFirst), &decltype(made)::value_type::second);
	const auto found =
	    ecs::script_find::FindCreature(map_coords::FromMetres({position.x, position.z}), radius, subtype, newestFirst);
	if (found != entt::null)
	{
		Locator::scriptObjects::value().Register(found, false);
	}
	return found;
}

/// The thing a script's search finds, taken into the scripts' table, or none. Types with no search (markers, dances,
/// flocks and two unused ones) and types out of range are an error. A creature has a search of its own. A search for
/// livings in a state looks through the cells within the radius but doesn't measure the distance of what it finds there.
static entt::entity FindForScript(int32_t type, uint32_t subtype, const glm::vec3& position, std::optional<float> radius,
                                  bool excludingScripted, std::optional<uint32_t> inState = std::nullopt)
{
	constexpr int32_t k_LastFindType = 41;
	if (type < 1 || type > k_LastFindType)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Script object type {} out of range", type);
		return entt::null;
	}
	const auto objectType = static_cast<ObjectType>(type);
	switch (objectType)
	{
	case ObjectType::Marker:
	case ObjectType::Dance:
	case ObjectType::Flock:
	case ObjectType::InfluenceRing:
	case ObjectType::WeatherThing:
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "No search for script object type {}", type);
		return entt::null;
	case ObjectType::Creature:
		// The search for a creature does not look at the state or at script control: a search for a creature in a
		// state finds the same creature as the search near the place
		return FindCreatureForScript(subtype, position, radius);
	default:
		break;
	}
	const auto at = map_coords::FromMetres({position.x, position.z});
	entt::entity found = entt::null;
	if (objectType == ObjectType::Town)
	{
		found = FindScriptTown(at, radius.value_or(ecs::script_find::k_TownAtReach));
	}
	else
	{
		const ScriptFindRequest request {.type = objectType,
		                                 .subtype = subtype,
		                                 .at = at,
		                                 .radius = inState.has_value() ? std::nullopt : radius,
		                                 .excludingScripted = excludingScripted,
		                                 .finalState = inState};
		found = ecs::script_find::FindNearest(at, radius.value_or(ecs::script_find::k_AtReach),
		                                      [&request](glm::ivec2 cell) { return ScriptFindCandidates(request, cell); });
	}
	if (found == entt::null)
	{
		// Not finding a thing is no mistake: scripts that wait for one to come (the singing stones put in their places)
		// ask again and again. Only the closest tracing says which script asks and for what
		const auto& tasks = Locator::vm::value().GetTasks();
		[[maybe_unused]] const auto task = tasks.find(CurrentTask());
		SPDLOG_LOGGER_TRACE(spdlog::get("scripting"), "Thing not found: type {} subtype {} at ({}, {}) radius {} for {}", type,
		                    subtype, position.x, position.z, radius.value_or(0.0f),
		                    task != tasks.end() ? task->second.name : std::string("no script"));
		return entt::null;
	}
	Locator::scriptObjects::value().Register(found, false);
	return found;
}

void Call() // 026 CALL
{
	// The nearest thing of the type and subtype in the cells within a metre of the place
	const auto excludingScripted = Pop().intVal != 0;
	const auto position = PopVec();
	const auto subtype = Pop().uintVal;
	const auto type = Pop().intVal;
	const auto found = FindForScript(type, subtype, position, std::nullopt, excludingScripted);
	Pusho(found == entt::null ? 0u : static_cast<uint32_t>(found));
}

/// A thing that a script makes goes into the map cells immediately, as the game puts each new thing in the map when it
/// makes it. A script can then make a villager and release it in the same turn: the villager stands in the map and
/// starts its usual tasks again. A thing in a hand or in flight does not go into the cells.
static void FileInMapAtOnce(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object) || !Locator::entitiesMap::has_value() ||
	    registry.AllOf<ecs::components::MapCellResident>(object) ||
	    registry.AnyOf<ecs::components::InHand, ecs::components::InPhysics>(object))
	{
		return;
	}
	Locator::entitiesMap::value().Refile(object);
}

/// CREATE and CREATE_WITH_ANGLE_AND_SCALE: a thing of a type the scripts may create, turned and scaled, which the script
/// then holds; none for any other type, or when nothing could be made
void CreateForScript(int32_t type, int32_t subtype, const glm::vec3& position, float yAngleRadians, float scale)
{
	if (!script::create_rules::IsCreatableType(type))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Invalid type {}", type);
		PushObject(entt::null);
		return;
	}
	auto object = CreateScriptObject(static_cast<ObjectType>(type), static_cast<uint32_t>(subtype), position, 0.0f, 0.0f,
	                                 yAngleRadians, 0.0f, scale);
	if (object == static_cast<entt::entity>(0))
	{
		object = entt::null;
	}
	if (object == entt::null)
	{
		ScriptMessage("Thing not created");
	}
	FileInMapAtOnce(object);
	RegisterCreated(object);
	PushObject(object);
}

void Create() // 027 CREATE
{
	const auto position = PopVec();
	const auto subtype = Pop().intVal;
	const auto type = Pop().intVal;
	CreateForScript(type, subtype, position, 0.0f, 1.0f);
}

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

/// The villagers of a town in the order the scripts look through them: each building's people, the newest building
/// first and each building's newest person first, then the homeless, the newest first
static std::vector<entt::entity> TownVillagersInOrder(const ecs::Registry& registry, entt::entity town)
{
	std::vector<entt::entity> villagers;
	const auto* data = registry.TryGet<const ecs::components::Town>(town);
	if (data == nullptr)
	{
		return villagers;
	}
	for (const auto abode : data->abodes)
	{
		if (const auto* building = registry.Valid(abode) ? registry.TryGet<const ecs::components::Abode>(abode) : nullptr)
		{
			villagers.insert(villagers.end(), building->inhabitants.begin(), building->inhabitants.end());
		}
	}
	villagers.insert(villagers.end(), data->homelessVillagers.begin(), data->homelessVillagers.end());
	return villagers;
}

/// Whether a container a script can look in: a town, a flock or a dance
bool IsScriptContainer(const ecs::Registry& registry, entt::entity thing)
{
	return registry.Valid(thing) && (registry.AllOf<ecs::components::Town>(thing) ||
	                                 ecs::script_flocks::IsFlock(registry, thing) || ecs::dances::IsDance(registry, thing));
}

/// The things in a script container, in the order the scripts look through them
std::vector<entt::entity> ContainerMembers(const ecs::Registry& registry, entt::entity container)
{
	if (ecs::script_flocks::IsFlock(registry, container))
	{
		return registry.Get<const ecs::components::Flock>(container).members;
	}
	if (registry.AllOf<ecs::components::Town>(container))
	{
		return TownVillagersInOrder(registry, container);
	}
	if (ecs::dances::IsDance(registry, container))
	{
		return ecs::dances::Dancers(registry, container);
	}
	return {};
}

void MoveGameThing() // 033 MOVE_GAME_THING
{
	// How near a creature has to come; others go to the point itself
	const auto radius = Popf();
	const auto position = PopVec();
	const auto object = PopObject();
	if (!Locator::entitiesRegistry::value().Valid(object))
	{
		ObjectNoLongerValid();
		return;
	}
	if (IsDirectableVillager(object))
	{
		// One in a hand, in the air or drowning stays where it is. The goal keeps the height of the script's point above
		// the land, so a point with no height gives a goal below the land.
		auto& living = Locator::livingActionSystem::value();
		if (living.VillagerCanBeDirected(object))
		{
			living.VillagerScriptMoveTo(object, map_coords::FromWorld(Locator::terrainSystem::value(), position));
		}
		return;
	}
	if (Locator::entitiesRegistry::value().AllOf<ecs::components::Animal>(object))
	{
		// An animal walks there and is held still for the script, or is held at once when already there
		if (Locator::animalSystem::has_value())
		{
			Locator::animalSystem::value().ScriptMoveTo(object, glm::vec2(position.x, position.z));
		}
		return;
	}
	if (ecs::script_flocks::IsFlock(Locator::entitiesRegistry::value(), object))
	{
		// Its place moves, and the goal of its leader, who isn't sent there
		ecs::script_flocks::MoveTo(Locator::entitiesRegistry::value(), object,
		                           map_coords::FromMetres({position.x, position.z}));
		return;
	}
	if (const auto* creature = Locator::entitiesRegistry::value().TryGet<const ecs::components::Creature>(object);
	    creature != nullptr)
	{
		// One in the map walks there in two goes under the script's control; one in a hand or in the air stays put
		const auto& registry = Locator::entitiesRegistry::value();
		const auto& transform = registry.Get<const Transform>(object);
		const bool inMap =
		    ecs::InMapCells(registry, object) &&
		    !registry.AnyOf<ecs::components::InHand, ecs::components::InPhysics, ecs::components::CarriedByTornado>(object);
		if (inMap && Locator::creatureMindSystem::has_value())
		{
			Locator::creatureMindSystem::value().CarryOutForScript(
			    object, creature_mind::MoveForScript(
			                glm::vec2(transform.position.x, transform.position.z), position,
			                script::property_rules::CreatureHeight(ecs::components::ShownSize(*creature)), radius));
		}
		return;
	}
	// A weather thing's storm sets off for the point at its speed
	if (Locator::weatherSystem::has_value() && Locator::weatherSystem::value().MoveWeatherThing(object, position))
	{
		return;
	}
	// TODO(opening): computer players and other things
	NotImplemented();
}

/// A villager's town: it leaves its old town's homeless, then joins the new town
static void MoveVillagerToTown(entt::entity villager, entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& person = registry.Get<ecs::components::Villager>(villager);
	if (auto* old = registry.Valid(person.town) ? registry.TryGet<ecs::components::Town>(person.town) : nullptr)
	{
		std::erase(old->homelessVillagers, villager);
	}
	static_cast<void>(Locator::townSystem::value().AddVillagerToTown(town, villager));
}

/// A living a script puts in a flock keeps up with it from then on
static void SetFlockState(entt::entity living)
{
	if (IsDirectableVillager(living))
	{
		Locator::livingActionSystem::value().VillagerSetScriptState(living, VillagerStates::MoveInFlock);
	}
	else if (Locator::animalSystem::has_value() && Locator::entitiesRegistry::value().AllOf<ecs::components::Animal>(living))
	{
		Locator::animalSystem::value().SetScriptState(living, LivingStates::LivingMoveInFlock);
	}
}

/// Whether a thing is a living a flock can take: a villager or an animal
static bool IsFlockLiving(const ecs::Registry& registry, entt::entity thing)
{
	return registry.Valid(thing) && registry.AnyOf<ecs::components::Villager, ecs::components::Animal>(thing);
}

void FlockCreate() // 036 FLOCK_CREATE
{
	const auto position = PopVec();
	auto& registry = Locator::entitiesRegistry::value();
	// Nobody owns a script's flock: it belongs to the neutral player
	const auto flock = ecs::script_flocks::Create(registry, map_coords::FromMetres({position.x, position.z}));
	RegisterCreated(flock);
	Pusho(static_cast<uint32_t>(flock));
}

/// Puts a living, or each member of a flock, into a flock
static void FlockAttachToFlock(entt::entity object, entt::entity flock, bool asLeader)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& scripts = Locator::scriptObjects::value();
	if (ecs::script_flocks::IsFlock(registry, object))
	{
		// The flock takes in every member of the other, which goes once it is empty
		while (ecs::script_flocks::Size(registry, object) > 0)
		{
			ecs::script_flocks::AddLiving(registry, flock, registry.Get<const ecs::components::Flock>(object).members.front());
		}
		Pusho(static_cast<uint32_t>(flock));
		return;
	}
	if (!IsFlockLiving(registry, object))
	{
		ScriptMessage("JONTY-Trying to add non living to flock");
		ScriptMessage("Thing not added to Flock");
		Pusho(0);
		return;
	}
	if (asLeader)
	{
		ecs::script_flocks::AddLeader(registry, flock, object);
	}
	else if (!ecs::script_flocks::AddLiving(registry, flock, object))
	{
		ScriptMessage("Living already in Flock");
	}
	SetFlockState(object);
	Pusho(static_cast<uint32_t>(flock));
	scripts.AddReference(object);
}

/// A villager, or each villager of a container, joins a town
static void FlockAttachToTown(entt::entity object, entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (IsScriptContainer(registry, object))
	{
		// The script takes control of each member
		for (const auto member : ContainerMembers(registry, object))
		{
			TakeControlInLoop(registry, member);
			if (registry.Valid(member) && registry.AllOf<ecs::components::Villager>(member))
			{
				MoveVillagerToTown(member, town);
				continue;
			}
			// A member that is not a villager stays where it is. It puts no value on the script's stack (deviation 178).
			ScriptMessage("JONTY-Trying to add non villager to town");
		}
		Pusho(static_cast<uint32_t>(town));
		return;
	}
	if (!registry.AllOf<ecs::components::Villager>(object))
	{
		// TODO(opening): a spell dispenser given to a town is the town's from then on
		ScriptMessage("JONTY-Trying to add non villager to town");
		Pusho(0);
		return;
	}
	// It stays in any flock it is in, doing what it was doing
	MoveVillagerToTown(object, town);
	Pusho(static_cast<uint32_t>(town));
}

/// A living joins a dance, dancing in the group whose turn it is that takes it, or standing in the dance in none
static void JoinDance(entt::entity living, entt::entity dance, bool asCentre)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (asCentre)
	{
		registry.Get<ecs::components::Dance>(dance).owner = living;
	}
	// One already dancing leaves its dance first
	ecs::dances::RemoveDancer(registry, living);
	const auto sex = registry.AllOf<ecs::components::Villager>(living) ? ecs::villager_dance::DanceSex(living)
	                                                                   : ecs::components::DanceGroup::k_AnySex;
	static_cast<void>(ecs::dances::AddDancer(registry, dance, living, sex));
	if (IsDirectableVillager(living))
	{
		Locator::livingActionSystem::value().VillagerSetScriptState(living, VillagerStates::InDance);
	}
}

/// A living, or each villager of a container, joins a dance
static void FlockAttachToDance(entt::entity object, entt::entity dance, bool asCentre)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& scripts = Locator::scriptObjects::value();
	if (IsScriptContainer(registry, object))
	{
		// The script takes control of each member
		for (const auto member : ContainerMembers(registry, object))
		{
			TakeControlInLoop(registry, member);
			if (IsFlockLiving(registry, member))
			{
				JoinDance(member, dance, asCentre);
				scripts.AddReference(member);
			}
		}
		Pusho(static_cast<uint32_t>(dance));
		return;
	}
	if (!IsFlockLiving(registry, object))
	{
		ScriptMessage("JONTY-Trying to add non living to dance");
		Pusho(0);
		return;
	}
	JoinDance(object, dance, asCentre);
	scripts.AddReference(object);
	Pusho(static_cast<uint32_t>(dance));
}

void FlockAttach() // 037 FLOCK_ATTACH
{
	const auto asLeader = Pop().intVal != 0;
	const auto target = PopObject();
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ScriptMessage("Id deleted to attach");
		Pusho(0);
		return;
	}
	if (target == entt::null || !registry.Valid(target))
	{
		ScriptMessage("Id deleted to attach to");
		Pusho(0);
		return;
	}
	// The type of the target sets the operation: first a dance, then a flock, then a town
	if (ecs::dances::IsDance(registry, target))
	{
		registry.AssignOrReplace<ecs::components::ScriptControlled>(target);
		FlockAttachToDance(object, target, asLeader);
		return;
	}
	if (ecs::script_flocks::IsFlock(registry, target))
	{
		FlockAttachToFlock(object, target, asLeader);
		return;
	}
	if (registry.AllOf<ecs::components::Town>(target))
	{
		FlockAttachToTown(object, target);
		return;
	}
	ScriptMessage("Thing not added to id");
	Pusho(0);
}

/// A living leaves a flock, keeping its state, the flock staying when it is left empty
/// A member that a script takes out of a flock: an animal goes into a new flock of its own, anything else is in no flock
/// after it. The flock stays, also when it is empty.
static void LeaveFlockForScript(ecs::Registry& registry, entt::entity member)
{
	if (registry.AllOf<ecs::components::Animal>(member))
	{
		ecs::script_flocks::SeparateIntoNewFlock(registry, member);
		return;
	}
	ecs::script_flocks::Remove(registry, member, false);
}

static void DetachFromFlock(uint32_t objectId, entt::entity object, entt::entity flock)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& scripts = Locator::scriptObjects::value();
	if (objectId != 0)
	{
		if (!IsFlockLiving(registry, object))
		{
			ScriptMessage("Trying to remove non living from flock");
			ScriptMessage("Thing not removed from Flock");
			Pusho(0);
			return;
		}
		LeaveFlockForScript(registry, object);
		scripts.RemoveReference(object);
		Pusho(objectId);
		return;
	}
	// A member at random, not the leader when there are others
	const auto member = ecs::script_flocks::MemberToDetach(registry, flock,
	                                                       [](uint32_t n) { return Locator::gameRandom::value().GameRand(n); });
	if (member == entt::null)
	{
		ScriptMessage("Thing not found from Flock");
		ScriptMessage("Thing not removed from Flock");
		Pusho(0);
		return;
	}
	LeaveFlockForScript(registry, member);
	scripts.Register(member, false);
	scripts.RemoveReference(member);
	Pusho(static_cast<uint32_t>(member));
}

/// A dancer leaves a dance: the one given, or with none given the first of its groups', passing over what the dance is
/// danced about while others dance
static void DetachFromDance(uint32_t objectId, entt::entity object, entt::entity dance)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& scripts = Locator::scriptObjects::value();
	if (objectId != 0)
	{
		if (!IsFlockLiving(registry, object))
		{
			ScriptMessage("Trying to remove non living from dance");
			ScriptMessage("Thing not removed from Dance");
			Pusho(0);
			return;
		}
		ecs::dances::RemoveDancer(registry, object);
		scripts.RemoveReference(object);
		Pusho(objectId);
		return;
	}
	const auto& data = registry.Get<const ecs::components::Dance>(dance);
	const auto exclude = data.dancers == 1 ? entt::null : data.owner;
	const auto dancer = ecs::dances::FirstDancer(registry, dance, exclude);
	if (dancer == entt::null)
	{
		ScriptMessage("Thing not removed from Dance");
		Pusho(0);
		return;
	}
	ecs::dances::RemoveDancer(registry, dancer);
	scripts.Register(dancer, false);
	Pusho(static_cast<uint32_t>(dancer));
	scripts.RemoveReference(dancer);
}

void FlockDetach() // 038 FLOCK_DETACH
{
	const auto container = PopObject();
	const auto objectId = Pop().uintVal;
	auto& registry = Locator::entitiesRegistry::value();
	if (container == entt::null || !registry.Valid(container))
	{
		// The game pushes nothing here
		ScriptMessage("From thing dead!");
		return;
	}
	const auto object = objectId != 0 ? Locator::scriptObjects::value().Fetch(static_cast<entt::entity>(objectId)) : entt::null;
	if (ecs::script_flocks::IsFlock(registry, container))
	{
		DetachFromFlock(objectId, object, container);
		return;
	}
	if (registry.AllOf<ecs::components::Town>(container))
	{
		const auto* person = registry.Valid(object) ? registry.TryGet<const ecs::components::Villager>(object) : nullptr;
		if (person != nullptr && person->town == container)
		{
			// The villager leaves the town altogether, and the script is given nothing back
			Locator::townSystem::value().RemoveVillager(object);
			Pusho(0);
			return;
		}
		if (person != nullptr)
		{
			ScriptMessage("Wrong Town");
		}
		ScriptMessage("Thing not removed from Town");
		Pusho(0);
		return;
	}
	if (IsFlockLiving(registry, container))
	{
		const auto flock = ecs::script_flocks::FlockOf(registry, container);
		if (flock == entt::null)
		{
			ScriptMessage("No flock for living so what the...");
			Pusho(0);
			return;
		}
		DetachFromFlock(objectId, object, flock);
		return;
	}
	if (ecs::dances::IsDance(registry, container))
	{
		DetachFromDance(objectId, object, container);
		return;
	}
	ScriptMessage("Not living - confused");
	Pusho(0);
}

void FlockDisband() // 039 FLOCK_DISBAND
{
	const auto container = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (container == entt::null || !registry.Valid(container))
	{
		ScriptMessage("Disbanding a NULL object");
		ScriptMessage("Bad Id for Disband");
		return;
	}
	// Every member of a flock or dance leaves it, and one the script controls waits for it; the flock or dance stays,
	// empty. A town or an abode keeps its own
	if (Locator::scriptObjects::value().Disband(container) || registry.AllOf<ecs::components::Abode>(container))
	{
		return;
	}
	ScriptMessage("Bad Id for Disband");
}

void IdSize() // 040 ID_SIZE
{
	const auto container = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (container != entt::null && ecs::script_flocks::IsFlock(registry, container))
	{
		Pushf(static_cast<float>(ecs::script_flocks::Size(registry, container)));
		return;
	}
	if (container != entt::null && registry.Valid(container) && registry.AllOf<ecs::components::Town>(container))
	{
		// Its grown-ups and its children
		uint32_t people = 0;
		registry.Each<const ecs::components::Villager>(
		    [&people, container](const ecs::components::Villager& person) { people += person.town == container ? 1 : 0; });
		Pushf(static_cast<float>(people));
		return;
	}
	if (container != entt::null && ecs::dances::IsDance(registry, container))
	{
		Pushf(static_cast<float>(ecs::dances::Size(registry, container)));
		return;
	}
	// TODO(opening): footballs
	ScriptMessage("Cannot Find Flock/Dance/Town Size");
	Pushf(0.0f);
}

void FlockMember() // 041 FLOCK_MEMBER
{
	const auto flock = PopObject();
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (!ecs::script_flocks::IsFlock(registry, flock) || !IsFlockLiving(registry, object))
	{
		ScriptMessage("Invalid Flock or Thing");
		Pushb(false);
		return;
	}
	Pushb(ecs::script_flocks::IsMember(registry, flock, object));
}

void GetHandPosition() // 042 GET_HAND_POSITION
{
	// The hand's position is given also in the temple, with a message
	if (PlayerInsideTemple())
	{
		ScriptMessage("Hand in citadel");
	}
	const auto reported = Locator::handSystem::value().GetPlayerHandPositions()[0];
	PushVec(reported.value_or(glm::vec3(0.0f)));
}

/// A thing a script deletes goes at once: a creature leaves the game, a dance's dancers go back to deciding what to do,
/// a flock's members are in no flock any more
static void DeleteNow(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<ecs::components::Creature>(object))
	{
		static_cast<void>(ecs::creature_removal::RemoveFromGame(object));
	}
	else if (ecs::dances::IsDance(registry, object))
	{
		Locator::danceSystem::value().Destroy(object);
	}
	else if (ecs::script_flocks::IsFlock(registry, object))
	{
		ecs::script_flocks::Destroy(registry, object);
	}
	else
	{
		// A shield miracle takes its shield with it at once
		if (const auto* spell = registry.TryGet<const ecs::components::Spell>(object);
		    spell != nullptr && spell->spellClass == magic::SpellClass::Shield)
		{
			if (Locator::magicShieldSystem::has_value())
			{
				Locator::magicShieldSystem::value().RemoveWithSpell(object);
			}
			if (Locator::magicSystem::has_value())
			{
				Locator::magicSystem::value().CloseDown(object);
			}
		}
		ecs::world_objects::Remove(object);
	}
}

void ObjectDelete() // 048 OBJECT_DELETE
{
	namespace script_delete = ecs::script_delete;
	const auto mode = Pop().intVal;
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return;
	}
	const auto* temple = registry.TryGet<const ecs::components::Temple>(object);
	const auto step = script_delete::Choose(mode, {.puzzleGame = registry.AllOf<ecs::components::PuzzleGame>(object),
	                                               .object = IsScriptObject(registry, object),
	                                               .creature = registry.AllOf<ecs::components::Creature>(object),
	                                               .temple = temple != nullptr,
	                                               .templeDestroying = temple != nullptr && temple->destroying});
	const auto* transform = registry.TryGet<const Transform>(object);
	const auto position = transform != nullptr ? transform->position : glm::vec3(0.0f);
	switch (step)
	{
	case script_delete::Step::Remove:
		DeleteNow(object);
		return;
	case script_delete::Step::Fizz:
		Locator::creatureFizzSystem::value().SetFizz(object, 1.0f, 2.0f, true);
		break;
	case script_delete::Step::GhostThenRemove:
		ecs::world_objects::LeaveGhost(object);
		DeleteNow(object);
		return;
	case script_delete::Step::BreakUpThenRemove:
		// The pieces fly out from the place where it stood
		Locator::particleRuleSystem::value().ExplodeObject(object, position, script_delete::k_ExplodeSpeed);
		DeleteNow(object);
		return;
	case script_delete::Step::StartTempleDestruction:
		if (Locator::templeDestructionSystem::has_value())
		{
			Locator::templeDestructionSystem::value().Start(object);
		}
		break;
	case script_delete::Step::BreakUpTemple:
		// The temple stays until its destruction ends
		Locator::particleRuleSystem::value().ExplodeObject(object, position, script_delete::k_TempleExplodeSpeed);
		break;
	case script_delete::Step::ClearPuzzle:
		// The puzzle game stays, but its pieces go
		if (Locator::puzzleGameSystem::has_value())
		{
			static_cast<void>(Locator::puzzleGameSystem::value().DeletePieces(object));
		}
		break;
	case script_delete::Step::LetGo:
		break;
	}
	// The script lets go of the thing. A flock or a dance that stays loses its members
	if (registry.Valid(object))
	{
		static_cast<void>(Locator::scriptObjects::value().Disband(object));
	}
}

void CallNear() // 051 CALL_NEAR
{
	// The nearest thing of the type and subtype within the radius of the place
	const auto excludingScripted = Pop().intVal != 0;
	const auto radius = Popf();
	const auto position = PopVec();
	const auto subtype = Pop().uintVal;
	const auto type = Pop().intVal;
	const auto found = FindForScript(type, subtype, position, radius, excludingScripted);
	Pusho(found == entt::null ? 0u : static_cast<uint32_t>(found));
}

void SpecialEffectPosition() // 052 SPECIAL_EFFECT_POSITION
{
	const auto turns = SpecialEffectTurns(Popf());
	const auto position = PopVec();
	const auto effect = Pop().intVal;
	PushObject(ScriptSpotVisualThing(StartScriptSpotVisual(effect, position, turns, entt::null), position));
}

void SpecialEffectObject() // 053 SPECIAL_EFFECT_OBJECT
{
	const auto turns = SpecialEffectTurns(Popf());
	const auto object = PopObject();
	const auto effect = Pop().intVal;
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.Valid(object) ? registry.TryGet<Transform>(object) : nullptr;
	if (transform == nullptr)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SPECIAL_EFFECT_OBJECT: thing invalid, spell not created");
		PushObject(entt::null);
		return;
	}
	// The effect stands on the object and ends when it goes
	const auto position = transform->position;
	PushObject(ScriptSpotVisualThing(StartScriptSpotVisual(effect, position, turns, object), position));
}

void DanceCreate() // 054 DANCE_CREATE
{
	const auto duration = Popf();
	const auto position = PopVec();
	const auto type = Pop().intVal;
	const auto centre = PopObject();
	// What it is danced about may be nothing
	const auto dance = Locator::danceSystem::value().Create(
	    static_cast<DanceInfo>(type), position, centre, static_cast<uint32_t>(map_coords::FtoL(duration)), true, entt::null);
	if (dance == entt::null)
	{
		ScriptMessage("Dance not created");
		Pusho(0);
		return;
	}
	RegisterCreated(dance);
	Pusho(static_cast<uint32_t>(dance));
}

void CallIn() // 055 CALL_IN
{
	const auto excludingScripted = Pop().intVal != 0;
	const auto container = PopObject();
	const auto subtype = Pop().uintVal;
	const auto type = Pop().intVal;
	auto& registry = Locator::entitiesRegistry::value();
	if (container == entt::null || !registry.Valid(container))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "Cannot find in");
		Pusho(0);
		return;
	}
	if (!IsScriptContainer(registry, container))
	{
		ScriptMessage("Cannot look in object");
		ScriptMessage("Cannot find in");
		Pusho(0);
		return;
	}
	// TODO(opening): a town's animals and its storage pit
	const auto objectType = static_cast<ObjectType>(type);
	if (registry.AllOf<ecs::components::Town>(container) && objectType != ObjectType::Villager &&
	    objectType != ObjectType::VillagerChild)
	{
		ScriptMessage("Looking for strange type in Town");
		ScriptMessage("Cannot find in");
		Pusho(0);
		return;
	}
	// The first of its things of the type and subtype, passing over those a script holds when asked to
	for (const auto member : ContainerMembers(registry, container))
	{
		if (!registry.Valid(member) || (excludingScripted && registry.AllOf<ecs::components::InScript>(member)))
		{
			continue;
		}
		const auto kind = ecs::script_find::KindOf(registry, member);
		if (kind.has_value() && ecs::script_find::Matches(*kind, objectType, subtype))
		{
			Locator::scriptObjects::value().Register(member, false);
			Pusho(static_cast<uint32_t>(member));
			return;
		}
	}
	ScriptMessage("Cannot find in");
	Pusho(0);
}

void ChangeInnerOuterProperties() // 056 CHANGE_INNER_OUTER_PROPERTIES
{
	const auto calm = Popf();
	const auto outer = Popf();
	const auto inner = Popf();
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (object != entt::null && ecs::script_flocks::IsFlock(registry, object))
	{
		// A nought leaves that distance as it was; the calm is kept whatever it is
		auto& flock = registry.Get<ecs::components::Flock>(object);
		if (outer != 0.0f)
		{
			flock.domainRadius = static_cast<uint16_t>(map_coords::FtoL(outer));
		}
		if (inner != 0.0f)
		{
			flock.flockDistance = static_cast<uint16_t>(map_coords::FtoL(inner));
		}
		flock.calm = map_coords::FtoL(calm);
		return;
	}
	// A weather thing's storm takes the inner and outer distances as they are; its calm is not used
	if (object != entt::null && Locator::weatherSystem::has_value() &&
	    Locator::weatherSystem::value().ChangeWeatherThing(
	        object, [=](ecs::components::Storm& storm) { ecs::script_weather::SetInnerOuter(storm, inner, outer); }))
	{
		return;
	}
	ScriptMessage("Invalid thing for Changing Variables");
}

/// What a script gave for a challenge's record, taken off its stack: every value, however many arguments it gave its
/// reminder script, so that nothing is left behind for the script's next calls
script::challenge_snapshots::Snapshot PopChallengeSnapshot(script::challenge_snapshots::Call call)
{
	auto& lhvm = Locator::vm::value();
	const auto snapshot =
	    script::challenge_snapshots::Read(call, {
	                                                .pop =
	                                                    [&lhvm]() {
		                                                    script::challenge_snapshots::Value value;
		                                                    value.value = lhvm.Pop(value.type);
		                                                    return value;
	                                                    },
	                                                .text = [&lhvm](uint32_t offset) { return lhvm.GetString(offset); },
	                                            });
	if (snapshot.tooManyArguments)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Too many arguments for a challenge's reminder script: {}",
		                    snapshot.arguments.size());
	}
	return snapshot;
}

/// The player's record of a challenge is kept up to date from a script's snapshot, and a snapshot of a challenge already
/// recorded moves the first player's alignment
void KeepChallengeRecord(const script::challenge_snapshots::Snapshot& snapshot, script::challenge_snapshots::Call call)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto turn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
	const ecs::challenge_records::References references {
	    .add =
	        [](entt::entity thing) {
		        if (Locator::scriptObjects::has_value())
		        {
			        Locator::scriptObjects::value().AddReference(thing);
		        }
	        },
	    .remove =
	        [](entt::entity thing) {
		        if (Locator::scriptObjects::has_value())
		        {
			        Locator::scriptObjects::value().RemoveReference(thing);
		        }
	        },
	};
	const auto outcome = ecs::challenge_records::Record(registry, snapshot, call, turn, references);
	if (outcome.takesPicture && Locator::rendererRequests::has_value())
	{
		// The picture is of the screen as the player sees it now
		auto* picture = registry.TryGet<ecs::components::ChallengePicture>(outcome.record);
		if (picture == nullptr)
		{
			picture = &registry.Assign<ecs::components::ChallengePicture>(outcome.record);
		}
		// The renderer takes the picture at the end of a draw, so the request goes to the main thread
		PostToMainThread(MainThreadApi::Renderer, [name = ecs::challenge_pictures::Ask(*picture, outcome.record)] {
			Locator::rendererRequests::value().RequestScreenPicture(name, &ecs::challenge_pictures::Take);
		});
	}
	if (!outcome.alignment.has_value() || !Locator::alignmentSystem::has_value() || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto player =
	    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
	auto& alignment = Locator::alignmentSystem::value();
	const auto change =
	    ecs::challenge_records::AlignmentChange(alignment.GetPlayerAlignment(player), *outcome.alignment,
	                                            Locator::infoConstants::value().player.maxScriptAlignmentChange);
	// The totals of the alignment changes count the change before the alignment takes it
	alignment.AddToHistory(player, ecs::components::AlignmentCause::Script, change);
	alignment.AddPlayerAlignment(player, change);
}

void CHLApi::RecordChallenge(const script::challenge_snapshots::Snapshot& snapshot, script::challenge_snapshots::Call call)
{
	KeepChallengeRecord(snapshot, call);
}

void Snapshot() // 057 SNAPSHOT
{
	KeepChallengeRecord(PopChallengeSnapshot(script::challenge_snapshots::Call::Start),
	                    script::challenge_snapshots::Call::Start);
}

/// The player that these natives name by its place in the game's player list, from 0. None past the last player.
std::optional<PlayerNames> ScriptPlayerByIndex(uint32_t index)
{
	if (index >= ecs::game_players::Count())
	{
		return std::nullopt;
	}
	return static_cast<PlayerNames>(index);
}

void GetAlignment() // 058 GET_ALIGNMENT
{
	const auto player = ScriptPlayerByIndex(Pop().uintVal);
	// The game has no player past the last one and stops; here the script gets a neutral alignment
	if (!player.has_value() || !Locator::alignmentSystem::has_value())
	{
		Pushf(0.0f);
		return;
	}
	Pushf(Locator::alignmentSystem::value().GetPlayerAlignment(*player));
}

void SetAlignment() // 059 SET_ALIGNMENT
{
	const auto player = ScriptPlayerByIndex(Pop().uintVal);
	const auto change = Pop().floatVal;
	// In spite of its name, the native adds the amount to the alignment at once
	if (!ecs::systems::alignment::ScriptChangeInRange(change))
	{
		ScriptMessage("Alignment out of range");
		return;
	}
	// The game has no player past the last one and stops; here nothing changes
	if (!player.has_value() || !Locator::alignmentSystem::has_value())
	{
		return;
	}
	Locator::alignmentSystem::value().AddPlayerAlignment(*player, change);
}

/// An influence a script makes for a player, at a place or going about with an object: within it the player has
/// influence, or none at all for an anti-influence. It takes its place in the scripts' table as made by a script; none for
/// a player there isn't.
static entt::entity CreateScriptInfluence(uint32_t player, float radius, bool anti, glm::vec3 position, entt::entity follows)
{
	if (player >= ecs::game_players::Count())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Could not make influence ring!");
		return entt::null;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto ring = registry.Create();
	registry.Assign<Transform>(ring, position, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<ecs::components::InfluenceSource>(ring, static_cast<PlayerNames>(player), radius, anti, follows);
	RegisterCreated(ring);
	return ring;
}

void InfluenceObject() // 060 INFLUENCE_OBJECT
{
	const auto anti = Pop().intVal != 0;
	// The game's own player number, not the scripts'
	const auto player = Pop().uintVal;
	const auto radius = Popf();
	const auto target = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = target != entt::null && registry.Valid(target) ? registry.TryGet<Transform>(target) : nullptr;
	if (transform == nullptr)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "thing not valid");
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Could not make influence ring!");
		PushObject(entt::null);
		return;
	}
	PushObject(CreateScriptInfluence(player, radius, anti, transform->position, target));
}

void InfluencePosition() // 061 INFLUENCE_POSITION
{
	const auto anti = Pop().intVal != 0;
	const auto player = Pop().uintVal;
	const auto radius = Popf();
	const auto position = PopVec();
	PushObject(CreateScriptInfluence(player, radius, anti, position, entt::null));
}

void GetInfluence() // 062 GET_INFLUENCE
{
	const auto position = PopVec();
	const auto raw = Pop().intVal != 0;
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	// A player who isn't in the game has no influence anywhere
	std::optional<float> influence;
	if (ScriptNamesPlayer(player) && Locator::playerSystem::has_value() && Locator::influenceSystem::has_value() &&
	    Locator::entitiesRegistry::value().Valid(Locator::playerSystem::value().FindPlayer(player).value_or(entt::null)))
	{
		influence =
		    Locator::influenceSystem::value().PlayerOwnInfluence(player, map_coords::FromMetres({position.x, position.z}));
	}
	// Where the player has none, the allies that lend their influence to the player lend it
	std::vector<float> lent;
	if (influence.has_value())
	{
		lent = ecs::ally_influence::LentInfluence(player, map_coords::FromMetres({position.x, position.z}));
	}
	Pushf(script::influence::Answer(influence, raw, lent));
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

/// The first thing of a type and subtype in a container, inside the radius of a place or outside it. The game uses
/// the same search for a town or a flock in the two cases, and only the distance test changes.
static void CallInByDistance(bool near)
{
	const auto excludingScripted = Pop().intVal != 0;
	const auto radius = Popf();
	const auto position = PopVec();
	const auto container = PopObject();
	const auto subtype = Pop().uintVal;
	const auto type = Pop().intVal;
	auto& registry = Locator::entitiesRegistry::value();
	// Not finding one is no mistake: the Pied Piper asks every few seconds for a child near him. Only the closest tracing
	// tells of it
	const auto notFound = [=]() {
		SPDLOG_LOGGER_TRACE(spdlog::get("scripting"), "Cannot find in Near. {}.{}", type, subtype);
		Pusho(0);
	};
	if (container == entt::null || !registry.Valid(container))
	{
		notFound();
		return;
	}
	if (!IsScriptContainer(registry, container))
	{
		ScriptMessage("Cannot look in object");
		notFound();
		return;
	}
	const auto objectType = static_cast<ObjectType>(type);
	if (registry.AllOf<ecs::components::Town>(container))
	{
		if (objectType == ObjectType::Animal)
		{
			// TODO(town-script-natives): a town's animals, its flocks' in the order it took them; no script asks for them
			notFound();
			return;
		}
		if (objectType != ObjectType::Villager && objectType != ObjectType::VillagerChild)
		{
			ScriptMessage("Looking for strange type in Town");
			notFound();
			return;
		}
	}
	// The first of its things of the type and subtype on the correct side of the radius, passing over those a script
	// holds when asked to
	const auto at = map_coords::FromMetres({position.x, position.z});
	for (const auto member : ContainerMembers(registry, container))
	{
		if (!registry.Valid(member) || !registry.AllOf<Transform>(member) ||
		    (excludingScripted && registry.AllOf<ecs::components::InScript>(member)))
		{
			continue;
		}
		const auto kind = ecs::script_find::KindOf(registry, member);
		if (!kind.has_value() || !ecs::script_find::Matches(*kind, objectType, subtype))
		{
			continue;
		}
		const float distance = gutils::GetDistanceInMetres(at, ScriptFindPosition(registry, member));
		if (near ? distance > radius : !ecs::script_containers::IsNotNear(distance, radius))
		{
			continue;
		}
		Locator::scriptObjects::value().Register(member, false);
		Pusho(static_cast<uint32_t>(member));
		return;
	}
	notFound();
}

void CallInNear() // 067 CALL_IN_NEAR
{
	CallInByDistance(true);
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

void GetObjectDropped() // 088 GET_OBJECT_DROPPED
{
	// The last thing a creature let go of, none once it has gone; asked only of creatures
	const auto creature = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* dropped = registry.Valid(creature) && registry.AllOf<ecs::components::Creature>(creature)
	                          ? registry.TryGet<const ecs::components::CreatureDroppedObject>(creature)
	                          : nullptr;
	if (registry.Valid(creature) && !registry.AllOf<ecs::components::Creature>(creature))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GET_OBJECT_DROPPED: only creatures drop things");
	}
	const bool exists = dropped != nullptr && dropped->object != entt::null && registry.Valid(dropped->object);
	Pusho(exists ? static_cast<uint32_t>(dropped->object) : 0);
}

void ClearDroppedByObject() // 089 CLEAR_DROPPED_BY_OBJECT
{
	// The creature forgets what it last let go of
	const auto creature = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.Valid(creature) && registry.AllOf<ecs::components::CreatureDroppedObject>(creature))
	{
		registry.Remove<ecs::components::CreatureDroppedObject>(creature);
	}
}

void CreateReaction() // 090 CREATE_REACTION
{
	// The living around the thing react to it as to the kind of reaction given (the kind is taken as a byte, its
	// last number meaning none). It belongs to no player, so it impresses no one, and its time starts when the first of
	// them takes it up.
	const auto type = static_cast<uint8_t>(Pop().intVal);
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ScriptMessage("no reaction for thing");
		return;
	}
	const auto* transform = registry.TryGet<const Transform>(object);
	if (type == std::numeric_limits<uint8_t>::max() || transform == nullptr || !Locator::reactionSystem::has_value())
	{
		return;
	}
	Locator::reactionSystem::value().Create({.initiator = object,
	                                         .type = static_cast<Reaction>(type),
	                                         .player = PlayerNames::NEUTRAL,
	                                         .position = transform->position,
	                                         .playerless = true});
}

void RemoveReaction() // 091 REMOVE_REACTION
{
	// The reactions that the thing started stop. A group stops the reactions of each of its members, not its own.
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ScriptMessage("no reaction for thing");
		return;
	}
	if (!Locator::reactionSystem::has_value())
	{
		return;
	}
	auto& reactions = Locator::reactionSystem::value();
	if (IsScriptContainer(registry, object))
	{
		for (const auto member : ContainerMembers(registry, object))
		{
			reactions.RemoveFrom(member);
		}
		return;
	}
	reactions.RemoveFrom(object);
}

void GetCountdownTimer() // 092 GET_COUNTDOWN_TIMER
{
	// The whole seconds left, also after the timer stops or when it never started
	Pushf(ScriptCountdown().GetSecondsLeft());
}

void GetTargetObject() // 097 GET_TARGET_OBJECT
{
	// The thing that the creature's plan acts on. After the plan stops, the native gives the thing of the last plan
	// until the creature starts a new plan. The native gives 0 when there is no such thing, or when the thing is gone.
	// When the script gives no object, the native puts nothing on the stack, as the game does.
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ScriptMessage("no object");
		return;
	}
	if (!registry.AllOf<ecs::components::Creature>(object))
	{
		ScriptMessage("Only for creatures");
		PushObject(entt::null);
		return;
	}
	const auto* mind = registry.TryGet<const CreatureMindState>(object);
	std::optional<uint32_t> target;
	if (mind != nullptr)
	{
		target = mind->planner.current.has_value() ? mind->planner.current->object : mind->planner.lastObject;
	}
	const auto thing = target.has_value() ? static_cast<entt::entity>(*target) : entt::null;
	PushObject(registry.Valid(thing) ? thing : entt::null);
}

void CountdownTimerExists() // 099 COUNTDOWN_TIMER_EXISTS
{
	// The timer runs: it started and did not get to zero, and no script removed it
	Pushb(ScriptCountdown().IsRunning());
}

void LookGameThing() // 100 LOOK_GAME_THING
{
	// The advisor comes out and keeps looking at the thing, wherever it goes, until it is gone or told otherwise
	const auto target = PopObject();
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	if (target == entt::null || !Locator::entitiesRegistry::value().Valid(target))
	{
		ObjectNoLongerValid();
		return;
	}
	if (auto* advisors = ScriptAdvisors(); advisors != nullptr)
	{
		advisors->SpiritLookObject(spirit, entt::to_integral(target));
	}
}

void GetObjectDestination() // 101 GET_OBJECT_DESTINATION
{
	// The place where a living thing goes, with the land under it. A thing that does not live gives 0, 0, 0.
	const auto object = PopObject();
	const auto destination = ecs::script_destination::Of(Locator::entitiesRegistry::value(), object);
	if (!destination.has_value() || !Locator::terrainSystem::has_value())
	{
		PushVec(glm::vec3(0.0f));
		return;
	}
	PushVec(map_coords::ToWorld(Locator::terrainSystem::value(), *destination));
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

void PopulateContainer() // 109 POPULATE_CONTAINER
{
	namespace populate = ecs::script_populate;
	const auto subtype = Pop().intVal;
	const auto type = Pop().intVal;
	const auto count = populate::CountOf(Popf());
	const auto container = PopObject();
	if (!populate::IsValidType(type))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Invalid type={}", type);
		// The game leaves a value on the stack here although the statement gives none back
		Pusho(0);
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(container) || container == static_cast<entt::entity>(0))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Container not valid");
		return;
	}
	// About the container's place: openblack's containers that can be filled are flocks, at their home on the land
	glm::vec3 centre(0.0f);
	if (const auto* flock = registry.TryGet<const ecs::components::Flock>(container))
	{
		const auto place = map_coords::ToMetres(flock->place);
		centre = {place.x, Locator::terrainSystem::value().GetHeightAt(place), place.y};
	}
	else if (const auto* transform = registry.TryGet<const Transform>(container))
	{
		centre = transform->position;
	}
	const float spread = populate::SpreadOf(count);
	const auto floatRand = [](float x) { return Locator::gameRandom::value().GameFloatRand(x); };
	auto& scriptObjects = Locator::scriptObjects::value();
	for (uint32_t i = 0; i < count; ++i)
	{
		const auto place = populate::PlaceOf(centre, spread, floatRand);
		const auto thing = CreateScriptObject(static_cast<ObjectType>(type), static_cast<uint32_t>(subtype), place, 0.0f, 0.0f,
		                                      0.0f, 0.0f, 1.0f);
		if (thing == static_cast<entt::entity>(0) || !registry.Valid(thing))
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Could not create thing for populate");
			return;
		}
		if (!registry.AnyOf<ecs::components::Animal, ecs::components::Villager, ecs::components::Creature>(thing))
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Created Thing Not Living");
			registry.Destroy(thing);
			return;
		}
		RegisterCreated(thing);
		// A flock takes it as its newest member and it goes about with it
		if (!registry.AllOf<ecs::components::Flock>(container) || !IsFlockLiving(registry, thing))
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Not implemented - Thing not added to id");
			return;
		}
		ecs::script_flocks::AddLiving(registry, container, thing);
		SetFlockState(thing);
		scriptObjects.AddReference(thing);
	}
}

void AddReference() // 110 ADD_REFERENCE
{
	// A script takes hold of an object. The game's table says one value comes back, but the game's function pushes none
	// A variable that holds no object references nothing
	if (const auto id = Pop().uintVal; id != 0)
	{
		Locator::scriptObjects::value().AddReference(static_cast<entt::entity>(id));
	}
}

void RemoveReference() // 111 REMOVE_REFERENCE
{
	if (const auto id = Pop().uintVal; id != 0)
	{
		Locator::scriptObjects::value().RemoveReference(static_cast<entt::entity>(id));
	}
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

/// A weather native's change to the storm of the weather thing it was given; a thing that is no weather thing is told
/// of and left alone
void ChangeWeatherThing(entt::entity thing, const std::function<void(ecs::components::Storm&)>& change)
{
	if (thing == entt::null || !Locator::entitiesRegistry::value().Valid(thing))
	{
		return;
	}
	if (!Locator::weatherSystem::has_value() || !Locator::weatherSystem::value().ChangeWeatherThing(thing, change))
	{
		ScriptMessage("Weather properties on non weather thing");
	}
}

void ChangeWeatherProperties() // 123 CHANGE_WEATHER_PROPERTIES
{
	const auto fallSpeed = Popf();
	const auto overcast = Popf();
	const auto snowfall = Popf();
	const auto rainfall = Popf();
	const auto temperature = Popf();
	ChangeWeatherThing(PopObject(), [=](ecs::components::Storm& storm) {
		ecs::script_weather::SetWeather(storm, temperature, rainfall, snowfall, overcast, fallSpeed);
	});
}

void ChangeLightningProperties() // 124 CHANGE_LIGHTNING_PROPERTIES
{
	const auto forkMax = Popf();
	const auto forkMin = Popf();
	const auto sheetMax = Popf();
	const auto sheetMin = Popf();
	ChangeWeatherThing(PopObject(), [=](ecs::components::Storm& storm) {
		ecs::script_weather::SetLightning(storm, sheetMin, sheetMax, forkMin, forkMax);
	});
}

void ChangeTimeFadeProperties() // 125 CHANGE_TIME_FADE_PROPERTIES
{
	const auto fadeSeconds = Popf();
	const auto lastsFor = Popf();
	ChangeWeatherThing(PopObject(),
	                   [=](ecs::components::Storm& storm) { ecs::script_weather::SetTimeFade(storm, lastsFor, fadeSeconds); });
}

void ChangeCloudProperties() // 126 CHANGE_CLOUD_PROPERTIES
{
	const auto height = Popf();
	const auto shade = Popf();
	const auto count = Popf();
	ChangeWeatherThing(PopObject(),
	                   [=](ecs::components::Storm& storm) { ecs::script_weather::SetClouds(storm, count, shade, height); });
}

void SetHeadingAndSpeed() // 127 SET_HEADING_AND_SPEED
{
	// The thing is sent off towards the place at the speed: a thing at rest is thrown, tumbling at random, by no one; a
	// thing already flying only changes its velocity
	const auto speed = Popf();
	const auto place = PopVec();
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		return;
	}
	const auto* transform = registry.TryGet<const Transform>(object);
	const auto velocity =
	    script::property_rules::HeadingVelocity(transform != nullptr ? transform->position : glm::vec3(0.0f), place, speed);
	if (Locator::weatherSystem::has_value() &&
	    Locator::weatherSystem::value().ChangeWeatherThing(object, [](ecs::components::Storm&) {}))
	{
		// TODO(vortex-leave): a weather thing takes the velocity as the way it drifts each turn; openblack's weather
		// things drift towards a destination instead
		NotImplemented();
		return;
	}
	if (!IsWorldObject(object) || transform == nullptr || !Locator::dynamicsSystem::has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Thing not an object or weather for Physics");
		return;
	}
	auto& dynamics = Locator::dynamicsSystem::value();
	if (registry.AllOf<ecs::components::InPhysics>(object))
	{
		if (auto* entry = dynamics.Find(object); entry != nullptr && entry->body != nullptr)
		{
			entry->body->velocity = velocity;
		}
		return;
	}
	auto& random = Locator::gameRandom::value();
	glm::vec3 spin;
	spin.x = script::property_rules::HeadingSpin(random.GameFloatRand(script::property_rules::k_HeadingSpinDraw));
	spin.y = script::property_rules::HeadingSpin(random.GameFloatRand(script::property_rules::k_HeadingSpinDraw));
	spin.z = script::property_rules::HeadingSpin(random.GameFloatRand(script::property_rules::k_HeadingSpinDraw));
	dynamics.InitialisePhysics(object, {.velocity = velocity, .spin = spin});
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

void BuildBuilding() // 130 BUILD_BUILDING
{
	const auto desire = Popf();
	const auto position = PopVec();
	// Every town starts what it planned there
	if (ecs::town_planning::StartPlannedAt(position, desire).empty())
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("scripting"), "BUILD_BUILDING: no planned building at ({}, {})", position.x,
		                    position.z);
	}
}

void SetAffectedByWind() // 131 SET_AFFECTED_BY_WIND
{
	const auto object = PopObject();
	const auto affected = static_cast<bool>(Pop().intVal);
	if (object == entt::null || !Locator::entitiesRegistry::value().Valid(object))
	{
		ScriptMessage("Invalid thing");
		return;
	}
	// Only a weather thing's storm can be blown along; anything else takes no notice
	if (Locator::weatherSystem::has_value())
	{
		Locator::weatherSystem::value().SetWeatherThingAffectedByWind(object, affected);
	}
}

/// The thing a resource native is given, if it is a thing of the world that can hold resources: an error is told
/// for none or for a container
std::optional<entt::entity> ResourceThing(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ScriptMessage("No thing for resource");
		return std::nullopt;
	}
	if (!registry.AllOf<ecs::components::Transform>(object) || IsScriptContainer(registry, object))
	{
		ScriptMessage("Not object for resource");
		return std::nullopt;
	}
	return object;
}

/// A script's quantity, a number made whole towards none and taken as unsigned
uint32_t ResourceQuantity(float quantity)
{
	return static_cast<uint32_t>(static_cast<int32_t>(quantity));
}

void GetResource() // 133 GET_RESOURCE
{
	const auto object = PopObject();
	const auto type = static_cast<ResourceType>(Pop().intVal);
	const auto thing = ResourceThing(object);
	if (!thing.has_value() || !Locator::resourceStoreSystem::has_value())
	{
		Pushf(0.0f);
		return;
	}
	Pushf(static_cast<float>(Locator::resourceStoreSystem::value().GetResource(*thing, type)));
}

void AddResource() // 134 ADD_RESOURCE
{
	const auto object = PopObject();
	const auto quantity = ResourceQuantity(Popf());
	const auto type = static_cast<ResourceType>(Pop().intVal);
	const auto thing = ResourceThing(object);
	if (!thing.has_value() || !Locator::resourceStoreSystem::has_value())
	{
		Pushf(0.0f);
		return;
	}
	Pushf(static_cast<float>(Locator::resourceStoreSystem::value().AddResource(*thing, type, quantity, std::nullopt)));
}

void RemoveResource() // 135 REMOVE_RESOURCE
{
	const auto object = PopObject();
	const auto quantity = ResourceQuantity(Popf());
	const auto type = static_cast<ResourceType>(Pop().intVal);
	const auto thing = ResourceThing(object);
	if (!thing.has_value() || !Locator::resourceStoreSystem::has_value())
	{
		Pushf(0.0f);
		return;
	}
	Pushf(static_cast<float>(Locator::resourceStoreSystem::value().RemoveResource(*thing, type, quantity)));
}

void GetTargetRelativePos() // 136 GET_TARGET_RELATIVE_POS
{
	// The point at the distance past the second point, on the line from the first point, turned by the angle
	const auto angle = Popf();
	const auto distance = Popf();
	const auto to = PopVec();
	const auto from = PopVec();
	PushVec(script::property_rules::TargetRelativePosition(from, to, distance, angle));
}

void StopPointing() // 137 STOP_POINTING
{
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	if (auto* advisors = ScriptAdvisors(); advisors != nullptr)
	{
		advisors->SpiritStopPointing(spirit);
	}
}

void StopLooking() // 138 STOP_LOOKING
{
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	if (auto* advisors = ScriptAdvisors(); advisors != nullptr)
	{
		advisors->SpiritStopLooking(spirit);
	}
}

void LookAtPosition() // 139 LOOK_AT_POSITION
{
	const auto position = PopVec();
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	if (auto* advisors = ScriptAdvisors(); advisors != nullptr)
	{
		advisors->SpiritLookAtPosition(spirit, position);
	}
}

void CallInNotNear() // 141 CALL_IN_NOT_NEAR
{
	// The first thing of the type and subtype in the container that is farther from the place than the radius
	CallInByDistance(false);
}

void GetObjectState() // 143 GET_OBJECT_STATE
{
	const auto object = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ScriptMessage("Thing invalid");
		Pushi(0);
		return;
	}
	// A villager or an animal gives the game's number of the state that it is in for itself
	if (const auto state = FinalGameStateOf(registry, object))
	{
		Pushi(*state);
		return;
	}
	// A creature is a living thing that keeps no such state, so it gives 0
	if (registry.AllOf<ecs::components::Creature>(object))
	{
		Pushi(0);
		return;
	}
	// A puzzle game that nobody has started gives whether it has played
	if (Locator::puzzleGameSystem::has_value())
	{
		if (const auto state = Locator::puzzleGameSystem::value().State(object))
		{
			Pushi(static_cast<int32_t>(*state));
			return;
		}
	}
	ScriptMessage("Thing not a known type for function");
	Pushi(0);
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

void GetInclusionDistance() // 150 GET_INCLUSION_DISTANCE
{
	// The world camera's distance to the fence from its last fence test
	const auto view = ScriptView();
	Pushf(view.has_value() ? view->inclusionDistance : 0.0f);
}

void GetLandHeight() // 151 GET_LAND_HEIGHT
{
	const auto position = PopVec();

	const auto& island = Locator::terrainSystem::value();
	const auto elevation = island.GetHeightAt(glm::vec2(position.x, position.z));

	Pushf(elevation);
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

void ClearClickedObject() // 156 CLEAR_CLICKED_OBJECT
{
	if (auto* clicked = LocalHandClicked())
	{
		hand_click::ClearThing(*clicked);
	}
}

void ClearClickedPosition() // 157 CLEAR_CLICKED_POSITION
{
	if (auto* clicked = LocalHandClicked())
	{
		hand_click::ClearPlace(*clicked);
	}
}

void PositionClicked() // 158 POSITION_CLICKED
{
	const auto radius = Popf();
	const auto position = PopVec();
	const auto* clicked = LocalHandClicked();
	// Along the ground: the position's height doesn't count
	const map_coords::MapCoords at {.x = map_coords::ToFixed(position.x), .z = map_coords::ToFixed(position.z)};
	Pushb(clicked != nullptr && hand_click::IsPlaceClicked(*clicked, at, radius));
}

void ReleaseFromScript() // 159 RELEASE_FROM_SCRIPT
{
	Locator::scriptObjects::value().ReleaseFromScript(PopObject());
}

void GetObjectHandIsOver() // 160 GET_OBJECT_HAND_IS_OVER
{
	// The thing under the cursor of the player's hand, from the last pick that a turn did. The game warns that a network
	// game gives each computer the thing under its own hand.
	if (Locator::time::has_value() && Locator::time::value().IsNetworkGame())
	{
		ScriptMessage("This is not multiplayer friendly");
	}
	const auto object = Locator::handSystem::has_value()
	                        ? Locator::handSystem::value().GetReportedPickOf(PlayerNames::PLAYER_ONE)
	                        : entt::entity {entt::null};
	const auto& registry = Locator::entitiesRegistry::value();
	PushObject(object != entt::null && registry.Valid(object) ? object : entt::entity {entt::null});
}

void IdPoisonedSize() // 161 ID_POISONED_SIZE
{
	const auto container = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	if (container == entt::null || !registry.Valid(container))
	{
		ScriptMessage("Thing not valid");
		Pushf(0.0f);
		return;
	}
	if (!IsScriptContainer(registry, container))
	{
		ScriptMessage("No Loop funtion for type");
		Pushf(0.0f);
		return;
	}
	// How many of its members are poisoned
	const auto members = ContainerMembers(registry, container);
	Pushf(static_cast<float>(std::ranges::count_if(
	    members, [&registry](entt::entity member) { return ecs::script_poison::IsPoisoned(registry, member); })));
}

void IsPoisoned() // 162 IS_POISONED
{
	const auto object = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ScriptMessage("Thing not valid");
		Pushb(false);
		return;
	}
	Pushb(ecs::script_poison::IsPoisoned(registry, object));
}

/// The first member of a script container that is of the type and subtype, and poisoned or else not poisoned, as the scripts
/// ask. It is none when there is none, and the script is told of it.
static void CallPoisonedStateIn(bool poisoned, std::string_view notFound)
{
	const auto excludingScripted = Pop().intVal != 0;
	const auto container = PopObject();
	const auto subtype = Pop().uintVal;
	const auto type = Pop().intVal;
	auto& registry = Locator::entitiesRegistry::value();
	const auto fail = [&notFound]() {
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "{}", notFound);
		Pusho(0);
	};
	if (container == entt::null || !registry.Valid(container))
	{
		fail();
		return;
	}
	if (!IsScriptContainer(registry, container))
	{
		ScriptMessage("Cannot look in object");
		fail();
		return;
	}
	const auto objectType = static_cast<ObjectType>(type);
	if (registry.AllOf<ecs::components::Town>(container) && objectType != ObjectType::Villager &&
	    objectType != ObjectType::VillagerChild)
	{
		ScriptMessage("Looking for strange type in Town");
		fail();
		return;
	}
	for (const auto member : ContainerMembers(registry, container))
	{
		if (!registry.Valid(member) || (excludingScripted && registry.AllOf<ecs::components::InScript>(member)) ||
		    ecs::script_poison::IsPoisoned(registry, member) != poisoned)
		{
			continue;
		}
		const auto kind = ecs::script_find::KindOf(registry, member);
		if (kind.has_value() && ecs::script_find::Matches(*kind, objectType, subtype))
		{
			Locator::scriptObjects::value().Register(member, false);
			Pusho(static_cast<uint32_t>(member));
			return;
		}
	}
	fail();
}

void CallPoisonedIn() // 163 CALL_POISONED_IN
{
	CallPoisonedStateIn(true, "Cannot find poisoned in");
}

void CallNotPoisonedIn() // 164 CALL_NOT_POISONED_IN
{
	CallPoisonedStateIn(false, "Cannot find not poisoned in");
}

void SetIdMoveable() // 168 SET_ID_MOVEABLE
{
	const auto object = PopObject();
	const auto moveable = static_cast<bool>(Pop().intVal);
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_ID_MOVEABLE: thing not valid");
		return;
	}
	// A thing made unmoveable is held where it is: the physics never takes it
	if (moveable)
	{
		registry.Remove<ecs::components::Immovable>(object);
	}
	else
	{
		registry.AssignOrReplace<ecs::components::Immovable>(object);
	}
}

void SetIdPickupable() // 169 SET_ID_PICKUPABLE
{
	const auto object = PopObject();
	const auto pickupable = Pop().uintVal != 0;
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_ID_PICKUPABLE: thing not valid");
		return;
	}
	// A thing a script says can't be picked up is left by the hand
	if (pickupable)
	{
		registry.Remove<ecs::components::CannotBePickedUp>(object);
	}
	else
	{
		registry.AssignOrReplace<ecs::components::CannotBePickedUp>(object);
	}
}

void IsOnFire() // 170 IS_ON_FIRE
{
	const auto object = PopObject();
	Pushb(Locator::fireSystem::has_value() && Locator::fireSystem::value().IsOnFire(object));
}

void IsFireNear() // 171 IS_FIRE_NEAR
{
	const auto radius = Popf();
	const auto position = PopVec();
	Pushb(Locator::fireSystem::has_value() && Locator::fireSystem::value().IsFireNear(position, radius));
}

void StopScriptsInFiles() // 172 STOP_SCRIPTS_IN_FILES
{
	const auto sourceFilenames = PopString();

	auto& lhvm = Locator::vm::value();
	lhvm.StopScripts([&sourceFilenames]([[maybe_unused]] const std::string& name, const std::string& filename) -> bool {
		return script::name_lists::HoldsFile(sourceFilenames, filename);
	});
}

void SetPoisoned() // 173 SET_POISONED
{
	const auto object = PopObject();
	const auto value = Pop().uintVal;
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ScriptMessage("Thing not valid");
		return;
	}
	// A group poisons or cures each of its members
	if (IsScriptContainer(registry, object))
	{
		for (const auto member : ContainerMembers(registry, object))
		{
			ecs::script_poison::SetPoisoned(registry, member, value);
		}
		return;
	}
	if (!registry.AllOf<Transform>(object) || registry.AllOf<ecs::components::ScriptMarker>(object))
	{
		ScriptMessage("Thing not object");
		return;
	}
	ecs::script_poison::SetPoisoned(registry, object, value);
}

void SetTemperature() // 174 SET_TEMPERATURE
{
	const auto temperature = Popf();
	const auto object = PopObject();
	if (Locator::fireSystem::has_value())
	{
		Locator::fireSystem::value().SetTemperature(object, temperature, entt::null);
	}
}

void SetOnFire() // 175 SET_ON_FIRE
{
	const auto burnSpeed = Popf();
	const auto object = PopObject();
	const auto enable = static_cast<bool>(Pop().intVal);
	if (!Locator::fireSystem::has_value())
	{
		return;
	}
	// Set alight at the speed, or brought back to the air's temperature
	if (enable)
	{
		Locator::fireSystem::value().SetOnFire(object, burnSpeed);
	}
	else
	{
		Locator::fireSystem::value().PutOut(object);
	}
}

void SetTarget() // 176 SET_TARGET
{
	// The thing flies with no air drag and gets to the place after the time
	const auto seconds = Popf();
	const auto place = PopVec();
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	const bool valid = object != entt::null && registry.Valid(object);
	if (!valid)
	{
		ScriptMessage("Thing not valid");
	}
	// A time of zero or less, or a time that is not a number, is not a time
	const bool timeValid = seconds > 0.0f;
	if (!timeValid)
	{
		ScriptMessage("Invalid time");
	}
	if (!valid || !timeValid || !IsWorldObject(object) || !Locator::dynamicsSystem::has_value())
	{
		return;
	}
	const auto* transform = registry.TryGet<const Transform>(object);
	if (transform == nullptr)
	{
		return;
	}
	const auto velocity = script::property_rules::TargetVelocity(transform->position, place, seconds);
	// The game takes the three draws of the spin before it looks at the physics, so it also takes them for a flying thing
	auto& random = Locator::gameRandom::value();
	glm::vec3 spin;
	spin.x = script::property_rules::HeadingSpin(random.GameFloatRand(script::property_rules::k_HeadingSpinDraw));
	spin.y = script::property_rules::HeadingSpin(random.GameFloatRand(script::property_rules::k_HeadingSpinDraw));
	spin.z = script::property_rules::HeadingSpin(random.GameFloatRand(script::property_rules::k_HeadingSpinDraw));
	auto& dynamics = Locator::dynamicsSystem::value();
	if (registry.AllOf<ecs::components::InPhysics>(object))
	{
		// A thing that flies already only changes its velocity
		if (auto* entry = dynamics.Find(object); entry != nullptr && entry->body != nullptr)
		{
			entry->body->velocity = velocity;
			entry->body->SetDrag(0.0f);
		}
		return;
	}
	const auto started = dynamics.InitialisePhysics(object, {.velocity = velocity, .spin = spin});
	if (started.entry != nullptr && started.entry->body != nullptr)
	{
		started.entry->body->SetDrag(0.0f);
	}
}

void WalkPath() // 177 WALK_PATH
{
	const auto to = Popf();
	const auto from = Popf();
	const auto path = Pop().intVal;
	const auto forward = Pop().intVal != 0;
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		ScriptMessage("Thing not valid");
		return;
	}
	if (registry.AllOf<ecs::components::Villager>(object))
	{
		// A villager walks the track at its own speed, then waits for the script
		if (!ecs::villager_script::StartPathWalk(object, path, forward, from, to))
		{
			SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "Cannot load track No {}", path);
		}
		return;
	}
	if (registry.AnyOf<ecs::components::Creature, ecs::components::Animal>(object))
	{
		// TODO(opening): creatures and animals walk the track at their own speed too
		NotImplemented();
		return;
	}
	// The things that move as the game's mobile objects do walk the track, anything else can't
	if (!registry.AnyOf<ecs::components::Shark, ecs::components::MobileObject>(object))
	{
		ScriptMessage("Thing is invalid for move path");
		return;
	}
	static_cast<void>(Locator::walkPathSystem::value().Start(object, path, forward, from, to));
}

void GetWalkPathPercentage() // 179 GET_WALK_PATH_PERCENTAGE
{
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		ScriptMessage("Thing not valid");
		Pushf(1.0f);
		return;
	}
	if (registry.AllOf<ecs::components::Villager>(object))
	{
		// How much of its track it has walked
		// TODO(opening): the game reads the walk of a villager given none to walk; here it has walked the whole way
		Pushf(ecs::villager_script::PathWalkPercentage(object).value_or(1.0f));
		return;
	}
	if (registry.AnyOf<ecs::components::Creature, ecs::components::Animal>(object))
	{
		// TODO(opening): how much of its track a creature or an animal has walked
		NotImplemented();
		Pushf(1.0f);
		return;
	}
	// Anything else, the things walking tracks too, has always walked the whole way
	Pushf(1.0f);
}

void IsOfType() // 192 IS_OF_TYPE
{
	// Whether a thing is of a script type and subtype, the subtype 5000 meaning any of the type
	const auto subtype = Pop().uintVal;
	const auto type = Pop().intVal;
	const auto object = PopObject();
	constexpr int32_t k_LastType = 41;
	if (type < 1 || type > k_LastType)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Script object type {} out of range", type);
		Pushb(false);
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ObjectNoLongerValid();
		Pushb(false);
		return;
	}
	const auto kind = ecs::script_find::KindOf(registry, object);
	Pushb(kind.has_value() && ecs::script_find::Matches(*kind, static_cast<ObjectType>(type), subtype));
}

void ClearHitObject() // 193 CLEAR_HIT_OBJECT
{
	// The physics forgets the last thing hit and what hit it
	Locator::dynamicsSystem::value().RecordHit(entt::null, entt::null);
}

void GameThingHit() // 194 GAME_THING_HIT
{
	const auto object = PopObject();
	if (object == entt::null || !Locator::entitiesRegistry::value().Valid(object))
	{
		ObjectNoLongerValid();
		Pushb(false);
		return;
	}
	// Whether it is the thing the physics last saw hit, until a script clears it
	const auto hit = Locator::dynamicsSystem::value().GetHitObject();
	if (hit == object)
	{
		Pushb(true);
		return;
	}
	// The physics sees only the dome of a shield miracle. A hit on the dome is also a hit on its miracle (deviation 180).
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* shield =
	    hit != entt::null && registry.Valid(hit) ? registry.TryGet<const ecs::components::MagicShield>(hit) : nullptr;
	Pushb(shield != nullptr && shield->spell == object);
}

void SpellAtThing() // 195 SPELL_AT_THING
{
	const auto curl = Popf();
	const auto duration = Popf();
	const auto radius = Popf();
	const auto from = PopVec();
	const auto target = PopObject();
	const auto spell = Pop().intVal;
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.Valid(target) ? registry.TryGet<const ecs::components::Transform>(target) : nullptr;
	if (transform == nullptr)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Invalid thing");
		Pusho(0);
		return;
	}
	if (!magic::IsScriptMagicType(spell) || !Locator::magicSystem::has_value() || !Locator::infoConstants::has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Invalid magic");
		Pusho(0);
		return;
	}
	// The neutral player's miracle, spun by the script's curl: on an object as its seed casts, else at the thing's place
	const auto type = static_cast<MagicType>(spell);
	const auto& info = Locator::infoConstants::value();
	const auto cast = magic::MakeScriptCast(magic::GetMagicEffectInfo(info, type).initialChants, transform->position, from,
	                                        radius, duration, curl);
	const auto seed = magic::FindFirstSpellSeedForMagicType(info, type);
	const bool object = IsScriptObject(registry, target);
	const bool onObject = object && seed.has_value() && magic::GetSpellSeedInfo(info, *seed).castOnObject != 0;
	auto& magicSystem = Locator::magicSystem::value();
	const auto entity = onObject ? magicSystem.CastOnObject(type, PlayerNames::NEUTRAL, target, cast.cast, cast.info)
	                             : magicSystem.CastAtPoint(type, PlayerNames::NEUTRAL, cast.point, cast.cast, cast.info);
	if (entity == entt::null)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Spell not created");
		Pusho(0);
		return;
	}
	// The miracle is the script's own making, which it controls from its first reference
	RegisterCreated(entity);
	Pusho(static_cast<uint32_t>(entity));
}

void SpellAtPos() // 196 SPELL_AT_POS
{
	const auto curl = Popf();
	const auto duration = Popf();
	const auto radius = Popf();
	const auto from = PopVec();
	const auto target = PopVec();
	const auto spell = Pop().intVal;
	if (!magic::IsScriptMagicType(spell) || !Locator::magicSystem::has_value() || !Locator::infoConstants::has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Invalid magic");
		// The script still takes a result off the stack
		Pusho(0);
		return;
	}
	// The neutral player's miracle, spun by the script's curl
	const auto type = static_cast<MagicType>(spell);
	const auto cast = magic::MakeScriptCast(magic::GetMagicEffectInfo(Locator::infoConstants::value(), type).initialChants,
	                                        target, from, radius, duration, curl);
	const auto entity = Locator::magicSystem::value().CastAtPoint(type, PlayerNames::NEUTRAL, cast.point, cast.cast, cast.info);
	if (entity == entt::null)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Spell not created");
		Pusho(0);
		return;
	}
	// The miracle is the script's own making, which it controls from its first reference
	RegisterCreated(entity);
	Pusho(static_cast<uint32_t>(entity));
}

/// The walk speed of a thing in metres a turn, as the slowest speed of a group reads it: an animal's speed also when it
/// is dead, a villager's speed of its walk, and 0 for a thing that does not walk
static float WalkSpeedOf(const ecs::Registry& registry, entt::entity thing)
{
	if (const auto* animal = registry.TryGet<const ecs::components::Animal>(thing); animal != nullptr)
	{
		return animals::SpeedForScript(animal->move.speed, false);
	}
	if (const auto* wallHug = registry.TryGet<const ecs::components::WallHug>(thing); wallHug != nullptr)
	{
		return ecs::villager_script_rules::WalkSpeedToScriptSpeed(wallHug->speed);
	}
	return 0.0f;
}

void GetSlowestSpeed() // 198 GET_SLOWEST_SPEED
{
	// The lowest walk speed of the members of a town, a flock or a dance
	const auto container = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	if (container == entt::null || !registry.Valid(container))
	{
		ObjectNoLongerValid();
		Pushf(0.0f);
		return;
	}
	if (!IsScriptContainer(registry, container))
	{
		ScriptMessage("Object not container");
		Pushf(0.0f);
		return;
	}
	std::vector<float> speeds;
	for (const auto member : ContainerMembers(registry, container))
	{
		speeds.push_back(WalkSpeedOf(registry, member));
	}
	Pushf(ecs::script_containers::SlowestSpeed(speeds));
}

void GetObjectHeld199() // 199 GET_OBJECT_HELD
{
	// What the hand of this computer holds, if anything: a thing it took hold of, else the miracle in it. In a network
	// game each computer reads the hand of its own god, thus a script that acts on the answer can part the game
	// (vanilla-bugs row 497).
	auto held = Locator::handGrabSystem::has_value() ? Locator::handGrabSystem::value().GetHeld() : std::nullopt;
	if (!held.has_value() && Locator::magicSystem::has_value())
	{
		held = Locator::magicSystem::value().GetHeldSeed();
	}
	if (!held.has_value() || !Locator::entitiesRegistry::value().Valid(*held))
	{
		PushObject(entt::null);
		return;
	}
	Locator::scriptObjects::value().Register(*held, false);
	PushObject(*held);
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

void PlayGesture() // 204 PLAY_GESTURE
{
	// The shape of the gesture shows on the land as if this computer's player drew it: its square, so many metres
	// across, goes from the point towards the east and the south. No gesture shows for gesture 0.
	const auto size = Popf();
	const auto corner = PopVec();
	const auto gesture = static_cast<GestureType>(Pop().intVal);
	if (Locator::gestureSystem::has_value())
	{
		Locator::gestureSystem::value().ShowGestureOnLand(gesture, corner, size, std::nullopt);
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

void HasMouseWheel() // 206 HAS_MOUSE_WHEEL
{
	// The view report of the player tells whether its mouse has a wheel
	const auto view = ScriptView();
	Pushb(view.has_value() && view->mouseWheel);
}

void NumMouseButtons() // 207 NUM_MOUSE_BUTTONS
{
	// The view report of the player tells how many buttons its mouse has
	const auto view = ScriptView();
	Pushf(view.has_value() ? static_cast<float>(view->mouseButtons) : 0.0f);
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
	else if (!registry.AllOf<ecs::components::Creature>(first) || !registry.AllOf<ecs::components::Creature>(second))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GET_ARENA: thing not creature");
	}
	else if (Locator::creatureFightSystem::has_value())
	{
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

void GetHitObject() // 224 GET_HIT_OBJECT
{
	// The last thing a body in the physics hit, none once it has gone
	const auto hit = Locator::dynamicsSystem::value().GetHitObject();
	Pusho(hit == entt::null ? 0 : static_cast<uint32_t>(hit));
}

void GetObjectWhichHit() // 225 GET_OBJECT_WHICH_HIT
{
	// What hit it, none once it has gone
	const auto hitter = Locator::dynamicsSystem::value().GetObjectWhichHit();
	Pusho(hitter == entt::null ? 0 : static_cast<uint32_t>(hitter));
}

void GetNearestTownOfPlayer() // 226 GET_NEAREST_TOWN_OF_PLAYER
{
	// The player's nearest town to the place within the radius, by the game's map distance. The towns go in the order
	// of the player's own list, and the last of equals wins.
	const auto radius = Popf();
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	const auto position = PopVec();
	auto& registry = Locator::entitiesRegistry::value();
	entt::entity found = entt::null;
	// The game has no player for a number past its players and stops. This search then finds no town.
	if (ScriptNamesPlayer(player))
	{
		const auto from = openblack::map_coords::FromMetres({position.x, position.z});
		std::vector<openblack::ecs::player_towns::Distance> towns;
		for (const auto town : openblack::ecs::player_towns::InOrder(registry, player))
		{
			if (const auto* transform = registry.TryGet<const Transform>(town))
			{
				const auto at = openblack::map_coords::FromMetres({transform->position.x, transform->position.z});
				towns.push_back({.town = town, .metres = openblack::gutils::GetDistanceInMetres(from, at)});
			}
		}
		found = openblack::ecs::player_towns::NearestOfPlayer(towns, radius);
	}
	if (found == entt::null)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "No town of the player near enough");
		Pusho(0);
		return;
	}
	Locator::scriptObjects::value().Register(found, false);
	Pusho(static_cast<uint32_t>(found));
}

void SpellAtPoint() // 227 SPELL_AT_POINT
{
	// Finds a miracle already at a point, casting nothing: for either shield the first shield standing over the point,
	// else the newest miracle of the kind whose last event was strictly within the radius
	const auto radius = Popf();
	const auto position = PopVec();
	const auto spell = Pop().intVal;
	std::optional<entt::entity> found;
	if (spell == static_cast<int32_t>(MagicType::Shield) || spell == static_cast<int32_t>(MagicType::PhysicalShield))
	{
		if (Locator::magicShieldSystem::has_value())
		{
			found = Locator::magicShieldSystem::value().ShieldAt(position);
		}
	}
	else if (Locator::magicSystem::has_value())
	{
		found = Locator::magicSystem::value().SpellAt(static_cast<MagicType>(spell), position, radius);
	}
	Pusho(found.has_value() ? static_cast<uint32_t>(*found) : 0);
}

void SetAttackOwnTown() // 228 SET_ATTACK_OWN_TOWN
{
	// The creature can be angry with the things of its own player, or it cannot
	const auto object = PopObject();
	const auto allow = Pop().uintVal != 0;
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ScriptMessage("Thing not found!");
		return;
	}
	if (auto* mind = registry.TryGet<CreatureMindState>(object))
	{
		mind->mayAttackOwnPlayer = allow;
	}
	else
	{
		ScriptMessage("Thing not creature!");
	}
}

void SetMagicRadius() // 230 SET_MAGIC_RADIUS
{
	// A miracle's radius changes at once, and its effect and its shield follow it. Anything that is not a miracle does
	// not change.
	const auto radius = Popf();
	const auto object = PopObject();
	if (!Locator::entitiesRegistry::value().Valid(object))
	{
		ScriptMessage("Thing not valid");
		return;
	}
	if (Locator::magicSystem::has_value())
	{
		Locator::magicSystem::value().SetSpellRadius(object, radius);
	}
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

void UpdateSnapshot() // 238 UPDATE_SNAPSHOT
{
	KeepChallengeRecord(PopChallengeSnapshot(script::challenge_snapshots::Call::Update),
	                    script::challenge_snapshots::Call::Update);
}

/// The kinds of reward a script can give are numbered 1 to 60
constexpr int32_t k_LastRewardType = 60;

/// A reward chest of a kind for the first player, given to a town or none: on the land, or from the sky
uint32_t GiveReward(int32_t type, glm::vec3 position, entt::entity town, bool fromSky)
{
	if (type < 1 || type > k_LastRewardType)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "CREATE_REWARD: invalid type {}", type);
		return 0;
	}
	if (!Locator::rewardSystem::has_value())
	{
		return 0;
	}
	const auto player =
	    Locator::playerSystem::has_value() ? std::optional(Locator::playerSystem::value().GetLocalPlayer()) : std::nullopt;
	const auto chest =
	    Locator::rewardSystem::value().Create(position, static_cast<RewardObjectInfo>(type), player, town, fromSky);
	RegisterCreated(chest);
	return static_cast<uint32_t>(chest);
}

void CreateReward() // 239 CREATE_REWARD
{
	const auto fromSky = Pop().intVal != 0;
	const auto position = PopVec();
	const auto type = Pop().intVal;
	Pusho(GiveReward(type, position, entt::null, fromSky));
}

void CreateRewardInTown() // 240 CREATE_REWARD_IN_TOWN
{
	const auto fromSky = Pop().intVal != 0;
	const auto position = PopVec();
	const auto town = PopObject();
	const auto type = Pop().intVal;
	// The town must be one
	if (!Locator::entitiesRegistry::value().Valid(town) ||
	    !Locator::entitiesRegistry::value().AllOf<ecs::components::Town>(town))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "CREATE_REWARD_IN_TOWN: not a town");
		Pusho(0);
		return;
	}
	Pusho(GiveReward(type, position, town, fromSky));
}

void SetPlayerMagic() // 244 SET_PLAYER_MAGIC
{
	const auto player = Popf();
	const auto magic = Pop().intVal;
	const bool enable = Pop().intVal != 0;
	if (!script::lesson_natives::ValidMagic(magic))
	{
		ScriptMessage(script::lesson_natives::k_InvalidMagic);
		return;
	}
	// One more thing enables the magic type for the player, or one fewer does. A player that is not there is ignored.
	if (ScriptPlayer(player) != nullptr && Locator::spellIconSystem::has_value())
	{
		Locator::spellIconSystem::value().SetMagicTypeEnabled(ScriptPlayerName(static_cast<int32_t>(player)),
		                                                      static_cast<MagicType>(magic), enable);
	}
}

void HasPlayerMagic() // 245 HAS_PLAYER_MAGIC
{
	const auto player = Popf();
	const auto magic = Pop().intVal;
	if (!script::lesson_natives::ValidMagic(magic))
	{
		ScriptMessage(script::lesson_natives::k_InvalidMagic);
	}
	const auto* component = script::lesson_natives::ValidMagic(magic) ? ScriptPlayer(player) : nullptr;
	Pushb(script::lesson_natives::HasPlayerMagic(magic, component != nullptr ? &component->miracles : nullptr));
}

/// The belief of a town a script names, reporting an object that is gone or isn't a town as the game does
ecs::components::TownImpression* ScriptTownImpression(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ObjectNoLongerValid();
		return nullptr;
	}
	if (!registry.AllOf<ecs::components::Town>(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Not town!");
		return nullptr;
	}
	return registry.AnyOf<ecs::components::TownImpression>(object) ? &registry.Get<ecs::components::TownImpression>(object)
	                                                               : &registry.Assign<ecs::components::TownImpression>(object);
}

void BeliefForPlayer() // 247 BELIEF_FOR_PLAYER
{
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object) || !ScriptNamesPlayer(player))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object or player no longer valid");
		Pushf(0.0f);
		return;
	}
	const auto* town = registry.TryGet<const ecs::components::Town>(object);
	std::optional<float> townBelief;
	if (const auto* impression = registry.TryGet<const ecs::components::TownImpression>(object);
	    town != nullptr && impression != nullptr)
	{
		townBelief = impression->belief.belief.at(static_cast<size_t>(player));
	}
	Pushf(script::property_rules::BeliefForPlayer(town != nullptr, townBelief, ecs::world_objects::PlayerOf(object), player));
}

void ObjectRelativeBelief() // 251 OBJECT_RELATIVE_BELIEF
{
	// The player takes the town: the player it believes in most is halved, and the taker believes that most and the amount
	const auto amount = Popf();
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	auto* impression = ScriptTownImpression(PopObject());
	if (impression == nullptr || !ScriptNamesPlayer(player))
	{
		return;
	}
	// The players that play and the neutral player take part. A player that nobody plays does not.
	std::vector<PlayerNames> inGame;
	for (const auto name : ecs::game_players::All())
	{
		if (name == PlayerNames::NEUTRAL ||
		    (Locator::playerSystem::has_value() && Locator::playerSystem::value().PlaysInGame(name)))
		{
			inGame.push_back(name);
		}
	}
	magic::town_belief::TakeTown(impression->belief, player, amount, inGame);
}

void CreateWithAngleAndScale() // 252 CREATE_WITH_ANGLE_AND_SCALE
{
	// The angle is given in degrees
	const auto position = PopVec();
	const auto subtype = Pop().intVal;
	const auto type = Pop().intVal;
	const auto scale = Popf();
	const auto angle = script::create_rules::AngleFromDegrees(Popf());
	CreateForScript(type, subtype, position, angle, scale);
}

void SetVirtualInfluence() // 254 SET_VIRTUAL_INFLUENCE
{
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	const auto enable = Pop().intVal != 0;
	SPDLOG_LOGGER_DEBUG(spdlog::get("scripting"), "The hand's virtual influence for player {} is turned {}",
	                    static_cast<int>(player), enable ? "on" : "off");
	// Turned off, the player's hand keeps nothing of their influence past the border, and loses what it had
	if (!Locator::playerSystem::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = Locator::playerSystem::value().FindPlayer(player).value_or(entt::null);
	if (!registry.Valid(entity))
	{
		return;
	}
	auto& state =
	    (registry.AnyOf<ecs::components::VirtualInfluence>(entity) ? registry.Get<ecs::components::VirtualInfluence>(entity)
	                                                               : registry.Assign<ecs::components::VirtualInfluence>(entity))
	        .state;
	state.disabled = !enable;
	if (state.disabled)
	{
		state.fraction = 0.0f;
	}
}

void SetActive() // 255 SET_ACTIVE
{
	const auto object = PopObject();
	const auto active = Pop().intVal != 0;
	if (object == entt::null)
	{
		ObjectNoLongerValid();
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<ecs::components::ScriptHighlight>(object))
	{
		Locator::scriptHighlightSystem::value().SetActive(object, active);
		return;
	}
	// A dispenser set active floats a new bubble at once
	if (registry.AllOf<ecs::components::SpellDispenser>(object))
	{
		Locator::magicSystem::value().SetDispenserActive(object, active);
		return;
	}
	// A scaffold set active starts its building at once where it stands, for no player. Set inactive, it does nothing.
	if (registry.AllOf<ecs::components::Scaffold>(object))
	{
		if (active && Locator::workshopSystem::has_value())
		{
			Locator::workshopSystem::value().ForceBuild(object, std::nullopt);
		}
		return;
	}
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Invalid object type");
}

void ThingValid() // 256 THING_VALID
{
	// No object, one that has gone or one no longer to be dealt with is not valid; a town is valid while any of it stands
	const auto object = PopObject();
	const auto* town =
	    object != entt::null ? Locator::entitiesRegistry::value().TryGet<const ecs::components::Town>(object) : nullptr;
	Pushb(object != entt::null && (town == nullptr || !TownCompletelyDestroyed(*town)));
}

void VortexFadeOut() // 257 VORTEX_FADE_OUT
{
	// The vortex starts to fade out, whatever it was doing. Nothing given at all is an error; a thing that isn't a vortex
	// is one too.
	DataType type {};
	const auto value = Pop(type);
	// A script's object 0 is none, as PopObject reads it
	const auto vortex = value.uintVal == 0 ? entt::entity {entt::null}
	                                       : Locator::scriptObjects::value().Fetch(static_cast<entt::entity>(value.uintVal));
	if (vortex == entt::null || !Locator::entitiesRegistry::value().Valid(vortex))
	{
		if (type == DataType::None)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "vortex fade out failed");
		}
		return;
	}
	if (!Locator::vortexSystem::value().StartFadeOut(vortex))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Thing not vortex");
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "vortex fade out failed");
	}
}

void RemoveReactionOfType() // 258 REMOVE_REACTION_OF_TYPE
{
	// The reactions of that kind the thing made go; a group's are those its members made
	const auto type = static_cast<Reaction>(Pop().intVal);
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ScriptMessage("no reaction for thing");
		return;
	}
	if (!Locator::reactionSystem::has_value())
	{
		return;
	}
	auto& reactions = Locator::reactionSystem::value();
	if (IsScriptContainer(registry, object))
	{
		for (const auto member : ContainerMembers(registry, object))
		{
			reactions.RemoveFrom(member, type);
		}
		return;
	}
	reactions.RemoveFrom(object, type);
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

void ObjectCastByObject() // 261 OBJECT_CAST_BY_OBJECT
{
	// Whether the thing made the miracle. With no thing, the game gives the same message as for a thing that is not a
	// miracle.
	const auto caster = PopObject();
	const auto spellObject = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	if (spellObject == entt::null || !registry.Valid(spellObject))
	{
		ScriptMessage("Spell Object does not exist");
		Pushb(false);
		return;
	}
	const auto* spell = registry.TryGet<const ecs::components::Spell>(spellObject);
	if (spell == nullptr || caster == entt::null)
	{
		ScriptMessage("Object should be a spell");
		Pushb(false);
		return;
	}
	Pushb(ecs::spell_creator::CastBy(*spell, caster));
}

void IsWindMagicAtPos() // 262 IS_WIND_MAGIC_AT_POS
{
	// Whether a storm with no clouds of its own, as the wind miracle makes, is near the point. Every storm counts, also
	// one that ended.
	const auto radius = Popf();
	const auto position = PopVec();
	bool found = false;
	Locator::entitiesRegistry::value().Each<const ecs::components::Storm>(
	    [&found, position, radius](const ecs::components::Storm& storm) {
		    found = found ||
		            ecs::script_weather::WindMagicNear(storm.position, storm.outerRadius, storm.cloudCount, position, radius);
	    });
	Pushb(found);
}

void CreateMist() // 263 CREATE_MIST
{
	// A puff of mist at the point given, in the colour given, one over the height ratio times wider than tall seen level
	const auto heightRatio = Popf();
	const auto transparency = Popf();
	const auto blue = Popf();
	const auto green = Popf();
	const auto red = Popf();
	const auto scale = Popf();
	const auto position = PopVec();
	const float ground = Locator::terrainSystem::value().GetHeightAt(glm::vec2(position.x, position.z));
	const auto mist = ecs::archetypes::MistArchetype::Create(
	    position, position.y - ground, mists::ScriptColour(red, green, blue, transparency), scale, 1.0f / heightRatio);
	if (mist == entt::null)
	{
		ScriptMessage("Thing not created");
		PushObject(entt::null);
		return;
	}
	RegisterCreated(mist);
	PushObject(mist);
}

void SetMistFade() // 264 SET_MIST_FADE
{
	// The mist's size and alpha (0 to 255) go from their starts to their ends over the seconds given, a step each turn
	const auto duration = Popf();
	const auto endTransparency = Popf();
	const auto startTransparency = Popf();
	const auto endScale = Popf();
	const auto startScale = Popf();
	const auto mist = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (mist == entt::null || !registry.Valid(mist))
	{
		ScriptMessage("Thing deleted");
		return;
	}
	if (!registry.AllOf<ecs::components::Mist>(mist))
	{
		ScriptMessage("Must be mist object!");
		return;
	}
	Locator::mistSystem::value().SetFade(mist, startScale, endScale, startTransparency, endTransparency, duration);
}

void GetObjectFade() // 265 GET_OBJECT_FADE
{
	// How much of its script fade a mist did, from 0 to 1. Any other object gives 1.
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ScriptMessage("Thing deleted");
		Pushf(1.0f);
		return;
	}
	const auto* mist = registry.TryGet<const ecs::components::Mist>(object);
	if (mist == nullptr)
	{
		ScriptMessage("Must be mist object!");
		Pushf(1.0f);
		return;
	}
	// A mist that no script faded has no fade turns, so it gives 0 / 0 as a fade of 0 seconds does
	Pushf(mists::FadeDone(mist->fadeElapsed, mist->fadeTurns));
}

void PlayHandDemo() // 266 PLAY_HAND_DEMO
{
	const auto withoutHandModify = Pop().intVal != 0;
	const auto withPause = Pop().intVal != 0;
	const auto name = PopString();
	Locator::handDemoSystem::value().Play(name, CurrentTask(), withPause, withoutHandModify);
}

void IsPlayingHandDemo() // 267 IS_PLAYING_HAND_DEMO
{
	// The scripts' "hand demonstration played": true when none plays
	Pushb(!Locator::handDemoSystem::value().IsPlaying(0));
}

void GetInteractionMagnitude() // 270 GET_INTERACTION_MAGNITUDE
{
	// How strongly the player last rewarded (above 0) or punished (below 0) the creature
	const auto creature = CreatureForNative(PopObject());
	Pushf(creature.has_value() && Locator::creatureMindSystem::has_value()
	          ? Locator::creatureMindSystem::value().GetInteractionMagnitude(*creature)
	          : 0.0f);
}

void CreateHighlight() // 272 CREATE_HIGHLIGHT
{
	const auto challenge = Pop().uintVal;
	const auto position = PopVec();
	const auto kind = Pop().uintVal;
	// A kind past the info table makes none (the game reads past its table)
	const auto highlight = Locator::scriptHighlightSystem::value().Create(kind, position, challenge);
	if (highlight == entt::null)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Highlight not created");
		PushObject(entt::null);
		return;
	}
	RegisterCreated(highlight);
	PushObject(highlight);
}

void GetObjectHeld273() // 273 GET_OBJECT_HELD
{
	// What a creature holds in its hand, if anything
	const auto creature = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (creature == entt::null || !registry.Valid(creature))
	{
		ScriptMessage("No creature");
		PushObject(entt::null);
		return;
	}
	if (!registry.AllOf<ecs::components::Creature>(creature))
	{
		ScriptMessage("Not creature");
		PushObject(entt::null);
		return;
	}
	const auto held = Locator::creatureObjectActionSystem::has_value()
	                      ? Locator::creatureObjectActionSystem::value().GetCarried(creature)
	                      : std::nullopt;
	if (!held.has_value())
	{
		PushObject(entt::null);
		return;
	}
	Locator::scriptObjects::value().Register(*held, false);
	PushObject(*held);
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

void SetSkeleton() // 291 SET_SKELETON
{
	const auto object = PopObject();
	const auto enable = Pop().intVal != 0;
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ScriptMessage("Thing not valid");
		return;
	}
	// A group makes each of its villagers a skeleton, or themselves again; anything but a villager is left as it is
	if (IsScriptContainer(registry, object))
	{
		for (const auto member : ContainerMembers(registry, object))
		{
			villager_skeleton::SetByScript(member, enable);
		}
		return;
	}
	villager_skeleton::SetByScript(object, enable);
}

void IsSkeleton() // 292 IS_SKELETON
{
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ScriptMessage("Thing not valid");
		Pushb(false);
		return;
	}
	// Only a villager a script made a skeleton is one; a body lying as bones is not
	Pushb(villager_skeleton::Is(object));
}

void PlayerSpellCastTime() // 293 PLAYER_SPELL_CAST_TIME
{
	// The seconds since the last cast of the player. The record of the last cast always keeps its start values.
	const auto* player = ScriptPlayer(Popf());
	if (player == nullptr)
	{
		Pushf(script::cast_history::k_NoPlayerTime);
		return;
	}
	const auto turn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
	constexpr auto k_MillisecondsPerTurn = static_cast<int32_t>(ecs::systems::TimeSystemInterface::k_TurnDuration.count());
	Pushf(script::cast_history::SecondsSinceCast(static_cast<uint32_t>(turn), {}, k_MillisecondsPerTurn));
}

void PlayerSpellLastCast() // 294 PLAYER_SPELL_LAST_CAST
{
	// The magic type of the last cast of the player, from the record that always keeps its start values
	const auto* player = ScriptPlayer(Popf());
	Pushi(player != nullptr ? static_cast<int32_t>(script::cast_history::LastCast {}.type) : 0);
}

void GetLastSpellCastPos() // 295 GET_LAST_SPELL_CAST_POS
{
	// The place of the last cast of the player, from the record that always keeps its start values. The height is the
	// height of the land at the place, plus the height of the record above the land.
	const auto* player = ScriptPlayer(Popf());
	glm::vec3 place {0.0f};
	if (player != nullptr)
	{
		constexpr script::cast_history::LastCast k_Cast {};
		const map_coords::MapCoords coords {k_Cast.x, k_Cast.z};
		place = Locator::terrainSystem::has_value()
		            ? map_coords::ToWorld(Locator::terrainSystem::value(), coords)
		            : glm::vec3(map_coords::ToMetres(coords).x, 0.0f, map_coords::ToMetres(coords).y);
		place.y += k_Cast.height;
	}
	PushVec(place);
}

void AddSpotVisualTargetPos() // 296 ADD_SPOT_VISUAL_TARGET_POS
{
	// The visual's rules also act on the point, such as a beam that ends there
	const auto position = PopVec();
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "ADD_SPOT_VISUAL_TARGET_POS: thing not valid");
		return;
	}
	// A thing that holds no visual of a script takes no target
	if (const auto effect = ecs::script_spot_visuals::EffectOf(registry, object);
	    effect.has_value() && Locator::effectRequestSystem::has_value())
	{
		Locator::effectRequestSystem::value().AddTargetPosition(*effect, position);
	}
}

void AddSpotVisualTargetObject() // 297 ADD_SPOT_VISUAL_TARGET_OBJECT
{
	// The visual's rules also act on the target, such as a beam that follows it while it moves
	const auto target = PopObject();
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "ADD_SPOT_VISUAL_TARGET_OBJECT: thing not valid");
		return;
	}
	// No target, or a thing that holds no visual of a script, adds nothing
	if (!registry.Valid(target))
	{
		return;
	}
	if (const auto effect = ecs::script_spot_visuals::EffectOf(registry, object);
	    effect.has_value() && Locator::effectRequestSystem::has_value())
	{
		Locator::effectRequestSystem::value().AddTarget(*effect, target);
	}
}

void SetIndestructable() // 298 SET_INDESTRUCTABLE
{
	const auto object = PopObject();
	const bool indestructible = (Pop().uintVal & 1u) != 0;
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_INDESTRUCTABLE: thing not valid");
		return;
	}
	const auto mark = [&registry, indestructible](entt::entity thing) {
		if (indestructible)
		{
			registry.AssignOrReplace<ecs::components::Indestructible>(thing);
		}
		else
		{
			registry.Remove<ecs::components::Indestructible>(thing);
		}
	};
	// A town is marked through every villager of it; anything else itself
	if (registry.AllOf<ecs::components::Town>(object))
	{
		registry.Each<const ecs::components::Villager>(
		    [&mark, object](entt::entity villager, const ecs::components::Villager& person) {
			    if (person.town == object)
			    {
				    mark(villager);
			    }
		    });
		return;
	}
	mark(object);
}

void ImmersionExists() // 304 IMMERSION_EXISTS
{
	// openblack drives no force-feedback mouse, so there is never a device for the scripts' effects
	Pushb(false);
}

void SetDrawHighlight() // 306 SET_DRAW_HIGHLIGHT
{
	// The challenge scrolls show or hide; the signs always show
	Locator::chlapi::value().SetHighlightDrawOn(Pop().intVal != 0);
}

void SetOpenClose() // 307 SET_OPEN_CLOSE
{
	const auto object = PopObject();
	// The script's word is kept as it is: 1 opens, 0 closes
	const auto open = static_cast<int32_t>(Pop().uintVal);
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_OPEN_CLOSE: thing not found");
		return;
	}
	if (!Locator::animatedStaticSystem::has_value() || !Locator::animatedStaticSystem::value().SetOpenState(object, open))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_OPEN_CLOSE: thing must be an animated static");
	}
}

void SetIntroBuilding() // 308 SET_INTRO_BUILDING
{
	// The lowest bit of the value tells the builders that need wood to walk away from their sites and wait for it
	Locator::entitiesRegistry::value().Context().scriptBuildersWaitForWood = (Pop().uintVal & 1u) != 0;
}

void MoveComputerPlayerPosition() // 310 MOVE_COMPUTER_PLAYER_POSITION
{
	const auto withFixedHeight = Pop().intVal != 0;
	const auto speed = Popf();
	const auto position = PopVec();
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	if (Locator::computerPlayerSystem::has_value())
	{
		Locator::computerPlayerSystem::value().MoveHand(player, position, speed, withFixedHeight);
	}
}

void EnableDisableComputerPlayer311() // 311 ENABLE_DISABLE_COMPUTER_PLAYER
{
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	const auto enable = Pop().intVal != 0;
	if (Locator::computerPlayerSystem::has_value())
	{
		Locator::computerPlayerSystem::value().SetActive(player, enable);
	}
}

void GetComputerPlayerPosition() // 312 GET_COMPUTER_PLAYER_POSITION
{
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	// A player with no computer mind gives the origin
	std::optional<glm::vec3> position;
	if (Locator::computerPlayerSystem::has_value())
	{
		position = Locator::computerPlayerSystem::value().HandPosition(player);
	}
	PushVec(position.value_or(glm::vec3(0.0f)));
}

void SetComputerPlayerPosition() // 313 SET_COMPUTER_PLAYER_POSITION
{
	const auto withFixedHeight = Pop().intVal != 0;
	const auto position = PopVec();
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	if (Locator::computerPlayerSystem::has_value())
	{
		Locator::computerPlayerSystem::value().SetHand(player, position, withFixedHeight);
	}
}

void CallNearInState() // 316 CALL_NEAR_IN_STATE
{
	// The nearest living thing of the type and subtype in the state, in the cells within the radius of the place
	const auto excludingScripted = Pop().intVal != 0;
	const auto radius = Popf();
	const auto position = PopVec();
	const auto state = Pop().uintVal;
	const auto subtype = Pop().uintVal;
	const auto type = Pop().intVal;
	const auto found = FindForScript(type, subtype, position, radius, excludingScripted, state);
	Pusho(found == entt::null ? 0u : static_cast<uint32_t>(found));
}

void SetSunDraw() // 319 SET_SUN_DRAW
{
	// Any value other than 0 hides the sun and its glare, and 0 shows them. The name of the native says the opposite
	// (see docs/vanilla-bugs.md).
	const auto hide = Pop().uintVal != 0;
	if (Locator::skySystem::has_value())
	{
		Locator::skySystem::value().SetSunHidden(hide);
	}
}

void ObjectInfoBits() // 320 OBJECT_INFO_BITS
{
	// What the gate stones laid in a plinth are worth: ape 1, tiger 2, cow 4. Asked of anything else, the original
	// reports the error and pushes no answer at all, so the script's next pop finds whatever lies below.
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "OBJECT_INFO_BITS: thing not valid");
		return;
	}
	const auto value = Locator::animatedStaticSystem::has_value()
	                       ? Locator::animatedStaticSystem::value().GateStoneValue(object)
	                       : std::nullopt;
	if (!value.has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "OBJECT_INFO_BITS: thing must be an animated static");
		return;
	}
	Pushf(static_cast<float>(*value));
}

void SetHurtByFire() // 321 SET_HURT_BY_FIRE
{
	const auto object = PopObject();
	const auto enable = static_cast<bool>(Pop().intVal);
	if (Locator::fireSystem::has_value())
	{
		Locator::fireSystem::value().SetHurtByFire(object, enable);
	}
}

void ConfinedObject() // 322 CONFINED_OBJECT
{
	// The creature is kept within the radius of the point, on the ground, while no working leash is on it
	const auto radius = Popf();
	const auto point = PopVec();
	const auto creature = CreatureForNative(PopObject());
	if (!creature.has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto& leash = registry.TryGet<ecs::components::CreatureLeash>(*creature) != nullptr
	                  ? registry.Get<ecs::components::CreatureLeash>(*creature)
	                  : registry.Assign<ecs::components::CreatureLeash>(*creature);
	const auto area = script::creature_rules::ConfinementFor(point, radius);
	leash.confinementCentre = area.centre;
	leash.confinementRadius = area.radius;
}

void ClearConfinedObject() // 323 CLEAR_CONFINED_OBJECT
{
	// The creature is free of the area it is kept in
	const auto creature = CreatureForNative(PopObject());
	if (!creature.has_value())
	{
		return;
	}
	if (auto* leash = Locator::entitiesRegistry::value().TryGet<ecs::components::CreatureLeash>(*creature))
	{
		leash->confinementRadius = 0.0f;
	}
}

void GetObjectFlock() // 324 GET_OBJECT_FLOCK
{
	// The flock of a living thing, none when it is in no flock
	const auto object = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object) || !IsLivingThing(object))
	{
		ScriptMessage("Cannot find flock");
		Pusho(0);
		return;
	}
	const auto flock = ecs::script_flocks::FlockOf(registry, object);
	if (flock == entt::null || !registry.Valid(flock))
	{
		Pusho(0);
		return;
	}
	Locator::scriptObjects::value().Register(flock, false);
	Pusho(static_cast<uint32_t>(flock));
}

void SetPlayerBelief() // 325 SET_PLAYER_BELIEF
{
	// The town's belief in the player becomes a share of its belief in its owner
	const auto share = Popf();
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	const auto object = PopObject();
	auto* impression = ScriptTownImpression(object);
	if (impression == nullptr || !ScriptNamesPlayer(player))
	{
		return;
	}
	const auto& town = Locator::entitiesRegistry::value().Get<const ecs::components::Town>(object);
	magic::town_belief::SetRelativeToOwner(impression->belief, town.owner, player, share);
}

void PlayJcSpecial() // 326 PLAY_JC_SPECIAL
{
	const auto special = Pop().intVal;
	switch (special)
	{
	case 14:
	case 15:
		// The camera's bookmarks are shown and taken again, or put away
		Locator::cameraBookmarkSystem::value().SetEnabled(special == 14);
		return;
	case 0:
	case 1:
	case 2:
	case 4:
	case 5:
		// The opening's light from the sky, the camera chasing it, and the hand that lifts the boy from the sea and sets
		// him down beside his parents
		Locator::introSystem::value().Play(special);
		return;
	case 3:
		// TODO(opening): the clouds' effect, which no script uses
		NotImplemented(special);
		return;
	case 6:
		// The explorers push their boat into the sea and sail away
		Locator::boatLaunchSystem::value().Play();
		return;
	default:
		// The others do nothing
		return;
	}
}

void IsPlayingJcSpecial() // 327 IS_PLAYING_JC_SPECIAL
{
	// Every special counts as playing, but for whether the hand's lifting clip has come round
	constexpr int32_t k_LiftComeRound = 13;
	const auto special = Pop().intVal;
	Pushb(special != k_LiftComeRound || Locator::introSystem::value().HasLiftFinished());
}

/// Whether a script's thing is one in the world with a component
template <typename Component>
static bool IsThingWith(entt::entity thing)
{
	const auto& registry = Locator::entitiesRegistry::value();
	return thing != entt::null && registry.Valid(thing) && registry.AllOf<Component>(thing);
}

void VortexParameters() // 328 VORTEX_PARAMETERS
{
	// A vortex that leads into the land is told the town those who come out join, where its animals gather in a flock,
	// kept within a distance and a radius, and the flock its people follow. A thing given for the town or the flock that
	// is none takes their place as nothing. Without a vortex nothing is set; a vortex without a town is still told the rest
	const auto flock = PopObject();
	const auto radius = Popf();
	const auto distance = Popf();
	const auto place = PopVec();
	const auto town = PopObject();
	const auto vortex = PopObject();
	const bool isVortex = IsThingWith<ecs::components::Vortex>(vortex);
	const bool isTown = IsThingWith<ecs::components::Town>(town);
	if (!isTown || !isVortex)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Thing not valid");
		if (!isVortex)
		{
			return;
		}
	}
	Locator::vortexSystem::value().SetArrival(vortex,
	                                          {.town = isTown ? town : entt::null,
	                                           .gatheringPlace = place,
	                                           .flockDistance = distance,
	                                           .flockRadius = radius,
	                                           .flock = IsThingWith<ecs::components::Flock>(flock) ? flock : entt::null});
}

/// Whether an icon at the sites of a player's temple has prayer power in its store, for one magic type or for any
bool PlayersIconStores(PlayerNames player, std::optional<MagicType> type)
{
	if (!Locator::worshipSiteSystem::has_value() || !Locator::spellIconSystem::has_value())
	{
		return false;
	}
	const auto temple = Locator::worshipSiteSystem::value().TempleOf(player);
	return temple != entt::null && Locator::spellIconSystem::value().AnyIconStores(temple, type);
}

void IsSpellCharging() // 330 IS_SPELL_CHARGING
{
	// A miracle charges when an icon of the player has prayer power in its store, whatever hand asked for it
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	Pushb(PlayersIconStores(player, std::nullopt));
}

void IsThatSpellCharging() // 331 IS_THAT_SPELL_CHARGING
{
	// As IS_SPELL_CHARGING, for the icons whose plain miracle is of the magic type
	const auto type = static_cast<MagicType>(Pop().intVal);
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	Pushb(PlayersIconStores(player, type));
}

void FlockWithinLimits() // 333 FLOCK_WITHIN_LIMITS
{
	// Whether each member of a flock is within the flock's distance of the flock. A thing that is not a flock gives true.
	const auto object = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ScriptMessage("Thing no longer valid");
		Pushb(true);
		return;
	}
	if (!ecs::script_flocks::IsFlock(registry, object))
	{
		ScriptMessage("Invalid thing");
		Pushb(true);
		return;
	}
	Pushb(ecs::script_flocks::AllMembersWithinFlockDistance(registry, object));
}

void HighlightProperties() // 334 HIGHLIGHT_PROPERTIES
{
	const auto category = Pop().uintVal;
	const auto text = Pop().uintVal;
	const auto object = PopObject();
	if (object == entt::null || !Locator::entitiesRegistry::value().AllOf<ecs::components::ScriptHighlight>(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Thing not valid");
		return;
	}
	Locator::scriptHighlightSystem::value().SetProperties(object, text, category);
}

void HandDemoTrigger() // 336 HAND_DEMO_TRIGGER
{
	// Whether the demonstration reached a mark; asking lets a demonstration held there go on
	Pushb(Locator::handDemoSystem::value().TakeTrigger());
}

void SetTownDesireBoost() // 341 SET_TOWN_DESIRE_BOOST
{
	// A town wants one of its desires that much more, or less, until a script changes it again; its order of desires is
	// put right at once
	const auto boost = Popf();
	const auto desire = Pop().intVal;
	const auto town = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	const bool isTown = town != entt::null && registry.Valid(town) && registry.AllOf<ecs::components::Town>(town);
	if (!isTown)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_TOWN_DESIRE_BOOST: object not a town");
	}
	if (!ecs::town_desire::ValidScriptBoost(desire, boost))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_TOWN_DESIRE_BOOST: desire {} or boost {} out of range", desire,
		                    boost);
		return;
	}
	if (isTown)
	{
		Locator::townDesireSystem::value().SetBoost(town, static_cast<TownDesireInfo>(desire), boost, true);
	}
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

void ComputerPlayerReady() // 344 COMPUTER_PLAYER_READY
{
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	Pushb(Locator::computerPlayerSystem::has_value() && Locator::computerPlayerSystem::value().Ready(player));
}

void EnableDisableComputerPlayer345() // 345 ENABLE_DISABLE_COMPUTER_PLAYER
{
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	const auto pause = Pop().intVal != 0;
	if (Locator::computerPlayerSystem::has_value())
	{
		Locator::computerPlayerSystem::value().SetPaused(player, pause);
	}
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

void ThingJcSpecial() // 349 THING_JC_SPECIAL
{
	const auto target = PopObject();
	const auto special = static_cast<ecs::high_detail_rules::ThingSpecial>(Pop().intVal);
	const auto on = Pop().intVal != 0;
	if (target == entt::null || !Locator::entitiesRegistry::value().Valid(target))
	{
		ObjectNoLongerValid();
		return;
	}
	if (special == ecs::high_detail_rules::ThingSpecial::NeverFade)
	{
		// The one order any thing takes, drawn in high detail or not: it stays in view however far off it is, whatever
		// `on` says
		Locator::entitiesRegistry::value().AssignOrReplace<ecs::components::NeverFades>(target);
		return;
	}
	// The other orders are for a thing drawn in high detail and are ignored by anything else
	const bool drawnInHighDetail = Locator::highDetailSystem::value().Order(target, special, on);
	if (drawnInHighDetail && special == ecs::high_detail_rules::ThingSpecial::ReleaseIntroSpecials)
	{
		// Given to one drawn in high detail, the opening's light, its hand and its chasing camera all go
		Locator::introSystem::value().ReleaseAll();
	}
}

void UpdateSnapshotPicture() // 351 UPDATE_SNAPSHOT_PICTURE
{
	KeepChallengeRecord(PopChallengeSnapshot(script::challenge_snapshots::Call::Picture),
	                    script::challenge_snapshots::Call::Picture);
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

void CreateRandomVillagerOfTribe() // 353 CREATE_RANDOM_VILLAGER_OF_TRIBE
{
	const auto position = PopVec();
	const auto tribe = Pop().intVal;
	// Any of the tribe's kinds of villager, a grown-up, belonging to no town and left to decide for itself what to do
	const auto draw = Locator::gameRandom::value().GameRand(script::create_rules::k_VillagersPerTribe);
	const auto& infos = Locator::infoConstants::value().villager;
	const auto row = script::create_rules::TribeVillagerRow(tribe, draw, infos.size());
	if (!row.has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Villager not created");
		Pusho(0);
		return;
	}
	const auto villager = VillagerArchetype::Create(position, position, static_cast<VillagerInfo>(*row),
	                                                infos.at(*row).grownUpAge, VillagerArchetype::Joins::Nobody);
	RegisterCreated(villager);
	Pusho(static_cast<uint32_t>(villager));
}

/// Whether a thing is a worship site
bool IsWorshipSite(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(object) && registry.AllOf<ecs::components::WorshipSite>(object);
}

void GameSetMana() // 355 GAME_SET_MANA
{
	// A worship site, or the site of a spell icon, gets this prayer power in its battery. Its icons lose their stores.
	const auto mana = Popf();
	const auto object = PopObject();
	const bool icon = Locator::spellIconSystem::has_value() && Locator::spellIconSystem::value().IsIcon(object);
	if (!IsWorshipSite(object) && !icon)
	{
		ScriptMessage("Thing not valid!");
		return;
	}
	if (!Locator::spellIconSystem::has_value() || !Locator::worshipSiteSystem::has_value())
	{
		return;
	}
	auto& icons = Locator::spellIconSystem::value();
	const auto site = IsWorshipSite(object) ? object : icons.SiteOfIcon(object);
	if (site == entt::null)
	{
		return;
	}
	icons.EmptyIconStores(site);
	Locator::worshipSiteSystem::value().SetStoredChants(site, mana);
}

void SetMagicProperties() // 356 SET_MAGIC_PROPERTIES
{
	// A dispenser is given the miracle it holds and the seconds between its bubbles; for none it keeps its building's
	// period, and with no turns between them it is turned off
	const auto seconds = Popf();
	const auto number = Pop().intVal;
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Thing not valid");
		return;
	}
	const auto* dispenser = registry.TryGet<const ecs::components::SpellDispenser>(object);
	if (dispenser == nullptr)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Thing must be a dispenser");
		return;
	}
	const auto type = script::create_rules::MagicTypeFromScript(number);
	if (!type.has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Invalid magic type {}", number);
		return;
	}
	const auto& building = Locator::infoConstants::value().abode.at(static_cast<size_t>(dispenser->building));
	const auto turns = script::create_rules::DispenserTurns(seconds, building.timeEachMobileObjectTakesToProduce);
	Locator::magicSystem::value().SetDispenserMagic(object, *type, turns);
}

void SexIsMale() // 358 SEX_IS_MALE
{
	// Only a villager can be female: anything else, or a thing that has gone, answers male
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	const auto* villager =
	    object != entt::null && registry.Valid(object) ? registry.TryGet<const ecs::components::Villager>(object) : nullptr;
	Pushb(villager == nullptr || villager->sex == ecs::components::Villager::Sex::MALE);
}

void IsActive() // 361 IS_ACTIVE
{
	const auto object = PopObject();
	if (object == entt::null)
	{
		ObjectNoLongerValid();
		Pushb(false);
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	if (const auto* highlight = registry.TryGet<const ecs::components::ScriptHighlight>(object); highlight != nullptr)
	{
		Pushb(highlight->active);
		return;
	}
	if (const auto* dispenser = registry.TryGet<const ecs::components::SpellDispenser>(object); dispenser != nullptr)
	{
		Pushb(dispenser->timer.active);
		return;
	}
	// A reward is active once the player has tapped it open
	if (registry.AllOf<ecs::components::Reward>(object))
	{
		Pushb(Locator::rewardSystem::has_value() && Locator::rewardSystem::value().IsOpened(object));
		return;
	}
	// Nothing else is ever active
	Pushb(false);
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

void SetComputerPlayerPersonality() // 364 SET_COMPUTER_PLAYER_PERSONALITY
{
	const auto weight = Popf();
	const auto aspect = PopString();
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	if (Locator::computerPlayerSystem::has_value())
	{
		Locator::computerPlayerSystem::value().SetPersonality(player, aspect, weight);
	}
}

void SetComputerPlayerSuppression() // 365 SET_COMPUTER_PLAYER_SUPPRESSION
{
	const auto suppression = Popf();
	const auto aspect = PopString();
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	if (Locator::computerPlayerSystem::has_value())
	{
		Locator::computerPlayerSystem::value().SetSuppression(player, aspect, suppression);
	}
}

void ForceComputerPlayerAction() // 366 FORCE_COMPUTER_PLAYER_ACTION
{
	const auto second = PopObject();
	const auto first = PopObject();
	const auto action = PopString();
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	if (Locator::computerPlayerSystem::has_value())
	{
		std::ignore = Locator::computerPlayerSystem::value().ForceAction(player, action, first, second);
	}
}

void QueueComputerPlayerAction() // 367 QUEUE_COMPUTER_PLAYER_ACTION
{
	const auto second = PopObject();
	const auto first = PopObject();
	const auto action = PopString();
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	if (Locator::computerPlayerSystem::has_value())
	{
		Locator::computerPlayerSystem::value().QueueAction(player, action, first, second);
	}
}

void GetTownWithId() // 368 GET_TOWN_WITH_ID
{
	// The town the land's script made with this number, whoever owns it now
	const auto id = ecs::script_find::TownIdFromScript(Popf());
	auto& registry = Locator::entitiesRegistry::value();
	const auto& towns = registry.Context().towns;
	const auto found = towns.find(id);
	if (found == towns.end() || !registry.Valid(found->second))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Town not found");
		Pusho(0);
		return;
	}
	Locator::scriptObjects::value().Register(found->second, false);
	Pusho(static_cast<uint32_t>(found->second));
}

void SetDisciple() // 369 SET_DISCIPLE
{
	// Whether with a sound: the game takes it and makes none
	Pop();
	const auto number = Pop().intVal;
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		ObjectNoLongerValid();
		return;
	}
	if (!registry.AllOf<ecs::components::Villager>(object))
	{
		ScriptMessage("Thing must be villager");
		return;
	}
	// The villager takes up the work at once, glowing as a disciple, and sets about it when it next decides
	if (const auto type = disciple::FromScript(number); type.has_value())
	{
		ecs::villager_disciple::SetType(object, *type);
	}
}

void ReleaseComputerPlayer() // 370 RELEASE_COMPUTER_PLAYER
{
	// The mind continues and releases the hand
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	if (Locator::computerPlayerSystem::has_value())
	{
		Locator::computerPlayerSystem::value().SetPaused(player, false);
	}
}

void SetComputerPlayerSpeed() // 371 SET_COMPUTER_PLAYER_SPEED
{
	const auto speed = Popf();
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	if (Locator::computerPlayerSystem::has_value())
	{
		Locator::computerPlayerSystem::value().SetSpeed(player, speed);
	}
}

void CallComputerPlayer() // 374 CALL_COMPUTER_PLAYER
{
	// Each player has a computer player from its start. The script gets the player's entity, which keeps the state of
	// the computer player.
	const auto name = ScriptPlayerName(static_cast<int32_t>(Popf()));
	const auto player =
	    Locator::playerSystem::has_value() ? Locator::playerSystem::value().FindPlayer(name) : std::optional<entt::entity> {};
	if (!player.has_value() || !Locator::entitiesRegistry::value().Valid(*player))
	{
		ScriptMessage("Could not get computer player!");
		Pusho(0);
		return;
	}
	Locator::scriptObjects::value().Register(*player, false);
	Pusho(static_cast<uint32_t>(*player));
}

void CallBuildingInTown() // 375 CALL_BUILDING_IN_TOWN
{
	// The newest building of the town of the kind given that is built at least as much as given and that no script
	// holds. Without "excluding scripted" the game gives the newest building of the town of any kind (vanilla-bugs 927).
	const bool excludingScripted = Pop().intVal != 0;
	const auto minBuilt = Popf();
	const auto town = PopObject();
	const auto number = static_cast<AbodeNumber>(Pop().intVal);
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* data =
	    town != entt::null && registry.Valid(town) ? registry.TryGet<const ecs::components::Town>(town) : nullptr;
	if (data == nullptr)
	{
		ScriptMessage("Must be town");
		Pusho(0);
		return;
	}
	std::vector<ecs::script_containers::TownBuilding> buildings;
	for (const auto entity : data->abodes)
	{
		if (!registry.Valid(entity))
		{
			continue;
		}
		const auto* abode = registry.TryGet<const ecs::components::Abode>(entity);
		const auto* progress = registry.TryGet<const ecs::components::BuildProgress>(entity);
		buildings.push_back({.entity = entity,
		                     .number = abode != nullptr ? abode->type : AbodeNumber::Invalid,
		                     .built = progress != nullptr ? progress->built : 1.0f,
		                     .inScript = registry.AllOf<ecs::components::InScript>(entity)});
	}
	const auto found = ecs::script_containers::BuildingInTown(buildings, number, minBuilt, excludingScripted);
	if (found == entt::null)
	{
		Pusho(0);
		return;
	}
	Locator::scriptObjects::value().Register(found, false);
	Pusho(static_cast<uint32_t>(found));
}

void SetCanBuildWorshipsite() // 376 SET_CAN_BUILD_WORSHIPSITE
{
	// A town or a temple is let have worship sites made, or stopped
	const auto object = PopObject();
	const auto enable = Pop().intVal != 0;
	if (Locator::worshipSiteSystem::has_value())
	{
		Locator::worshipSiteSystem::value().SetCanHaveSites(object, enable);
	}
}

void SetComputerPlayerAttitude() // 378 SET_COMPUTER_PLAYER_ATTITUDE
{
	const auto attitude = Popf();
	const auto other = ScriptPlayerName(static_cast<int32_t>(Popf()));
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	// The mind of the first player now thinks this of the second; a number that names no player does nothing
	if (ScriptNamesPlayer(player) && ScriptNamesPlayer(other) && Locator::computerPlayerSystem::has_value())
	{
		Locator::computerPlayerSystem::value().SetAttitude(player, other, attitude);
	}
}

void GetComputerPlayerAttitude() // 379 GET_COMPUTER_PLAYER_ATTITUDE
{
	const auto other = ScriptPlayerName(static_cast<int32_t>(Popf()));
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	// A number that names no player gives 0
	float attitude = 0.0f;
	if (ScriptNamesPlayer(player) && ScriptNamesPlayer(other) && Locator::computerPlayerSystem::has_value())
	{
		attitude = Locator::computerPlayerSystem::value().Attitude(player, other);
	}
	Pushf(attitude);
}

void LoadComputerPlayerPersonality() // 380 LOAD_COMPUTER_PLAYER_PERSONALITY
{
	const auto file = PopString();
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	// The mind of the player gets the weights of the file. A missing file or a number that names no player does nothing.
	if (ScriptNamesPlayer(player) && Locator::computerPlayerSystem::has_value())
	{
		Locator::computerPlayerSystem::value().LoadPersonality(player, file);
	}
}

void SaveComputerPlayerPersonality() // 381 SAVE_COMPUTER_PLAYER_PERSONALITY
{
	const auto file = PopString();
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	// The file gets the weights of the mind of the player. A number that names no player does nothing.
	if (ScriptNamesPlayer(player) && Locator::computerPlayerSystem::has_value())
	{
		Locator::computerPlayerSystem::value().SavePersonality(player, file);
	}
}

void SetPlayerAlly() // 382 SET_PLAYER_ALLY
{
	const auto strength = Popf();
	const auto other = ScriptPlayerName(static_cast<int32_t>(Popf()));
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	// The two players are allies of each other with this strength; a number that names no player does nothing
	if (ScriptNamesPlayer(player) && ScriptNamesPlayer(other) && Locator::playerSystem::has_value())
	{
		Locator::playerSystem::value().SetAlly(other, player, strength);
	}
}

void CallFlying() // 383 CALL_FLYING
{
	// The first thing of the type and subtype in the physics within the radius of the place, measured across the land:
	// the search examines the bodies in the order of the physics' list
	const auto excludingScripted = Pop().intVal != 0;
	const auto radius = Popf();
	const auto position = PopVec();
	const auto subtype = Pop().uintVal;
	const auto type = Pop().intVal;
	if (!Locator::dynamicsSystem::has_value())
	{
		Pusho(0);
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto at = map_coords::FromMetres({position.x, position.z});
	const auto objectType = static_cast<ObjectType>(type);
	entt::entity found = entt::null;
	Locator::dynamicsSystem::value().ForEachEntry([&](const ecs::PhysicsEntry& entry) {
		const auto thing = entry.entity;
		if (found != entt::null || !registry.Valid(thing) || !registry.AllOf<Transform>(thing))
		{
			return;
		}
		// The excluding variant does not use a thing that a script holds already
		if (excludingScripted && registry.AllOf<ecs::components::InScript>(thing))
		{
			return;
		}
		const auto kind = ecs::script_find::KindOf(registry, thing);
		if (!kind.has_value() || !ecs::script_find::Matches(*kind, objectType, subtype))
		{
			return;
		}
		if (gutils::GetDistanceInMetres(at, ScriptFindPosition(registry, thing)) <= radius)
		{
			found = thing;
		}
	});
	if (found == entt::null)
	{
		Pusho(0);
		return;
	}
	Locator::scriptObjects::value().Register(found, false);
	Pusho(static_cast<uint32_t>(found));
}

void SetObjectFadeIn() // 384 SET_OBJECT_FADE_IN
{
	// A creature drops out of sight at once (with the energise sound) and fizzes back in over the seconds given. The
	// game fades nothing else in: any other object is only reported.
	const auto seconds = Popf();
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object dead man!");
		return;
	}
	if (!registry.AllOf<ecs::components::Creature>(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_OBJECT_FADE_IN: only creatures fade in");
		return;
	}
	auto& fizz = Locator::creatureFizzSystem::value();
	fizz.SetFizz(object, 1.0f, 0.0f, false);
	fizz.SetFizz(object, 0.0f, seconds, false);
}

void IsAffectedBySpell() // 385 IS_AFFECTED_BY_SPELL
{
	// Whether a creature is under a spell of the magic type now. A magic type that casts no spell on a creature gives no.
	const auto spell = static_cast<MagicType>(Pop().intVal);
	const auto object = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		ScriptMessage("Object dead man!");
		Pushb(false);
		return;
	}
	const auto* spells = registry.TryGet<const ecs::components::CreatureSpells>(object);
	if (!registry.AllOf<ecs::components::Creature>(object) || spells == nullptr)
	{
		ScriptMessage("Object must be a creature!");
		Pushb(false);
		return;
	}
	const auto creatureSpell = creature_spells::SpellOf(spell);
	Pushb(creatureSpell.has_value() && spells->spells.IsActive(*creatureSpell));
}

void SetMagicInObject() // 386 SET_MAGIC_IN_OBJECT
{
	// A town gets a magic type or loses it. A power-up level does not give its plain miracle with it.
	const auto object = PopObject();
	const auto type = static_cast<MagicType>(Pop().intVal);
	const auto enable = Pop().intVal != 0;
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		ScriptMessage("Thing not valid");
		return;
	}
	if (!registry.AllOf<ecs::components::Town>(object))
	{
		ScriptMessage("Object should be town");
		return;
	}
	if (!Locator::spellIconSystem::has_value())
	{
		return;
	}
	auto& icons = Locator::spellIconSystem::value();
	if (enable)
	{
		icons.TownHolds(object, type);
	}
	else
	{
		icons.TownReleases(object, type);
	}
}

/// Whether a thing is a building of the buildings' table for the adult counts of the scripts: an abode or a miracle
/// dispenser
static bool IsAdultHolderBuilding(const ecs::Registry& registry, entt::entity object)
{
	return registry.AnyOf<ecs::components::Abode, ecs::components::SpellDispenser>(object);
}

/// The grown villagers of a town now
static uint32_t TownAdultsNow(const ecs::Registry& registry, entt::entity town)
{
	uint32_t adults = 0;
	registry.Each<const ecs::components::Villager>([town, &adults](const ecs::components::Villager& villager) {
		if (villager.town == town && villager.lifeStage != ecs::components::Villager::LifeStage::Child)
		{
			++adults;
		}
	});
	return adults;
}

/// The room for adults and for children in the built buildings of a town
struct TownRoom
{
	int32_t adults {0};
	int32_t children {0};
};

/// The room in the built buildings of a town now: the places of each building of its tribe's table that has a place
/// for anyone, as the town stats count them
static TownRoom TownRoomNow(const ecs::Registry& registry, entt::entity townEntity, const ecs::components::Town& town)
{
	const auto* tribe = registry.TryGet<const Tribe>(townEntity);
	if (tribe == nullptr || !Locator::infoConstants::has_value())
	{
		return {};
	}
	const auto& rows = Locator::infoConstants::value().abode;
	TownRoom room;
	registry.Each<const ecs::components::Abode>([&](entt::entity entity, const ecs::components::Abode& abode) {
		if (abode.townId != town.id)
		{
			return;
		}
		const auto* progress = registry.TryGet<const ecs::components::BuildProgress>(entity);
		if (progress != nullptr && progress->built < 1.0f)
		{
			return;
		}
		const auto row = std::ranges::find_if(
		    rows, [&](const GAbodeInfo& info) { return info.tribeType == *tribe && info.abodeNumber == abode.type; });
		if (row != rows.end() && row->maxVillagersInAbode + row->maxChildrenInAbode != 0)
		{
			room.adults += static_cast<int32_t>(row->maxVillagersInAbode);
			room.children += static_cast<int32_t>(row->maxChildrenInAbode);
		}
	});
	return room;
}

/// The adults of a town or a building, or the room for them, as ID_ADULT_SIZE and OBJECT_ADULT_CAPACITY give it. Any
/// other thing gives 0 and a message.
static void PushAdultCount(bool room)
{
	const auto object = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ScriptMessage("Object should be alive!");
		Pushf(0.0f);
		return;
	}
	if (const auto* town = registry.TryGet<const ecs::components::Town>(object); town != nullptr)
	{
		// The town keeps these counts up to date at all times, so they are counted now and not taken from the turn's
		// town stats
		Pushf(room ? ecs::script_containers::TownAdultRoom(TownRoomNow(registry, object, *town).adults)
		           : ecs::script_containers::TownAdults(TownAdultsNow(registry, object)));
		return;
	}
	if (!IsAdultHolderBuilding(registry, object))
	{
		ScriptMessage("Object should be town, or abode");
		Pushf(0.0f);
		return;
	}
	if (room)
	{
		const auto* info = ecs::world_objects::AbodeInfoOf(object);
		Pushf(ecs::script_containers::AbodeAdultRoom(info != nullptr ? info->maxVillagersInAbode : 0));
		return;
	}
	const bool lived = registry.AllOf<ecs::components::Abode>(object);
	Pushf(ecs::script_containers::AbodeAdults(lived ? ecs::town_homes::HouseholdOf(registry, object).adults : 0));
}

void IdAdultSize() // 387 ID_ADULT_SIZE
{
	PushAdultCount(false);
}

void ObjectCapacity() // 388 OBJECT_CAPACITY
{
	// The room for adults and children of a town or of a building. Any other thing gives 0 and a message.
	const auto object = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ScriptMessage("Object should be alive!");
		Pushf(0.0f);
		return;
	}
	if (const auto* town = registry.TryGet<const ecs::components::Town>(object); town != nullptr)
	{
		const auto room = TownRoomNow(registry, object, *town);
		Pushf(ecs::script_containers::TownCapacity(room.adults, room.children));
		return;
	}
	if (!IsAdultHolderBuilding(registry, object))
	{
		ScriptMessage("Object should be town, or abode");
		Pushf(0.0f);
		return;
	}
	const auto* info = ecs::world_objects::AbodeInfoOf(object);
	Pushf(info != nullptr ? ecs::script_containers::AbodeCapacity(info->maxVillagersInAbode, info->maxChildrenInAbode) : 0.0f);
}

void ObjectAdultCapacity() // 389 OBJECT_ADULT_CAPACITY
{
	PushAdultCount(true);
}

void SquareRoot() // 397 SQUARE_ROOT
{
	const auto value = Popf();
	auto root = 0.0f;
	if (value > 0.0f)
	{
		root = std::sqrt(value);
	}
	Pushf(root);
}

void GetPlayerAlly() // 398 GET_PLAYER_ALLY
{
	const auto other = ScriptPlayerName(static_cast<int32_t>(Popf()));
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	// The strength that the second player keeps for the first; a number that names no player gives 0
	float strength = 0.0f;
	if (ScriptNamesPlayer(player) && ScriptNamesPlayer(other) && Locator::playerSystem::has_value())
	{
		strength = Locator::playerSystem::value().AllyStrength(other, player);
	}
	Pushf(strength);
}

/// The player a script names: its 0 is the neutral player, the others count from 1
PlayerNames ScriptPlayerName(int32_t scriptPlayer)
{
	return script::script_players::NameOf(scriptPlayer);
}

ecs::components::Player* ScriptPlayer(float number)
{
	const auto name = ScriptPlayerName(static_cast<int32_t>(number));
	if (!Locator::playerSystem::has_value())
	{
		return nullptr;
	}
	const auto entity = Locator::playerSystem::value().FindPlayer(name).value_or(entt::null);
	auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(entity) ? registry.TryGet<ecs::components::Player>(entity) : nullptr;
}

void SetPlayerWindResistance() // 399 SET_PLAYER_WIND_RESISTANCE
{
	auto* player = ScriptPlayer(Popf());
	const auto resistance = Pop().uintVal;
	// What the player's hand throws flies without the air's drag while it is set
	if (player != nullptr)
	{
		player->windResistance = resistance;
	}
	Pushi(0);
}

void GetPlayerWindResistance() // 400 GET_PLAYER_WIND_RESISTANCE
{
	const auto* player = ScriptPlayer(Popf());
	// Of its two arguments the game reads only the player
	Pop();
	Pushi(player != nullptr ? static_cast<int32_t>(player->windResistance) : 0);
}

void PauseUnpauseClimateSystem() // 401 PAUSE_UNPAUSE_CLIMATE_SYSTEM
{
	const auto enable = Pop().intVal != 0;
	if (Locator::weatherSystem::has_value())
	{
		Locator::weatherSystem::value().SetClimateSystemEnabled(enable);
	}
}

void PauseUnpauseStormCreationInClimateSystem() // 402 PAUSE_UNPAUSE_STORM_CREATION_IN_CLIMATE_SYSTEM
{
	const auto enable = Pop().intVal != 0;
	if (Locator::weatherSystem::has_value())
	{
		Locator::weatherSystem::value().SetStormCreationEnabled(enable);
	}
}

void GetManaForSpell() // 403 GET_MANA_FOR_SPELL
{
	// The prayer power that a miracle of the magic type costs to create
	const auto spell = Pop().intVal;
	if (spell < 0 || static_cast<size_t>(spell) >= magic::k_MagicTypeCount || !Locator::infoConstants::has_value())
	{
		Pushf(0.0f);
		return;
	}
	Pushf(magic::GetChantsRequiredToCreate(Locator::infoConstants::value(), static_cast<MagicType>(spell)));
}

void KillStormsInArea() // 404 KILL_STORMS_IN_AREA
{
	const auto radius = Popf();
	const auto position = PopVec();
	// A miracle's storm comes back over its clouds at the next step, fading in again
	if (Locator::weatherSystem::has_value())
	{
		Locator::weatherSystem::value().KillStormsInArea(position, radius);
	}
}

void InsideTemple() // 405 INSIDE_TEMPLE
{
	Pushb(PlayerInsideTemple());
}

void RestartObject() // 406 RESTART_OBJECT
{
	// A maze puzzle starts again. Any other thing does not change.
	const auto object = PopObject();
	if (object == entt::null || !Locator::entitiesRegistry::value().Valid(object))
	{
		ScriptMessage("Object should be alive!");
		return;
	}
	if (Locator::puzzleGameSystem::has_value())
	{
		Locator::puzzleGameSystem::value().Restart(object);
	}
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

void GetTownWorshipDeaths() // 410 GET_TOWN_WORSHIP_DEATHS
{
	// How many of the town's people have died of chanting at worship; 0, with an error, for what isn't a town
	const auto object = PopObject();
	const auto* town =
	    object != entt::null ? Locator::entitiesRegistry::value().TryGet<const ecs::components::Town>(object) : nullptr;
	if (town == nullptr)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GET_TOWN_WORSHIP_DEATHS: town invalid");
		Pushf(0.0f);
		return;
	}
	Pushf(static_cast<float>(ecs::villager_after_death::DeathsOf(object, DeathReason::Chant)));
}

void GetHandState() // 413 GET_HAND_STATE
{
	hand::script_state::Facts facts;
	facts.interfaceActive =
	    !Locator::cinematicDirectorSystem::has_value() || Locator::cinematicDirectorSystem::value().IsInterfaceActive();
	facts.inTemple = PlayerInsideTemple();
	// The grip of the land, the other button and the turn of the camera come from the view report of the player
	if (const auto view = ScriptView(); view.has_value())
	{
		facts.gripsLand = view->gripsLand;
		facts.otherButton = view->otherButton;
		facts.cameraTurned = view->cameraTurned;
	}
	if (Locator::cameraHelpSystem::has_value())
	{
		facts.cameraFeatures = Locator::cameraHelpSystem::value().Get().features;
	}
	Pushi(static_cast<int32_t>(hand::script_state::StateOf(facts)));
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

void GetPlayerTownTotal() // 417 GET_PLAYER_TOWN_TOTAL
{
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	// The number of towns that the player has now
	const auto& registry = Locator::entitiesRegistry::value();
	Pushf(static_cast<float>(ecs::player_towns::Total(registry, player)));
}

void KeyDown() // 419 KEY_DOWN
{
	// The key comes by the game's number of the key. The view report of the player tells the keys held.
	const auto key = Pop().intVal;
	const auto view = ScriptView();
	Pushb(view.has_value() && script::lesson_natives::KeyDown(key, view->keysHeld));
}

void GetObjectClicked() // 421 GET_OBJECT_CLICKED
{
	const auto* clicked = LocalHandClicked();
	const auto object = clicked != nullptr ? clicked->thing : entt::entity {entt::null};
	if (object == entt::null || !Locator::entitiesRegistry::value().Valid(object))
	{
		Pusho(0);
		return;
	}
	// The thing found takes a place in the scripts' table, the game's own
	Locator::scriptObjects::value().Register(object, false);
	Pusho(static_cast<uint32_t>(object));
}

void GetMana() // 422 GET_MANA
{
	// The prayer power in the battery of a worship site. Only a site has it, not a spell icon.
	const auto object = PopObject();
	if (!IsWorshipSite(object))
	{
		ScriptMessage("Thing not valid!");
		Pushf(0.0f);
		return;
	}
	Pushf(Locator::worshipSiteSystem::has_value() ? Locator::worshipSiteSystem::value().StoredChants(object) : 0.0f);
}

void ClearPlayerSpellCharging() // 423 CLEAR_PLAYER_SPELL_CHARGING
{
	// At the sites of the player's temple, each icon that the first player charges stops its charge
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	if (!Locator::playerSystem::has_value() || !Locator::worshipSiteSystem::has_value() ||
	    !Locator::spellIconSystem::has_value())
	{
		return;
	}
	const auto temple = Locator::worshipSiteSystem::value().TempleOf(player);
	if (temple == entt::null)
	{
		return;
	}
	Locator::spellIconSystem::value().CancelChargesFor(temple, Locator::playerSystem::value().GetLocalPlayer());
}

void GetTotemStatue() // 425 GET_TOTEM_STATUE
{
	// A town centre gives the totem that stands on it. Any other thing, or a centre with no totem, gives nothing.
	const auto centre = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	entt::entity found = entt::null;
	if (centre != entt::null && registry.Valid(centre))
	{
		registry.Each<const ecs::components::VillageTotem>(
		    [centre, &found](entt::entity totem, const ecs::components::VillageTotem& component) {
			    if (component.townCentre == centre && found == entt::null)
			    {
				    found = totem;
			    }
		    });
	}
	if (found == entt::null)
	{
		Pusho(0);
		return;
	}
	Locator::scriptObjects::value().Register(found, false);
	Pusho(static_cast<uint32_t>(found));
}

void SetSetOnFire() // 426 SET_SET_ON_FIRE
{
	const auto object = PopObject();
	const auto enable = static_cast<bool>(Pop().intVal);
	if (Locator::fireSystem::has_value())
	{
		Locator::fireSystem::value().SetCanBeSetOnFire(object, enable);
	}
}

void SetLandBalance() // 427 SET_LAND_BALANCE
{
	// One of the eight balances of the land, as the land script sets it. The game writes outside the eight balances for
	// an index out of the range; openblack ignores such an index (vanilla-bugs 928).
	const auto value = Popf();
	const auto index = static_cast<int32_t>(Popf());
	lhscriptx::FeatureScriptCommands::SetGlobalLandBalance(index, value);
}

void SetObjectBeliefScale() // 428 SET_OBJECT_BELIEF_SCALE
{
	// The scale of the belief that a town gives to each player at its turn
	const auto scale = Popf();
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	auto* impression = object != entt::null && registry.Valid(object) && registry.AllOf<ecs::components::Town>(object)
	                       ? registry.TryGet<ecs::components::TownImpression>(object)
	                       : nullptr;
	if (impression == nullptr)
	{
		ScriptMessage("Town invalid!");
		return;
	}
	impression->belief.scale = scale;
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

void SetObjectCarrying() // 440 SET_OBJECT_CARRYING
{
	// A villager holds the object the script gives. The next change of its state animation can replace it, but not
	// while the villager is under the script.
	const auto carried = ecs::script_containers::CarriedObjectFromScript(Pop().uintVal);
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	auto* villager =
	    object != entt::null && registry.Valid(object) ? registry.TryGet<ecs::components::Villager>(object) : nullptr;
	if (villager == nullptr)
	{
		ScriptMessage("Villager invalid!");
		return;
	}
	villager->carried = static_cast<CarriedObject>(carried);
}

void GetTimeSinceObjectAttacked() // 442 GET_TIME_SINCE_OBJECT_ATTACKED
{
	const auto town = PopObject();
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	auto& registry = Locator::entitiesRegistry::value();
	// Only a town keeps a record of attacks. Any other thing gives the largest float.
	const auto* aggression = registry.Valid(town) ? registry.TryGet<const ecs::components::TownAggression>(town) : nullptr;
	if (aggression == nullptr || !Locator::time::has_value())
	{
		Pushf(std::numeric_limits<float>::max());
		return;
	}
	const auto msPerTurn =
	    std::chrono::duration_cast<std::chrono::milliseconds>(ecs::systems::TimeSystemInterface::k_TurnDuration);
	Pushf(ecs::town_aggression::SecondsSinceAttacked(aggression->record, player, Locator::time::value().GetTurn(),
	                                                 static_cast<uint32_t>(msPerTurn.count())));
}

void GetTownAndVillagerHealthTotal() // 443 GET_TOWN_AND_VILLAGER_HEALTH_TOTAL
{
	// The life of a town's buildings and of all its people added up: each building's people and then the building, the
	// buildings in the scripts' order, then the homeless
	const auto town = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* data = registry.Valid(town) ? registry.TryGet<const ecs::components::Town>(town) : nullptr;
	if (data == nullptr)
	{
		ScriptMessage("Invalid objects");
		Pushf(0.0f);
		return;
	}
	std::vector<script::town_health::AbodeLives> abodes;
	for (const auto abode : data->abodes)
	{
		const auto* building = registry.Valid(abode) ? registry.TryGet<const ecs::components::Abode>(abode) : nullptr;
		if (building == nullptr)
		{
			continue;
		}
		auto& lives = abodes.emplace_back(script::town_health::AbodeLives {.life = ecs::world_objects::LifeOf(abode)});
		std::ranges::transform(building->inhabitants, std::back_inserter(lives.people),
		                       [](entt::entity person) { return ecs::world_objects::LifeOf(person); });
	}
	std::vector<float> homeless;
	std::ranges::transform(data->homelessVillagers, std::back_inserter(homeless),
	                       [](entt::entity person) { return ecs::world_objects::LifeOf(person); });
	Pushf(script::town_health::Total(abodes, homeless));
}

void GameAddForBuilding() // 444 GAME_ADD_FOR_BUILDING
{
	// The town gets the building site of the thing as one of its own sites, so its builders work at it too. The site
	// stays the site of the town of the building.
	const auto town = PopObject();
	const auto building = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	const bool isTown = town != entt::null && registry.Valid(town) && registry.AllOf<ecs::components::Town>(town);
	if (building == entt::null || !isTown || !ecs::construction::ShareSite(building, town))
	{
		ScriptMessage("Invalid objects");
	}
}

/// What the search for the dead looks at in a thing
static script::dead_living::LivingFacts DeadLivingFactsOf(const ecs::Registry& registry, entt::entity thing)
{
	using script::dead_living::LivingFacts;
	if (registry.AllOf<ecs::components::Villager>(thing))
	{
		const auto* action = registry.TryGet<const ecs::components::LivingAction>(thing);
		if (action == nullptr || !Locator::livingActionSystem::has_value())
		{
			return {};
		}
		return {.kind = LivingFacts::Kind::Villager,
		        .deathMark = registry.AllOf<ecs::components::VillagerDeath>(thing),
		        .topState = action->states.at(static_cast<size_t>(ecs::components::LivingAction::Index::Top)),
		        .finalState = static_cast<uint8_t>(Locator::livingActionSystem::value().VillagerGetFinalState(*action))};
	}
	if (const auto* animal = registry.TryGet<const ecs::components::Animal>(thing))
	{
		// Of the animal states, only the dead state has the game's number of the dead state
		return {.kind = LivingFacts::Kind::Animal,
		        .deathMark = animal->dying,
		        .topState =
		            animal->state == ecs::components::AnimalState::Dead ? script::dead_living::k_DeadState : uint8_t {0},
		        .life = animal->life};
	}
	return {};
}

void GetDeadLiving() // 446 GET_DEAD_LIVING
{
	// The nearest dead villager or animal in the cells within the radius of the point. A thing farther than the radius
	// in these cells also counts. The script then holds the thing.
	const auto radius = Popf();
	const auto position = PopVec();
	const auto at = map_coords::FromMetres({position.x, position.z});
	const auto found = ecs::script_find::FindNearest(at, radius, [](glm::ivec2 cell) {
		const auto& registry = Locator::entitiesRegistry::value();
		std::vector<ecs::script_find::Candidate> dead;
		for (const auto thing : Locator::entitiesMap::value().GetAllInCell(cell))
		{
			if (registry.Valid(thing) && registry.AllOf<Transform>(thing) &&
			    script::dead_living::IsDead(DeadLivingFactsOf(registry, thing)))
			{
				dead.push_back({.entity = thing, .at = ScriptFindPosition(registry, thing)});
			}
		}
		return dead;
	});
	if (found == entt::null)
	{
		Pusho(0);
		return;
	}
	Locator::scriptObjects::value().Register(found, false);
	Pusho(static_cast<uint32_t>(found));
}

void GetSacrificeTotal() // 449 GET_SACRIFICE_TOTAL
{
	// All the prayer power that sacrifices at a worship site's altar have stored there; 0 for anything but a site
	const auto site = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* chants = registry.Valid(site) ? registry.TryGet<const ecs::components::WorshipChants>(site) : nullptr;
	Pushf(chants != nullptr ? chants->sacrificed : 0.0f);
}

/// A player's temple, the earliest made when there are several, none when the player has none
std::optional<entt::entity> PlayersTemple(PlayerNames player)
{
	std::optional<entt::entity> found;
	Locator::entitiesRegistry::value().Each<const ecs::components::Temple>(
	    [player, &found](entt::entity entity, const ecs::components::Temple& temple) {
		    if (temple.owner == player && (!found.has_value() || entity < *found))
		    {
			    found = entity;
		    }
	    });
	return found;
}

void GetTemplePosition() // 451 GET_TEMPLE_POSITION
{
	// Where a player's temple stands, or nowhere (all zero) without one
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	glm::vec3 position {0.0f};
	if (const auto templeEntity = PlayersTemple(player); templeEntity.has_value())
	{
		position = Locator::entitiesRegistry::value().Get<const Transform>(*templeEntity).position;
	}
	PushVec(position);
}

void GetSpellIconInTemple() // 453 GET_SPELL_ICON_IN_TEMPLE
{
	// The icon of a magic type's first seed at the worship sites of a temple
	const auto temple = PopObject();
	const auto type = static_cast<MagicType>(Pop().intVal);
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(temple) || !registry.AllOf<ecs::components::Temple>(temple))
	{
		ScriptMessage("Invalid Temple");
		Pusho(0);
		return;
	}
	if (!Locator::spellIconSystem::has_value() || !Locator::infoConstants::has_value())
	{
		Pusho(0);
		return;
	}
	const auto seed = magic::FindFirstSpellSeedForMagicType(Locator::infoConstants::value(), type);
	const auto icon = seed.has_value() ? Locator::spellIconSystem::value().IconInTemple(temple, *seed) : entt::null;
	if (icon == entt::null)
	{
		Pusho(0);
		return;
	}
	// The icon takes a place in the scripts' table, not as a thing that a script made
	Locator::scriptObjects::value().Register(icon, false);
	Pusho(static_cast<uint32_t>(icon));
}

void GameClearComputerPlayerActions() // 454 GAME_CLEAR_COMPUTER_PLAYER_ACTIONS
{
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	if (Locator::computerPlayerSystem::has_value())
	{
		Locator::computerPlayerSystem::value().ClearActions(player);
	}
}

/// The first thing in a town, a flock or a dance for a script, or nothing
static entt::entity FirstInContainer(const ecs::Registry& registry, entt::entity container)
{
	if (const auto* town = registry.TryGet<const ecs::components::Town>(container); town != nullptr)
	{
		const auto* building = !town->abodes.empty() && registry.Valid(town->abodes.front())
		                           ? registry.TryGet<const ecs::components::Abode>(town->abodes.front())
		                           : nullptr;
		return ecs::script_containers::FirstInTown(building != nullptr ? std::span<const entt::entity>(building->inhabitants)
		                                                               : std::span<const entt::entity> {},
		                                           town->homelessVillagers);
	}
	if (ecs::script_flocks::IsFlock(registry, container))
	{
		const auto& members = registry.Get<const ecs::components::Flock>(container).members;
		return members.empty() ? entt::entity {entt::null} : members.front();
	}
	if (ecs::dances::IsDance(registry, container))
	{
		return ecs::dance_rules::FirstForScript(registry.Get<const ecs::components::Dance>(container).groups);
	}
	return entt::null;
}

/// Pushes a thing that a container gave, or nothing
static void PushContainerThing(entt::entity thing)
{
	if (thing == entt::null)
	{
		Pusho(0);
		return;
	}
	Locator::scriptObjects::value().Register(thing, false);
	Pusho(static_cast<uint32_t>(thing));
}

/// Takes the container from the stack and pushes its first thing
static void PushFirstInContainer()
{
	const auto container = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	if (container == entt::null || !IsScriptContainer(registry, container))
	{
		ScriptMessage("Invalid container");
		Pusho(0);
		return;
	}
	PushContainerThing(FirstInContainer(registry, container));
}

void GetFirstInContainer() // 455 GET_FIRST_IN_CONTAINER
{
	PushFirstInContainer();
}

void GetNextInContainer() // 456 GET_NEXT_IN_CONTAINER
{
	// With no thing to start after, or one that is gone, the script gets the first thing of the container
	const auto after = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	if (after == entt::null || !registry.Valid(after))
	{
		PushFirstInContainer();
		return;
	}
	const auto container = PopObject();
	if (container == entt::null || !IsScriptContainer(registry, container))
	{
		ScriptMessage("Invalid container");
		Pusho(0);
		return;
	}
	if (ecs::dances::IsDance(registry, container))
	{
		// The game walks a list of the dance's things that nothing fills while the game runs, so a script never gets
		// a next thing in a dance
		Pusho(0);
		return;
	}
	PushContainerThing(ecs::script_containers::NextAfter(ContainerMembers(registry, container), after));
}

void GetTempleEntrancePosition() // 457 GET_TEMPLE_ENTRANCE_POSITION
{
	// A point out from a player's temple towards its entrance, at a distance and a height, or nowhere (all zero)
	// without a temple
	const auto height = Popf();
	const auto radius = Popf();
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	const auto templeEntity = PlayersTemple(player);
	if (!templeEntity.has_value())
	{
		PushVec(glm::vec3(0.0f));
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& transform = registry.Get<const Transform>(*templeEntity);
	// The entrance is the place its model marks for it, turned and placed with the temple, raised or lowered as the
	// land under it is from the land under the temple; a model without it has its entrance at the temple
	auto entrance = transform.position;
	if (const auto* mesh = registry.TryGet<const ecs::components::Mesh>(*templeEntity); mesh != nullptr)
	{
		auto& meshes = Locator::resources::value().GetMeshes();
		if (meshes.Contains(mesh->id) && meshes.Handle(mesh->id)->GetExtraMetrics().size() > openblack::temple::k_EntrancePlace)
		{
			const auto& place = meshes.Handle(mesh->id)->GetExtraMetrics().at(openblack::temple::k_EntrancePlace);
			entrance = transform.position + (transform.rotation * (transform.scale * glm::vec3(place[3])));
			const auto& land = Locator::terrainSystem::value();
			entrance.y +=
			    land.GetHeightAt({entrance.x, entrance.z}) - land.GetHeightAt({transform.position.x, transform.position.z});
		}
	}
	PushVec(openblack::temple::TowardsEntrance(transform.position, entrance, radius, height));
}

void SetHandDemoKeys() // 459 SET_HAND_DEMO_KEYS
{
	// The game does nothing with it
	static_cast<void>(Pop());
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

void CHLApi::InitFunctionsTable0()
{
	CREATE_FUNCTION_BINDING("NONE", 0, 0, None);
	CREATE_FUNCTION_BINDING("SET_CAMERA_POSITION", 3, 0, SetCameraPosition);
	CREATE_FUNCTION_BINDING("SET_CAMERA_FOCUS", 3, 0, SetCameraFocus);
	CREATE_FUNCTION_BINDING("MOVE_CAMERA_POSITION", 4, 0, MoveCameraPosition);
	CREATE_FUNCTION_BINDING("MOVE_CAMERA_FOCUS", 4, 0, MoveCameraFocus);
	CREATE_FUNCTION_BINDING("GET_CAMERA_POSITION", 0, 3, GetCameraPosition);
	CREATE_FUNCTION_BINDING("GET_CAMERA_FOCUS", 0, 3, GetCameraFocus);
	CREATE_FUNCTION_BINDING("SPIRIT_EJECT", 1, 0, SpiritEject);
	CREATE_FUNCTION_BINDING("SPIRIT_HOME", 1, 0, SpiritHome);
	CREATE_FUNCTION_BINDING("SPIRIT_POINT_POS", 5, 0, SpiritPointPos);
	CREATE_FUNCTION_BINDING("SPIRIT_POINT_GAME_THING", 3, 0, SpiritPointGameThing);
	CREATE_FUNCTION_BINDING("GAME_THING_FIELD_OF_VIEW", 1, 1, GameThingFieldOfView);
	CREATE_FUNCTION_BINDING("POS_FIELD_OF_VIEW", 3, 1, PosFieldOfView);
	CREATE_FUNCTION_BINDING("RUN_TEXT", 3, 0, RunText);
	CREATE_FUNCTION_BINDING("TEMP_TEXT", 3, 0, TempText);
	CREATE_FUNCTION_BINDING("TEXT_READ", 0, 1, TextRead);
	CREATE_FUNCTION_BINDING("GAME_THING_CLICKED", 1, 1, GameThingClicked);
	CREATE_FUNCTION_BINDING("SET_SCRIPT_STATE", 2, 0, SetScriptState);
	CREATE_FUNCTION_BINDING("SET_SCRIPT_STATE_POS", 4, 0, SetScriptStatePos);
	CREATE_FUNCTION_BINDING("SET_SCRIPT_FLOAT", 2, 0, SetScriptFloat);
	CREATE_FUNCTION_BINDING("SET_SCRIPT_ULONG", 3, 0, SetScriptUlong);
	CREATE_FUNCTION_BINDING("GET_PROPERTY", 2, 1, GetProperty);
	CREATE_FUNCTION_BINDING("SET_PROPERTY", 3, 0, SetProperty);
	CREATE_FUNCTION_BINDING("GET_POSITION", 1, 3, GetPosition);
	CREATE_FUNCTION_BINDING("SET_POSITION", 4, 0, SetPosition);
	CREATE_FUNCTION_BINDING("GET_DISTANCE", 6, 1, GetDistance);
	CREATE_FUNCTION_BINDING("CALL", 6, 1, Call);
	CREATE_FUNCTION_BINDING("CREATE", 5, 1, Create);
	CREATE_FUNCTION_BINDING("RANDOM", 2, 1, Random);
	CREATE_FUNCTION_BINDING("DLL_GETTIME", 0, 1, DllGettime);
	CREATE_FUNCTION_BINDING("START_CAMERA_CONTROL", 0, 1, StartCameraControl);
	CREATE_FUNCTION_BINDING("END_CAMERA_CONTROL", 0, 0, EndCameraControl);
	CREATE_FUNCTION_BINDING("SET_WIDESCREEN", 1, 0, SetWidescreen);
	CREATE_FUNCTION_BINDING("MOVE_GAME_THING", 5, 0, MoveGameThing);
	CREATE_FUNCTION_BINDING("SET_FOCUS", 4, 0, SetFocus);
	CREATE_FUNCTION_BINDING("HAS_CAMERA_ARRIVED", 0, 1, HasCameraArrived);
	CREATE_FUNCTION_BINDING("FLOCK_CREATE", 3, 1, FlockCreate);
	CREATE_FUNCTION_BINDING("FLOCK_ATTACH", 3, 1, FlockAttach);
	CREATE_FUNCTION_BINDING("FLOCK_DETACH", 2, 1, FlockDetach);
	CREATE_FUNCTION_BINDING("FLOCK_DISBAND", 1, 0, FlockDisband);
	CREATE_FUNCTION_BINDING("ID_SIZE", 1, 1, IdSize);
	CREATE_FUNCTION_BINDING("FLOCK_MEMBER", 2, 1, FlockMember);
	CREATE_FUNCTION_BINDING("GET_HAND_POSITION", 0, 3, GetHandPosition);
	CREATE_FUNCTION_BINDING("PLAY_SOUND_EFFECT", 6, 0, PlaySoundEffect);
	CREATE_FUNCTION_BINDING("START_MUSIC", 1, 0, StartMusic);
	CREATE_FUNCTION_BINDING("STOP_MUSIC", 0, 0, StopMusic);
	CREATE_FUNCTION_BINDING("ATTACH_MUSIC", 2, 0, AttachMusic);
	CREATE_FUNCTION_BINDING("DETACH_MUSIC", 1, 0, DetachMusic);
	CREATE_FUNCTION_BINDING("OBJECT_DELETE", 2, 0, ObjectDelete);
	CREATE_FUNCTION_BINDING("FOCUS_FOLLOW", 1, 0, FocusFollow);
	CREATE_FUNCTION_BINDING("POSITION_FOLLOW", 1, 0, PositionFollow);
	CREATE_FUNCTION_BINDING("CALL_NEAR", 7, 1, CallNear);
	CREATE_FUNCTION_BINDING("SPECIAL_EFFECT_POSITION", 5, 1, SpecialEffectPosition);
	CREATE_FUNCTION_BINDING("SPECIAL_EFFECT_OBJECT", 3, 1, SpecialEffectObject);
	CREATE_FUNCTION_BINDING("DANCE_CREATE", 6, 1, DanceCreate);
	CREATE_FUNCTION_BINDING("CALL_IN", 4, 1, CallIn);
	CREATE_FUNCTION_BINDING("CHANGE_INNER_OUTER_PROPERTIES", 4, 0, ChangeInnerOuterProperties);
	CREATE_FUNCTION_BINDING("SNAPSHOT", -1, 0, Snapshot);
	CREATE_FUNCTION_BINDING("GET_ALIGNMENT", 1, 1, GetAlignment);
	CREATE_FUNCTION_BINDING("SET_ALIGNMENT", 2, 0, SetAlignment);
	CREATE_FUNCTION_BINDING("INFLUENCE_OBJECT", 4, 1, InfluenceObject);
	CREATE_FUNCTION_BINDING("INFLUENCE_POSITION", 6, 1, InfluencePosition);
	CREATE_FUNCTION_BINDING("GET_INFLUENCE", 5, 1, GetInfluence);
	CREATE_FUNCTION_BINDING("SET_INTERFACE_INTERACTION", 1, 0, SetInterfaceInteraction);
	CREATE_FUNCTION_BINDING("PLAYED", 1, 1, Played);
	CREATE_FUNCTION_BINDING("RANDOM_ULONG", 2, 1, RandomUlong);
	CREATE_FUNCTION_BINDING("SET_GAMESPEED", 1, 0, SetGamespeed);
	CREATE_FUNCTION_BINDING("CALL_IN_NEAR", 8, 1, CallInNear);
	CREATE_FUNCTION_BINDING("OVERRIDE_STATE_ANIMATION", 2, 0, OverrideStateAnimation);
	CREATE_FUNCTION_BINDING("CREATURE_CREATE_RELATIVE_TO_CREATURE", 6, 1, CreatureCreateRelativeToCreature);
	CREATE_FUNCTION_BINDING("CREATURE_LEARN_EVERYTHING", 1, 0, CreatureLearnEverything);
	CREATE_FUNCTION_BINDING("CREATURE_SET_KNOWS_ACTION", 4, 0, CreatureSetKnowsAction);
	CREATE_FUNCTION_BINDING("CREATURE_SET_AGENDA_PRIORITY", 2, 0, CreatureSetAgendaPriority);
	CREATE_FUNCTION_BINDING("CREATURE_TURN_OFF_ALL_DESIRES", 1, 0, CreatureTurnOffAllDesires);
	CREATE_FUNCTION_BINDING("CREATURE_LEARN_DISTINCTION_ABOUT_ACTIVITY_OBJECT", 4, 0,
	                        CreatureLearnDistinctionAboutActivityObject);
	CREATE_FUNCTION_BINDING("CREATURE_DO_ACTION", 4, 0, CreatureDoAction);
	CREATE_FUNCTION_BINDING("IN_CREATURE_HAND", 2, 1, InCreatureHand);
	CREATE_FUNCTION_BINDING("CREATURE_SET_DESIRE_VALUE", 3, 0, CreatureSetDesireValue);
	CREATE_FUNCTION_BINDING("CREATURE_SET_DESIRE_ACTIVATED", 3, 0, CreatureSetDesireActivated78);
	CREATE_FUNCTION_BINDING("CREATURE_SET_DESIRE_ACTIVATED", 2, 0, CreatureSetDesireActivated79);
	CREATE_FUNCTION_BINDING("CREATURE_SET_DESIRE_MAXIMUM", 3, 0, CreatureSetDesireMaximum);
	CREATE_FUNCTION_BINDING("CONVERT_CAMERA_POSITION", 1, 3, ConvertCameraPosition);
	CREATE_FUNCTION_BINDING("CONVERT_CAMERA_FOCUS", 1, 3, ConvertCameraFocus);
	CREATE_FUNCTION_BINDING("CREATURE_SET_PLAYER", 1, 0, CreatureSetPlayer);
	CREATE_FUNCTION_BINDING("START_COUNTDOWN_TIMER", 1, 0, StartCountdownTimer);
	CREATE_FUNCTION_BINDING("CREATURE_INITIALISE_NUM_TIMES_PERFORMED_ACTION", 2, 0, CreatureInitialiseNumTimesPerformedAction);
	CREATE_FUNCTION_BINDING("CREATURE_GET_NUM_TIMES_ACTION_PERFORMED", 2, 1, CreatureGetNumTimesActionPerformed);
	CREATE_FUNCTION_BINDING("REMOVE_COUNTDOWN_TIMER", 0, 0, RemoveCountdownTimer);
	CREATE_FUNCTION_BINDING("GET_OBJECT_DROPPED", 1, 1, GetObjectDropped);
	CREATE_FUNCTION_BINDING("CLEAR_DROPPED_BY_OBJECT", 1, 0, ClearDroppedByObject);
	CREATE_FUNCTION_BINDING("CREATE_REACTION", 2, 0, CreateReaction);
	CREATE_FUNCTION_BINDING("REMOVE_REACTION", 1, 0, RemoveReaction);
	CREATE_FUNCTION_BINDING("GET_COUNTDOWN_TIMER", 0, 1, GetCountdownTimer);
	CREATE_FUNCTION_BINDING("START_DUAL_CAMERA", 2, 0, StartDualCamera);
	CREATE_FUNCTION_BINDING("UPDATE_DUAL_CAMERA", 2, 0, UpdateDualCamera);
	CREATE_FUNCTION_BINDING("RELEASE_DUAL_CAMERA", 0, 0, ReleaseDualCamera);
	CREATE_FUNCTION_BINDING("SET_CREATURE_HELP", 1, 0, SetCreatureHelp);
	CREATE_FUNCTION_BINDING("GET_TARGET_OBJECT", 1, 1, GetTargetObject);
	CREATE_FUNCTION_BINDING("CREATURE_DESIRE_IS", 2, 1, CreatureDesireIs);
	CREATE_FUNCTION_BINDING("COUNTDOWN_TIMER_EXISTS", 0, 1, CountdownTimerExists);
	CREATE_FUNCTION_BINDING("LOOK_GAME_THING", 2, 0, LookGameThing);
	CREATE_FUNCTION_BINDING("GET_OBJECT_DESTINATION", 1, 3, GetObjectDestination);
	CREATE_FUNCTION_BINDING("CREATURE_FORCE_FINISH", 1, 0, CreatureForceFinish);
	CREATE_FUNCTION_BINDING("HIDE_COUNTDOWN_TIMER", 0, 0, HideCountdownTimer);
	CREATE_FUNCTION_BINDING("GET_ACTION_TEXT_FOR_OBJECT", 1, 1, GetActionTextForObject);
	CREATE_FUNCTION_BINDING("CREATE_DUAL_CAMERA_WITH_POINT", 4, 0, CreateDualCameraWithPoint);
	CREATE_FUNCTION_BINDING("SET_CAMERA_TO_FACE_OBJECT", 2, 0, SetCameraToFaceObject);
	CREATE_FUNCTION_BINDING("MOVE_CAMERA_TO_FACE_OBJECT", 3, 0, MoveCameraToFaceObject);
}

void CHLApi::InitFunctionsTable1()
{
	CREATE_FUNCTION_BINDING("GET_MOON_PERCENTAGE", 0, 1, GetMoonPercentage);
	CREATE_FUNCTION_BINDING("POPULATE_CONTAINER", 4, 0, PopulateContainer);
	CREATE_FUNCTION_BINDING("ADD_REFERENCE", 1, 1, AddReference);
	CREATE_FUNCTION_BINDING("REMOVE_REFERENCE", 1, 1, RemoveReference);
	CREATE_FUNCTION_BINDING("SET_GAME_TIME", 1, 0, SetGameTime);
	CREATE_FUNCTION_BINDING("GET_GAME_TIME", 0, 1, GetGameTime);
	CREATE_FUNCTION_BINDING("GET_REAL_TIME", 0, 1, GetRealTime);
	CREATE_FUNCTION_BINDING("GET_REAL_DAY", 0, 1, GetRealDay115);
	CREATE_FUNCTION_BINDING("GET_REAL_DAY", 0, 1, GetRealDay116);
	CREATE_FUNCTION_BINDING("GET_REAL_MONTH", 0, 1, GetRealMonth);
	CREATE_FUNCTION_BINDING("GET_REAL_YEAR", 0, 1, GetRealYear);
	CREATE_FUNCTION_BINDING("RUN_CAMERA_PATH", 1, 0, RunCameraPath);
	CREATE_FUNCTION_BINDING("START_DIALOGUE", 0, 1, StartDialogue);
	CREATE_FUNCTION_BINDING("END_DIALOGUE", 0, 0, EndDialogue);
	CREATE_FUNCTION_BINDING("IS_DIALOGUE_READY", 0, 1, IsDialogueReady);
	CREATE_FUNCTION_BINDING("CHANGE_WEATHER_PROPERTIES", 6, 0, ChangeWeatherProperties);
	CREATE_FUNCTION_BINDING("CHANGE_LIGHTNING_PROPERTIES", 5, 0, ChangeLightningProperties);
	CREATE_FUNCTION_BINDING("CHANGE_TIME_FADE_PROPERTIES", 3, 0, ChangeTimeFadeProperties);
	CREATE_FUNCTION_BINDING("CHANGE_CLOUD_PROPERTIES", 4, 0, ChangeCloudProperties);
	CREATE_FUNCTION_BINDING("SET_HEADING_AND_SPEED", 5, 0, SetHeadingAndSpeed);
	CREATE_FUNCTION_BINDING("START_GAME_SPEED", 0, 0, StartGameSpeed);
	CREATE_FUNCTION_BINDING("END_GAME_SPEED", 0, 0, EndGameSpeed);
	CREATE_FUNCTION_BINDING("BUILD_BUILDING", 4, 0, BuildBuilding);
	CREATE_FUNCTION_BINDING("SET_AFFECTED_BY_WIND", 2, 0, SetAffectedByWind);
	CREATE_FUNCTION_BINDING("WIDESCREEN_TRANSISTION_FINISHED", 0, 1, WidescreenTransistionFinished);
	CREATE_FUNCTION_BINDING("GET_RESOURCE", 2, 1, GetResource);
	CREATE_FUNCTION_BINDING("ADD_RESOURCE", 3, 1, AddResource);
	CREATE_FUNCTION_BINDING("REMOVE_RESOURCE", 3, 1, RemoveResource);
	CREATE_FUNCTION_BINDING("GET_TARGET_RELATIVE_POS", 8, 3, GetTargetRelativePos);
	CREATE_FUNCTION_BINDING("STOP_POINTING", 1, 0, StopPointing);
	CREATE_FUNCTION_BINDING("STOP_LOOKING", 1, 0, StopLooking);
	CREATE_FUNCTION_BINDING("LOOK_AT_POSITION", 4, 0, LookAtPosition);
	CREATE_FUNCTION_BINDING("PLAY_SPIRIT_ANIM", 5, 0, PlaySpiritAnim);
	CREATE_FUNCTION_BINDING("CALL_IN_NOT_NEAR", 8, 1, CallInNotNear);
	CREATE_FUNCTION_BINDING("SET_CAMERA_ZONE", 1, 0, SetCameraZone);
	CREATE_FUNCTION_BINDING("GET_OBJECT_STATE", 1, 1, GetObjectState);
	CREATE_FUNCTION_BINDING("REVEAL_COUNTDOWN_TIMER", 0, 0, RevealCountdownTimer);
	CREATE_FUNCTION_BINDING("SET_TIMER_TIME", 2, 0, SetTimerTime);
	CREATE_FUNCTION_BINDING("CREATE_TIMER", 1, 1, CreateTimer);
	CREATE_FUNCTION_BINDING("GET_TIMER_TIME_REMAINING", 1, 1, GetTimerTimeRemaining);
	CREATE_FUNCTION_BINDING("GET_TIMER_TIME_SINCE_SET", 1, 1, GetTimerTimeSinceSet);
	CREATE_FUNCTION_BINDING("MOVE_MUSIC", 2, 0, MoveMusic);
	CREATE_FUNCTION_BINDING("GET_INCLUSION_DISTANCE", 0, 1, GetInclusionDistance);
	CREATE_FUNCTION_BINDING("GET_LAND_HEIGHT", 3, 1, GetLandHeight);
	CREATE_FUNCTION_BINDING("LOAD_MAP", 1, 0, LoadMap);
	CREATE_FUNCTION_BINDING("STOP_ALL_SCRIPTS_EXCLUDING", 1, 0, StopAllScriptsExcluding);
	CREATE_FUNCTION_BINDING("STOP_ALL_SCRIPTS_IN_FILES_EXCLUDING", 1, 0, StopAllScriptsInFilesExcluding);
	CREATE_FUNCTION_BINDING("STOP_SCRIPT", 1, 0, StopScript);
	CREATE_FUNCTION_BINDING("CLEAR_CLICKED_OBJECT", 0, 0, ClearClickedObject);
	CREATE_FUNCTION_BINDING("CLEAR_CLICKED_POSITION", 0, 0, ClearClickedPosition);
	CREATE_FUNCTION_BINDING("POSITION_CLICKED", 4, 1, PositionClicked);
	CREATE_FUNCTION_BINDING("RELEASE_FROM_SCRIPT", 1, 0, ReleaseFromScript);
	CREATE_FUNCTION_BINDING("GET_OBJECT_HAND_IS_OVER", 0, 1, GetObjectHandIsOver);
	CREATE_FUNCTION_BINDING("ID_POISONED_SIZE", 1, 1, IdPoisonedSize);
	CREATE_FUNCTION_BINDING("IS_POISONED", 1, 1, IsPoisoned);
	CREATE_FUNCTION_BINDING("CALL_POISONED_IN", 4, 1, CallPoisonedIn);
	CREATE_FUNCTION_BINDING("CALL_NOT_POISONED_IN", 4, 1, CallNotPoisonedIn);
	CREATE_FUNCTION_BINDING("SPIRIT_PLAYED", 1, 1, SpiritPlayed);
	CREATE_FUNCTION_BINDING("CLING_SPIRIT", 3, 0, ClingSpirit);
	CREATE_FUNCTION_BINDING("FLY_SPIRIT", 3, 0, FlySpirit);
	CREATE_FUNCTION_BINDING("SET_ID_MOVEABLE", 2, 0, SetIdMoveable);
	CREATE_FUNCTION_BINDING("SET_ID_PICKUPABLE", 2, 0, SetIdPickupable);
	CREATE_FUNCTION_BINDING("IS_ON_FIRE", 1, 1, IsOnFire);
	CREATE_FUNCTION_BINDING("IS_FIRE_NEAR", 4, 1, IsFireNear);
	CREATE_FUNCTION_BINDING("STOP_SCRIPTS_IN_FILES", 1, 0, StopScriptsInFiles);
	CREATE_FUNCTION_BINDING("SET_POISONED", 2, 0, SetPoisoned);
	CREATE_FUNCTION_BINDING("SET_TEMPERATURE", 2, 0, SetTemperature);
	CREATE_FUNCTION_BINDING("SET_ON_FIRE", 3, 0, SetOnFire);
	CREATE_FUNCTION_BINDING("SET_TARGET", 5, 0, SetTarget);
	CREATE_FUNCTION_BINDING("WALK_PATH", 5, 0, WalkPath);
	CREATE_FUNCTION_BINDING("FOCUS_AND_POSITION_FOLLOW", 2, 0, FocusAndPositionFollow);
	CREATE_FUNCTION_BINDING("GET_WALK_PATH_PERCENTAGE", 1, 1, GetWalkPathPercentage);
	CREATE_FUNCTION_BINDING("CAMERA_PROPERTIES", 4, 0, CameraProperties);
	CREATE_FUNCTION_BINDING("ENABLE_DISABLE_MUSIC", 2, 0, EnableDisableMusic);
	CREATE_FUNCTION_BINDING("GET_MUSIC_OBJ_DISTANCE", 1, 1, GetMusicObjDistance);
	CREATE_FUNCTION_BINDING("GET_MUSIC_ENUM_DISTANCE", 1, 1, GetMusicEnumDistance);
	CREATE_FUNCTION_BINDING("SET_MUSIC_PLAY_POSITION", 4, 0, SetMusicPlayPosition);
	CREATE_FUNCTION_BINDING("ATTACH_OBJECT_LEASH_TO_OBJECT", 2, 0, AttachObjectLeashToObject);
	CREATE_FUNCTION_BINDING("ATTACH_OBJECT_LEASH_TO_HAND", 1, 0, AttachObjectLeashToHand);
	CREATE_FUNCTION_BINDING("DETACH_OBJECT_LEASH", 1, 0, DetachObjectLeash);
	CREATE_FUNCTION_BINDING("SET_CREATURE_ONLY_DESIRE", 3, 0, SetCreatureOnlyDesire);
	CREATE_FUNCTION_BINDING("SET_CREATURE_ONLY_DESIRE_OFF", 1, 0, SetCreatureOnlyDesireOff);
	CREATE_FUNCTION_BINDING("RESTART_MUSIC", 1, 0, RestartMusic);
	CREATE_FUNCTION_BINDING("MUSIC_PLAYED", 1, 1, MusicPlayed191);
	CREATE_FUNCTION_BINDING("IS_OF_TYPE", 3, 1, IsOfType);
	CREATE_FUNCTION_BINDING("CLEAR_HIT_OBJECT", 0, 0, ClearHitObject);
	CREATE_FUNCTION_BINDING("GAME_THING_HIT", 1, 1, GameThingHit);
	CREATE_FUNCTION_BINDING("SPELL_AT_THING", 8, 1, SpellAtThing);
	CREATE_FUNCTION_BINDING("SPELL_AT_POS", 10, 1, SpellAtPos);
	CREATE_FUNCTION_BINDING("CALL_PLAYER_CREATURE", 1, 1, CallPlayerCreature);
	CREATE_FUNCTION_BINDING("GET_SLOWEST_SPEED", 1, 1, GetSlowestSpeed);
	CREATE_FUNCTION_BINDING("GET_OBJECT_HELD", 0, 1, GetObjectHeld199);
}

void CHLApi::InitFunctionsTable2()
{
	CREATE_FUNCTION_BINDING("HELP_SYSTEM_ON", 0, 1, HelpSystemOn);
	CREATE_FUNCTION_BINDING("SHAKE_CAMERA", 6, 0, ShakeCamera);
	CREATE_FUNCTION_BINDING("SET_ANIMATION_MODIFY", 2, 0, SetAnimationModify);
	CREATE_FUNCTION_BINDING("SET_AVI_SEQUENCE", 2, 0, SetAviSequence);
	CREATE_FUNCTION_BINDING("PLAY_GESTURE", 5, 0, PlayGesture);
	CREATE_FUNCTION_BINDING("DEV_FUNCTION", 1, 0, DevFunction);
	CREATE_FUNCTION_BINDING("HAS_MOUSE_WHEEL", 0, 1, HasMouseWheel);
	CREATE_FUNCTION_BINDING("NUM_MOUSE_BUTTONS", 0, 1, NumMouseButtons);
	CREATE_FUNCTION_BINDING("SET_CREATURE_DEV_STAGE", 2, 0, SetCreatureDevStage);
	CREATE_FUNCTION_BINDING("SET_FIXED_CAM_ROTATION", 4, 0, SetFixedCamRotation);
	CREATE_FUNCTION_BINDING("SWAP_CREATURE", 2, 0, SwapCreature);
	CREATE_FUNCTION_BINDING("GET_ARENA", 5, 1, GetArena);
	CREATE_FUNCTION_BINDING("GET_FOOTBALL_PITCH", 1, 1, GetFootballPitch);
	CREATE_FUNCTION_BINDING("STOP_ALL_GAMES", 1, 0, StopAllGames);
	CREATE_FUNCTION_BINDING("ATTACH_TO_GAME", 3, 0, AttachToGame);
	CREATE_FUNCTION_BINDING("DETACH_FROM_GAME", 3, 0, DetachFromGame);
	CREATE_FUNCTION_BINDING("DETACH_UNDEFINED_FROM_GAME", 2, 0, DetachUndefinedFromGame);
	CREATE_FUNCTION_BINDING("SET_ONLY_FOR_SCRIPTS", 2, 0, SetOnlyForScripts);
	CREATE_FUNCTION_BINDING("START_MATCH_WITH_REFEREE", 2, 0, StartMatchWithReferee);
	CREATE_FUNCTION_BINDING("GAME_TEAM_SIZE", 2, 0, GameTeamSize);
	CREATE_FUNCTION_BINDING("GAME_TYPE", 1, 1, GameType);
	CREATE_FUNCTION_BINDING("GAME_SUB_TYPE", 1, 1, GameSubType);
	CREATE_FUNCTION_BINDING("IS_LEASHED", 1, 1, IsLeashed);
	CREATE_FUNCTION_BINDING("SET_CREATURE_HOME", 4, 0, SetCreatureHome);
	CREATE_FUNCTION_BINDING("GET_HIT_OBJECT", 0, 1, GetHitObject);
	CREATE_FUNCTION_BINDING("GET_OBJECT_WHICH_HIT", 0, 1, GetObjectWhichHit);
	CREATE_FUNCTION_BINDING("GET_NEAREST_TOWN_OF_PLAYER", 5, 1, GetNearestTownOfPlayer);
	CREATE_FUNCTION_BINDING("SPELL_AT_POINT", 5, 1, SpellAtPoint);
	CREATE_FUNCTION_BINDING("SET_ATTACK_OWN_TOWN", 2, 0, SetAttackOwnTown);
	CREATE_FUNCTION_BINDING("IS_FIGHTING", 1, 1, IsFighting);
	CREATE_FUNCTION_BINDING("SET_MAGIC_RADIUS", 2, 0, SetMagicRadius);
	CREATE_FUNCTION_BINDING("TEMP_TEXT_WITH_NUMBER", 4, 0, TempTextWithNumber);
	CREATE_FUNCTION_BINDING("RUN_TEXT_WITH_NUMBER", 4, 0, RunTextWithNumber);
	CREATE_FUNCTION_BINDING("CREATURE_SPELL_REVERSION", 2, 0, CreatureSpellReversion);
	CREATE_FUNCTION_BINDING("GET_DESIRE", 2, 1, GetDesire);
	CREATE_FUNCTION_BINDING("GET_EVENTS_PER_SECOND", 1, 1, GetEventsPerSecond);
	CREATE_FUNCTION_BINDING("GET_TIME_SINCE", 1, 1, GetTimeSince);
	CREATE_FUNCTION_BINDING("GET_TOTAL_EVENTS", 1, 1, GetTotalEvents);
	CREATE_FUNCTION_BINDING("UPDATE_SNAPSHOT", -1, 0, UpdateSnapshot);
	CREATE_FUNCTION_BINDING("CREATE_REWARD", 5, 1, CreateReward);
	CREATE_FUNCTION_BINDING("CREATE_REWARD_IN_TOWN", 6, 1, CreateRewardInTown);
	CREATE_FUNCTION_BINDING("SET_FADE", 4, 0, SetFade);
	CREATE_FUNCTION_BINDING("SET_FADE_IN", 1, 0, SetFadeIn);
	CREATE_FUNCTION_BINDING("FADE_FINISHED", 0, 1, FadeFinished);
	CREATE_FUNCTION_BINDING("SET_PLAYER_MAGIC", 3, 0, SetPlayerMagic);
	CREATE_FUNCTION_BINDING("HAS_PLAYER_MAGIC", 2, 1, HasPlayerMagic);
	CREATE_FUNCTION_BINDING("SPIRIT_SPEAKS", 2, 1, SpiritSpeaks);
	CREATE_FUNCTION_BINDING("BELIEF_FOR_PLAYER", 2, 1, BeliefForPlayer);
	CREATE_FUNCTION_BINDING("GET_HELP", 1, 1, GetHelp);
	CREATE_FUNCTION_BINDING("SET_LEASH_WORKS", 2, 0, SetLeashWorks);
	CREATE_FUNCTION_BINDING("LOAD_MY_CREATURE", 3, 0, LoadMyCreature);
	CREATE_FUNCTION_BINDING("OBJECT_RELATIVE_BELIEF", 3, 0, ObjectRelativeBelief);
	CREATE_FUNCTION_BINDING("CREATE_WITH_ANGLE_AND_SCALE", 7, 1, CreateWithAngleAndScale);
	CREATE_FUNCTION_BINDING("SET_HELP_SYSTEM", 1, 0, SetHelpSystem);
	CREATE_FUNCTION_BINDING("SET_VIRTUAL_INFLUENCE", 2, 0, SetVirtualInfluence);
	CREATE_FUNCTION_BINDING("SET_ACTIVE", 2, 0, SetActive);
	CREATE_FUNCTION_BINDING("THING_VALID", 1, 1, ThingValid);
	CREATE_FUNCTION_BINDING("VORTEX_FADE_OUT", 1, 0, VortexFadeOut);
	CREATE_FUNCTION_BINDING("REMOVE_REACTION_OF_TYPE", 2, 0, RemoveReactionOfType);
	CREATE_FUNCTION_BINDING("CREATURE_LEARN_EVERYTHING_EXCLUDING", 2, 0, CreatureLearnEverythingExcluding);
	CREATE_FUNCTION_BINDING("PLAYED_PERCENTAGE", 1, 1, PlayedPercentage);
	CREATE_FUNCTION_BINDING("OBJECT_CAST_BY_OBJECT", 2, 1, ObjectCastByObject);
	CREATE_FUNCTION_BINDING("IS_WIND_MAGIC_AT_POS", 1, 1, IsWindMagicAtPos);
	CREATE_FUNCTION_BINDING("CREATE_MIST", 9, 1, CreateMist);
	CREATE_FUNCTION_BINDING("SET_MIST_FADE", 6, 0, SetMistFade);
	CREATE_FUNCTION_BINDING("GET_OBJECT_FADE", 1, 1, GetObjectFade);
	CREATE_FUNCTION_BINDING("PLAY_HAND_DEMO", 3, 0, PlayHandDemo);
	CREATE_FUNCTION_BINDING("IS_PLAYING_HAND_DEMO", 0, 1, IsPlayingHandDemo);
	CREATE_FUNCTION_BINDING("GET_ARSE_POSITION", 1, 3, GetArsePosition);
	CREATE_FUNCTION_BINDING("IS_LEASHED_TO_OBJECT", 2, 1, IsLeashedToObject);
	CREATE_FUNCTION_BINDING("GET_INTERACTION_MAGNITUDE", 1, 1, GetInteractionMagnitude);
	CREATE_FUNCTION_BINDING("IS_CREATURE_AVAILABLE", 1, 1, IsCreatureAvailable);
	CREATE_FUNCTION_BINDING("CREATE_HIGHLIGHT", 5, 1, CreateHighlight);
	CREATE_FUNCTION_BINDING("GET_OBJECT_HELD", 1, 1, GetObjectHeld273);
	CREATE_FUNCTION_BINDING("GET_ACTION_COUNT", 2, 1, GetActionCount);
	CREATE_FUNCTION_BINDING("GET_OBJECT_LEASH_TYPE", 1, 1, GetObjectLeashType);
	CREATE_FUNCTION_BINDING("SET_FOCUS_FOLLOW", 1, 0, SetFocusFollow);
	CREATE_FUNCTION_BINDING("SET_POSITION_FOLLOW", 1, 0, SetPositionFollow);
	CREATE_FUNCTION_BINDING("SET_FOCUS_AND_POSITION_FOLLOW", 2, 0, SetFocusAndPositionFollow);
	CREATE_FUNCTION_BINDING("SET_CAMERA_LENS", 1, 0, SetCameraLens);
	CREATE_FUNCTION_BINDING("MOVE_CAMERA_LENS", 2, 0, MoveCameraLens);
	CREATE_FUNCTION_BINDING("CREATURE_REACTION", 2, 0, CreatureReaction);
	CREATE_FUNCTION_BINDING("CREATURE_IN_DEV_SCRIPT", 2, 0, CreatureInDevScript);
	CREATE_FUNCTION_BINDING("STORE_CAMERA_DETAILS", 0, 0, StoreCameraDetails);
	CREATE_FUNCTION_BINDING("RESTORE_CAMERA_DETAILS", 0, 0, RestoreCameraDetails);
	CREATE_FUNCTION_BINDING("START_ANGLE_SOUND", 1, 0, StartAngleSound285);
	CREATE_FUNCTION_BINDING("SET_CAMERA_POS_FOC_LENS", 7, 0, SetCameraPosFocLens);
	CREATE_FUNCTION_BINDING("MOVE_CAMERA_POS_FOC_LENS", 8, 0, MoveCameraPosFocLens);
	CREATE_FUNCTION_BINDING("GAME_TIME_ON_OFF", 1, 0, GameTimeOnOff);
	CREATE_FUNCTION_BINDING("MOVE_GAME_TIME", 2, 0, MoveGameTime);
	CREATE_FUNCTION_BINDING("SET_HIGH_GRAPHICS_DETAIL", 2, 0, SetHighGraphicsDetail);
	CREATE_FUNCTION_BINDING("SET_SKELETON", 2, 0, SetSkeleton);
	CREATE_FUNCTION_BINDING("IS_SKELETON", 1, 1, IsSkeleton);
	CREATE_FUNCTION_BINDING("PLAYER_SPELL_CAST_TIME", 1, 1, PlayerSpellCastTime);
	CREATE_FUNCTION_BINDING("PLAYER_SPELL_LAST_CAST", 1, 1, PlayerSpellLastCast);
	CREATE_FUNCTION_BINDING("GET_LAST_SPELL_CAST_POS", 1, 3, GetLastSpellCastPos);
	CREATE_FUNCTION_BINDING("ADD_SPOT_VISUAL_TARGET_POS", 4, 0, AddSpotVisualTargetPos);
	CREATE_FUNCTION_BINDING("ADD_SPOT_VISUAL_TARGET_OBJECT", 2, 0, AddSpotVisualTargetObject);
	CREATE_FUNCTION_BINDING("SET_INDESTRUCTABLE", 2, 0, SetIndestructable);
	CREATE_FUNCTION_BINDING("SET_GRAPHICS_CLIPPING", 2, 0, SetGraphicsClipping);
}

void CHLApi::InitFunctionsTable3()
{
	CREATE_FUNCTION_BINDING("SPIRIT_APPEAR", 1, 0, SpiritAppear);
	CREATE_FUNCTION_BINDING("SPIRIT_DISAPPEAR", 1, 0, SpiritDisappear);
	CREATE_FUNCTION_BINDING("SET_FOCUS_ON_OBJECT", 2, 0, SetFocusOnObject);
	CREATE_FUNCTION_BINDING("RELEASE_OBJECT_FOCUS", 1, 0, ReleaseObjectFocus);
	CREATE_FUNCTION_BINDING("IMMERSION_EXISTS", 0, 1, ImmersionExists);
	CREATE_FUNCTION_BINDING("SET_DRAW_LEASH", 1, 0, SetDrawLeash);
	CREATE_FUNCTION_BINDING("SET_DRAW_HIGHLIGHT", 1, 0, SetDrawHighlight);
	CREATE_FUNCTION_BINDING("SET_OPEN_CLOSE", 2, 0, SetOpenClose);
	CREATE_FUNCTION_BINDING("SET_INTRO_BUILDING", 1, 0, SetIntroBuilding);
	CREATE_FUNCTION_BINDING("CREATURE_FORCE_FRIENDS", 3, 0, CreatureForceFriends);
	CREATE_FUNCTION_BINDING("MOVE_COMPUTER_PLAYER_POSITION", 6, 0, MoveComputerPlayerPosition);
	CREATE_FUNCTION_BINDING("ENABLE_DISABLE_COMPUTER_PLAYER", 2, 0, EnableDisableComputerPlayer311);
	CREATE_FUNCTION_BINDING("GET_COMPUTER_PLAYER_POSITION", 1, 3, GetComputerPlayerPosition);
	CREATE_FUNCTION_BINDING("SET_COMPUTER_PLAYER_POSITION", 5, 0, SetComputerPlayerPosition);
	CREATE_FUNCTION_BINDING("GET_STORED_CAMERA_POSITION", 0, 3, GetStoredCameraPosition);
	CREATE_FUNCTION_BINDING("GET_STORED_CAMERA_FOCUS", 0, 3, GetStoredCameraFocus);
	CREATE_FUNCTION_BINDING("CALL_NEAR_IN_STATE", 8, 1, CallNearInState);
	CREATE_FUNCTION_BINDING("SET_CREATURE_SOUND", 1, 0, SetCreatureSound);
	CREATE_FUNCTION_BINDING("CREATURE_INTERACTING_WITH", 2, 1, CreatureInteractingWith);
	CREATE_FUNCTION_BINDING("SET_SUN_DRAW", 1, 0, SetSunDraw);
	CREATE_FUNCTION_BINDING("OBJECT_INFO_BITS", 1, 1, ObjectInfoBits);
	CREATE_FUNCTION_BINDING("SET_HURT_BY_FIRE", 2, 0, SetHurtByFire);
	CREATE_FUNCTION_BINDING("CONFINED_OBJECT", 5, 0, ConfinedObject);
	CREATE_FUNCTION_BINDING("CLEAR_CONFINED_OBJECT", 1, 0, ClearConfinedObject);
	CREATE_FUNCTION_BINDING("GET_OBJECT_FLOCK", 1, 1, GetObjectFlock);
	CREATE_FUNCTION_BINDING("SET_PLAYER_BELIEF", 3, 0, SetPlayerBelief);
	CREATE_FUNCTION_BINDING("PLAY_JC_SPECIAL", 1, 0, PlayJcSpecial);
	CREATE_FUNCTION_BINDING("IS_PLAYING_JC_SPECIAL", 1, 1, IsPlayingJcSpecial);
	CREATE_FUNCTION_BINDING("VORTEX_PARAMETERS", 8, 0, VortexParameters);
	CREATE_FUNCTION_BINDING("LOAD_CREATURE", 6, 0, LoadCreature);
	CREATE_FUNCTION_BINDING("IS_SPELL_CHARGING", 1, 1, IsSpellCharging);
	CREATE_FUNCTION_BINDING("IS_THAT_SPELL_CHARGING", 2, 1, IsThatSpellCharging);
	CREATE_FUNCTION_BINDING("OPPOSING_CREATURE", 1, 1, OpposingCreature);
	CREATE_FUNCTION_BINDING("FLOCK_WITHIN_LIMITS", 1, 1, FlockWithinLimits);
	CREATE_FUNCTION_BINDING("HIGHLIGHT_PROPERTIES", 3, 0, HighlightProperties);
	CREATE_FUNCTION_BINDING("LAST_MUSIC_LINE", 1, 1, LastMusicLine);
	CREATE_FUNCTION_BINDING("HAND_DEMO_TRIGGER", 0, 1, HandDemoTrigger);
	CREATE_FUNCTION_BINDING("GET_BELLY_POSITION", 1, 3, GetBellyPosition);
	CREATE_FUNCTION_BINDING("SET_CREATURE_CREED_PROPERTIES", 5, 0, SetCreatureCreedProperties);
	CREATE_FUNCTION_BINDING("GAME_THING_CAN_VIEW_CAMERA", 2, 1, GameThingCanViewCamera);
	CREATE_FUNCTION_BINDING("GAME_PLAY_SAY_SOUND_EFFECT", 6, 0, GamePlaySaySoundEffect);
	CREATE_FUNCTION_BINDING("SET_TOWN_DESIRE_BOOST", 3, 0, SetTownDesireBoost);
	CREATE_FUNCTION_BINDING("IS_LOCKED_INTERACTION", 1, 1, IsLockedInteraction);
	CREATE_FUNCTION_BINDING("SET_CREATURE_NAME", 2, 0, SetCreatureName);
	CREATE_FUNCTION_BINDING("COMPUTER_PLAYER_READY", 1, 1, ComputerPlayerReady);
	CREATE_FUNCTION_BINDING("ENABLE_DISABLE_COMPUTER_PLAYER", 2, 0, EnableDisableComputerPlayer345);
	CREATE_FUNCTION_BINDING("CLEAR_ACTOR_MIND", 1, 0, ClearActorMind);
	CREATE_FUNCTION_BINDING("ENTER_EXIT_CITADEL", 1, 0, EnterExitCitadel);
	CREATE_FUNCTION_BINDING("START_ANGLE_SOUND", 1, 0, StartAngleSound348);
	CREATE_FUNCTION_BINDING("THING_JC_SPECIAL", 3, 0, ThingJcSpecial);
	CREATE_FUNCTION_BINDING("MUSIC_PLAYED", 1, 1, MusicPlayed350);
	CREATE_FUNCTION_BINDING("UPDATE_SNAPSHOT_PICTURE", 11, 0, UpdateSnapshotPicture);
	CREATE_FUNCTION_BINDING("STOP_SCRIPTS_IN_FILES_EXCLUDING", 2, 0, StopScriptsInFilesExcluding);
	CREATE_FUNCTION_BINDING("CREATE_RANDOM_VILLAGER_OF_TRIBE", 4, 1, CreateRandomVillagerOfTribe);
	CREATE_FUNCTION_BINDING("TOGGLE_LEASH", 1, 0, ToggleLeash);
	CREATE_FUNCTION_BINDING("GAME_SET_MANA", 2, 0, GameSetMana);
	CREATE_FUNCTION_BINDING("SET_MAGIC_PROPERTIES", 3, 0, SetMagicProperties);
	CREATE_FUNCTION_BINDING("SET_GAME_SOUND", 1, 0, SetGameSound);
	CREATE_FUNCTION_BINDING("SEX_IS_MALE", 1, 1, SexIsMale);
	CREATE_FUNCTION_BINDING("GET_FIRST_HELP", 1, 1, GetFirstHelp);
	CREATE_FUNCTION_BINDING("GET_LAST_HELP", 1, 1, GetLastHelp);
	CREATE_FUNCTION_BINDING("IS_ACTIVE", 1, 1, IsActive);
	CREATE_FUNCTION_BINDING("SET_BOOKMARK_POSITION", 4, 0, SetBookmarkPosition);
	CREATE_FUNCTION_BINDING("SET_SCAFFOLD_PROPERTIES", 4, 0, SetScaffoldProperties);
	CREATE_FUNCTION_BINDING("SET_COMPUTER_PLAYER_PERSONALITY", 3, 0, SetComputerPlayerPersonality);
	CREATE_FUNCTION_BINDING("SET_COMPUTER_PLAYER_SUPPRESSION", 3, 0, SetComputerPlayerSuppression);
	CREATE_FUNCTION_BINDING("FORCE_COMPUTER_PLAYER_ACTION", 4, 0, ForceComputerPlayerAction);
	CREATE_FUNCTION_BINDING("QUEUE_COMPUTER_PLAYER_ACTION", 4, 0, QueueComputerPlayerAction);
	CREATE_FUNCTION_BINDING("GET_TOWN_WITH_ID", 1, 1, GetTownWithId);
	CREATE_FUNCTION_BINDING("SET_DISCIPLE", 3, 0, SetDisciple);
	CREATE_FUNCTION_BINDING("RELEASE_COMPUTER_PLAYER", 1, 0, ReleaseComputerPlayer);
	CREATE_FUNCTION_BINDING("SET_COMPUTER_PLAYER_SPEED", 2, 0, SetComputerPlayerSpeed);
	CREATE_FUNCTION_BINDING("SET_FOCUS_FOLLOW_COMPUTER_PLAYER", 1, 0, SetFocusFollowComputerPlayer);
	CREATE_FUNCTION_BINDING("SET_POSITION_FOLLOW_COMPUTER_PLAYER", 1, 0, SetPositionFollowComputerPlayer);
	CREATE_FUNCTION_BINDING("CALL_COMPUTER_PLAYER", 1, 1, CallComputerPlayer);
	CREATE_FUNCTION_BINDING("CALL_BUILDING_IN_TOWN", 4, 1, CallBuildingInTown);
	CREATE_FUNCTION_BINDING("SET_CAN_BUILD_WORSHIPSITE", 2, 0, SetCanBuildWorshipsite);
	CREATE_FUNCTION_BINDING("GET_FACING_CAMERA_POSITION", 1, 3, GetFacingCameraPosition);
	CREATE_FUNCTION_BINDING("SET_COMPUTER_PLAYER_ATTITUDE", 3, 0, SetComputerPlayerAttitude);
	CREATE_FUNCTION_BINDING("GET_COMPUTER_PLAYER_ATTITUDE", 2, 1, GetComputerPlayerAttitude);
	CREATE_FUNCTION_BINDING("LOAD_COMPUTER_PLAYER_PERSONALITY", 2, 0, LoadComputerPlayerPersonality);
	CREATE_FUNCTION_BINDING("SAVE_COMPUTER_PLAYER_PERSONALITY", 2, 0, SaveComputerPlayerPersonality);
	CREATE_FUNCTION_BINDING("SET_PLAYER_ALLY", 3, 0, SetPlayerAlly);
	CREATE_FUNCTION_BINDING("CALL_FLYING", 7, 1, CallFlying);
	CREATE_FUNCTION_BINDING("SET_OBJECT_FADE_IN", 2, 0, SetObjectFadeIn);
	CREATE_FUNCTION_BINDING("IS_AFFECTED_BY_SPELL", 2, 1, IsAffectedBySpell);
	CREATE_FUNCTION_BINDING("SET_MAGIC_IN_OBJECT", 3, 0, SetMagicInObject);
	CREATE_FUNCTION_BINDING("ID_ADULT_SIZE", 1, 1, IdAdultSize);
	CREATE_FUNCTION_BINDING("OBJECT_CAPACITY", 1, 1, ObjectCapacity);
	CREATE_FUNCTION_BINDING("OBJECT_ADULT_CAPACITY", 1, 1, ObjectAdultCapacity);
	CREATE_FUNCTION_BINDING("SET_CREATURE_AUTO_FIGHTING", 2, 0, SetCreatureAutoFighting);
	CREATE_FUNCTION_BINDING("IS_AUTO_FIGHTING", 1, 1, IsAutoFighting);
	CREATE_FUNCTION_BINDING("SET_CREATURE_QUEUE_FIGHT_MOVE", 2, 0, SetCreatureQueueFightMove);
	CREATE_FUNCTION_BINDING("SET_CREATURE_QUEUE_FIGHT_SPELL", 2, 0, SetCreatureQueueFightSpell);
	CREATE_FUNCTION_BINDING("SET_CREATURE_QUEUE_FIGHT_STEP", 2, 0, SetCreatureQueueFightStep);
	CREATE_FUNCTION_BINDING("GET_CREATURE_FIGHT_ACTION", 1, 1, GetCreatureFightAction);
	CREATE_FUNCTION_BINDING("CREATURE_FIGHT_QUEUE_HITS", 1, 1, CreatureFightQueueHits);
	CREATE_FUNCTION_BINDING("SQUARE_ROOT", 1, 1, SquareRoot);
	CREATE_FUNCTION_BINDING("GET_PLAYER_ALLY", 2, 1, GetPlayerAlly);
	CREATE_FUNCTION_BINDING("SET_PLAYER_WIND_RESISTANCE", 2, 1, SetPlayerWindResistance);
}

void CHLApi::InitFunctionsTable4()
{
	CREATE_FUNCTION_BINDING("GET_PLAYER_WIND_RESISTANCE", 2, 1, GetPlayerWindResistance);
	CREATE_FUNCTION_BINDING("PAUSE_UNPAUSE_CLIMATE_SYSTEM", 1, 0, PauseUnpauseClimateSystem);
	CREATE_FUNCTION_BINDING("PAUSE_UNPAUSE_STORM_CREATION_IN_CLIMATE_SYSTEM", 1, 0, PauseUnpauseStormCreationInClimateSystem);
	CREATE_FUNCTION_BINDING("GET_MANA_FOR_SPELL", 1, 1, GetManaForSpell);
	CREATE_FUNCTION_BINDING("KILL_STORMS_IN_AREA", 4, 0, KillStormsInArea);
	CREATE_FUNCTION_BINDING("INSIDE_TEMPLE", 0, 1, InsideTemple);
	CREATE_FUNCTION_BINDING("RESTART_OBJECT", 1, 0, RestartObject);
	CREATE_FUNCTION_BINDING("SET_GAME_TIME_PROPERTIES", 3, 0, SetGameTimeProperties);
	CREATE_FUNCTION_BINDING("RESET_GAME_TIME_PROPERTIES", 0, 0, ResetGameTimeProperties);
	CREATE_FUNCTION_BINDING("SOUND_EXISTS", 0, 1, SoundExists);
	CREATE_FUNCTION_BINDING("GET_TOWN_WORSHIP_DEATHS", 1, 1, GetTownWorshipDeaths);
	CREATE_FUNCTION_BINDING("GAME_CLEAR_DIALOGUE", 0, 0, GameClearDialogue);
	CREATE_FUNCTION_BINDING("GAME_CLOSE_DIALOGUE", 0, 0, GameCloseDialogue);
	CREATE_FUNCTION_BINDING("GET_HAND_STATE", 0, 1, GetHandState);
	CREATE_FUNCTION_BINDING("SET_INTERFACE_CITADEL", 1, 0, SetInterfaceCitadel);
	CREATE_FUNCTION_BINDING("MAP_SCRIPT_FUNCTION", 1, 0, MapScriptFunction);
	CREATE_FUNCTION_BINDING("WITHIN_ROTATION", 0, 1, WithinRotation);
	CREATE_FUNCTION_BINDING("GET_PLAYER_TOWN_TOTAL", 1, 1, GetPlayerTownTotal);
	CREATE_FUNCTION_BINDING("SPIRIT_SCREEN_POINT", 3, 0, SpiritScreenPoint);
	CREATE_FUNCTION_BINDING("KEY_DOWN", 1, 1, KeyDown);
	CREATE_FUNCTION_BINDING("SET_FIGHT_EXIT", 1, 0, SetFightExit);
	CREATE_FUNCTION_BINDING("GET_OBJECT_CLICKED", 0, 1, GetObjectClicked);
	CREATE_FUNCTION_BINDING("GET_MANA", 1, 1, GetMana);
	CREATE_FUNCTION_BINDING("CLEAR_PLAYER_SPELL_CHARGING", 1, 0, ClearPlayerSpellCharging);
	CREATE_FUNCTION_BINDING("STOP_SOUND_EFFECT", 3, 0, StopSoundEffect);
	CREATE_FUNCTION_BINDING("GET_TOTEM_STATUE", 1, 1, GetTotemStatue);
	CREATE_FUNCTION_BINDING("SET_SET_ON_FIRE", 2, 0, SetSetOnFire);
	CREATE_FUNCTION_BINDING("SET_LAND_BALANCE", 2, 0, SetLandBalance);
	CREATE_FUNCTION_BINDING("SET_OBJECT_BELIEF_SCALE", 2, 0, SetObjectBeliefScale);
	CREATE_FUNCTION_BINDING("START_IMMERSION", 1, 0, StartImmersion);
	CREATE_FUNCTION_BINDING("STOP_IMMERSION", 1, 0, StopImmersion);
	CREATE_FUNCTION_BINDING("STOP_ALL_IMMERSION", 0, 0, StopAllImmersion);
	CREATE_FUNCTION_BINDING("SET_CREATURE_IN_TEMPLE", 1, 0, SetCreatureInTemple);
	CREATE_FUNCTION_BINDING("GAME_DRAW_TEXT", 7, 0, GameDrawText);
	CREATE_FUNCTION_BINDING("GAME_DRAW_TEMP_TEXT", 7, 0, GameDrawTempText);
	CREATE_FUNCTION_BINDING("FADE_ALL_DRAW_TEXT", 1, 0, FadeAllDrawText);
	CREATE_FUNCTION_BINDING("SET_DRAW_TEXT_COLOUR", 3, 0, SetDrawTextColour);
	CREATE_FUNCTION_BINDING("SET_CLIPPING_WINDOW", 5, 0, SetClippingWindow);
	CREATE_FUNCTION_BINDING("CLEAR_CLIPPING_WINDOW", 1, 0, ClearClippingWindow);
	CREATE_FUNCTION_BINDING("SAVE_GAME_IN_SLOT", 1, 0, SaveGameInSlot);
	CREATE_FUNCTION_BINDING("SET_OBJECT_CARRYING", 2, 0, SetObjectCarrying);
	CREATE_FUNCTION_BINDING("POS_VALID_FOR_CREATURE", 3, 1, PosValidForCreature);
	CREATE_FUNCTION_BINDING("GET_TIME_SINCE_OBJECT_ATTACKED", 2, 1, GetTimeSinceObjectAttacked);
	CREATE_FUNCTION_BINDING("GET_TOWN_AND_VILLAGER_HEALTH_TOTAL", 1, 1, GetTownAndVillagerHealthTotal);
	CREATE_FUNCTION_BINDING("GAME_ADD_FOR_BUILDING", 2, 0, GameAddForBuilding);
	CREATE_FUNCTION_BINDING("ENABLE_DISABLE_ALIGNMENT_MUSIC", 1, 0, EnableDisableAlignmentMusic);
	CREATE_FUNCTION_BINDING("GET_DEAD_LIVING", 4, 1, GetDeadLiving);
	CREATE_FUNCTION_BINDING("ATTACH_SOUND_TAG", 4, 0, AttachSoundTag);
	CREATE_FUNCTION_BINDING("DETACH_SOUND_TAG", 3, 0, DetachSoundTag);
	CREATE_FUNCTION_BINDING("GET_SACRIFICE_TOTAL", 1, 1, GetSacrificeTotal);
	CREATE_FUNCTION_BINDING("GAME_SOUND_PLAYING", 2, 1, GameSoundPlaying);
	CREATE_FUNCTION_BINDING("GET_TEMPLE_POSITION", 1, 3, GetTemplePosition);
	CREATE_FUNCTION_BINDING("CREATURE_AUTOSCALE", 3, 0, CreatureAutoscale);
	CREATE_FUNCTION_BINDING("GET_SPELL_ICON_IN_TEMPLE", 2, 1, GetSpellIconInTemple);
	CREATE_FUNCTION_BINDING("GAME_CLEAR_COMPUTER_PLAYER_ACTIONS", 1, 0, GameClearComputerPlayerActions);
	CREATE_FUNCTION_BINDING("GET_FIRST_IN_CONTAINER", 1, 1, GetFirstInContainer);
	CREATE_FUNCTION_BINDING("GET_NEXT_IN_CONTAINER", 2, 1, GetNextInContainer);
	CREATE_FUNCTION_BINDING("GET_TEMPLE_ENTRANCE_POSITION", 3, 3, GetTempleEntrancePosition);
	CREATE_FUNCTION_BINDING("SAY_SOUND_EFFECT_PLAYING", 2, 1, SaySoundEffectPlaying);
	CREATE_FUNCTION_BINDING("SET_HAND_DEMO_KEYS", 1, 0, SetHandDemoKeys);
	CREATE_FUNCTION_BINDING("CAN_SKIP_TUTORIAL", 0, 1, CanSkipTutorial);
	CREATE_FUNCTION_BINDING("CAN_SKIP_CREATURE_TRAINING", 0, 1, CanSkipCreatureTraining);
	CREATE_FUNCTION_BINDING("IS_KEEPING_OLD_CREATURE", 0, 1, IsKeepingOldCreature);
	CREATE_FUNCTION_BINDING("CURRENT_PROFILE_HAS_CREATURE", 0, 1, CurrentProfileHasCreature);
}

} // namespace openblack::chlapi
