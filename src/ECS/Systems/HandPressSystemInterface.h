/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <optional>

#include <glm/vec2.hpp>

#include "Input/HandPointer.h"
#include "Input/PointerButtons.h"

union SDL_Event;

namespace openblack::ecs::systems
{

/// The buttons of the mouse and the pointer of the hand on this computer, and the routing of each press and let go of
/// the Action and Move buttons. A press goes to the creature hand, the miracles, the fights, the hand that grabs, the
/// leash, the totems or the tip bubble. The turns of the world do not read this system, and the dedicated server does
/// not make it.
class HandPressSystemInterface
{
public:
	/// What the game gives to the routing of one event
	struct PressContext
	{
		/// The temple is active: the hand grips no land and does not take the presses
		bool inTemple {false};
		/// Plays the feedback of a grip of the land, after a press of the Move button that nothing else took
		std::function<void()> playGrabSound;
	};

	virtual ~HandPressSystemInterface() = default;

	/// Routes one event of the window: the buttons, the let go and the press of the Action button, the press of the Move
	/// button, the let go of a fight press and of a creature, and then the flags of the hand. Do this before the game
	/// does the event's own work.
	virtual void Press(const SDL_Event& event, const PressContext& context) = 0;

	/// The buttons of the mouse after the last event
	[[nodiscard]] virtual const input::PointerButtons& GetPointerButtons() const = 0;
	/// The flags of the hand after the last event. The hand starts with no grip until a button goes down.
	[[nodiscard]] virtual const input::HandButtons& GetHandButtons() const = 0;

	/// Gives whether a press of the Action button went since the last call, and clears it. The leash uses it so that a
	/// press that a miracle or a creature took taps nothing else.
	[[nodiscard]] virtual bool TakeActionPress() = 0;
	/// Gives whether the left button went down since the last call, and clears it. A press of the button can click the
	/// dialogue on.
	[[nodiscard]] virtual bool TakeDialogueClick() = 0;

	/// The place of the pointer on the hand when the hand holds a totem, none when it holds no totem
	[[nodiscard]] virtual const std::optional<glm::ivec2>& GetTotemPointer() const = 0;
	virtual void SetTotemPointer(std::optional<glm::ivec2> pointer) = 0;

	/// The cursor that the game works with, in the pixels of the window
	[[nodiscard]] virtual glm::ivec2 GetMousePosition() const = 0;
	/// Sets the cursor and keeps the pointer of the hand as it is
	virtual void SetMousePosition(glm::ivec2 position) = 0;
	/// Puts the cursor at a place, and from now on the hand follows the pointer that the window reports
	virtual void MovePointer(glm::ivec2 position) = 0;
	/// Puts the cursor where the hand goes this frame (see input::HandPointer::Frame)
	virtual void FramePointer(std::optional<glm::ivec2> gamePointer) = 0;
	[[nodiscard]] virtual const input::HandPointer& GetHandPointer() const = 0;
};

} // namespace openblack::ecs::systems
