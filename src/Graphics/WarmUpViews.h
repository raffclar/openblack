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

#include "Visual/RenderPass.h"

/// The views of a warm-up: the renderer draws the first frame of a land behind the loading screen, so that it makes the
/// pipelines and the render targets of the scene there. The views that draw into the window keep to one pixel, and the
/// loading screen's view clears all of the window again before it draws.
namespace openblack::graphics::warm_up
{

/// The views of the scene that draw into the window. A warm-up draws one pixel of each.
constexpr std::array k_WindowSceneViews {RenderPass::Sky,
                                         RenderPass::Land,
                                         RenderPass::Main,
                                         RenderPass::Translucent,
                                         RenderPass::GestureHintCollar0,
                                         RenderPass::GestureHintCollar1,
                                         RenderPass::GestureHintCollar2,
                                         RenderPass::GestureHintSeed0,
                                         RenderPass::GestureHintSeed1,
                                         RenderPass::GestureHintSeed2,
                                         RenderPass::GestureHintSeed3,
                                         RenderPass::GestureHintSeed4,
                                         RenderPass::GestureHints,
                                         RenderPass::Advisors};

/// The views of the scene that the next frames draw again. Under Vulkan, a warm-up can divide the draws into these
/// views between some frames, because one frame with all of them can use all of the renderer's descriptor sets.
constexpr std::array k_PartedViews {RenderPass::ReflectionSky,
                                    RenderPass::Reflection,
                                    RenderPass::ReflectionTranslucent,
                                    RenderPass::ReflectionMirrored,
                                    RenderPass::Sky,
                                    RenderPass::Land,
                                    RenderPass::Main,
                                    RenderPass::Translucent,
                                    RenderPass::GestureHintCollar0,
                                    RenderPass::GestureHintCollar1,
                                    RenderPass::GestureHintCollar2,
                                    RenderPass::GestureHintSeed0,
                                    RenderPass::GestureHintSeed1,
                                    RenderPass::GestureHintSeed2,
                                    RenderPass::GestureHintSeed3,
                                    RenderPass::GestureHintSeed4,
                                    RenderPass::GestureHints,
                                    RenderPass::Advisors};

/// The most draws into k_PartedViews that one frame of a warm-up does under Vulkan. Each draw takes one descriptor set
/// at most. Thus a frame keeps in its share of the sets, with space for the other views and the loading screen.
constexpr uint32_t k_DrawsPerPart = 768;

/// The most frames that one warm-up uses
constexpr uint32_t k_MaxParts = 64;

/// The view that the loading screen draws in. In a warm-up's frame it first clears all of the window again, as the
/// main view clears it. It comes after every view in k_WindowSceneViews.
constexpr auto k_CoverView = RenderPass::Interface;

/// How far the creatures that a warm-up draws fizzing have fizzed: half way, which fizzes their eyes too
constexpr float k_CreatureFizz = 0.5f;
/// How frozen the creatures that a warm-up draws fizzing are: wholly, which adds the ice over them
constexpr float k_CreatureFreeze = 1.0f;

/// The loading frames after the last frame of a pipeline prefetch before the count of the pipelines that are not ready
/// is correct. The renderer draws a frame on its own thread while the game makes the next frame.
constexpr uint32_t k_PrefetchSettleFrames = 2;

/// The longest time that the loading screen waits for the pipelines of a prefetch. After this time, the warm-up makes
/// the pipelines that are not ready itself.
constexpr std::chrono::milliseconds k_PrefetchWaitLimit {20000};

/// The time between two loading frames while the loading screen waits for the pipelines of a prefetch. The renderer
/// takes the ready pipelines in each frame. More frames only make more work for the device that makes the pipelines.
constexpr std::chrono::milliseconds k_PrefetchFrameInterval {4};

/// Gives true while the loading screen must wait for the pipelines of a prefetch. framesAfter is the number of loading
/// frames after the prefetch, pending is the number of pipelines that are not ready and waited is the time of the wait.
[[nodiscard]] constexpr bool PrefetchWaits(uint32_t framesAfter, uint32_t pending, std::chrono::milliseconds waited) noexcept
{
	if (waited >= k_PrefetchWaitLimit)
	{
		return false;
	}
	return framesAfter < k_PrefetchSettleFrames || pending > 0;
}

} // namespace openblack::graphics::warm_up
