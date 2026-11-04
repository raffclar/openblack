/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TestbedScenarioRunner.h"

#include <cmath>
#include <ctime>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <MindFile.h>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <glm/trigonometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/FlatLand.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "3D/ScreenPick.h"
#include "3D/SkyDome.h"
#include "3D/TempleInteriorInterface.h"
#include "CHLApi.h"
#include "Camera/Camera.h"
#include "Camera/DoubleClickFlight.h"
#include "Commands/PlayerCommands.h"
#include "Commands/TurnCodec.h"
#include "Common/FileDialog.h"
#include "Common/PortableMath.h"
#include "Creature/CaveCreature.h"
#include "Creature/CreatureActivity.h"
#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLearning.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureScriptPlay.h"
#include "Debug/ComputerActions.h"
#include "Debug/FightAutopilotInterface.h"
#include "Debug/TestbedMultiplayer.h"
#include "Debug/TownMiracles.h"
#include "DebugGuiInterface.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Archetypes/CitadelArchetype.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Archetypes/DrinkWaypointArchetype.h"
#include "ECS/Archetypes/FieldArchetype.h"
#include "ECS/Archetypes/SharkArchetype.h"
#include "ECS/Archetypes/TownArchetype.h"
#include "ECS/Archetypes/VillagerArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureCopy.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureHandGlows.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/GameStanding.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/MiracleImpression.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Sky.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/Vortex.h"
#include "ECS/Components/Weather.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/ComputerPlayerStart.h"
#include "ECS/CreatureFromFile.h"
#include "ECS/Dances.h"
#include "ECS/Map.h"
#include "ECS/ObserverInterfaceRules.h"
#include "ECS/Registry.h"
#include "ECS/ScriptFind.h"
#include "ECS/ScriptFlocks.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/AnimatedStaticSystemInterface.h"
#include "ECS/Systems/BoatLaunchSystemInterface.h"
#include "ECS/Systems/CameraBookmarkSystemInterface.h"
#include "ECS/Systems/CameraHelpSystemInterface.h"
#include "ECS/Systems/CameraZoneSystemInterface.h"
#include "ECS/Systems/ChatSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/ComputerPlayerSystemInterface.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureCarryOverSystemInterface.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/FightTranscriptSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/FireflySystemInterface.h"
#include "ECS/Systems/FootballSystemInterface.h"
#include "ECS/Systems/FootpathSystemInterface.h"
#include "ECS/Systems/FootprintSystemInterface.h"
#include "ECS/Systems/GameEndSystemInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/GuidanceSystemInterface.h"
#include "ECS/Systems/HandDemoSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HelpMessageSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerCreatureReactions.h"
#include "ECS/Systems/Implementations/VillagerDisciples.h"
#include "ECS/Systems/IncomingMessageSystemInterface.h"
#include "ECS/Systems/InspectorLoading.h"
#include "ECS/Systems/InspectorSystemInterface.h"
#include "ECS/Systems/LandSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/MusicMoodSystemInterface.h"
#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/ProfileFolderSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/Systems/RewardSystemInterface.h"
#include "ECS/Systems/SaveGameSystemInterface.h"
#include "ECS/Systems/ScriptHighlightSystemInterface.h"
#include "ECS/Systems/ScriptMusicSystemInterface.h"
#include "ECS/Systems/SkyClockSystemInterface.h"
#include "ECS/Systems/TattooEditorSystemInterface.h"
#include "ECS/Systems/TattooMenuSystemInterface.h"
#include "ECS/Systems/TeleportSystemInterface.h"
#include "ECS/Systems/TempleDestructionSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/Systems/VideoSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/Systems/VortexSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "FileSystem/FileSystemInterface.h"
#include "Gui/GameTexts.h"
#include "Gui/TextDatabase.h"
#include "Help/HelpProfileFile.h"
#include "Help/Spirits.h"
#include "InfoConstants.h"
#include "Input/GameActionMapInterface.h"
#include "LHScriptX/FeatureScriptCommands.h"
#include "LHScriptX/Script.h"
#include "Locator.h"
#include "Magic/TownBelief.h"
#include "PlayerSlots.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "ScriptHeaders/ScriptInterfaceLevels.h"
#include "Sound/CustomMusicPlayerInterface.h"
#include "Sound/GameMusic.h"
#include "Sound/MusicMood.h"
#include "Sound/SoundRequestInterface.h"
#include "TestbedScenarioRunnerParts.h"
#include "Visual/RendererRequestsInterface.h"
#include "Windowing/WindowingInterface.h"

#include "../Profiler.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;
using namespace openblack::testbed_scenarios::runner_parts;
using openblack::ecs::archetypes::CreatureArchetype;
using openblack::ecs::components::Creature;
using openblack::ecs::components::CreatureAnimation;
using openblack::ecs::components::CreatureMindState;
using openblack::ecs::components::CreatureNeeds;
using openblack::ecs::components::Transform;

namespace
{

/// The lines of the log of commands kept
constexpr size_t k_LogLines = 8;
/// The least the overview takes in either way of the middle of what it frames
constexpr float k_MinOverviewHalfSize = 25.0f;
/// The live results of a benchmark are summed up again every so many frames
constexpr uint32_t k_FramesPerSummary = 30;
/// What the store of a crowd's town starts with
constexpr uint32_t k_CrowdFood = 2000;

constexpr uint32_t k_CrowdWood = 2000;

double Milliseconds(std::chrono::steady_clock::duration duration)
{
	return std::chrono::duration<double, std::milli>(duration).count();
}

#ifdef NDEBUG
constexpr std::string_view k_Build = "optimised";
#else
constexpr std::string_view k_Build = "debug";
#endif

/// Where a creature faces on the land; its mesh looks back along +z
glm::vec2 AheadOf(const Transform& transform)
{
	const auto ahead = -(transform.rotation * glm::vec3(0.0f, 0.0f, 1.0f));
	return {ahead.x, ahead.z};
}

/// Whether a command sends its creature somewhere, or has it face, throw or point somewhere
bool HasPoint(Kind kind)
{
	return kind == Kind::WalkTo || kind == Kind::RunTo || kind == Kind::FleeFrom || kind == Kind::TurnToFace ||
	       kind == Kind::ThrowAt || kind == Kind::PointAt || kind == Kind::TurnToFaceWhenStill;
}

/// The camera's fields of view across and up and down, in radians
glm::vec2 FieldsOfView(const Camera& camera)
{
	return {camera.GetHorizontalFieldOfView(), camera.GetVerticalFieldOfView()};
}

/// The player has grey rings of their own for a design, which the tattoo editor puts in the creature's block of
/// pictures when a tattoo is dropped
std::string GiveOwnTattooPicture(size_t design)
{
	if (!Locator::tattooEditorSystem::has_value() || design >= creature_tattoo::k_DesignCount)
	{
		return "no tattoo editor or no such design";
	}
	creature_tattoo::Picture rings {};
	constexpr int k_Middle = static_cast<int>(creature_tattoo::k_DesignSize) / 2;
	for (int y = 0; y < static_cast<int>(creature_tattoo::k_DesignSize); ++y)
	{
		for (int x = 0; x < static_cast<int>(creature_tattoo::k_DesignSize); ++x)
		{
			const auto distance = static_cast<int>(
			    std::sqrt(static_cast<float>(((x - k_Middle) * (x - k_Middle)) + ((y - k_Middle) * (y - k_Middle)))));
			const auto index = static_cast<size_t>((y * static_cast<int>(creature_tattoo::k_DesignSize)) + x);
			rings.at(index) = distance < k_Middle && (distance / 6) % 2 == 0 ? uint8_t {255} : uint8_t {0};
		}
	}
	auto& editor = Locator::tattooEditorSystem::value();
	auto pictures = editor.GetPictures();
	pictures.at(design) = rings;
	editor.SetPictures(pictures);
	return fmt::format("design {} has the player's own picture", design + 1);
}

/// The made-up round trip of the ping line in microseconds: 40 ms, then 8 ms more each second to 64 ms, then 40 ms
/// again
[[nodiscard]] uint64_t DemoRoundTrip(float seconds)
{
	constexpr uint64_t k_First = 40'000;
	constexpr uint64_t k_Step = 8'000;
	constexpr uint64_t k_Steps = 4;
	return k_First + (k_Step * (static_cast<uint64_t>(seconds) % k_Steps));
}

/// A message comes into the list of incoming messages: the message of the real weather with a picture, a chat line or an
/// e-mail
std::string AddIncomingMessage(size_t value, std::string_view text)
{
	if (!Locator::incomingMessageSystem::has_value())
	{
		return "failed: no list of incoming messages";
	}
	auto& messages = Locator::incomingMessageSystem::value();
	if (value == k_IncomingChat || value == k_IncomingMail)
	{
		const auto kind = value == k_IncomingChat ? incoming_messages::Kind::Chat : incoming_messages::Kind::Mail;
		messages.Add(kind, gui::ToUtf16(text), 0);
	}
	else if (value < incoming_messages::k_WeatherPictures)
	{
		messages.AddWeather(gui::ToUtf16(text), static_cast<uint8_t>(value));
	}
	else
	{
		return fmt::format("failed: no message for the value {}", value);
	}
	return fmt::format("{} messages in the list, the top one \"{}\"", messages.GetQueue().Count(),
	                   gui::ToUtf8(messages.GetQueue().ShownText()));
}

/// The chat of a network game: a god says a line, or the chat of this computer opens with a line in it
std::string Chat(GameStageInterface* stage, PlayerNames player, size_t value, std::string_view text)
{
	if (!Locator::chatSystem::has_value() || stage == nullptr || !stage->HasInterface())
	{
		return "failed: no chat";
	}
	auto& chat = Locator::chatSystem::value();
	// The god of this computer and the god that speaks have the names of the testbed
	const auto own = Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer() : PlayerNames::PLAYER_ONE;
	auto names = chat.GetNames();
	for (const auto god : {own, player})
	{
		if (static_cast<size_t>(god) < k_ChatNames.size())
		{
			names.try_emplace(god, k_ChatNames.at(static_cast<size_t>(god)));
		}
	}
	chat.SetNames(names);
	const auto line = gui::ToUtf16(text);
	if (value == k_ChatFriends)
	{
		return stage->OpenChatWithFriends();
	}
	if (value == k_ChatStaffSays || value == k_ChatAnnouncement)
	{
		return stage->PingServerSays(value, line);
	}
	if (value == k_ChatTypes)
	{
		stage->OpenChat(line);
		return fmt::format("the chat opens with \"{}\"", text);
	}
	if (value != k_ChatSays)
	{
		return fmt::format("failed: no chat for the value {}", value);
	}
	// The line of this computer goes through the turns, and the line of another god comes in as the turns give it
	if (player == own)
	{
		// The line goes to the other named gods
		std::vector<PlayerNames> others;
		for (const auto& [god, name] : names)
		{
			if (god != own)
			{
				others.push_back(god);
			}
		}
		chat.SendLine(line, others);
		return fmt::format("this computer says \"{}\"", text);
	}
	chat.Receive(player, player_commands::ChatSpeech {.recipients = {}, .text = line});
	return fmt::format("god {} says \"{}\"", static_cast<int>(player), text);
}

/// The game writes the help record of the player to their folder in the profile folder, then reads it again
std::string CheckProfileFolder()
{
	if (!Locator::profileFolderSystem::has_value() || !Locator::helpProfileSystem::has_value() ||
	    !Locator::playerProfileSystem::has_value())
	{
		return "failed: no profile folder";
	}
	const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile();
	if (!profile.has_value())
	{
		return "failed: no player profile";
	}
	const auto player = gui::ToUtf16(profile->name);
	auto& folder = Locator::profileFolderSystem::value();
	auto& record = Locator::helpProfileSystem::value();
	const auto written = help::profile::ToFileBytes(record.Get());
	if (!record.SaveRecord(folder, player))
	{
		return "failed: the game could not write the help record";
	}
	const auto read = folder.ReadPlayerFile(player, help::profile::k_FileName);
	const auto path = (*folder.GetPlayerFolder(player) / help::profile::k_FileName).generic_string();
	if (!read.has_value() || *read != written)
	{
		return fmt::format("failed: {} does not hold the help record", path);
	}
	record.LoadRecord(folder, player);
	if (help::profile::ToFileBytes(record.Get()) != written)
	{
		return "failed: the help record changed when the game read it again";
	}
	return fmt::format("passed: {} holds the help record ({} bytes) and reads back the same", path, read->size());
}

/// The custom music player plays a track of the custom music folder, or stops; the music moods look at once
std::string PlayCustomMusic(size_t track)
{
	auto* player = Locator::sounds::has_value() ? Locator::sounds::value().GetCustomMusic() : nullptr;
	if (player == nullptr || !Locator::musicMoodSystem::has_value())
	{
		return "failed: no custom music player";
	}
	auto& moods = Locator::musicMoodSystem::value();
	if (track == k_StopCustomMusic)
	{
		player->Stop();
		moods.UpdateOnGameTurn(true);
		return "the custom music stops";
	}
	moods.LoadFolder();
	if (track >= player->GetTracks().size())
	{
		return fmt::format("failed: the custom music folder has {} tracks", player->GetTracks().size());
	}
	player->SetCurrentTrack(track);
	player->Play();
	const auto id = music_mood::TrackId(player->GetSongName());
	const auto records = moods.GetRecords();
	const auto record = std::ranges::find(records, id, &music_mood::Record::id);
	const auto mood = record != records.end() && !record->emotions.empty() ? record->emotions.front().mood : 0.0f;
	moods.UpdateOnGameTurn(true);
	return fmt::format("plays {} (track {:08x}), the creature's mood for it {:.3f}", player->GetSongName(), id, mood);
}
} // namespace

