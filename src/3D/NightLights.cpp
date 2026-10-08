/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "NightLights.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <bit>

#include <LNDFile.h>
#include <entt/entity/entity.hpp>
#include <spdlog/spdlog.h>

#include "3D/NightLightsState.h"
#include "Audio/Services/LanternSounds.h"
#include "Common/GameRandom.h"
#include "DayNightClock.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/VillageLightSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "FrameAnim.h"
#include "GameClock.h"
#include "Graphics/ArgbColour.h"
#include "Graphics/Texture2D.h"
#include "LandIslandInterface.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;

uint32_t night_lights::WindowColour(const DayNightClock& clock, const glm::vec3& position, bool someoneHome)
{
	// villagers at home, and the visual night
	if (!someoneHome || !clock.IsVisualNight())
	{
		return 0; // lit normally
	}
	const float f = std::abs(position.x + position.z) * 0.1f + position.y;
	const float t = clock.GetVisualTime() + (f - std::trunc(f));
	int32_t intensity = 0;
	if (t > 20.5f)
	{
		intensity = static_cast<int32_t>((t - 20.5f) * 8192.0f);
	}
	else if (t < 3.0f)
	{
		intensity = static_cast<int32_t>((3.0f - t) * 8192.0f);
	}
	if (intensity <= 0)
	{
		return 0; // lit normally
	}
	static constexpr std::array<int32_t, 8> k_Flicker = {0, 7, 3, 5, 4, 2, 6, 1};
	const auto v = static_cast<uint32_t>(((k_Flicker[static_cast<int32_t>(t * 1000.0f) & 7] << 2) & 0x1F) | 0xE0);
	// the grey in r, g and b with alpha 0xFF; below 256 the intensity scales it, (g k) >> 8 (the same product per
	// channel as argb_colour::ScaleRgbShift8KeepAlpha)
	const uint32_t colour = argb_colour::Argb(v, v, v, 0xFF);
	return intensity < 256 ? argb_colour::ScaleRgbShift8KeepAlpha(colour, static_cast<uint32_t>(intensity)) : colour;
}

float night_lights::VillageLightIntensity(float t)
{
	constexpr float k_On = 16.5f;
	constexpr float k_Off = 7.0f;
	constexpr float k_Ramp = 1.0f;
	if (t > k_On)
	{
		return k_Ramp + k_On < t ? 255.0f : (t - k_On) * 255.0f / k_Ramp;
	}
	if (t > k_Off)
	{
		return 0.0f;
	}
	return k_Off - k_Ramp > t ? 255.0f : (k_Off - t) * 255.0f / k_Ramp;
}

night_lights::LightImage night_lights::LoadLightImage(const std::vector<uint8_t>& raw)
{
	LightImage image;
	image.side = static_cast<int>(std::lround(std::sqrt(static_cast<double>(raw.size()))));
	if (image.side * image.side != static_cast<int>(raw.size()))
	{
		image.side = 0;
		return image;
	}
	image.texels.resize(raw.size());
	std::transform(raw.begin(), raw.end(), image.texels.begin(), [](uint8_t v) {
		return static_cast<uint8_t>(std::min(47, static_cast<int>(static_cast<float>(v) * 48.0f / 255.0f + 0.5f)));
	});
	return image;
}

namespace
{
/// The NightLights state (Locator::villageLightSystem)
openblack::night_lights::State& NightLightsState()
{
	return openblack::Locator::villageLightSystem::value().GetState();
}

using night_lights::VillageLight;

const std::vector<uint8_t>& Luminosity(const night_lights::LightCells& cells)
{
	auto& luminosity = NightLightsState().luminosity;
	if (luminosity.firstCell == cells.firstCell && luminosity.size == cells.size && !luminosity.values.empty())
	{
		return luminosity.values;
	}
	luminosity.firstCell = cells.firstCell;
	luminosity.size = cells.size;
	luminosity.values.assign(static_cast<size_t>(cells.size.x) * cells.size.y, 0);
	if (!Locator::terrainSystem::has_value())
	{
		return luminosity.values;
	}
	const auto& island = Locator::terrainSystem::value();
	const int side = island.GetCellsPerSide();
	for (int z = 0; z < cells.size.y; ++z)
	{
		for (int x = 0; x < cells.size.x; ++x)
		{
			const glm::ivec2 cell = cells.firstCell + glm::ivec2(x, z);
			if (cell.x < 0 || cell.y < 0 || cell.x >= side || cell.y >= side)
			{
				continue;
			}
			luminosity.values[static_cast<size_t>(z) * cells.size.x + x] =
			    island.GetCell(glm::u16vec2(static_cast<uint16_t>(cell.x), static_cast<uint16_t>(cell.y))).luminosity;
		}
	}
	return luminosity.values;
}
} // namespace

