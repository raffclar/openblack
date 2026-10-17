/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the flowing water in the rivers, with the experiment's water on

#define LOCATOR_IMPLEMENTATIONS

#include "StreamWaterSystem.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <exception>
#include <numbers>

#include <L3DFile.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "ECS/Components/Stream.h"
#include "ECS/Registry.h"
#include "ECS/Systems/UnderwaterSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/Underwater.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
namespace streams = openblack::graphics::underwater::streams;

namespace
{
/// How many leaves and twigs drift down the rivers at once
constexpr size_t k_Drifters = 48;
/// How long a drifter lives at most, and how long it floats on the sea after it leaves a river
constexpr float k_DrifterLife = 150.0f;
constexpr float k_SeaLife = 30.0f;
/// A drifter that hardly moves for this long starts again upstream
constexpr float k_StuckSeconds = 6.0f;
/// The jet of water out of a river's mouth reaches this far into the sea
constexpr float k_JetReach = 25.0f;
/// The cache's name for the river channel's model file
constexpr entt::hashed_string k_ChannelFileId = entt::hashed_string("experiment/river_channel_file");

bool WaterOn()
{
	return Locator::underwaterSystem::has_value() && Locator::underwaterSystem::value().IsEnabled() &&
	       Locator::underwaterSystem::value().GetEffectOff() != openblack::graphics::underwater::Effect::Streams;
}

/// The channel of the original's river model, from its footprint
streams::Channel LoadChannel()
{
	auto& files = Locator::resources::value().GetL3DFiles();
	if (!files.Contains(k_ChannelFileId.value()))
	{
		const auto path = Locator::filesystem::value().GetPath<openblack::filesystem::Path::Data>() / "river.l3d";
		try
		{
			files.Load(k_ChannelFileId.value(), openblack::resources::L3DFileLoader::FromDiskTag {}, path);
		}
		catch (std::exception& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("graphics"), "Streams experiment: cannot read {}: {}", path.string(), err.what());
			return {};
		}
	}
	const auto& footprint = files.Handle(k_ChannelFileId.value())->GetFootprint();
	if (!footprint.has_value() || footprint->entries.empty() || footprint->entries[0].triangles.empty())
	{
		return {};
	}
	const auto& entry = footprint->entries[0];
	// The alpha is the top four bits of each 16-bit texel
	std::vector<uint8_t> alpha(entry.pixels.size());
	std::ranges::transform(entry.pixels, alpha.begin(), [](uint16_t texel) { return static_cast<uint8_t>(texel >> 12); });
	const auto& triangle = entry.triangles[0];
	std::array<glm::vec2, 3> model {};
	std::array<glm::vec2, 3> texels {};
	for (size_t i = 0; i < 3; ++i)
	{
		model.at(i) = {triangle.world.at(i).x, triangle.world.at(i).y};
		texels.at(i) = {triangle.texture.at(i).x, triangle.texture.at(i).y};
	}
	return streams::ChannelFrom({static_cast<int>(footprint->header.width), static_cast<int>(footprint->header.height)},
	                            std::move(alpha), model, texels);
}

/// The nearest point on any river's line, and the segment it is on
std::pair<glm::vec2, const streams::Segment*> NearestOnRiver(const streams::Network& network, glm::vec2 point)
{
	float best = 1e9f;
	std::pair<glm::vec2, const streams::Segment*> nearest {point, nullptr};
	for (const auto& segment : network.segments)
	{
		const glm::vec2 line = segment.to - segment.from;
		const float length2 = glm::dot(line, line);
		const float t = length2 > 0.0f ? std::clamp(glm::dot(point - segment.from, line) / length2, 0.0f, 1.0f) : 0.0f;
		const glm::vec2 on = segment.from + line * t;
		const float distance = glm::distance(on, point);
		if (distance < best)
		{
			best = distance;
			nearest = {on, &segment};
		}
	}
	return nearest;
}
} // namespace

