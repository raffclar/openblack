/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "UnderwaterSystem.h"

#include <cmath>
#include <cstddef>

#include <algorithm>
#include <filesystem>
#include <functional>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <vector>

#include <LNDFile.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/Lightning.h"
#include "3D/TempleInteriorInterface.h"
#include "Audio/AudioManagerInterface.h"
#include "Camera/Camera.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Sky.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Weather.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "Graphics/Texture2D.h"
#include "Graphics/Underwater.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack::ecs::systems;

void UnderwaterSystem::Update(std::chrono::milliseconds frameRealTime)
{
	const float frameSeconds = std::chrono::duration<float>(frameRealTime).count();
	UpdateSea(frameSeconds);
	// The debug tour puts the camera where it is now on its way
	if (_tourSeconds.has_value() && Locator::camera::has_value())
	{
		*_tourSeconds += std::chrono::duration<float>(frameRealTime).count();
		using graphics::underwater::Tour;
		float length = graphics::underwater::k_TourSeconds;
		if (_tour == Tour::FarSeabed)
		{
			// Six units over the sea bed, looking ahead along it and a little down
			length = graphics::underwater::k_FarTourSeconds;
			const auto here = graphics::underwater::FarTourGround(*_tourSeconds);
			const auto ahead = graphics::underwater::FarTourGround(*_tourSeconds + 1.5f);
			const auto along = glm::length(ahead - here) > 0.0f ? glm::normalize(ahead - here) : glm::vec2(-1.0f, 0.0f);
			const auto target = here + along * 45.0f;
			const glm::vec3 eye {here.x, -_seabed.DepthAt(here) + 6.0f, here.y};
			const glm::vec3 focus {target.x, -_seabed.DepthAt(target) + 1.0f, target.y};
			Locator::camera::value().Place(eye, focus);
		}
		else
		{
			const auto pose = graphics::underwater::TourAt(*_tourSeconds);
			Locator::camera::value().Place(pose.eye, pose.focus);
		}
		if (*_tourSeconds >= length)
		{
			_tourSeconds.reset();
		}
	}
	// How deep the camera is, and the land's sounds and the music turned down under the sea and back up above it. The
	// change takes about a third of a second each way, so nothing clicks at the surface.
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	const auto eye = Locator::camera::has_value() ? Locator::camera::value().GetOrigin() : glm::vec3(0.0f);
	_eyeDepth = _enabled && !inTemple ? graphics::underwater::EyeDepth(eye.y, _seaLevel) : 0.0f;
	_eyeUnder = _eyeDepth > 0.0f;
	constexpr float k_DuckUnder = 0.3f;
	constexpr float k_DuckPerSecond = 2.5f;
	const float target = _eyeUnder ? k_DuckUnder : 1.0f;
	const float step = k_DuckPerSecond * std::chrono::duration<float>(frameRealTime).count();
	_duck = target < _duck ? std::max(target, _duck - step) : std::min(target, _duck + step);
	if (Locator::audio::has_value())
	{
		Locator::audio::value().SetUnderwaterDuck(_duck);
	}

	if (!Locator::terrainSystem::has_value() || !Locator::terrainSystem::value().IsLoaded())
	{
		return;
	}
	// The texture caches can be emptied when a land opens, so the sand is looked for each frame
	LoadSand();
	const auto& island = Locator::terrainSystem::value();
	const void* land = &island.GetHeightMap();
	if (land == _builtFor)
	{
		return;
	}
	_builtFor = land;
	// A corner for each cell corner of the whole map: the sea bed goes on past the land's blocks to the map's edge
	constexpr int k_Corners = LandIslandInterface::k_MapCellsPerSide + 1;
	const glm::ivec2 size {k_Corners, k_Corners};
	std::vector<uint8_t> altitudes(static_cast<size_t>(size.x) * static_cast<size_t>(size.y));
	// How much of each corner's ground is the beach's sand, from its country's materials at its altitude
	std::vector<float> beach(altitudes.size(), 0.0f);
	const auto& countries = island.GetCountries();
	const auto types = island.GetMaterialTypes();
	size_t beachCorners = 0;
	for (int z = 0; z < size.y; ++z)
	{
		for (int x = 0; x < size.x; ++x)
		{
			const auto i = static_cast<size_t>(z) * static_cast<size_t>(size.x) + static_cast<size_t>(x);
			const auto& cell = island.GetCell(glm::u16vec2(x, z));
			altitudes[i] = cell.altitude;
			if (cell.properties.country < countries.size())
			{
				beach[i] = seabed::BeachShare(countries[cell.properties.country], cell.altitude, types);
				beachCorners += beach[i] > 0.5f && cell.altitude > seabed::k_SeaLevelAltitude ? 1 : 0;
			}
		}
	}
	_seabed = seabed::Build(altitudes, size, glm::vec2(0.0f), LandIslandInterface::k_CellSize, beach);
	SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "Underwater experiment: {} corners of beach sand above the sea", beachCorners);
	_reef = seabed::PlaceReef(_seabed, graphics::underwater::k_LowestSea, graphics::underwater::k_MostCoral,
	                          graphics::underwater::k_MostPlants);
	SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "Underwater experiment: {} coral and plants on the reef", _reef.size());
	++_revision;
}

