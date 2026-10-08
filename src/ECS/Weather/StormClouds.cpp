/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "StormClouds.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <unordered_map>
#include <vector>

#include <glm/mat4x4.hpp>
#include <spdlog/spdlog.h>

#include "3D/FrameAnim.h"
#include "3D/LandLight.h"
#include "3D/LandLightTable.h"
#include "Camera/Camera.h"
#include "Common/GameRandom.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/Weather/WeatherState.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/DetailLevel.h"
#include "Graphics/Haze.h"
#include "Graphics/Mists.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "Storms.h"
#include "WeatherLand.h"

using namespace openblack;
using namespace openblack::weather;

namespace
{
/// The weather's state (Locator::weatherSystem)
openblack::weather::State& WeatherState()
{
	return openblack::Locator::weatherSystem::value().GetState();
}

/// The clouds' random numbers come from the CRT rand (game_random::crt)
float Random(float a, float b)
{
	return game_random::crt::Random(a, b);
}

/// graphics::haze::ApplyObject(pos, specular, &colour) with the specular a grey min(255, int(flash f3 x 127)) in all
/// four bytes; it darkens the colour and returns the mist's specular, drawn by the mists' effect branch
uint32_t Haze(uint32_t& argb, const glm::vec3& position, float flash)
{
	const auto grey = std::min(static_cast<uint32_t>(static_cast<int32_t>(flash * 127.0f)), 0xFFu);
	const uint32_t specular = grey << 24 | grey << 16 | grey << 8 | grey;
	if (!Locator::camera::has_value())
	{
		return specular;
	}
	const auto view = Locator::camera::value().GetViewMatrix(Camera::Interpolation::Current);
	return graphics::haze::ApplyObject(graphics::haze::Frame(), graphics::haze::Depth(view, position), specular, &argb);
}

/// sstorm.raw (40 x 40 grey, 1600 bytes): the storms' land shadow
const std::vector<uint8_t>& StormShadowImage()
{
	static const std::vector<uint8_t> k_None;
	if (!Locator::filesystem::has_value() || !Locator::resources::has_value())
	{
		return k_None;
	}
	// the file system resolves the path when it reads: a missing file is reported by LoadOptionalBlob, not thrown
	return resources::LoadOptionalBlob(Locator::resources::value().GetBlobs(),
	                                   Locator::filesystem::value().GetPath<filesystem::Path::Textures>() / "sstorm.raw",
	                                   "storm shadows");
}
} // namespace

uint32_t storm_clouds::PuffColour(uint32_t baseArgb, float blackness, float fade)
{
	uint32_t rgb = baseArgb & 0x00FFFFFFu;
	if (blackness > 0.0f)
	{
		// each byte x (1 - blackness x 0.5), truncated
		const float mul = 1.0f - blackness * 0.5f;
		rgb = 0;
		for (const uint32_t shift : {16u, 8u, 0u})
		{
			const auto c =
			    static_cast<uint32_t>(static_cast<int>(static_cast<float>((baseArgb >> shift) & 0xFFu) * mul)) & 0xFFu;
			rgb |= c << shift;
		}
	}
	// alpha = int(fade x 0.75 x the base's alpha byte)
	const auto alpha = static_cast<uint32_t>(static_cast<int>(fade * 0.75f * static_cast<float>(baseArgb >> 24u))) & 0xFFu;
	return (alpha << 24u) | rgb;
}

namespace
{
/// What the storm clouds' test hook keeps between calls, in the debug hooks' store (Locator::debugHooks)
struct StormCloudsDebugHooksState
{
	bool done {false}; // OPENBLACK_TEST_STORM_CLOUDS: the test storm has been made (or its value rejected)
};

StormCloudsDebugHooksState& StormCloudsDebugHooksData()
{
	if (!Locator::debugHooks::has_value())
	{
		std::fputs("storm_clouds: no debug hooks in the locator (Locator::debugHooks)\n", stderr);
		std::abort();
	}
	return Locator::debugHooks::value().Get<StormCloudsDebugHooksState>();
}

/// OPENBLACK_TEST_STORM_CLOUDS="x,z,radius[,clouds[,blackness[,elevation]]]": a storm made as a new storm's defaults
/// (8 clouds, blackness 0.5, elevation 160, inner 100, outer 300) with that radius as its inner one and 3 x
/// it as the outer one, a fade of 1 s, an almost endless life and rain 100, the first frame a land exists (a test helper,
/// not in the original: the climate storms and the weather things make such storms in a game)
void RunDebugHook()
{
	auto& done = StormCloudsDebugHooksData().done;
	const char* text = std::getenv("OPENBLACK_TEST_STORM_CLOUDS");
	if (done || text == nullptr || !Locator::terrainSystem::has_value())
	{
		return;
	}
	done = true;
	float x = 0.0f;
	float z = 0.0f;
	float radius = 100.0f;
	int clouds = 8;
	float blackness = 0.5f;
	float elevation = 160.0f;
	if (std::sscanf(text, "%f,%f,%f,%d,%f,%f", &x, &z, &radius, &clouds, &blackness, &elevation) < 3)
	{
		return;
	}
	storms::StormDescriptor d {
	    .position = glm::vec3(x, LandHeightAt(x, z), z),
	    .innerRadius = radius,
	    .outerRadius = radius * 3.0f,
	    .fadeInTime = 1.0f,
	    .lifeTime = 1e9f,
	    .numClouds = clouds,
	    .blackness = blackness,
	    .elevation = elevation,
	};
	const auto id = storms::Create(d);
	SPDLOG_LOGGER_INFO(spdlog::get("game"),
	                   "Storm clouds test: storm {} at ({:.1f}, {:.1f}) radius {:.0f}/{:.0f}, {} clouds, "
	                   "blackness {:.2f}, elevation {:.0f}",
	                   id, x, z, d.innerRadius, d.outerRadius, clouds, blackness, elevation);
}
} // namespace

