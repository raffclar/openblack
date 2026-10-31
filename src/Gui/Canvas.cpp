/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Canvas.h"

#include <algorithm>
#include <array>

#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <glm/common.hpp>

#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/RendererInterface.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/ShaderProgram.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"
#include "Visual/RenderPass.h"

using namespace openblack::gui;
using openblack::graphics::RenderPass;

namespace
{
/// Quads are two triangles of their own six vertices. Batches stay well within a transient buffer.
constexpr uint32_t k_MaxBatchVertices = 0x6000;

uint32_t ToAbgr(glm::vec4 colour)
{
	const auto c = glm::clamp(colour, 0.0f, 1.0f) * 255.0f + 0.5f;
	return (static_cast<uint32_t>(c.a) << 24) | (static_cast<uint32_t>(c.b) << 16) | (static_cast<uint32_t>(c.g) << 8) |
	       static_cast<uint32_t>(c.r);
}

bgfx::VertexLayout& GetLayout()
{
	static bgfx::VertexLayout layout = [] {
		bgfx::VertexLayout result;
		result.begin()
		    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
		    .end();
		return result;
	}();
	return layout;
}

/// The state of a canvas's draws that blend in a way and test the depth in a way. They never write the depth.
constexpr uint64_t StateOf(Canvas::Blend blend, Canvas::DepthTest depthTest = Canvas::DepthTest::Off)
{
	uint64_t test = 0;
	if (depthTest == Canvas::DepthTest::NearerIsGreater)
	{
		test = BGFX_STATE_DEPTH_TEST_GEQUAL;
	}
	else if (depthTest == Canvas::DepthTest::NearerIsLess)
	{
		test = BGFX_STATE_DEPTH_TEST_LEQUAL;
	}
	const auto mix = blend == Canvas::Blend::Additive
	                     ? BGFX_STATE_BLEND_FUNC_SEPARATE(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE,
	                                                      BGFX_STATE_BLEND_ZERO, BGFX_STATE_BLEND_ONE)
	                     : BGFX_STATE_BLEND_FUNC_SEPARATE(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA,
	                                                      BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA);
	return BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | mix | test;
}
} // namespace

Canvas::Canvas(RenderPass view, DepthTest depthTest)
    : _view(view)
    , _depthTest(depthTest)
    , _white(std::make_unique<graphics::Texture2D>("InterfaceWhite"))
{
	static constexpr std::array<uint8_t, 4> k_White = {0xFF, 0xFF, 0xFF, 0xFF};
	_white->Create(1, 1, 1, graphics::TextureFormat::RGBA8, graphics::Wrapping::ClampEdge, graphics::Filter::Nearest,
	               bgfx::copy(k_White.data(), static_cast<uint32_t>(k_White.size())));
}

Canvas::~Canvas() = default;

void Canvas::StartBatch(const graphics::Texture2D* texture)
{
	if (_batches.empty() || _batches.back().texture != texture || _batches.back().blend != _blend ||
	    _batches.back().sortDepth != _sortDepth || _batches.back().vertexCount + 6 > k_MaxBatchVertices)
	{
		_batches.push_back({.texture = texture,
		                    .blend = _blend,
		                    .firstVertex = static_cast<uint32_t>(_vertices.size()),
		                    .vertexCount = 0,
		                    .sortDepth = _sortDepth});
	}
}

void Canvas::Begin(glm::u16vec2 resolution)
{
	_resolution = resolution;
	_vertices.clear();
	_batches.clear();
	_blend = Blend::Alpha;
	_depth = 0.0f;
	_sortDepth = 0;
	_origin = glm::vec2(0.0f);
}

void Canvas::DrawQuad(glm::vec2 min, glm::vec2 max, glm::vec2 uvMin, glm::vec2 uvMax, glm::vec4 colour,
                      const graphics::Texture2D* texture)
{
	if (colour.a <= 0.0f || min.x == max.x || min.y == max.y)
	{
		return;
	}
	const auto* drawn = texture != nullptr ? texture : _white.get();
	StartBatch(drawn);
	const auto abgr = ToAbgr(colour);
	min += _origin;
	max += _origin;
	const Vertex topLeft {.x = min.x, .y = min.y, .z = _depth, .u = uvMin.x, .v = uvMin.y, .abgr = abgr};
	const Vertex topRight {.x = max.x, .y = min.y, .z = _depth, .u = uvMax.x, .v = uvMin.y, .abgr = abgr};
	const Vertex bottomLeft {.x = min.x, .y = max.y, .z = _depth, .u = uvMin.x, .v = uvMax.y, .abgr = abgr};
	const Vertex bottomRight {.x = max.x, .y = max.y, .z = _depth, .u = uvMax.x, .v = uvMax.y, .abgr = abgr};
	_vertices.insert(_vertices.end(), {topLeft, topRight, bottomRight, topLeft, bottomRight, bottomLeft});
	_batches.back().vertexCount += 6;
}

