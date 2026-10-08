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

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/Billboard.h"
#include "Graphics/InputPromptFrame.h"
#include "Help/HelpTextDisplay.h"

// What the end of the frame draws over the scene, decided before the draw (the "overlays" part of the frame
// snapshot): the screen fade and the cinema bars, the help text (the box, the words, the click cue of the help
// system's draw) and the advisor spirits (and their trails). Values only: Game.cpp fills it once a frame before
// DrawScene (FillOverlayFrame) and the Renderer reads it through DrawSceneDesc::overlay; nothing in the draw asks
// help::Get(), help::spirits::Get() or the screen fade (Locator::screenFade).
namespace openblack::graphics
{
class L3DMesh;

/// A world-space vertex of the WorldQuad program (position, uv, ABGR): the spirits' sprites and trails
struct SpiritQuadVertex
{
	float x, y, z, u, v;
	uint32_t abgr;
};

/// The script fade and the bars of ScreenFade (3D/ScreenFade.h) at this frame's resolution
struct FadeOverlay
{
	/// ScreenFade::GetColour, ARGB: nothing is drawn at alpha 0
	uint32_t colour {0};
	/// ScreenFade::LetterboxHeight(W, H, wide screen fraction): each bar's height, 0 = no bars
	int barPixels {0};
};

/// The help text for this frame
struct HelpTextOverlay
{
	/// help::Get() != nullptr (the original's own extra gate is not ported)
	bool active {false};
	/// HelpTextDisplay::Layout at this frame's resolution and bar
	help::TextFrame text;
};

/// One spirit as it is drawn this frame
struct SpiritOverlay
{
	int advisor {0};
	/// true: drawn at the end of the frame, over the scene (in-world blend < 0.5); false: drawn in the frame (>= 0.5)
	bool overlay {false};
	/// The mesh of the .hd's L3D0 (immutable once made; shared so the draw keeps it alive, not game state)
	std::shared_ptr<const L3DMesh> mesh;
	/// The posed bones: world space, the dude's scale included; empty when not posed
	std::vector<glm::mat4> bones;
	/// The instance's model matrix: the identity (the bones are in the world); its [0][3] the fade is the Renderer's
	glm::mat4 model {1.0f};
	/// The object colour's alpha byte, the visibility x 255 with the go-invisible flicker; 0 draws no model
	uint8_t alpha {255};
	/// The explicit colour and specular set before the land light: white and 0 out of the world
	uint32_t colour {0xFFFFFFFFu};
	uint32_t specular {0u};
	/// The in-world blend (0..1), and the point (x, z) where the land light is sampled; set when the blend is not 0.
	/// The Renderer applies help::spirits::Runtime::WorldColour with its own table (the table of this frame is built
	/// inside DrawScene, UpdateLandLight)
	float inWorld {0.0f};
	std::optional<glm::vec2> landLightPoint;
	/// Light 0's position for the overlay, nullopt in the frame. Only the position moves: the light's colour stays
	/// model_light's
	std::optional<glm::vec3> lightPosition;
	/// The halo (good only) then the puff (while it lasts), sprites already turned into world triangles (three
	/// vertices each) with the camera of the spirits' Update; smoke.raw in mode 6
	std::vector<SpiritQuadVertex> sprites;
};

/// The intro light of PLAY_JC_SPECIAL 0 (ecs/IntroSpecial.h) this frame: one entry of the transparency queue (key
/// |keyPoint - camera|^2 in SumOrder::XYZ, the light's head before this frame's step) and what its callback draws
/// there, in order: screen sprites with their near test in the misc0.raw additive material (mode 13), ZFUNC ALWAYS
/// around them when depthAlways. Not a screen overlay: it rides here as the frame's value copy, the Renderer submits it
/// in the main view's queue
struct IntroLightOverlay
{
	bool active {false};
	glm::vec3 keyPoint {0.0f};
	bool depthAlways {false};
	std::vector<billboard::Sprite> sprites;
};

/// The help system's did-you-know bubble as it is laid out this frame (FillOverlayFrame)
struct DidYouKnowOverlay
{
	bool active {false};
	/// The bubble's 9-slice and tail (help::BubbleShape), screen pixels, three vertices each
	std::vector<SpiritQuadVertex> shape;
	/// One line of the text: the run (its alpha the line's top) and the alpha of its bottom edge
	struct Line
	{
		help::TextRun run;
		uint8_t alphaBottom;
	};
	/// The text's lines (shadows first), in the bubble's font j0
	std::vector<Line> lines;
	/// The $g<n> gesture symbols: screen quads of S_Gesture0 / S_Gesture1 (n / 16), three vertices each
	std::array<std::vector<SpiritQuadVertex>, 2> gestures;
	/// The blinking scroll arrows (boxes in the atmosphere material), pixels and uv
	struct Arrow
	{
		float x0, y0, x1, y1, u0, v0, u1, v1;
	};
	std::vector<Arrow> arrows;
	/// The $m<n> icons: key or mouse icon draws (Renderer::DrawKeyOrMouseIn)
	struct KeyIcon
	{
		int32_t animType;
		int32_t clickType;
		int32_t row;
		std::u16string keyName;
		int32_t x;
		int32_t y;
		int32_t size;
		uint8_t alpha;
	};
	std::vector<KeyIcon> keyIcons;
	/// The draw alpha for them: the dark colour's alpha byte / 2, always 0x7F
	uint8_t arrowAlpha {0};
};

/// The overlays of one frame
struct OverlayFrame
{
	/// The resolution everything below was laid out for (the Renderer's Main view, ConfigureView); 0 x 0 = nothing
	int width {0};
	int height {0};
	FadeOverlay fade;
	HelpTextOverlay helpText;
	/// The dudes out of home, frame ones first (DrawSpirits(false) then (true)), each once
	std::vector<SpiritOverlay> spirits;
	/// The rainbow trails, world triangles (three vertices each) of both dudes out of home
	/// and not in the world; rainbow.raw in mode 15
	std::vector<SpiritQuadVertex> spiritTrails;
	/// The key and mouse icons after the frame's two updates (newest first), the hand's point and the tick count, drawn
	/// twice by Renderer::DrawInputPrompts (pass 0 under the bars, pass 1 after the help text). The click cue of the help
	/// system's draw is one of them
	InputPromptFrame inputPrompts;
	/// The intro light (ecs::intro_special::FillFrame)
	IntroLightOverlay introLight;
	/// The did-you-know bubble (drawn by the help system)
	DidYouKnowOverlay didYouKnow;
};

} // namespace openblack::graphics