void UnderwaterSystem::UpdateSea(float frameSeconds)
{
	namespace underwater = graphics::underwater;
	_seconds = std::fmod(_seconds + frameSeconds, 3600.0f);
	// The tide, from the hour and the moon's phase
	float hour = 12.0f;
	float phase = std::numbers::pi_v<float>;
	if (Locator::skySystem::has_value())
	{
		const auto& sky = Locator::skySystem::value();
		hour = sky.GetClock().GetScriptTime();
		if (const auto* moon = Locator::entitiesRegistry::value().TryGet<const ecs::components::Moon>(sky.GetMoon()))
		{
			phase = moon->phase;
		}
	}
	const float tide = _enabled && underwater::k_MoonTides ? underwater::TideLevel(hour, phase) : underwater::k_SeaLevel;
	// The tide moves slowly, so a jump of the clock does not jerk the sea
	const float before = _seaLevel;
	_seaLevel = underwater::EaseTowards(_seaLevel, tide, 0.4f, frameSeconds);
	if (frameSeconds > 0.0f)
	{
		_tideRate = underwater::EaseTowards(_tideRate, (_seaLevel - before) / frameSeconds, 0.05f, frameSeconds);
	}

	// The storm at the camera: the sea gets rough over some seconds and calms again more slowly
	float storm = 0.0f;
	if (Locator::weatherSystem::has_value() && Locator::camera::has_value())
	{
		const auto weather = Locator::weatherSystem::value().GetWeatherSmooth(Locator::camera::value().GetOrigin());
		const glm::vec2 wind {static_cast<float>(weather.windX), static_cast<float>(weather.windZ)};
		storm = underwater::Storminess(weather.rain, weather.overcast, wind);
		if (glm::length(wind) > 5.0f)
		{
			// The waves turn with the wind, slowly
			const float target = std::atan2(wind.y, wind.x);
			const float turn = std::remainder(target - _windAngle, 2.0f * std::numbers::pi_v<float>);
			_windAngle += std::clamp(turn, -0.2f * frameSeconds, 0.2f * frameSeconds);
		}
	}
	_storm = underwater::EaseTowards(_storm, storm, storm > _storm ? 0.12f : 0.05f, frameSeconds);
	_waves = underwater::WavesFor(_storm, _windAngle);
	// The waterspouts, out at sea from the coast of the sea bed's map
	const auto& bed = _seabed;
	const auto seaDistance = [&bed](glm::vec2 point) {
		if (bed.size.x == 0 || bed.shore.empty())
		{
			return 0.0f;
		}
		const glm::ivec2 corner =
		    glm::clamp(glm::ivec2(glm::round((point - bed.origin) / bed.spacing)), glm::ivec2(0), bed.size - 1);
		return bed.shore[static_cast<size_t>((corner.y * bed.size.x) + corner.x)].x;
	};
	{
		const glm::vec2 centre = bed.origin + glm::vec2(bed.size - 1) * bed.spacing * 0.5f;
		_waterspouts = underwater::WaterspoutsAt(_seconds, _storm, centre, seaDistance);
	}
	UpdateStorm(frameSeconds, seaDistance);
	_current = underwater::CurrentFor(_tideRate, _windAngle, _storm);

	// The things that float: the bodies in the physics over the sea, near its surface
	_floaters.clear();
	if (!_enabled || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	Locator::entitiesRegistry::value().Each<const ecs::components::PhysicsDrawPose>(
	    [this](entt::entity /*entity*/, const ecs::components::PhysicsDrawPose& pose) {
		    if (Floats(pose.origin))
		    {
			    _floaters.push_back(pose.origin);
		    }
	    });
	const auto eye = Locator::camera::has_value() ? Locator::camera::value().GetOrigin() : glm::vec3(0.0f);
	std::ranges::sort(_floaters, {}, [&eye](const glm::vec3& at) { return glm::distance(at, eye); });
	constexpr size_t k_MostFloaters = 8;
	if (_floaters.size() > k_MostFloaters)
	{
		_floaters.resize(k_MostFloaters);
	}
}

void UnderwaterSystem::UpdateStorm(float frameSeconds, const std::function<float(glm::vec2)>& seaDistance)
{
	namespace underwater = graphics::underwater;
	// The rogue wave comes near what the camera looks at. The point stays the same while a wave lives.
	if (!_rogue.has_value() && Locator::camera::has_value())
	{
		const auto focus = Locator::camera::value().GetFocus();
		_rogueCentre = {focus.x, focus.z};
	}
	_rogueSeconds += frameSeconds;
	if (_rogueSeconds > underwater::k_RoguePeriod * 1000.0f)
	{
		_rogueSeconds = 0.0f;
		_rogueCertain = false;
	}
	if (_rogueCertain && _rogueSeconds > underwater::k_RoguePeriod)
	{
		_rogueCertain = false;
	}
	_rogue = _enabled ? underwater::RogueWaveAt(_rogueSeconds, _waves, _rogueCentre, seaDistance, _rogueCertain) : std::nullopt;

	// The lightning: the youngest flash of the storms, and the flash of the sky over the camera that the land takes too
	std::optional<underwater::StrikeFlash> flash;
	float bolt = 0.0f;
	float sky = 0.0f;
	if (Locator::weatherSystem::has_value() && Locator::entitiesRegistry::has_value())
	{
		Locator::entitiesRegistry::value().Each<const ecs::components::Storm>([&](const ecs::components::Storm& storm) {
			if (storm.dead || !storm.flash.active || (flash.has_value() && flash->time <= storm.flash.time))
			{
				return;
			}
			flash = underwater::StrikeFlash {.position = {storm.flash.position.x, storm.flash.position.z},
			                                 .time = storm.flash.time,
			                                 .strength = storm.flash.strength,
			                                 .active = true};
			bolt = lightning::Brightness(storm.flash);
		});
		if (Locator::camera::has_value())
		{
			sky = static_cast<float>(Locator::weatherSystem::value().GetLightningFlash(Locator::camera::value().GetOrigin())) /
			      255.0f;
		}
	}
	_strike = underwater::TrackStrike(_strike, flash, frameSeconds, flash.has_value() ? seaDistance(flash->position) : 0.0f);
	_seaFlash = {sky, bolt};
}

bool UnderwaterSystem::Floats(const glm::vec3& origin) const
{
	// The physics floats a thing at the usual sea height: near it, and over water deeper than a unit
	return std::abs(origin.y - graphics::underwater::k_SeaLevel) < 3.0f && _seabed.DepthAt({origin.x, origin.z}) > 1.0f;
}

glm::mat4 UnderwaterSystem::FloatOnWaves(const glm::mat4& model) const
{
	const glm::vec3 origin = model[3];
	if (!_enabled || !Floats(origin))
	{
		return model;
	}
	// The rogue wave lifts it too, faded near the coast and in shallow water as the shaders fade it
	const glm::vec2 ground {origin.x, origin.z};
	const auto shore = _seabed.shore.empty() ? glm::vec4(1000.0f, 0.0f, 0.0f, -160.0f)
	                                         : graphics::underwater::ShoreAt(_seabed.shore, _seabed.cliff, _seabed.size,
	                                                                         _seabed.spacing, ground - _seabed.origin);
	const auto wave = graphics::underwater::WavesAt(ground, _seconds, _waves, _rogue, shore.x, _seaLevel - shore.w);
	// A thing that sinks rides the waves less as it goes down, and not at all from 3 units under the surface, so it goes
	// on down smoothly to the sea bed
	constexpr float k_FullRideDepth = 1.0f;
	constexpr float k_NoRideDepth = 3.0f;
	const float ride = glm::clamp(
	    (origin.y - graphics::underwater::k_SeaLevel + k_NoRideDepth) / (k_NoRideDepth - k_FullRideDepth), 0.0f, 1.0f);
	const float lift = (_seaLevel - graphics::underwater::k_SeaLevel + wave.height) * ride;
	// It tilts with the surface, a little less than the surface does
	const glm::vec3 up {0.0f, 1.0f, 0.0f};
	const auto normal = glm::normalize(glm::vec3(-wave.slope.x, 1.0f, -wave.slope.y));
	const auto tilt = glm::rotation(up, glm::normalize(glm::mix(up, normal, 0.7f * ride)));
	return glm::translate(glm::mat4(1.0f), origin + glm::vec3(0.0f, lift, 0.0f)) * glm::mat4_cast(tilt) *
	       glm::translate(glm::mat4(1.0f), -origin) * model;
}

void UnderwaterSystem::StartDebugStorm()
{
	if (!Locator::weatherSystem::has_value())
	{
		return;
	}
	// Rain, cloud and a strong wind over the land for forty seconds, coming in and clearing over ten each
	ecs::systems::ForcedStorm storm {};
	storm.effect.rain = 100;
	storm.effect.overcast = 100;
	storm.effect.windX = 90;
	storm.effect.windZ = 40;
	storm.seconds = 40.0f;
	storm.fadeSeconds = 10.0f;
	// Lightning now and then, to see it on the sea, and a rogue wave once the sea is rough
	storm.thunderWait = {4.0f, 9.0f};
	storm.boltWait = {2.0f, 5.0f};
	_rogueSeconds = 0.0f;
	_rogueCertain = true;
	Locator::weatherSystem::value().ForceStorm(storm);
}

void UnderwaterSystem::DropDebugFloaters()
{
	if (!Locator::dynamicsSystem::has_value() || !Locator::camera::has_value())
	{
		return;
	}
	const auto focus = Locator::camera::value().GetFocus();
	std::vector<entt::entity> near;
	Locator::entitiesRegistry::value().Each<const ecs::components::MobileObject, const ecs::components::Transform>(
	    [&](entt::entity entity, const ecs::components::MobileObject& /*mobile*/, const ecs::components::Transform& transform) {
		    if (glm::distance(glm::vec2(transform.position.x, transform.position.z), glm::vec2(focus.x, focus.z)) < 40.0f)
		    {
			    near.push_back(entity);
		    }
	    });
	for (const auto entity : near)
	{
		Locator::dynamicsSystem::value().InitialisePhysics(entity,
		                                                   ecs::systems::PhysicsStart {.velocity = {0.0f, -1.0f, 0.0f}});
	}
	SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "Underwater experiment: dropped {} things into the physics", near.size());
}