void storm_clouds::DrawFrame(float milliseconds)
{
	RunDebugHook();
	// the storms that went take their puffs with them
	std::erase_if(WeatherState().puffs, [](const auto& entry) {
		bool alive = false;
		storms::ForEach([&](const storms::Storm& storm) { alive = alive || storm.id == entry.first; });
		return !alive;
	});
	const uint32_t base = land_light::CurrentTable().GetLandColour();
	storms::ForEachMutable([&](storms::Storm& storm) {
		auto& d = storm.descriptor;
		// every storm draws its clouds, marked or not
		if (d.numClouds <= 0)
		{
			return;
		}
		if (d.numClouds > 16) // the count itself is capped
		{
			d.numClouds = 16;
		}
		auto& puffs = WeatherState().puffs[storm.id];
		// one new puff a frame up to the count: a mist (type 7) in the effect branch, k Random(2.5,
		// 5); size 2 x Random(0.01, 0.015); at (Random(-1, 1), Random(-10, 10) + elevation, Random(-1, 1))
		if (static_cast<int>(puffs.size()) < d.numClouds)
		{
			// the five draws in their order: k, size, then the offset's x, y, z
			const float puffK = Random(2.5f, 5.0f);
			const float puffSize = Random(0.01f, 0.015f) * 2.0f;
			const float puffX = Random(-1.0f, 1.0f);
			const float puffY = Random(-10.0f, 10.0f) + d.elevation;
			const float puffZ = Random(-1.0f, 1.0f);
			const Puff puff {.offset = {puffX, puffY, puffZ}, .frames = 0, .size = puffSize, .k = puffK};
			puffs.push_back(puff);
		}
		const float spread = storm.outerRadius + storm.innerRadius;
		for (auto& puff : puffs)
		{
			// a new target every 400 frames, reached in steps of 0.0025 of the way. (approximate: counted in frames, as
			// the original; openblack's frame rate is not the original's)
			if (puff.frames != 0)
			{
				--puff.frames;
				puff.offset += puff.step;
			}
			else
			{
				puff.target.x = Random(-1.0f, 1.0f);
				puff.target.y = Random(0.0f, 20.0f) + d.elevation;
				puff.target.z = Random(-1.0f, 1.0f);
				puff.step = (puff.target - puff.offset) * 0.0025f;
				puff.frames = 400;
			}
			glm::vec3 position;
			position.x = spread * puff.offset.x * 0.5f + storm.drawPosition.x;
			position.z = spread * puff.offset.z * 0.5f + storm.drawPosition.z;
			position.y = LandHeightAt(position.x, position.z) + puff.offset.y;
			uint32_t colour = PuffColour(base, d.blackness, storm.fade);
			const uint32_t specular = Haze(colour, position, storm.flash.f3);
			// drawn only above alpha 5
			if ((colour >> 24u) > 5u)
			{
				// the mist's counter += int(frame ms x 0.255), modulo 900, run only for a mist sent to the Z-sorter (its
				// sphere on screen: mists::InView); the fraction kept as the map mists do (frame_anim::MistAdvance)
				if (mists::InView(position, spread * puff.size))
				{
					graphics::frame_anim::MistClock clock {puff.counter, puff.counterRemainder};
					graphics::frame_anim::MistAdvance(clock, milliseconds);
					puff.counter = clock.counter;
					puff.counterRemainder = clock.remainder;
				}
				mists::MistDesc mist {
				    .position = position,
				    .size = spread * puff.size,
				    .colour = colour,
				    .edgeShrink = true,
				    .k = puff.k,
				    .counter = puff.counter,
				    .specular = specular,
				};
				mists::Submit(mist);
			}
		}
		// the storm's shadow, s = (blackness + 0.7) x fade; at 1 or more stamped with 1, else only above 0.01; with
		// "CloudShadows" on: sstorm.raw stamped on the land light at the storm, 40 wide, strength s, mode 2
		float shadow = (d.blackness + 0.7f) * storm.fade;
		const bool cloudShadows =
		    Locator::config::has_value() && graphics::GetDetailLevel(Locator::config::value().detailLevel).clouds;
		if (shadow > 1.0f)
		{
			shadow = 1.0f;
		}
		const auto& image = StormShadowImage();
		if (shadow > 0.01f && cloudShadows && image.size() == 40u * 40u)
		{
			land_light::AddStamp(storm.drawPosition, image.data(), 40, true, shadow, 2);
		}
	});
}

void storm_clouds::Clear()
{
	WeatherState().puffs.clear();
}

size_t storm_clouds::PuffCount(uint32_t stormId)
{
	const auto it = WeatherState().puffs.find(stormId);
	return it == WeatherState().puffs.end() ? 0 : it->second.size();
}