namespace openblack::testbed_scenarios::runner_parts
{
/// The profiler's stages as benchmarks name them: the drawing of the reflection and of the main pass told apart, and
/// everything from drawing the scene on counted as rendering
std::vector<benchmark::StageInfo> BenchmarkStages()
{
	static const std::vector<std::string> k_Names = [] {
		std::vector<std::string> names;
		for (size_t i = 0; i < Profiler::k_StageNames.size(); ++i)
		{
			const auto stage = static_cast<Profiler::Stage>(i);
			std::string name(Profiler::k_StageNames.at(i));
			if (stage > Profiler::Stage::ReflectionPass && stage <= Profiler::Stage::ReflectionDrawParticles)
			{
				name = "Reflection " + name;
			}
			else if (stage > Profiler::Stage::MainPass && stage <= Profiler::Stage::MainPassDrawParticles)
			{
				name = "Main " + name;
			}
			names.push_back(std::move(name));
		}
		return names;
	}();
	std::vector<benchmark::StageInfo> stages;
	for (size_t i = 0; i < k_Names.size(); ++i)
	{
		stages.push_back({.name = k_Names.at(i), .render = static_cast<Profiler::Stage>(i) >= Profiler::Stage::SceneDraw});
	}
	return stages;
}

/// The debug windows leave the mouse alone while a scenario drives it, so that the real pointer resting on one of them
/// doesn't take the scenario's presses
void KeepDebugWindowsOffTheMouse(bool off)
{
	if (Locator::debugGui::has_value())
	{
		Locator::debugGui::value().KeepOffTheMouse(off);
	}
}

std::string_view Started(bool started)
{
	return started ? "started" : "can't";
}
} // namespace openblack::testbed_scenarios::runner_parts

void Runner::Start(const Scenario& scenario)
{
	// Loading until its things are laid out on the fresh testbed, so that the inspector's tools never see the land
	// without them
	const ecs::systems::InspectorLoading loading("scenario " + std::string(scenario.id));
	Stop();
	// The camera is the player's again before the scenario frames it, and the cave closes
	if (Locator::creatureModeSystem::has_value())
	{
		Locator::creatureModeSystem::value().Leave();
	}
	if (Locator::creatureCaveSystem::has_value() && Locator::creatureCaveSystem::value().IsOpen())
	{
		Locator::creatureCaveSystem::value().Close();
	}
	_scenario = &scenario;
	_running = true;
	_seconds = 0.0f;
	_lastNativeResult.reset();
	_fireBallTakeFrames.reset();
	_nextMiracleLog = 0.0f;
	_timeline = {};
	_creatures.clear();
	_slotCreatures.clear();
	_objects.clear();
	_birdFlocks.clear();
	_scriptFlock = entt::null;
	_keptByScript.clear();
	_madeByNatives.clear();
	_globes.clear();
	_dispensers.clear();
	_temples.clear();
	_throwsDue.clear();
	_turnsAsked.clear();
	_deviceRestoreAsked.reset();
	for (const auto& setup : scenario.throws)
	{
		_throwsDue.emplace_back(setup.delaySeconds);
	}
	_walks.clear();
	_particles.clear();
	_miracles.clear();
	_started.clear();
	_log.clear();
	_checks.Start(scenario.checks);
	_checksReported = false;
	_shot.reset();
	_crowdCreatures.clear();
	_village = {};
	_crowdNext = 0;
	_crowdAbodes.clear();
	_crowdEntities.clear();
	_crowdProgress = {};
	_settledFrames = 0;
	_recorder.reset();
	_liveResults = {};
	_framesSinceSummary = 0;
	_saved = false;
	if (scenario.crowd.has_value())
	{
		const auto& crowd = *scenario.crowd;
		if (crowd.kind == Crowd::Kind::Creatures)
		{
			// The towns among the creatures first, so that the creatures keep clear of their homes
			if (crowd.villagers > 0)
			{
				_village = LayOutVillagers(crowd.villagers, crowd.seed);
			}
			_crowdCreatures = LayOutCreatures(crowd.count, crowd.seed, TownCentres(_village), k_TownClearRadius);
		}
		else
		{
			_village = LayOutVillagers(crowd.count, crowd.seed);
		}
		_crowdProgress.total =
		    _village.towns.size() + _village.abodes.size() + _village.villagers.size() + _crowdCreatures.size();
		_recorder = std::make_unique<benchmark::Recorder>(BenchmarkStages(), _benchmark.frames);
	}

	// A fresh testbed: the last one's creatures, objects, footprints, weather and scripts all go with it
	if (Locator::landSystem::has_value())
	{
		auto& land = Locator::landSystem::value();
		if (scenario.environment.land == Land::GodsRingMap)
		{
			if (!land.LoadGodsRingMap())
			{
				_running = false;
				return;
			}
		}
		else if (scenario.environment.land == Land::Story)
		{
			// The land plays as the story plays it, with its scripts from a fresh start
			const auto path =
			    Locator::filesystem::value().GetPath<filesystem::Path::Scripts>() / std::string(scenario.environment.storyLand);
			if (!land.LoadMapWithFreshScripts(path, land_change::Kind::NewGame))
			{
				Log(fmt::format("The story land {} did not load", scenario.environment.storyLand));
				_running = false;
				return;
			}
		}
		else
		{
			land.LoadTestbed();
		}
	}
	if (!Locator::terrainSystem::has_value())
	{
		_running = false;
		return;
	}
	const auto& land = Locator::terrainSystem::value();
	_middle = (land.GetExtent().minimum + land.GetExtent().maximum) * 0.5f;

	SetUpEnvironment(scenario.environment);
	// The testbed's lake has a water point at the middle of its south edge, as a land's script marks its places to
	// drink. The creature drinks there.
	if (scenario.environment.land == Land::Testbed)
	{
		const glm::vec2 drinkAt {flat_land::k_LakeCentre.x,
		                         flat_land::k_LakeCentre.y - flat_land::k_LakeHalfExtent.y + (flat_land::k_CellSize * 0.5f)};
		ecs::archetypes::DrinkWaypointArchetype::Create({drinkAt.x, land.GetHeightAt(drinkAt), drinkAt.y});
	}
	if (const auto& temple = scenario.environment.temple; temple.has_value())
	{
		const auto at = MapPoint(_middle, *temple);
		ecs::archetypes::CitadelArchetype::Create({at.x, land.GetHeightAt(at), at.y}, PlayerNames::PLAYER_ONE, 0.0f,
		                                          glm::vec3(1.0f));
	}
	if (!scenario.objectsAfterLandScript)
	{
		PlaceObjects(scenario, _middle);
	}
	for (const auto& offset : scenario.sharks)
	{
		const auto at = MapPoint(_middle, offset);
		ecs::archetypes::SharkArchetype::Create({at.x, 0.0f, at.y}, 1.0f);
	}
	PlaceHighlights(scenario, _middle);
	// As on a land, the things are on the map before the footpaths come, so no footpath goes round them
	if (Locator::entitiesMap::has_value())
	{
		Locator::entitiesMap::value().Sync();
	}
	if (Locator::footpathSystem::has_value())
	{
		Locator::footpathSystem::value().ProcessObstacleChanges();
	}
	PlaceFootpaths(scenario, _middle);
	if (!scenario.landScript.empty())
	{
		lhscriptx::Script landScript({_middle.x, 0.0f, _middle.y}, map_coords::FromMetres(_middle));
		landScript.Load(std::string(scenario.landScript));
	}
	// The land's start makes the sheet of the players' symbols after the land's script, which switches on the computer
	// players
	if (scenario.environment.symbolsAfterLandScript)
	{
		if (_stage != nullptr)
		{
			_stage->MakePlayersSymbols(scenario.environment.profileSymbol);
		}
	}
	if (scenario.objectsAfterLandScript)
	{
		PlaceObjects(scenario, _middle);
	}
	PlaceBirds(scenario, _middle);
	PlaceCreatures(scenario, _middle);
	PlaceDispensers(scenario);
	// A network game fills its empty slots after the land script. The temples of the scenario must stand first.
	if (scenario.fillEmptySlots && Locator::computerPlayerSystem::has_value())
	{
		constexpr std::array k_Humans {PlayerNames::PLAYER_ONE};
		Locator::computerPlayerSystem::value().FillEmptySlots(computer_player::k_SlotGods, k_Humans);
		// The commands can name the creatures of the filled slots after the creatures of the scenario
		std::vector<entt::entity> filled;
		Locator::entitiesRegistry::value().Each<const Creature>([this, &filled](entt::entity entity, const Creature&) {
			if (std::ranges::find(_creatures, entity) == _creatures.end())
			{
				filled.push_back(entity);
			}
		});
		std::ranges::sort(filled);
		_slotCreatures = std::move(filled);
	}
	if (Locator::playerSystem::has_value())
	{
		for (const auto& ally : scenario.allies)
		{
			Locator::playerSystem::value().SetAlly(ally.player, ally.other, ally.strength);
		}
	}
	// With the scenario's things laid out on the land, each town of a player with a temple is given its worship site, as
	// a land's are once it is laid out
	if (Locator::worshipSiteSystem::has_value())
	{
		Locator::worshipSiteSystem::value().LandLaidOut();
	}
	if (!scenario.townMiracles.empty())
	{
		const auto& towns = Locator::entitiesRegistry::value().Context().towns;
		if (const auto town = towns.find(k_ScenarioTownId); town != towns.end())
		{
			for (const auto type : scenario.townMiracles)
			{
				debug::town_miracles::Apply({.town = town->second, .type = type});
			}
		}
	}
	if (scenario.worshipShare.has_value())
	{
		SetUpWorship(*scenario.worshipShare);
	}
	_startTurn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
	_computerActionsDone.assign(scenario.computerActions.size(), false);
	UpdateComputerActions();
	if (!scenario.debugWindow.empty() && Locator::debugGui::has_value())
	{
		Locator::debugGui::value().OpenWindow(scenario.debugWindow);
	}
	SetUpStatistics(scenario);
	if (Locator::fireflySystem::has_value())
	{
		for (const auto& [name, weight] : scenario.fireflyRewards)
		{
			Locator::fireflySystem::value().SetRewardWeight(name, weight);
		}
	}
	if (scenario.townBeliefInPlayer.has_value())
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto& towns = registry.Context().towns;
		if (const auto town = towns.find(k_ScenarioTownId); town != towns.end() && registry.Valid(town->second))
		{
			if (auto* impression = registry.TryGet<ecs::components::TownImpression>(town->second))
			{
				magic::town_belief::SetInPlayer(impression->belief, PlayerNames::PLAYER_ONE, *scenario.townBeliefInPlayer);
			}
		}
	}
	if (scenario.townBeliefInRival.has_value())
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto& towns = registry.Context().towns;
		if (const auto town = towns.find(k_ScenarioTownId); town != towns.end() && registry.Valid(town->second))
		{
			if (auto* impression = registry.TryGet<ecs::components::TownImpression>(town->second))
			{
				magic::town_belief::SetInPlayer(impression->belief, PlayerNames::PLAYER_TWO, *scenario.townBeliefInRival);
			}
		}
	}
	if (!scenario.townBeliefInPlayers.empty())
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto& towns = registry.Context().towns;
		if (const auto town = towns.find(k_ScenarioTownId); town != towns.end() && registry.Valid(town->second))
		{
			if (auto* impression = registry.TryGet<ecs::components::TownImpression>(town->second))
			{
				for (const auto& [player, belief] : scenario.townBeliefInPlayers)
				{
					magic::town_belief::SetInPlayer(impression->belief, player, belief);
				}
			}
		}
	}
	if (scenario.townOwner.has_value())
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto& towns = registry.Context().towns;
		if (const auto town = towns.find(k_ScenarioTownId); town != towns.end() && registry.Valid(town->second))
		{
			auto& owned = registry.Get<ecs::components::Town>(town->second);
			if (owned.owner != *scenario.townOwner)
			{
				// The town goes last in the list of its new player
				owned.owner = *scenario.townOwner;
				owned.gained = registry.Context().nextTownGained++;
			}
		}
	}
	if (scenario.townFeelingAboutCreatures.has_value())
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto& towns = registry.Context().towns;
		if (const auto town = towns.find(k_ScenarioTownId); town != towns.end() && registry.Valid(town->second))
		{
			const auto turn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
			for (const auto creature : _creatures)
			{
				ecs::villager_creature::SetFeeling(town->second, creature, *scenario.townFeelingAboutCreatures, turn);
			}
		}
	}
	if (scenario.tribalPower.has_value() && Locator::magicSystem::has_value())
	{
		Locator::magicSystem::value().SetTribalPower(PlayerNames::PLAYER_ONE, scenario.tribalPower->first,
		                                             scenario.tribalPower->second);
	}
	// The hand held still over the land, looked down on from above it
	if (scenario.hand.has_value() && Locator::magicSystem::has_value())
	{
		const auto at = MapPoint(_middle, scenario.hand->offset);
		const glm::vec3 point {at.x, land.GetHeightAt(at), at.y};
		const glm::vec3 hand = point + glm::vec3(0.0f, scenario.hand->height, 0.0f);
		_heldHand = ecs::systems::MagicSystemInterface::HandFrame {
		    .handPosition = hand,
		    .point = point,
		    .rayOrigin = hand + glm::vec3(0.0f, k_HandEyeHeight, 0.0f),
		    .rayDirection = glm::vec3(0.0f, -1.0f, 0.0f),
		    .cameraForward = glm::vec3(0.0f, -1.0f, 0.0f),
		    .overWorld = true,
		};
		_heldHandHeight = scenario.hand->height;
		Locator::magicSystem::value().DriveHand(*_heldHand);
	}
	for (const auto& miracle : scenario.miracles)
	{
		_miracles.push_back({.nextAt = miracle.delaySeconds, .letGoAt = std::nullopt, .spell = entt::null});
	}
	// The storms a land's setup script places, in the island's global climate
	if (Locator::weatherSystem::has_value())
	{
		for (const auto& storm : scenario.landStorms)
		{
			const auto at = MapPoint(_middle, storm.offset);
			const auto to = MapPoint(_middle, storm.destination);
			lhscriptx::FeatureScriptCommands::CreateWeatherStorm(
			    0, {at.x, land.GetHeightAt(at), at.y}, storm.age, storm.cloudCount, std::string(storm.shape),
			    std::string(storm.clouds), std::string(storm.weather), storm.speed, {to.x, land.GetHeightAt(to), to.y});
		}
	}
	for (size_t i = 0; i < scenario.particles.size(); ++i)
	{
		_particles.push_back({StartParticle(i), 0.0f});
	}
	_vortices.assign(scenario.vortices.size(), {});
	_rewards.assign(scenario.rewards.size(), entt::null);
	Frame(scenario.framing.shot, scenario.framing.creature, scenario.framing.distance);
	// In a network game the commands step by the turns from this turn
	_layoutTurn = Locator::time::has_value() ? std::optional(Locator::time::value().GetTurn()) : std::nullopt;
	if (_layoutTurn.has_value())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The testbed scenario {} is laid out at turn {}", scenario.id, *_layoutTurn);
	}
	Log(fmt::format("Started {}", scenario.name));
}

