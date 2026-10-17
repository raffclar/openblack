/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the grids of light probes and the shade from the scene's depth

#include "LightProbesView.h"

using namespace openblack::graphics;
namespace gi = openblack::graphics::global_illumination;

LightProbesView::LightProbesView()
    : grids {std::make_unique<OwnedTarget>("Light Probes A", Size().x, Size().y, bgfx::TextureFormat::RGBA16F),
             std::make_unique<OwnedTarget>("Light Probes B", Size().x, Size().y, bgfx::TextureFormat::RGBA16F)}
    , blended("Light Probes", Size().x, Size().y, bgfx::TextureFormat::RGBA16F)
{
}

glm::u16vec2 LightProbesView::Size()
{
	return {gi::k_ProbesPerSide * 3, gi::k_ProbesPerSide};
}
