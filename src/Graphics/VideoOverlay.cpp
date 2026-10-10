/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VideoOverlay.h"

#include <algorithm>
#include <vector>

#include <BinkYuv.h>
#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <glm/vec4.hpp>

#include "Graphics/RendererInterface.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/ShaderProgram.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"

using namespace openblack::graphics;

namespace
{
struct Vertex
{
	float x;
	float y;
	float u;
	float v;
	uint32_t abgr;
};

const bgfx::VertexLayout& Layout()
{
	static const bgfx::VertexLayout k_Layout = [] {
		bgfx::VertexLayout layout;
		layout.begin()
		    .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
		    .end();
		return layout;
	}();
	return k_Layout;
}
} // namespace

VideoOverlay::VideoOverlay()
    : _colourTables(std::make_unique<Texture2D>("VideoColourTables"))
{
	// The tables are small whole numbers, exact in half floats
	const auto tables = bink::MakeShaderLookup();
	std::vector<uint16_t> halves;
	halves.reserve(tables.size() * 4);
	for (const auto& entry : tables)
	{
		for (const int16_t value : entry)
		{
			halves.push_back(bx::halfFromFloat(static_cast<float>(value)));
		}
	}
	_colourTables->Create(256, 2, 1, TextureFormat::RGBA16F, Wrapping::ClampEdge, Filter::Nearest,
	                      bgfx::copy(halves.data(), static_cast<uint32_t>(halves.size() * sizeof(uint16_t))));
	for (auto& plane : _planes)
	{
		plane = std::make_unique<Texture2D>("VideoPlane");
	}
}

VideoOverlay::~VideoOverlay() = default;

void VideoOverlay::Upload(const Planes& planes)
{
	if (planes.y.empty() || planes.yStride == 0 || planes.chromaStride == 0)
	{
		return;
	}
	const std::array<std::span<const uint8_t>, 3> data = {planes.y, planes.u, planes.v};
	const std::array<uint32_t, 3> strides = {planes.yStride, planes.chromaStride, planes.chromaStride};
	for (size_t i = 0; i < 3; ++i)
	{
		const auto size = glm::u16vec2(strides.at(i), data.at(i).size() / strides.at(i));
		if (size != _planeSizes.at(i))
		{
			_planes.at(i)->CreateWithinFrame(size.x, size.y, 1, TextureFormat::R8, Wrapping::ClampEdge, Filter::Nearest,
			                                 nullptr);
			_planeSizes.at(i) = size;
			_uploadedSerial.reset();
		}
	}
	if (_uploadedSerial == planes.serial && _uploadedData == planes.y.data())
	{
		return;
	}
	for (size_t i = 0; i < 3; ++i)
	{
		const auto size = _planeSizes.at(i);
		_planes.at(i)->Update(data.at(i).data(), static_cast<uint32_t>(size.x) * size.y);
	}
	_uploadedSerial = planes.serial;
	_uploadedData = planes.y.data();
}

void VideoOverlay::Draw(RenderPass view, glm::u16vec2 resolution, const Planes& planes, video::ScreenRect rect, uint8_t alpha,
                        bool sixteenBit)
{
	if (alpha == 0 || planes.width == 0 || planes.height == 0 || bgfx::getAvailTransientVertexBuffer(6, Layout()) < 6)
	{
		return;
	}
	Upload(planes);
	const bool hasPicture = !planes.y.empty() && _uploadedSerial.has_value();

	const auto viewId = static_cast<bgfx::ViewId>(view);
	bgfx::setViewMode(viewId, bgfx::ViewMode::Sequential);
	bgfx::setViewRect(viewId, 0, 0, resolution.x, resolution.y);
	std::array<float, 16> projection {};
	bx::mtxOrtho(projection.data(), 0.0f, static_cast<float>(resolution.x), static_cast<float>(resolution.y), 0.0f, 0.0f, 1.0f,
	             0.0f, bgfx::getCaps()->homogeneousDepth);
	bgfx::setViewTransform(viewId, nullptr, projection.data());

	bgfx::TransientVertexBuffer buffer;
	bgfx::allocTransientVertexBuffer(&buffer, 6, Layout());
	const auto x0 = static_cast<float>(rect.x);
	const auto y0 = static_cast<float>(rect.y);
	const auto x1 = static_cast<float>(rect.x + rect.width);
	const auto y1 = static_cast<float>(rect.y + rect.height);
	const uint32_t abgr = (static_cast<uint32_t>(alpha) << 24) | 0x00FFFFFFu;
	const std::array<Vertex, 6> vertices = {{
	    {x0, y0, 0.0f, 0.0f, abgr},
	    {x1, y0, 1.0f, 0.0f, abgr},
	    {x1, y1, 1.0f, 1.0f, abgr},
	    {x0, y0, 0.0f, 0.0f, abgr},
	    {x1, y1, 1.0f, 1.0f, abgr},
	    {x0, y1, 0.0f, 1.0f, abgr},
	}};
	std::ranges::copy(vertices, reinterpret_cast<Vertex*>(buffer.data));
	bgfx::setVertexBuffer(0, &buffer);

	const auto* program = Locator::rendererInterface::value().GetShaderManager().GetShader("Video");
	const glm::vec4 size(static_cast<float>(planes.width), static_cast<float>(planes.height),
	                     static_cast<float>(_planeSizes[0].x), static_cast<float>(_planeSizes[0].y));
	const glm::vec4 chroma(static_cast<float>(_planeSizes[1].x), static_cast<float>(_planeSizes[1].y), 0.0f, 0.0f);
	const glm::vec4 options(sixteenBit ? 1.0f : 0.0f, hasPicture ? 1.0f : 0.0f, 0.0f, 0.0f);
	program->SetUniformValue("u_videoSize", &size);
	program->SetUniformValue("u_videoChroma", &chroma);
	program->SetUniformValue("u_videoOptions", &options);
	if (hasPicture)
	{
		program->SetTextureSampler("s_lumaPlane", 0, *_planes[0]);
		program->SetTextureSampler("s_uPlane", 1, *_planes[1]);
		program->SetTextureSampler("s_vPlane", 2, *_planes[2]);
	}
	program->SetTextureSampler("s_colourTables", 3, *_colourTables);
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
	               BGFX_STATE_BLEND_FUNC_SEPARATE(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA,
	                                              BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA));
	program->Submit(viewId);
}
