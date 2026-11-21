/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScreenGlowPass.h"

#include <algorithm>
#include <ranges>
#include <utility>

#include <bgfx/bgfx.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "FrameBuffer.h"
#include "GraphicsHandleBgfx.h"
#include "RendererDetail.h"
#include "ScreenGlow.h"
#include "ScreenshotCapture.h"
#include "ShaderManager.h"
#include "ShaderProgram.h"

using namespace openblack::graphics;

namespace
{
/// The window's passes that go over the glow: the debug windows and the pointer
constexpr std::array k_OverTheGlow {RenderPass::ImGui, RenderPass::Cursor};

[[nodiscard]] constexpr bool OverTheGlow(RenderPass pass)
{
	return std::ranges::find(k_OverTheGlow, pass) != k_OverTheGlow.end();
}

/// The passes that draw the frame that the glow goes over: all the other passes of the window
constexpr auto k_FramePasses = [] {
	std::array<RenderPass, OffscreenScreenshots::k_ScreenPasses.size() - k_OverTheGlow.size()> passes {};
	std::ranges::copy_if(OffscreenScreenshots::k_ScreenPasses, passes.begin(),
	                     [](RenderPass pass) { return !OverTheGlow(pass); });
	return passes;
}();

constexpr uint32_t k_PointSampler = BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_SAMPLER_POINT;

void SetSampler(const ShaderProgram& program, const char* name, uint8_t stage, const Texture2D& texture)
{
	if (const auto sampler = program.FindUniform(name); sampler.has_value())
	{
		program.SetTextureSampler(*sampler, stage, texture, k_PointSampler);
	}
}
} // namespace

ScreenGlowPass::ScreenGlowPass() = default;

ScreenGlowPass::~ScreenGlowPass() = default;

bool ScreenGlowPass::Supported()
{
	if (bgfx::getRendererType() == bgfx::RendererType::Noop)
	{
		return false;
	}
	const auto formats = bgfx::getCaps()->formats[bgfx::TextureFormat::RGBA32F];
	return (formats & BGFX_CAPS_FORMAT_TEXTURE_FRAMEBUFFER) != 0;
}

void ScreenGlowPass::Want(uint8_t strength, std::chrono::milliseconds frameRealTime)
{
	if (strength == 0)
	{
		return;
	}
	_wanted = strength;
	// The game took the points after each frame with the glow. Its frames came at the screen's refresh.
	_stepDue = frame_pace::Due(_pace, frameRealTime);
}

void ScreenGlowPass::Submit(const ShaderManager& shaders, glm::u16vec2 window, std::optional<FrameBufferHandle> windowTarget)
{
	const auto strength = std::exchange(_wanted, std::nullopt);
	const auto* copy = shaders.GetShader("ScreenGlow");
	const auto* step = shaders.GetShader("ScreenGlowStep");
	if (!strength.has_value() || copy == nullptr || step == nullptr || window.x == 0 || window.y == 0 || !Supported())
	{
		return;
	}
	if (uint16_t width = 0, height = 0; _frame)
	{
		_frame->GetSize(width, height);
		if (width != window.x || height != window.y)
		{
			_frame.reset();
		}
	}
	if (!_frame)
	{
		_frame = std::make_unique<FrameBuffer>("ScreenGlowFrame", window.x, window.y, TextureFormat::RGBA8,
		                                       TextureFormat::Depth24Stencil8, 1, Wrapping::ClampEdge);
	}
	for (auto& points : _points)
	{
		if (!points)
		{
			points = std::make_unique<FrameBuffer>("ScreenGlowPoints", static_cast<uint16_t>(screen_glow::k_Side),
			                                       static_cast<uint16_t>(screen_glow::k_Side), TextureFormat::RGBA32F,
			                                       std::nullopt, 1, Wrapping::ClampEdge);
			_hasValues = false;
		}
	}
	for (const auto pass : k_FramePasses)
	{
		_frame->Bind(pass);
	}
	_redirected = true;

	const glm::vec4 u_screenGlow {window.x, window.y, static_cast<float>(*strength) / 255.0f, _hasValues ? 1.0f : 0.0f};
	const auto& frame = _frame->GetColorAttachment();
	const auto& values = _points.at(_current)->GetColorAttachment();
	const glm::vec2 size(window);

	// The frame with the glow into the window, or into the target of a picture
	{
		const auto viewId = static_cast<bgfx::ViewId>(RenderPass::ScreenGlow);
		const auto projection =
		    bgfx::getCaps()->originBottomLeft ? glm::ortho(0.0f, size.x, 0.0f, size.y) : glm::ortho(0.0f, size.x, size.y, 0.0f);
		const glm::mat4 identity(1.0f);
		const bgfx::FrameBufferHandle target =
		    windowTarget.has_value() ? toBgfx(*windowTarget) : bgfx::FrameBufferHandle {bgfx::kInvalidHandle};
		bgfx::setViewFrameBuffer(viewId, target);
		bgfx::setViewRect(viewId, 0, 0, window.x, window.y);
		bgfx::setViewMode(viewId, bgfx::ViewMode::Sequential);
		bgfx::setViewTransform(viewId, glm::value_ptr(identity), glm::value_ptr(projection));
		bgfx::setViewClear(viewId, BGFX_CLEAR_NONE);
		SetSampler(*copy, "s_screenGlowScene", 0, frame);
		SetSampler(*copy, "s_screenGlowPoints", 1, values);
		copy->SetUniformValue("u_screenGlow", &u_screenGlow);
		detail::SubmitLandQuad(viewId, *copy, {0.0f, 0.0f}, size, {0.0f, 0.0f}, {1.0f, 1.0f},
		                       BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
	}

	// The points step towards that frame, into the other target
	if (std::exchange(_stepDue, false))
	{
		const auto next = static_cast<uint8_t>(1 - _current);
		constexpr auto k_Side = static_cast<float>(screen_glow::k_Side);
		const auto viewId = detail::SetUpLandView(RenderPass::ScreenGlowStep, *_points.at(next),
		                                          glm::u16vec2(static_cast<uint16_t>(screen_glow::k_Side)));
		SetSampler(*step, "s_screenGlowScene", 0, frame);
		SetSampler(*step, "s_screenGlowPoints", 1, values);
		step->SetUniformValue("u_screenGlow", &u_screenGlow);
		detail::SubmitLandQuad(viewId, *step, {0.0f, 0.0f}, {k_Side, k_Side}, {0.0f, 0.0f}, {1.0f, 1.0f},
		                       BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
		_current = next;
		_hasValues = true;
	}
}

void ScreenGlowPass::FrameEnded()
{
	if (!std::exchange(_redirected, false))
	{
		return;
	}
	for (const auto pass : k_FramePasses)
	{
		bgfx::setViewFrameBuffer(static_cast<bgfx::ViewId>(pass), BGFX_INVALID_HANDLE);
	}
}

void ScreenGlowPass::Lost()
{
	_hasValues = false;
}
