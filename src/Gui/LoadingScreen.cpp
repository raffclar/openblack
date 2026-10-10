/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LoadingScreen.h"

#include <algorithm>
#include <utility>

#include <bgfx/bgfx.h>
#include <spdlog/spdlog.h>

#include "FileSystem/FileSystemInterface.h"
#include "Graphics/RenderPass.h"
#include "Graphics/Texture2D.h"
#include "Graphics/VideoOverlay.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::gui;
using openblack::filesystem::Path;

namespace
{
/// The scripts the game's text is in, in the order the game reads them
constexpr std::array k_TextScripts = {"InfoScript2.txt", "InfoScriptPatch2.txt", "InfoScriptMultiplayer2.txt"};
/// The front end's font
constexpr std::string_view k_Font = "j0";
/// The front end's button atlas, 256 pixels square
constexpr std::string_view k_Atlas = "Front_end_buttons";
constexpr uint32_t k_AtlasSize = 256;
/// The tip pictures
constexpr std::string_view k_TipsFilm = "Data/tips.bik";

/// The screen's colours, as 0xAARRGGBB
constexpr uint32_t k_BandColour = 0xA0808080;
constexpr uint32_t k_ShadeColour = 0xFF000000;
constexpr uint32_t k_SweepColour = 0xCDFFFFFF;
constexpr uint32_t k_DarkColour = 0xFF000000;
/// The red, green and blue of the backdrop and the version number, whose alpha follows the fade
constexpr glm::vec3 k_BackdropColour {200.0f / 255.0f};
/// The bar's outline: white tinted by white, each channel 255 * 255 / 256
constexpr glm::vec3 k_BevelLineColour {254.0f / 255.0f};
/// The bar's body is the colour of one texel of the button atlas, the middle of its second cell
constexpr glm::uvec2 k_BevelTexel {24, 8};
/// The blurred picture is sampled a quarter of its texels in from each edge of its part of the texture
constexpr float k_BlurInset = 1.0f / 64.0f;
/// The game's version when it doesn't say: 1.00
constexpr uint32_t k_DefaultVersion = 100;

/// The pieces of the button atlas the frame's soft shadow is cut from, in 256ths: corners, edges
struct ShadowPiece
{
	glm::vec2 uvMin;
	glm::vec2 uvMax;
};
constexpr ShadowPiece k_TopLeft {{48.0f, 0.0f}, {54.0f, 6.0f}};
constexpr ShadowPiece k_TopRight {{74.0f, 0.0f}, {80.0f, 6.0f}};
constexpr ShadowPiece k_BottomLeft {{48.0f, 26.0f}, {54.0f, 32.0f}};
constexpr ShadowPiece k_BottomRight {{74.0f, 26.0f}, {80.0f, 32.0f}};
constexpr ShadowPiece k_Top {{54.0f, 0.0f}, {74.0f, 6.0f}};
constexpr ShadowPiece k_Bottom {{54.0f, 26.0f}, {74.0f, 32.0f}};
constexpr ShadowPiece k_Left {{48.0f, 6.0f}, {54.0f, 26.0f}};
constexpr ShadowPiece k_Right {{74.0f, 6.0f}, {80.0f, 26.0f}};

std::vector<uint8_t> ReadIfExists(const std::filesystem::path& path)
{
	auto& fileSystem = Locator::filesystem::value();
	if (!fileSystem.Exists(path))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Missing {} for the loading screen", path.generic_string());
		return {};
	}
	return fileSystem.ReadAll(path);
}

glm::vec4 ColourOf(uint32_t argb, uint8_t alpha)
{
	return {static_cast<float>((argb >> 16) & 0xFF) / 255.0f, static_cast<float>((argb >> 8) & 0xFF) / 255.0f,
	        static_cast<float>(argb & 0xFF) / 255.0f, static_cast<float>(alpha) / 255.0f};
}

float AlphaOf(uint8_t alpha)
{
	return static_cast<float>(alpha) / 255.0f;
}
} // namespace

