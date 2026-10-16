/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <unordered_map>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat4x4.hpp>

#include "ECS/Systems/WaterWakeSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::graphics
{
class L3DMesh;
}

namespace openblack::ecs::systems
{

class WaterWakeSystem final: public WaterWakeSystemInterface
{
public:
	void Update(std::chrono::milliseconds frameRealTime) override;
	[[nodiscard]] Frame GetFrame() const override;
	void DropDebugThings() override;
	void PlayDebugBoat() override;

private:
	/// How a thing moved over the last frames
	struct Track
	{
		/// Where it is, eased over a tenth of a second, so that a thing that moves once a game turn moves smoothly
		glm::vec3 position {0.0f};
		/// How fast it moves, eased the same way
		glm::vec3 velocity {0.0f};
		/// Where its lowest point was in the last frame, and the water's height there
		float bottom {0.0f};
		float water {0.0f};
		/// Whether it was seen this frame
		bool seen {false};
	};
	/// A thing found in the water this frame
	struct Candidate
	{
		entt::entity entity;
		glm::mat4 model;
		glm::vec3 boxLow;
		glm::vec3 boxHigh;
		bool floats;
		bool boat;
		/// The model, for the points of the hull near its water line
		const graphics::L3DMesh* mesh;
	};

	void Follow(const Candidate& candidate, float frameSeconds, float seaLevel, float seconds);

	std::unordered_map<entt::entity, Track> _tracks;
	graphics::underwater::wake::FieldClock _clock;
	graphics::underwater::wake::StepPlan _plan;
	glm::vec2 _centre {0.0f};
	std::vector<FieldWrite> _movers;
	std::vector<FieldWrite> _rings;
	std::vector<graphics::underwater::wake::Hull> _hulls;
	std::vector<ActiveSplash> _splashes;
	/// The most that any thing wrote into the field this frame
	float _height {0.0f};
	float _foam {0.0f};
};

} // namespace openblack::ecs::systems
