/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "InfluenceCurtains.h"

#include <algorithm>

#include <bgfx/bgfx.h>

#include "ECS/PlayerColour.h"
#include "GraphicsHandleBgfx.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
bgfx::VertexLayout CurtainLayout()
{
	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
	    .end();
	return layout;
}

/// The same circle and the same colour of its player: its curtain does not change
bool SameCurtain(const influence::Circle& a, const influence::Circle& b)
{
	return a.player == b.player && a.hidden == b.hidden && a.curtain.positions == b.curtain.positions &&
	       a.curtain.uvs == b.curtain.uvs && a.curtain.indices == b.curtain.indices;
}
} // namespace

std::vector<InfluenceCurtainVertex> openblack::graphics::CurtainVertices(const influence::Circle& circle, uint32_t rgb)
{
	const auto& curtain = circle.curtain;
	const auto abgr = ((rgb & 0xFFu) << 16u) | (rgb & 0xFF00u) | ((rgb >> 16u) & 0xFFu);
	const auto lastColumn = circle.Columns() - 1;
	std::vector<InfluenceCurtainVertex> vertices(curtain.positions.size());
	for (size_t i = 0; i < vertices.size(); ++i)
	{
		const auto column = i / 3;
		const bool hidden = circle.hidden.at(column);
		const auto colour = hidden && column == lastColumn ? 0xFFFFFFu : abgr;
		const uint32_t alpha = i % 3 == 1 && !hidden ? 0xFFu : 0u;
		vertices[i] = {.position = curtain.positions[i], .uv = curtain.uvs[i], .colour = (alpha << 24u) | colour};
	}
	return vertices;
}

glm::vec4 openblack::graphics::CurtainFrameValues(glm::vec2 scroll, uint8_t alpha)
{
	return {scroll, static_cast<float>(alpha) / 255.0f, 0.0f};
}

InfluenceCurtains::~InfluenceCurtains()
{
	std::ranges::for_each(_curtains, Release);
}

void InfluenceCurtains::Release(Curtain& curtain)
{
	// The graphics library frees a buffer after the frame, so the draws of this frame that use it stay correct
	if (curtain.vertices.has_value())
	{
		bgfx::destroy(toBgfx(*curtain.vertices));
		curtain.vertices.reset();
	}
	if (curtain.indices.has_value())
	{
		bgfx::destroy(toBgfx(*curtain.indices));
		curtain.indices.reset();
	}
}

void InfluenceCurtains::Update(std::span<const influence::Circle> circles, int32_t landNumber)
{
	while (_curtains.size() > circles.size())
	{
		Release(_curtains.back());
		_curtains.pop_back();
	}
	_curtains.resize(circles.size());
	const auto layout = CurtainLayout();
	for (size_t i = 0; i < circles.size(); ++i)
	{
		const auto& circle = circles[i];
		auto& kept = _curtains[i];
		if (kept.vertices.has_value() && kept.landNumber == landNumber && SameCurtain(kept.circle, circle))
		{
			continue;
		}
		Release(kept);
		kept.circle = circle;
		kept.landNumber = landNumber;
		const auto& curtain = circle.curtain;
		if (curtain.positions.empty() || curtain.indices.empty())
		{
			continue;
		}
		const auto rgb = player_colour::FromTableOnLand(influence::k_PlayerColours, circle.player, landNumber);
		const auto vertices = CurtainVertices(circle, rgb);
		std::vector<uint16_t> indices(curtain.indices.size());
		std::ranges::transform(curtain.indices, indices.begin(), [](uint32_t index) { return static_cast<uint16_t>(index); });
		const auto vertexBuffer = bgfx::createVertexBuffer(
		    bgfx::copy(vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(InfluenceCurtainVertex))), layout);
		const auto indexBuffer =
		    bgfx::createIndexBuffer(bgfx::copy(indices.data(), static_cast<uint32_t>(indices.size() * sizeof(uint16_t))));
		if (bgfx::isValid(vertexBuffer))
		{
			kept.vertices = fromBgfx(vertexBuffer);
		}
		if (bgfx::isValid(indexBuffer))
		{
			kept.indices = fromBgfx(indexBuffer);
		}
		if (!kept.vertices.has_value() || !kept.indices.has_value())
		{
			Release(kept);
		}
	}
}

bool InfluenceCurtains::Bind(size_t circle) const
{
	if (circle >= _curtains.size())
	{
		return false;
	}
	const auto& kept = _curtains[circle];
	if (!kept.vertices.has_value() || !kept.indices.has_value())
	{
		return false;
	}
	bgfx::setVertexBuffer(0, toBgfx(*kept.vertices));
	bgfx::setIndexBuffer(toBgfx(*kept.indices));
	return true;
}