std::unique_ptr<LoadingScreen> LoadingScreen::Create()
{
	auto& fileSystem = Locator::filesystem::value();
	TextDatabase texts;
	for (const auto* script : k_TextScripts)
	{
		texts.AddScript(ReadIfExists(fileSystem.GetPath<Path::Scripts>() / script));
	}
	const auto met = ReadIfExists(fileSystem.GetPath<Path::Data>() / (std::string(k_Font) + ".met"));
	const auto fnt = ReadIfExists(fileSystem.GetPath<Path::Data>() / (std::string(k_Font) + ".fnt"));
	auto font = GameFont::Load(met, fnt);
	const auto colour = ReadIfExists(fileSystem.GetPath<Path::Textures>() / (std::string(k_Atlas) + ".raw"));
	const auto alpha = ReadIfExists(fileSystem.GetPath<Path::Textures>() / (std::string(k_Atlas) + "a.raw"));
	constexpr size_t k_Pixels = static_cast<size_t>(k_AtlasSize) * k_AtlasSize;
	if (!font || colour.size() != k_Pixels * 3 || alpha.size() != k_Pixels)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The loading screen can't be shown without its font and buttons");
		return nullptr;
	}
	std::vector<uint8_t> atlas(k_Pixels * 4);
	for (size_t i = 0; i < k_Pixels; ++i)
	{
		atlas[i * 4] = colour[i * 3];
		atlas[(i * 4) + 1] = colour[(i * 3) + 1];
		atlas[(i * 4) + 2] = colour[(i * 3) + 2];
		atlas[(i * 4) + 3] = alpha[i];
	}
	return std::unique_ptr<LoadingScreen>(new LoadingScreen(std::move(texts), std::move(*font), std::move(atlas)));
}

LoadingScreen::LoadingScreen(TextDatabase texts, GameFont font, std::vector<uint8_t> atlasPixels)
    : _texts(std::move(texts))
    , _font(std::move(font))
    , _atlasPixels(std::move(atlasPixels))
    , _atlas(std::make_unique<graphics::Texture2D>("LoadingScreenButtons"))
    , _fontTexture(std::make_unique<graphics::Texture2D>("LoadingScreenFont"))
    , _overlay(std::make_unique<graphics::VideoOverlay>())
    , _version(loading::VersionText(k_DefaultVersion, 0))
{
	_atlas->Create(k_AtlasSize, k_AtlasSize, 1, graphics::TextureFormat::RGBA8, graphics::Wrapping::ClampEdge,
	               graphics::Filter::Linear, bgfx::copy(_atlasPixels.data(), static_cast<uint32_t>(_atlasPixels.size())));
	// White glyphs, their coverage in alpha
	const auto& coverage = _font.GetAtlas();
	const auto* glyphs = bgfx::alloc(static_cast<uint32_t>(coverage.size() * 4));
	for (size_t i = 0; i < coverage.size(); ++i)
	{
		glyphs->data[i * 4] = 0xFF;
		glyphs->data[(i * 4) + 1] = 0xFF;
		glyphs->data[(i * 4) + 2] = 0xFF;
		glyphs->data[(i * 4) + 3] = coverage[i];
	}
	_fontTexture->Create(_font.GetAtlasSize().x, _font.GetAtlasSize().y, 1, graphics::TextureFormat::RGBA8,
	                     graphics::Wrapping::ClampEdge, graphics::Filter::Linear, glyphs);
	_painter = std::make_unique<DialogPainter>(_canvas, _font, *_atlas, *_fontTexture);
}

LoadingScreen::~LoadingScreen() = default;

void LoadingScreen::PrepareTip(uint32_t milliseconds, bool hasProfiles)
{
	if (_picture)
	{
		return;
	}
	_tip = loading::PickTip(milliseconds, hasProfiles);
	MakePicture();
	_tipShowing = true;
	_tipText = loading::BlankControlCodes(_texts.Get(loading::TipTextName(_tip)));
}