void StreamWaterSystem::Rebuild()
{
	std::vector<std::vector<glm::vec2>> rivers;
	std::pair<size_t, float> mark {0, 0.0f};
	if (Locator::entitiesRegistry::has_value())
	{
		Locator::entitiesRegistry::value().Each<const components::Stream>([&](const components::Stream& stream) {
			std::vector<glm::vec2> points;
			points.reserve(stream.points.size());
			for (const auto& point : stream.points)
			{
				points.emplace_back(point.x, point.z);
				mark.first += 1;
				mark.second += point.x + point.z;
			}
			// The rivers in the order of their numbers, so that the same land gives the same network
			const auto at = static_cast<size_t>(std::max(stream.id, 0));
			if (rivers.size() <= at)
			{
				rivers.resize(at + 1);
			}
			rivers[at] = std::move(points);
		});
	}
	const uint32_t seabedRevision =
	    Locator::underwaterSystem::has_value() ? Locator::underwaterSystem::value().GetSeabedRevision() : 0;
	if (mark == _riversMark && seabedRevision == _seabedRevision)
	{
		return;
	}
	_riversMark = mark;
	_seabedRevision = seabedRevision;
	++_revision;
	_drifters.clear();
	_stuck.clear();
	if (!Locator::terrainSystem::has_value() || rivers.empty())
	{
		_network = {};
		_flowMap = {};
		return;
	}
	const auto& land = Locator::terrainSystem::value();
	_network = streams::BuildNetwork(rivers, [&land](glm::vec2 point) { return land.GetHeightAt(point); });
	_flowMap = streams::BuildFlowMap(_network);
	if (_channel.alpha.empty())
	{
		_channel = LoadChannel();
	}
	_drifters.resize(k_Drifters);
	_stuck.assign(k_Drifters, 0.0f);
	for (auto& drifter : _drifters)
	{
		Spawn(drifter);
		// They start at all ages, so that they do not all start again at once
		drifter.age = std::uniform_real_distribution<float>(0.0f, drifter.life * 0.8f)(_random);
	}
	SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "Streams experiment: {} rivers, {} stretches, {} mouths, map {}x{}",
	                   rivers.size(), _network.segments.size(), _network.mouths.size(), _flowMap.size.x, _flowMap.size.y);
}

void StreamWaterSystem::Spawn(streams::Drifter& drifter)
{
	drifter = {};
	if (_network.segments.empty())
	{
		return;
	}
	// A point on the rivers, each stretch as likely as its length
	float total = 0.0f;
	for (const auto& segment : _network.segments)
	{
		total += glm::distance(segment.from, segment.to);
	}
	float pick = std::uniform_real_distribution<float>(0.0f, total)(_random);
	for (const auto& segment : _network.segments)
	{
		const float length = glm::distance(segment.from, segment.to);
		if (pick <= length)
		{
			drifter.position = glm::mix(segment.from, segment.to, length > 0.0f ? pick / length : 0.0f);
			break;
		}
		pick -= length;
	}
	drifter.life = std::uniform_real_distribution<float>(0.5f, 1.0f)(_random) * k_DrifterLife;
	drifter.angle = std::uniform_real_distribution<float>(0.0f, 2.0f * std::numbers::pi_v<float>)(_random);
	drifter.spin = std::uniform_real_distribution<float>(-0.6f, 0.6f)(_random);
	drifter.seed = static_cast<uint32_t>(_random());
}

