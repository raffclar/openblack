/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <memory>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "Graphics/RenderPass.h"

namespace openblack::graphics
{
class Texture2D;
}

namespace openblack::gui
{

/// Draws the game's interface over the scene: textured, tinted rectangles in screen pixels, batched into as few draw
/// calls as the textures allow and submitted to a view, RenderPass::Interface unless another is given.
class Canvas
{
public:
	/// How what is drawn goes over what is under it
	enum class Blend : uint8_t
	{
		Alpha,
		/// Added, by its alpha, as the game's additive weather material is
		Additive,
	};
	/// How what is drawn is tested against the depth that the scene leaves in the view
	enum class DepthTest : uint8_t
	{
		/// It goes over all of the scene
		Off,
		/// It shows only where it is as near as the scene or nearer, with the depth buffer's near values above its far
		/// values
		NearerIsGreater,
		/// The same, with the depth buffer's near values below its far values
		NearerIsLess,
	};

	explicit Canvas(graphics::RenderPass view = graphics::RenderPass::Interface, DepthTest depthTest = DepthTest::Off);
	~Canvas();
	Canvas(const Canvas&) = delete;
	Canvas& operator=(const Canvas&) = delete;

	/// Starts a frame of the interface for a window of a size, blending by alpha
	void Begin(glm::u16vec2 resolution);
	/// Where what is drawn from now on is placed on the window: its positions are taken from this corner rather than
	/// the window's. Back to the window's corner with each frame
	void SetOrigin(glm::vec2 origin) noexcept { _origin = origin; }
	/// How what is drawn from now on blends
	void SetBlend(Blend blend) noexcept { _blend = blend; }
	/// The depth buffer's value of what is drawn from now on. Back to 0 with each frame
	void SetDepth(float depth) noexcept { _depth = depth; }
	/// How the frame's draws are tested against the scene's depth, from this frame on
	void SetDepthTest(DepthTest depthTest) noexcept { _depthTest = depthTest; }
	/// A rectangle of a texture, tinted by colour (red, green, blue and alpha from 0 to 1). Without a texture it is
	/// filled with the colour. Corners whose minimum is beyond the maximum flip the texture.
	void DrawQuad(glm::vec2 min, glm::vec2 max, glm::vec2 uvMin, glm::vec2 uvMax, glm::vec4 colour,
	              const graphics::Texture2D* texture);
	/// A filled four sided shape, its corners in order round it, each with its own colour
	void DrawShape(const std::array<glm::vec2, 4>& corners, const std::array<glm::vec4, 4>& colours);
	/// A four sided shape of a texture, its corners in order round it, each with its own place on the texture and colour
	void DrawShape(const std::array<glm::vec2, 4>& corners, const std::array<glm::vec2, 4>& uvs,
	               const std::array<glm::vec4, 4>& colours, const graphics::Texture2D* texture);
	/// A one pixel wide line between the centres of two pixels, along a row or a column
	void DrawLine(glm::ivec2 from, glm::ivec2 to, glm::vec4 colour);
	/// Submits the frame's interface
	void End();
	/// Makes the renderer's pipelines for a canvas in a view before the canvas first draws there: a draw that covers no
	/// pixel for each way that a canvas blends, with the depth test of the canvas
	static void WarmUp(graphics::RenderPass view, DepthTest depthTest = DepthTest::Off);

	[[nodiscard]] glm::u16vec2 GetResolution() const noexcept { return _resolution; }

private:
	struct Vertex
	{
		float x;
		float y;
		float z;
		float u;
		float v;
		uint32_t abgr;
	};
	struct Batch
	{
		const graphics::Texture2D* texture;
		Blend blend;
		uint32_t firstVertex;
		uint32_t vertexCount;
	};

	graphics::RenderPass _view;
	glm::u16vec2 _resolution {0, 0};
	Blend _blend {Blend::Alpha};
	DepthTest _depthTest {DepthTest::Off};
	float _depth {0.0f};
	glm::vec2 _origin {0.0f, 0.0f};
	std::vector<Vertex> _vertices;
	std::vector<Batch> _batches;
	std::unique_ptr<graphics::Texture2D> _white;
};

} // namespace openblack::gui