void Runner::Stop()
{
	if (!_running)
	{
		return;
	}
	_running = false;
	// The ping line belongs to the network game again
	if (_demoPingSince.has_value() && _stage != nullptr)
	{
		_stage->SetDemoRoundTrip(std::nullopt);
	}
	_demoPingSince.reset();
	// The debug option's bonus creatures are as they were
	if (_unlockedBefore.has_value() && Locator::config::has_value())
	{
		Locator::config::value().unlockedBonusCreatures = *_unlockedBefore;
	}
	_unlockedBefore.reset();
	// The land's number is as it was
	if (_landNumberBefore.has_value() && Locator::entitiesRegistry::has_value())
	{
		Locator::entitiesRegistry::value().Context().mapScriptGlobals.landNumber = *_landNumberBefore;
	}
	_landNumberBefore.reset();
	// The camera zones that the scenario set go
	if (_cameraZoneSet && Locator::cameraZoneSystem::has_value())
	{
		Locator::cameraZoneSystem::value().ClearZones();
	}
	_cameraZoneSet = false;
	if (_skirmishBefore.has_value() && Locator::entitiesRegistry::has_value())
	{
		// The end of the skirmish goes too, so that the world turns again
		auto& registry = Locator::entitiesRegistry::value();
		registry.Context().skirmish = *_skirmishBefore;
		registry.Context().gameEnd = {};
		std::vector<entt::entity> standings;
		registry.Each<const ecs::components::GameStanding>(
		    [&standings](entt::entity entity, const ecs::components::GameStanding&) { standings.push_back(entity); });
		for (const auto entity : standings)
		{
			registry.Remove<ecs::components::GameStanding>(entity);
		}
	}
	_skirmishBefore.reset();
	if (_networkGameBefore.has_value() && Locator::time::has_value())
	{
		Locator::time::value().SetNetworkGame(*_networkGameBefore);
	}
	_networkGameBefore.reset();
	// The players that the scenario added go
	if (Locator::entitiesRegistry::has_value())
	{
		auto& registry = Locator::entitiesRegistry::value();
		for (const auto player : _addedPlayers)
		{
			if (registry.Valid(player))
			{
				registry.Destroy(player);
			}
		}
		if (!_addedPlayers.empty() && Locator::playerSystem::has_value())
		{
			Locator::playerSystem::value().RegisterPlayers();
		}
	}
	_addedPlayers.clear();
	// The mouse is the player's again
	_sweep.reset();
	_heldHand.reset();
	_sweepMovesHeldHand = false;
	if (Locator::gameActionSystem::has_value())
	{
		if (_ownsPointer && Locator::gameActionSystem::value().GetScriptedPointer().has_value())
		{
			Locator::gameActionSystem::value().SetScriptedPointer(std::nullopt);
			KeepDebugWindowsOffTheMouse(false);
		}
		_ownsPointer = false;
	}
	// Its particle effects die away
	if (Locator::particleRuleSystem::has_value())
	{
		for (const auto& particle : _particles)
		{
			Locator::particleRuleSystem::value().CloseDown(particle.effect);
		}
	}
	_particles.clear();
	// Its vortices go, though the ground they levelled stays levelled as the game leaves it
	for (const auto& running : _vortices)
	{
		if (running.vortex != entt::null && Locator::entitiesRegistry::value().Valid(running.vortex))
		{
			Locator::entitiesRegistry::value().Destroy(running.vortex);
		}
	}
	_vortices.clear();
	// Its reward chests go, opened or not
	for (const auto chest : _rewards)
	{
		if (chest != entt::null && Locator::entitiesRegistry::value().Valid(chest))
		{
			Locator::entitiesRegistry::value().Destroy(chest);
		}
	}
	_rewards.clear();
	// Its held miracles stop, and the hand is the mouse's again
	if (Locator::magicSystem::has_value())
	{
		auto& magic = Locator::magicSystem::value();
		if (std::ranges::any_of(_miracles, [](const RunningMiracle& m) { return m.pressAt || m.releaseAt; }))
		{
			magic.ReleaseAction();
			magic.DiscardHeldSeed();
		}
		magic.DriveHand(std::nullopt);
		if (_scenario != nullptr && _scenario->tribalPower.has_value())
		{
			magic.SetTribalPower(PlayerNames::PLAYER_ONE, _scenario->tribalPower->first, 1.0f);
		}
		for (const auto& miracle : _miracles)
		{
			if (miracle.letGoAt.has_value())
			{
				Locator::magicSystem::value().CloseDown(miracle.spell);
			}
		}
	}
	_miracles.clear();
	// The hand lets go of a creature a scenario held it to
	if (Locator::creatureHandSystem::has_value() && Locator::creatureHandSystem::value().IsHeldByCommand())
	{
		Locator::creatureHandSystem::value().Release();
	}
	if (Locator::creaturePhysiologySystem::has_value())
	{
		auto& physiology = Locator::creaturePhysiologySystem::value();
		physiology.SetTimeScale(1.0f);
		physiology.SetFaintingEnabled(true);
	}
	if (Locator::footprintSystem::has_value())
	{
		Locator::footprintSystem::value().SetAprilFoolsOverride(std::nullopt);
	}
	SetUpDate(std::nullopt);
	if (Locator::skyClockSystem::has_value())
	{
		Locator::skyClockSystem::value().GetClock().SetRunning(true);
	}
	Log("Stopped");
}

std::optional<entt::entity> Runner::CreatureAt(size_t index) const
{
	// After the creatures of the scenario come the creatures of the filled slots
	const auto total = _creatures.size() + _slotCreatures.size();
	if (index >= total || !Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	const auto entity = index < _creatures.size() ? _creatures[index] : _slotCreatures[index - _creatures.size()];
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity) || !registry.AllOf<Creature>(entity))
	{
		return std::nullopt;
	}
	return entity;
}

