/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the haze of the air's light and the air's sky near the horizon. The original game has a
// haze by distance only.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <limits>
#include <numbers>
#include <vector>

#include <LNDFile.h>
#include <bgfx/bgfx.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec4.hpp>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/LandLightFrame.h"
#include "3D/LandLightTable.h"
#include "3D/OceanInterface.h"
#include "3D/SkyDome.h"
#include "3D/SkyFrame.h"
#include "Camera/Camera.h"
#include "ECS/Components/Sky.h"
#include "ECS/Registry.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/UnderwaterSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "EngineConfig.h"
#include "Graphics/DetailLevel.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/IndexBuffer.h"
#include "Graphics/Mesh.h"
#include "Graphics/Moon.h"
#include "Graphics/PlaceLight.h"
#include "Graphics/RealMoon.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/ShaderProgram.h"
#include "Graphics/Underwater.h"
#include "Graphics/VertexBuffer.h"
#include "Graphics/VolumetricClouds.h"
#include "Graphics/VolumetricFog.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;
namespace atmos = openblack::graphics::atmosphere;
namespace gi = openblack::graphics::global_illumination;

namespace
{
/// The cool, silver tint of the moon's light (as the clouds and the fog have it)
constexpr glm::vec3 k_MoonTint {0.72f, 0.84f, 1.1f};
/// The sky's directions that the processor sums into the sky's light
constexpr uint16_t k_SkySamples = 32;
/// The seconds between two sums of the sky's light on the processor at most, while the light moves
constexpr float k_SkySeconds = 0.5f;
/// The steps along each ray of the haze's grid, with and without the shadows
constexpr float k_FullSteps = 12.0f;
constexpr float k_ReducedSteps = 6.0f;
/// The haze is this much brighter than the original game's haze at the horizon: a bright, airy haze that lifts the far
/// land rather than greys it
constexpr float k_HazeBrightness = 1.3f;
/// The height in kilometres over which the sky's exposure does not change (a little over the eye's highest height
/// before the experiment let it climb to space)
constexpr float k_HighestExposure = 32.0f;
/// How strongly the moon shows high in a clear sky, 0 to 255
constexpr float k_MoonFullAlpha = 200.0f;

void BindScreenTriangle()
{
	const auto& mesh = Locator::oceanSystem::value().GetMesh();
	mesh.GetIndexBuffer().Bind(mesh.GetIndexBuffer().GetCount(), 0);
	mesh.GetVertexBuffer().Bind();
}

/// Whether the sky's light must be worked out again for the light, the air and the eye's height
bool Stale(const std::optional<AtmosphereView::SkyCache>& cache, glm::vec3 towardsLight, float turbidity, float altitude,
           float seconds)
{
	if (!cache.has_value())
	{
		return true;
	}
	const bool moved = glm::dot(cache->towardsLight, towardsLight) < 0.99999f ||
	                   std::abs(cache->turbidity - turbidity) > 0.05f || std::abs(cache->altitude - altitude) > 0.05f;
	return moved && seconds - cache->seconds >= k_SkySeconds;
}

/// The colour that the air gives to the light at a height of the light, at the brightness that the clouds and the fog
/// have without the haze, and the same light in the colours of the screen
struct AirTint
{
	glm::vec3 tint {1.0f};
	glm::vec3 onScreen {1.0f};
};
AirTint AirLightTint(float lightHeight, const atmos::Air& air)
{
	const glm::vec3 airLight = atmos::LightAtGround(lightHeight, air);
	const float brightness = atmos::Luminance(volumetric_clouds::SunThroughAir(lightHeight));
	const glm::vec3 tint = airLight * (brightness / std::max(atmos::Luminance(airLight), 1e-4f));
	// The strongest colour on the screen stays as strong, so a low sun stays bright
	const glm::vec3 onScreen = atmos::OnScreen(airLight);
	const float peak = std::max({tint.r, tint.g, tint.b});
	const float peakOnScreen = std::max({onScreen.r, onScreen.g, onScreen.b, 1e-4f});
	return {.tint = tint, .onScreen = onScreen * (peak / peakOnScreen)};
}

/// A direction with the height `height` (a sine) and the same direction across the ground as `towards`
glm::vec3 AtHeight(glm::vec3 towards, float height)
{
	const glm::vec2 across {towards.x, towards.z};
	const glm::vec2 unit = glm::length(across) > 1e-6f ? glm::normalize(across) : glm::vec2(1.0f, 0.0f);
	const float flat = std::sqrt(std::max(1.0f - (height * height), 0.0f));
	return {unit.x * flat, height, unit.y * flat};
}

/// What the table of the light of each place is made from
struct PlaceInputs
{
	atmos::Air air;
	glm::vec3 sunTowards {0.0f, 1.0f, 0.0f};
	glm::vec3 moonTowards {0.0f, -1.0f, 0.0f};
	/// The island's own sky colour as the game shows it. It is also the sky when the land's palette is not loaded.
	glm::vec3 island {0.0f};
	/// The tint of the land's colours on the island's haze
	glm::vec3 placeTint {1.0f};
	/// The height of the eye for the sky's exposure, in kilometres, and the height of the sea's horizon there
	float altitude {0.0f};
	float horizonY {0.0f};
};

/// The table of the light of each place by the height of its own sun (place_light), and the moon's light there. The
/// sun, the night glow and the sky colour follow the same rules as the island's light. The air's brightness at each
/// height costs a march, so the renderer works out two heights again each frame, in turn.
void PlaceLightTable(AtmosphereView& view, AtmosphereFrame& atmosphere, const PlaceInputs& inputs)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* sky = Locator::skySystem::has_value() ? &Locator::skySystem::value() : nullptr;
	const auto* dome = sky != nullptr ? registry.TryGet<const ecs::components::SkyDome>(sky->GetDome()) : nullptr;
	const auto* sun = sky != nullptr ? registry.TryGet<const ecs::components::Sun>(sky->GetSun()) : nullptr;
	const auto* moon = sky != nullptr ? registry.TryGet<const ecs::components::Moon>(sky->GetMoon()) : nullptr;
	const auto& palettes = Locator::resources::value().GetLandLightPalettes();
	const LandLightPalette* palette =
	    palettes.Contains(LandLightPalette::k_Id.value()) ? &*palettes.Handle(LandLightPalette::k_Id.value()) : nullptr;
	const bool fog = detail_level::Fog(Locator::config::value().detailLevel);
	const LandLightInputs land = dome != nullptr ? dome->landLight : LandLightInputs {};
	const float overcast = dome != nullptr ? dome->overcast : 0.0f;
	LandLightTable table;
	const place_light::Inputs knotInputs {
	    .sunColour = (sun != nullptr ? sun->colour : glm::vec3(1.0f)) * underwater::k_SunLight,
	    .sunStrength =
	        [overcast, fog](float height) {
		        return sky_dome::ThroughOvercast(atmos::SunStrength(height), overcast, fog) / 255.0f;
	        },
	    .nightGlow = [](float height) { return atmos::NightGlow(height); },
	    .skyColour =
	        [&](float height) {
		        if (palette == nullptr)
		        {
			        return inputs.island;
		        }
		        table.Build(*palette, atmos::SkyTypeFromSun(height), land.alignment, land.overcast);
		        return table.GetHaze().colour / 255.0f;
	        },
	    .airTint = [&inputs](float height) { return AirLightTint(height, inputs.air).onScreen; },
	};
	const auto heightOf = [](int knot) {
		return std::sin(place_light::KnotDegrees(knot) * std::numbers::pi_v<float> / 180.0f);
	};
	std::array<place_light::Knot, place_light::k_Knots> knots {};
	for (int i = 0; i < place_light::k_Knots; ++i)
	{
		knots[static_cast<size_t>(i)] = place_light::KnotAt(heightOf(i), knotInputs);
	}
	// The air's brightness on the screen at each height, as the island's exposure finds it for its own light
	const auto scaleOf = [&](int knot) {
		const float height = heightOf(knot);
		const glm::vec3 sunThere = AtHeight(inputs.sunTowards, height);
		const float moonShare = atmos::AirMoonShare(height);
		const glm::vec3 light =
		    atmos::AirLightTowards(sunThere, atmos::AirMoonTowards(sunThere, inputs.moonTowards), moonShare);
		const glm::vec3 horizon = atmos::HorizonRadiance(light, inputs.altitude, inputs.air, inputs.horizonY);
		const glm::vec3 tint = glm::mix(glm::vec3(1.0f), k_MoonTint, moonShare);
		return atmos::ScreenScale(horizon, knots[static_cast<size_t>(knot)].sky, tint, atmos::k_OriginalHue) * k_HazeBrightness;
	};
	constexpr int k_ScalesPerFrame = 2;
	const int count = view.placeScaleReady ? k_ScalesPerFrame : place_light::k_Knots;
	for (int i = 0; i < count; ++i)
	{
		const int knot = view.nextPlaceScale;
		view.placeScale[static_cast<size_t>(knot)] = scaleOf(knot);
		view.nextPlaceScale = (knot + 1) % place_light::k_Knots;
	}
	view.placeScaleReady = true;
	// The island's haze has the tint of the land's colours, and the game's sky colour can differ from the palette's.
	// Each knot takes the same change, so the table gives the island's own light at the island's sun height.
	glm::vec3 skyRatio {1.0f};
	if (palette != nullptr)
	{
		const glm::vec3 tableSky = knotInputs.skyColour(inputs.sunTowards.y);
		skyRatio = glm::clamp(inputs.island / glm::max(tableSky, glm::vec3(1e-3f)), glm::vec3(0.5f), glm::vec3(2.0f));
	}
	for (size_t i = 0; i < knots.size(); ++i)
	{
		knots[i].airScale = view.placeScale[i] * inputs.placeTint;
		knots[i].sky *= skyRatio;
	}
	atmosphere.placeLight = place_light::Pack(knots);
	// The moon's light at full strength, with its phase; each place takes a share by the moon's height there
	if (moon != nullptr)
	{
		const float full =
		    sky_dome::ThroughOvercast(k_MoonFullAlpha, overcast, fog) / 255.0f * underwater::PhaseBrightness(moon->phase);
		atmosphere.placeMoon = {moon->colour * underwater::k_MoonLight, full};
	}
}
} // namespace

