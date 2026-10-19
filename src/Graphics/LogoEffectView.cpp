/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LogoEffectView.h"

#include <cstring>

#include <algorithm>
#include <iterator>
#include <span>

#include <L3DFile.h>
#include <bgfx/bgfx.h>
#include <glm/mat4x4.hpp>

#include "3D/L3DSkinFormat.h"
#include "Graphics/RenderPass.h"
#include "Graphics/RendererInterface.h"
#include "Graphics/ShaderManager.h"
#include "Locator.h"

namespace openblack::graphics
{

namespace
{
/// Blends by the alpha. The alpha of the picture stays opaque, so that the picture shows on the screen as it is.
constexpr uint64_t k_AlphaBlend = BGFX_STATE_BLEND_FUNC_SEPARATE(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA,
                                                                 BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA);
constexpr uint64_t k_AdditiveBlend = BGFX_STATE_BLEND_FUNC_SEPARATE(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE,
                                                                    BGFX_STATE_BLEND_ZERO, BGFX_STATE_BLEND_ONE);
/// The sky sphere writes its depth. L3D models face clockwise, so the faces turned away are not drawn.
constexpr uint64_t k_SkyState = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LEQUAL |
                                BGFX_STATE_CULL_CCW | k_AlphaBlend;
/// The dots are tested against the sky's depth and write none, both faces drawn
constexpr uint64_t k_DotState = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_LEQUAL;
constexpr uint32_t k_Black = 0x000000FFU;

[[nodiscard]] uint32_t ToAbgr(uint32_t argb)
{
	return (argb & 0xFF00FF00U) | ((argb & 0xFFU) << 16U) | ((argb >> 16U) & 0xFFU);
}

const bgfx::VertexLayout& Layout()
{
	static const auto layout = [] {
		bgfx::VertexLayout made;
		made.begin()
		    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
		    .end();
		return made;
	}();
	return layout;
}

/// The camera's view: the world turned into the camera's axes, its forward axis the depth
[[nodiscard]] glm::mat4 ViewOf(const LogoEffectView::Scene& scene)
{
	glm::mat4 view(1.0f);
	const std::array axes {scene.right, scene.up, scene.forward};
	for (int axis = 0; axis < 3; ++axis)
	{
		const auto& direction = axes.at(static_cast<size_t>(axis));
		view[0][axis] = direction.x;
		view[1][axis] = direction.y;
		view[2][axis] = direction.z;
		view[3][axis] = scene.offset[axis];
	}
	return view;
}

/// The game's projection: its scales across and up, with no far plane
[[nodiscard]] glm::mat4 ProjectionOf(const LogoEffectView::Scene& scene)
{
	glm::mat4 projection(0.0f);
	projection[0][0] = scene.clipScaleX;
	projection[1][1] = scene.clipScaleY;
	projection[2][3] = 1.0f;
	projection[2][0] = scene.clipShift.x;
	projection[2][1] = scene.clipShift.y;
	if (bgfx::getCaps()->homogeneousDepth)
	{
		projection[2][2] = 1.0f;
		projection[3][2] = -2.0f * scene.nearClip;
	}
	else
	{
		projection[2][2] = 1.0f;
		projection[3][2] = -scene.nearClip;
	}
	return projection;
}
} // namespace

std::unique_ptr<LogoEffectView> LogoEffectView::Create(glm::u16vec2 size, const l3d::L3DFile& sky)
{
	if (sky.GetSubmeshHeaders().empty() || sky.GetPrimitiveSpan(0).empty() || size.x == 0 || size.y == 0)
	{
		return nullptr;
	}
	const auto primitives = sky.GetPrimitiveSpan(0);
	const auto vertices = sky.GetVertexSpan(0);
	const auto indices = sky.GetIndexSpan(0);
	std::vector<Vertex> skyVertices;
	skyVertices.reserve(vertices.size());
	std::ranges::transform(vertices, std::back_inserter(skyVertices), [](const l3d::L3DVertex& vertex) {
		return Vertex {.position = {vertex.position.x, vertex.position.y, vertex.position.z},
		               .uv = {vertex.texCoord.x, vertex.texCoord.y},
		               .abgr = 0xFFFFFFFFU};
	});
	// Each primitive's indices count from its first vertex
	std::vector<uint16_t> skyIndices;
	uint32_t firstVertex = 0;
	uint32_t firstIndex = 0;
	for (const auto& primitive : primitives)
	{
		for (uint32_t i = 0; i < primitive.numTriangles * 3 && firstIndex + i < indices.size(); ++i)
		{
			skyIndices.push_back(static_cast<uint16_t>(indices[firstIndex + i] + firstVertex));
		}
		firstIndex += primitive.numTriangles * 3;
		firstVertex += primitive.numVertices;
	}
	const auto& skins = sky.GetSkins();
	const auto skin = std::ranges::find(skins, primitives.front().material.skinID, &l3d::L3DTexture::id);
	if (skin == skins.end())
	{
		return nullptr;
	}
	auto texture = std::make_unique<Texture2D>("LogoEffectSky");
	const auto texels = std::span(reinterpret_cast<const uint16_t*>(skin->texels.data()), skin->texels.size());
	const auto flags = static_cast<uint32_t>(sky.GetHeader().flags);
	if (!l3d_skin::HasOpaqueSkins(flags))
	{
		texture->Create(l3d::L3DTexture::k_Width, l3d::L3DTexture::k_Height, 1, TextureFormat::BGRA4, Wrapping::Repeat,
		                Filter::Linear, bgfx::copy(skin->texels.data(), static_cast<uint32_t>(sizeof(skin->texels))));
	}
	else if ((bgfx::getCaps()->formats[bgfx::TextureFormat::BGR5A1] & BGFX_CAPS_FORMAT_TEXTURE_2D) != 0)
	{
		// The sky's skin: 5 bits for each colour, opaque
		const auto* memory = bgfx::alloc(static_cast<uint32_t>(texels.size() * sizeof(uint16_t)));
		std::ranges::transform(texels, reinterpret_cast<uint16_t*>(memory->data), l3d_skin::OpaqueTexel);
		texture->Create(l3d::L3DTexture::k_Width, l3d::L3DTexture::k_Height, 1, TextureFormat::BGR5A1, Wrapping::Repeat,
		                Filter::Linear, memory);
	}
	else
	{
		const auto* memory = bgfx::alloc(static_cast<uint32_t>(texels.size() * 4));
		for (size_t i = 0; i < texels.size(); ++i)
		{
			std::ranges::copy(l3d_skin::OpaqueTexelBytes(texels[i]), memory->data + (i * 4));
		}
		texture->Create(l3d::L3DTexture::k_Width, l3d::L3DTexture::k_Height, 1, TextureFormat::RGBA8, Wrapping::Repeat,
		                Filter::Linear, memory);
	}
	return std::unique_ptr<LogoEffectView>(
	    new LogoEffectView(size, std::move(skyVertices), std::move(skyIndices), std::move(texture)));
}

LogoEffectView::LogoEffectView(glm::u16vec2 size, std::vector<Vertex> skyVertices, std::vector<uint16_t> skyIndices,
                               std::unique_ptr<Texture2D> skySkin)
    : _size(size)
    , _target("LogoEffect", size.x, size.y, TextureFormat::RGBA8, TextureFormat::Depth24Stencil8, 1, Wrapping::ClampEdge)
    , _skyVertices(std::move(skyVertices))
    , _skyIndices(std::move(skyIndices))
    , _skySkin(std::move(skySkin))
{
	_target.Bind(RenderPass::LogoEffect);
	_target.Bind(RenderPass::LogoEffectOverlay);
}

LogoEffectView::~LogoEffectView()
{
	// The passes draw to the screen again
	bgfx::setViewFrameBuffer(static_cast<bgfx::ViewId>(RenderPass::LogoEffect), BGFX_INVALID_HANDLE);
	bgfx::setViewFrameBuffer(static_cast<bgfx::ViewId>(RenderPass::LogoEffectOverlay), BGFX_INVALID_HANDLE);
}

void LogoEffectView::Draw(const Scene& scene)
{
	const auto pass = static_cast<bgfx::ViewId>(RenderPass::LogoEffect);
	bgfx::setViewMode(pass, bgfx::ViewMode::Sequential);
	bgfx::setViewRect(pass, 0, 0, _size.x, _size.y);
	// Only the depth is cleared between frames: the colour of the last frame stays under the next
	bgfx::setViewClear(pass, _cleared ? BGFX_CLEAR_DEPTH : BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, k_Black, 1.0f, 0);
	_cleared = true;
	const auto view = ViewOf(scene);
	const auto projection = ProjectionOf(scene);
	bgfx::setViewTransform(pass, &view[0][0], &projection[0][0]);
	bgfx::touch(pass);

	const auto* program = Locator::rendererInterface::value().GetShaderManager().GetShader("Text3D");
	const auto& layout = Layout();
	// The sky sphere, unlit, in the sky's colour
	const auto vertexCount = static_cast<uint32_t>(_skyVertices.size());
	const auto indexCount = static_cast<uint32_t>(_skyIndices.size());
	if (bgfx::getAvailTransientVertexBuffer(vertexCount, layout) >= vertexCount &&
	    bgfx::getAvailTransientIndexBuffer(indexCount) >= indexCount)
	{
		bgfx::TransientVertexBuffer vertices;
		bgfx::TransientIndexBuffer indices;
		bgfx::allocTransientVertexBuffer(&vertices, vertexCount, layout);
		bgfx::allocTransientIndexBuffer(&indices, indexCount);
		const auto colour = ToAbgr(scene.skyColour);
		auto* out = reinterpret_cast<Vertex*>(vertices.data);
		std::ranges::transform(_skyVertices, out, [colour](Vertex vertex) {
			vertex.abgr = colour;
			return vertex;
		});
		std::memcpy(indices.data, _skyIndices.data(), indexCount * sizeof(uint16_t));
		bgfx::setVertexBuffer(0, &vertices);
		bgfx::setIndexBuffer(&indices);
		program->SetTextureSampler("s_texture", 0, *_skySkin);
		bgfx::setState(k_SkyState);
		program->Submit(pass);
	}

	// The dots, in their order, one draw for each run of dots that blend the same way
	if (scene.dotTexture == nullptr || scene.dots.empty())
	{
		return;
	}
	constexpr std::array<uint16_t, 6> k_Corners {0, 1, 2, 0, 2, 3};
	const std::array<glm::vec2, 4> uvs {
	    scene.dotUvMin, {scene.dotUvMax.x, scene.dotUvMin.y}, scene.dotUvMax, {scene.dotUvMin.x, scene.dotUvMax.y}};
	size_t first = 0;
	while (first < scene.dots.size())
	{
		const auto additive = scene.dots[first].additive;
		auto last = first;
		while (last < scene.dots.size() && scene.dots[last].additive == additive)
		{
			++last;
		}
		const auto count = static_cast<uint32_t>((last - first) * k_Corners.size());
		if (bgfx::getAvailTransientVertexBuffer(count, layout) < count)
		{
			return;
		}
		bgfx::TransientVertexBuffer vertices;
		bgfx::allocTransientVertexBuffer(&vertices, count, layout);
		auto* out = reinterpret_cast<Vertex*>(vertices.data);
		for (auto i = first; i < last; ++i)
		{
			const auto& dot = scene.dots[i];
			const auto colour = ToAbgr(dot.argb);
			for (const auto corner : k_Corners)
			{
				*out++ = {.position = dot.corners.at(corner), .uv = uvs.at(corner), .abgr = colour};
			}
		}
		bgfx::setVertexBuffer(0, &vertices);
		program->SetTextureSampler("s_texture", 0, *scene.dotTexture);
		bgfx::setState(k_DotState | (additive ? k_AdditiveBlend : k_AlphaBlend));
		program->Submit(pass);
		first = last;
	}
}

} // namespace openblack::graphics