void night_lights::StampLight(const LightImage& image, float x, float z, float intensity, LightCells& cells)
{
	if (intensity <= 0.0f || image.side < 2 || cells.cap == nullptr)
	{
		return;
	}
	const auto& luminosity = Luminosity(cells);
	const int side = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetCellsPerSide() : 512;
	const int n = image.side;

	// Cell and sub-cell weight 255 * (1 - fraction) along each axis, truncated towards zero
	const auto split = [](float f, int& index) {
		index = static_cast<int>(f);
		if (f >= 0.0f)
		{
			return static_cast<int>(255.0f - (f - static_cast<float>(index)) * 255.0f);
		}
		const int weight = static_cast<int>((static_cast<float>(index) - f) * 255.0f);
		--index;
		return weight;
	};
	int ix = 0;
	int iz = 0;
	const int wz = split(z * 0.1f, iz) & 0xFF;
	const int wx = split(x * 0.1f, ix) & 0xFF;

	int start = 0;
	int nx = n;
	int nz = n;
	if (ix > side - n)
	{
		if (ix >= side)
		{
			return;
		}
		nx = side - ix;
	}
	else if (ix < 0)
	{
		start -= ix * n;
		nx = ix + n;
		ix = 0;
	}
	if (iz > side - n)
	{
		if (iz >= side)
		{
			return;
		}
		nz = side - iz;
	}
	else if (iz < 0)
	{
		start -= iz;
		nz = iz + n;
		iz = 0;
	}

	const int strength = static_cast<int>(intensity * 255.0f);
	const int k = (static_cast<int>(cells.fullLightGreen) * 3 * 16) >> 8;
	const auto& texels = image.texels;
	for (int i = 0; i < nx - 1; ++i)
	{
		for (int j = 0; j < nz - 1; ++j)
		{
			const int cx = ix + i - cells.firstCell.x;
			const int cz = iz + j - cells.firstCell.y;
			if (cx < 0 || cz < 0 || cx >= cells.size.x || cz >= cells.size.y)
			{
				continue;
			}
			const auto cellIndex = static_cast<size_t>(cz) * cells.size.x + cx;
			const int own = luminosity[cellIndex];
			if (own == 0)
			{
				continue; // no block
			}
			// Image rows run along x, bytes along z
			const size_t q = static_cast<size_t>(start + i * n + j);
			const int b = texels[q];
			const int a = texels[q + 1];
			const int d = texels[q + n];
			const int c = texels[q + n + 1];
			const int r1 = b + (((a - b) * wz) >> 8);
			const int r2 = d + (((c - d) * wz) >> 8);
			const int r = r1 + (((r2 - r1) * wx) >> 8);
			const int v = (r * strength) / 255;
			auto& cap = (*cells.cap)[cellIndex];
			const int lum = std::min(own, static_cast<int>(cap));
			if (v <= ((lum * k) >> 8))
			{
				continue;
			}
			// replaces the lit land (>= 48), keeps the brighter of two lights
			if (lum >= 48 || lum < v)
			{
				cap = static_cast<uint8_t>(v);
			}
		}
	}
}

namespace
{
constexpr float k_JitterMs = 30.0f;
constexpr int k_GlowCell = 56; // (flags & ~7) | 0x38
constexpr glm::vec3 k_LightColour {0xF3 / 255.0f, 0x84 / 255.0f, 0x21 / 255.0f};

/// Every draw here is the CRT's random
using game_random::crt::Random;

const std::vector<uint8_t>& ReadTexture(const char* name)
{
	auto& fileSystem = Locator::filesystem::value();
	return resources::LoadBlob(Locator::resources::value().GetBlobs(),
	                           fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Textures>() / name));
}