void Renderer::BeginAtmosphere(const DrawSceneDesc& desc) const
{
	_atmosphereFrame.reset();
	if (!_underwaterFrame.has_value() || !Locator::underwaterSystem::has_value() || !Locator::oceanSystem::has_value() ||
	    _shaderManager->GetShader("AtmosphereSkyView") == nullptr)
	{
		return;
	}
	using underwater::Effect;
	const auto& system = Locator::underwaterSystem::value();
	const auto& frame = *_underwaterFrame;
	if (_atmosphere == nullptr)
	{
		_atmosphere = std::make_unique<AtmosphereView>();
	}
	auto& view = *_atmosphere;
	const float seconds = system.GetSeconds();
	const float elapsed = view.lastSeconds.has_value() ? std::clamp(seconds - *view.lastSeconds, 0.0f, 0.25f) : 0.0f;
	view.lastSeconds = seconds;

	// The air: salt over the sea, more after rain and in a storm
	float rain = 0.0f;
	if (Locator::weatherSystem::has_value())
	{
		rain = static_cast<float>(Locator::weatherSystem::value().GetWeatherSmooth(desc.camera->GetOrigin()).rain) / 100.0f;
	}
	view.wetness = volumetric_fog::StepWetness(view.wetness, std::clamp(rain, 0.0f, 1.0f), elapsed);
	auto air = atmos::AirFor(system.GetWaves().storm, view.wetness);
	// The processor's table of the light that the air scatters more than once, again only when the dust has changed
	if (!view.multipleFor.has_value() || std::abs(*view.multipleFor - air.turbidity) > 0.05f)
	{
		view.multipleTable = atmos::MakeMultipleTable(air);
		view.multipleFor = air.turbidity;
	}
	air.multiple = view.multipleTable;
	const float seaLevel = frame.params.z;
	const glm::vec3 eye = frame.eye;
	// The world curves away past a flat cap that holds the map, and the sea's horizon goes down as the eye climbs. The
	// air works in the frame of the same ball, so from space the air's planet and the sea are one ball.
	const float mapSize = static_cast<float>(LandIslandInterface::k_MapCellsPerSide) * LandIslandInterface::k_CellSize;
	const auto cap = atmos::MapCap({eye.x, eye.z}, eye.y - seaLevel, mapSize, desc.camera->IsIslandFar());
	const auto airFrame = atmos::AirFrameFor(cap, eye.y - seaLevel);
	const float altitude = atmos::AltitudeOf(airFrame.height);
	const glm::vec3 gameLight =
	    glm::length(glm::vec3(frame.above)) > 0.0f ? glm::normalize(glm::vec3(frame.above)) : glm::vec3(0.0f, 1.0f, 0.0f);
	// The air's light comes from the sun on its real path (atmosphere::SunTowards). After sunset it moves to the moon
	// on its real path, or to the point opposite the sun while the moon is down.
	float hour = 12.0f;
	const ecs::components::Moon* moon = nullptr;
	if (Locator::skySystem::has_value())
	{
		const auto& sky = Locator::skySystem::value();
		hour = sky.GetClock().GetScriptTime();
		moon = Locator::entitiesRegistry::value().TryGet<const ecs::components::Moon>(sky.GetMoon());
	}
	// Far round the planet the sun, the moon and the magnetic pole are where they are over that place
	const glm::quat planetFrame = desc.camera->GetPlanetFrame();
	const glm::quat observer = glm::conjugate(planetFrame);
	const glm::vec3 sunTowards = observer * atmos::SunTowards(hour);
	const glm::vec3 moonTowards = moon != nullptr ? observer * sky_frame::MoonTowards(*moon, hour) : -sunTowards;
	const glm::vec3 sunFromEye = glm::normalize((sunTowards * atmos::k_SunDistance) - atmos::LightEye(eye, seaLevel));
	const float moonShare = atmos::AirMoonShare(sunTowards.y);
	const glm::vec3 towardsLight =
	    atmos::AirLightTowards(sunFromEye, atmos::AirMoonTowards(sunTowards, moonTowards), moonShare);

	// The sky's light on the processor, again only when the light, the air or the height has changed enough. Over
	// k_HighestExposure the exposure and the sky's light stay as they are there, so the sky goes dark as the eye climbs
	// to space and the bright limb does not change the exposure.
	const float skyAltitude = std::min(altitude, k_HighestExposure);
	const float skyHeight = std::min(eye.y - seaLevel, k_HighestExposure * atmos::k_UnitsPerKilometre);
	if (Stale(view.cache, towardsLight, air.turbidity, skyAltitude, seconds))
	{
		view.cache = AtmosphereView::SkyCache {
		    .towardsLight = towardsLight,
		    .turbidity = air.turbidity,
		    .altitude = skyAltitude,
		    .seconds = seconds,
		    .horizon = atmos::HorizonRadiance(towardsLight, skyAltitude, air, atmos::WorldHorizonY(cap, skyHeight)),
		    .sky = gi::SkySh(towardsLight, skyAltitude, air, k_SkySamples),
		};
	}
	const auto& cache = *view.cache;

	AtmosphereFrame atmosphere;
	atmosphere.haze = system.IsEffectOn(Effect::Haze);
	atmosphere.lighting = system.IsEffectOn(Effect::GlobalIllumination);
	atmosphere.hazeDetail = underwater::HazeDetail(system.GetQuality());
	atmosphere.lightDetail = underwater::LightingDetail(system.GetQuality());
	atmosphere.params = {air.turbidity, atmos::k_HazeScale, altitude, seaLevel};
	atmosphere.light = {towardsLight, atmosphere.haze ? 1.0f : 0.0f};
	atmosphere.airUp = {airFrame.up, 0.0f};
	atmosphere.skyViewLight = {atmos::TurnToUp(airFrame.up, towardsLight), atmosphere.light.w};
	atmosphere.sunHeight = sunTowards.y;
	// The place's own colour tints the haze: the land's cells round the point that the camera looks at
	{
		const auto& island = Locator::terrainSystem::value();
		const glm::vec3 focus = Locator::camera::has_value() ? Locator::camera::value().GetFocus() : desc.camera->GetFocus();
		constexpr int k_Reach = 3;
		constexpr float k_Step = 30.0f;
		std::vector<glm::vec3> colours;
		colours.reserve((2 * k_Reach + 1) * (2 * k_Reach + 1));
		for (int z = -k_Reach; z <= k_Reach; ++z)
		{
			for (int x = -k_Reach; x <= k_Reach; ++x)
			{
				const glm::vec2 at = glm::vec2(focus.x, focus.z) + glm::vec2(x, z) * k_Step;
				const auto cellX = static_cast<int>(at.x / LandIslandInterface::k_CellSize);
				const auto cellZ = static_cast<int>(at.y / LandIslandInterface::k_CellSize);
				if (cellX < 0 || cellZ < 0 || cellX >= LandIslandInterface::k_MapCellsPerSide ||
				    cellZ >= LandIslandInterface::k_MapCellsPerSide)
				{
					continue;
				}
				const auto& cell = island.GetCell(glm::u16vec2(cellX, cellZ));
				// The game keeps a cell's colour with red and blue swapped
				colours.emplace_back(glm::vec3(cell.b, cell.g, cell.r) / 255.0f);
			}
		}
		atmosphere.placeTint = atmos::PlaceTint(colours, atmos::k_PlaceShare);
	}
	// The air's horizon is as bright as the original game's haze, with some of its colour
	const glm::vec3 tint = glm::mix(glm::vec3(1.0f), k_MoonTint, moonShare);
	const glm::vec3 original = glm::vec3(frame.sky);
	atmosphere.sun = {atmos::ScreenScale(cache.horizon, original, tint, atmos::k_OriginalHue), 0.0f};
	atmosphere.sun = {glm::vec3(atmosphere.sun) * atmosphere.placeTint * k_HazeBrightness, 0.0f};
	atmosphere.horizon = {cache.horizon * glm::vec3(atmosphere.sun), 0.0f};
	// The clouds and the fog take the air's colour of the light, at the brightness that they have without the haze
	const auto airTint = AirLightTint(towardsLight.y, air);
	atmosphere.lightTint = airTint.tint;
	atmosphere.lightTintOnScreen = airTint.onScreen;
	atmosphere.lightAtGround = atmos::LightAtGround(towardsLight.y, air);
	// The game's own sun or moon in the sky, for its glare and for the light through the leaves
	atmosphere.sunDisc = {gameLight, frame.lightColour.w * glm::mix(1.0f, 0.35f, frame.light.w)};
	atmosphere.sunColour = {glm::vec3(frame.lightColour) * atmosphere.lightTint, 0.0f};
	atmosphere.sky = cache.sky;
	atmosphere.worldCurve = {eye.x, eye.z, cap.radius, atmos::k_WorldRadius};
	atmosphere.worldCap = {cap.eyeFromMiddle, airFrame.height, 0.0f};
	// The sky above the air and the aurora, round the middle of the world's ball. The aurora's night runs from noon to
	// noon, so that one night keeps one strength.
	const int night = (moon != nullptr ? moon->passedDays : 0) - (hour < 12.0f ? 1 : 0);
	// The noctilucent clouds of the night: the debug choice, or the night's own from the game's date
	float noctilucent = system.GetNoctilucentOverride().value_or(0.0f);
	if (!system.GetNoctilucentOverride().has_value() && moon != nullptr)
	{
		const int64_t date = moon->dateOverride.value_or(moon->date);
		const int32_t days = moon->passedDays - (hour < 12.0f ? 1 : 0);
		noctilucent = atmos::NoctilucentStrength((date / graphics::moon::k_SecondsPerDay) + days, atmos::DayOfYear(date, days));
	}
	atmosphere.airSun = {sunTowards, noctilucent};
	atmosphere.aurora = {observer * atmos::MagneticPole(), atmos::AuroraTonight(night, system.GetAuroraChoice())};
	const glm::mat3 turn = glm::mat3_cast(planetFrame);
	atmosphere.planetSun = {sunTowards, 0.0f};
	// The moon lights the night side of the ball with a share of the island's light, by its phase
	constexpr float k_MoonShare = 0.35f;
	atmosphere.planetMoon = {moonTowards, moon != nullptr ? k_MoonShare * underwater::PhaseBrightness(moon->phase) : 0.0f};
	PlaceLightTable(view, atmosphere,
	                {.air = air,
	                 .sunTowards = sunTowards,
	                 .moonTowards = moonTowards,
	                 .island = glm::vec3(frame.sky),
	                 .placeTint = atmosphere.placeTint,
	                 .altitude = skyAltitude,
	                 .horizonY = atmos::WorldHorizonY(cap, skyHeight)});
	atmosphere.worldTurn = {glm::vec4(glm::row(turn, 0), 0.0f), glm::vec4(glm::row(turn, 1), 0.0f),
	                        glm::vec4(glm::row(turn, 2), 0.0f)};
	// The stars, the Milky Way and the nebulae have fixed places on the sky's sphere, which turns with the hour
	const glm::mat3 celestial = atmos::CelestialFrame(hour) * turn;
	atmosphere.celestial = {glm::vec4(glm::row(celestial, 0), 0.0f), glm::vec4(glm::row(celestial, 1), 0.0f),
	                        glm::vec4(glm::row(celestial, 2), 0.0f)};
	atmosphere.airPlanet = {eye.x - cap.eyeFromMiddle.x, seaLevel - atmos::k_WorldRadius, eye.z - cap.eyeFromMiddle.y, seconds};
	if (moon != nullptr && moon->placement.has_value())
	{
		const float radius = real_moon::Radius(atmos::k_MoonDistance) / atmos::k_MoonDistance;
		atmosphere.moonDisc = {glm::normalize(moon->placement->offset), std::cos(radius)};
	}
	// The sky's light from above: the mean of the air's upper sky in the colours that the dome shows. It is blue by day
	// and blue-grey at dusk, because the sky high up does not have the red of the horizon.
	constexpr glm::vec3 k_Up {0.0f, 1.0f, 0.0f};
	atmosphere.skyFromAbove = atmos::DimSkyColour(
	    gi::ShIrradiance(cache.sky, k_Up) / std::numbers::pi_v<float> * glm::vec3(atmosphere.sun), atmosphere.sunHeight);
	// The sky's dome: its middle across the land, its half width and its height. The shaders take it as a half ball
	// of this size; a point near its skin is on the dome (sky_dome.sh)
	if (Locator::skySystem::has_value())
	{
		const auto* dome =
		    Locator::entitiesRegistry::value().TryGet<const ecs::components::SkyDome>(Locator::skySystem::value().GetDome());
		const auto* mesh = dome != nullptr ? Locator::resources::value().GetMeshes().Find(dome->meshId) : nullptr;
		if (mesh != nullptr)
		{
			const auto box = mesh->GetBoundingBox();
			const float wide = 0.5f * std::min(box.Size().x, box.Size().z) * sky_dome::k_Scale;
			const float high = box.maxima.y * sky_dome::k_Scale;
			atmosphere.skyDome = {sky_dome::k_Centre.x, sky_dome::k_Centre.z, wide, high};
		}
	}
	const auto medium = atmos::MediumAt(altitude, air);
	atmosphere.lowTierDensity = atmos::Luminance(medium.extinction) * atmos::HazeScaleAt(altitude) / atmos::k_UnitsPerKilometre;
	_atmosphereFrame = atmosphere;

	const auto setUniforms = [&atmosphere](const ShaderProgram& program) {
		program.SetUniformValue("u_atmosParams", &atmosphere.params);
		program.SetUniformValue("u_atmosLight", &atmosphere.light);
		program.SetUniformValue("u_atmosSun", &atmosphere.sun);
	};
	// The texture of the light that gets through, when the air's dust has changed
	const auto* transmittanceProgram = _shaderManager->GetShader("AtmosphereTransmittance");
	const auto* skyProgram = _shaderManager->GetShader("AtmosphereSkyView");
	const auto* multipleProgram = _shaderManager->GetShader("AtmosphereMultiple");
	if (transmittanceProgram != nullptr &&
	    (!view.transmittanceFor.has_value() || std::abs(*view.transmittanceFor - air.turbidity) > 0.05f))
	{
		const auto pass = static_cast<bgfx::ViewId>(RenderPass::AtmosphereTransmittance);
		bgfx::setViewFrameBuffer(pass, view.transmittance.Target());
		bgfx::setViewRect(pass, 0, 0, view.transmittance.Width(), view.transmittance.Height());
		bgfx::setViewClear(pass, BGFX_CLEAR_NONE);
		setUniforms(*transmittanceProgram);
		const glm::vec4 target {view.transmittance.Width(), view.transmittance.Height(), 0.0f, 0.0f};
		transmittanceProgram->SetUniformValue("u_atmosTarget", &target);
		BindScreenTriangle();
		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
		transmittanceProgram->Submit(pass);
		view.transmittanceFor = air.turbidity;
		view.transmittance.drawn = true;
		// The table of the light that the air scatters more than once reads the new texture of the light that gets through
		if (multipleProgram != nullptr)
		{
			const auto multiplePass = static_cast<bgfx::ViewId>(RenderPass::AtmosphereMultiple);
			bgfx::setViewFrameBuffer(multiplePass, view.multiple.Target());
			bgfx::setViewRect(multiplePass, 0, 0, view.multiple.Width(), view.multiple.Height());
			bgfx::setViewClear(multiplePass, BGFX_CLEAR_NONE);
			setUniforms(*multipleProgram);
			multipleProgram->SetTextureSampler("s_atmosTransmittance", 13, view.transmittance.Texture());
			const glm::vec4 multipleTarget {view.multiple.Width(), view.multiple.Height(), 0.0f, 0.0f};
			multipleProgram->SetUniformValue("u_atmosTarget", &multipleTarget);
			BindScreenTriangle();
			bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
			multipleProgram->Submit(multiplePass);
			view.multiple.drawn = true;
		}
	}
	// The sky round the eye, each frame
	{
		const auto pass = static_cast<bgfx::ViewId>(RenderPass::AtmosphereSkyView);
		bgfx::setViewFrameBuffer(pass, view.skyView.Target());
		bgfx::setViewRect(pass, 0, 0, view.skyView.Width(), view.skyView.Height());
		bgfx::setViewClear(pass, BGFX_CLEAR_NONE);
		setUniforms(*skyProgram);
		// The sky round the eye is in the air's own frame, with the light turned into it
		skyProgram->SetUniformValue("u_atmosLight", &atmosphere.skyViewLight);
		skyProgram->SetTextureSampler("s_atmosTransmittance", 13, view.transmittance.Texture());
		skyProgram->SetTextureSampler("s_atmosMultiple", 15, view.multiple.Texture());
		const glm::vec4 target {view.skyView.Width(), view.skyView.Height(), 0.0f, 0.0f};
		skyProgram->SetUniformValue("u_atmosTarget", &target);
		BindScreenTriangle();
		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
		skyProgram->Submit(pass);
		view.skyView.drawn = true;
	}
	// The aurora at half the screen's size, before the sky's pass reads it. With no aurora tonight the pass only clears
	// its target.
	if (const auto* auroraProgram = _shaderManager->GetShader("Aurora"))
	{
		const auto* stats = bgfx::getStats();
		const glm::u16vec2 half {static_cast<uint16_t>((stats->width + 1) / 2), static_cast<uint16_t>((stats->height + 1) / 2)};
		auto& target = view.Aurora(half);
		const auto pass = static_cast<bgfx::ViewId>(RenderPass::Aurora);
		bgfx::setViewFrameBuffer(pass, target.Target());
		bgfx::setViewRect(pass, 0, 0, half.x, half.y);
		bgfx::setViewClear(pass, BGFX_CLEAR_COLOR, 0x000000ffu);
		if (atmosphere.aurora.w > 0.01f && atmosphere.haze && !system.IsAuroraFullSize())
		{
			_shaderManager->SetCamera(RenderPass::Aurora, *desc.camera);
			setUniforms(*auroraProgram);
			BindAirSky(*auroraProgram);
			auroraProgram->SetTextureSampler("s_atmosTransmittance", 13, view.transmittance.Texture());
			BindScreenTriangle();
			bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
			auroraProgram->Submit(pass);
		}
		else
		{
			bgfx::touch(pass);
		}
		target.drawn = true;
	}
}

