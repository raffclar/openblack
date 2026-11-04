/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <ranges>
#include <utility>

#include <LNDFile.h>

#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "3D/OceanInterface.h"
#include "ECS/Components/Cloud.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Tree.h"
#include "ECS/DrawnRegistry.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CloudSystemInterface.h"
#include "ECS/Systems/MistSystemInterface.h"
#include "ECS/Systems/RainSystemInterface.h"
#include "ECS/Systems/RealWeatherSystemInterface.h"
#include "ECS/Systems/SkyClockSystemInterface.h"
#include "ECS/Systems/SnowSystemInterface.h"
#include "ECS/Systems/SnowfallSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/VegetationInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "GameProviderShared.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::inspector;
using namespace openblack::ecs::components;
using namespace openblack::inspector::game_providers;

// The land, the sea and the weather

namespace openblack::inspector::game_providers
{

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
	provider->Add(Query("storm_clouds", "The clouds of the storms that the frames make and draw: for each, its storm, its "
	                                    "place in the order that its storm gathered them, where it is drawn, its size and "
	                                    "its alpha"),
	              Serve<Locator::cloudSystem>(
	                  "the clouds", [](const ecs::systems::CloudSystemInterface& clouds, const QueryContext& /*c*/) {
		                  Json list = Json::array();
		                  for (const auto& cloud : clouds.GetStormClouds())
		                  {
			                  list.push_back({{"storm", entt::to_integral(cloud.storm)},
			                                  {"index", cloud.index},
			                                  {"position", Point(cloud.position)},
			                                  {"size", cloud.mist.size},
			                                  {"alpha", cloud.mist.colour >> 24u}});
		                  }
		                  return Json {{"count", clouds.GetStormClouds().size()}, {"clouds", std::move(list)}};
	                  }));
	provider->Add(Query("real_weather", "The real-world weather: the weather that the lobby gave, whether it changes the land, "
	                                    "its storm, the place of its message and the background weather"),
	              Serve<Locator::realWeatherSystem>(
	                  "the real weather", [](const ecs::systems::RealWeatherSystemInterface& real, const QueryContext& /*c*/) {
		                  Json result {{"applied", real.IsApplied()}, {"storm", real.GetStorm() != entt::null}};
		                  if (const auto weather = real.GetWeather(); weather.has_value())
		                  {
			                  result["weather"] = Json {{"type", weather->type},
			                                            {"temperature", weather->temperature},
			                                            {"visibility", weather->visibility},
			                                            {"city", weather->city},
			                                            {"area", weather->area},
			                                            {"country", weather->country}};
			                  result["place"] = real.GetPlace().value_or("");
		                  }
		                  else
		                  {
			                  result["weather"] = nullptr;
		                  }
		                  if (Locator::weatherSystem::has_value())
		                  {
			                  const auto background = Locator::weatherSystem::value().GetBackgroundWeather();
			                  result["background"] = Json {{"temperature", background.temperature},
			                                               {"rain", background.rain},
			                                               {"snow", background.snow},
			                                               {"overcast", background.overcast},
			                                               {"wind", {background.windX, background.windZ}},
			                                               {"snow_cover", background.snowCover}};
		                  }
		                  return result;
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
	provider->Add(Query("precipitation", "The rain's streaks, fall and splashes, and the falling snow's flakes"),
	              [](const QueryContext& /*c*/) {
		              Json result = Json::object();
		              if (Locator::rainSystem::has_value())
		              {
			              const auto& rain = Locator::rainSystem::value();
			              result["rain"] = {{"streaks", rain.GetStreaks().size()},
			                                {"height", rain.GetHeight()},
			                                {"speed", rain.GetFall().speed},
			                                {"splashes", rain.GetSplashCount()}};
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
		              // The sky's clouds are things of the drawn registry
		              const auto* drawn = DrawnRegistry();
		              return Json {{"mists", CountOf<Mist>(registry)},
		                           {"mist_system", Locator::mistSystem::has_value()},
		                           {"clouds", drawn != nullptr ? CountOf<Cloud>(*drawn) : size_t {0}},
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
	provider->Add(Query("state", "The sky: its type, its time, the script clock's hour and the day and night's times"),
	              Serve<Locator::skyClockSystem>(
	                  "the sky", [](const ecs::systems::SkyClockSystemInterface& sky, const QueryContext& /*c*/) {
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

} // namespace openblack::inspector::game_providers