void LoadingScreen::MakePicture()
{
	auto& picture = _picture.emplace();
	picture.still = video::ReadStill(std::filesystem::path(k_TipsFilm), static_cast<uint32_t>(_tip));
	if (!picture.still)
	{
		return;
	}
	// The blur is made from the picture as the game copies it, 5 bits a channel
	const auto& still = *picture.still;
	const auto sixteen = loading::ToSixteenBit(still.bgrx, still.width, still.height, loading::Rgb16::Rgb555);
	const auto blur = loading::BlurPicture(sixteen, still.width, still.height, loading::Rgb16::Rgb555);
	if (blur.texels.empty())
	{
		return;
	}
	const auto* texels = bgfx::alloc(static_cast<uint32_t>(blur.texels.size() * 4));
	for (size_t i = 0; i < blur.texels.size(); ++i)
	{
		const auto rgba = loading::ToRgba8(blur.texels[i], loading::Rgb16::Rgb555);
		std::ranges::copy(rgba, texels->data + (i * 4));
	}
	picture.blur = std::make_unique<graphics::Texture2D>("LoadingScreenBlur");
	picture.blur->Create(256, 256, 1, graphics::TextureFormat::RGBA8, graphics::Wrapping::ClampEdge, graphics::Filter::Linear,
	                     texels);
	picture.blurSize = {blur.width, blur.height};
}

loading::Rect LoadingScreen::ToScreen(const loading::Rect& layout) const noexcept
{
	const auto topLeft = loading::Adjust({layout.left, layout.top}, _resolution.x, _resolution.y);
	const auto bottomRight = loading::Adjust({layout.right, layout.bottom}, _resolution.x, _resolution.y);
	return {.left = topLeft[0], .top = topLeft[1], .right = bottomRight[0], .bottom = bottomRight[1]};
}

void LoadingScreen::DrawColourBox(loading::Rect rect, const std::array<uint32_t, 4>& colours, uint8_t drawAlpha)
{
	const auto left = static_cast<float>(rect.left);
	const auto top = static_cast<float>(rect.top);
	const auto right = static_cast<float>(rect.right);
	const auto bottom = static_cast<float>(rect.bottom);
	std::array<glm::vec4, 4> faded {};
	for (size_t i = 0; i < 4; ++i)
	{
		faded.at(i) = ColourOf(colours.at(i), loading::BoxAlpha(static_cast<uint8_t>(colours.at(i) >> 24), drawAlpha));
	}
	_canvas.DrawShape({glm::vec2 {left, top}, {right, top}, {right, bottom}, {left, bottom}}, faded);
}

float LoadingScreen::DrawText(std::u16string_view text, const loading::Rect& box, float top, float size, glm::vec4 colour,
                              bool draw)
{
	const auto fontSize = size;
	const auto width = static_cast<float>(box.right - box.left);
	const auto lines = _font.Wrap(text, fontSize, width);
	if (draw)
	{
		auto y = top;
		for (const auto& line : lines)
		{
			// Lines that start below the box are left out
			if (y > static_cast<float>(box.bottom))
			{
				break;
			}
			const float x = static_cast<float>(box.left) + (width - _font.GetWidth(line, fontSize)) * 0.5f;
			_painter->DrawString({x, y}, line, fontSize, colour);
			y += fontSize;
		}
	}
	return static_cast<float>(lines.size()) * fontSize;
}

void LoadingScreen::DrawLayoutLine(glm::ivec2 from, glm::ivec2 to, glm::vec4 colour)
{
	const auto a = loading::Adjust({from.x, from.y}, _resolution.x, _resolution.y);
	const auto b = loading::Adjust({to.x, to.y}, _resolution.x, _resolution.y);
	_canvas.DrawLine({a[0], a[1]}, {b[0], b[1]}, colour);
}

