/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "3D/AllMeshes.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Sky.h"
#include "ECS/Systems/VortexSystemInterface.h"

namespace openblack
{
class Camera;
} // namespace openblack

namespace openblack::ecs::systems
{
struct RenderContext;
} // namespace openblack::ecs::systems

namespace openblack::graphics
{

/// What the frame draws, as the steps of the frame left it before the draw. The steps of the frame fill it on the game's
/// thread. The passes of the land and of the models read only this, not the world, so the world can change while they
/// encode.
struct DrawnFrame
{
	/// A run of matrices in `bones`
	struct BoneRun
	{
		uint32_t first {0};
		uint32_t count {0};
	};

	/// The pose of a villager as the models' pass draws it
	struct VillagerLook
	{
		AnimId clip {AnimId::Invalid};
		/// How many bones its pose has. The pass compares this with the bones of the model.
		uint32_t poseBones {0};
		/// Its bones, when the pass draws it with its own bones and not from the bone palette
		std::optional<BoneRun> bones;
		bool inPhysics {false};
		bool inHand {false};
		bool highDetail {false};
	};

	/// The pose and the look of an animal as the models' pass draws it
	struct AnimalLook
	{
		BoneRun bones;
		ecs::components::AnimalLight light {ecs::components::AnimalLight::BrightestLand};
		glm::vec3 colour {255.0f};
		uint8_t alpha {255};
		std::optional<float> cutBelow;
		bool aboveCutShown {true};
		std::optional<glm::vec3> underSea;
	};

	/// The pose of scenery that its own clip moves, as the models' pass draws it
	struct StillLook
	{
		BoneRun bones;
		bool boatFilmPart {false};
	};

	/// What the models' pass reads of a thing that it draws in its own pose. A part is empty when the thing does not
	/// have that pose.
	struct EntityLook
	{
		std::optional<VillagerLook> villager;
		std::optional<AnimalLook> animal;
		std::optional<StillLook> still;
	};

	/// What the sky's passes read
	struct Sky
	{
		std::optional<ecs::components::SkyDome> dome;
		struct Sun
		{
			ecs::components::Sun sun;
			ecs::components::CelestialBody body;
		};
		std::optional<Sun> sun;
		bool sunHidden {false};
		struct Moon
		{
			ecs::components::Moon moon;
			ecs::components::CelestialBody body;
			std::optional<ecs::components::CelestialGlow> glow;
		};
		std::optional<Moon> moon;
		/// The value of the land that the sky shows, from 0, good, to 2, evil
		float skyLand {0.0f};
		/// The sky as the game counts it: from 2 at night through 1 at dusk to 0 by day
		float skyType {0.0f};
	};

	/// What the land's pass reads, other than the land's blocks
	struct Land
	{
		std::vector<ecs::systems::VortexSystemInterface::GroundMark> groundMarks;
		bool reflected {true};
		bool coarse {false};
	};

	/// The game's camera as the frame shows it. The copy has no camera model and does not move.
	std::unique_ptr<const Camera> camera;
	/// The time of the frame, in milliseconds by the game's clock
	uint32_t time {0};
	float timeOfDay {0.0f};
	bool templeActive {false};
	Sky sky;
	Land land;
	/// The instances and the lists of draws of the frame. The steps of the frame make them, and only they change them.
	const ecs::systems::RenderContext* models {nullptr};
	/// How many times the steps of the frame had changed `models` when the frame filled this
	uint64_t modelWrites {0};
	/// The look of each thing of the land's posed draws, in the order of the draws
	std::vector<EntityLook> entityLooks;
	/// The look of each thing of the drawn registry's posed draws, in the order of the draws
	std::vector<EntityLook> drawnEntityLooks;
	/// The bones of all the looks
	std::vector<glm::mat4> bones;

	[[nodiscard]] std::span<const glm::mat4> BonesOf(BoneRun run) const
	{
		return std::span<const glm::mat4>(bones).subspan(run.first, run.count);
	}
	/// Whether the steps of the frame changed the models after the frame filled this, of the count of changes now
	[[nodiscard]] bool ModelsChanged(uint64_t writesNow) const { return models != nullptr && writesNow != modelWrites; }
};

} // namespace openblack::graphics
