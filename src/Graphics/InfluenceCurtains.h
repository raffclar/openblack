/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>
#include <span>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "3D/InfluenceCircle.h"
#include "GraphicsHandle.h"

namespace openblack::graphics
{

/// A point of the curtain of an influence circle, as the static buffer of the circle keeps it
struct InfluenceCurtainVertex
{
	glm::vec3 position;
	glm::vec2 uv;
	/// The colour as alpha, blue, green and red bytes. The alpha is full where the border shows and zero where it does
	/// not. The shader multiplies it by the strength of the border in the frame.
	uint32_t colour;
};

/// The points of a circle's curtain in the colour of its player (red, green and blue in the low bytes). Only the middle
/// row shows, and not where the circle is inside another of the player's. A hidden closing column fades to white.
[[nodiscard]] std::vector<InfluenceCurtainVertex> CurtainVertices(const influence::Circle& circle, uint32_t rgb);

/// The values that change each frame for all the curtains: the scroll of the texture, and the strength of the border,
/// of 255. The shader reads them as one vec4.
[[nodiscard]] glm::vec4 CurtainFrameValues(glm::vec2 scroll, uint8_t alpha);

/// The curtains of the influence circles in static buffers. A curtain goes to the GPU again only when its circle or the
/// colour of its player changes. The scroll of the texture and the strength of the border go to the GPU as a uniform.
class InfluenceCurtains
{
public:
	InfluenceCurtains() = default;
	~InfluenceCurtains();
	InfluenceCurtains(const InfluenceCurtains&) = delete;
	InfluenceCurtains& operator=(const InfluenceCurtains&) = delete;
	InfluenceCurtains(InfluenceCurtains&&) = delete;
	InfluenceCurtains& operator=(InfluenceCurtains&&) = delete;

	/// Makes the buffers of the circles that changed after the last update, and frees the buffers of the circles that
	/// are gone. The land gives the colours of the players.
	void Update(std::span<const influence::Circle> circles, int32_t landNumber);
	/// Sets the buffers of a circle of the last update for the next draw of the main encoder. False when the circle has
	/// no buffers.
	[[nodiscard]] bool Bind(size_t circle) const;

private:
	struct Curtain
	{
		influence::Circle circle;
		int32_t landNumber {0};
		std::optional<VertexBufferHandle> vertices;
		std::optional<IndexBufferHandle> indices;
	};
	static void Release(Curtain& curtain);

	std::vector<Curtain> _curtains;
};

} // namespace openblack::graphics