void LoadingScreen::DrawBar(const loading::Layout& layout, float progress, uint8_t alpha)
{
	const auto bar = loading::BarFor(layout, progress, _resolution.x, _resolution.y);

	// The bar's bevelled body: the atlas texel's colour, edged two pixels in
	const auto body = ToScreen(bar.body);
	const auto* texel = &_atlasPixels[((k_BevelTexel.y * k_AtlasSize) + k_BevelTexel.x) * 4];
	_canvas.DrawQuad({body.left, body.top}, {body.right, body.bottom}, {0.0f, 0.0f}, {1.0f, 1.0f},
	                 {static_cast<float>(texel[0]) / 255.0f, static_cast<float>(texel[1]) / 255.0f,
	                  static_cast<float>(texel[2]) / 255.0f, AlphaOf(alpha)},
	                 nullptr);
	const glm::vec4 bevelLine(k_BevelLineColour, AlphaOf(alpha));
	const glm::ivec2 topLeft {bar.body.left + 2, bar.body.top + 2};
	const glm::ivec2 bottomRight {bar.body.right - 2, bar.body.bottom - 2};
	DrawLayoutLine(topLeft, {bottomRight.x, topLeft.y}, bevelLine);
	DrawLayoutLine(bottomRight, {topLeft.x, bottomRight.y}, bevelLine);
	DrawLayoutLine({topLeft.x, bottomRight.y}, topLeft, bevelLine);
	DrawLayoutLine({bottomRight.x, topLeft.y}, bottomRight, bevelLine);

	// The bright sweep, then the dark one running over it
	const int32_t barLeft = bar.outline.left + 3;
	DrawColourBox(ToScreen({.left = barLeft, .top = bar.sweepTop, .right = bar.brightStart, .bottom = bar.brightBottom}),
	              {k_SweepColour, k_SweepColour, k_SweepColour, k_SweepColour}, alpha);
	DrawColourBox(ToScreen({.left = bar.brightStart, .top = bar.sweepTop, .right = bar.brightEnd, .bottom = bar.brightBottom}),
	              {k_SweepColour, 0, 0, k_SweepColour}, alpha);
	DrawColourBox(ToScreen({.left = barLeft, .top = bar.sweepTop, .right = bar.darkStart, .bottom = bar.darkBottom}),
	              {k_DarkColour, k_DarkColour, k_DarkColour, k_DarkColour}, alpha);
	DrawColourBox(ToScreen({.left = bar.darkStart, .top = bar.sweepTop, .right = bar.darkEnd, .bottom = bar.darkBottom}),
	              {k_DarkColour, 0, 0, k_DarkColour}, alpha);
	DrawColourBox(ToScreen(bar.topShadow), {k_DarkColour, k_DarkColour, 0, 0}, alpha);
	DrawColourBox(ToScreen(bar.leftShadow), {k_DarkColour, 0, 0, k_DarkColour}, alpha);

	// The picture's frame: a soft shadow the border wide, at half strength
	const auto border = bar.border;
	const loading::Rect outer {.left = bar.outline.left - border,
	                           .top = bar.outline.top - border,
	                           .right = bar.outline.right + border,
	                           .bottom = bar.outline.bottom + border};
	const glm::vec4 shadow(1.0f, 1.0f, 1.0f, AlphaOf(static_cast<uint8_t>(alpha / 2)));
	const auto piece = [this, &shadow](loading::Rect layoutRect, const ShadowPiece& part) {
		const auto rect = ToScreen(layoutRect);
		_canvas.DrawQuad({rect.left, rect.top}, {rect.right, rect.bottom}, part.uvMin / 256.0f, part.uvMax / 256.0f, shadow,
		                 _atlas.get());
	};
	piece({outer.left, outer.top, outer.left + border, outer.top + border}, k_TopLeft);
	piece({outer.right - border, outer.top, outer.right, outer.top + border}, k_TopRight);
	piece({outer.left, outer.bottom - border, outer.left + border, outer.bottom}, k_BottomLeft);
	piece({outer.right - border, outer.bottom - border, outer.right, outer.bottom}, k_BottomRight);
	piece({outer.left + border, outer.top, outer.right - border, outer.top + border}, k_Top);
	piece({outer.left + border, outer.bottom - border, outer.right - border, outer.bottom}, k_Bottom);
	piece({outer.left, outer.top + border, outer.left + border, outer.bottom - border}, k_Left);
	piece({outer.right - border, outer.top + border, outer.right, outer.bottom - border}, k_Right);

	// and a white line round the picture
	const glm::vec4 white(1.0f, 1.0f, 1.0f, AlphaOf(alpha));
	const auto& o = bar.outline;
	DrawLayoutLine({o.left, o.top}, {o.right, o.top}, white);
	DrawLayoutLine({o.left, o.top}, {o.left, o.bottom}, white);
	DrawLayoutLine({o.right, o.top}, {o.right, o.bottom}, white);
	DrawLayoutLine({o.left, o.bottom}, {o.right, o.bottom}, white);
}

