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

#include <filesystem>
#include <optional>

#include "Graphics/OverlayFrame.h"
#include "Graphics/RenderPass.h"
#include "Graphics/RendererInterface.h"
#include "Graphics/SuperVillagerFrame.h"

namespace openblack
{
class Camera;

namespace ecs
{
class Registry;
}

namespace engine
{

/// (openblack engine) What one frame's logic hands to its draw. It is filled by Game::LogicFrame at the end
/// of the frame's logic and read by Game::DrawFrame. It OWNS by value what DrawSceneDesc referenced in Game::Run's
/// locals before (the overlays, the SuperVillagers' part, the clocks, the hand, the config flags of the draw, this
/// frame's screenshot request), so the draw no longer needs anything of the logic's state but the camera copy.
/// One thread (now): one snapshot, filled and drawn in turn, the same calls in the same order as before.
/// Two threads (planned): two of them (and two draw cameras), the logic filling one while the draw reads the other.
struct FrameSnapshot
{
	graphics::OverlayFrame overlay;              ///< FillOverlayFrame + intro_special::FillFrame (introLight)
	graphics::SuperVillagerFrame superVillagers; ///< super_villager::FillFrame (Intro: values and entity ids only)
	/// The draw's camera: Game's _drawCamera, which Camera::CopyViewTo fills in LogicFrame. (two threads) one per snapshot
	const Camera* camera {nullptr};
	uint32_t time {0};      ///< DrawSceneDesc::time, the ms since Run started (TODO(#481) as before)
	float timeOfDay {0.0f}; ///< SkySystem::GetTime
	graphics::RendererInterface::DrawClock clock {};
	graphics::RendererInterface::DrawHand hand {};
	float bumpMapStrength {0.0f};
	float smallBumpMapStrength {0.0f};
	bool drawSky {true};
	bool drawWater {true};
	bool drawIsland {true};
	bool drawEntities {true};
	bool drawSprites {true};
	bool drawBoundingBoxes {false};
	bool wireframe {false};
	/// This frame's screenshot request (Locator::screenshotRequest's when its frame is this one): taken in LogicFrame,
	/// asked of the renderer in DrawFrame. Nothing between the two can change it (only Update and the debug GUI's
	/// Draw write it, the GUI's Draw coming after the request is read, as before)
	std::optional<std::filesystem::path> screenshot;
	uint32_t frameCount {0}; ///< The Game's frame count of this frame (the draw-stats hook's test)

	/// The main view's DrawSceneDesc over this snapshot. `entities` is the live registry, kept because the desc still
	/// has the field; nothing in the draw reads it. (two threads) the field goes
	[[nodiscard]] graphics::RendererInterface::DrawSceneDesc Desc(const ecs::Registry& entities) const
	{
		return {
		    .camera = camera,
		    .frameBuffer = nullptr,
		    .entities = entities,
		    .overlay = overlay,
		    .superVillagers = superVillagers,
		    .time = time,
		    .timeOfDay = timeOfDay,
		    .clock = clock,
		    .hand = hand,
		    .bumpMapStrength = bumpMapStrength,
		    .smallBumpMapStrength = smallBumpMapStrength,
		    .viewId = graphics::RenderPass::Main,
		    .drawSky = drawSky,
		    .drawWater = drawWater,
		    .drawIsland = drawIsland,
		    .drawEntities = drawEntities,
		    .drawSprites = drawSprites,
		    .drawBoundingBoxes = drawBoundingBoxes,
		    .wireframe = wireframe,
		};
	}
};

} // namespace engine
} // namespace openblack
