/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameProviders.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <chrono>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <utility>

#include <Inspector.h>
#include <InspectorQuery.h>
#include <L3DFile.h>
#include <LHVM.h>
#include <LNDFile.h>
#include <glm/matrix.hpp>
#include <glm/trigonometric.hpp>

#include "3D/DayNightClock.h"
#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "3D/OceanInterface.h"
#include "3D/TempleInteriorInterface.h"
#include "Audio/AudioManagerInterface.h"
#include "Audio/GameMusic.h"
#include "CHLApi.h"
#include "Camera/Camera.h"
#include "Camera/CameraZones.h"
#include "Common/EventManager.h"
#include "Common/GameRandom.h"
#include "Common/RandomNumberManager.h"
#include "Creature/BonusCreatures.h"
#include "Creature/CreatureDesires.h"
#include "Creature/LeashRules.h"
#include "Debug/DebugGuiInterface.h"
#include "Debug/TempleVillagersSurvey.h"
#include "ECS/ArtefactRules.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/AppleBarrel.h"
#include "ECS/Components/AudioEmitter.h"
#include "ECS/Components/BoatFilm.h"
#include "ECS/Components/CameraBookmark.h"
#include "ECS/Components/ChimneySmoke.h"
#include "ECS/Components/Cloud.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureCopy.h"
#include "ECS/Components/CreatureHelp.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Firefly.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/GuidanceHeard.h"
#include "ECS/Components/HiddenByState.h"
#include "ECS/Components/HighDetail.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Shark.h"
#include "ECS/Components/Sky.h"
#include "ECS/Components/SoundTag.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TempleExterior.h"
#include "ECS/Components/TownArtefact.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageLight.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Vortex.h"
#include "ECS/Components/WalkPath.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Map.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AbodeKnockSystemInterface.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/AnimatedStaticSystemInterface.h"
#include "ECS/Systems/AppleBarrelSystemInterface.h"
#include "ECS/Systems/ArtefactSystemInterface.h"
#include "ECS/Systems/BoatLaunchSystemInterface.h"
#include "ECS/Systems/BuildingDamageSystemInterface.h"
#include "ECS/Systems/CameraBookmarkSystemInterface.h"
#include "ECS/Systems/CameraHelpSystemInterface.h"
#include "ECS/Systems/CameraPathSystemInterface.h"
#include "ECS/Systems/CameraZoneSystemInterface.h"
#include "ECS/Systems/ChimneySmokeSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CloudSystemInterface.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureAudioSystemInterface.h"
#include "ECS/Systems/CreatureCarryOverSystemInterface.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureFizzSystemInterface.h"
#include "ECS/Systems/CreatureHairSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreaturePenSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/DialogueControlSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/EditorSystemInterface.h"
#include "ECS/Systems/ExplosionSystemInterface.h"
#include "ECS/Systems/FieldSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/FireflySystemInterface.h"
#include "ECS/Systems/FishFarmSystemInterface.h"
#include "ECS/Systems/FootprintSystemInterface.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/GestureEventsInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/GuidanceSystemInterface.h"
#include "ECS/Systems/HandDemoSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HeartbeatSystemInterface.h"
#include "ECS/Systems/HelpMessageSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/HelpSpeechSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/HighDetailSystemInterface.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/InspectorSystemInterface.h"
#include "ECS/Systems/IntroSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/MagicShieldSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/MiracleFxSystemInterface.h"
#include "ECS/Systems/MistSystemInterface.h"
#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PathfindingSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/RainSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/Systems/RewardSystemInterface.h"
#include "ECS/Systems/ScriptControlSystemInterface.h"
#include "ECS/Systems/ScriptHighlightSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/Systems/SharkSystemInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/SnowSystemInterface.h"
#include "ECS/Systems/SnowfallSystemInterface.h"
#include "ECS/Systems/SoundTagSystemInterface.h"
#include "ECS/Systems/TattooEditorSystemInterface.h"
#include "ECS/Systems/TeleportSystemInterface.h"
#include "ECS/Systems/TempleDestructionSystemInterface.h"
#include "ECS/Systems/TempleExteriorSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TipBubbleSystemInterface.h"
#include "ECS/Systems/TornadoSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/Systems/TutorialSkipSystemInterface.h"
#include "ECS/Systems/VegetationInterface.h"
#include "ECS/Systems/VideoSystemInterface.h"
#include "ECS/Systems/VillageLightSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/Systems/VortexSystemInterface.h"
#include "ECS/Systems/WalkPathSystemInterface.h"
#include "ECS/Systems/WaterRingSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "EditProviders.h"
#include "Editor/EditorSelection.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Game.h"
#include "GameControls.h"
#include "Graphics/RendererInterface.h"
#include "Gui/ScriptTexts.h"
#include "Gui/TextDatabase.h"
#include "Hand/HandTrace.h"
#include "Help/AdvisorModel.h"
#include "Help/AdvisorVoices.h"
#include "Help/DialogueText.h"
#include "Help/HelpProfile.h"
#include "Help/Spirits.h"
#include "InfoConstants.h"
#include "Input/GameActionMapInterface.h"
#include "Locator.h"
#include "Particles/LightSheet.h"
#include "Profiler.h"
#include "RegistryProviders.h"
#include "Resources/ResourcesInterface.h"
#include "RunControl.h"
#include "SystemProviders.h"
#include "Temple/TemplePeople.h"
#include "Windowing/WindowingInterface.h"
#include "WorldProviders.h"

using namespace openblack;
using namespace openblack::inspector;
using namespace openblack::ecs::components;