void LoadingScreen::DrawTips(glm::u16vec2 resolution, float fade, float progress)
{
	if (fade <= 0.0f)
	{
		return;
	}
	// A tip whose picture has gone gets it back
	if (!_picture)
	{
		MakePicture();
	}
	_resolution = resolution;
	const int32_t width = resolution.x;
	const int32_t height = resolution.y;
	const auto layout = loading::LayoutFor(width, height);
	const auto levels = loading::FadeOf(fade);
	const float alpha = AlphaOf(levels.alpha);
	const auto& tip = *_picture;
	const auto& picture = layout.picture;
	const video::ScreenRect pictureRect {
	    .x = picture.left, .y = picture.top, .width = picture.right - picture.left, .height = picture.bottom - picture.top};

	_canvas.Begin(resolution);
	if (tip.blur)
	{
		const glm::vec2 blurMin(k_BlurInset);
		const glm::vec2 blurMax = glm::vec2(tip.blurSize) / 256.0f - k_BlurInset;
		// The blurred picture over the whole screen, darkening to black down its lower half
		_canvas.DrawQuad({0.0f, 0.0f}, glm::vec2(resolution), blurMin, blurMax, glm::vec4(k_BackdropColour, alpha),
		                 tip.blur.get());
		DrawColourBox({.left = 0, .top = height / 2, .right = width, .bottom = height}, {0, 0, k_ShadeColour, k_ShadeColour},
		              levels.alpha);
		// The picture blurred, then sharp over it
		if (levels.sharp < 1.0f)
		{
			_canvas.DrawQuad({picture.left, picture.top}, {picture.right, picture.bottom}, blurMin, blurMax,
			                 glm::vec4(1.0f, 1.0f, 1.0f, alpha), tip.blur.get());
		}
		if (levels.sharp > 0.0f)
		{
			_canvas.End();
			_overlay->Draw(graphics::RenderPass::Interface, resolution, tip.still->GetPlanes(), pictureRect, levels.sharpAlpha,
			               true);
			_canvas.Begin(resolution);
		}
	}
	else if (tip.still)
	{
		_canvas.End();
		_overlay->Draw(graphics::RenderPass::Interface, resolution, tip.still->GetPlanes(), pictureRect, levels.alpha, true);
		_canvas.Begin(resolution);
	}

	// The band behind the text, fading in from the left and out to the right
	const auto& band = layout.band;
	DrawColourBox({.left = 0, .top = band.top, .right = width / 5, .bottom = band.bottom}, {0, k_BandColour, k_BandColour, 0},
	              levels.alpha);
	DrawColourBox({.left = width / 5, .top = band.top, .right = 4 * width / 5, .bottom = band.bottom},
	              {k_BandColour, k_BandColour, k_BandColour, k_BandColour}, levels.alpha);
	DrawColourBox({.left = 4 * width / 5, .top = band.top, .right = width, .bottom = band.bottom},
	              {k_BandColour, 0, 0, k_BandColour}, levels.alpha);

	// The tip, as big as fits the band, with its shadow
	const auto& box = layout.text;
	const glm::vec4 textColour(1.0f, 1.0f, 1.0f, alpha);
	const int32_t size = loading::FitTextSize(layout, [this, &box, &textColour](int32_t at) {
		return DrawText(_tipText, box, static_cast<float>(box.top), static_cast<float>(at), textColour, false);
	});
	const auto textSize = static_cast<float>(size);
	const float textHeight = DrawText(_tipText, box, static_cast<float>(box.top), textSize, textColour, false);
	const float offset = (static_cast<float>(box.bottom - box.top) - textHeight) * 0.5f;
	const loading::Rect shadowBox {.left = box.left + 2, .top = box.top + 2, .right = box.right + 2, .bottom = box.bottom + 2};
	DrawText(_tipText, shadowBox, static_cast<float>(box.top + 2) + offset, textSize,
	         glm::vec4(0.0f, 0.0f, 0.0f, AlphaOf(static_cast<uint8_t>(levels.alpha / 2))), true);
	DrawText(_tipText, box, static_cast<float>(box.top) + offset, textSize, textColour, true);

	// The version, in the bottom right corner
	const float versionWidth = _font.GetWidth(_version, layout.versionSize);
	_painter->DrawString(
	    {static_cast<float>(width) - versionWidth - 2.0f, static_cast<float>(height) - layout.versionSize - 2.0f}, _version,
	    layout.versionSize, glm::vec4(k_BackdropColour, alpha));

	DrawBar(layout, progress, levels.alpha);
	_canvas.End();
}