void Renderer::BindAtmosphere(const ShaderProgram& program, float skyShare, bool screenAurora) const
{
	const bool ready =
	    _atmosphereFrame.has_value() && _atmosphere != nullptr && _atmosphere->skyView.drawn && _atmosphereFrame->haze;
	const float sunHeight = _atmosphereFrame.has_value() ? _atmosphereFrame->sunHeight : 1.0f;
	// The view on the screen reads the aurora's pass; the faces of the far cube march the aurora themselves, with fewer
	// steps. For a debug comparison the view on the screen can march the aurora's full march itself.
	const bool fullSize = Locator::underwaterSystem::has_value() && Locator::underwaterSystem::value().IsAuroraFullSize();
	const auto* aurora = ready ? _atmosphere->Aurora() : nullptr;
	const bool readsAurora = screenAurora && !fullSize && aurora != nullptr && aurora->drawn;
	// The full march for the comparison has no probes (a step count under 0), as the march before the pass had none
	const float marchSteps = screenAurora ? -atmos::k_AuroraSteps : atmos::k_AuroraCubeSteps;
	const glm::vec4 share {ready ? skyShare : 0.0f, sunHeight, readsAurora ? 1.0f : 0.0f, marchSteps};
	if (readsAurora && program.HasUniform("s_airAurora"))
	{
		program.SetTextureSampler("s_airAurora", 12, aurora->Texture());
	}
	program.SetUniformValue("u_skyAtmos", &share);
	if (!ready)
	{
		return;
	}
	const auto& atmosphere = *_atmosphereFrame;
	program.SetUniformValue("u_atmosParams", &atmosphere.params);
	program.SetUniformValue("u_atmosLight", &atmosphere.light);
	program.SetUniformValue("u_atmosSun", &atmosphere.sun);
	if (program.HasUniform("u_atmosUp"))
	{
		program.SetUniformValue("u_atmosUp", &atmosphere.airUp);
	}
	BindAirSky(program);
	program.SetTextureSampler("s_atmosTransmittance", 13, _atmosphere->transmittance.Texture());
	program.SetTextureSampler("s_atmosSkyView", 14, _atmosphere->skyView.Texture());
}

