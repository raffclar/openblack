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
	/// Gives whether the player let go of the left button since the last call, and clears it. The let go of the button
	/// can click the dialogue on.
	[[nodiscard]] virtual bool TakeDialogueClick() = 0;
	/// Whether the interface keeps the flag of a press of the select button that holds the land (see
	/// input::select_press). The help reads it when the interface goes off or comes back.
	[[nodiscard]] virtual bool SelectPressHolds() const { return false; }
	/// The action of the hand ended, and the interface clears the flag of the select press
	virtual void EndSelectPress() {}
	/// The end of the interface lock stops a grip of the land by the select button. The select button grips nothing again
	/// until the player lets go of it.
	virtual void StopLandGrip() {}
	/// Both buttons ended the interface lock: the select button grips the land at once, as the hand turns the camera.
	/// Gives whether the grip started.
	virtual bool GripLandAgain() { return false; }
	/// Whether the end of the interface lock, or a press in a wait of the interface, stopped the grip of the land, and the
	/// player did not let go of the select button since
	[[nodiscard]] virtual bool LandGripStopped() const { return false; }
	/// Whether the hand grips the land with the grip that both buttons started. This grip keeps no thing.
	[[nodiscard]] virtual bool LandGripKeepsNoThing() const { return false; }

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