void Runner::ApplyStates()
{
	auto& registry = Locator::entitiesRegistry::value();
	for (size_t i = 0; i < _creatures.size(); ++i)
	{
		const auto entity = CreatureAt(i);
		if (!entity.has_value())
		{
			continue;
		}
		const auto& setup = _scenario->creatures[i];
		auto* needs = registry.TryGet<CreatureNeeds>(*entity);
		auto* mind = registry.TryGet<CreatureMindState>(*entity);
		const auto* creature = registry.TryGet<const Creature>(*entity);
		// The body and the mind start from the species' tables on their first turn, over anything set before
		if (needs == nullptr || mind == nullptr || creature == nullptr || !needs->started || !mind->desires.has_value())
		{
			continue;
		}
		if (setup.pauseMind && setup.thinksAfterSeconds.has_value() && _seconds >= *setup.thinksAfterSeconds)
		{
			mind->paused = false;
		}
		if (_started[i] && !setup.hold)
		{
			continue;
		}
		auto overrides = setup.needs;
		if (_started[i])
		{
			// Its age is only set as it starts, so it still grows older
			overrides.age.reset();
		}
		Apply(overrides, needs->needs, creature->size);
		Apply(setup.desires, *mind->desires);
		if (!_started[i] && !setup.desires.empty())
		{
			// Its first thought was had with the species' desires; it thinks again with the scenario's
			mind->idle = {};
		}
		_started[i] = true;
	}
}

std::optional<entt::entity> Runner::GlobeAt(size_t index) const
{
	if (index >= _globes.size() || !Locator::entitiesRegistry::has_value() ||
	    !Locator::entitiesRegistry::value().Valid(_globes[index]))
	{
		return std::nullopt;
	}
	return _globes[index];
}

std::optional<entt::entity> Runner::ObjectAt(size_t index) const
{
	if (index >= _objects.size() || !Locator::entitiesRegistry::has_value() ||
	    !Locator::entitiesRegistry::value().Valid(_objects[index]))
	{
		return std::nullopt;
	}
	return _objects[index];
}

std::optional<entt::entity> Runner::TotemAt(size_t index) const
{
	const auto centre = ObjectAt(index);
	if (!centre.has_value())
	{
		return std::nullopt;
	}
	std::optional<entt::entity> totem;
	Locator::entitiesRegistry::value().Each<const ecs::components::VillageTotem>(
	    [&totem, &centre](entt::entity entity, const ecs::components::VillageTotem& component) {
		    if (component.townCentre == *centre)
		    {
			    totem = entity;
		    }
	    });
	return totem;
}

bool Runner::IsFree(size_t creature) const
{
	const auto entity = CreatureAt(creature);
	if (!entity.has_value())
	{
		// A creature that is gone holds nothing up
		return true;
	}
	if (Locator::creatureLocomotionSystem::has_value() && Locator::creatureLocomotionSystem::value().IsMoving(*entity))
	{
		return false;
	}
	const auto* animation = Locator::entitiesRegistry::value().TryGet<const CreatureAnimation>(*entity);
	return animation == nullptr || !creature_layers::IsPlaying(animation->body);
}

void Runner::Give(const Command& command)
{
	if (command.kind == Kind::HoldSeed || command.kind == Kind::DrawGesture || command.kind == Kind::SummonSeed ||
	    command.kind == Kind::PressKey || command.kind == Kind::HandTakeFireBall || command.kind == Kind::HandTapObject ||
	    command.kind == Kind::HandTake || command.kind == Kind::HandApply || command.kind == Kind::HandTakeShownSeed ||
	    command.kind == Kind::HandPour || command.kind == Kind::HandRelease || command.kind == Kind::SetReportLatency ||
	    command.kind == Kind::RemoveObject || command.kind == Kind::HandGripTotem ||
	    command.kind == Kind::AttachObjectScriptSound || command.kind == Kind::SayLine ||
	    command.kind == Kind::PlayGuidanceVoice || command.kind == Kind::ScriptRelease || command.kind == Kind::FillSiteIcons ||
	    command.kind == Kind::BuildWorshipSites || command.kind == Kind::ScriptBuildsWorshipSites ||
	    command.kind == Kind::PressPlainKey || command.kind == Kind::PlainKeyDown || command.kind == Kind::PlainKeyRepeat ||
	    command.kind == Kind::PlainKeyUp || command.kind == Kind::ChargeIconWithSeedHeld ||
	    command.kind == Kind::TakeIconSeed || command.kind == Kind::TapSiteIcon || command.kind == Kind::TapTempleEntrance ||
	    command.kind == Kind::FinishBuilding || command.kind == Kind::ScaffoldStartsSite ||
	    command.kind == Kind::DestroyScaffoldSite || command.kind == Kind::ScaffoldClearsGround ||
	    command.kind == Kind::FillBuiltAmount || command.kind == Kind::PauseGame || command.kind == Kind::SetGameSpeed ||
	    command.kind == Kind::RemoveTeleportStone || command.kind == Kind::MakeDance || command.kind == Kind::OtherGodHand ||
	    command.kind == Kind::ObserverCamera || command.kind == Kind::OwnGod || command.kind == Kind::BringCreature ||
	    command.kind == Kind::ComputerPersonality || command.kind == Kind::ComputerUntaughtSends ||
	    command.kind == Kind::HandCastOnDispenser || command.kind == Kind::HandCastOnCreature ||
	    command.kind == Kind::CloseHeldMiracle || command.kind == Kind::JoinFootball)
	{
		const auto result = GivePlayerCommand(command);
		Log(fmt::format("{:.1f}s: {}{}{}", _seconds, Name(command.kind), result.empty() ? "" : ": ", result));
		return;
	}
	if (IsPointerCommand(command.kind))
	{
		const auto result = GivePointerCommand(command);
		const auto line = fmt::format("{:.2f}s: {}: {}", _seconds, Name(command.kind), result);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Testbed {}", line);
		Log(line);
		return;
	}
	if (command.kind == Kind::CameraLooksAt)
	{
		const auto look = MapPoint(_middle, command.point);
		// The eye is 15 m south of the point and 15 m above it, so that a script that waits for the camera within 30 m
		// of the point goes on
		const auto& land = Locator::terrainSystem::value();
		const auto height = land.GetHeightAt(look);
		Locator::camera::value().SetOrigin({look.x, height + 15.0f, look.y - 15.0f}).SetFocus({look.x, height, look.y});
		Log(fmt::format("{:.1f}s: {}", _seconds, Name(command.kind)));
		return;
	}
	if (command.kind == Kind::ClickScroll)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), ClickScroll(command)));
		return;
	}
	if (command.kind == Kind::ClickCreatureAt)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), ClickCreatureAt(command)));
		return;
	}
	if (command.kind == Kind::TapRewardNear)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), TapRewardNear(command)));
		return;
	}
	if (command.kind == Kind::ClickCreatureNear)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), ClickCreatureNear(command)));
		return;
	}
	if (command.kind == Kind::HandTakeNear)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), HandTakeNear(command)));
		return;
	}
	if (command.kind == Kind::RunScript)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), RunScript(command)));
		return;
	}
	if (command.kind == Kind::CallNative)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), CallNative(command)));
		return;
	}
	if (command.kind == Kind::StopScript)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), StopScript(command)));
		return;
	}
	if (command.kind == Kind::RecordChallenge)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), RecordChallenge(command)));
		return;
	}
	if (GiveFrontEndCommand(command))
	{
		return;
	}
	if (GiveStoryCommand(command))
	{
		return;
	}
	GiveCreatureCommand(command);
}

bool Runner::GiveFrontEndCommand(const Command& command)
{
	if (command.kind == Kind::OpenTattooMenu)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind),
		                (_stage != nullptr ? _stage->OpenTattooMenu() : "failed: no menu")));
		return true;
	}
	if (command.kind == Kind::ShowDemoPing)
	{
		_demoPingSince = command.value != 0 ? std::optional(_seconds) : std::nullopt;
		if (!_demoPingSince.has_value() && _stage != nullptr)
		{
			_stage->SetDemoRoundTrip(std::nullopt);
		}
		UpdateDemoPing();
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), _demoPingSince.has_value() ? "shown" : "gone"));
		return true;
	}
	if (command.kind == Kind::TattooEditorAsNetworkGame)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind),
		                (_stage != nullptr ? _stage->TattooEditorAsNetworkGame(command.value != 0) : "failed: no interface")));
		return true;
	}
	if (command.kind == Kind::OpenControlsBox)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind),
		                (_stage != nullptr ? _stage->OpenControlsBox(command.value) : "failed: no menu")));
		return true;
	}
	if (command.kind == Kind::JoinOnline)
	{
		const bool asked = _stage != nullptr && _stage->RequestJoinOnline();
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), asked ? "asked" : "failed: no game"));
		return true;
	}
	if (command.kind == Kind::Chat)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind),
		                Chat(_stage, command.player, command.value, command.action)));
		return true;
	}
	if (command.kind == Kind::AddIncomingMessage)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), AddIncomingMessage(command.value, command.action)));
		return true;
	}
	if (command.kind == Kind::OpenMultiplayerBox)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind),
		                (_stage != nullptr ? _stage->OpenMultiplayerBox(command.value) : "failed: no interface")));
		return true;
	}
	if (command.kind == Kind::SkirmishBox)
	{
		if (command.value == 5 || command.value == 6)
		{
			// The game's own mind file takes the place of the creature that the profile keeps. This needs no interface, thus
			// the dedicated server brings the creature too.
			const auto path =
			    Locator::filesystem::value().GetPath<filesystem::Path::CreatureMind>(true) / std::string(command.mindFile);
			auto mind = std::make_shared<creaturemind::MindFileData>();
			const bool read = creaturemind::ReadFile(path, *mind) == creaturemind::MindResult::Success;
			const auto brought = ecs::creature_from_file::BringKept(PlayerNames::PLAYER_ONE, read ? mind : nullptr);
			// With 6, the profile also keeps the new creature in its file. Thus a load of a saved skirmish brings it again.
			if (command.value == 6 && brought.creature.has_value() && Locator::creatureCarryOverSystem::has_value())
			{
				Locator::creatureCarryOverSystem::value().KeepCreature(*brought.creature);
			}
			Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind),
			                brought.creature.has_value()
			                    ? fmt::format("the player's god brings creature {}", entt::to_integral(*brought.creature))
			                    : fmt::format("no creature, reason {}", static_cast<int>(brought.refusal))));
			return true;
		}
		if (_stage == nullptr || !_stage->HasInterface())
		{
			Log(fmt::format("{:.1f}s: {}: failed: no interface", _seconds, Name(command.kind)));
			return true;
		}
		if (command.value == 1 || command.value == 4)
		{
			Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), _stage->OpenSkirmishMenu(command.value == 4)));
			return true;
		}
		if (command.value == 3)
		{
			// The two lines of the script of Two Gods
			auto& lands = Locator::landSystem::value();
			lands.ShowLandMessage({.kind = land_message::Message::Kind::Start, .text = "Two Gods", .number = 2});
			lands.ShowLandMessage(
			    {.kind = land_message::Message::Kind::AddLine, .text = "Battle another god for this realm", .number = 2});
			Log(fmt::format("{:.1f}s: {}: the land information box shows", _seconds, Name(command.kind)));
			return true;
		}
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), _stage->OpenSkirmishBox(command.value == 2)));
		return true;
	}
	if (command.kind == Kind::EndBox)
	{
		if (_stage == nullptr || !_stage->HasInterface())
		{
			Log(fmt::format("{:.1f}s: {}: failed: no interface", _seconds, Name(command.kind)));
			return true;
		}
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), _stage->OpenEndBox(command.value)));
		return true;
	}
	if (command.kind == Kind::OpenMenuPage)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind),
		                (_stage != nullptr ? _stage->OpenMenuPage(command.value, command.ctrl) : "failed: no menu")));
		return true;
	}
	if (command.kind == Kind::PressMenuKey)
	{
		Log(fmt::format(
		    "{:.1f}s: {}: {}", _seconds, Name(command.kind),
		    (_stage != nullptr ? _stage->PressMenuKey(command.value, command.ctrl) : "failed: the menu is not open")));
		return true;
	}
	if (command.kind == Kind::ShowCustomMusicControls)
	{
		if (_stage == nullptr || !_stage->HasInterface())
		{
			Log(fmt::format("{:.1f}s: {}: failed: no interface", _seconds, Name(command.kind)));
			return true;
		}
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), _stage->ShowCustomMusicControls(command.value)));
		return true;
	}
	if (command.kind == Kind::StartScriptMusic)
	{
		if (command.value >= static_cast<size_t>(audio::MusicType::_COUNT))
		{
			Log(fmt::format("{:.1f}s: {}: no music type {}", _seconds, Name(command.kind), command.value));
			return true;
		}
		const auto type = static_cast<audio::MusicType>(command.value);
		if (Locator::scriptMusicSystem::has_value())
		{
			Locator::scriptMusicSystem::value().ResetLyrics();
			Locator::scriptMusicSystem::value().Start(type);
		}
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), audio::GetMusicTypeName(type)));
		return true;
	}
	if (command.kind == Kind::PlayCustomMusic)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), PlayCustomMusic(command.value)));
		return true;
	}
	if (command.kind == Kind::OwnTattooPicture)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), GiveOwnTattooPicture(command.value)));
		return true;
	}
	if (command.kind == Kind::ProfileFolderCheck)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), CheckProfileFolder()));
		return true;
	}
	return false;
}