void UnderwaterSystem::CycleEffectOff()
{
	using graphics::underwater::Effect;
	const auto next = (static_cast<uint8_t>(_effectOff) + 1) % static_cast<uint8_t>(Effect::_Count);
	_effectOff = static_cast<Effect>(next);
	SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "Underwater experiment: effect {} turned off (0 is none)", next);
}

void UnderwaterSystem::LoadSand()
{
	// The coral atlas, with its alpha, through the texture cache
	{
		auto& textures = Locator::resources::value().GetTextures();
		const std::filesystem::path coral {graphics::underwater::k_CoralPath};
		const std::filesystem::path coralAlpha {graphics::underwater::k_CoralAlphaPath};
		if (!textures.Contains(graphics::underwater::k_CoralTextureId.value()) && std::filesystem::exists(coral) &&
		    std::filesystem::exists(coralAlpha))
		{
			try
			{
				textures.Load(graphics::underwater::k_CoralTextureId.value(),
				              resources::Texture2DLoader::FromDiskWithAlphaTag {}, coral, coralAlpha,
				              graphics::underwater::k_CoralSide);
			}
			catch (std::runtime_error& err)
			{
				SPDLOG_LOGGER_ERROR(spdlog::get("graphics"), "{}", err.what());
			}
		}
	}
	// The sand of the sea bed, from the experiment's folder, through the texture cache
	auto& textures = Locator::resources::value().GetTextures();
	const std::filesystem::path sand {graphics::underwater::k_SandPath};
	if (textures.Contains(graphics::underwater::k_SandTextureId.value()) || !std::filesystem::exists(sand))
	{
		return;
	}
	try
	{
		textures.Load(graphics::underwater::k_SandTextureId.value(), resources::Texture2DLoader::FromDiskTag {}, sand);
		SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "Loaded the sea bed's sand from {}", sand.string());
	}
	catch (std::runtime_error& err)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("graphics"), "{}", err.what());
	}
}

float UnderwaterSystem::FloorAt(glm::vec2 point, float landHeight) const
{
	if (!_enabled)
	{
		return landHeight;
	}
	return seabed::FloorHeight(_seabed, point, landHeight);
}
