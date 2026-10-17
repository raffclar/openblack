/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <random>
#include <vector>

#include "ECS/Systems/StreamWaterSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class StreamWaterSystem final: public StreamWaterSystemInterface
{
public:
	void Update(std::chrono::milliseconds frameRealTime) override;
	[[nodiscard]] const graphics::underwater::streams::Network& GetNetwork() const override { return _network; }
	[[nodiscard]] const graphics::underwater::streams::FlowMap& GetFlowMap() const override { return _flowMap; }
	[[nodiscard]] uint32_t GetRevision() const override { return _revision; }
	[[nodiscard]] std::optional<Water> WaterAt(glm::vec2 point) const override;
	[[nodiscard]] std::span<const graphics::underwater::streams::Drifter> GetDrifters() const override { return _drifters; }
	[[nodiscard]] float DrifterHeight(const graphics::underwater::streams::Drifter& drifter) const override;
	[[nodiscard]] float LoudnessAt(glm::vec3 point) const override;

private:
	/// Finds the rivers again when the land's rivers have changed
	void Rebuild();
	/// Starts a drifter again at a point upstream on a river
	void Spawn(graphics::underwater::streams::Drifter& drifter);
	void MoveDrifters(float seconds);

	graphics::underwater::streams::Network _network;
	graphics::underwater::streams::FlowMap _flowMap;
	graphics::underwater::streams::Channel _channel;
	std::vector<graphics::underwater::streams::Drifter> _drifters;
	/// How long each drifter has hardly moved, in seconds
	std::vector<float> _stuck;
	/// A mark of the land's rivers: their number of points and the sum of their places
	std::pair<size_t, float> _riversMark {0, 0.0f};
	uint32_t _seabedRevision {0};
	uint32_t _revision {0};
	std::mt19937 _random {0x5EED};
};

} // namespace openblack::ecs::systems