bool Runner::GiveStoryCommand(const Command& command)
{
	if (command.kind == Kind::PlayBoatFilm)
	{
		Log(fmt::format("{:.1f}s: {}", _seconds, PlayBoatFilm(command)));
		return true;
	}
	if (command.kind == Kind::EnterTemple)
	{
		const auto room = static_cast<TempleRoom>(command.value);
		if (Locator::temple::has_value())
		{
			auto& temple = Locator::temple::value();
			// In the temple, no room is a walk through the main room's way out
			if (temple.Active() && room == TempleRoom::Unknown)
			{
				temple.EnterRoom(room);
			}
			else if (temple.Active())
			{
				temple.GoToRoom(room);
			}
			else
			{
				temple.ActivateAfterFrame(room);
			}
		}
		Log(fmt::format("{:.1f}s: the player goes into temple room {}", _seconds, command.value));
		return true;
	}
	if (command.kind == Kind::Reframe)
	{
		Frame(_scenario->framing.shot, _scenario->framing.creature, _scenario->framing.distance);
		Log(fmt::format("{:.1f}s: the camera is put back", _seconds));
		return true;
	}
	if (command.kind == Kind::RestoreDeviceInDomeFollow)
	{
		_deviceRestoreAsked =
		    command.value == 1 ? graphics::device_restore::Request::Lose : graphics::device_restore::Request::MakeAgain;
		Log(fmt::format("{:.1f}s: the graphics device is made again when the dome next follows the sky", _seconds));
		return true;
	}
	if (command.kind == Kind::RestoreDevice)
	{
		const bool lose = command.value == 1;
		if (Locator::rendererRequests::has_value())
		{
			Locator::rendererRequests::value().RequestDeviceRestore(lose ? graphics::device_restore::Request::Lose
			                                                             : graphics::device_restore::Request::MakeAgain);
		}
		Log(fmt::format("{:.1f}s: the graphics device is {}", _seconds, lose ? "lost and made again" : "made again"));
		return true;
	}
	if (command.kind == Kind::PutLoadedArena)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), PutLoadedArena(command)));
		return true;
	}
	if (command.kind == Kind::SetHour)
	{
		if (Locator::skyClockSystem::has_value())
		{
			Locator::skyClockSystem::value().SetTime(command.hour);
		}
		Log(fmt::format("{:.1f}s: the hour is {:.1f}", _seconds, command.hour));
		return true;
	}
	if (command.kind == Kind::Advisor)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), GiveAdvisorCommand(command)));
		return true;
	}
	if (command.kind == Kind::SetOpenClose || command.kind == Kind::LayGateStone)
	{
		GiveSceneryCommand(command);
		return true;
	}
	if (command.kind == Kind::SetAlignment)
	{
		if (Locator::alignmentSystem::has_value())
		{
			Locator::alignmentSystem::value().SetPlayerAlignment(command.player, command.alignment);
		}
		Log(fmt::format("{:.1f}s: the alignment is {:.2f}", _seconds, command.alignment));
		return true;
	}
	if (command.kind == Kind::SaveGame || command.kind == Kind::LoadGame)
	{
		const auto slot = static_cast<uint32_t>(command.value);
		const bool saving = command.kind == Kind::SaveGame;
		bool done = false;
		if (Locator::saveGameSystem::has_value())
		{
			auto& saves = Locator::saveGameSystem::value();
			done = saving ? saves.Save(slot, u"Testbed") : saves.Load(slot);
		}
		Log(fmt::format("{:.1f}s: {} slot {}: {}", _seconds, saving ? "save into" : "load from", slot,
		                done ? "done" : "failed"));
		return true;
	}
	if (command.kind == Kind::PlayLogoEffect)
	{
		if (_stage != nullptr)
		{
			_stage->PlayLogoEffect();
		}
		Log(fmt::format("{:.1f}s: the logo effect played", _seconds));
		return true;
	}
	if (command.kind == Kind::ContinueGame)
	{
		const bool loaded = _stage != nullptr && _stage->ContinueFromExitSave();
		Log(fmt::format("{:.1f}s: continue from the save on exit: {}", _seconds,
		                loaded ? "loaded, the menu shows" : "a new game"));
		return true;
	}
	if (command.kind == Kind::QuickSave)
	{
		// As the key: the key checks the rules, and the command saves at the next turn
		const bool sent = Locator::saveGameSystem::has_value() && Locator::playerCommandSystem::has_value() &&
		                  Locator::saveGameSystem::value().PressQuickSave();
		if (sent)
		{
			Locator::playerCommandSystem::value().Submit(player_commands::QuickSave {});
		}
		Log(fmt::format("{:.1f}s: quick save: {}", _seconds, sent ? "sent" : "refused"));
		return true;
	}
	if (command.kind == Kind::SetTownBeliefInRival)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto& towns = registry.Context().towns;
		if (const auto town = towns.find(k_ScenarioTownId); town != towns.end() && registry.Valid(town->second))
		{
			if (auto* impression = registry.TryGet<ecs::components::TownImpression>(town->second))
			{
				magic::town_belief::SetInPlayer(impression->belief, PlayerNames::PLAYER_TWO, command.amount);
			}
		}
		Log(fmt::format("{:.1f}s: the town believes {:.2f} in the second player", _seconds, command.amount));
		return true;
	}
	if (command.kind == Kind::GiveLandTown)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto id = ecs::script_find::TownIdFromScript(static_cast<float>(command.value));
		const auto& towns = registry.Context().towns;
		auto* impression = [&]() -> ecs::components::TownImpression* {
			const auto town = towns.find(id);
			return town != towns.end() && registry.Valid(town->second)
			           ? registry.TryGet<ecs::components::TownImpression>(town->second)
			           : nullptr;
		}();
		if (impression != nullptr)
		{
			for (size_t i = 0; i < magic::town_belief::k_PlayerCount; ++i)
			{
				const auto other = static_cast<PlayerNames>(i);
				magic::town_belief::Set(impression->belief, other, other == command.player ? command.amount : 0.0f);
			}
		}
		Log(fmt::format("{:.1f}s: the land's town {} believes {:.2f} in the player{}", _seconds, command.value, command.amount,
		                impression != nullptr ? "" : " (no such town)"));
		return true;
	}
	if (command.kind == Kind::EndGuidanceWaits)
	{
		if (Locator::guidanceSystem::has_value())
		{
			Locator::guidanceSystem::value().EndWaits();
		}
		Log(fmt::format("{:.1f}s: the guidance's waits are over", _seconds));
		return true;
	}
	if (command.kind == Kind::RunHelpMessage)
	{
		const bool started =
		    Locator::helpMessageSystem::has_value() && Locator::helpMessageSystem::value().RunMessageSet(command.value);
		Log(fmt::format("{:.1f}s: the help message set {} {}", _seconds, command.value, started ? "runs" : "doesn't run"));
		return true;
	}
	if (command.kind == Kind::DestroyTemple)
	{
		// The scenario's temple, or the land's temple of the god of the slot when the scenario made no temples
		std::optional<entt::entity> temple;
		if (command.value < _temples.size())
		{
			temple = _temples.at(command.value);
		}
		else if (const auto god = player_slots::PlayerOfSlot(command.value); _temples.empty() && god.has_value())
		{
			Locator::entitiesRegistry::value().Each<const ecs::components::Temple>(
			    [&temple, god](entt::entity entity, const ecs::components::Temple& each) {
				    if (each.owner == *god)
				    {
					    temple = entity;
				    }
			    });
		}
		const bool known = temple.has_value();
		if (known && Locator::templeDestructionSystem::has_value())
		{
			Locator::templeDestructionSystem::value().Start(*temple);
		}
		Log(fmt::format("{:.1f}s: temple {} {}", _seconds, command.value, known ? "starts being destroyed" : "isn't there"));
		return true;
	}
	if (command.kind == Kind::LookAtTempleScroll)
	{
		if (Locator::temple::has_value())
		{
			Locator::temple::value().LookAtScroll(command.value);
		}
		Log(fmt::format("{:.1f}s: the temple's camera looks at scroll {}", _seconds, command.value));
		return true;
	}
	if (command.kind == Kind::LookAtSaveGamePicture)
	{
		if (Locator::temple::has_value())
		{
			Locator::temple::value().LookAtSaveGamePicture(static_cast<uint32_t>(command.value));
		}
		Log(fmt::format("{:.1f}s: the temple's camera looks at the picture of save slot {}", _seconds, command.value));
		return true;
	}
	if (command.kind == Kind::ReadTip)
	{
		// A sign far under the land, tapped once by the player
		const auto text = static_cast<uint32_t>(command.value);
		const auto category = static_cast<uint32_t>(std::lround(command.amount));
		const bool read = Locator::scriptHighlightSystem::has_value();
		if (read)
		{
			auto& highlights = Locator::scriptHighlightSystem::value();
			const auto sign =
			    highlights.Create(static_cast<uint32_t>(HighlightInfo::DidYouKnowSign), {0.0f, -1000.0f, 0.0f}, 0);
			highlights.SetProperties(sign, text, category);
			highlights.Tap(sign, true);
		}
		Log(fmt::format("{:.1f}s: tip {} of category {} {}", _seconds, text, category, read ? "read" : "not read"));
		return true;
	}
	if (command.kind == Kind::RunText)
	{
		// Not alone on its line and not waiting for a click, as the lessons' texts are shown
		const bool shown = Locator::helpTextSystem::has_value() && Locator::helpTextSystem::value().IsStarted() &&
		                   Locator::helpTextSystem::value().RunText(false, static_cast<uint32_t>(command.value), 0);
		Log(fmt::format("{:.1f}s: help text {} {}", _seconds, command.value, shown ? "shown" : "not shown"));
		return true;
	}
	if (command.kind == Kind::PlayHandDemo)
	{
		if (Locator::handDemoSystem::has_value())
		{
			Locator::handDemoSystem::value().Play(command.handDemo, 0, false, false);
		}
		Log(fmt::format("{:.1f}s: the hand demonstration {} plays", _seconds, command.handDemo));
		return true;
	}
	if (command.kind == Kind::FlyToBookmark)
	{
		const auto at = MapPoint(_middle, command.point);
		const glm::vec3 focus {at.x, Locator::terrainSystem::value().GetHeightAt(at), at.y};
		const glm::vec3 origin = focus + glm::vec3(0.0f, 50.0f, -80.0f);
		if (Locator::cameraBookmarkSystem::has_value())
		{
			Locator::cameraBookmarkSystem::value().SetBookmark(static_cast<size_t>(command.value), focus, std::nullopt, origin);
		}
		Locator::camera::value().GetModel().SetFlight(origin, focus, camera_fly::k_BookmarkRise);
		Log(fmt::format("{:.1f}s: the camera flies to bookmark {} at ({:.0f}, {:.0f}, {:.0f})", _seconds, command.value + 1,
		                focus.x, focus.y, focus.z));
		return true;
	}
	if (command.kind == Kind::TabToTown)
	{
		if (Locator::cameraBookmarkSystem::has_value())
		{
			Locator::cameraBookmarkSystem::value().FlyToNextTown(std::nullopt);
		}
		Log(fmt::format("{:.1f}s: the player presses the Tab key", _seconds));
		return true;
	}
	if (command.kind == Kind::SetInterfaceLevel)
	{
		if (Locator::cameraHelpSystem::has_value())
		{
			auto& help = Locator::cameraHelpSystem::value().Get();
			const bool known = help.SetInterfaceLevel(static_cast<int32_t>(command.value));
			if (known && Locator::playerSystem::has_value())
			{
				script::interface_levels::Apply(
				    Locator::entitiesRegistry::value(),
				    Locator::playerSystem::value().FindPlayer(PlayerNames::PLAYER_ONE).value_or(entt::null),
				    static_cast<int32_t>(command.value));
			}
			Log(fmt::format("{:.1f}s: the interface level is {}{}, the hand reaches {:.0f}", _seconds, command.value,
			                known ? "" : " (a level the game does not know)", help.handReach));
		}
		return true;
	}
	if (command.kind == Kind::PlayIntroFilm)
	{
		if (Locator::videoSystem::has_value())
		{
			auto& videos = Locator::videoSystem::value();
			videos.Play("Data/INTRO.bik");
			videos.ScheduleIntro();
		}
		Log(fmt::format("{:.1f}s: the intro film plays", _seconds));
		return true;
	}
	if (command.kind == Kind::PlayFallingSpellFilm)
	{
		// The script's film starts and the script's fade goes at once
		if (Locator::videoSystem::has_value())
		{
			Locator::videoSystem::value().StartFallingSpell();
		}
		if (Locator::cinematicDirectorSystem::has_value())
		{
			Locator::cinematicDirectorSystem::value().FadeBackToNormal(0);
		}
		Log(fmt::format("{:.1f}s: the falling spell's film starts", _seconds));
		return true;
	}
	if (command.kind == Kind::WideScreen)
	{
		// Held by a script, as the land's scripts hold them, under a number no script task has: a task of that number
		// stopping would take the bars away with it
		constexpr uint32_t k_ScriptOwner = std::numeric_limits<uint32_t>::max();
		if (Locator::cinematicDirectorSystem::has_value())
		{
			Locator::cinematicDirectorSystem::value().SetWideScreen(command.value != 0, k_ScriptOwner);
		}
		Log(fmt::format("{:.1f}s: the cinema bars slide {}", _seconds, command.value != 0 ? "in" : "out"));
		return true;
	}
	// With a value of 1, the scenario's object catches fire, with or without a creature. With a value of 2, it catches
	// at the most heat of its fire, so that it burns for a time.
	if (command.kind == Kind::SetAlight && (command.value == 1 || command.value == 2))
	{
		const auto burning = ObjectAt(command.object);
		if (Locator::fireSystem::has_value() && burning.has_value())
		{
			Locator::fireSystem::value().SetOnFire(*burning, command.value == 2 ? 1.0f : 0.0f);
		}
		Log(fmt::format("{:.1f}s: object {} {}", _seconds, command.object,
		                Locator::fireSystem::has_value() && burning.has_value() ? "alight" : "no fire"));
		return true;
	}
	return false;
}