void LoadingScreen::DrawLayoutText(std::u16string_view text, loading::Rect layout, glm::vec4 colour)
{
	if (text.empty())
	{
		return;
	}
	const auto placed = loading::TextBoxFor(layout, layout.bottom, loading::k_BigTextSize, _resolution.x, _resolution.y);
	const auto& box = placed.box;
	auto start = placed.start;
	const float height = DrawText(text, box, static_cast<float>(start), placed.size, colour, false);
	if (height < static_cast<float>(box.bottom - box.top))
	{
		start = static_cast<int32_t>((static_cast<float>(box.bottom - box.top) - height) * 0.5f + static_cast<float>(box.top));
	}
	DrawText(text, box, static_cast<float>(start), placed.size, colour, true);
}

void LoadingScreen::DrawPleaseWait(glm::u16vec2 resolution, uint8_t alpha)
{
	_resolution = resolution;
	constexpr uint32_t k_Shade = 0x9F000000;
	constexpr uint32_t k_Edge = 0xFF202020;
	constexpr uint32_t k_Body = 0xFF404040;
	constexpr uint32_t k_Line = 0xF0F0F0;
	const auto banner = loading::PleaseWaitBannerFor(resolution.x, resolution.y);
	const auto& band = banner.band;
	const int32_t width = resolution.x;
	_canvas.Begin(resolution);
	DrawColourBox(banner.shadowAbove, {0, 0, k_Shade, k_Shade}, alpha);
	DrawColourBox(banner.shadowBelow, {k_Shade, k_Shade, 0, 0}, alpha);
	DrawColourBox({.left = band.left, .top = band.top, .right = banner.edge, .bottom = band.bottom},
	              {k_Edge, k_Body, k_Body, k_Edge}, alpha);
	DrawColourBox({.left = banner.edge, .top = band.top, .right = width - banner.edge, .bottom = band.bottom},
	              {k_Body, k_Body, k_Body, k_Body}, alpha);
	DrawColourBox({.left = width - banner.edge, .top = band.top, .right = width, .bottom = band.bottom},
	              {k_Body, k_Edge, k_Edge, k_Body}, alpha);
	// The lines along it take the banner's alpha
	const auto line = ColourOf(k_Line, alpha);
	_canvas.DrawLine({band.left, band.top}, {band.right, band.top}, line);
	_canvas.DrawLine({band.left, band.bottom}, {band.right, band.bottom}, line);
	// "Loading...", with its shadow, both at the banner's alpha
	const auto text = _texts.Get(loading::k_PleaseWaitTextName);
	DrawLayoutText(text, loading::k_PleaseWaitShadow, glm::vec4(0.0f, 0.0f, 0.0f, AlphaOf(alpha)));
	DrawLayoutText(text, loading::k_PleaseWaitText, glm::vec4(1.0f, 1.0f, 1.0f, AlphaOf(alpha)));
	_canvas.End();
}

void LoadingScreen::DrawPicture(glm::u16vec2 resolution, const graphics::VideoOverlay::Planes& planes, video::ScreenRect rect,
                                uint8_t alpha)
{
	_canvas.Begin(resolution);
	_canvas.End();
	_overlay->Draw(graphics::RenderPass::Interface, resolution, planes, rect, alpha, true);
}