graphics::TextureHandle Texture(const char* id, const char* file)
{
	auto& textures = Locator::resources::value().GetTextures();
	const auto key = entt::hashed_string(id);
	if (!textures.Contains(key))
	{
		auto& fileSystem = Locator::filesystem::value();
		textures.Load(key, resources::Texture2DLoader::FromDiskTag {},
		              fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Textures>() / file));
	}
	return textures.Handle(key)->GetNativeHandle();
}

void Load()
{
	auto& state = NightLightsState();
	state.loaded = true;
	try
	{
		state.hand = night_lights::LoadLightImage(ReadTexture("light_hand.raw"));
		state.village = night_lights::LoadLightImage(ReadTexture("village_diffuse.raw"));
		state.fire = Texture("raw/S_Firea", "S_Firea.raw");
		state.glow = Texture("raw/smokea", "smokea.raw");
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Night lights: {}", e.what());
	}
}

void DestroySprites(VillageLight& light)
{
	auto& registry = Locator::entitiesRegistry::value();
	for (auto& sprite : light.sprites)
	{
		if (sprite != entt::null && registry.Valid(sprite))
		{
			registry.Destroy(sprite);
		}
		sprite = entt::null;
	}
}

/// The lights of the street lanterns (type 0 town, 1 country) and of the Norse Gate's lamps (type 0), at the object's
/// point. Keyed on the light, not on the mesh: a Bonfire has the campfire mesh and no light.
void Rescan()
{
	auto& state = NightLightsState();
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<std::pair<glm::vec3, int>> found;
	registry.Each<const LanternLight, const Transform>(
	    [&](entt::entity /*unused*/, const LanternLight& light, const Transform& transform) {
		    found.emplace_back(transform.position, static_cast<int>(light.type));
	    },
	    entt::exclude<Unavailable>);
	const bool same = found.size() == state.lights.size() &&
	                  std::equal(found.begin(), found.end(), state.lights.begin(), [](const auto& f, const VillageLight& l) {
		                  return f.first == l.position && f.second == l.type;
	                  });
	if (same)
	{
		return;
	}
	for (auto& light : state.lights)
	{
		DestroySprites(light);
	}
	state.lights.clear();
	for (const auto& [position, type] : found)
	{
		VillageLight light {
		    .position = position,
		    .type = type,
		};
		// the jitter clock starts at Random(0, 30)
		light.timer = Random(0.0f, k_JitterMs);
		const glm::vec3 at = position + glm::vec3(0.0f, type == 1 ? 1.0f : 5.0f, 0.0f);
		for (size_t i = 0; i < light.sprites.size(); ++i)
		{
			const bool flame = i < 2;
			// a flame's size Random(-0.1, 0.1) + 1, at least 1e-4, drawn before the sprite's start
			if (flame)
			{
				light.flameSize[i] = std::max(Random(-0.1f, 0.1f) + 1.0f, 1e-4f);
			}
			// every sprite, the glow too, rewrites its entry of the global start table
			state.flameStarts.at(i) = graphics::frame_anim::LanternStart(Random(0.0f, 31.0f));
			const auto sprite = registry.Create();
			// the flames start at cell 0 ((flags & ~0x3F)); the glow is smoke.raw cell 56 ((flags & ~7) | 0x38)
			registry.Assign<Sprite>(sprite, flame ? state.fire : state.glow,
			                        graphics::frame_anim::SpriteCellUv(flame ? 0 : k_GlowCell)[0], glm::vec2(1.0f / 8.0f),
			                        glm::vec4(0.0f), true);
			registry.Assign<Transform>(sprite, at, glm::mat3(1.0f), glm::vec3(flame ? light.flameSize[i] : light.glowSize));
			light.sprites[i] = sprite;
		}
		state.lights.push_back(light);
	}
}
} // namespace