void Runner::GiveSceneryCommand(const Command& command)
{
	const auto object = ObjectAt(command.object);
	if (!object.has_value() || !Locator::animatedStaticSystem::has_value())
	{
		Log(fmt::format("{:.1f}s: no such object", _seconds));
		return;
	}
	auto& scenery = Locator::animatedStaticSystem::value();
	if (command.kind == Kind::SetOpenClose)
	{
		const bool done = scenery.SetOpenState(*object, static_cast<int32_t>(command.value));
		Log(fmt::format("{:.1f}s: {} object {}{}", _seconds, command.value == 1 ? "opened" : "closed", command.object,
		                done ? "" : ", which isn't animated scenery"));
		return;
	}
	// As the hand gives it: laid in the plinth, the stone is used up, leaving its ghost
	const auto plinth = ObjectAt(command.value);
	if (plinth.has_value() && scenery.LayGateStone(*plinth, *object))
	{
		ecs::world_objects::LeaveGhost(*object);
		ecs::world_objects::Remove(*object);
		Log(fmt::format("{:.1f}s: object {} laid in the plinth, now worth {}", _seconds, command.object,
		                scenery.GateStoneValue(*plinth).value_or(0)));
		return;
	}
	Log(fmt::format("{:.1f}s: object {} isn't taken by object {}", _seconds, command.object, command.value));
}

void Runner::Frame(Shot shot, size_t creature, float distance)
{
	_shot = shot;
	_shotCreature = creature;
	_shotDistance = distance;
	UpdateCamera();
}

void Runner::UpdateCamera()
{
	if (!_shot.has_value() || _scenario == nullptr || !Locator::terrainSystem::has_value())
	{
		return;
	}
	// Creature Mode takes the camera from the scenario's shot for good
	if (Locator::creatureModeSystem::has_value() && Locator::creatureModeSystem::value().IsActive())
	{
		_shot.reset();
		return;
	}
	auto& camera = Locator::camera::value();
	auto& registry = Locator::entitiesRegistry::value();
	const auto& land = Locator::terrainSystem::value();
	std::optional<CameraPlacement> placement;
	switch (*_shot)
	{
	case Shot::Testbed:
		// As the testbed leaves it: south of the middle and up, looking north just east of the middle
		placement = CameraPlacement {
		    .origin = {_middle.x + flat_land::k_CameraAside, land.GetHeightAt(_middle) + flat_land::k_CameraHeight,
		               _middle.y - flat_land::k_CameraBack},
		    .focus = {_middle.x + flat_land::k_CameraAside, land.GetHeightAt(_middle), _middle.y},
		};
		break;
	case Shot::Overview:
	{
		std::vector<glm::vec2> points;
		for (size_t i = 0; i < _creatures.size(); ++i)
		{
			if (const auto entity = CreatureAt(i))
			{
				points.push_back(glm::xz(registry.Get<Transform>(*entity).position));
			}
		}
		for (const auto& object : _scenario->objects)
		{
			points.push_back(MapPoint(_middle, object.offset));
		}
		for (const auto& particle : _scenario->particles)
		{
			points.push_back(MapPoint(_middle, particle.offset));
		}
		for (const auto& extra : _scenario->framing.include)
		{
			points.push_back(MapPoint(_middle, extra));
		}
		// And everywhere the creatures are sent
		for (const auto& command : _scenario->commands)
		{
			if (HasPoint(command.kind))
			{
				points.push_back(MapPoint(_middle, command.point));
			}
		}
		const auto bounds = BoundsOf(points, glm::vec2(k_MinOverviewHalfSize));
		placement = Overview({bounds.centre.x, land.GetHeightAt(bounds.centre), bounds.centre.y}, bounds.halfSize,
		                     FieldsOfView(camera), _shotDistance);
		break;
	}
	case Shot::Placed:
	{
		const auto eye = MapPoint(_middle, {_scenario->framing.eye.x, _scenario->framing.eye.z});
		const auto look = MapPoint(_middle, {_scenario->framing.look.x, _scenario->framing.look.z});
		placement = CameraPlacement {
		    .origin = {eye.x, land.GetHeightAt(eye) + _scenario->framing.eye.y, eye.y},
		    .focus = {look.x, land.GetHeightAt(look) + _scenario->framing.look.y, look.y},
		};
		break;
	}
	case Shot::Follow:
	case Shot::Head:
		if (const auto entity = CreatureAt(_shotCreature))
		{
			// The camera keeps up with the body where the frame draws it
			const auto transform =
			    DrawnTransform(registry.Get<Transform>(*entity), registry.TryGet<CreatureAnimation>(*entity));
			const auto height = CreatureHeight(ShownSize(registry.Get<Creature>(*entity)));
			placement = *_shot == Shot::Follow ? Follow(transform.position, height, _shotDistance)
			                                   : Head(transform.position, AheadOf(transform), height, _shotDistance);
		}
		break;
	}
	if (placement.has_value())
	{
		camera.SetOrigin(placement->origin).SetFocus(placement->focus);
	}
	// A shot of everything is taken once; following and close ups keep up with the creature
	if (*_shot == Shot::Overview || *_shot == Shot::Testbed || *_shot == Shot::Placed)
	{
		_shot.reset();
	}
}

void Runner::UpdateDemoPing() const
{
	if (_demoPingSince.has_value() && _stage != nullptr)
	{
		_stage->SetDemoRoundTrip(DemoRoundTrip(_seconds - *_demoPingSince));
	}
}

void Runner::Update(float seconds)
{
	if (_scenario == nullptr)
	{
		return;
	}
	UpdateCamera();
	if (!_running)
	{
		return;
	}
	UpdateChecks();
	// Another land was loaded over the testbed
	const auto gone = [](entt::entity entity) { return !Locator::entitiesRegistry::value().Valid(entity); };
	if ((!_creatures.empty() && std::ranges::none_of(std::views::iota(size_t {0}, _creatures.size()),
	                                                 [this](size_t i) { return CreatureAt(i).has_value(); })) ||
	    (!_crowdEntities.empty() && gone(_crowdEntities.front())))
	{
		_running = false;
		_shot.reset();
		Log("Its creatures are gone");
		return;
	}
	_seconds += seconds;
	LogMiracles();
	Measure();
	SpawnCrowd();
	UpdateParticles(seconds);
	UpdateVortices();
	UpdateRewards();
	UpdateMiracles(seconds);
	UpdateVillagerWalks();
	UpdateTurnsAsked();
	UpdateDeviceRestore();
	UpdateThrows();
	UpdateComputerActions();
	WatchGesture();
	FinishTakingFireBall();
	ApplyStates();
	UpdatePointer(seconds);
	UpdateDemoPing();
	auto due = Advance(_timeline, _scenario->commands, _scenario->repeatFrom, seconds, [this](const Command& command) {
		const bool toHand =
		    command.kind == Kind::HoldSeed || command.kind == Kind::DrawGesture || command.kind == Kind::SummonSeed;
		if (toHand)
		{
			return !Locator::gestureSystem::has_value() || !Locator::gestureSystem::value().IsDrawingPath();
		}
		return IsFree(command.creature);
	});
	const auto turn = Locator::time::has_value() ? std::optional(Locator::time::value().GetTurn()) : std::nullopt;
	if (_followedGives.has_value() && turn.has_value())
	{
		due = GivesAt(*_followedGives, *turn, _scenario->commands.size());
	}
	for (const auto index : due)
	{
		if (_giveObserver && turn.has_value())
		{
			_giveObserver(*turn, index);
		}
		Give(_scenario->commands[index]);
	}
}

