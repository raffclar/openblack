/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameProviders.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#include <Inspector.h>
#include <InspectorQuery.h>
#include <LHVM.h>
#include <LNDFile.h>
#include <glm/trigonometric.hpp>

#include "3D/DayNightClock.h"
#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "3D/OceanInterface.h"
#include "3D/TempleInteriorInterface.h"
#include "Audio/AudioManagerInterface.h"
#include "CHLApi.h"
#include "Camera/Camera.h"
#include "Common/EventManager.h"
#include "Common/GameRandom.h"
#include "Common/RandomNumberManager.h"
#include "Debug/DebugGuiInterface.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AudioEmitter.h"
#include "ECS/Components/CameraBookmark.h"
#include "ECS/Components/ChimneySmoke.h"
#include "ECS/Components/Cloud.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Sky.h"
#include "ECS/Components/SoundTag.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TempleExterior.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageLight.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Map.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AbodeKnockSystemInterface.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/BuildingDamageSystemInterface.h"
#include "ECS/Systems/CameraBookmarkSystemInterface.h"
#include "ECS/Systems/CameraHelpSystemInterface.h"
#include "ECS/Systems/CameraPathSystemInterface.h"
#include "ECS/Systems/ChimneySmokeSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CloudSystemInterface.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureAudioSystemInterface.h"
#include "ECS/Systems/CreatureCarryOverSystemInterface.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureHairSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/EditorSystemInterface.h"
#include "ECS/Systems/ExplosionSystemInterface.h"
#include "ECS/Systems/FieldSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/FireflySystemInterface.h"
#include "ECS/Systems/FootprintSystemInterface.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/GestureEventsInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/InspectorSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/MagicShieldSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/MiracleFxSystemInterface.h"
#include "ECS/Systems/MistSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PathfindingSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/RainSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/Systems/RewardSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/SnowSystemInterface.h"
#include "ECS/Systems/SnowfallSystemInterface.h"
#include "ECS/Systems/SoundTagSystemInterface.h"
#include "ECS/Systems/TeleportSystemInterface.h"
#include "ECS/Systems/TempleDestructionSystemInterface.h"
#include "ECS/Systems/TempleExteriorSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TornadoSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/Systems/VegetationInterface.h"
#include "ECS/Systems/VillageLightSystemInterface.h"
#include "ECS/Systems/WaterRingSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "EditProviders.h"
#include "Editor/EditorSelection.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "GameControls.h"
#include "Graphics/RendererInterface.h"
#include "InfoConstants.h"
#include "Input/GameActionMapInterface.h"
#include "Locator.h"
#include "Profiler.h"
#include "RegistryProviders.h"
#include "Resources/ResourcesInterface.h"
#include "RunControl.h"
#include "SystemProviders.h"
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
    LocatorCoverage {"tornadoSystem", "magic.state"},
    LocatorCoverage {"magicShieldSystem", "magic.state"},
    LocatorCoverage {"forestSystem", "living.forests"},
    LocatorCoverage {"fireflySystem", "living.fireflies"},
    LocatorCoverage {"gestureSystem", "players.gestures"},
    LocatorCoverage {"miracleFxSystem", "magic.state"},
    LocatorCoverage {"fireSystem", "magic.fires"},
    LocatorCoverage {"explosionSystem", "magic.state"},
    LocatorCoverage {"rewardSystem", "living.rewards"},
    LocatorCoverage {"scriptObjects", "script.objects"},
    LocatorCoverage {"buildingDamageSystem", "living.building"},
    LocatorCoverage {"inspector", "engine.services"},
    LocatorCoverage {"vm", "script.vm"},
    LocatorCoverage {"chlapi", "script.natives"},
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

