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

#include "RenderPass.h"

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
                                         RenderPass::GestureHints,
                                         RenderPass::Advisors};

/// The views of the scene that the next frames draw again. Under Vulkan, a warm-up can divide the draws into these
/// views between some frames, because one frame with all of them can use all of the renderer's descriptor sets.
constexpr std::array k_PartedViews {RenderPass::ReflectionSky,
                                    RenderPass::Reflection,
                                    RenderPass::ReflectionTranslucent,
                                    RenderPass::Sky,
                                    RenderPass::Land,
                                    RenderPass::Main,
                                    RenderPass::Translucent,
                                    RenderPass::GestureHintCollar0,
                                    RenderPass::GestureHintCollar1,
                                    RenderPass::GestureHintCollar2,
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

} // namespace openblack::graphics::warm_up