void Runner::BeforeTurn()
{
	if (!Locator::time::has_value())
	{
		return;
	}
	if (TurnStepOf(_layoutTurn, Locator::time::value().GetTurn()) == TurnStep::Update)
	{
		Update(std::chrono::duration<float>(ecs::systems::TimeSystemInterface::k_TurnDuration).count());
	}
}

std::optional<double> Runner::ReadCheck(const checks::Check& check) const
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto creatureIndex = [&check]() { return check.index; };
	switch (check.quantity)
	{
	case checks::Quantity::CreaturesAlive:
	{
		size_t alive = 0;
		for (size_t i = 0; i < _creatures.size() + _slotCreatures.size(); ++i)
		{
			alive += CreatureAt(i).has_value() ? 1 : 0;
		}
		return static_cast<double>(alive);
	}
	case checks::Quantity::ObjectsAlive:
	{
		size_t alive = 0;
		for (size_t i = 0; i < _objects.size(); ++i)
		{
			alive += ObjectAt(i).has_value() ? 1 : 0;
		}
		return static_cast<double>(alive);
	}
	case checks::Quantity::Entities:
		for (const auto& [name, count] : EntityCounts())
		{
			if (name == check.kind)
			{
				return static_cast<double>(count);
			}
		}
		return std::nullopt;
	case checks::Quantity::CreatureEnergy:
	case checks::Quantity::CreatureThirst:
	case checks::Quantity::CreatureExhaustion:
	case checks::Quantity::CreaturePoo:
	case checks::Quantity::CreatureWarmth:
	{
		const auto creature = CreatureAt(creatureIndex());
		const auto* needs = creature.has_value() ? registry.TryGet<const ecs::components::CreatureNeeds>(*creature) : nullptr;
		if (needs == nullptr)
		{
			return std::nullopt;
		}
		const auto& body = needs->needs;
		switch (check.quantity)
		{
		case checks::Quantity::CreatureEnergy:
			return static_cast<double>(body.energy);
		case checks::Quantity::CreatureThirst:
			return static_cast<double>(body.dehydration);
		case checks::Quantity::CreatureExhaustion:
			return static_cast<double>(body.exhaustion);
		case checks::Quantity::CreaturePoo:
			return static_cast<double>(body.poo);
		default:
			return static_cast<double>(body.warmth);
		}
	}
	case checks::Quantity::CreatureSize:
	{
		const auto creature = CreatureAt(creatureIndex());
		const auto* body = creature.has_value() ? registry.TryGet<const Creature>(*creature) : nullptr;
		return body != nullptr ? std::optional(static_cast<double>(body->size)) : std::nullopt;
	}
	case checks::Quantity::CreatureLife:
	{
		const auto creature = CreatureAt(creatureIndex());
		return creature.has_value() ? std::optional(static_cast<double>(ecs::world_objects::LifeOf(*creature))) : std::nullopt;
	}
	case checks::Quantity::CreatureDistance:
	{
		const auto creature = CreatureAt(creatureIndex());
		const auto* transform = creature.has_value() ? registry.TryGet<const Transform>(*creature) : nullptr;
		if (transform == nullptr)
		{
			return std::nullopt;
		}
		const auto goal = MapPoint(_middle, check.point);
		return static_cast<double>(glm::distance(glm::vec2(transform->position.x, transform->position.z), goal));
	}
	case checks::Quantity::ObjectResource:
	{
		const auto object = ObjectAt(check.index);
		if (!object.has_value() || !Locator::resourceStoreSystem::has_value())
		{
			return std::nullopt;
		}
		const auto& stores = Locator::resourceStoreSystem::value();
		return static_cast<double>(stores.GetResource(*object, ResourceType::Food)) +
		       static_cast<double>(stores.GetResource(*object, ResourceType::Wood));
	}
	case checks::Quantity::ObjectDistance:
	{
		const auto object = ObjectAt(check.index);
		const auto* transform = object.has_value() ? registry.TryGet<const Transform>(*object) : nullptr;
		if (transform == nullptr)
		{
			return std::nullopt;
		}
		const auto goal = MapPoint(_middle, check.point);
		return static_cast<double>(glm::distance(glm::vec2(transform->position.x, transform->position.z), goal));
	}
	case checks::Quantity::ObjectTemperature:
	{
		const auto object = ObjectAt(check.index);
		if (!object.has_value() || !Locator::fireSystem::has_value())
		{
			return std::nullopt;
		}
		return static_cast<double>(Locator::fireSystem::value().GetTemperature(*object));
	}
	case checks::Quantity::FireNear:
	{
		if (!Locator::fireSystem::has_value())
		{
			return std::nullopt;
		}
		const auto point = MapPoint(_middle, check.point);
		return Locator::fireSystem::value().IsFireNear({point.x, 0.0f, point.y}, check.radius) ? 1.0 : 0.0;
	}
	case checks::Quantity::RewardChestsInATown:
	{
		size_t inATown = 0;
		registry.Each<const ecs::components::Reward>([&registry, &inATown](const ecs::components::Reward& chest) {
			inATown += registry.Valid(chest.town) && registry.AllOf<ecs::components::Town>(chest.town) ? 1 : 0;
		});
		return static_cast<double>(inATown);
	}
	}
	return std::nullopt;
}

void Runner::UpdateChecks()
{
	if (_scenario == nullptr || _checksReported || !Locator::time::has_value())
	{
		return;
	}
	// A scenario with no checks says nothing, unless the command line asked for its verdict
	if (_scenario->checks.empty() && !_benchmark.reportChecks)
	{
		return;
	}
	if (!_checks.Done())
	{
		const auto turns = Locator::time::value().GetTurn() - _startTurn;
		_checks.Update(turns, [this](const checks::Check& check) { return ReadCheck(check); });
	}
	const auto verdict = _checks.GetVerdict();
	if (verdict == checks::Verdict::Waiting)
	{
		return;
	}
	_checksReported = true;
	auto lines = _checks.Report(_scenario->id);
	for (const auto& line : lines)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Scenario check: {}", line);
	}
	if (_stage != nullptr)
	{
		_stage->ReportChecks({.scenario = std::string(_scenario->id), .verdict = verdict, .lines = std::move(lines)},
		                     _benchmark.reportChecks);
	}
}

void Runner::UpdateTurnsAsked()
{
	if (!Locator::creatureLocomotionSystem::has_value() || !Locator::entitiesRegistry::has_value())
	{
		_turnsAsked.clear();
		return;
	}
	auto& locomotion = Locator::creatureLocomotionSystem::value();
	const auto& registry = Locator::entitiesRegistry::value();
	// A turn asked for again starts once the creature stands. A creature that is gone asks no more.
	std::erase_if(_turnsAsked, [&](const std::pair<entt::entity, glm::vec2>& asked) {
		return !registry.Valid(asked.first) || locomotion.TurnToFace(asked.first, asked.second);
	});
}

void Runner::UpdateDeviceRestore()
{
	if (!_deviceRestoreAsked.has_value() || !Locator::rendererRequests::has_value() || !Locator::skyClockSystem::has_value())
	{
		return;
	}
	const auto* dome = Locator::entitiesRegistry::value().TryGet<const ecs::components::SkyDome>(
	    Locator::skyClockSystem::value().GetEntities().dome);
	if (dome == nullptr)
	{
		return;
	}
	// Part way: the follow blended its first band and has more rows to blend
	const auto rowsDone = dome->follow.RowsDone();
	if (rowsDone == 0 || rowsDone >= sky_dome::k_Rows)
	{
		return;
	}
	Locator::rendererRequests::value().RequestDeviceRestore(*_deviceRestoreAsked);
	_deviceRestoreAsked.reset();
	Log(fmt::format("{:.1f}s: the graphics device is made again with {} rows of the dome blended for the sky type {:.3f}",
	                _seconds, rowsDone, dome->follow.Following()));
}

void Runner::Log(std::string line)
{
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Testbed: {}", line);
	_log.push_back(std::move(line));
	while (_log.size() > k_LogLines)
	{
		_log.pop_front();
	}
}

std::optional<CrowdProgress> Runner::GetCrowdProgress() const
{
	if (_scenario == nullptr || !_scenario->crowd.has_value())
	{
		return std::nullopt;
	}
	return _crowdProgress;
}

std::span<const benchmark::StageInfo> Runner::GetStages() const
{
	if (_recorder == nullptr)
	{
		return {};
	}
	return _recorder->Stages();
}

uint32_t Runner::GetWarmUpLeft() const
{
	return _settledFrames >= _benchmark.warmUpFrames ? 0 : _benchmark.warmUpFrames - _settledFrames;
}

