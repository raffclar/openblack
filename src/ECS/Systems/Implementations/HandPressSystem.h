/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>
#include <optional>

#include "ECS/Components/InterfaceLimits.h"
#include "ECS/Systems/HandPressSystemInterface.h"
#include "ECS/Systems/Implementations/HandPressWorld.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class HandPressSystem final: public HandPressSystemInterface
{
public:
	/// The routing reads and calls the services of the locator
	HandPressSystem();
	/// The routing reads and calls the services that the caller gives
	explicit HandPressSystem(std::unique_ptr<HandPressWorld> world);
	~HandPressSystem() override;

	void Press(const SDL_Event& event, const PressContext& context) final;

	[[nodiscard]] const input::PointerButtons& GetPointerButtons() const final { return _buttons; }
	[[nodiscard]] const input::HandButtons& GetHandButtons() const final { return _handButtons; }
	[[nodiscard]] bool TakeActionPress() final;
	[[nodiscard]] bool TakeDialogueClick() final;
	[[nodiscard]] bool SelectPressHolds() const final { return _selectPress; }
	void EndSelectPress() final { _selectPress = false; }
	[[nodiscard]] const std::optional<glm::ivec2>& GetTotemPointer() const final { return _totemPointer; }
	void SetTotemPointer(std::optional<glm::ivec2> pointer) final { _totemPointer = pointer; }
	[[nodiscard]] glm::ivec2 GetMousePosition() const final { return _mousePosition; }
	void SetMousePosition(glm::ivec2 position) final { _mousePosition = position; }
	void MovePointer(glm::ivec2 position) final;
	void FramePointer(std::optional<glm::ivec2> gamePointer) final;
	[[nodiscard]] const input::HandPointer& GetHandPointer() const final { return _handPointer; }

	/// The button whose press directs the fight of the player's creature, until it is let go
	[[nodiscard]] const std::optional<creature_fight::Button>& GetFightButton() const { return _fightButton; }

private:
	void ActionLetGo(const SDL_Event& event, bool rightLetGo, bool inTemple, bool handTakesPresses, bool hasGrab,
	                 bool handHoldsThing);
	void ActionPress(const SDL_Event& event, bool inTemple, bool handTakesPresses, bool hasGrab, bool handHoldsThing,
	                 const components::InterfaceLimits& interfaceLimits, bool mayHold);
	void MovePress(const SDL_Event& event, bool inTemple, bool handTakesPresses, const PressContext& context);

	std::unique_ptr<HandPressWorld> _world;
	input::PointerButtons _buttons;
	input::HandButtons _handButtons;
	/// The last press of the Action button went to a miracle in the hand, a creature or another taker, so it taps
	/// nothing else for the leash
	bool _actionPressTaken {false};
	std::optional<creature_fight::Button> _fightButton;
	/// The player let go of the left button since the last frame, which may click the dialogue on
	bool _dialogueClick {false};
	/// The flag of the interface for a press of the select button that holds the land
	bool _selectPress {false};
	std::optional<glm::ivec2> _totemPointer;
	glm::ivec2 _mousePosition {0, 0};
	/// Whether the hand follows the pointer that the window reports yet, or until the mouse first moves the game's own
	input::HandPointer _handPointer;
};

} // namespace openblack::ecs::systems
