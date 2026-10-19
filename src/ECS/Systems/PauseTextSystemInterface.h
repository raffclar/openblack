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
#include <vector>

#include <glm/vec2.hpp>

#include "3D/OrientedText.h"
#include "Gui/PauseText.h"

namespace openblack
{
class Camera;
} // namespace openblack

namespace openblack::gui
{
class GameInterface;
} // namespace openblack::gui

namespace openblack::graphics
{
class Texture2D;
} // namespace openblack::graphics

namespace openblack::ecs::systems
{

/// The words at the top of the screen while the game is paused, slower or quicker than normal ("Game Paused", "Slow
/// Down Time", "Speed Up Time"). Each frame the system steps their fade, and the renderer draws their letters in the
/// world over the scene.
class PauseTextSystemInterface
{
public:
	virtual ~PauseTextSystemInterface() = default;

	/// The interface whose texts and font the words come from, none while there is none
	virtual void SetInterface(const gui::GameInterface* interface) = 0;
	/// The frame that the game draws next, some real milliseconds after the last one
	virtual void Update(const gui::pause_text::Situation& situation, uint32_t milliseconds) = 0;
	/// The words to draw at this frame, none while nothing shows
	[[nodiscard]] virtual std::optional<gui::pause_text::Shown> GetShown() const = 0;
	/// The letters to draw for a camera on a screen of a size, none while nothing shows
	[[nodiscard]] virtual std::vector<OrientedTextVertex> GetText(const Camera& camera, glm::u16vec2 screen) const = 0;
	/// The font's texture that the letters are cut from, none without a font
	[[nodiscard]] virtual const graphics::Texture2D* GetTextTexture() const = 0;
};

} // namespace openblack::ecs::systems