void Canvas::DrawShape(const std::array<glm::vec2, 4>& corners, const std::array<glm::vec4, 4>& colours)
{
	const auto middle = glm::vec2(0.5f);
	DrawShape(corners, {middle, middle, middle, middle}, colours, nullptr);
}

void Canvas::DrawShape(const std::array<glm::vec2, 4>& corners, const std::array<glm::vec2, 4>& uvs,
                       const std::array<glm::vec4, 4>& colours, const graphics::Texture2D* texture)
{
	const auto* drawn = texture != nullptr ? texture : _white.get();
	StartBatch(drawn);
	std::array<Vertex, 4> vertices {};
	for (size_t i = 0; i < 4; ++i)
	{
		vertices.at(i) = {.x = corners.at(i).x + _origin.x,
		                  .y = corners.at(i).y + _origin.y,
		                  .z = _depth,
		                  .u = uvs.at(i).x,
		                  .v = uvs.at(i).y,
		                  .abgr = ToAbgr(colours.at(i))};
	}
	_vertices.insert(_vertices.end(), {vertices[0], vertices[1], vertices[2], vertices[0], vertices[2], vertices[3]});
	_batches.back().vertexCount += 6;
}

void Canvas::DrawLine(glm::ivec2 from, glm::ivec2 to, glm::vec4 colour)
{
	const auto min = glm::min(from, to);
	const auto max = glm::max(from, to) + 1;
	DrawQuad(min, max, {0.0f, 0.0f}, {1.0f, 1.0f}, colour, nullptr);
}

void Canvas::End()
{
	const auto view = static_cast<bgfx::ViewId>(_view);
	bgfx::setViewMode(view, bgfx::ViewMode::Sequential);
	bgfx::setViewClear(view, BGFX_CLEAR_NONE);
	bgfx::setViewRect(view, 0, 0, _resolution.x, _resolution.y);
	std::array<float, 16> projection {};
	bx::mtxOrtho(projection.data(), 0.0f, static_cast<float>(_resolution.x), static_cast<float>(_resolution.y), 0.0f, 0.0f,
	             1.0f, 0.0f, bgfx::getCaps()->homogeneousDepth);
	bgfx::setViewTransform(view, nullptr, projection.data());
	bgfx::touch(view);
	if (_vertices.empty())
	{
		return;
	}

	// In a sorted view the view's own transform is the world camera's, so the draws take the canvas's projection with
	// them
	const auto* shaders = Locator::rendererInterface::value().GetShaderManager();
	const auto* program =
	    shaders != nullptr ? shaders->GetShader(_sortedView.has_value() ? "InterfaceSorted" : "Interface") : nullptr;
	const auto target = _sortedView.has_value() ? static_cast<bgfx::ViewId>(*_sortedView) : view;
	auto& layout = GetLayout();
	for (const auto& batch : _batches)
	{
		if (bgfx::getAvailTransientVertexBuffer(batch.vertexCount, layout) < batch.vertexCount)
		{
			break;
		}
		bgfx::TransientVertexBuffer vertices;
		bgfx::allocTransientVertexBuffer(&vertices, batch.vertexCount, layout);
		std::copy_n(_vertices.begin() + batch.firstVertex, batch.vertexCount, reinterpret_cast<Vertex*>(vertices.data));
		bgfx::setVertexBuffer(0, &vertices);
		program->SetTextureSampler("s_texture", 0, *batch.texture);
		bgfx::setState(StateOf(batch.blend, _depthTest));
		if (_sortedView.has_value())
		{
			program->SetUniformValue("u_canvasProjection", projection.data());
		}
		program->Submit(target, batch.sortDepth);
	}
}

void Canvas::WarmUp(RenderPass view, DepthTest depthTest, bool sorted)
{
	// One triangle with its corners in one place, for each way to blend
	constexpr std::array k_Blends {Blend::Alpha, Blend::Additive};
	constexpr uint32_t k_Corners = 3;
	const auto* shaders = Locator::rendererInterface::value().GetShaderManager();
	const auto* program = shaders != nullptr ? shaders->GetShader(sorted ? "InterfaceSorted" : "Interface") : nullptr;
	auto& layout = GetLayout();
	const std::array<float, 16> noProjection {};
	for (const auto blend : k_Blends)
	{
		if (bgfx::getAvailTransientVertexBuffer(k_Corners, layout) < k_Corners)
		{
			return;
		}
		bgfx::TransientVertexBuffer vertices;
		bgfx::allocTransientVertexBuffer(&vertices, k_Corners, layout);
		std::fill_n(reinterpret_cast<Vertex*>(vertices.data), k_Corners, Vertex {});
		bgfx::setVertexBuffer(0, &vertices);
		bgfx::setState(StateOf(blend, depthTest));
		if (sorted)
		{
			program->SetUniformValue("u_canvasProjection", noProjection.data());
		}
		program->Submit(static_cast<bgfx::ViewId>(view));
	}
}
