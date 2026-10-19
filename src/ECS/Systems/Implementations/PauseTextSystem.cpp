/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "PauseTextSystem.h"

#include <string_view>

#include <glm/mat4x4.hpp>
#include <glm/matrix.hpp>

#include "Camera/Camera.h"
#include "Gui/GameInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
namespace pause_text = openblack::gui::pause_text;

namespace
{

/// Where the camera is and its axes, as the scene is drawn from it
logo_effect::View ViewOf(const Camera& camera)
{
	const auto view = camera.GetViewMatrix(Camera::Interpolation::Current);
	return {.position = glm::vec3(glm::inverse(view)[3]),
	        .right = {view[0][0], view[1][0], view[2][0]},
	        .up = {view[0][1], view[1][1], view[2][1]},
	        .forward = {view[0][2], view[1][2], view[2][2]},
	        .offset = glm::vec3(view[3])};
}

} // namespace

void PauseTextSystem::Update(const pause_text::Situation& situation, uint32_t milliseconds)
{
	_shown = _fader.Step(situation, milliseconds);
}

std::vector<OrientedTextVertex> PauseTextSystem::GetText(const Camera& camera, glm::u16vec2 screen) const
{
	std::vector<OrientedTextVertex> vertices;
	if (!_shown.has_value() || _interface == nullptr || screen.x == 0 || screen.y == 0)
	{
		return vertices;
	}
	const auto& font = _interface->GetFont();
	const std::u16string_view text = _interface->GetTexts().Get(pause_text::k_TextNames.at(static_cast<size_t>(_shown->kind)));
	if (text.empty())
	{
		return vertices;
	}
	const logo_effect::Screen size {.width = screen.x, .height = screen.y};
	const auto view = ViewOf(camera);
	const auto lens = logo_effect::LensOf(camera.GetHorizontalFieldOfView(), camera.GetNearClip(), size);
	const float tilt = pause_text::Tilt(view, lens, size);
	const auto layoutWidth = pause_text::LayoutWidth(size);
	const auto alpha = pause_text::AlphaOf(_shown->eased);
	for (size_t i = 0; i < text.size(); ++i)
	{
		const auto letter = pause_text::PlaceLetter(i, text.size(), *_shown, size, layoutWidth, tilt);
		const auto frame = pause_text::FrameOf(letter, view, lens, size);
		const auto character = text.substr(i, 1);
		const float offset = pause_text::LetterOffset(font.Advance(character.front()));
		AppendOrientedText(vertices, font, frame, 0, 1, character, {offset, pause_text::k_LetterRaise, 0.0f},
		                   pause_text::k_LetterSize, 1.0f, {255, 255, 255, alpha});
	}
	return vertices;
}

const graphics::Texture2D* PauseTextSystem::GetTextTexture() const
{
	return _interface != nullptr ? &_interface->GetFontTexture() : nullptr;
}
