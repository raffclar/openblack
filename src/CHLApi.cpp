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

void GetInclusionDistance() // 150 GET_INCLUSION_DISTANCE
{
	// The world camera's distance to the fence from its last fence test
	const auto view = ScriptView();
	Pushf(view.has_value() ? view->inclusionDistance : 0.0f);
}

void ReleaseFromScript() // 159 RELEASE_FROM_SCRIPT
{
	Locator::scriptObjects::value().ReleaseFromScript(PopObject());
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