void Runner::SpawnCrowd()
{
	if (_scenario == nullptr || !_scenario->crowd.has_value() || _crowdProgress.Done() ||
	    !Locator::terrainSystem::has_value() || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto start = std::chrono::steady_clock::now();
	const auto batch = std::min(_scenario->crowd->perFrame, _crowdProgress.total - _crowdNext);
	for (size_t i = 0; i < batch; ++i)
	{
		SpawnCrowdMember(_crowdNext++);
	}
	_crowdProgress.spawned = _crowdNext;
	_crowdProgress.spawnMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	++_crowdProgress.spawnFrames;
	if (_crowdProgress.Done())
	{
		Log(fmt::format("Spawned {} in {:.0f} ms over {} frames", _crowdProgress.total, _crowdProgress.spawnMs,
		                _crowdProgress.spawnFrames));
	}
}

void Runner::SpawnCrowdMember(size_t index)
{
	const auto& land = Locator::terrainSystem::value();
	const auto onLand = [&land, this](glm::vec2 offset) {
		const auto point = MapPoint(_middle, offset);
		return glm::vec3(point.x, land.GetHeightAt(point), point.y);
	};
	// The towns first, then their homes and stores, then the villagers who live in them, then the creatures
	const auto towns = _village.towns.size();
	const auto abodes = _village.abodes.size();
	const auto village = towns + abodes + _village.villagers.size();
	if (index >= village)
	{
		const auto& member = _crowdCreatures.at(index - village);
		_crowdEntities.push_back(CreatureArchetype::Create(onLand(member.offset), member.owner, member.species, 0,
		                                                   glm::radians(member.facingDegrees), k_DefaultSize,
		                                                   CreatureArchetype::StartBody(member.species)));
		return;
	}
	if (index < towns)
	{
		const auto& town = _village.towns.at(index);
		_crowdEntities.push_back(
		    ecs::archetypes::TownArchetype::Create(static_cast<int>(index), onLand(town.offset), town.owner, town.tribe));
		return;
	}
	if (index < towns + abodes)
	{
		const auto& abode = _village.abodes.at(index - towns);
		_crowdAbodes.push_back(ecs::archetypes::AbodeArchetype::Create(static_cast<uint32_t>(abode.town), onLand(abode.offset),
		                                                               abode.type, glm::radians(abode.yawDegrees), 1.0f,
		                                                               k_CrowdFood, k_CrowdWood));
		return;
	}
	const auto& member = _village.villagers.at(index - towns - abodes);
	const auto& home = _village.abodes.at(member.abode);
	const auto entity =
	    ecs::archetypes::VillagerArchetype::Create(onLand(home.offset), onLand(member.offset), member.type, member.age);
	_crowdEntities.push_back(entity);
	// It lives in the home laid out for it, rather than the first in its town with room
	auto& registry = Locator::entitiesRegistry::value();
	const auto abode = _crowdAbodes.at(member.abode);
	const auto& townIds = registry.Context().towns;
	const auto town = townIds.find(static_cast<uint32_t>(home.town));
	if (abode != entt::null && registry.Valid(abode) && town != townIds.end())
	{
		Locator::townSystem::value().AddVillagerToAbode(abode, entity);
		registry.Get<ecs::components::Villager>(entity).town = town->second;
	}
}

void Runner::Measure()
{
	if (_recorder == nullptr || !_crowdProgress.Done() || !Locator::profiler::has_value())
	{
		return;
	}
	if (_settledFrames < _benchmark.warmUpFrames)
	{
		++_settledFrames;
		return;
	}
	// The last frame, whole: it ended as this one started
	const auto& profiler = Locator::profiler::value();
	const auto& entry = profiler.GetEntries().at(profiler.GetEntryIndex(-1));
	const auto frame = entry.frameEnd - entry.frameStart;
	const auto& draw = entry.stages.at(static_cast<size_t>(Profiler::Stage::SceneDraw));
	const bool drawn = draw.finalized && draw.start >= entry.frameStart && draw.start <= entry.frameEnd;
	const auto update = drawn ? draw.start - entry.frameStart : frame;
	std::array<float, static_cast<size_t>(Profiler::Stage::_count)> stages {};
	for (size_t i = 0; i < stages.size(); ++i)
	{
		stages.at(i) = static_cast<float>(Milliseconds(entry.stages.at(i).total));
	}
	// What the renderer last drew, the frame before
	const auto draws = Locator::rendererRequests::has_value() ? Locator::rendererRequests::value().GetDrawsOfLastFrame() : 0u;
	_recorder->Add(static_cast<float>(Milliseconds(frame)), static_cast<float>(Milliseconds(update)),
	               static_cast<float>(Milliseconds(frame - update)), stages, static_cast<float>(draws));
	if (++_framesSinceSummary >= k_FramesPerSummary)
	{
		_framesSinceSummary = 0;
		_liveResults = _recorder->SummariseNewest(Runner::k_LiveFrames);
	}
	if (_benchmark.autoSave.has_value() && !_saved && _recorder->Count() >= _recorder->Capacity())
	{
		_saved = true;
		_liveResults = _recorder->Summarise();
		const auto written = SaveResults(_benchmark.autoSave);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Benchmark {}: frame mean {:.2f} ms p95 {:.2f} max {:.2f}; results {}",
		                   _scenario->id, _liveResults.frame.average, _liveResults.frame.p95, _liveResults.frame.max,
		                   written.has_value() ? written->generic_string() : "not written");
		if (_stage != nullptr)
		{
			_stage->RequestQuit();
		}
	}
}

std::vector<std::pair<std::string, size_t>> Runner::EntityCounts() const
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return {};
	}
	auto& registry = Locator::entitiesRegistry::value();
	return {
	    {"placed", registry.Size<Transform>()},
	    {"creatures", registry.Size<Creature>()},
	    {"villagers", registry.Size<ecs::components::Villager>()},
	    {"abodes", registry.Size<ecs::components::Abode>()},
	    {"towns", registry.Size<ecs::components::Town>()},
	};
}

std::optional<std::filesystem::path> Runner::SaveResults(std::optional<std::filesystem::path> base)
{
	if (_scenario == nullptr || _recorder == nullptr || _recorder->Count() == 0)
	{
		return std::nullopt;
	}
	if (!base.has_value())
	{
		const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		std::tm local {};
#ifdef _WIN32
		localtime_s(&local, &now);
#else
		localtime_r(&now, &local);
#endif
		base = std::filesystem::path("benchmarks") / fmt::format("{}_{}_{:04}{:02}{:02}_{:02}{:02}{:02}", _scenario->id,
		                                                         k_Build, local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
		                                                         local.tm_hour, local.tm_min, local.tm_sec);
	}
	benchmark::RunInfo run {
	    .scenarioId = std::string(_scenario->id),
	    .scenarioName = std::string(_scenario->name),
	    .build = std::string(k_Build),
	    .crowd = _crowdProgress.total,
	    .spawnMs = _crowdProgress.spawnMs,
	    .spawnFrames = _crowdProgress.spawnFrames,
	    .warmUpFrames = _benchmark.warmUpFrames,
	    .entityCounts = EntityCounts(),
	};
	if (Locator::windowing::has_value())
	{
		const auto size = Locator::windowing::value().GetSize();
		run.width = static_cast<uint32_t>(size.x);
		run.height = static_cast<uint32_t>(size.y);
	}
	std::error_code error;
	if (base->has_parent_path())
	{
		std::filesystem::create_directories(base->parent_path(), error);
	}
	auto jsonPath = *base;
	jsonPath += ".json";
	auto csvPath = *base;
	csvPath += ".csv";
	std::ofstream json(jsonPath, std::ios::binary);
	std::ofstream csv(csvPath, std::ios::binary);
	if (!json || !csv)
	{
		Log(fmt::format("Couldn't write {}", jsonPath.generic_string()));
		return std::nullopt;
	}
	const auto results = _recorder->Summarise();
	json << benchmark::ToJson(run, results, _recorder->Stages());
	csv << benchmark::ToCsv(run, results, _recorder->Stages());
	Log(fmt::format("Saved {}", jsonPath.generic_string()));
	return jsonPath;
}

std::string Runner::PlayBoatFilm(const Command& command) const
{
	if (!Locator::boatLaunchSystem::has_value() || !Locator::terrainSystem::has_value())
	{
		return "no boat film";
	}
	const auto dock = MapPoint(_middle, command.point);
	// The sailing clip starts the boat about 67 metres west and 59 south of where it is played from: played from there,
	// the boat sets off from the lake's north-east corner and crosses it to the south west
	constexpr glm::vec2 k_SailingClipStart {-67.3f, -59.4f};
	const auto corner = flat_land::k_LakeCentre + flat_land::k_LakeHalfExtent;
	const auto sailing = corner - k_SailingClipStart;
	const ecs::systems::BoatLaunchSystemInterface::Places places {
	    .dock = {dock.x, Locator::terrainSystem::value().GetHeightAt(dock), dock.y},
	    .sailingStart = {sailing.x, 0.0f, sailing.y},
	};
	Locator::boatLaunchSystem::value().Play(places);
	return fmt::format("the explorers' boat film plays from ({:.0f}, {:.0f})", dock.x, dock.y);
}

std::string Runner::RecordChallenge(const Command& command) const
{
	if (!Locator::terrainSystem::has_value())
	{
		return "no land";
	}
	// The camera is put over the place first, and the picture is of the frame drawn from there
	const auto at = MapPoint(_middle, command.point);
	const auto focus = glm::vec3(at.x, Locator::terrainSystem::value().GetHeightAt(at), at.y);
	const auto position = focus + glm::vec3(0.0f, 50.0f, 70.0f);
	auto& camera = Locator::camera::value();
	camera.SetOrigin(position);
	camera.SetFocus(focus);
	script::challenge_snapshots::Snapshot snapshot;
	snapshot.challenge = static_cast<uint32_t>(command.value);
	snapshot.reminderScript = "";
	snapshot.success = command.amount;
	snapshot.alignment = command.alignment;
	snapshot.position = position;
	snapshot.focus = focus;
	snapshot.quest = command.ctrl;
	// The title is the number of a help text. The dedicated server has no game interface, so it reads the names from
	// the same text scripts. Thus the server and the game record the same title.
	const auto* shown = _stage != nullptr ? _stage->GetTexts() : nullptr;
	const auto loaded = shown == nullptr ? std::optional(gui::LoadGameTexts()) : std::nullopt;
	const auto& texts = shown != nullptr ? *shown : *loaded;
	if (const auto number = HelpTextNumber(texts.GetHelpNames(), command.title); number.has_value())
	{
		snapshot.title = *number;
	}
	chlapi::CHLApi::RecordChallenge(snapshot, script::challenge_snapshots::Call::Start);
	return fmt::format("challenge {} ({}), {:.0f}% done{}", command.value, command.title, command.amount * 100.0f,
	                   command.ctrl ? ", a quest" : "");
}

std::string Runner::GiveAdvisorCommand(const Command& command) const
{
	if (!Locator::advisorSystem::has_value())
	{
		return "no advisors";
	}
	auto& control = Locator::advisorSystem::value().GetController();
	// The scripts name the good advisor 1 and the evil one 2
	const int32_t type = command.value == 0 ? 1 : 2;
	const auto who = command.value == 0 ? "the good advisor" : "the evil advisor";
	using Action = Command::AdvisorAction;
	switch (command.advisor)
	{
	case Action::Out:
		control.SpiritEject(type, false);
		return fmt::format("{} comes out", who);
	case Action::Appear:
		control.SpiritEject(type, true);
		return fmt::format("{} appears", who);
	case Action::Home:
		control.SpiritHome(type, false);
		return fmt::format("{} goes home", who);
	case Action::Vanish:
		control.SpiritHome(type, true);
		return fmt::format("{} vanishes", who);
	case Action::Cling:
		control.SpiritCling(type, command.point.x, command.point.y);
		return fmt::format("{} clings at ({:.2f}, {:.2f})", who, command.point.x, command.point.y);
	case Action::Fly:
		control.SpiritFly(type, command.point.x, command.point.y);
		return fmt::format("{} flies to ({:.2f}, {:.2f})", who, command.point.x, command.point.y);
	case Action::PointOnScreen:
	{
		const auto& screen = control.GetScreen();
		control.SpiritScreenPoint(type, glm::ivec2(static_cast<int32_t>(static_cast<float>(screen.width) * command.point.x),
		                                           static_cast<int32_t>(static_cast<float>(screen.height) * command.point.y)));
		return fmt::format("{} points at ({:.2f}, {:.2f}) on the screen", who, command.point.x, command.point.y);
	}
	case Action::PointAtLand:
	case Action::LookAtLand:
	{
		const auto at = MapPoint(_middle, command.point);
		const float height = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(at) : 0.0f;
		const glm::vec3 position(at.x, height, at.y);
		if (command.advisor == Action::LookAtLand)
		{
			control.SpiritLookAtPosition(type, position);
			return fmt::format("{} looks at ({:.0f}, {:.0f})", who, at.x, at.y);
		}
		control.SpiritPointPosition(type, position, command.gentle);
		return fmt::format("{} points at ({:.0f}, {:.0f}){}", who, at.x, at.y, command.gentle ? " out in the world" : "");
	}
	case Action::PlayAnim:
		control.SpiritPlayAnim(type, command.point.x, command.point.y, command.anim, command.amount);
		return fmt::format("{} plays {}", who, help::spirits::AnimName(command.anim));
	case Action::Feel:
		control.Dude(type == 1 ? help::spirits::k_GoodDude : help::spirits::k_EvilDude).SetEmotion(command.anim, 1.0f);
		return fmt::format("{} feels {}", who, help::spirits::EmotionName(command.anim));
	}
	return "";
}