namespace
{

/// Every locator service and the query that inspects it, in the locator's order
constexpr std::array k_Coverage {
    LocatorCoverage {"config", "engine.config"},
    LocatorCoverage {"infoConstants", "engine.services"},
    LocatorCoverage {"profiler", "engine.frame"},
    LocatorCoverage {"events", "engine.services"},
    LocatorCoverage {"windowing", "engine.frame"},
    LocatorCoverage {"debugGui", "engine.services"},
    LocatorCoverage {"filesystem", "engine.services"},
    LocatorCoverage {"resources", "engine.resources"},
    LocatorCoverage {"rng", "engine.services"},
    LocatorCoverage {"gameRandom", "engine.services"},
    LocatorCoverage {"terrainSystem", "land.terrain"},
    LocatorCoverage {"oceanSystem", "land.ocean"},
    LocatorCoverage {"skySystem", "sky.state"},
    LocatorCoverage {"audio", "audio.state"},
    LocatorCoverage {"camera", "camera.state"},
    LocatorCoverage {"gameActionSystem", "players.input"},
    LocatorCoverage {"rendereringSystem", "engine.frame"},
    LocatorCoverage {"rendererInterface", "engine.frame"},
    LocatorCoverage {"dynamicsSystem", "physics.state"},
    LocatorCoverage {"pickingSystem", "players.pick"},
    LocatorCoverage {"cameraBookmarkSystem", "view.bookmarks"},
    LocatorCoverage {"cameraZoneSystem", "view.zones"},
    LocatorCoverage {"cameraPathSystem", "view.state"},
    LocatorCoverage {"livingActionSystem", "living.action"},
    LocatorCoverage {"townSystem", "town.list"},
    LocatorCoverage {"resourceStoreSystem", "living.resource"},
    LocatorCoverage {"weatherSystem", "land.weather"},
    LocatorCoverage {"pathfindingSystem", "living.pathfinding"},
    LocatorCoverage {"entitiesRegistry", "ecs.components"},
    LocatorCoverage {"entitiesMap", "map.cell"},
    LocatorCoverage {"playerSystem", "players.list"},
    LocatorCoverage {"alignmentSystem", "players.list"},
    LocatorCoverage {"cameraHelpSystem", "view.state"},
    LocatorCoverage {"templeExteriorSystem", "temple.state"},
    LocatorCoverage {"templeDestructionSystem", "temple.state"},
    LocatorCoverage {"worshipSiteSystem", "worship.sites"},
    LocatorCoverage {"handSystem", "players.hand"},
    LocatorCoverage {"handGrabSystem", "players.hand"},
    LocatorCoverage {"temple", "temple.interior"},
    LocatorCoverage {"time", "game.state"},
    LocatorCoverage {"vegetation", "land.vegetation"},
    LocatorCoverage {"mistSystem", "land.atmosphere"},
    LocatorCoverage {"cloudSystem", "land.atmosphere"},
    LocatorCoverage {"villageLightSystem", "living.villages"},
    LocatorCoverage {"fieldSystem", "living.field"},
    LocatorCoverage {"animalSystem", "living.animal"},
    LocatorCoverage {"snowSystem", "land.snow"},
    LocatorCoverage {"snowfallSystem", "land.precipitation"},
    LocatorCoverage {"waterRingSystem", "particles.splash"},
    LocatorCoverage {"creatureAnimationSystem", "creatures.status"},
    LocatorCoverage {"creatureMindSystem", "creature.desires"},
    LocatorCoverage {"creatureLocomotionSystem", "creatures.status"},
    LocatorCoverage {"creatureHairSystem", "creatures.systems"},
    LocatorCoverage {"creatureAudioSystem", "creatures.systems"},
    LocatorCoverage {"creatureObjectActionSystem", "creatures.status"},
    LocatorCoverage {"creatureHandSystem", "creatures.systems"},
    LocatorCoverage {"footprintSystem", "creatures.systems"},
    LocatorCoverage {"creatureSkinSystem", "creatures.status"},
    LocatorCoverage {"editorSystem", "view.editor"},
    LocatorCoverage {"creaturePhysiologySystem", "creatures.systems"},
    LocatorCoverage {"leashSystem", "creatures.status"},
    LocatorCoverage {"creatureFightSystem", "creatures.status"},
    LocatorCoverage {"creatureModeSystem", "creatures.systems"},
    LocatorCoverage {"creatureCaveSystem", "creatures.systems"},
    LocatorCoverage {"cinematicDirectorSystem", "view.cinematic"},
    LocatorCoverage {"scriptControlSystem", "view.script_control"},
    LocatorCoverage {"dialogueControlSystem", "view.script_control"},
    LocatorCoverage {"helpSpeechSystem", "view.script_control"},
    LocatorCoverage {"highDetailSystem", "view.script_control"},
    LocatorCoverage {"walkPathSystem", "living.walk_paths"},
    LocatorCoverage {"danceSystem", "living.dances"},
    LocatorCoverage {"sharkSystem", "living.sharks"},
    LocatorCoverage {"introSystem", "living.intro"},
    LocatorCoverage {"boatLaunchSystem", "living.boat_launch"},
    LocatorCoverage {"soundTagSystem", "living.sound_tags"},
    LocatorCoverage {"rainSystem", "land.precipitation"},
    LocatorCoverage {"chimneySmokeSystem", "living.chimneys"},
    LocatorCoverage {"abodeKnockSystem", "view.state"},
    LocatorCoverage {"influenceSystem", "influence.hand"},
    LocatorCoverage {"townDesireSystem", "living.town_desires"},
    LocatorCoverage {"particleSystem", "particles.effects"},
    LocatorCoverage {"magicSystem", "magic.state"},
    LocatorCoverage {"gestureEvents", "players.gestures"},
    LocatorCoverage {"reactionSystem", "magic.reactions"},
    LocatorCoverage {"teleportSystem", "magic.teleport"},
    LocatorCoverage {"creatureCarryOverSystem", "creatures.systems"},
    LocatorCoverage {"creaturePenSystem", "creatures.systems"},
    LocatorCoverage {"creatureFizzSystem", "creatures.systems"},
    LocatorCoverage {"tattooEditorSystem", "creatures.systems"},
    LocatorCoverage {"tornadoSystem", "magic.state"},
    LocatorCoverage {"vortexSystem", "magic.vortices"},
    LocatorCoverage {"magicShieldSystem", "magic.state"},
    LocatorCoverage {"forestSystem", "living.forests"},
    LocatorCoverage {"fireflySystem", "living.fireflies"},
    LocatorCoverage {"fishFarmSystem", "living.fish_farms"},
    LocatorCoverage {"animatedStaticSystem", "living.animated_statics"},
    LocatorCoverage {"objectDrawListSystem", "view.draw_list"},
    LocatorCoverage {"appleBarrelSystem", "living.apple_barrel"},
    LocatorCoverage {"gestureSystem", "players.gestures"},
    LocatorCoverage {"miracleFxSystem", "magic.state"},
    LocatorCoverage {"fireSystem", "magic.fires"},
    LocatorCoverage {"explosionSystem", "magic.state"},
    LocatorCoverage {"rewardSystem", "living.rewards"},
    LocatorCoverage {"scriptObjects", "script.objects"},
    LocatorCoverage {"scriptHighlightSystem", "script.highlights"},
    LocatorCoverage {"buildingDamageSystem", "living.building"},
    LocatorCoverage {"inspector", "engine.services"},
    LocatorCoverage {"vm", "script.vm"},
    LocatorCoverage {"chlapi", "script.natives"},
    LocatorCoverage {"villageTotemSystem", "living.totems"},
    LocatorCoverage {"spellIconSystem", "town.spells"},
    LocatorCoverage {"artefactSystem", "living.artefacts"},
    LocatorCoverage {"heartbeatSystem", "players.heartbeat"},
    LocatorCoverage {"videoSystem", "view.video"},
    LocatorCoverage {"playerProfileSystem", "players.new_game"},
    LocatorCoverage {"tutorialSkipSystem", "players.new_game"},
    LocatorCoverage {"advisorSystem", "help.advisors"},
    LocatorCoverage {"helpTextSystem", "help.dialogue"},
    LocatorCoverage {"helpProfileSystem", "help.profile"},
    LocatorCoverage {"creatureHelpSystem", "help.creature"},
    LocatorCoverage {"helpMessageSystem", "help.messages"},
    LocatorCoverage {"guidanceSystem", "help.guidance"},
    LocatorCoverage {"tipBubbleSystem", "help.bubble"},
    LocatorCoverage {"handDemoSystem", "help.hand_demo"},
    LocatorCoverage {"helpSpeechSystem", "help.dialogue"},
    LocatorCoverage {"dialogueControlSystem", "help.dialogue"},
};

constexpr std::string_view k_NoRegistry = "there is no registry: no land is loaded";

Json Point(const glm::vec3& point)
{
	return {point.x, point.y, point.z};
}

Json Point(const glm::vec2& point)
{
	return {point.x, point.y};
}

Json Id(entt::entity entity)
{
	return entity == entt::null ? Json(nullptr) : Json(ToId(entity));
}

Json Id(const std::optional<entt::entity>& entity)
{
	return entity.has_value() ? Id(*entity) : Json(nullptr);
}

template <typename Value>
Json Optional(const std::optional<Value>& value)
{
	if (!value.has_value())
	{
		return nullptr;
	}
	if constexpr (std::is_same_v<Value, glm::vec3> || std::is_same_v<Value, glm::vec2>)
	{
		return Point(*value);
	}
	else
	{
		return *value;
	}
}

const ecs::Registry* Registry()
{
	return Locator::entitiesRegistry::has_value() ? &Locator::entitiesRegistry::value() : nullptr;
}

const InfoConstants* Info()
{
	return Locator::infoConstants::has_value() ? &Locator::infoConstants::value() : nullptr;
}

WorldSources World()
{
	return {.registry = &Registry, .info = &Info};
}

/// How many entities have a component
template <typename Component>
size_t CountOf(const ecs::Registry& registry)
{
	const auto* storage = registry.Underlying().storage<Component>();
	return storage != nullptr ? storage->size() : 0;
}

QueryDescription Query(std::string name, std::string description, std::vector<ParameterDescription> parameters = {},
                       ResultKind kind = ResultKind::Object, bool needsNear = false)
{
	return {.name = std::move(name),
	        .description = std::move(description),
	        .parameters = std::move(parameters),
	        .kind = kind,
	        .needsNear = needsNear,
	        .writes = false};
}

ParameterDescription IdParameter(std::string description)
{
	return {.name = "id", .type = "integer", .description = std::move(description), .required = true};
}

ParameterDescription PositionParameter()
{
	return {.name = "position", .type = "point", .description = "[x, z] or [x, y, z]", .required = true};
}

/// The entity a request names, if it exists
std::optional<entt::entity> EntityParam(const Json& params)
{
	const auto* registry = Registry();
	const auto it = params.find("id");
	const auto entity = it == params.end() ? std::nullopt : FromId(*it);
	if (registry == nullptr || !entity.has_value() || !registry->Valid(*entity))
	{
		return std::nullopt;
	}
	return entity;
}

std::optional<glm::vec3> PositionParam(const Json& params)
{
	const auto it = params.find("position");
	if (it == params.end())
	{
		return std::nullopt;
	}
	const auto point = ReadPoint(*it);
	if (!point.has_value())
	{
		return std::nullopt;
	}
	return glm::vec3(static_cast<float>((*point)[0]), static_cast<float>((*point)[1]), static_cast<float>((*point)[2]));
}

PlayerNames PlayerParam(const Json& params)
{
	const auto number = static_cast<int>(NumberMember(params, "player").value_or(0.0));
	return static_cast<PlayerNames>(std::clamp(number, 0, static_cast<int>(PlayerNames::NEUTRAL)));
}

/// A query of one locator service, answered only while the service is there
template <typename Service, typename Read>
FunctionProvider::Function Serve(std::string_view name, Read read)
{
	return [name, read](const QueryContext& context) -> QueryResult {
		if (!Service::has_value())
		{
			return QueryResult::Error(std::string(name) + " isn't there: no land is loaded, or this mode doesn't make it");
		}
		if constexpr (std::is_same_v<std::invoke_result_t<Read, decltype(Service::value()), const QueryContext&>, QueryResult>)
		{
			return read(Service::value(), context);
		}
		else
		{
			return QueryResult::Value(read(Service::value(), context));
		}
	};
}

/// A query of the registry, answered only while there is one
template <typename Read>
FunctionProvider::Function ServeRegistry(Read read)
{
	return [read](const QueryContext& context) -> QueryResult {
		const auto* registry = Registry();
		if (registry == nullptr)
		{
			return QueryResult::Error(std::string(k_NoRegistry));
		}
		if constexpr (std::is_same_v<std::invoke_result_t<Read, const ecs::Registry&, const QueryContext&>, QueryResult>)
		{
			return read(*registry, context);
		}
		else
		{
			return QueryResult::Value(read(*registry, context));
		}
	};
}

Json Listed(const ecs::Registry& registry, entt::entity entity)
{
	return ToListItem(entity, Describe(registry, entity, Info()));
}

std::string_view KindName(ecs::PhysicsEntry::Kind kind)
{
	switch (kind)
	{
	case ecs::PhysicsEntry::Kind::Other:
		return "other";
	case ecs::PhysicsEntry::Kind::Villager:
		return "villager";
	case ecs::PhysicsEntry::Kind::FelledTree:
		return "felled_tree";
	case ecs::PhysicsEntry::Kind::FelledTreeToppled:
		return "felled_tree_toppled";
	}
	return "unknown";
}

// The engine's services

/// The names of the breeder's bonus creatures the debug option unlocks
Json UnlockedBonusCreatureNames(const creature_swap::Unlocked& unlocked)
{
	auto names = Json::array();
	for (size_t i = 0; i < creature_swap::k_BonusCount; ++i)
	{
		if (unlocked.test(i))
		{
			names.push_back(std::string(creature_swap::NameOf(static_cast<creature_swap::Bonus>(i))));
		}
	}
	return names;
}

std::unique_ptr<ProviderInterface> EngineProvider()
{
	auto provider = std::make_unique<FunctionProvider>("engine");
	provider->Add(
	    Query("config", "The engine's settings: what is drawn, the display and the camera's clip"),
	    Serve<Locator::config>("the config", [](const EngineConfig& config, const QueryContext& /*context*/) {
		    return Json {
		        {"draw",
		         {{"sky", config.drawSky},
		          {"water", config.drawWater},
		          {"island", config.drawIsland},
		          {"entities", config.drawEntities},
		          {"vegetation", config.drawVegetation},
		          {"sprites", config.drawSprites},
		          {"bounding_boxes", config.drawBoundingBoxes}}},
		        {"shadows", {{"creatures", config.drawCreatureShadows}, {"dynamic_trees", config.dynamicTreeShadows}}},
		        {"wireframe", config.wireframe},
		        {"unlocked_bonus_creatures", UnlockedBonusCreatureNames(config.unlockedBonusCreatures)},
		        {"time_of_day", config.timeOfDay},
		        {"resolution", {config.resolution.x, config.resolution.y}},
		        {"vsync", config.vsync},
		        {"camera_fov", config.cameraXFov},
		        {"camera_near_clip", config.cameraNearClip},
		        {"gui_scale", config.guiScale},
		        {"running", config.running},
		    };
	    }));
	provider->Add(Query("frame", "The last frame: its time, the window's size, the renderer's flags and how much it draws"),
	              [](const QueryContext& /*context*/) {
		              Json result = Json::object();
		              if (Locator::profiler::has_value())
		              {
			              const auto& profiler = Locator::profiler::value();
			              const auto& entry = profiler.GetEntries()[profiler.GetEntryIndex(-1)];
			              result["frame_ms"] =
			                  std::chrono::duration<double, std::milli>(entry.frameEnd - entry.frameStart).count();
		              }
		              if (Locator::windowing::has_value())
		              {
			              const auto size = Locator::windowing::value().GetSize();
			              result["window"] = {size.x, size.y};
		              }
		              if (Locator::rendererInterface::has_value())
		              {
			              result["renderer_debug"] = Locator::rendererInterface::value().GetDebug();
			              result["renderer_profile"] = Locator::rendererInterface::value().GetProfile();
		              }
		              if (Locator::rendereringSystem::has_value())
		              {
			              const auto& context = Locator::rendereringSystem::value().GetContext();
			              result["drawn_entities"] = context.entityDraws.size();
			              result["instances"] = context.instanceUniforms.size();
			              result["trees"] = context.treeInstanceCount;
		              }
		              return QueryResult::Value(std::move(result));
	              });
	provider->Add(Query("resources", "How many of each kind of resource the caches hold"),
	              Serve<Locator::resources>("the resources",
	                                        [](resources::ResourcesInterface& resources, const QueryContext& /*context*/) {
		                                        return Json {
		                                            {"meshes", resources.GetMeshes().Size()},
		                                            {"textures", resources.GetTextures().Size()},
		                                            {"animations", resources.GetAnimations().Size()},
		                                            {"sounds", resources.GetSounds().Size()},
		                                            {"levels", resources.GetLevels().Size()},
		                                            {"creature_minds", resources.GetCreatureMinds().Size()},
		                                            {"particle_files", resources.GetParticleFiles().Size()},
		                                            {"camera_paths", resources.GetCameraPaths().Size()},
		                                        };
	                                        }));
	provider->Add(Query("services", "Which of the engine's services are there, the game's random seeds, the game's path, the "
	                                "debug windows' focus and the inspector's port"),
	              [](const QueryContext& /*context*/) {
		              Json result = {
		                  {"info_tables", Locator::infoConstants::has_value()},
		                  {"events", Locator::events::has_value()},
		                  {"rng", Locator::rng::has_value()},
		              };
		              if (Locator::gameRandom::has_value())
		              {
			              const auto seeds = Locator::gameRandom::value().GetSeeds();
			              result["seeds"] = {{"synced", seeds.synced}, {"local", seeds.local}};
		              }
		              if (Locator::filesystem::has_value())
		              {
			              result["game_path"] = Locator::filesystem::value().GetGamePath().generic_string();
		              }
		              if (Locator::debugGui::has_value())
		              {
			              result["debug_gui_focus"] = Locator::debugGui::value().StealsFocus();
			              result["mouse_over_debug_window"] = Locator::debugGui::value().IsMouseOverWindow();
		              }
		              if (Locator::inspector::has_value())
		              {
			              result["inspector_port"] = Locator::inspector::value().GetPort();
		              }
		              return QueryResult::Value(std::move(result));
	              });
	return provider;
}

// The land, the sea and the weather

std::unique_ptr<ProviderInterface> LandProvider()
{
	auto provider = std::make_unique<FunctionProvider>("land");
	provider->Add(Query("terrain", "The land: its extent, blocks and countries"),
	              Serve<Locator::terrainSystem>("the land", [](const LandIslandInterface& land, const QueryContext& /*c*/) {
		              const auto extent = land.GetExtent();
		              return Json {{"minimum", Point(extent.minimum)},
		                           {"maximum", Point(extent.maximum)},
		                           {"blocks", land.GetBlocks().size()},
		                           {"countries", land.GetCountries().size()}};
	              }));
	provider->Add(Query("height", "The land's height and normal under a point", {PositionParameter()}),
	              Serve<Locator::terrainSystem>("the land", [](const LandIslandInterface& land, const QueryContext& context) {
		              const auto position = PositionParam(context.params);
		              if (!position.has_value())
		              {
			              return QueryResult::Error("position must be [x, z] or [x, y, z]");
		              }
		              const glm::vec2 point(position->x, position->z);
		              return QueryResult::Value(
		                  {{"height", land.GetHeightAt(point)}, {"normal", Point(land.GetNormalAt(point))}});
	              }));
	provider->Add(Query("ocean", "The sea's textures"),
	              Serve<Locator::oceanSystem>("the sea", [](const OceanInterface& ocean, const QueryContext& /*c*/) {
		              return Json {{"texture", ocean.GetTexture()}};
	              }));
	provider->Add(Query("weather", "The climate: whether it runs, its storms and the season"),
	              Serve<Locator::weatherSystem>(
	                  "the weather", [](const ecs::systems::WeatherSystemInterface& weather, const QueryContext& /*c*/) {
		                  const auto turn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
		                  Json storms = Json::array();
		                  for (const auto& storm : weather.GetActiveStorms())
		                  {
			                  if (storms.size() == 8)
			                  {
				                  break;
			                  }
			                  storms.push_back({{"position", Point(storm.currentPosition)},
			                                    {"radius", storm.currentInnerRadius},
			                                    {"strength", storm.currentStrength},
			                                    {"age", storm.age},
			                                    {"dead", storm.dead}});
		                  }
		                  return Json {{"climate", weather.IsClimateSystemEnabled()},
		                               {"storm_creation", weather.IsStormCreationEnabled()},
		                               {"storm_count", weather.GetActiveStorms().size()},
		                               {"storms", std::move(storms)},
		                               {"season", weather.GetSeason(turn)},
		                               {"days_from_start", weather.GetDaysFromStart(turn)}};
	                  }));
	provider->Add(Query("weather_at", "The weather at a point: temperature, rain, snow, overcast, wind", {PositionParameter()}),
	              Serve<Locator::weatherSystem>("the weather",
	                                            [](ecs::systems::WeatherSystemInterface& weather, const QueryContext& context) {
		                                            const auto position = PositionParam(context.params);
		                                            if (!position.has_value())
		                                            {
			                                            return QueryResult::Error("position must be [x, z] or [x, y, z]");
		                                            }
		                                            const auto info = weather.GetWeather(*position);
		                                            return QueryResult::Value({{"temperature", info.temperature},
		                                                                       {"rain", info.rain},
		                                                                       {"snow", info.snow},
		                                                                       {"overcast", info.overcast},
		                                                                       {"wind", {info.windX, info.windZ}},
		                                                                       {"snow_cover", info.snowCover}});
	                                            }));
	provider->Add(
	    Query("snow", "The snow lying on the land: its cells, deepest and revision"),
	    Serve<Locator::snowSystem>("the snow", [](const ecs::systems::SnowSystemInterface& snow, const QueryContext& /*c*/) {
		    const auto depths = snow.GetDepths();
		    const auto deepest = depths.empty() ? 0.0f : *std::ranges::max_element(depths);
		    return Json {{"cells", depths.size()}, {"deepest", deepest}, {"revision", snow.GetRevision()}};
	    }));
	provider->Add(
	    Query("precipitation", "The rain's streaks and fall, and the falling snow's flakes"), [](const QueryContext& /*c*/) {
		    Json result = Json::object();
		    if (Locator::rainSystem::has_value())
		    {
			    const auto& rain = Locator::rainSystem::value();
			    result["rain"] = {
			        {"streaks", rain.GetStreaks().size()}, {"height", rain.GetHeight()}, {"speed", rain.GetFall().speed}};
		    }
		    if (Locator::snowfallSystem::has_value())
		    {
			    result["snowflakes"] = Locator::snowfallSystem::value().GetFlakes().size();
		    }
		    return QueryResult::Value(std::move(result));
	    });
	provider->Add(Query("vegetation", "How many trees and fields sway with the wind"),
	              ServeRegistry([](const ecs::Registry& registry, const QueryContext& /*c*/) {
		              return Json {{"swaying", Locator::vegetation::has_value()},
		                           {"trees", CountOf<Tree>(registry)},
		                           {"fields", CountOf<Field>(registry)}};
	              }));
	provider->Add(Query("atmosphere", "The mists and clouds over the land"),
	              ServeRegistry([](const ecs::Registry& registry, const QueryContext& /*c*/) {
		              return Json {{"mists", CountOf<Mist>(registry)},
		                           {"mist_system", Locator::mistSystem::has_value()},
		                           {"clouds", CountOf<Cloud>(registry)},
		                           {"cloud_system", Locator::cloudSystem::has_value()}};
	              }));
	return provider;
}

std::unique_ptr<ProviderInterface> SkyStateProvider(std::unique_ptr<ProviderInterface> sky)
{
	// The moon's provider, with the sky's own state beside it
	auto provider = std::make_unique<FunctionProvider>("sky");
	auto* moon = sky.release();
	std::shared_ptr<ProviderInterface> shared(moon);
	for (const auto& description : shared->Describe())
	{
		provider->Add(description,
		              [shared, name = description.name](const QueryContext& context) { return shared->Run(name, context); });
	}
	provider->Add(
	    Query("state", "The sky: its type, its time, the script clock's hour and the day and night's times"),
	    Serve<Locator::skySystem>("the sky", [](const ecs::systems::SkySystemInterface& sky, const QueryContext& /*c*/) {
		    const auto times = sky.GetDayNightTimes();
		    return Json {{"sky_type", sky.GetCurrentSkyType()},
		                 {"time", sky.GetTime()},
		                 {"script_hour", sky.GetClock().GetScriptTime()},
		                 {"visual_hour", sky.GetClock().GetVisualTime()},
		                 {"night_full", times.nightFull},
		                 {"dusk_start", times.duskStart},
		                 {"dusk_end", times.duskEnd},
		                 {"day_full", times.dayFull}};
	    }));
	return provider;
}

// The things living on the land

std::unique_ptr<ProviderInterface> LivingProvider()
{
	auto provider = std::make_unique<FunctionProvider>("living");
	provider->Add(
	    Query("animated_statics", "The scenery the scripts open and close: its open word and its gate stones' value", {},
	          ResultKind::List),
	    ServeRegistry([](const ecs::Registry& registry, const QueryContext& /*c*/) {
		    Json items = Json::array();
		    registry.Each<const AnimatedStatic>([&items, &registry](entt::entity entity, const AnimatedStatic& scenery) {
			    auto item = Listed(registry, entity);
			    item["open_state"] = scenery.openState;
			    if (Locator::animatedStaticSystem::has_value())
			    {
				    item["gate_stones_value"] = Optional(Locator::animatedStaticSystem::value().GateStoneValue(entity));
			    }
			    items.push_back(std::move(item));
		    });
		    return items;
	    }));
	provider->Add(
	    Query("apple_barrel",
	          "The apple barrel beside the local player's storage pit nearest their temple: the pit, the barrel and its "
	          "worm, where they stand and the worm's place in its clip; none while no pit carries it",
	          {}),
	    ServeRegistry([](const ecs::Registry& registry, const QueryContext& /*c*/) {
		    const auto barrel =
		        Locator::appleBarrelSystem::has_value() ? Locator::appleBarrelSystem::value().GetBarrel() : entt::null;
		    if (barrel == entt::null || !registry.Valid(barrel) || !registry.AllOf<AppleBarrel, Transform>(barrel))
		    {
			    return Json(nullptr);
		    }
		    const auto& found = registry.Get<const AppleBarrel>(barrel);
		    Json result = {{"barrel", Id(barrel)},
		                   {"store", Id(found.store)},
		                   {"worm", Id(found.worm)},
		                   {"position", Point(registry.Get<const Transform>(barrel).position)}};
		    if (registry.Valid(found.worm) && registry.AllOf<AppleBarrelWorm, Transform>(found.worm))
		    {
			    result["worm_position"] = Point(registry.Get<const Transform>(found.worm).position);
			    result["worm_place"] = registry.Get<const AppleBarrelWorm>(found.worm).place;
		    }
		    return result;
	    }));
	provider->Add(
	    Query("totems",
	          "The village centres' totems: their town centre and icon, the share they stand at, ease to and are "
	          "held at, how fast they move, whether the hand grips one, and their see-through second totem",
	          {}, ResultKind::List),
	    ServeRegistry([](const ecs::Registry& registry, const QueryContext& /*c*/) {
		    Json items = Json::array();
		    const auto gripped =
		        Locator::villageTotemSystem::has_value() ? Locator::villageTotemSystem::value().GetGripped() : std::nullopt;
		    registry.Each<const VillageTotem>([&items, &registry, gripped](entt::entity entity, const VillageTotem& totem) {
			    auto item = Listed(registry, entity);
			    item["town_centre"] = Id(totem.townCentre);
			    item["icon"] = Id(totem.icon);
			    item["rest_y"] = totem.restY;
			    item["share"] = totem.ease.share;
			    item["speed"] = totem.ease.speed;
			    item["target"] = totem.ease.target;
			    item["moving"] = totem.ease.moving;
			    item["held"] = totem.held;
			    item["gripped"] = gripped.has_value() && *gripped == entity;
			    item["ghost"] = Id(totem.ghost);
			    items.push_back(std::move(item));
		    });
		    return items;
	    }));
	provider->Add(
	    Query("artefacts",
	          "The artefacts: the town and worship site each last joined and whether it is among their artefacts now, its "
	          "owner and worth, its dance (started or not, its state, dancers and turns since it started), whether it "
	          "glows and shows its owner's symbol, and how many each player owns",
	          {}, ResultKind::Object),
	    ServeRegistry([](const ecs::Registry& registry, const QueryContext& /*c*/) {
		    Json items = Json::array();
		    registry.Each<const TownArtefact>([&items, &registry](entt::entity entity, const TownArtefact& artefact) {
			    auto item = Listed(registry, entity);
			    item["town"] = Id(artefact.town);
			    item["site"] = Id(artefact.site);
			    item["in_town"] = artefact.inTown;
			    item["in_site"] = artefact.inSite;
			    item["player"] = artefact.player.has_value() ? Json(static_cast<int>(*artefact.player)) : Json(nullptr);
			    item["value"] = artefact.value;
			    item["dance"] = Id(artefact.dance);
			    item["dancing"] = artefact.dancing;
			    item["dance_started"] = artefact.danceStarted;
			    if (const auto* dance = registry.Valid(artefact.dance) ? registry.TryGet<const Dance>(artefact.dance) : nullptr)
			    {
				    item["dance_type"] = static_cast<int>(dance->type);
				    item["dance_state"] = dance->state == Dance::State::Dancing ? "dancing" : "stopped";
				    item["dancers"] = dance->dancers;
				    item["dance_duration"] = dance->duration;
				    item["dance_start_turn"] = dance->startTurn;
			    }
			    item["effect"] = artefact.effect;
			    item["glows"] = ecs::artefact_rules::Glows(artefact.value);
			    item["symbol"] = artefact.symbol && ecs::artefact_rules::ShowsSymbol(artefact.value);
			    items.push_back(std::move(item));
		    });
		    Json counts = Json::object();
		    if (Locator::artefactSystem::has_value())
		    {
			    for (int p = 0; p < static_cast<int>(PlayerNames::_COUNT); ++p)
			    {
				    counts[std::to_string(p)] = Locator::artefactSystem::value().CountFor(static_cast<PlayerNames>(p));
			    }
		    }
		    return QueryResult::Value(Json {{"artefacts", std::move(items)}, {"count_by_player", std::move(counts)}});
	    }));
	provider->Add({.name = "artefact_worth",
	               .description = "Sets an artefact's worth as its dances would raise it: it starts glowing once worth more "
	                              "than 1, and shows its owner's symbol once worth more than 2",
	               .parameters = {IdParameter("The artefact's entity id"),
	                              {.name = "worth", .type = "number", .description = "Its new worth", .required = true},
	                              {.name = "town",
	                               .type = "integer",
	                               .description = "A town to make it an artefact of first, as if put there by the player",
	                               .required = false},
	                              {.name = "player",
	                               .type = "integer",
	                               .description = "The player who puts it in the town (0 by default)",
	                               .required = false}},
	               .kind = ResultKind::Object,
	               .needsNear = false,
	               .writes = true},
	              [](const QueryContext& context) {
		              const auto entity = EntityParam(context.params);
		              const auto worth = context.params.find("worth");
		              if (!entity.has_value() || worth == context.params.end() || !worth->is_number() ||
		                  !Locator::artefactSystem::has_value())
		              {
			              return QueryResult::Error("give an artefact's id and a worth");
		              }
		              // Made an artefact of a town first, when one is named, as if its player had put it there
		              if (const auto town = context.params.find("town"); town != context.params.end())
		              {
			              const auto townEntity = FromId(*town);
			              const auto player = context.params.value("player", 0);
			              if (!townEntity.has_value() || player < 0 || player >= static_cast<int>(PlayerNames::_COUNT))
			              {
				              return QueryResult::Error("give a town's id and a player number");
			              }
			              Locator::artefactSystem::value().AddToTown(*entity, *townEntity, static_cast<PlayerNames>(player));
		              }
		              if (!Locator::artefactSystem::value().SetWorth(*entity, worth->get<float>()))
		              {
			              return QueryResult::Error("it isn't an artefact");
		              }
		              return QueryResult::Value(Json {{"id", Id(*entity)}, {"worth", worth->get<float>()}});
	              });
	provider->Add(Query("action", "A villager's or animal's action states: top, final, previous, and turns in them",
	                    {IdParameter("The living thing's entity id")}),
	              ServeRegistry([](const ecs::Registry& registry, const QueryContext& context) {
		              const auto entity = EntityParam(context.params);
		              const auto* action = entity.has_value() ? registry.TryGet<const LivingAction>(*entity) : nullptr;
		              if (action == nullptr)
		              {
			              return QueryResult::Error("it has no living action");
		              }
		              Json result = {{"id", ToId(*entity)},
		                             {"top", action->states[0]},
		                             {"final", action->states[1]},
		                             {"previous", action->states[2]},
		                             {"turns_until_change", action->turnsUntilStateChange},
		                             {"turns_since_change", action->turnsSinceStateChange}};
		              if (Locator::livingActionSystem::has_value())
		              {
			              result["final_state"] =
			                  static_cast<int>(Locator::livingActionSystem::value().VillagerGetFinalState(*action));
		              }
		              return QueryResult::Value(std::move(result));
	              }));
	provider->Add(Query("animal",
	                    "An animal: its kind, state (and the game's number of the state it is in for itself), clip, flock "
	                    "and town, its kind's radius, where it moves to, whether "
	                    "a creature fears it, and a grazer's needs",
	                    {IdParameter("The animal's entity id")}),
	              Serve<Locator::animalSystem>(
	                  "the animals", [](const ecs::systems::AnimalSystemInterface& animals, const QueryContext& context) {
		                  const auto entity = EntityParam(context.params);
		                  const auto* registry = Registry();
		                  if (!entity.has_value() || registry == nullptr || !registry->AllOf<Animal>(*entity))
		                  {
			                  return QueryResult::Error("no animal with that id");
		                  }
		                  const auto& animal = registry->Get<const Animal>(*entity);
		                  Json result = {{"id", ToId(*entity)},
		                                 {"kind", static_cast<int>(animal.type)},
		                                 {"state", static_cast<int>(animal.state)},
		                                 {"final_game_state", animals.FinalGameStateOf(*entity).value_or(0)},
		                                 {"turns_in_state", animal.turnsInState},
		                                 {"birth_turn", animal.birthTurn},
		                                 {"clip", static_cast<int>(animal.animation)},
		                                 {"speed", animal.move.speed},
		                                 {"flock", Id(animal.flock)},
		                                 {"town", Id(animal.town)},
		                                 {"radius", animals.RadiusOf(*entity)},
		                                 {"movement", Point(animals.MovementOf(*entity))},
		                                 {"goal", Point(animals.GoalOf(*entity))},
		                                 {"frightening_to_creature", animals.IsFrighteningToCreature(*entity)},
		                                 {"player_can_pick_up", animals.CanPlayerPickUp(*entity)}};
		                  // A grazer's needs and meal
		                  if (const auto* grazer = registry->TryGet<const Grazer>(*entity))
		                  {
			                  result["grazer"] = {{"hunger", grazer->needs.hunger},
			                                      {"sleep", grazer->needs.sleep},
			                                      {"breed", grazer->needs.breed},
			                                      {"meals", grazer->meals}};
		                  }
		                  return QueryResult::Value(std::move(result));
	                  }));
	provider->Add(
	    Query("villages", "Whether it is dark enough for the village lights, and how many there are"),
	    Serve<Locator::villageLightSystem>(
	        "the village lights", [](const ecs::systems::VillageLightSystemInterface& lights, const QueryContext& /*c*/) {
		        const auto* registry = Registry();
		        return Json {{"dark", lights.IsDark()}, {"lights", registry != nullptr ? CountOf<VillageLight>(*registry) : 0}};
	        }));
	provider->Add(
	    Query("field", "A field's crop as it is drawn: its tint, height and sway", {IdParameter("The field's entity id")}),
	    Serve<Locator::fieldSystem>(
	        "the fields", [](const ecs::systems::FieldSystemInterface& fields, const QueryContext& context) {
		        const auto entity = EntityParam(context.params);
		        const auto* field = entity.has_value() ? Registry()->TryGet<const Field>(*entity) : nullptr;
		        if (field == nullptr)
		        {
			        return QueryResult::Error("no field with that id");
		        }
		        const auto look = fields.GetLook(*field);
		        if (!look.has_value())
		        {
			        return QueryResult::Value({{"id", ToId(*entity)}, {"crop", nullptr}});
		        }
		        return QueryResult::Value(
		            {{"id", ToId(*entity)}, {"tint", look->tint}, {"height", look->height}, {"sways", look->sways}});
	        }));
	provider->Add(Query("forests", "The land's forests: their wood and trees", {}, ResultKind::List),
	              Serve<Locator::forestSystem>(
	                  "the forests", [](const ecs::systems::ForestSystemInterface& forests, const QueryContext& /*c*/) {
		                  const auto* registry = Registry();
		                  Json items = Json::array();
		                  for (const auto forest : forests.LandForests())
		                  {
			                  auto item = registry != nullptr ? Listed(*registry, forest) : Json {{"id", ToId(forest)}};
			                  item["wood"] = forests.WoodOf(forest);
			                  item["trees"] = forests.TreesOf(forest, false).size();
			                  item["growing"] = forests.TreesOf(forest, true).size();
			                  items.push_back(std::move(item));
		                  }
		                  return items;
	                  }));
	provider->Add(Query("sharks", "The sharks: where each is this turn and was at its start, the way it faces, its clip "
	                              "and the wake's timer"),
	              Serve<Locator::sharkSystem>(
	                  "the sharks", [](const ecs::systems::SharkSystemInterface& sharks, const QueryContext& /*c*/) {
		                  Json items = Json::array();
		                  if (const auto* registry = Registry(); registry != nullptr)
		                  {
			                  registry->Each<const Shark>([&items, registry](entt::entity entity, const Shark& shark) {
				                  auto item = Listed(*registry, entity);
				                  item["at"] = Point(shark.position);
				                  item["turn_start"] = Point(shark.turnStart);
				                  item["heading"] = shark.heading;
				                  item["clip_place"] = shark.clipPlace;
				                  items.push_back(std::move(item));
			                  });
		                  }
		                  return Json {{"wake_timer", sharks.GetWakeTimer()}, {"sharks", std::move(items)}};
	                  }));
	provider->Add(Query("intro", "The opening's light falling onto the boy and the god's hand lifting him and setting him "
	                             "down: the stage, the light's state and head, the chasing camera, the hand's clip time and "
	                             "where it holds the boy"),
	              Serve<Locator::introSystem>(
	                  "the opening's light and hand", [](const ecs::systems::IntroSystemInterface& intro, const QueryContext&) {
		                  const auto state = intro.GetState();
		                  Json json {
		                      {"stage", state.stage},
		                      {"light", state.light.has_value() ? Json(static_cast<int32_t>(*state.light)) : Json(nullptr)},
		                      {"light_head", state.lightHead.has_value() ? Point(*state.lightHead) : Json(nullptr)},
		                      {"light_elapsed", state.lightElapsed},
		                      {"camera_chasing", state.cameraChasing},
		                      {"hand", state.hand == entt::null ? Json(nullptr) : Json(ToId(state.hand))},
		                      {"hand_clip_time", state.handClipTime},
		                      {"hand_playing", state.handPlaying},
		                      {"holding_boy", state.holdingBoy},
		                      {"grip", state.grip.has_value() ? Point(*state.grip) : Json(nullptr)},
		                      {"lift_finished", state.liftFinished}};
		                  if (const auto view = intro.GetCameraView(); view.has_value())
		                  {
			                  json["camera"] = {{"origin", Point(view->origin)}, {"focus", Point(view->focus)}};
		                  }
		                  return json;
	                  }));
	provider->Add(
	    Query("boat_launch", "The explorers' boat film: the stage, its clocks, where it plays, where the boat is drawn and "
	                         "the things drawn in it with their clips' places"),
	    Serve<Locator::boatLaunchSystem>(
	        "the explorers' boat", [](const ecs::systems::BoatLaunchSystemInterface& film, const QueryContext&) {
		        const auto state = film.GetState();
		        Json wakes = Json::array();
		        for (const auto phase : state.clocks.wakes)
		        {
			        wakes.push_back(phase);
		        }
		        Json parts = Json::array();
		        if (const auto* registry = Registry(); registry != nullptr)
		        {
			        registry->Each<const BoatFilmPart, const Transform>(
			            [&parts, registry](entt::entity entity, const BoatFilmPart& part, const Transform& transform) {
				            auto item = Listed(*registry, entity);
				            item["role"] = static_cast<int32_t>(part.role);
				            item["index"] = part.index;
				            item["at"] = Point(transform.position);
				            item["clip_time"] = part.clipTime;
				            parts.push_back(std::move(item));
			            });
		        }
		        Json json {{"playing", state.playing},
		                   {"stage", state.clocks.stage == ecs::boat_launch::Stage::Launch ? "launch" : "sailing"},
		                   {"elapsed", state.clocks.elapsed},
		                   {"boat_time", state.clocks.boatTime},
		                   {"pushing", state.clocks.pushing},
		                   {"wakes", std::move(wakes)},
		                   {"dock", Point(state.places.dock)},
		                   {"sailing_start", Point(state.places.sailingStart)},
		                   {"parts", std::move(parts)}};
		        if (state.boat.has_value())
		        {
			        json["boat"] = Point(glm::vec3((*state.boat)[3]));
		        }
		        return json;
	        }));
	provider->Add(Query("walk_paths", "The things walking the camera editor's tracks: the track, how far and up to where", {},
	                    ResultKind::List),
	              Serve<Locator::walkPathSystem>(
	                  "the walk paths", [](const ecs::systems::WalkPathSystemInterface& /*paths*/, const QueryContext&) {
		                  Json items = Json::array();
		                  if (const auto* registry = Registry(); registry != nullptr)
		                  {
			                  registry->Each<const WalkPath>([&items, registry](entt::entity entity, const WalkPath& path) {
				                  auto item = Listed(*registry, entity);
				                  item["track"] = path.number;
				                  item["forward"] = path.walk.forward;
				                  item["current"] = path.walk.current;
				                  item["to"] = path.walk.to;
				                  item["percentage"] = camera_track::Percentage(path.walk, *path.track);
				                  item["segment"] = path.walk.runner.GetSegment();
				                  items.push_back(std::move(item));
			                  });
		                  }
		                  return items;
	                  }));
	provider->Add(
	    Query("dances",
	          "The dances: what they are danced for, whether dancing, their speed and clock, and each group's shape, move "
	          "and dancers",
	          {}, ResultKind::List),
	    Serve<Locator::danceSystem>("the dances", [](const ecs::systems::DanceSystemInterface& /*dances*/,
	                                                 const QueryContext&) {
		    Json items = Json::array();
		    if (const auto* registry = Registry(); registry != nullptr)
		    {
			    registry->Each<const Dance>([&items, registry](entt::entity entity, const Dance& dance) {
				    auto item = Listed(*registry, entity);
				    item["type"] = static_cast<int>(dance.type);
				    item["owner"] = ToId(dance.owner);
				    item["dancing"] = dance.state == Dance::State::Dancing;
				    item["speed"] = dance.speed;
				    item["rate"] = dance.rate;
				    item["clock"] = dance.clock;
				    item["start_turn"] = dance.startTurn;
				    item["duration"] = dance.duration;
				    item["dancers"] = dance.dancers;
				    item["round"] = dance.groups.round;
				    Json groups = Json::array();
				    for (const auto& group : dance.groups.all)
				    {
					    Json dancers = Json::array();
					    for (const auto dancer : group.dancers)
					    {
						    dancers.push_back(ToId(dancer));
					    }
					    groups.push_back({{"name", group.name},
					                      {"limited", group.limited},
					                      {"quota", group.quota},
					                      {"weight", group.weight},
					                      {"sexes", group.sexes},
					                      {"shape", group.shape},
					                      {"radius", group.radius},
					                      {"offset", {group.offset.x, group.offset.y}},
					                      {"rotation", group.rotation},
					                      {"spin", group.spin},
					                      {"move", {group.move.action, group.move.first, group.move.second, group.move.end}},
					                      {"moved", group.moved},
					                      {"dancers", std::move(dancers)}});
				    }
				    item["groups"] = std::move(groups);
				    items.push_back(std::move(item));
			    });
		    }
		    return items;
	    }));
	provider->Add(Query("flocks",
	                    "The flocks, of animals and of the scripts' villagers: their place, their domain and flock distances, "
	                    "and their members from the first to the leader",
	                    {}, ResultKind::List),
	              ServeRegistry([](const ecs::Registry& registry, const QueryContext& /*c*/) {
		              Json items = Json::array();
		              registry.Each<const Flock>([&items](entt::entity entity, const Flock& flock) {
			              Json members = Json::array();
			              for (const auto member : flock.members)
			              {
				              members.push_back(ToId(member));
			              }
			              const auto place = map_coords::ToMetres(flock.place);
			              items.push_back({{"id", ToId(entity)},
			                               {"place", {place.x, place.y}},
			                               {"domain_radius", flock.domainRadius},
			                               {"flock_distance", flock.flockDistance},
			                               {"calm", flock.calm},
			                               {"members", std::move(members)}});
		              });
		              return items;
	              }));
	provider->Add(
	    Query("fireflies",
	          "How many fireflies there are and how many hide, fly out, hover and fly home; whether the next nightfall "
	          "tops them up; the trees and the things standing over several cells they can hide in; and what the last "
	          "frame did with those out: in view, near enough to be seen, drawn, and whether their picture was found"),
	    Serve<Locator::fireflySystem>(
	        "the fireflies", [](const ecs::systems::FireflySystemInterface& fireflies, const QueryContext& /*c*/) {
		        std::array<size_t, 4> states {};
		        size_t hidden = 0;
		        if (const auto* registry = Registry(); registry != nullptr)
		        {
			        for (const auto entity : fireflies.GetFireflies())
			        {
				        const auto& firefly = registry->Get<const Firefly>(entity);
				        ++states.at(static_cast<size_t>(firefly.state));
				        hidden += firefly.hidden ? 1 : 0;
			        }
		        }
		        const auto frame = fireflies.GetLastFrame();
		        return Json {{"fireflies", fireflies.GetFireflies().size()},
		                     {"resting", states.at(static_cast<size_t>(Firefly::State::Resting))},
		                     {"flying_out", states.at(static_cast<size_t>(Firefly::State::FlyingOut))},
		                     {"hovering", states.at(static_cast<size_t>(Firefly::State::Hovering))},
		                     {"flying_home", states.at(static_cast<size_t>(Firefly::State::FlyingHome))},
		                     {"hidden", hidden},
		                     {"top_up_due", fireflies.IsTopUpDue()},
		                     {"trees", fireflies.GetTreeCount()},
		                     {"fixed", fireflies.GetFixedCount()},
		                     {"last_frame",
		                      {{"out", frame.out},
		                       {"in_view", frame.inView},
		                       {"in_reach", frame.inReach},
		                       {"drawn", frame.drawn},
		                       {"picture_ready", frame.pictureReady}}}};
	        }));
	provider->Add(Query("fish_farms", "The fish farms: their town, fish left, fishermen and shoal", {}, ResultKind::List),
	              Serve<Locator::fishFarmSystem>(
	                  "the fish farms", [](const ecs::systems::FishFarmSystemInterface& farms, const QueryContext& /*c*/) {
		                  Json items = Json::array();
		                  const auto* registry = Registry();
		                  if (registry == nullptr)
		                  {
			                  return items;
		                  }
		                  registry->Each<const FishFarm>([&](entt::entity entity, const FishFarm& farm) {
			                  auto item = Listed(*registry, entity);
			                  item["town"] = farm.town == entt::null ? Json(nullptr) : Json(ToId(farm.town));
			                  item["fish_left"] = farms.FishLeft(entity);
			                  item["fishermen"] = farm.fishermen.size();
			                  item["shoal"] = farm.shoal.has_value() ? Point(farm.shoal->centre) : Json(nullptr);
			                  items.push_back(std::move(item));
		                  });
		                  return items;
	                  }));
	provider->Add(
	    Query("animated_statics",
	          "The scenery the scripts open and close (gates, the gate stone plinth, the piper's cave, the phone box): "
	          "its open word, place in its clip and resting place, whether it is in the land's draw list and was on "
	          "screen, its gate stones' value and the circles a creature's route goes round",
	          {}, ResultKind::List),
	    Serve<Locator::animatedStaticSystem>(
	        "the animated scenery", [](const ecs::systems::AnimatedStaticSystemInterface& scenery, const QueryContext& /*c*/) {
		        Json items = Json::array();
		        const auto* registry = Registry();
		        if (registry == nullptr)
		        {
			        return items;
		        }
		        registry->Each<const AnimatedStatic>([&](entt::entity entity, const AnimatedStatic& still) {
			        auto item = Listed(*registry, entity);
			        item["type"] = static_cast<int>(still.type);
			        item["open"] = still.openState;
			        if (const auto* pose = registry->TryGet<const AnimatedStaticPose>(entity); pose != nullptr)
			        {
				        item["place"] = pose->place;
				        item["resting_place"] = pose->restingPlace;
				        item["in_draw_list"] = pose->inDrawList;
				        item["on_screen"] = pose->onScreen;
				        item["stones_drawn"] = pose->stones.size();
			        }
			        item["stone_value"] = scenery.GateStoneValue(entity).value_or(0);
			        const auto circles = scenery.RouteCircles(entity);
			        item["route_circles"] = circles.has_value() ? Json(circles->size()) : Json(nullptr);
			        items.push_back(std::move(item));
		        });
		        return items;
	        }));
	provider->Add(Query("chimneys", "The chimneys smoking", {}, ResultKind::List),
	              ServeRegistry([](const ecs::Registry& registry, const QueryContext& /*c*/) {
		              Json items = Json::array();
		              registry.Each<const ChimneySmoke>([&items](entt::entity entity, const ChimneySmoke& smoke) {
			              items.push_back({{"id", ToId(entity)},
			                               {"position", Point(smoke.chimney)},
			                               {"state", static_cast<int>(smoke.state)},
			                               {"system", Locator::chimneySmokeSystem::has_value()}});
		              });
		              return items;
	              }));
	provider->Add(
	    Query("sound_tags", "The sounds tagged to things: sound, whether active, and its emitter", {}, ResultKind::List),
	    ServeRegistry([](const ecs::Registry& registry, const QueryContext& /*c*/) {
		    Json items = Json::array();
		    registry.Each<const SoundTag>([&items, &registry](entt::entity entity, const SoundTag& tag) {
			    auto item = Listed(registry, entity);
			    item["sound"] = tag.sound;
			    item["active"] = tag.active;
			    item["emitter"] = Id(tag.emitter);
			    items.push_back(std::move(item));
		    });
		    return items;
	    }));
	provider->Add(Query("rewards", "The rewards on their way to the land, lying on it, or opened", {}, ResultKind::List),
	              ServeRegistry([](const ecs::Registry& registry, const QueryContext& /*c*/) {
		              Json items = Json::array();
		              registry.Each<const Reward>([&items, &registry](entt::entity entity, const Reward& reward) {
			              auto item = Listed(registry, entity);
			              item["type"] = static_cast<int>(reward.type);
			              item["state"] = static_cast<int>(reward.state);
			              item["opened"] = reward.opened;
			              item["turns_left"] = reward.turnsLeft;
			              item["clip_place"] = reward.clipPlace;
			              item["landing_point"] = Point(reward.landingPoint);
			              items.push_back(std::move(item));
		              });
		              return items;
	              }));
	provider->Add(Query("pathfinding", "The walkers, and the circles of the things in their way they head for or go round"),
	              ServeRegistry([](const ecs::Registry& registry, const QueryContext& /*c*/) {
		              size_t walkers = 0;
		              registry.Each<const WallHug>([&walkers](entt::entity, const WallHug&) { ++walkers; });
		              Json heading = Json::array();
		              registry.Each<const WallHugObjectReference>(
		                  [&heading, &registry](entt::entity entity, const WallHugObjectReference& reference) {
			                  auto item = Listed(registry, entity);
			                  item["obstacle"] = Id(reference.entity);
			                  item["centre"] = Point(reference.centre);
			                  item["radius"] = reference.radius;
			                  item["steps_away"] = reference.stepsAway;
			                  heading.push_back(std::move(item));
		                  });
		              return Json {{"walkers", walkers},
		                           {"heading_for", std::move(heading)},
		                           {"system", Locator::pathfindingSystem::has_value()}};
	              }));
	provider->Add(Query("resource", "What a thing holds as a resource, and the store a pile belongs to",
	                    {IdParameter("The thing's entity id")}),
	              Serve<Locator::resourceStoreSystem>(
	                  "the stores", [](const ecs::systems::ResourceStoreSystemInterface& stores, const QueryContext& context) {
		                  const auto entity = EntityParam(context.params);
		                  if (!entity.has_value())
		                  {
			                  return QueryResult::Error("no entity with that id");
		                  }
		                  const auto resource = stores.ResourceOf(*entity);
		                  return QueryResult::Value({{"id", ToId(*entity)},
		                                             {"type", static_cast<int>(resource.type)},
		                                             {"amount", resource.amount},
		                                             {"poisoned", resource.poisoned},
		                                             {"store", Id(stores.StoreOf(*entity))}});
	                  }));
	provider->Add(
	    Query("building", "How much of a building is drawn while it is built or broken",
	          {IdParameter("The building's entity id")}),
	    Serve<Locator::buildingDamageSystem>(
	        "the building damage", [](const ecs::systems::BuildingDamageSystemInterface& damage, const QueryContext& context) {
		        const auto entity = EntityParam(context.params);
		        if (!entity.has_value())
		        {
			        return QueryResult::Error("no entity with that id");
		        }
		        return QueryResult::Value({{"id", ToId(*entity)},
		                                   {"partial_share", Optional(damage.PartialShare(*entity))},
		                                   {"draws_whole", damage.DrawsWhole(*entity)}});
	        }));
	provider->Add(
	    Query("town_desires", "What a town wants most, and how much it wants each thing",
	          {IdParameter("The town's entity id")}),
	    Serve<Locator::townDesireSystem>("the town desires", [](const ecs::systems::TownDesireSystemInterface& desires,
	                                                            const QueryContext& context) {
		    const auto entity = EntityParam(context.params);
		    if (!entity.has_value())
		    {
			    return QueryResult::Error("no entity with that id");
		    }
		    Json each = Json::object();
		    for (int i = 0; i <= static_cast<int>(TownDesireInfo::ForSleep); ++i)
		    {
			    const auto desire = static_cast<TownDesireInfo>(i);
			    each[std::to_string(i)] = desires.GetDesire(*entity, desire);
		    }
		    Json raw = Json::object();
		    Json doing = Json::object();
		    const auto* townDesire = Locator::entitiesRegistry::value().TryGet<const ecs::components::TownDesire>(*entity);
		    for (int i = 0; i <= static_cast<int>(TownDesireInfo::ForSleep); ++i)
		    {
			    raw[std::to_string(i)] = desires.GetRawDesire(*entity, static_cast<TownDesireInfo>(i));
			    if (townDesire != nullptr)
			    {
				    doing[std::to_string(i)] = townDesire->doingNow.at(static_cast<size_t>(i));
			    }
		    }
		    // The wood the town wants before it sends villagers for more: the targets grow past a number of abodes
		    Json wood = Json::object();
		    if (Locator::infoConstants::has_value())
		    {
			    const auto& town = Locator::infoConstants::value().town;
			    const auto& multipliers =
			        Locator::infoConstants::value().townDesire.at(static_cast<size_t>(TownDesireInfo::ForWood)).tribeMultiplier;
			    wood = {{"minimum_for_desire", town.minimumWoodForDesire},
			            {"maximum_for_desire", town.maximumWoodForDesire},
			            {"buildings_for_desired_wood", town.numOfBuildingsForDesiredWood},
			            {"tribe_multipliers", std::vector<float>(multipliers.begin(), multipliers.end())}};
		    }
		    return QueryResult::Value({{"id", ToId(*entity)},
		                               {"most_wanted", static_cast<int>(desires.GetMostWanted(*entity))},
		                               {"desires", std::move(each)},
		                               {"raw", std::move(raw)},
		                               {"doing_now", std::move(doing)},
		                               {"wood_targets", std::move(wood)}});
	    }));
	return provider;
}

// The players, their hands and their input

std::unique_ptr<ProviderInterface> PlayersProvider()
{
	auto provider = std::make_unique<FunctionProvider>("players");
	provider->Add(Query("new_game", "How the new game started: whether the tutorial and the creature training are "
	                                "skipped and the old creature kept, and the player profiles"),
	              [](const QueryContext& /*c*/) {
		              Json result = Json::object();
		              if (Locator::tutorialSkipSystem::has_value())
		              {
			              const auto& skip = Locator::tutorialSkipSystem::value().Get();
			              result["skip_tutorial"] = skip.skipTutorial;
			              result["skip_creature_training"] = skip.skipCreatureTraining;
			              result["keep_old_creature"] = skip.keepOldCreature;
			              result["start_with_cow"] = skip.startWithCow;
		              }
		              if (Locator::playerProfileSystem::has_value())
		              {
			              const auto& profiles = Locator::playerProfileSystem::value();
			              result["profiles"] = profiles.GetProfileCount();
			              result["profile_has_creature"] = profiles.CurrentProfileHasCreature();
		              }
		              return QueryResult::Value(std::move(result));
	              });
	provider->Add(Query("list", "The players: their entities, primary creatures and alignment", {}, ResultKind::List),
	              Serve<Locator::playerSystem>(
	                  "the players", [](const ecs::systems::PlayerSystemInterface& players, const QueryContext& /*c*/) {
		                  Json items = Json::array();
		                  const auto* registry = Registry();
		                  if (registry == nullptr)
		                  {
			                  return items;
		                  }
		                  // The players the land has, by their own components: the system is asked only of those it knows
		                  registry->Each<const Player>([&](entt::entity entity, const Player& player) {
			                  const auto name = player.name;
			                  Json item = {{"player", static_cast<int>(name)},
			                               {"id", Id(entity)},
			                               {"local", players.GetLocalPlayer() == name},
			                               {"computer", players.IsComputerPlayer(name)},
			                               {"primary_creature", Id(players.GetPrimaryCreature(name))}};
			                  if (Locator::alignmentSystem::has_value())
			                  {
				                  item["alignment"] = Locator::alignmentSystem::value().GetPlayerAlignment(name);
				                  item["pending_alignment"] = Locator::alignmentSystem::value().GetPendingAlignment(name);
			                  }
			                  items.push_back(std::move(item));
		                  });
		                  return items;
	                  }));
	provider->Add(Query("heartbeat", "Each player's heartbeat: danger, rate, beat and pulse", {}, ResultKind::List),
	              Serve<Locator::heartbeatSystem>(
	                  "the heartbeats", [](const ecs::systems::HeartbeatSystemInterface& hearts, const QueryContext& /*c*/) {
		                  Json items = Json::array();
		                  const auto* registry = Registry();
		                  if (registry == nullptr)
		                  {
			                  return items;
		                  }
		                  registry->Each<const Player>([&](const Player& player) {
			                  const auto state = hearts.StateOf(player.name);
			                  if (!state.has_value())
			                  {
				                  return;
			                  }
			                  items.push_back({{"player", static_cast<int>(player.name)},
			                                   {"stress", state->stress},
			                                   {"rate", state->rate},
			                                   {"beats_per_second", state->rate * ecs::heartbeat::k_BeatsPerRate},
			                                   {"phase", state->phase},
			                                   {"pulse", state->pulse},
			                                   {"believer_share", state->believerShare},
			                                   {"influence_share", state->influenceShare},
			                                   {"destruction_beats", state->destructionBeats}});
		                  });
		                  return items;
	                  }));
	provider->Add(
	    Query("hand", "This computer's hands: where they are, and what the hand holds or takes"),
	    Serve<Locator::handSystem>("the hands", [](const ecs::systems::HandSystemInterface& hands, const QueryContext& /*c*/) {
		    Json result = Json::object();
		    const auto entities = hands.GetPlayerHands();
		    const auto positions = hands.GetPlayerHandPositions();
		    result["left"] = {{"id", Id(entities[0])}, {"position", Optional(positions[0])}};
		    result["right"] = {{"id", Id(entities[1])}, {"position", Optional(positions[1])}};
		    if (Locator::handGrabSystem::has_value())
		    {
			    const auto& grab = Locator::handGrabSystem::value();
			    result["held"] = Id(grab.GetHeld());
			    result["busy"] = grab.IsBusy();
			    result["in_influence"] = grab.IsInInfluence();
			    result["cursor_raise"] = grab.GetCursorRaise();
		    }
		    return result;
	    }));
	provider->Add(
	    Query("pick", "What the cursor picks: the object, the point and the land under it, and the land behind the hand's "
	                  "middle, about which a press with nothing under the cursor reaches"),
	    Serve<Locator::pickingSystem>("the picking",
	                                  [](const ecs::systems::PickingSystemInterface& picking, const QueryContext& /*c*/) {
		                                  const auto& pick = picking.GetPick();
		                                  return Json {{"object", Id(pick.object)},
		                                               {"point", Optional(pick.point)},
		                                               {"land", Optional(pick.land)},
		                                               {"distance", pick.distance},
		                                               {"hover_object", Id(pick.hoverObject)},
		                                               {"hover_seconds", pick.hoverSeconds},
		                                               {"behind_hand", Optional(picking.GetBehindHand())}};
	                                  }));
	provider->Add(
	    Query("hand_trace",
	          "The player's hand frame by frame, for hand bugs: start keeps the last frames (frames, 600 by default) of every "
	          "part of the frame that put the hand somewhere, where it was placed, drawn and showed, on the screen too, and "
	          "what it did; read returns them (the last few with last) and the jumps among them: frames where the hand "
	          "moved on the screen more than threshold pixels (12) further than the cursor, came_back when it returned "
	          "within return_within frames (4); stop forgets them",
	          {{.name = "action", .type = "string", .description = "start, read (the default) or stop", .required = false},
	           {.name = "frames", .type = "integer", .description = "With start: how many frames to keep", .required = false},
	           {.name = "last", .type = "integer", .description = "With read: only the last frames", .required = false},
	           {.name = "threshold",
	            .type = "number",
	            .description = "With read: the pixels beyond the cursor's move that make a jump",
	            .required = false},
	           {.name = "return_within",
	            .type = "integer",
	            .description = "With read: the frames a jump may take to come back",
	            .required = false},
	           {.name = "jumps_only",
	            .type = "boolean",
	            .description = "With read: only the jumps, each with the frames around it",
	            .required = false}}),
	    [](const QueryContext& context) {
		    if (!Locator::handSystem::has_value() || Locator::handSystem::value().GetTrace() == nullptr)
		    {
			    return QueryResult::Error("the hands keep no trace");
		    }
		    auto& trace = *Locator::handSystem::value().GetTrace();
		    const auto action = context.params.value("action", std::string("read"));
		    if (action == "start")
		    {
			    const auto frames = context.params.value("frames", 600);
			    if (frames < 1 || frames > 100000)
			    {
				    return QueryResult::Error("frames is 1 to 100000");
			    }
			    trace.Enable(static_cast<size_t>(frames));
			    return QueryResult::Value(Json {{"tracing", true}, {"frames", frames}});
		    }
		    if (action == "stop")
		    {
			    trace.Disable();
			    return QueryResult::Value(Json {{"tracing", false}});
		    }
		    if (action != "read")
		    {
			    return QueryResult::Error("action is start, read or stop");
		    }
		    auto frames = trace.Frames();
		    const auto threshold = context.params.value("threshold", 12.0f);
		    const auto returnWithin = static_cast<size_t>(std::max(context.params.value("return_within", 4), 0));
		    const auto jumps = hand_trace::FindJumps(frames, threshold, returnWithin);
		    const auto vec = [](glm::vec3 v) { return Json {v.x, v.y, v.z}; };
		    const auto frameJson = [&vec](const hand_trace::Frame& frame) {
			    Json writes = Json::array();
			    for (const auto& write : frame.Writes())
			    {
				    writes.push_back({{"by", std::string(write.writer)}, {"at", vec(write.position)}});
			    }
			    return Json {{"frame", frame.frame},
			                 {"seconds", frame.seconds},
			                 {"cursor", {frame.cursor.x, frame.cursor.y}},
			                 {"placed", vec(frame.placed)},
			                 {"drawn", vec(frame.drawn)},
			                 {"visible", vec(frame.visible)},
			                 {"screen", frame.screen.has_value() ? Json {frame.screen->x, frame.screen->y} : Json(nullptr)},
			                 {"wrist", vec(frame.wrist)},
			                 {"wrist_screen", frame.wristScreen.has_value() ? Json {frame.wristScreen->x, frame.wristScreen->y}
			                                                                : Json(nullptr)},
			                 {"stage", frame.stage},
			                 {"busy", frame.busy},
			                 {"held", frame.held.has_value() ? Json(*frame.held) : Json(nullptr)},
			                 {"picked", frame.picked.has_value() ? Json(*frame.picked) : Json(nullptr)},
			                 {"cursor_land", frame.cursorLand.has_value() ? vec(*frame.cursorLand) : Json(nullptr)},
			                 {"rest_on_thing", frame.restOnThing.has_value() ? vec(*frame.restOnThing) : Json(nullptr)},
			                 {"held_at", frame.heldAt.has_value() ? vec(*frame.heldAt) : Json(nullptr)},
			                 {"writes", std::move(writes)},
			                 {"writes_lost", frame.writesLost}};
		    };
		    Json jumpItems = Json::array();
		    for (const auto& jump : jumps)
		    {
			    jumpItems.push_back({{"frame", jump.frame},
			                         {"pixels", jump.pixels},
			                         {"beyond_cursor", jump.beyondCursor},
			                         {"came_back", jump.cameBack},
			                         {"last_writer", std::string(jump.writer)}});
		    }
		    Json items = Json::array();
		    if (context.params.value("jumps_only", false))
		    {
			    // Each jump with the two frames before it and the few it may take to come back
			    std::vector<bool> keep(frames.size(), false);
			    for (const auto& jump : jumps)
			    {
				    const auto from = jump.index >= 2 ? jump.index - 2 : 0;
				    for (size_t i = from; i < frames.size() && i <= jump.index + returnWithin; ++i)
				    {
					    keep[i] = true;
				    }
			    }
			    for (size_t i = 0; i < frames.size(); ++i)
			    {
				    if (keep[i])
				    {
					    items.push_back(frameJson(frames[i]));
				    }
			    }
		    }
		    else
		    {
			    const auto last = static_cast<size_t>(std::max(context.params.value("last", 0), 0));
			    const size_t first = last > 0 && last < frames.size() ? frames.size() - last : 0;
			    for (size_t i = first; i < frames.size(); ++i)
			    {
				    items.push_back(frameJson(frames[i]));
			    }
		    }
		    return QueryResult::Value(Json {{"tracing", trace.Enabled()},
		                                    {"kept", frames.size()},
		                                    {"jumps", std::move(jumpItems)},
		                                    {"frames", std::move(items)}});
	    });
	provider->Add(Query("input", "The mouse and the hands as the game's actions see them"),
	              Serve<Locator::gameActionSystem>(
	                  "the game's actions", [](const input::GameActionInterface& actions, const QueryContext& /*c*/) {
		                  const auto mouse = actions.GetMousePosition();
		                  const auto hands = actions.GetHandPositions();
		                  return Json {{"mouse", {mouse.x, mouse.y}},
		                               {"wheel", actions.GetMouseWheelDelta()},
		                               {"hands", {Optional(hands[0]), Optional(hands[1])}},
		                               {"queued_presses", actions.HasQueuedPresses()},
		                               {"scripted_pointer", actions.GetScriptedPointer().has_value()},
		                               {"cursor_frozen", actions.IsCursorFrozen()}};
	                  }));
	provider->Add(Query("gestures", "The gesture being drawn and the last recognised"),
	              Serve<Locator::gestureSystem>(
	                  "the gestures", [](const ecs::systems::GestureSystemInterface& gestures, const QueryContext& /*c*/) {
		                  Json result = {{"drawing", gestures.IsDrawingPath()},
		                                 {"gesturing", gestures.IsGesturing()},
		                                 {"templates", gestures.GetTemplates().size()},
		                                 {"leash_picker_open", gestures.IsLeashPickerOpen()},
		                                 {"circle_seconds_left", gestures.GetCircleSecondsLeft()},
		                                 {"events_queue", Locator::gestureEvents::has_value()}};
		                  // The row of hints at the bottom right: each hinted gesture and its leash, and the row's fade
		                  Json hints = Json::array();
		                  for (const auto& hint : gestures.GetHints().hints)
		                  {
			                  hints.push_back({{"gesture", static_cast<uint32_t>(hint.gesture)},
			                                   {"leash", static_cast<int>(hint.leash)},
			                                   {"dot", hint.dot}});
		                  }
		                  result["hints"] = {{"alpha", gestures.GetHints().alpha}, {"shown", hints}};
		                  if (const auto last = gestures.GetLastRecognised(); last.has_value())
		                  {
			                  result["last_recognised"] = {{"number", last->number}, {"age", last->age}};
		                  }
		                  return result;
	                  }));
	return provider;
}

// Miracles, fire and what reacts to them

std::unique_ptr<ProviderInterface> MagicProvider()
{
	auto provider = std::make_unique<FunctionProvider>("magic");
	provider->Add(
	    Query("state", "The hand's miracle, the spells and dispensers there are, and the other miracles' "
	                   "systems: shields, tornadoes, explosions"),
	    Serve<Locator::magicSystem>("magic", [](const ecs::systems::MagicSystemInterface& magic, const QueryContext& /*c*/) {
		    Json result = {{"spells", magic.GetSpells().size()},
		                   {"dispensers", magic.GetDispensers().size()},
		                   {"hand_busy", magic.IsHandBusy()},
		                   {"held_seed", Id(magic.GetHeldSeed())},
		                   {"last_hand_result", static_cast<int>(magic.GetLastHandResult())},
		                   {"ignoring_influence", magic.IsIgnoringInfluence()}};
		    if (Locator::magicShieldSystem::has_value())
		    {
			    result["shields"] = Locator::magicShieldSystem::value().GetDomes(0.0f).size();
		    }
		    if (Locator::tornadoSystem::has_value())
		    {
			    result["carried_by_tornadoes"] = Locator::tornadoSystem::value().CarriedCount();
		    }
		    if (Locator::explosionSystem::has_value())
		    {
			    result["shaking"] = Locator::explosionSystem::value().IsShaking();
		    }
		    result["miracle_fx"] = Locator::miracleFxSystem::has_value();
		    return result;
	    }));
	provider->Add(
	    Query("spells", "The spells cast, nearest first when searched about a point", {}, ResultKind::List),
	    Serve<Locator::magicSystem>("magic", [](const ecs::systems::MagicSystemInterface& magic, const QueryContext& /*c*/) {
		    Json items = Json::array();
		    for (const auto& spell : magic.GetSpells())
		    {
			    items.push_back({{"id", Id(spell.entity)},
			                     {"type", static_cast<int>(spell.magicType)},
			                     {"player", static_cast<int>(spell.player)},
			                     {"position", Point(spell.position)},
			                     {"age", spell.age},
			                     {"duration", spell.duration},
			                     {"strength", spell.strength},
			                     {"closing", spell.closing}});
		    }
		    return items;
	    }));
	provider->Add(
	    Query("dispensers", "The miracle dispensers", {}, ResultKind::List),
	    Serve<Locator::magicSystem>("magic", [](const ecs::systems::MagicSystemInterface& magic, const QueryContext& /*c*/) {
		    Json items = Json::array();
		    for (const auto& dispenser : magic.GetDispensers())
		    {
			    items.push_back({{"id", Id(dispenser.entity)},
			                     {"type", static_cast<int>(dispenser.magicType)},
			                     {"position", Point(dispenser.position)},
			                     {"has_orb", dispenser.hasOrb},
			                     {"active", dispenser.active},
			                     {"tick", dispenser.tick},
			                     {"period", dispenser.period}});
		    }
		    return items;
	    }));
	provider->Add(
	    Query("teleport", "A player's teleport stones",
	          {{.name = "player", .type = "integer", .description = "The player's number, 0 by default", .required = false}},
	          ResultKind::List),
	    Serve<Locator::teleportSystem>(
	        "the teleports", [](const ecs::systems::TeleportSystemInterface& teleports, const QueryContext& context) {
		        const auto* registry = Registry();
		        Json items = Json::array();
		        for (const auto stone : teleports.GetStones(PlayerParam(context.params)))
		        {
			        items.push_back(registry != nullptr ? Listed(*registry, stone) : Json {{"id", ToId(stone)}});
		        }
		        return items;
	        }));
	provider->Add(Query("vortices",
	                    "The vortices between the lands: their kind, state, openness and how far they levelled the ground", {},
	                    ResultKind::List),
	              Serve<Locator::vortexSystem>(
	                  "the vortices", [](const ecs::systems::VortexSystemInterface& vortices, const QueryContext& /*c*/) {
		                  Json items = Json::array();
		                  if (auto* registry = Registry(); registry != nullptr)
		                  {
			                  registry->Each<const ecs::components::Vortex>(
			                      [&items, &vortices](entt::entity entity, const ecs::components::Vortex& vortex) {
				                      items.push_back({{"id", Id(entity)},
				                                       {"type", static_cast<int>(vortex.type)},
				                                       {"state", static_cast<int>(vortex.state)},
				                                       {"state_start_turn", vortex.stateStartTurn},
				                                       {"centre", Point(vortex.centre)},
				                                       {"openness", vortices.GetOpenness(entity)},
				                                       {"level_applied", vortex.levelApplied},
				                                       {"before_land_effect", vortex.beforeLandEffect},
				                                       {"after_land_effect", vortex.afterLandEffect},
				                                       {"object_mover_effect", vortex.objectMoverEffect},
				                                       {"light_map_effect", vortex.lightMapEffect}});
			                      });
		                  }
		                  return items;
	                  }));
	provider->Add(Query("reactions", "The reactions going on, nearest first when searched about a point", {}, ResultKind::List),
	              Serve<Locator::reactionSystem>(
	                  "the reactions", [](const ecs::systems::ReactionSystemInterface& reactions, const QueryContext& /*c*/) {
		                  Json items = Json::array();
		                  for (const auto& reaction : reactions.GetReactions())
		                  {
			                  items.push_back({{"id", reaction.id},
			                                   {"type", static_cast<int>(reaction.source.type)},
			                                   {"initiator", Id(reaction.source.initiator)},
			                                   {"player", static_cast<int>(reaction.source.player)},
			                                   {"position", Point(reaction.source.position)},
			                                   {"age", reaction.age},
			                                   {"reach", reaction.reach}});
		                  }
		                  return items;
	                  }));
	provider->Add(
	    Query("fires", "The fires burning, nearest first when searched about a point", {}, ResultKind::List),
	    Serve<Locator::fireSystem>("the fires", [](const ecs::systems::FireSystemInterface& fires, const QueryContext& /*c*/) {
		    Json items = Json::array();
		    for (const auto& fire : fires.GetFires())
		    {
			    items.push_back({{"id", Id(fire.object)},
			                     {"position", Point(fire.position)},
			                     {"temperature", fire.temperature},
			                     {"burning", fire.burning},
			                     {"charring", fire.charring},
			                     {"life", fire.life},
			                     {"root", Id(fire.root)},
			                     {"blaze_size", fire.blazeSize},
			                     {"firemen", fire.firemen}});
		    }
		    return items;
	    }));
	return provider;
}

// The temple

std::unique_ptr<ProviderInterface> TempleProvider()
{
	auto provider = std::make_unique<FunctionProvider>("temple");
	provider->Add(
	    Query("state", "The temples on the land: their owners, and whether they are being destroyed", {}, ResultKind::List),
	    ServeRegistry([](const ecs::Registry& registry, const QueryContext& /*c*/) {
		    Json items = Json::array();
		    registry.Each<const Temple>([&items, &registry](entt::entity entity, const Temple& temple) {
			    auto item = Listed(registry, entity);
			    item["owner"] = static_cast<int>(temple.owner);
			    item["destroying"] = temple.destroying;
			    item["last_hit_turn"] = temple.lastHitTurn;
			    item["has_exterior"] = registry.AllOf<TempleExterior>(entity);
			    items.push_back(std::move(item));
		    });
		    return items;
	    }));
	provider->Add(Query("interior", "The temple's inside: whether the player is in it, the room, the visits, and the "
	                                "worshippers walking about it (how many came in, and where those drawn are)"),
	              Serve<Locator::temple>("the temple", [](const TempleInteriorInterface& temple, const QueryContext& /*c*/) {
		              const auto transition = temple.GetTransitionRoom();
		              Json drawn = Json::array();
		              for (const auto& person : temple.GetPeople())
		              {
			              drawn.push_back(Point(glm::vec3(person.model[3])));
		              }
		              return Json {
		                  {"people_inside", temple.GetPeopleInside()},
		                  {"people_drawn", std::move(drawn)},
		                  {"active", temple.Active()},
		                  {"position", Point(temple.GetPosition())},
		                  {"room", static_cast<int>(temple.GetCurrentRoom())},
		                  {"transition_room", transition.has_value() ? Json(static_cast<int>(*transition)) : Json(nullptr)},
		                  {"visits", temple.GetVisits()}};
	              }));
	provider->Add(
	    Query("villagers",
	          "The villagers around the player's temple, as the Temple Villagers debug window shows them: its worship "
	          "sites and the places at them, the towns worshipping there, every villager worshipping, on its way or "
	          "within the radius with its state and walk, and those walking about inside the temple",
	          {{.name = "radius",
	            .type = "number",
	            .description = "Villagers this near the temple's middle, in metres, are listed too (60 if not given)",
	            .required = false},
	           {.name = "player", .type = "integer", .description = "The player, 0 if not given", .required = false}}),
	    [](const QueryContext& context) {
		    const auto player = static_cast<PlayerNames>(context.params.value("player", 0));
		    const auto survey = temple_villagers::Take(player, context.params.value("radius", 60.0f), false);
		    if (survey.temple == entt::null)
		    {
			    return QueryResult::Error("the player has no temple");
		    }
		    Json sites = Json::array();
		    for (const auto& site : survey.sites)
		    {
			    Json places = Json::object();
			    for (size_t i = 0; i < site.places.size(); ++i)
			    {
				    if (const auto& place = site.places.at(i); place.has_value())
				    {
					    places[std::string(temple_villagers::Name(static_cast<temple_villagers::SitePlace>(i)))] = {place->x,
					                                                                                                place->y};
				    }
			    }
			    Json worshippers = Json::array();
			    for (const auto villager : site.worshippers)
			    {
				    worshippers.push_back(Id(villager));
			    }
			    sites.push_back({{"id", Id(site.site)},
			                     {"tribe", static_cast<int>(site.tribe)},
			                     {"place", site.place},
			                     {"built", site.built},
			                     {"dancers", site.dancers},
			                     {"food_in_pot", site.foodInPot},
			                     {"worshippers", std::move(worshippers)},
			                     {"places", std::move(places)}});
		    }
		    Json towns = Json::array();
		    for (const auto& town : survey.towns)
		    {
			    towns.push_back({{"id", Id(town.town)},
			                     {"town_id", town.id},
			                     {"site", Id(town.site)},
			                     {"share", town.share},
			                     {"population", town.population},
			                     {"worshipping", town.worshipping},
			                     {"on_the_way", town.onTheWay},
			                     {"needed", town.needed}});
		    }
		    Json villagers = Json::array();
		    for (const auto& villager : survey.villagers)
		    {
			    Json item {{"id", Id(villager.villager)},
			               {"standing", std::string(temple_villagers::Name(villager.standing))},
			               {"tribe", static_cast<int>(villager.tribe)},
			               {"job", static_cast<int>(villager.number)},
			               {"child", villager.child},
			               {"age", villager.age},
			               {"life", villager.life},
			               {"town", Id(villager.town)},
			               {"site", Id(villager.site)},
			               {"state", static_cast<int>(villager.final)},
			               {"top_state", static_cast<int>(villager.top)},
			               {"turns_in_state", villager.turnsInState},
			               {"position", Point(villager.position)},
			               {"distance", villager.distance}};
			    if (villager.walk.has_value())
			    {
				    const auto& walk = *villager.walk;
				    Json walking {{"goal", {walk.goal.x, walk.goal.y}},
				                  {"move_state", std::string(walk.moveState)},
				                  {"speed", walk.speed}};
				    if (walk.stepGoal.has_value())
				    {
					    walking["step_goal"] = {walk.stepGoal->x, walk.stepGoal->y};
				    }
				    if (walk.circleCentre.has_value())
				    {
					    walking["circle"] = {{"centre", {walk.circleCentre->x, walk.circleCentre->y}},
					                         {"radius", walk.circleRadius}};
				    }
				    item["walk"] = std::move(walking);
			    }
			    villagers.push_back(std::move(item));
		    }
		    Json inside = Json::array();
		    for (const auto& walker : survey.inside.walkers)
		    {
			    inside.push_back({{"villager", Id(walker.villager)},
			                      {"position", Point(walker.position)},
			                      {"target", Point(walker.target)},
			                      {"walking", walker.walking},
			                      {"timer", walker.timer},
			                      {"pause", walker.pause}});
		    }
		    return QueryResult::Value({{"temple", Id(survey.temple)},
		                               {"temple_position", Point(survey.templePosition)},
		                               {"sites", std::move(sites)},
		                               {"towns", std::move(towns)},
		                               {"villagers", std::move(villagers)},
		                               {"in_temple", survey.inside.active},
		                               {"inside", std::move(inside)}});
	    });
	return provider;
}

// The camera's helpers, the editor and the cinematics

std::unique_ptr<ProviderInterface> ViewProvider()
{
	auto provider = std::make_unique<FunctionProvider>("view");
	provider->Add(Query("video", "The video playing: its frame, frame count and rate, its fade, the falling spell, and "
	                             "whether films and 16-bit colour are on"),
	              [](const QueryContext& /*c*/) {
		              Json result = Json::object();
		              if (Locator::videoSystem::has_value())
		              {
			              const auto& videos = Locator::videoSystem::value();
			              result["playing"] = videos.IsPlaying();
			              result["covers_screen"] = videos.CoversScreen();
			              result["hides_world"] = videos.HidesWorld();
			              result["films"] = videos.AreFilmsEnabled();
			              result["sixteen_bit_colour"] = videos.IsSixteenBitColour();
			              if (const auto status = videos.GetStatus())
			              {
				              result["frame"] = status->frame;
				              result["frame_count"] = status->frameCount;
				              result["fps"] = status->fps;
				              result["alpha"] = status->alpha;
				              result["falling_spell"] = status->fallingSpell;
				              result["falling_spell_state"] = status->fallingSpellState;
			              }
		              }
		              return QueryResult::Value(std::move(result));
	              });
	provider->Add(Query("state", "Whether a camera path holds the camera, the camera's help, and the knock readout"),
	              [](const QueryContext& /*c*/) {
		              Json result = Json::object();
		              if (Locator::cameraPathSystem::has_value())
		              {
			              auto& path = Locator::cameraPathSystem::value();
			              result["path"] = {
			                  {"pathing", path.IsPathing()}, {"paused", path.IsPaused()}, {"holds_camera", path.HoldsCamera()}};
		              }
		              if (Locator::cameraHelpSystem::has_value())
		              {
			              const auto& help = Locator::cameraHelpSystem::value().Get();
			              result["help"] = {{"features", help.features},
			                                {"auto_pitch", help.autoPitch},
			                                {"camera_keys", help.cameraKeys},
			                                {"hand_reach", help.handReach}};
		              }
		              if (Locator::abodeKnockSystem::has_value())
		              {
			              const auto& knock = Locator::abodeKnockSystem::value();
			              result["knock_readout"] = {{"town", Id(knock.GetReadoutTown())}, {"scale", knock.GetReadoutScale()}};
		              }
		              return QueryResult::Value(std::move(result));
	              });
	provider->Add(Query("bookmarks", "The camera's bookmarks", {}, ResultKind::List),
	              Serve<Locator::cameraBookmarkSystem>(
	                  "the bookmarks", [](const ecs::systems::CameraBookmarkSystemInterface& bookmarks, const QueryContext&) {
		                  const auto* registry = Registry();
		                  Json items = Json::array();
		                  for (size_t i = 0; i < bookmarks.GetBookmarks().size(); ++i)
		                  {
			                  const auto entity = bookmarks.GetBookmarks()[i];
			                  if (entity == entt::null || registry == nullptr || !registry->Valid(entity))
			                  {
				                  continue;
			                  }
			                  auto item = Listed(*registry, entity);
			                  item["slot"] = i;
			                  items.push_back(std::move(item));
		                  }
		                  return items;
	                  }));
	provider->Add(
	    Query("zones", "The camera zones the land's scripts set: their file, the fence the camera is kept inside, its "
	                   "height limits and the places it is kept out of, whether the camera is inside the fence, and the "
	                   "force field the camera lights up where it hits the fence (its strongest point, how often hit)"),
	    Serve<Locator::cameraZoneSystem>(
	        "the camera zones", [](const ecs::systems::CameraZoneSystemInterface& system, const QueryContext&) {
		        const auto& zones = system.GetZones();
		        Json fence = Json::array();
		        for (const auto& corner : zones.fence)
		        {
			        fence.push_back(Json::array({corner.x, corner.y, corner.z}));
		        }
		        Json exclusions = Json::array();
		        for (const auto& exclusion : zones.exclusions)
		        {
			        exclusions.push_back(
			            {{"position", Json::array({exclusion.position.x, exclusion.position.y, exclusion.position.z})},
			             {"radius", exclusion.radius},
			             {"height", exclusion.height},
			             {"kind", exclusion.kind == camera_zones::Exclusion::Kind::Cylinder ? "cylinder" : "dome"}});
		        }
		        Json result {{"file", system.GetFileName()},
		                     {"exclusions_on", zones.exclusionsOn},
		                     {"fence_on", zones.fenceOn},
		                     {"max_altitude", zones.useMaxAltitude ? Json(zones.maxAltitude) : Json(nullptr)},
		                     {"height_above_land", zones.useHeightAboveLand ? Json(zones.heightAboveLand) : Json(nullptr)},
		                     {"fence", std::move(fence)},
		                     {"exclusions", std::move(exclusions)}};
		        // The wall of light the camera lights up where it hits the fence
		        const auto& forceField = system.GetForceField();
		        const auto points = std::min(forceField.Points(), forceField.Strengths().size());
		        const auto strengths = std::span(forceField.Strengths()).first(points);
		        result["force_field"] = {
		            {"drawn", !forceField.Hidden()},
		            {"showing", !forceField.Hidden() && forceField.Showing()},
		            {"points", points},
		            {"strongest", strengths.empty() ? 0.0f : std::ranges::max(strengths)},
		            {"hits", system.GetFenceHits()},
		        };
		        if (Locator::camera::has_value())
		        {
			        const auto& camera = Locator::camera::value();
			        const auto origin = camera.GetOrigin();
			        result["camera_inside"] =
			            camera_zones::CrossFence(zones.fence, zones.fenceOn, origin, camera.GetFocus() - origin).inside;
			        if (Locator::terrainSystem::has_value())
			        {
				        result["height_limit"] = camera_zones::HeightLimit(
				            zones, Locator::terrainSystem::value().GetHeightAt(glm::vec2(origin.x, origin.z)));
			        }
		        }
		        return result;
	        }));
	provider->Add(
	    Query("cinematic",
	          "The fade, the wide screen, whether the interface is shown, and the words the scripts write on the screen"),
	    Serve<Locator::cinematicDirectorSystem>(
	        "the cinematics", [](const ecs::systems::CinematicDirectorSystemInterface& director, const QueryContext&) {
		        auto texts = Json::array();
		        for (const auto& text : director.GetTexts())
		        {
			        texts.push_back({{"words", gui::ToUtf8(text.words)},
			                         {"corner", {text.corner.x, text.corner.y}},
			                         {"extent", {text.extent.x, text.extent.y}},
			                         {"size", text.size},
			                         {"colour", {text.colour.r, text.colour.g, text.colour.b}},
			                         {"alpha", text.alpha},
			                         {"target", text.target},
			                         {"rate", text.rate}});
		        }
		        return Json {{"fade_finished", director.IsFadeFinished()},
		                     {"fade_colour", director.GetFadeColour()},
		                     {"wide_screen", director.IsWideScreenOn()},
		                     {"wide_screen_fraction", director.GetWideScreenFraction()},
		                     {"interface_active", director.IsInterfaceActive()},
		                     {"close_clipping", director.IsCloseClipping()},
		                     {"texts", texts}};
	        }));
	provider->Add(
	    Query("draw_list",
	          "The land's draw list: the things on the blocks in view within 700 units that are drawn, at most 2999, the "
	          "nearest blocks first; with an id, whether that thing is in it and drawn",
	          {{.name = "id", .type = "integer", .description = "A thing to look up in the list", .required = false}}),
	    Serve<Locator::objectDrawListSystem>(
	        "the land's draw list",
	        [](const ecs::systems::ObjectDrawListSystemInterface& drawList, const QueryContext& context) {
		        const auto summary = drawList.GetSummary();
		        Json result {{"active", summary.active},
		                     {"rebuilt", summary.frame.rebuilt},
		                     {"draw_all", summary.frame.drawAll},
		                     {"made_at_turn", Optional(summary.madeAt)},
		                     {"blocks_in_view", summary.blocksInView},
		                     {"blocks_walked", summary.blocksWalked},
		                     {"farthest_walked", summary.farthestWalked},
		                     {"things", summary.things},
		                     {"full", summary.full},
		                     {"rebuilds_asked", summary.rebuildsAsked}};
		        if (const auto entity = EntityParam(context.params); entity.has_value())
		        {
			        result["listed"] = drawList.Listed(*entity);
			        if (const auto* registry = Registry(); registry != nullptr)
			        {
				        result["drawn"] = drawList.Drawn(*registry, *entity);
			        }
		        }
		        return result;
	        }));
	provider->Add(
	    Query("script_control", "The tasks with the camera and the dialogue, the scripts' spoken lines and the "
	                            "villagers drawn in high detail"),
	    [](const QueryContext& /*c*/) {
		    Json result = Json::object();
		    if (Locator::scriptControlSystem::has_value())
		    {
			    result["camera_owner"] = Locator::scriptControlSystem::value().GetCameraOwner();
		    }
		    if (Locator::dialogueControlSystem::has_value())
		    {
			    result["dialogue_owner"] = Locator::dialogueControlSystem::value().GetOwner();
		    }
		    if (Locator::helpSpeechSystem::has_value())
		    {
			    const auto& speech = Locator::helpSpeechSystem::value();
			    result["speech"] = {{"spoken_texts", speech.GetSpokenTextCount()}, {"lines_playing", speech.GetLineCount()}};
		    }
		    if (Locator::highDetailSystem::has_value())
		    {
			    Json items = Json::array();
			    if (const auto* registry = Registry(); registry != nullptr)
			    {
				    registry->Each<const HighDetail>([&items, registry](entt::entity entity, const HighDetail& detail) {
					    auto item = Listed(*registry, entity);
					    item["detailed_model"] = detail.usualModel.has_value();
					    item["face"] = detail.face.has_value() ? static_cast<int>(*detail.face) : -1;
					    item["follow_intro_hand"] = detail.orders.followIntroHand;
					    item["turn_at_once"] = detail.orders.turnAtOnce;
					    if (detail.eyes.has_value())
					    {
						    Json eyes = {{"blinking", detail.eyes->blink.blinking},
						                 {"until_next_blink", detail.eyes->blink.untilNext},
						                 {"into_blink", detail.eyes->blink.into},
						                 {"roll", detail.eyes->roll},
						                 {"drawn", detail.drawnEyes.has_value()}};
						    if (detail.drawnEyes.has_value())
						    {
							    const auto& right = detail.drawnEyes->eyes[0].eyeball[3];
							    eyes["closed"] = detail.drawnEyes->closed;
							    eyes["right_eye"] = {right.x, right.y, right.z};
						    }
						    item["eyes"] = std::move(eyes);
					    }
					    items.push_back(std::move(item));
				    });
			    }
			    result["high_detail"] = std::move(items);
		    }
		    return QueryResult::Value(std::move(result));
	    });
	provider->Add(Query("editor", "The editor: open, its tool, its camera and what is selected"),
	              Serve<Locator::editorSystem>(
	                  "the editor", [](const ecs::systems::EditorSystemInterface& editor, const QueryContext& /*c*/) {
		                  return Json {{"open", editor.IsOpen()},
		                               {"tool", static_cast<int>(editor.GetTool())},
		                               {"camera_mode", static_cast<int>(editor.GetCameraMode())},
		                               {"stepping", editor.IsStepping()},
		                               {"selected", Id(editor.GetSelection().Get())}};
	                  }));
	return provider;
}

// The advisors

/// How an advisor's mesh, posed with its bones this frame, lies before the camera: its depths along the view, the
/// vertices the projection clips, and its triangles by the way they wind on the screen
struct AdvisorDepths
{
	float nearestVertex {0.0f};
	float furthestVertex {0.0f};
	size_t vertices {0};
	size_t closerThanNear {0};
	size_t clippedNear {0};
	size_t clippedFar {0};
	size_t clockwise {0};
	size_t counterClockwise {0};
	size_t mirroredBones {0};
};

std::optional<AdvisorDepths> MeasureAdvisorDepths(const help::spirits::AdvisorModel& model, std::span<const glm::mat4> bones,
                                                  const Camera& camera)
{
	l3d::L3DFile file;
	if (bones.empty() || file.Open(model.file.mesh) != l3d::L3DResult::Success)
	{
		return std::nullopt;
	}
	const auto view = camera.GetViewMatrix(Camera::Interpolation::Current);
	const auto viewProjection = camera.GetProjectionMatrix(Camera::Projection::ReversedZ) * view;
	AdvisorDepths depths {.nearestVertex = std::numeric_limits<float>::max(),
	                      .furthestVertex = std::numeric_limits<float>::lowest()};
	for (const auto& bone : bones)
	{
		depths.mirroredBones += glm::determinant(glm::mat3(bone)) < 0.0f ? 1 : 0;
	}
	for (uint32_t submesh = 0; submesh < file.GetSubmeshHeaders().size(); ++submesh)
	{
		const auto& header = file.GetSubmeshHeaders()[submesh];
		// The drawn level of detail only, as the renderer picks it
		if (header.flags.isPhysics || header.flags.status != 0 || (header.flags.lodMask & 1) != 1)
		{
			continue;
		}
		const auto vertices = file.GetVertexSpan(submesh);
		std::vector<glm::vec4> clip(vertices.size(), glm::vec4(0.0f));
		size_t vertex = 0;
		for (const auto& group : file.GetVertexGroupSpan(submesh))
		{
			const auto& bone = bones[std::min<size_t>(group.boneIndex, bones.size() - 1)];
			for (uint16_t i = 0; i < group.vertexCount && vertex < vertices.size(); ++i, ++vertex)
			{
				const auto& position = vertices[vertex].position;
				const glm::vec4 world = bone * glm::vec4(position.x, position.y, position.z, 1.0f);
				// The view is left handed: it looks down its positive z
				const float depth = (view * world).z;
				depths.nearestVertex = std::min(depths.nearestVertex, depth);
				depths.furthestVertex = std::max(depths.furthestVertex, depth);
				depths.closerThanNear += depth < camera.GetNearClip() ? 1 : 0;
				clip[vertex] = viewProjection * world;
				// Reversed, depth runs from w at the near plane to 0 at the far plane; the renderer clips outside it
				depths.clippedNear += clip[vertex].z > clip[vertex].w ? 1 : 0;
				depths.clippedFar += clip[vertex].z < 0.0f ? 1 : 0;
				++depths.vertices;
			}
		}
		const auto indices = file.GetIndexSpan(submesh);
		uint32_t firstVertex = 0;
		uint32_t firstIndex = 0;
		for (const auto& primitive : file.GetPrimitiveSpan(submesh))
		{
			for (uint32_t triangle = 0; triangle < primitive.numTriangles; ++triangle)
			{
				std::array<glm::vec2, 3> corner {};
				bool inside = true;
				for (uint32_t k = 0; k < 3; ++k)
				{
					const uint32_t index = firstIndex + 3 * triangle + k;
					const size_t at = index < indices.size() ? size_t {indices[index]} + firstVertex : clip.size();
					if (at >= clip.size() || clip[at].w <= 0.0f)
					{
						inside = false;
						break;
					}
					corner.at(k) = glm::vec2(clip[at]) / clip[at].w;
				}
				if (!inside)
				{
					continue;
				}
				const glm::vec2 a = corner[1] - corner[0];
				const glm::vec2 b = corner[2] - corner[0];
				const float area = a.x * b.y - a.y * b.x;
				depths.counterClockwise += area > 0.0f ? 1 : 0;
				depths.clockwise += area < 0.0f ? 1 : 0;
			}
			firstVertex += primitive.numVertices;
			firstIndex += primitive.numTriangles * 3;
		}
	}
	return depths.vertices > 0 ? std::optional(depths) : std::nullopt;
}

/// Where an advisor hovers and clings on the screen, and where its bones are drawn there, even off the screen
void AddAdvisorPlacement(Json& item, const ecs::systems::AdvisorSystemInterface& advisors,
                         const ecs::systems::AdvisorSystemInterface::Draw& draw, const Camera* camera)
{
	const auto& controller = advisors.GetController();
	const auto& dude = controller.Dude(draw.advisor);
	const auto& screen = controller.GetScreen();
	const auto unit = static_cast<float>(screen.HoverUnit());
	const auto hover = dude.Hover();
	item["state"] = dude.State();
	item["hover"] = {hover.x, hover.y};
	// Both of the hover's axes are measured in hover units from the screen's middle
	item["hover_pixel"] = {hover.x * unit + static_cast<float>(screen.HalfWidth()),
	                       hover.y * unit + static_cast<float>(screen.HalfHeight())};
	item["screen"] = {
	    {"width", screen.width}, {"height", screen.height}, {"hover_unit", screen.HoverUnit()}, {"reach", screen.Reach()}};
	item["cling_edge"] = static_cast<int>(dude.ClingEdge());
	item["cling_target"] = {dude.ClingTarget().x, dude.ClingTarget().y};
	if (camera == nullptr || draw.bones.empty())
	{
		return;
	}
	const auto viewProjection = camera->GetViewProjectionMatrix();
	const auto toPixel = [&viewProjection, &screen](const glm::mat4& bone) -> std::optional<glm::vec2> {
		const auto clip = viewProjection * bone[3];
		if (clip.w <= 0.0f)
		{
			return std::nullopt;
		}
		const auto ndc = glm::vec2(clip) / clip.w;
		return glm::vec2((ndc.x + 1.0f) * 0.5f * static_cast<float>(screen.width),
		                 (1.0f - ndc.y) * 0.5f * static_cast<float>(screen.height));
	};
	if (const auto root = toPixel(draw.bones.front()))
	{
		item["root_pixel"] = {root->x, root->y};
	}
	// How far up and down the screen its bones reach
	float top = std::numeric_limits<float>::max();
	float bottom = std::numeric_limits<float>::lowest();
	for (const auto& bone : draw.bones)
	{
		if (const auto pixel = toPixel(bone))
		{
			top = std::min(top, pixel->y);
			bottom = std::max(bottom, pixel->y);
		}
	}
	if (top <= bottom)
	{
		item["bones_pixel_y"] = {top, bottom};
	}
}

std::unique_ptr<ProviderInterface> AdvisorsProvider()
{
	auto provider = std::make_unique<FunctionProvider>("advisors");
	provider->Add(
	    Query("draws",
	          "What is drawn of the advisors this frame: near the screen or in the world, alpha, how far out in "
	          "the world, where they hover and cling on the screen (and the screen they move on: its size, the pixels in "
	          "a hover unit and how many hover units out its sides lie) and where their bones are drawn, and how "
	          "close their posed meshes come to the camera beside its near plane",
	          {}, ResultKind::List),
	    Serve<Locator::advisorSystem>("the advisors", [](const ecs::systems::AdvisorSystemInterface& advisors,
	                                                     const QueryContext& /*c*/) {
		    const auto* camera = Locator::camera::has_value() ? &Locator::camera::value() : nullptr;
		    Json items = Json::array();
		    for (const auto& draw : advisors.GetDraws())
		    {
			    Json item = {{"advisor", draw.advisor},  {"near_screen", draw.nearScreen}, {"alpha", draw.alpha},
			                 {"in_world", draw.inWorld}, {"bones", draw.bones.size()},     {"sprites", draw.sprites.size()}};
			    if (!draw.bones.empty())
			    {
				    item["root"] = Point(glm::vec3(draw.bones.front()[3]));
			    }
			    AddAdvisorPlacement(item, advisors, draw, camera);
			    const auto* model = advisors.GetModel(draw.advisor);
			    if (camera != nullptr && model != nullptr)
			    {
				    item["near_clip"] = camera->GetNearClip();
				    if (const auto depths = MeasureAdvisorDepths(*model, draw.bones, *camera))
				    {
					    item["nearest_vertex_depth"] = depths->nearestVertex;
					    item["furthest_vertex_depth"] = depths->furthestVertex;
					    item["vertices"] = depths->vertices;
					    item["vertices_closer_than_near"] = depths->closerThanNear;
					    item["vertices_clipped_near"] = depths->clippedNear;
					    item["vertices_clipped_far"] = depths->clippedFar;
					    item["triangles_clockwise"] = depths->clockwise;
					    item["triangles_counter_clockwise"] = depths->counterClockwise;
					    item["mirrored_bones"] = depths->mirroredBones;
				    }
			    }
			    items.push_back(std::move(item));
		    }
		    return items;
	    }));
	return provider;
}

// The creatures' other systems

std::unique_ptr<ProviderInterface> CreaturesProvider()
{
	auto provider = std::make_unique<FunctionProvider>("creatures");
	provider->Add(Query("systems", "The creature systems' own state: the creature followed or in the cave, the hand on a "
	                               "creature, the hair, voices, footprints, physiology and the mind kept between lands"),
	              [](const QueryContext& /*c*/) {
		              Json result = Json::object();
		              if (Locator::creatureModeSystem::has_value())
		              {
			              const auto& mode = Locator::creatureModeSystem::value();
			              result["mode"] = {{"active", mode.IsActive()}, {"creature", Id(mode.GetCreature())}};
		              }
		              if (Locator::creatureCaveSystem::has_value())
		              {
			              const auto& cave = Locator::creatureCaveSystem::value();
			              result["cave"] = {{"open", cave.IsOpen()},
			                                {"in_temple", cave.InTemple()},
			                                {"creature", Id(cave.GetCreature())},
			                                {"creature_shown", cave.IsCreatureShown()},
			                                {"copy", Id(cave.GetCaveCreature())}};
			              // The copy of the creature by the cave's fire, and what it is doing
			              const auto& registry = Locator::entitiesRegistry::value();
			              if (const auto copy = cave.GetCaveCreature(); copy.has_value() && registry.Valid(*copy))
			              {
				              const auto* self = registry.TryGet<const ecs::components::CreatureCopy>(*copy);
				              const auto* state = registry.TryGet<const ecs::components::CaveCreature>(*copy);
				              const auto* transform = registry.TryGet<const ecs::components::Transform>(*copy);
				              const auto* animation = registry.TryGet<const ecs::components::CreatureAnimation>(*copy);
				              if (self != nullptr && state != nullptr && transform != nullptr && animation != nullptr)
				              {
					              const auto& s = state->state;
					              result["cave"]["copy_state"] = {
					                  {"size", self->size},
					                  {"position", Point(transform->position)},
					                  {"heading", s.heading},
					                  {"rest", s.rest == cave_creature::Rest::Sleep ? "sleep" : "sit"},
					                  {"turning", s.turn.has_value()},
					                  {"wanted_heading", s.wantedHeading.has_value() ? Json(*s.wantedHeading) : Json(nullptr)},
					                  {"zoomed", s.zoomed},
					                  {"body_animation", creature_layers::IsPlaying(animation->body)
					                                         ? Json(creature_layers::CurrentAnimation(animation->body))
					                                         : Json(nullptr)},
					                  {"body_time_ms", animation->body.timeMs},
					                  {"mirrored", animation->body.mirrored},
					                  {"gesture", animation->gesture.animation.has_value() ? Json(*animation->gesture.animation)
					                                                                       : Json(nullptr)},
					                  {"looks_at", animation->lookAt.has_value() ? Point(*animation->lookAt) : Json(nullptr)},
					                  {"light", Point(state->light)},
					                  {"fidgets", s.fidgets},
					              };
				              }
			              }
		              }
		              if (Locator::creatureHandSystem::has_value())
		              {
			              const auto& hand = Locator::creatureHandSystem::value();
			              result["hand"] = {{"creature", Id(hand.GetCreature())},
			                                {"under_cursor", Id(hand.CreatureUnderCursor())},
			                                {"feedback_sum", hand.GetFeedbackSum()}};
		              }
		              if (Locator::creatureHairSystem::has_value())
		              {
			              result["hair_shown"] = Locator::creatureHairSystem::value().IsShown();
		              }
		              if (Locator::creatureAudioSystem::has_value())
		              {
			              result["voices_muted"] = Locator::creatureAudioSystem::value().IsMuted();
		              }
		              if (Locator::footprintSystem::has_value())
		              {
			              result["footprints"] = Locator::footprintSystem::value().GetPrints().size();
		              }
		              if (Locator::creaturePhysiologySystem::has_value())
		              {
			              result["physiology_time_scale"] = Locator::creaturePhysiologySystem::value().GetTimeScale();
			              result["fainting"] = Locator::creaturePhysiologySystem::value().IsFaintingEnabled();
		              }
		              if (Locator::creatureFightSystem::has_value())
		              {
			              result["players_fighter"] = Id(Locator::creatureFightSystem::value().PlayersFighter());
		              }
		              if (Locator::creatureCarryOverSystem::has_value())
		              {
			              result["mind_kept"] = Locator::creatureCarryOverSystem::value().Kept() != nullptr;
		              }
		              // The temples' pens: each creature's shrunk size there is its Creature component's penSize
		              result["pens"] = Locator::creaturePenSystem::has_value();
		              if (Locator::creatureFizzSystem::has_value())
		              {
			              const auto scroll = Locator::creatureFizzSystem::value().EyeStaticScroll();
			              result["eye_static_scroll"] = {scroll.x, scroll.y};
		              }
		              // The tattoo editor: whether it is open, on which creature, and the tattoo site under the pointer
		              if (Locator::tattooEditorSystem::has_value())
		              {
			              const auto& editor = Locator::tattooEditorSystem::value();
			              result["tattoo_editor"] = {{"open", editor.IsOpen()},
			                                         {"creature", Id(editor.GetCreature())},
			                                         {"hovered_site", Optional(editor.GetHoveredSite())}};
		              }
		              return QueryResult::Value(std::move(result));
	              });
	provider->Add(Query("status",
	                    "A creature's body systems: moving, what its hands do, its leash, its fight, its animation and "
	                    "its skin",
	                    {IdParameter("The creature's entity id")}),
	              ServeRegistry([](const ecs::Registry& registry, const QueryContext& context) {
		              const auto entity = EntityParam(context.params);
		              if (!entity.has_value() || !registry.AllOf<Creature>(*entity))
		              {
			              return QueryResult::Error("no creature with that id");
		              }
		              const auto creature = *entity;
		              Json result = {{"id", ToId(creature)}};
		              if (Locator::creatureLocomotionSystem::has_value())
		              {
			              result["moving"] = Locator::creatureLocomotionSystem::value().IsMoving(creature);
		              }
		              if (Locator::creatureObjectActionSystem::has_value())
		              {
			              const auto& actions = Locator::creatureObjectActionSystem::value();
			              result["object_action"] = {{"state", static_cast<int>(actions.GetState(creature))},
			                                         {"progress", Optional(actions.GetProgress(creature))},
			                                         {"held", Id(actions.GetHeld(creature))},
			                                         {"catching", actions.IsCatching(creature)}};
		              }
		              if (Locator::leashSystem::has_value())
		              {
			              const auto& leash = Locator::leashSystem::value();
			              result["leash"] = {{"leashed", leash.IsLeashed(creature)},
			                                 {"type", static_cast<int>(leash.TypeOf(creature))},
			                                 {"tied_to", Id(leash.TiedTo(creature))},
			                                 {"order_target", Optional(leash.OrderTarget(creature))}};
		              }
		              if (Locator::creatureFightSystem::has_value())
		              {
			              const auto& fights = Locator::creatureFightSystem::value();
			              result["fight"] = {{"fighting", fights.IsFighting(creature)},
			                                 {"opponent", Id(fights.OpponentOf(creature))},
			                                 {"knocked_out", fights.IsKnockedOut(creature)}};
		              }
		              if (const auto* animation = registry.TryGet<const CreatureAnimation>(creature); animation != nullptr)
		              {
			              result["animation_system"] = Locator::creatureAnimationSystem::has_value();
		              }
		              if (const auto* skin = registry.TryGet<const CreatureSkin>(creature); skin != nullptr)
		              {
			              result["skin_revision"] = skin->revision;
			              result["skin_system"] = Locator::creatureSkinSystem::has_value();
		              }
		              return QueryResult::Value(std::move(result));
	              }));
	provider->Add(Query("leash",
	                    "A creature's leash (the local player's creature by default): the leashes it knows, the one worn and "
	                    "picked, whether it works, is tied and to what, its rope (ends, lengths, tension) and whether the rope "
	                    "pulls the creature now, the order it carries out, the desire the leash makes rule it, and the local "
	                    "player's hand on the rope (away, over or holding)",
	                    {{.name = "id", .type = "integer", .description = "The creature's entity id"}}),
	              ServeRegistry([](const ecs::Registry& registry, const QueryContext& context) {
		              if (!Locator::leashSystem::has_value())
		              {
			              return QueryResult::Error("no leash system");
		              }
		              const auto& leash = Locator::leashSystem::value();
		              const auto local = Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer()
		                                                                    : PlayerNames::PLAYER_ONE;
		              auto entity = EntityParam(context.params);
		              if (!context.params.contains("id"))
		              {
			              entity = leash.PlayersCreature(local);
		              }
		              if (!entity.has_value() || !registry.AllOf<Creature>(*entity))
		              {
			              return QueryResult::Error("no creature with that id");
		              }
		              const auto creature = *entity;
		              Json result = {{"id", ToId(creature)},
		                             {"rope_hand", static_cast<int>(leash.RopeHand(local))},
		                             {"rope_hand_name",
		                              std::array {"away", "over", "holding"}.at(static_cast<size_t>(leash.RopeHand(local)))}};
		              const auto* leashes = registry.TryGet<const CreatureLeash>(creature);
		              if (leashes == nullptr)
		              {
			              result["leashed"] = false;
			              return QueryResult::Value(std::move(result));
		              }
		              result["known"] = {leashes->known.test(0), leashes->known.test(1), leashes->known.test(2)};
		              result["selected"] = static_cast<int>(leashes->selected);
		              result["leashed"] = leashes->worn.has_value();
		              result["control"] = static_cast<int>(leashes->control);
		              result["pull"] = leashes->pull;
		              result["order_target"] = Optional(leash.OrderTarget(creature));
		              if (Locator::creatureMindSystem::has_value())
		              {
			              const auto ruling = Locator::creatureMindSystem::value().DominantDesire(creature);
			              result["ruling_desire"] =
			                  ruling.has_value() ? Json(std::string(creature_desires::Name(*ruling))) : Json(nullptr);
		              }
		              if (leashes->worn.has_value())
		              {
			              const auto& worn = *leashes->worn;
			              const auto& rope = worn.rope;
			              result["type"] = static_cast<int>(worn.type);
			              result["works"] = worn.works;
			              result["tied_to"] = Id(worn.tiedTo);
			              result["held_rope"] = worn.held.has_value();
			              result["rope"] = {{"start", {rope.start.x, rope.start.y, rope.start.z}},
			                                {"end", {rope.end.x, rope.end.y, rope.end.z}},
			                                {"slack_length", rope.slackLength},
			                                {"max_length", rope.maxLength},
			                                {"tension", rope.tension}};
			              result["pulls"] =
			                  creature_leash::Pulls({.tension = rope.tension,
			                                         .works = worn.works,
			                                         .tied = worn.tiedTo.has_value(),
			                                         .orderInForce = leashes->order.has_value(),
			                                         .scriptControlled = registry.AllOf<ScriptControlled>(creature)});
		              }
		              return QueryResult::Value(std::move(result));
	              }));
	auto fight = Query("fight",
	                   "Starts a fight between two creatures as the scripts and the leash start one, the first making the "
	                   "arena; how it went and both creatures' fight state",
	                   {IdParameter("The creature that starts the fight"),
	                    {.name = "opponent", .type = "integer", .description = "The other creature", .required = true}});
	fight.writes = true;
	provider->Add(std::move(fight), [](const QueryContext& context) {
		const auto* registry = Registry();
		if (registry == nullptr || !Locator::creatureFightSystem::has_value())
		{
			return QueryResult::Error("there are no fights without a land");
		}
		const auto creature = EntityParam(context.params);
		const auto opponentId = context.params.find("opponent");
		const auto opponent = opponentId == context.params.end() ? std::nullopt : FromId(*opponentId);
		if (!creature.has_value() || !opponent.has_value() || !registry->Valid(*opponent) ||
		    !registry->AllOf<Creature>(*creature) || !registry->AllOf<Creature>(*opponent))
		{
			return QueryResult::Error("creatures.fight needs two creatures: id and opponent");
		}
		auto& fights = Locator::creatureFightSystem::value();
		using Start = ecs::systems::CreatureFightSystemInterface::StartResult;
		constexpr std::array<std::string_view, 6> k_Results {"started",  "no_opponent", "busy",
		                                                     "too_weak", "no_arena",    "refused"};
		const auto started = fights.StartFight(*creature, *opponent);
		const auto index = std::min(static_cast<size_t>(started), k_Results.size() - 1);
		if (started != Start::Started)
		{
			return QueryResult::Error("the fight didn't start: " + std::string(k_Results.at(index)));
		}
		return QueryResult::Value({
		    {"result", k_Results.at(index)},
		    {"fighting", {fights.IsFighting(*creature), fights.IsFighting(*opponent)}},
		    {"opponent_of_id", Id(fights.OpponentOf(*creature))},
		});
	});
	auto move = Query("move",
	                  "Sends a creature walking or running to a point by its own legs (turning and stepping off as it "
	                  "does), or turning on the spot to face it",
	                  {IdParameter("The creature"),
	                   {.name = "point", .type = "array", .description = "[x, z] on the land", .required = true},
	                   {.name = "pace",
	                    .type = "string",
	                    .description = "walk (the default), run, or face to turn on the spot towards the point",
	                    .required = false}});
	move.writes = true;
	provider->Add(std::move(move), [](const QueryContext& context) {
		const auto* registry = Registry();
		if (registry == nullptr || !Locator::creatureLocomotionSystem::has_value())
		{
			return QueryResult::Error("no creatures move without a land");
		}
		const auto creature = EntityParam(context.params);
		const auto point = context.params.find("point");
		if (!creature.has_value() || !registry->AllOf<Creature>(*creature) || point == context.params.end() ||
		    !point->is_array() || point->size() != 2 || !(*point)[0].is_number() || !(*point)[1].is_number())
		{
			return QueryResult::Error("creatures.move needs a creature's id and a point [x, z]");
		}
		const glm::vec2 target((*point)[0].get<float>(), (*point)[1].get<float>());
		const auto paceParam = context.params.find("pace");
		const auto pace =
		    paceParam != context.params.end() && paceParam->is_string() ? paceParam->get<std::string>() : std::string("walk");
		auto& locomotion = Locator::creatureLocomotionSystem::value();
		if (pace == "face")
		{
			return QueryResult::Value({{"result", locomotion.TurnToFace(*creature, target) ? "started" : "refused"}});
		}
		if (pace != "walk" && pace != "run")
		{
			return QueryResult::Error("pace is walk, run or face");
		}
		using Locomotion = ecs::systems::CreatureLocomotionSystemInterface;
		constexpr std::array<std::string_view, 3> k_Results {"invalid_destination", "busy", "started"};
		const auto result =
		    locomotion.MoveTo(*creature, target, pace == "run" ? Locomotion::Pace::Run : Locomotion::Pace::Walk, 0.0f, 1.0f);
		return QueryResult::Value({{"result", k_Results.at(static_cast<size_t>(result))}});
	});
	provider->Add(Query("route",
	                    "A creature's walk in full: where it is going, the state of its walk, the route it follows (its legs, "
	                    "straight or round a circle, and points along it) and the circles it walks round (each grown by "
	                    "its own reach)",
	                    {IdParameter("The creature")}),
	              [](const QueryContext& context) {
		              const auto* registry = Registry();
		              const auto creature = EntityParam(context.params);
		              if (registry == nullptr || !creature.has_value())
		              {
			              return QueryResult::Error("creatures.route needs a creature's id");
		              }
		              const auto* locomotion = registry->TryGet<const CreatureLocomotion>(*creature);
		              if (locomotion == nullptr)
		              {
			              return QueryResult::Error("that isn't a creature that walks");
		              }
		              const auto& route = locomotion->follower.GetRoute();
		              Json points = Json::array();
		              for (const auto& point : route.Points(glm::radians(5.0f)))
		              {
			              points.push_back({point.x, point.y});
		              }
		              Json legs = Json::array();
		              for (const auto& leg : route.legs)
		              {
			              Json item {{"from", {leg.from.x, leg.from.y}}, {"to", {leg.to.x, leg.to.y}}, {"arc", leg.arc}};
			              if (leg.round.has_value())
			              {
				              item["centre"] = {leg.round->centre.x, leg.round->centre.y};
				              item["radius"] = leg.round->radius;
				              item["clockwise"] = leg.roundWay == creature_route::Way::Clockwise;
			              }
			              legs.push_back(std::move(item));
		              }
		              Json circles = Json::array();
		              for (const auto& obstacle : locomotion->follower.GetObstacles().All())
		              {
			              if (obstacle.active)
			              {
				              circles.push_back({{"centre", {obstacle.circle.centre.x, obstacle.circle.centre.y}},
				                                 {"radius", obstacle.circle.radius},
				                                 {"thing", Optional(obstacle.thing)}});
			              }
		              }
		              return QueryResult::Value({{"destination", Optional(locomotion->destination)},
		                                         {"motion", static_cast<int>(locomotion->motion)},
		                                         {"radius", locomotion->radius},
		                                         {"failed", locomotion->failed},
		                                         {"state", static_cast<int>(locomotion->follower.GetState())},
		                                         {"segment", route.segment},
		                                         {"legs", std::move(legs)},
		                                         {"points", std::move(points)},
		                                         {"circles", std::move(circles)}});
	              });
	return provider;
}

// The scripts

std::unique_ptr<ProviderInterface> ScriptProvider(ScriptTargetInterface& scripts)
{
	auto provider = std::make_unique<FunctionProvider>("script");
	provider->Add(Query("vm", "The script machine: its scripts, tasks, globals and the task running"),
	              Serve<Locator::vm>("the script machine", [](const lhvm::LHVM& vm, const QueryContext& /*c*/) {
		              return Json {{"scripts", vm.GetScripts().size()},
		                           {"tasks", vm.GetTasks().size()},
		                           {"variables", vm.GetVariables().size()},
		                           {"instructions", vm.GetInstructions().size()},
		                           {"current_task", vm.GetCurrentTaskNumber()}};
	              }));
	provider->Add(
	    Query("tasks",
	          "The script tasks running: their scripts, where they are, whether they sleep and how many values are on their "
	          "stack",
	          {}, ResultKind::List),
	    Serve<Locator::vm>("the script machine", [](const lhvm::LHVM& vm, const QueryContext& /*c*/) {
		    Json items = Json::array();
		    for (const auto& [number, task] : vm.GetTasks())
		    {
			    items.push_back({{"id", number},
			                     {"name", task.name},
			                     {"script", task.scriptId},
			                     {"address", task.instructionAddress},
			                     {"sleeping", task.sleeping},
			                     {"waiting_for", task.waitingTaskId},
			                     {"stack", task.stack.count},
			                     {"held", vm.IsTaskHeld(number)}});
		    }
		    return items;
	    }));
	provider->Add(
	    Query("natives", "The script natives called that openblack hasn't written, most called first", {}, ResultKind::List),
	    Serve<Locator::chlapi>("the script natives", [](const chlapi::CHLApi& api, const QueryContext& /*c*/) {
		    auto entries = api.GetStubCalls().Entries();
		    std::ranges::stable_sort(entries, [](const auto& a, const auto& b) { return a.calls > b.calls; });
		    Json items = Json::array();
		    for (const auto& entry : entries)
		    {
			    items.push_back({{"native", entry.native},
			                     {"detail", entry.detail.has_value() ? Json(*entry.detail) : Json(nullptr)},
			                     {"calls", entry.calls}});
		    }
		    return items;
	    }));
	provider->Add(
	    Query("objects",
	          "How many things scripts hold and control, and the scripts' object table: places taken (of 511), times it "
	          "was full, and its places by kind (taken, referenced, made by a script, object gone); places: true lists "
	          "every taken place",
	          {{.name = "places", .type = "boolean", .description = "List every taken place", .required = false}}),
	    ServeRegistry([](const ecs::Registry& registry, const QueryContext& context) {
		    Json result {{"in_script", CountOf<InScript>(registry)},
		                 {"script_controlled", CountOf<ScriptControlled>(registry)},
		                 {"system", Locator::scriptObjects::has_value()}};
		    if (!Locator::scriptObjects::has_value())
		    {
			    return result;
		    }
		    const auto& scripts = Locator::scriptObjects::value();
		    const auto places = scripts.Places();
		    const bool listed = context.params.is_object() && context.params.value("places", false);
		    Json byKind = Json::object();
		    Json list = Json::array();
		    for (const auto& place : places)
		    {
			    const bool gone = place.object == entt::null || !registry.Valid(place.object);
			    const auto kind = gone ? std::string("gone") : Describe(registry, place.object, Info()).kind;
			    auto& counts = byKind[kind];
			    if (counts.is_null())
			    {
				    counts = {{"places", 0}, {"referenced", 0}, {"made_by_script", 0}};
			    }
			    counts["places"] = counts["places"].get<int>() + 1;
			    counts["referenced"] = counts["referenced"].get<int>() + (place.references != 0 ? 1 : 0);
			    counts["made_by_script"] = counts["made_by_script"].get<int>() + (place.createdByScript ? 1 : 0);
			    if (listed)
			    {
				    list.push_back({{"place", place.place},
				                    {"id", gone ? Json(nullptr) : Json(ToId(place.object))},
				                    {"kind", kind},
				                    {"references", place.references},
				                    {"made_by_script", place.createdByScript}});
			    }
		    }
		    result["places_taken"] = places.size();
		    result["times_full"] = scripts.TimesFull();
		    result["by_kind"] = std::move(byKind);
		    if (listed)
		    {
			    result["places"] = std::move(list);
		    }
		    return result;
	    }));
	provider->Add(
	    Query("highlights",
	          "The scrolls and signs scripts put up: kind, challenge or text, started, height, where drawn; the beat and "
	          "the switch that shows the scrolls",
	          {}, ResultKind::List),
	    Serve<Locator::scriptHighlightSystem>(
	        "the script highlights",
	        [](const ecs::systems::ScriptHighlightSystemInterface& highlights, const QueryContext& /*c*/) {
		        Json items = Json::array();
		        const auto* registry = Registry();
		        if (registry == nullptr)
		        {
			        return items;
		        }
		        const bool scrollsDrawn = !Locator::chlapi::has_value() || Locator::chlapi::value().IsHighlightDrawOn();
		        const auto& pulse = highlights.GetPulse();
		        registry->Each<const ScriptHighlight>([&](entt::entity entity, const ScriptHighlight& highlight) {
			        auto item = Listed(*registry, entity);
			        item["kind"] = static_cast<int>(highlight.kind);
			        item["script_id"] = highlight.scriptId;
			        item["category"] = highlight.category;
			        item["active"] = highlight.active;
			        item["draw_height"] = highlight.drawHeight.has_value() ? Json(*highlight.drawHeight) : Json(nullptr);
			        item["height_above"] = highlight.heightAbove;
			        item["y_angle"] = highlight.yAngle;
			        item["centre"] = Point(highlight.centre);
			        item["radius"] = highlight.radius;
			        item["glints"] = highlight.glints;
			        item["active_effect"] = highlight.activeEffect;
			        item["hidden"] = registry->AllOf<HiddenByState>(entity);
			        item["scrolls_drawn"] = scrollsDrawn;
			        item["pulse"] = pulse.level;
			        items.push_back(std::move(item));
		        });
		        return items;
	        }));
	AddScriptControls(*provider, scripts);
	return provider;
}

/// The physics' bodies as the inspector reports them
std::optional<std::vector<BodyInfo>> Bodies()
{
	if (!Locator::dynamicsSystem::has_value())
	{
		return std::nullopt;
	}
	std::vector<BodyInfo> bodies;
	Locator::dynamicsSystem::value().ForEachEntry([&bodies](const ecs::PhysicsEntry& entry) {
		BodyInfo info {.entity = entry.entity,
		               .kind = std::string(KindName(entry.kind)),
		               .flags = entry.flags,
		               .player = entry.player.has_value() ? std::optional<int>(static_cast<int>(*entry.player)) : std::nullopt,
		               .thrower = entry.thrower,
		               .impact = entry.impact};
		if (entry.body != nullptr)
		{
			const auto& body = *entry.body;
			info.centre = body.Centre();
			info.velocity = body.velocity;
			info.speed = body.Speed();
			info.mass = body.Mass();
			info.radius = body.Radius();
			info.resting = body.resting;
			info.inWater = body.inWater;
			info.contacts = body.Contacts();
		}
		bodies.push_back(std::move(info));
	});
	return bodies;
}

// The help: the advisors and the scripts' dialogue

std::unique_ptr<ProviderInterface> HelpProvider()
{
	auto provider = std::make_unique<FunctionProvider>("help");
	provider->Add(
	    Query("advisors",
	          "The good and the evil advisor: what each does, where it hovers, whether it is talking and its mouth's lip sync",
	          {}, ResultKind::List),
	    Serve<Locator::advisorSystem>(
	        "the advisors", [](const ecs::systems::AdvisorSystemInterface& advisors, const QueryContext& /*c*/) -> Json {
		        if (!advisors.IsLoaded())
		        {
			        return Json::array();
		        }
		        const auto& controller = advisors.GetController();
		        const auto* voices =
		            Locator::helpTextSystem::has_value() ? &Locator::helpTextSystem::value().GetVoices() : nullptr;
		        Json items = Json::array();
		        for (int dude = 0; dude < 2; ++dude)
		        {
			        const auto& spirit = controller.Dude(dude);
			        // The mouth: the lip sync's three weights and the mouth clips posed this frame (clip, ms)
			        Json shapes = Json::array();
			        for (const auto& layer : spirit.Layers())
			        {
				        if (layer.kind == help::spirits::AnimLayer::Kind::Add && layer.clip >= help::spirits::anim::k_VowelE &&
				            layer.clip < help::spirits::anim::k_VowelE + 3)
				        {
					        shapes.push_back({{"clip", layer.clip}, {"ms", layer.milliseconds}});
				        }
			        }
			        Json weights = Json::array();
			        if (voices != nullptr)
			        {
				        for (const float w : voices->GetLipSyncKey(dude).weights)
				        {
					        // Not a number after a window of pure silence, as in the game
					        weights.push_back(std::isfinite(w) ? Json(w) : Json(nullptr));
				        }
			        }
			        items.push_back({{"advisor", dude == 0 ? "good" : "evil"},
			                         {"mouth", {{"weights", weights}, {"shapes", shapes}}},
			                         {"tags_left", spirit.TagsLeft()},
			                         {"control_state", static_cast<int>(controller.State(dude))},
			                         {"state", spirit.State()},
			                         {"hover", Point(spirit.Hover())},
			                         {"position", Point(spirit.Position())},
			                         {"alpha", spirit.Alpha()},
			                         {"playing_anim", spirit.IsPlayingAnim()},
			                         {"given_line", voices != nullptr && voices->IsActive(dude)},
			                         {"speaking", voices != nullptr && voices->GetSpeaker() == dude}});
		        }
		        return items;
	        }));
	provider->Add(Query("dialogue",
	                    "The scripts' dialogue: the text shown, whether it is read or waits for a click, who holds the "
	                    "dialogue and the line being said"),
	              Serve<Locator::helpTextSystem>(
	                  "the dialogue", [](const ecs::systems::HelpTextSystemInterface& help, const QueryContext& /*c*/) -> Json {
		                  const auto& voices = help.GetVoices();
		                  Json result {{"started", help.IsStarted()},
		                               {"text_read", help.IsTextRead()},
		                               {"speaker", voices.GetSpeaker()},
		                               {"sentence", voices.GetSentence()},
		                               {"owner", Locator::dialogueControlSystem::has_value()
		                                             ? Json(Locator::dialogueControlSystem::value().GetOwner())
		                                             : Json(nullptr)},
		                               {"speech_banks", Locator::helpSpeechSystem::has_value()
		                                                    ? Json(Locator::helpSpeechSystem::value().GetTable().GetBankCount(
		                                                          audio::SpeechBank::HelpSprites))
		                                                    : Json(nullptr)}};
		                  if (const auto* dialogue = help.GetDialogue(); dialogue != nullptr)
		                  {
			                  result["text"] = dialogue->GetCurrentText();
			                  result["narrator"] = help.GetNarrator(dialogue->GetCurrentText());
			                  result["waiting_for_click"] = dialogue->IsWaitingForClick();
			                  result["end_turn"] = dialogue->GetEndTurn();
			                  result["end_ms"] = dialogue->GetEndMs();
			                  result["drawn"] = dialogue->IsDrawn();
			                  result["box_shown"] = dialogue->GetDisplay().IsBoxShown();
		                  }
		                  return result;
	                  }));
	provider->Add(
	    Query("profile",
	          "The help's count of what the player has done: its clock, and each event's total, seconds since "
	          "and rate, for the events that have happened",
	          {}, ResultKind::List),
	    Serve<Locator::helpProfileSystem>(
	        "the help profile", [](const ecs::systems::HelpProfileSystemInterface& system, const QueryContext& /*c*/) -> Json {
		        const auto& profile = system.Get();
		        Json items = Json::array();
		        for (uint32_t event = 0; event < help::profile::k_EventCount; ++event)
		        {
			        // A copy: reading the times may tidy them, and a query changes nothing
			        auto count = profile.Count(event);
			        if (count.Total() == 0)
			        {
				        continue;
			        }
			        items.push_back({{"event", event},
			                         {"clock", profile.Clock()},
			                         {"total", count.Total()},
			                         {"seconds_since", count.SecondsSince(profile.Clock())},
			                         {"per_second", count.PerSecond(profile.Clock())},
			                         {"smoothed_rate", count.SmoothedRate()}});
		        }
		        return items;
	        }));
	provider->Add(Query("creature",
	                    "The advisor's messages about the player's creature: whether they are on, the ones told lately "
	                    "(kind, parameter, text, turn), and the creature's waiting ones and prompts",
	                    {}, ResultKind::Object),
	              Serve<Locator::creatureHelpSystem>(
	                  "the creature help",
	                  [](const ecs::systems::CreatureHelpSystemInterface& system, const QueryContext& /*c*/) -> Json {
		                  Json told = Json::array();
		                  for (const auto& message : system.GetTold())
		                  {
			                  told.push_back({{"creature", entt::to_integral(message.creature)},
			                                  {"kind", static_cast<int>(message.kind)},
			                                  {"parameter", message.parameter},
			                                  {"text", message.text.has_value() ? Json(*message.text) : Json(nullptr)},
			                                  {"turn", message.turn}});
		                  }
		                  Json creatures = Json::array();
		                  const auto& registry = Locator::entitiesRegistry::value();
		                  registry.Each<const CreatureHelpMemory>(
		                      [&creatures](entt::entity entity, const CreatureHelpMemory& help) {
			                      Json waiting = Json::array();
			                      for (size_t kind = 0; kind < help.memory.entries.size(); ++kind)
			                      {
				                      for (const auto& entry : help.memory.entries.at(kind))
				                      {
					                      if (entry.waiting)
					                      {
						                      waiting.push_back({{"kind", kind}, {"parameter", entry.parameter}});
					                      }
				                      }
			                      }
			                      creatures.push_back({{"creature", entt::to_integral(entity)},
			                                           {"waiting", waiting},
			                                           {"hand_over", help.memory.handOver},
			                                           {"body_problem_turn", help.memory.physicalProblemTurn},
			                                           {"tutorial_turn", help.memory.tutorialTurn},
			                                           {"tutorial_done", help.memory.tutorialDone}});
		                      });
		                  return Json {{"on", system.IsHelpOn()}, {"told", told}, {"creatures", creatures}};
	                  }));
	provider->Add(
	    Query("messages",
	          "The help the game starts by itself: the help scripts it started lately (script, texts, a "
	          "town's need) and how many times each message set has been said",
	          {}, ResultKind::Object),
	    Serve<Locator::helpMessageSystem>(
	        "the help messages", [](const ecs::systems::HelpMessageSystemInterface& system, const QueryContext& /*c*/) -> Json {
		        Json started = Json::array();
		        for (const auto& message : system.GetStarted())
		        {
			        started.push_back({{"script", message.script},
			                           {"first", message.first},
			                           {"last", message.last},
			                           {"stat_text", message.statText.has_value() ? Json(*message.statText) : Json(nullptr)},
			                           {"need", message.need.has_value() ? Json(*message.need) : Json(nullptr)},
			                           {"turn", message.turn}});
		        }
		        Json sent = Json::object();
		        const auto& counts = system.GetSentCounts();
		        for (size_t set = 0; set < counts.size(); ++set)
		        {
			        if (counts.at(set) != 0)
			        {
				        sent[std::to_string(set)] = counts.at(set);
			        }
		        }
		        return Json {{"started", started}, {"sets_said", sent}};
	        }));
	provider->Add(
	    Query("guidance",
	          "The advisors' remarks on what the player does (towns attacked, buildings wrecked, alignment drifting, the "
	          "moon): the ones asked for lately (kind, text, turn, whether the advisors got to say it), the turns until "
	          "the moon is looked at again, and the player's guidance (each kind's last turn, the alignment's drift, the "
	          "one-offs said)",
	          {}, ResultKind::Object),
	    Serve<Locator::guidanceSystem>(
	        "the guidance", [](const ecs::systems::GuidanceSystemInterface& system, const QueryContext& /*c*/) -> Json {
		        Json remarks = Json::array();
		        for (const auto& remark : system.GetRemarks())
		        {
			        remarks.push_back({{"kind", static_cast<int>(remark.kind)},
			                           {"text", remark.text},
			                           {"turn", remark.turn},
			                           {"started", remark.started}});
		        }
		        Json heard = nullptr;
		        if (Locator::entitiesRegistry::has_value())
		        {
			        Locator::entitiesRegistry::value().Each<const ecs::components::GuidanceHeard>(
			            [&heard](entt::entity entity, const ecs::components::GuidanceHeard& guidance) {
				            heard = {{"player", entt::to_integral(entity)},
				                     {"last_turns", guidance.lastTurn},
				                     {"alignment_drift", guidance.alignmentDrift},
				                     {"one_offs_said", guidance.oneOffSaid}};
			            });
		        }
		        return Json {{"remarks", remarks}, {"moon_wait", system.GetMoonWait()}, {"heard", heard}};
	        }));
	provider->Add(
	    Query("bubble", "The tip bubble over the \"did you know\" sign tapped last: the sign, its tip, whether it is up, "
	                    "its time to show, the point it points at, its scroll and whether the hand is on it or grips it"),
	    Serve<Locator::tipBubbleSystem>(
	        "the tip bubble", [](const ecs::systems::TipBubbleSystemInterface& bubble, const QueryContext& /*c*/) -> Json {
		        const auto& scroll = bubble.GetScroll();
		        const auto anchor = bubble.GetAnchor();
		        return Json {
		            {"sign", bubble.GetSign() == entt::null ? Json(nullptr) : Json(entt::to_integral(bubble.GetSign()))},
		            {"text", bubble.GetText()},
		            {"up", bubble.IsUp()},
		            {"display_time", bubble.GetDisplayTime()},
		            {"anchor", anchor.has_value() ? Point(*anchor) : Json(nullptr)},
		            {"scroll_lines", scroll.lines},
		            {"content_height", scroll.contentHeight},
		            {"line_height", scroll.lineHeight},
		            {"more_above", scroll.moreAbove},
		            {"more_below", scroll.moreBelow},
		            {"under_hand", bubble.IsUnderHand()},
		            {"gripped", bubble.IsGripped()}};
	        }));
	provider->Add(Query("hand_demo",
	                    "The tutorial's hand demonstration playing: its name, the script task that started it, the record "
	                    "it is at, whether it holds at a mark, and the camera and hints it has"),
	              Serve<Locator::handDemoSystem>(
	                  "the hand demonstrations",
	                  [](const ecs::systems::HandDemoSystemInterface& demos, const QueryContext& /*c*/) -> Json {
		                  const auto status = demos.GetStatus();
		                  if (!status.has_value())
		                  {
			                  return Json {{"playing", false}};
		                  }
		                  Json result {{"playing", true},
		                               {"name", status->name},
		                               {"task", status->task},
		                               {"record", status->record},
		                               {"records", status->records},
		                               {"pause_on_trigger", status->pauseOnTrigger},
		                               {"trigger_reached", status->triggerReached},
		                               {"holding", status->holding},
		                               {"hints", demos.GetHints()}};
		                  if (const auto camera = demos.GetCamera(); camera.has_value())
		                  {
			                  result["camera_origin"] = Point(camera->origin);
			                  result["camera_focus"] = Point(camera->focus);
		                  }
		                  return result;
	                  }));
	return provider;
}

} // namespace