std::unique_ptr<ProviderInterface> EngineProvider()
{
	auto provider = std::make_unique<FunctionProvider>("engine");
	provider->Add(Query("config", "The engine's settings: what is drawn, the display and the camera's clip"),
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
		                  {"wireframe", config.wireframe},
		                  {"time_of_day", config.timeOfDay},
		                  {"resolution", {config.resolution.x, config.resolution.y}},
		                  {"vsync", config.vsync},
		                  {"camera_fov", config.cameraXFov},
		                  {"camera_near_clip", config.cameraNearClip},
		                  {"camera_far_clip", config.cameraFarClip},
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
		              return Json {{"diffuse_texture", ocean.GetDiffuseTexture()}, {"alpha_texture", ocean.GetAlphaTexture()}};
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
	provider->Add(Query("animal", "An animal: its kind's radius, where it moves to and whether a creature fears it",
	                    {IdParameter("The animal's entity id")}),
	              Serve<Locator::animalSystem>(
	                  "the animals", [](const ecs::systems::AnimalSystemInterface& animals, const QueryContext& context) {
		                  const auto entity = EntityParam(context.params);
		                  const auto* registry = Registry();
		                  if (!entity.has_value() || registry == nullptr || !registry->AllOf<Animal>(*entity))
		                  {
			                  return QueryResult::Error("no animal with that id");
		                  }
		                  return QueryResult::Value({{"id", ToId(*entity)},
		                                             {"radius", animals.RadiusOf(*entity)},
		                                             {"movement", Point(animals.MovementOf(*entity))},
		                                             {"goal", Point(animals.GoalOf(*entity))},
		                                             {"frightening_to_creature", animals.IsFrighteningToCreature(*entity)},
		                                             {"player_can_pick_up", animals.CanPlayerPickUp(*entity)}});
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
	provider->Add(Query("fireflies", "How many fireflies there are"),
	              Serve<Locator::fireflySystem>(
	                  "the fireflies", [](const ecs::systems::FireflySystemInterface& fireflies, const QueryContext& /*c*/) {
		                  return Json {{"fireflies", fireflies.GetFireflies().size()}};
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
	provider->Add(Query("rewards", "The rewards on their way to the land or lying on it", {}, ResultKind::List),
	              ServeRegistry([](const ecs::Registry& registry, const QueryContext& /*c*/) {
		              Json items = Json::array();
		              registry.Each<const Reward>([&items, &registry](entt::entity entity, const Reward& reward) {
			              auto item = Listed(registry, entity);
			              item["type"] = static_cast<int>(reward.type);
			              item["state"] = static_cast<int>(reward.state);
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
	    Serve<Locator::townDesireSystem>(
	        "the town desires", [](const ecs::systems::TownDesireSystemInterface& desires, const QueryContext& context) {
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
		        return QueryResult::Value({{"id", ToId(*entity)},
		                                   {"most_wanted", static_cast<int>(desires.GetMostWanted(*entity))},
		                                   {"desires", std::move(each)}});
	        }));
	return provider;
}

// The players, their hands and their input

std::unique_ptr<ProviderInterface> PlayersProvider()
{
	auto provider = std::make_unique<FunctionProvider>("players");
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
	    Query("pick", "What the cursor picks: the object, the point and the land under it"),
	    Serve<Locator::pickingSystem>(
	        "the picking", [](const ecs::systems::PickingSystemInterface& picking, const QueryContext& /*c*/) {
		        const auto& pick = picking.GetPick();
		        return Json {
		            {"object", Id(pick.object)}, {"point", Optional(pick.point)},        {"land", Optional(pick.land)},
		            {"distance", pick.distance}, {"hover_object", Id(pick.hoverObject)}, {"hover_seconds", pick.hoverSeconds}};
	        }));
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
	provider->Add(Query("interior", "The temple's inside: whether the player is in it, the room and the visits"),
	              Serve<Locator::temple>("the temple", [](const TempleInteriorInterface& temple, const QueryContext& /*c*/) {
		              const auto transition = temple.GetTransitionRoom();
		              return Json {
		                  {"active", temple.Active()},
		                  {"position", Point(temple.GetPosition())},
		                  {"room", static_cast<int>(temple.GetCurrentRoom())},
		                  {"transition_room", transition.has_value() ? Json(static_cast<int>(*transition)) : Json(nullptr)},
		                  {"visits", temple.GetVisits()}};
	              }));
	return provider;
}

// The camera's helpers, the editor and the cinematics

std::unique_ptr<ProviderInterface> ViewProvider()
{
	auto provider = std::make_unique<FunctionProvider>("view");
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
	    Query("cinematic", "The fade, the wide screen and whether the interface is shown"),
	    Serve<Locator::cinematicDirectorSystem>(
	        "the cinematics", [](const ecs::systems::CinematicDirectorSystemInterface& director, const QueryContext&) {
		        return Json {{"fade_finished", director.IsFadeFinished()},
		                     {"fade_colour", director.GetFadeColour()},
		                     {"wide_screen", director.IsWideScreenOn()},
		                     {"wide_screen_fraction", director.GetWideScreenFraction()},
		                     {"interface_active", director.IsInterfaceActive()},
		                     {"close_clipping", director.IsCloseClipping()}};
	        }));
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
			              result["cave"] = {
			                  {"open", cave.IsOpen()}, {"in_temple", cave.InTemple()}, {"creature", Id(cave.GetCreature())}};
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
		constexpr std::array<std::string_view, 5> k_Results {"started", "no_opponent", "busy", "too_weak", "no_arena"};
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
	    Query("tasks", "The script tasks running: their scripts, where they are and whether they sleep", {}, ResultKind::List),
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
	provider->Add(Query("objects", "How many things scripts hold and control"),
	              ServeRegistry([](const ecs::Registry& registry, const QueryContext& /*c*/) {
		              return Json {{"in_script", CountOf<InScript>(registry)},
		                           {"script_controlled", CountOf<ScriptControlled>(registry)},
		                           {"system", Locator::scriptObjects::has_value()}};
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
		    return AudioState {.globalVolume = audio.GetGlobalVolume(),
		                       .sfxVolume = audio.GetSfxVolume(),
		                       .musicVolume = audio.GetMusicVolume(),
		                       .musicActive = audio.MusicIsActive()};
	    },
	}));

	inspector.Add(EngineProvider());
	inspector.Add(LandProvider());
	inspector.Add(LivingProvider());
	inspector.Add(PlayersProvider());
	inspector.Add(MagicProvider());
	inspector.Add(TempleProvider());
	inspector.Add(ViewProvider());
	inspector.Add(CreaturesProvider());
	inspector.Add(ScriptProvider(controls.scripts));

	auto game = std::make_unique<GameProvider>(runTarget);
	auto* gameProvider = game.get();
	inspector.Add(std::move(game));
	return gameProvider;
}
