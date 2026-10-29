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
#include <spdlog/spdlog.h>

#include "ShaderManager.h"

using namespace openblack::graphics;

std::unique_ptr<HeadlessRenderer> HeadlessRenderer::Create() noexcept
{
	// A frame before the start keeps the library on this thread: with no device, a thread of its own does no work
	bgfx::renderFrame();
	bgfx::Init init {};
	init.type = bgfx::RendererType::Noop;
	if (!bgfx::init(init))
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("graphics"), "Failed to initialize bgfx with no device.");
		return nullptr;
	}
	return std::make_unique<HeadlessRenderer>(Started {});
}

HeadlessRenderer::HeadlessRenderer(Started /*started*/) noexcept
    : _shaderManager(std::make_unique<ShaderManager>())
{
}

HeadlessRenderer::~HeadlessRenderer() noexcept
{
	_shaderManager.reset();
	bgfx::shutdown();
}

void HeadlessRenderer::Frame() noexcept
{
	bgfx::frame();
}
