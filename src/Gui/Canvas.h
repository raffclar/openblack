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
#include <optional>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "Gui/ScreenPixels.h"
#include "Visual/RenderPass.h"

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
	/// Puts the frame's draws into a view that the renderer sorts by depth, among the other draws of that view, rather
	/// than into the canvas's own view. Without a view, they go into the canvas's own view again.
	void SetSortedView(std::optional<graphics::RenderPass> view) noexcept { _sortedView = view; }
	/// The depth by which what is drawn from now on is sorted in the sorted view. Draws of one depth stay in the order
	/// they are drawn in. Back to 0 with each frame
	void SetSortDepth(uint32_t depth) noexcept { _sortDepth = depth; }
	/// A rectangle of the window in pixels, from its left top corner, outside which nothing shows
	struct Clip
	{
		uint16_t x {0};
		uint16_t y {0};
		uint16_t width {0};
		uint16_t height {0};

		bool operator==(const Clip&) const = default;
	};
	/// What is drawn from now on shows only inside a rectangle, its corners in pixels from the canvas's origin. With no
	/// rectangle it shows everywhere again. Back to no rectangle with each frame. The rectangle moves with the corners
	/// of each draw (see SetCorners).
	void SetClip(std::optional<std::array<glm::vec2, 2>> corners) noexcept;
	/// How the draws from now on place their corners on the game's screen. Back to screen_pixels::Corners::AsGiven with
	/// each frame
	void SetCorners(screen_pixels::Corners corners) noexcept { _corners = corners; }
	[[nodiscard]] screen_pixels::Corners GetCorners() const noexcept { return _corners; }
	/// The alpha test of the draws from now on (see graphics::alpha_test): a pixel with less alpha than the reference does
	/// not show. Back to 0, no test, with each frame
	void SetAlphaReference(uint8_t reference) noexcept { _alphaReference = reference; }
	[[nodiscard]] uint8_t GetAlphaReference() const noexcept { return _alphaReference; }
	/// Gives the draws an alpha test reference until the end of a scope, then puts back the reference before it
	class AlphaReferenceScope
	{
	public:
		AlphaReferenceScope(Canvas& canvas, uint8_t reference) noexcept
		    : _canvas(canvas)
		    , _before(canvas.GetAlphaReference())
		{
			_canvas.SetAlphaReference(reference);
		}
		~AlphaReferenceScope() { _canvas.SetAlphaReference(_before); }
		AlphaReferenceScope(const AlphaReferenceScope&) = delete;
		AlphaReferenceScope& operator=(const AlphaReferenceScope&) = delete;
		AlphaReferenceScope(AlphaReferenceScope&&) = delete;
		AlphaReferenceScope& operator=(AlphaReferenceScope&&) = delete;

	private:
		Canvas& _canvas;
		uint8_t _before;
	};
	/// Gives the draws a way to place their corners until the end of a scope, then puts back the way before it
	class CornersScope
	{
	public:
		CornersScope(Canvas& canvas, screen_pixels::Corners corners) noexcept
		    : _canvas(canvas)
		    , _before(canvas.GetCorners())
		{
			_canvas.SetCorners(corners);
		}
		~CornersScope() { _canvas.SetCorners(_before); }
		CornersScope(const CornersScope&) = delete;
		CornersScope& operator=(const CornersScope&) = delete;
		CornersScope(CornersScope&&) = delete;
		CornersScope& operator=(CornersScope&&) = delete;

	private:
		Canvas& _canvas;
		screen_pixels::Corners _before;
	};
	/// A rectangle of a texture, tinted by colour (red, green, blue and alpha from 0 to 1). Without a texture it is
	/// filled with the colour. Corners whose minimum is beyond the maximum flip the texture. All corners are on the
	/// game's screen, where the centre of a pixel is at whole coordinates.
	void DrawQuad(glm::vec2 min, glm::vec2 max, glm::vec2 uvMin, glm::vec2 uvMax, glm::vec4 colour,
	              const graphics::Texture2D* texture);
	/// A filled four sided shape, its corners in order round it, each with its own colour
	void DrawShape(const std::array<glm::vec2, 4>& corners, const std::array<glm::vec4, 4>& colours);
	/// A four sided shape of a texture, its corners in order round it, each with its own place on the texture and colour
	void DrawShape(const std::array<glm::vec2, 4>& corners, const std::array<glm::vec2, 4>& uvs,
	               const std::array<glm::vec4, 4>& colours, const graphics::Texture2D* texture);
	/// A one pixel wide line of the game's line draw between two pixels, along a row or a column. It fills the pixels
	/// that the game fills (see screen_pixels::LinePixels).
	void DrawLine(glm::ivec2 from, glm::ivec2 to, glm::vec4 colour);
	/// Submits the frame's interface
	void End();
	/// Makes the renderer's pipelines for a canvas in a view before the canvas first draws there: a draw that covers no
	/// pixel for each way that a canvas blends, with the depth test of the canvas. With sorted, for a canvas whose draws
	/// go into the view as a sorted view (see SetSortedView).
	static void WarmUp(graphics::RenderPass view, DepthTest depthTest = DepthTest::Off, bool sorted = false);

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
		uint8_t alphaReference;
		uint32_t firstVertex;
		uint32_t vertexCount;
		uint32_t sortDepth;
		std::optional<Clip> clip;
	};

	/// Starts a batch of a texture unless the last batch can take more of it
	void StartBatch(const graphics::Texture2D* texture);
	/// The pixels of the clip rectangle for the draws that place their corners in the current way
	[[nodiscard]] std::optional<Clip> ClipInPixels() const noexcept;
	/// The point in the render target of a corner from the canvas's origin
	[[nodiscard]] glm::vec2 ToTarget(glm::vec2 corner) const noexcept;

	graphics::RenderPass _view;
	glm::u16vec2 _resolution {0, 0};
	Blend _blend {Blend::Alpha};
	uint8_t _alphaReference {0};
	DepthTest _depthTest {DepthTest::Off};
	float _depth {0.0f};
	std::optional<graphics::RenderPass> _sortedView;
	uint32_t _sortDepth {0};
	glm::vec2 _origin {0.0f, 0.0f};
	screen_pixels::Corners _corners {screen_pixels::Corners::AsGiven};
	std::optional<std::array<glm::vec2, 2>> _clip;
	std::vector<Vertex> _vertices;
	std::vector<Batch> _batches;
	std::unique_ptr<graphics::Texture2D> _white;
};

} // namespace openblack::gui
