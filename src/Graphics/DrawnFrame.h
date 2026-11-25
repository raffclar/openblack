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

#include <array>
#include <chrono>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "3D/AllMeshes.h"
#include "3D/Extent.h"
#include "3D/LandHeights.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Sky.h"
#include "ECS/Systems/VortexSystemInterface.h"
#include "Enums.h"
#include "Graphics/DrawnCreatures.h"
#include "Graphics/DrawnEffects.h"
#include "Graphics/DrawnSprites.h"
#include "Graphics/DrawnTemple.h"

namespace openblack
{
class Camera;
class LandIslandInterface;
} // namespace openblack

namespace openblack::ecs::systems
{
struct RenderContext;
} // namespace openblack::ecs::systems

namespace openblack::graphics
{
class FrameBuffer;
class Mesh;

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
		/// The time of day that the windows of the houses show, and whether it is night for them, when the game has a
		/// clock of the sky
		std::optional<float> windowTime;
		bool windowNight {false};
		/// For each sample of the sun's glare, whether the land or the sea hides it from the camera. The renderer fills
		/// it with the effects, only when the frame draws the glare.
		std::array<bool, 5> glareLandHides {};
	};

	/// What the land's pass reads, other than the land's blocks
	struct Land
	{
		std::vector<ecs::systems::VortexSystemInterface::GroundMark> groundMarks;
		/// The lowest and the highest corners of the land on the map, when the game has a land
		std::optional<Extent2> extent;
		bool reflected {true};
		bool coarse {false};
		/// The height of the highest land that a corner can have
		float highestHeight {0.0f};
		/// The land's blocks: their meshes, textures and pages. Only the game's thread changes them, between the draws
		/// (see LandIslandInterface::CommitAltitudeChanges). The turn can change the heights of the corners. Thus the
		/// passes read the heights from `heights`, not from the land.
		const LandIslandInterface* island {nullptr};
		/// The heights of the land as the frame shows them. The copy keeps its memory from frame to frame and is made
		/// again only when the heights changed.
		LandHeights heights;
	};

	/// What the passes read of the hands of this computer
	struct Hands
	{
		/// The player of this computer
		PlayerNames ownPlayer {PlayerNames::PLAYER_ONE};
		/// The left hand and the right hand of the player of this computer
		std::array<entt::entity, 2> own {entt::null, entt::null};
		/// The colour that the flash of a known gesture adds to the light of the drawn hand, from 0 to 1 for each channel
		glm::vec3 glow {0.0f};
		/// The place on the water under the drawn hand, when the hand is near the low land. The hand's glow on the water
		/// shows there at night.
		std::optional<glm::vec2> waterGlowCentre;
		/// The alignment of the player of this computer, from -1, evil, to 1, good
		float ownAlignment {0.0f};

		/// The hand that the frame draws: the left hand of the player of this computer
		[[nodiscard]] entt::entity Drawn() const { return own[0]; }
	};

	/// The game's clock as the frame shows it
	struct Clock
	{
		/// How far the game is from the last turn to the next, from 0 to 1
		float turnFraction {0.0f};
		/// How long the frame took on the game's clock, which stops while the game is paused, and on the real clock
		std::chrono::milliseconds frameGameTime {0};
		std::chrono::milliseconds frameRealTime {0};
		/// How long the game ran
		std::chrono::milliseconds elapsed {0};
	};

	/// What the passes of the water read of the sea. These are things of the graphics device, which the sea makes at
	/// its start and which the world does not change.
	struct Sea
	{
		/// The target that the sea's reflection draws into
		const FrameBuffer* reflection {nullptr};
		const Mesh* mesh {nullptr};
		entt::id_type texture {0};
	};

	/// The game's camera as the frame shows it. The copy has no camera model and does not move.
	std::unique_ptr<const Camera> camera;
	/// The time of the frame, in milliseconds by the game's clock
	uint32_t time {0};
	float timeOfDay {0.0f};
	bool templeActive {false};
	Sky sky;
	Land land;
	/// What the passes read of the temple, while the player is in it
	DrawnTemple temple;
	Hands hands;
	Sea sea;
	Clock clock;
	/// The light of the models of the frame: the sun by day, or the light that the hand carries at night. The renderer
	/// fills it with the effects.
	glm::vec4 modelLight {0.0f};
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
	/// What the effects draw. The renderer fills it after the rest, only in a frame that draws the world.
	DrawnEffects effects;
	/// What the creatures' pass draws. The renderer fills it with the effects.
	DrawnCreatures creatures;
	/// The sprites that the frame shows, in the order of the draw. The renderer fills them with the effects.
	std::vector<DrawnSprite> sprites;

	[[nodiscard]] std::span<const glm::mat4> BonesOf(BoneRun run) const
	{
		return std::span<const glm::mat4>(bones).subspan(run.first, run.count);
	}
	/// Whether the steps of the frame changed the models after the frame filled this, of the count of changes now
	[[nodiscard]] bool ModelsChanged(uint64_t writesNow) const { return models != nullptr && writesNow != modelWrites; }
};

} // namespace openblack::graphics