bool Renderer::BindHazeGrid(const ShaderProgram& program, uint8_t stage) const
{
	if (!_atmosphereFrame.has_value() || _atmosphere == nullptr || !_atmosphereFrame->haze ||
	    _atmosphereFrame->hazeDetail == underwater::LightDetail::Basic || !_underwaterFrame.has_value() ||
	    _underwaterFrame->eye.w > 0.5f)
	{
		return false;
	}
	program.SetTextureSampler("s_atmosAerial", stage, _atmosphere->aerial.Texture());
	return true;
}

void Renderer::DrawHaze(const DrawSceneDesc& desc, const FrameBuffer& sceneTarget) const
{
	if (!_atmosphereFrame.has_value() || _atmosphere == nullptr || !_atmosphereFrame->haze || !_underwaterFrame.has_value())
	{
		return;
	}
	// Under the water the post pass has the water's own fog
	if (_underwaterFrame->eye.w > 0.5f)
	{
		return;
	}
	const auto* aerialProgram = _shaderManager->GetShader("AtmosphereAerial");
	const auto* applyProgram = _shaderManager->GetShader("HazeApply");
	if (aerialProgram == nullptr || applyProgram == nullptr)
	{
		return;
	}
	auto& view = *_atmosphere;
	const auto& atmosphere = *_atmosphereFrame;
	const bool grid = atmosphere.hazeDetail != underwater::LightDetail::Basic;
	const auto setUniforms = [&atmosphere](const ShaderProgram& program) {
		program.SetUniformValue("u_atmosParams", &atmosphere.params);
		program.SetUniformValue("u_atmosLight", &atmosphere.light);
		program.SetUniformValue("u_atmosSun", &atmosphere.sun);
		if (program.HasUniform("u_atmosUp"))
		{
			program.SetUniformValue("u_atmosUp", &atmosphere.airUp);
		}
	};

	// The haze's grid that follows the view
	if (grid)
	{
		const bool full = atmosphere.hazeDetail == underwater::LightDetail::Full;
		const auto pass = static_cast<bgfx::ViewId>(RenderPass::AtmosphereAerial);
		bgfx::setViewFrameBuffer(pass, view.aerial.Target());
		bgfx::setViewRect(pass, 0, 0, view.aerial.Width(), view.aerial.Height());
		bgfx::setViewClear(pass, BGFX_CLEAR_NONE);
		_shaderManager->SetCamera(RenderPass::AtmosphereAerial, *desc.camera);
		setUniforms(*aerialProgram);
		aerialProgram->SetTextureSampler("s_atmosTransmittance", 13, view.transmittance.Texture());
		aerialProgram->SetTextureSampler("s_atmosMultiple", 15, view.multiple.Texture());
		BindCloudShadow(*aerialProgram, 11);
		BindSunVisibility(*aerialProgram, 12);
		const glm::vec4 aerial {full ? 1.0f : 0.0f, full ? k_FullSteps : k_ReducedSteps, 0.0f, 0.0f};
		aerialProgram->SetUniformValue("u_atmosAerial", &aerial);
		aerialProgram->SetUniformValue("u_atmosGameLight", &atmosphere.sunDisc);
		SetWorldCurve(*aerialProgram);
		BindScreenTriangle();
		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
		aerialProgram->Submit(pass);
		view.aerial.drawn = true;
	}

	// The haze over the scene and the water, first of what blends over them. The clouds come after it and fade with
	// their own distance, so a cloud in front of the far sea gets no haze of the sea's distance.
	setUniforms(*applyProgram);
	if (grid)
	{
		applyProgram->SetTextureSampler("s_atmosAerial", 0, view.aerial.Texture());
	}
	applyProgram->SetTextureSampler("s_sceneDepth", 1, sceneTarget.GetDepthStencilAttachment());
	if (grid)
	{
		// The scene far away fades into the sky that the dome shows at the horizon
		applyProgram->SetTextureSampler("s_atmosSkyView", 14, view.skyView.Texture());
	}
	// From outside the sky's dome, as from space, the dome is not drawn and this pass draws all of the sky
	const auto& domeSize = atmosphere.skyDome;
	const glm::vec3 eye = desc.camera->GetOrigin();
	const glm::vec3 scaled =
	    domeSize.z > 0.0f ? glm::vec3 {(eye.x - domeSize.x) / domeSize.z, eye.y / domeSize.w, (eye.z - domeSize.y) / domeSize.z}
	                      : glm::vec3(0.0f);
	const bool outsideDome = glm::dot(scaled, scaled) >= 0.7f * 0.7f;
	const glm::vec4 apply {grid ? 1.0f : 0.0f, atmosphere.lowTierDensity, atmosphere.sunHeight, outsideDome ? 1.0f : 0.0f};
	applyProgram->SetUniformValue("u_hazeApply", &apply);
	// For a debug comparison the haze can march the aurora's full march itself (w of the horizon's colour)
	const bool fullSize = Locator::underwaterSystem::value().IsAuroraFullSize();
	const glm::vec4 horizon {glm::vec3(atmosphere.horizon), fullSize ? 1.0f : 0.0f};
	applyProgram->SetUniformValue("u_hazeHorizon", &horizon);
	applyProgram->SetUniformValue("u_hazeDome", &atmosphere.skyDome);
	applyProgram->SetUniformValue("u_hazeSun", &atmosphere.sunDisc);
	applyProgram->SetUniformValue("u_hazeSunColour", &atmosphere.sunColour);
	SetWorldCurve(*applyProgram);
	BindAirSky(*applyProgram);
	applyProgram->SetUniformValue("u_hazeMoon", &atmosphere.moonDisc);
	if (const auto* aurora = view.Aurora(); aurora != nullptr && aurora->drawn)
	{
		applyProgram->SetTextureSampler("s_airAurora", 12, aurora->Texture());
	}
	applyProgram->SetTextureSampler("s_atmosTransmittance", 13, _atmosphere->transmittance.Texture());
	applyProgram->SetTextureSampler("s_atmosSkyView", 14, view.skyView.Texture());
	BindScreenTriangle();
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_SRC_ALPHA));
	applyProgram->Submit(static_cast<bgfx::ViewId>(RenderPass::UnderwaterOver), std::numeric_limits<uint32_t>::max());
}

