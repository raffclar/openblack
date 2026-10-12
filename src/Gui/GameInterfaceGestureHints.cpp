/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>

#include "Camera/Camera.h"
#include "ECS/Components/Player.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "GameInterface.h"
#include "Gestures/GestureHints.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"

using namespace openblack::gui;
using openblack::Locator;

namespace
{
/// The player at this computer, whose colour the gestures' pictures are drawn in
constexpr size_t k_LocalPlayer = 0;

glm::vec4 Colour(const std::array<uint8_t, 4>& rgba)
{
	return glm::vec4(rgba.at(0), rgba.at(1), rgba.at(2), rgba.at(3)) / 255.0f;
}
} // namespace

void GameInterface::DrawGestureHints(glm::u16vec2 resolution)
{
	namespace hints = openblack::gesture_hints;
	if (!Locator::gestureSystem::has_value() || !Locator::camera::has_value() || _hintDotSheet == nullptr)
	{
		return;
	}
	const auto& row = Locator::gestureSystem::value().GetHints();
	if (row.hints.empty() || row.alpha <= 0.0f)
	{
		return;
	}
	const auto screen = glm::vec2(resolution);
	const float rowWidth = hints::RowWidth(screen);
	const float fov = Locator::camera::value().GetHorizontalFieldOfView();
	const float pictureSide = hints::SpriteSide(hints::k_PictureHalfSize, rowWidth, fov);
	const float dotSide = hints::SpriteSide(hints::k_DotHalfSize, rowWidth, fov);
	const auto playerRgb = openblack::ecs::components::Player::k_Colours.at(k_LocalPlayer) & 0xFFFFFFu;
	const auto pictureColour = Colour(hints::PictureColour(playerRgb, row.alpha));
	constexpr float k_DotCell = 1.0f / static_cast<float>(hints::k_DotSheetSide);
	const glm::vec2 dotUv {static_cast<float>(hints::k_DotPicture % hints::k_DotSheetSide) * k_DotCell,
	                       static_cast<float>(hints::k_DotPicture / hints::k_DotSheetSide) * k_DotCell};
	constexpr float k_GestureCell = 1.0f / static_cast<float>(hints::k_GestureSheetSide);

	for (size_t slot = 0; slot < row.hints.size(); ++slot)
	{
		const auto& hint = row.hints.at(slot);
		if (hint.path.empty())
		{
			continue;
		}
		const auto square = hints::Square(slot, screen);

		// The gesture's picture in the middle of its square, blended over what is under it
		const auto gesture = static_cast<int>(hint.gesture);
		const auto sheet = static_cast<size_t>(gesture / hints::k_GesturesPerSheet);
		const int cell = gesture % hints::k_GesturesPerSheet;
		if (sheet < _gestureSheets.size() && _gestureSheets.at(sheet) != nullptr)
		{
			const auto centre = hints::PixelOf(square, glm::vec2(0.5f));
			const glm::vec2 uv {static_cast<float>(cell % hints::k_GestureSheetSide) * k_GestureCell,
			                    static_cast<float>(cell / hints::k_GestureSheetSide) * k_GestureCell};
			_canvas.SetBlend(Canvas::Blend::Alpha);
			_canvas.DrawQuad(centre - (pictureSide * 0.5f), centre + (pictureSide * 0.5f), uv, uv + k_GestureCell,
			                 pictureColour, _gestureSheets.at(sheet).get());
		}

		// The dots running along the gesture's shape over it, added to what is under them
		_canvas.SetBlend(Canvas::Blend::Additive);
		for (const float at : hints::DotsAt(hint.dot))
		{
			const auto point = hints::PointAlong(hint.path, at);
			const auto centre = hints::PixelOf(square, {point.x, point.z});
			const auto shade = hints::DotShade(at, row.alpha);
			const auto colour = Colour({shade, shade, shade, hints::k_DotAlpha});
			_canvas.DrawQuad(centre - (dotSide * 0.5f), centre + (dotSide * 0.5f), dotUv, dotUv + k_DotCell, colour,
			                 _hintDotSheet.get());
		}
	}
	_canvas.SetBlend(Canvas::Blend::Alpha);
}