void night_lights::Update(float milliseconds, float scriptHour, const glm::vec3& baseColour, LightCells& cells)
{
	auto& state = NightLightsState();
	if (!state.loaded)
	{
		Load();
	}
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	state.rescanMs -= milliseconds;
	if (state.rescanMs <= 0.0f || state.lights.empty())
	{
		state.rescanMs = 1000.0f;
		Rescan();
	}

	// dark when the mean of the light-table base colour is under 120
	const float mean = (std::floor(baseColour.r * 255.0f + 0.5f) + std::floor(baseColour.g * 255.0f + 0.5f) +
	                    std::floor(baseColour.b * 255.0f + 0.5f)) /
	                   3.0f;
	// the lanterns' looping sample is heard only while it is dark
	audio::lantern_sounds::SetOn(mean < 120.0f);
	float villageAlpha = 0.0f;
	if (mean < 120.0f)
	{
		const float k = std::clamp((120.0f - mean) / 15.0f, 0.0f, 1.0f);
		const float intensity = VillageLightIntensity(scriptHour);
		villageAlpha = intensity * 0.5f;

		// the hand light (light_hand.raw at hand - 55), not while the hand is hidden
		if (Locator::handSystem::has_value())
		{
			const auto hand = Locator::handSystem::value().GetPlayerHands()[0];
			const auto& registry = Locator::entitiesRegistry::value();
			if (registry.Valid(hand))
			{
				const auto& position = registry.Get<const Transform>(hand).position;
				if (position != glm::vec3(0.0f))
				{
					StampLight(state.hand, position.x - 55.0f, position.z - 55.0f, k, cells);
				}
			}
		}
		// village_diffuse.raw from 50 units before the light, jittered
		for (const auto& light : state.lights)
		{
			StampLight(state.village, light.position.x - 50.0f + light.offset.x, light.position.z - 50.0f + light.offset.y,
			           intensity / 255.0f, cells);
		}
	}

	// jitter every 30 ms, the flames play backwards over 700 ms. The clock only runs while the village light alpha is
	// not 0 and there are lights (frame_anim::LanternAdvance, the game frame time in whole milliseconds:
	// game_clock::FrameGameMs, 0 while paused)
	const uint32_t wholeMs = game_clock::FrameGameMs();
	const bool running = villageAlpha != 0.0f && !state.lights.empty();
	const int a = graphics::frame_anim::LanternAdvance(state.flameMs, running ? wholeMs : 0u);
	const float alpha = std::trunc(villageAlpha) / 255.0f;
	auto& registry = Locator::entitiesRegistry::value();
	// the original walks its light list from the head, and puts each new light at the head: the newest first
	for (auto it = state.lights.rbegin(); it != state.lights.rend(); ++it)
	{
		auto& light = *it;
		// timer += the frame time (whole ms); above 30, -= 30 truncated(timer x (1/30)), then CRT Random(-0.5, 0.5)
		// for the x and z offsets, and the glow's size Random(-0.1, 0.1) + 3, at least 1e-4
		const float elapsed = static_cast<float>(wholeMs);
		light.timer = elapsed + light.timer;
		if (light.timer > k_JitterMs)
		{
			const float scaled = light.timer * std::bit_cast<float>(0x3D088889u);
			const float whole = static_cast<float>(static_cast<int32_t>(scaled));
			const float wrap = whole * k_JitterMs;
			light.timer = light.timer - wrap;
			const float x = Random(-0.5f, 0.5f);
			const float z = Random(-0.5f, 0.5f);
			light.offset = glm::vec2(x, z);
			light.glowSize = std::max(Random(-0.1f, 0.1f) + 3.0f, 1e-4f);
		}
		for (size_t i = 0; i < light.sprites.size(); ++i)
		{
			if (light.sprites[i] == entt::null || !registry.Valid(light.sprites[i]))
			{
				continue;
			}
			auto& sprite = registry.Get<Sprite>(light.sprites[i]);
			sprite.tint = glm::vec4(k_LightColour, alpha);
			if (i < 2)
			{
				// the start of flame i from the global table
				sprite.uvMin = graphics::frame_anim::SpriteCellUv(
				    graphics::frame_anim::LanternCell(a, static_cast<int>(i), state.flameStarts))[0];
			}
			else
			{
				registry.Get<Transform>(light.sprites[i]).scale = glm::vec3(light.glowSize);
			}
		}
	}
}

void night_lights::Clear()
{
	auto& state = NightLightsState();
	if (Locator::entitiesRegistry::has_value())
	{
		for (auto& light : state.lights)
		{
			DestroySprites(light);
		}
	}
	state.lights.clear();
	state.luminosity = {};
}
