/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HeadlessRenderer.h"

#include <array>
#include <numeric>

#include <bgfx/bgfx.h>
#include <spdlog/spdlog.h>

#include "ShaderManager.h"

using namespace openblack::graphics;

namespace
{
/// The transient buffers of each frame. The server draws nothing, so they are small.
constexpr uint32_t k_TransientBytes = 64 * 1024;
} // namespace

uint32_t openblack::graphics::GraphicsObjectCount()
{
	const auto* stats = bgfx::getStats();
	const std::array<uint32_t, 10> counts {
	    stats->numTextures,
	    stats->numVertexBuffers,
	    stats->numIndexBuffers,
	    stats->numDynamicVertexBuffers,
	    stats->numDynamicIndexBuffers,
	    stats->numFrameBuffers,
	    stats->numShaders,
	    stats->numPrograms,
	    stats->numUniforms,
	    stats->numOcclusionQueries,
	};
	return std::reduce(counts.begin(), counts.end(), 0u);
}

std::unique_ptr<HeadlessRenderer> HeadlessRenderer::Create() noexcept
{
	// A frame before the start keeps the library on this thread: with no device, a thread of its own does no work
	bgfx::renderFrame();
	bgfx::Init init {};
	init.type = bgfx::RendererType::Noop;
	// One thread submits, and nothing is drawn
	init.limits.maxEncoders = 1;
	init.limits.maxTransientVbSize = k_TransientBytes;
	init.limits.maxTransientIbSize = k_TransientBytes;
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
