/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

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

/// The view that the loading screen draws in. In a warm-up's frame it first clears all of the window again, as the
/// main view clears it. It comes after every view in k_WindowSceneViews.
constexpr auto k_CoverView = RenderPass::Interface;

/// How far the creatures that a warm-up draws fizzing have fizzed: half way, which fizzes their eyes too
constexpr float k_CreatureFizz = 0.5f;

} // namespace openblack::graphics::warm_up