void StreamWaterSystem::MoveDrifters(float seconds)
{
	if (_network.segments.empty() || seconds <= 0.0f)
	{
		return;
	}
	const auto& underwater = Locator::underwaterSystem::value();
	const float clock = underwater.GetSeconds();
	for (size_t i = 0; i < _drifters.size(); ++i)
	{
		auto& drifter = _drifters[i];
		drifter.age += seconds;
		glm::vec2 velocity {0.0f};
		if (!drifter.atSea)
		{
			const auto texel = streams::Sample(_flowMap, drifter.position);
			const auto [on, segment] = NearestOnRiver(_network, drifter.position);
			const glm::vec2 towards = on - drifter.position;
			const float apart = glm::length(towards);
			velocity = streams::DriftVelocity(texel, apart > 1e-4f ? towards / apart : glm::vec2(0.0f));
			// A drifter turns faster in fast and white water
			drifter.angle += drifter.spin * seconds * (1.0f + 3.0f * texel.white + glm::length(texel.flow));
			// At a river's mouth it leaves the river for the sea
			for (const auto& mouth : _network.mouths)
			{
				if (glm::distance(drifter.position, mouth.position) < 2.0f)
				{
					drifter.atSea = true;
					drifter.age = std::max(drifter.age, drifter.life - k_SeaLife);
				}
			}
		}
		else
		{
			// On the sea: the jet out of the nearest mouth, which fades with the distance, and the sea's current
			for (const auto& mouth : _network.mouths)
			{
				const float distance = glm::distance(drifter.position, mouth.position);
				velocity += mouth.direction * mouth.speed * std::max(1.0f - distance / k_JetReach, 0.0f);
			}
			const float level = underwater.GetSeaLevel();
			velocity += graphics::underwater::CurrentAt({drifter.position.x, level, drifter.position.y}, clock,
			                                            underwater.GetCurrent(), underwater.GetWaves(), level) *
			            0.5f;
			drifter.angle += drifter.spin * seconds * 0.3f;
		}
		drifter.position += velocity * seconds;
		_stuck[i] = glm::length(velocity) < streams::k_LeastSpeed * 0.5f ? _stuck[i] + seconds : 0.0f;
		if (drifter.age > drifter.life || _stuck[i] > k_StuckSeconds)
		{
			Spawn(drifter);
			_stuck[i] = 0.0f;
		}
	}
}

void StreamWaterSystem::Update(std::chrono::milliseconds frameRealTime)
{
	Rebuild();
	if (!WaterOn())
	{
		return;
	}
	// A slow frame moves the drifters no more than a tenth of a second
	MoveDrifters(std::min(std::chrono::duration<float>(frameRealTime).count(), 0.1f));
}

std::optional<StreamWaterSystemInterface::Water> StreamWaterSystem::WaterAt(glm::vec2 point) const
{
	if (!WaterOn() || _network.segments.empty() || !Locator::terrainSystem::has_value())
	{
		return std::nullopt;
	}
	const auto texel = streams::Sample(_flowMap, point);
	if (texel.distance > 6.0f)
	{
		return std::nullopt;
	}
	const float alpha = streams::ChannelAt(_network, _channel, point);
	if (alpha >= streams::k_WaterAlpha)
	{
		return std::nullopt;
	}
	const float depth = streams::DepthForAlpha(alpha);
	return Water {.surface = Locator::terrainSystem::value().GetHeightAt(point) + depth, .depth = depth, .flow = texel.flow};
}

float StreamWaterSystem::DrifterHeight(const streams::Drifter& drifter) const
{
	if (drifter.atSea && Locator::underwaterSystem::has_value())
	{
		const auto& underwater = Locator::underwaterSystem::value();
		const auto wave =
		    graphics::underwater::WavesAt(drifter.position, underwater.GetSeconds(), underwater.GetWaves()).height;
		// The waves are smaller in the shallow water near the coast
		const float depth = underwater.GetSeabed().DepthAt(drifter.position) + underwater.GetSeaLevel();
		return underwater.GetSeaLevel() + wave * std::clamp((depth - 0.2f) / 3.8f, 0.0f, 1.0f) + 0.03f;
	}
	// On a river it floats on the land's surface, where the river's water is drawn
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(drifter.position) + 0.04f : 0.0f;
}

float StreamWaterSystem::LoudnessAt(glm::vec3 point) const
{
	if (!WaterOn() || !Locator::terrainSystem::has_value())
	{
		return 0.0f;
	}
	// The sound fades as the listener goes up from the land
	const glm::vec2 across {point.x, point.z};
	const float height = std::max(point.y - Locator::terrainSystem::value().GetHeightAt(across), 0.0f);
	return streams::Loudness(_network, across) * 40.0f / (40.0f + height);
}