std::span<const LocatorCoverage> openblack::inspector::CoveredServices()
{
	return k_Coverage;
}

GameProvider* openblack::inspector::AddGameProviders(Inspector& inspector, const entt::meta_ctx& reflection,
                                                     RunTargetInterface& runTarget, WorldEditInterface& worldEdit,
                                                     const GameControls& controls)
{
	const RegistrySources registrySources {.registry = &Registry, .info = &Info};
	inspector.Add(std::make_unique<RegistryProvider>(registrySources, reflection));
	inspector.Add(std::make_unique<ObjectsProvider>(registrySources));
	inspector.Add(std::make_unique<EditProvider>(
	    EditSources {
	        .registry = []() -> ecs::Registry* {
		        return Locator::entitiesRegistry::has_value() ? &Locator::entitiesRegistry::value() : nullptr;
	        },
	        .info = &Info,
	    },
	    reflection, worldEdit));
	inspector.Add(SkyStateProvider(std::make_unique<SkyProvider>(SkySources {
	    .moon = []() -> std::optional<ecs::components::Moon> {
		    if (!Locator::skySystem::has_value() || !Locator::entitiesRegistry::has_value())
		    {
			    return std::nullopt;
		    }
		    const auto* moon =
		        Locator::entitiesRegistry::value().TryGet<ecs::components::Moon>(Locator::skySystem::value().GetMoon());
		    return moon != nullptr ? std::optional(*moon) : std::nullopt;
	    },
	    .cameraOrigin = []() -> std::optional<glm::vec3> {
		    if (!Locator::camera::has_value())
		    {
			    return std::nullopt;
		    }
		    return Locator::camera::value().GetOrigin();
	    },
	})));
	inspector.Add(std::make_unique<ParticlesProvider>(ParticleSources {
	    .rings = []() -> std::span<const water_rings::Ring> {
		    if (!Locator::waterRingSystem::has_value())
		    {
			    return {};
		    }
		    return Locator::waterRingSystem::value().GetRings();
	    },
	    .effects = []() -> std::vector<ParticleEffectInfo> {
		    if (!Locator::particleSystem::has_value())
		    {
			    return {};
		    }
		    return Locator::particleSystem::value().GetEffects();
	    },
	}));

	inspector.Add(MakePhysicsProvider({.bodies = &Bodies}));
	inspector.Add(MakeCreatureProvider({
	    .world = World(),
	    .tables = []() -> const creature_mind_tables::Tables* {
		    return Locator::creatureMindSystem::has_value() ? Locator::creatureMindSystem::value().GetTables() : nullptr;
	    },
	}));
	inspector.Add(MakeMapProvider({
	    .world = World(),
	    .map = []() -> const ecs::MapInterface* {
		    return Locator::entitiesMap::has_value() ? &Locator::entitiesMap::value() : nullptr;
	    },
	}));
	inspector.Add(MakeTownProvider(World()));
	inspector.Add(MakeWorshipProvider(World()));
	inspector.Add(MakeInfluenceProvider({
	    .hand = [](int player) -> std::optional<glm::vec3> {
		    // Only this computer's player's hands are known; the first hand there is
		    if (!Locator::handSystem::has_value() ||
		        (Locator::playerSystem::has_value() &&
		         static_cast<int>(Locator::playerSystem::value().GetLocalPlayer()) != player))
		    {
			    return std::nullopt;
		    }
		    for (const auto& position : Locator::handSystem::value().GetPlayerHandPositions())
		    {
			    if (position.has_value())
			    {
				    return position;
			    }
		    }
		    return std::nullopt;
	    },
	    .at = [](int player, glm::vec3 point) -> std::optional<InfluenceAt> {
		    if (!Locator::influenceSystem::has_value())
		    {
			    return std::nullopt;
		    }
		    const auto& influence = Locator::influenceSystem::value();
		    const auto name = static_cast<PlayerNames>(player);
		    return InfluenceAt {
		        .influence = influence.PlayerInfluence(name, point),
		        .handPoint = influence.HandPointInfluence(name, map_coords::FromMetres({point.x, point.z})),
		        .raw = influence.PlayerRawInfluence(name, point),
		    };
	    },
	    .borderShown =
	        [](int player) {
		        return Locator::influenceSystem::has_value() &&
		               Locator::influenceSystem::value().IsBorderShown(static_cast<PlayerNames>(player));
	        },
	    .circles = []() -> size_t {
		    return Locator::influenceSystem::has_value() ? Locator::influenceSystem::value().GetCircles().size() : 0;
	    },
	}));
	inspector.Add(MakeCameraProvider(controls.camera));
	inspector.Add(MakeGuiProvider(controls.gui));
	inspector.Add(MakeLevelProvider(controls.levels));
	inspector.Add(MakeAudioProvider({
	    .world = World(),
	    .state = []() -> std::optional<AudioState> {
		    if (!Locator::audio::has_value())
		    {
			    return std::nullopt;
		    }
		    auto& audio = Locator::audio::value();
		    AudioState state {.globalVolume = audio.GetGlobalVolume(),
		                      .sfxVolume = audio.GetSfxVolume(),
		                      .musicVolume = audio.GetMusicVolume(),
		                      .musicActive = audio.MusicIsActive(),
		                      .listener = audio.GetListener().origin};
		    if (const auto* game = Game::Instance(); game != nullptr && game->GetGameMusic() != nullptr)
		    {
			    const auto& music = *game->GetGameMusic();
			    state.musicPlaying = audio::GetMusicTypeName(music.GetPlaying());
			    state.landMusic = audio::GetMusicTypeName(music.GetLandType());
			    state.scriptMusic = audio::GetMusicTypeName(music.GetScriptType());
			    state.alignmentMusic = music.IsAlignmentMusicEnabled();
			    for (const auto& attached : music.GetAttachedMusic())
			    {
				    state.attachedMusic.emplace_back(audio::GetMusicTypeName(attached.type), attached.thing);
			    }
		    }
		    if (const auto* player = audio.GetMusic(); player != nullptr)
		    {
			    for (const auto& channel : player->GetChannels())
			    {
				    if (channel.active && channel.bank)
				    {
					    state.musicChannels.push_back({
					        .bank = channel.bank->path,
					        .volume = channel.volume,
					        .position = channel.placement ? std::optional(channel.placement->position) : std::nullopt,
					    });
				    }
			    }
		    }
		    return state;
	    },
	}));

	inspector.Add(EngineProvider());
	inspector.Add(LandProvider());
	inspector.Add(LivingProvider());
	inspector.Add(PlayersProvider());
	inspector.Add(MagicProvider());
	inspector.Add(TempleProvider());
	inspector.Add(ViewProvider());
	inspector.Add(AdvisorsProvider());
	inspector.Add(CreaturesProvider());
	inspector.Add(ScriptProvider(controls.scripts));
	inspector.Add(HelpProvider());

	auto game = std::make_unique<GameProvider>(runTarget);
	auto* gameProvider = game.get();
	inspector.Add(std::move(game));
	return gameProvider;
}
