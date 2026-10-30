/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HeadlessRenderer.h"

#include <bgfx/bgfx.h>

#include "ShaderManager.h"

using namespace openblack::graphics;

bool openblack::graphics::GraphicsLibraryStarted() noexcept
{
	// The library keeps its capabilities in its own data from its start until its stop. This read does not start it.
	return bgfx::getInternalData()->caps != nullptr;
}

HeadlessRenderer::HeadlessRenderer() noexcept
    : _shaderManager(std::make_unique<ShaderManager>())
{
}

HeadlessRenderer::~HeadlessRenderer() noexcept = default;