void Renderer::BindFoliageLight(const ShaderProgram& program) const
{
	const bool on = _atmosphereFrame.has_value() && _atmosphereFrame->lighting;
	const glm::vec4 light = on ? _atmosphereFrame->sunDisc : glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
	program.SetUniformValue("u_foliageLight", &light);
	if (on)
	{
		program.SetUniformValue("u_foliageLightColour", &_atmosphereFrame->sunColour);
	}
}

void Renderer::SetWorldCurve(const ShaderProgram& program) const
{
	if (!program.HasUniform("u_worldCurve"))
	{
		return;
	}
	const glm::vec4 flat {0.0f};
	program.SetUniformValue("u_worldCurve", _atmosphereFrame.has_value() ? &_atmosphereFrame->worldCurve : &flat);
	if (program.HasUniform("u_worldCap"))
	{
		program.SetUniformValue("u_worldCap", _atmosphereFrame.has_value() ? &_atmosphereFrame->worldCap : &flat);
	}
	if (program.HasUniform("u_planetSun"))
	{
		const glm::vec4 up {0.0f, 1.0f, 0.0f, 0.0f};
		program.SetUniformValue("u_planetSun", _atmosphereFrame.has_value() ? &_atmosphereFrame->planetSun : &up);
	}
	if (program.HasUniform("u_planetMoon"))
	{
		const glm::vec4 none {0.0f, -1.0f, 0.0f, 0.0f};
		program.SetUniformValue("u_planetMoon", _atmosphereFrame.has_value() ? &_atmosphereFrame->planetMoon : &none);
	}
	if (program.HasUniform("u_placeLight"))
	{
		static constexpr std::array<glm::vec4, place_light::k_Knots * place_light::k_VectorsPerKnot> k_None {};
		const auto& table = _atmosphereFrame.has_value() ? _atmosphereFrame->placeLight : k_None;
		program.SetUniformArray("u_placeLight", table.data(), static_cast<uint16_t>(table.size()));
		const glm::vec4 noMoon {0.0f};
		program.SetUniformValue("u_placeMoon", _atmosphereFrame.has_value() ? &_atmosphereFrame->placeMoon : &noMoon);
	}
	if (program.HasUniform("u_worldTurn"))
	{
		static constexpr std::array<glm::vec4, 3> k_Same {glm::vec4(1.0f, 0.0f, 0.0f, 0.0f), glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
		                                                  glm::vec4(0.0f, 0.0f, 1.0f, 0.0f)};
		const auto& turn = _atmosphereFrame.has_value() ? _atmosphereFrame->worldTurn : k_Same;
		program.SetUniformArray("u_worldTurn", turn.data(), static_cast<uint16_t>(turn.size()));
	}
	if (program.HasUniform("u_celestial"))
	{
		static constexpr std::array<glm::vec4, 3> k_Same {glm::vec4(1.0f, 0.0f, 0.0f, 0.0f), glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
		                                                  glm::vec4(0.0f, 0.0f, 1.0f, 0.0f)};
		const auto& celestial = _atmosphereFrame.has_value() ? _atmosphereFrame->celestial : k_Same;
		program.SetUniformArray("u_celestial", celestial.data(), static_cast<uint16_t>(celestial.size()));
	}
}

void Renderer::BindAirSky(const ShaderProgram& program) const
{
	if (!_atmosphereFrame.has_value() || !program.HasUniform("u_airSun"))
	{
		return;
	}
	program.SetUniformValue("u_airSun", &_atmosphereFrame->airSun);
	program.SetUniformValue("u_aurora", &_atmosphereFrame->aurora);
	program.SetUniformValue("u_airPlanet", &_atmosphereFrame->airPlanet);
	SetWorldCurve(program);
}
