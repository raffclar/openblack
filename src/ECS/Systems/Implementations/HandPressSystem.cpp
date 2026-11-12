/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "HandPressSystem.h"

#include <utility>

#include <SDL.h>
#include <glm/common.hpp>

#include "Camera/Camera.h"
#include "Common/MachineClock.h"
#include "Creature/CreatureHandRules.h"
#include "Debug/DebugGuiInterface.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/PuzzleTotemSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TipBubbleSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "GameDetail.h"
#include "Hand/HandVisibility.h"
#include "Input/SelectPress.h"
#include "Locator.h"
#include "ScriptHeaders/ScriptInterfaceLevels.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{

/// What a button event does to the flag of the select press. The left button is the select button, and the right
/// button is the other button. The second click of a double click comes as an event with two clicks.
[[nodiscard]] input::select_press::Event SelectPressEvent(const SDL_Event& event)
{
	using input::select_press::Event;
	if (event.type != SDL_MOUSEBUTTONDOWN && event.type != SDL_MOUSEBUTTONUP)
	{
		return Event::Other;
	}
	const bool down = event.type == SDL_MOUSEBUTTONDOWN;
	if (event.button.button == SDL_BUTTON_LEFT)
	{
		if (!down)
		{
			return Event::SelectLetGo;
		}
		return event.button.clicks >= 2 ? Event::SelectDoublePress : Event::SelectPress;
	}
	if (event.button.button == SDL_BUTTON_RIGHT && down)
	{
		return Event::OtherPress;
	}
	return Event::Other;
}

/// The services of the locator, as the routing of the presses uses them
class LocatorHandPressWorld final: public HandPressWorld
{
public:
	[[nodiscard]] bool HandTakesPresses() const final
	{
		return hand_visibility::TakesPresses(Locator::cinematicDirectorSystem::value().IsInterfaceActive());
	}
	[[nodiscard]] bool MouseOverDebugWindow() const final { return Locator::debugGui::value().IsMouseOverWindow(); }
	[[nodiscard]] ecs::components::InterfaceLimits Limits() const final
	{
		ecs::components::InterfaceLimits interfaceLimits {};
		if (Locator::playerSystem::has_value())
		{
			// In a network game the input can come before the first turn makes the player of this computer
			interfaceLimits = script::interface_levels::LimitsOf(
			    Locator::entitiesRegistry::value(), Locator::playerSystem::value().FindPlayer(game_detail::LocalInputPlayer()));
		}
		return interfaceLimits;
	}
	[[nodiscard]] bool Paused() const final { return Locator::time::value().IsPaused(); }
	[[nodiscard]] uint32_t Ticks() const final { return machine_clock::Ticks(); }
	[[nodiscard]] uint32_t Turn() const final { return Locator::time::value().GetTurn(); }
	[[nodiscard]] PlayerNames Player() const final { return game_detail::LocalInputPlayer(); }
	void Ray(glm::vec2 cursor, glm::vec3& origin, glm::vec3& direction) const final
	{
		const auto screenSize = Locator::windowing::value().GetSize();
		Locator::camera::value().DeprojectScreenToWorld(cursor / static_cast<glm::vec2>(glm::max(screenSize, glm::ivec2(1))),
		                                                origin, direction);
	}
	[[nodiscard]] bool HasPicking() const final { return Locator::pickingSystem::has_value(); }
	[[nodiscard]] std::optional<entt::entity> PickedObject() const final
	{
		if (Locator::pickingSystem::has_value())
		{
			return Locator::pickingSystem::value().GetPick().object;
		}
		return std::nullopt;
	}
	[[nodiscard]] std::optional<glm::vec3> PickedPoint() const final { return Locator::pickingSystem::value().GetPick().point; }
	[[nodiscard]] bool HasScriptHighlight(entt::entity thing) const final
	{
		return Locator::entitiesRegistry::value().Valid(thing) &&
		       Locator::entitiesRegistry::value().AllOf<ecs::components::ScriptHighlight>(thing);
	}

	[[nodiscard]] bool TipBubbleGripped() const final
	{
		return Locator::tipBubbleSystem::has_value() && Locator::tipBubbleSystem::value().IsGripped();
	}
	void LetGoOfTipBubble() final { Locator::tipBubbleSystem::value().LetGo(); }
	[[nodiscard]] bool TipBubbleUnderHand() const final
	{
		return Locator::tipBubbleSystem::has_value() && Locator::tipBubbleSystem::value().IsUnderHand();
	}
	void GripTipBubble() final { Locator::tipBubbleSystem::value().Grip(); }

	[[nodiscard]] bool HasTownTotems() const final { return Locator::villageTotemSystem::has_value(); }
	[[nodiscard]] bool TownTotemGripped() const final
	{
		return Locator::villageTotemSystem::has_value() && Locator::villageTotemSystem::value().GetGripped().has_value();
	}
	void LetGoOfTownTotem() final { Locator::villageTotemSystem::value().LetGo(); }
	[[nodiscard]] std::optional<entt::entity> TownTotemOf(entt::entity picked) const final
	{
		return Locator::villageTotemSystem::value().TotemOf(picked);
	}
	bool GripTownTotem(entt::entity totem, PlayerNames player) final
	{
		return Locator::villageTotemSystem::value().Grip(totem, player);
	}
	[[nodiscard]] bool HasPuzzleTotems() const final { return Locator::puzzleTotemSystem::has_value(); }
	[[nodiscard]] bool PuzzleTotemGripped() const final
	{
		return Locator::puzzleTotemSystem::has_value() && Locator::puzzleTotemSystem::value().GetGripped().has_value();
	}
	void LetGoOfPuzzleTotem() final { Locator::puzzleTotemSystem::value().LetGo(); }
	bool GripPuzzleTotem(entt::entity totem, PlayerNames player, glm::vec3 hand) final
	{
		return Locator::puzzleTotemSystem::value().Grip(totem, player, hand);
	}

	[[nodiscard]] bool HasHandGrab() const final { return Locator::handGrabSystem::has_value(); }
	[[nodiscard]] bool HandHoldsThing() const final { return Locator::handGrabSystem::value().IsBusy(); }
	bool GrabPress(uint32_t nowMs, uint32_t turn) final { return Locator::handGrabSystem::value().Press(nowMs, turn); }
	void GrabRelease(uint32_t nowMs, uint32_t turn) final
	{
		// TODO(hand): a press too short to take the thing taps it
		[[maybe_unused]] const auto tapped = Locator::handGrabSystem::value().Release(nowMs, turn);
	}
	void GrabClickReleased() final { Locator::handGrabSystem::value().ClickReleased(); }
	void GrabClickThing(entt::entity thing) final { Locator::handGrabSystem::value().ClickThing(thing); }

	[[nodiscard]] bool MagicHandBusy() const final { return Locator::magicSystem::value().IsHandBusy(); }
	bool MagicPressAction() final { return Locator::magicSystem::value().PressAction(); }
	void MagicReleaseAction() final { Locator::magicSystem::value().ReleaseAction(); }
	bool MagicPressFireBall(uint32_t nowMs, uint32_t turn) final
	{
		return Locator::magicSystem::value().PressFireBall(nowMs, turn);
	}
	void MagicReleaseFireBall(uint32_t nowMs, uint32_t turn) final
	{
		Locator::magicSystem::value().ReleaseFireBall(nowMs, turn);
	}

	[[nodiscard]] leash_snap::Hand RopeHand(PlayerNames player) const final
	{
		return Locator::leashSystem::value().RopeHand(player);
	}
	void LetGoOfRope(PlayerNames player, std::optional<glm::vec3> place) final
	{
		Locator::leashSystem::value().LetGoOfRope(player, place);
	}
	void LeashTapThing(PlayerNames player, entt::entity thing) final { Locator::leashSystem::value().TapThing(player, thing); }
	bool LeashPressAction(PlayerNames player, const glm::vec3& rayOrigin, const glm::vec3& rayDirection, glm::vec2 cursor,
	                      uint32_t nowMs) final
	{
		return Locator::leashSystem::value().PressAction(player, rayOrigin, rayDirection, cursor, nowMs);
	}
	bool PressRope(PlayerNames player) final { return Locator::leashSystem::value().PressRope(player); }
	[[nodiscard]] std::optional<entt::entity> PlayersCreature(PlayerNames player) const final
	{
		return Locator::leashSystem::value().PlayersCreature(player);
	}
	[[nodiscard]] bool IsLeashed(entt::entity creature) const final
	{
		return Locator::leashSystem::value().IsLeashed(creature);
	}
	[[nodiscard]] std::optional<glm::vec3> HolderPoint(entt::entity creature) const final
	{
		return Locator::leashSystem::value().HolderPoint(creature);
	}

	[[nodiscard]] std::optional<entt::entity> HeldCreature() const final
	{
		return Locator::creatureHandSystem::value().GetCreature();
	}
	void ReleaseCreature() final { Locator::creatureHandSystem::value().Release(); }
	std::optional<entt::entity> ReleaseWait() final { return Locator::creatureHandSystem::value().ReleaseWait(); }
	[[nodiscard]] bool WaitsForLockEnd() const final { return Locator::creatureHandSystem::value().WaitsForLockEnd(); }
	[[nodiscard]] std::optional<entt::entity> CreatureUnderCursor() const final
	{
		return Locator::creatureHandSystem::value().CreatureUnderCursor();
	}
	[[nodiscard]] bool MayHoldCreature(entt::entity creature) const final
	{
		return Locator::creatureHandSystem::value().MayHold(creature);
	}
	void GrabCreature() final { Locator::creatureHandSystem::value().Grab(); }
	void WaitForTap(entt::entity creature, uint32_t nowMs, uint32_t turn) final
	{
		Locator::creatureHandSystem::value().WaitForTap(creature, nowMs, turn);
	}

	[[nodiscard]] bool FightDirects() const final { return Locator::creatureFightSystem::value().DirectsFight(); }
	bool FightPress(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, creature_fight::Button button, uint32_t nowMs,
	                uint32_t turn) final
	{
		return Locator::creatureFightSystem::value().Press(rayOrigin, rayDirection, button, nowMs, turn);
	}
	[[nodiscard]] bool FightPressed() const final { return Locator::creatureFightSystem::value().IsPressed(); }
	void FightRelease(uint32_t nowMs, uint32_t turn) final { Locator::creatureFightSystem::value().Release(nowMs, turn); }

	bool CreatureModePress(const creature_mode::Press& press) final
	{
		return Locator::creatureModeSystem::has_value() && Locator::creatureModeSystem::value().Press(press);
	}
};

} // namespace

HandPressSystem::HandPressSystem()
    : HandPressSystem(std::make_unique<LocatorHandPressWorld>())
{
}

HandPressSystem::HandPressSystem(std::unique_ptr<HandPressWorld> world)
    : _world(std::move(world))
{
}

HandPressSystem::~HandPressSystem() = default;

void HandPressSystem::Press(const SDL_Event& event, const PressContext& context)
{
	const bool rightLetGo = _buttons.Apply(event);
	// The let go of the left button (the select button) may click the dialogue on, whatever else it does
	if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT && !_world->MouseOverDebugWindow())
	{
		_dialogueClick = true;
	}

	const bool inTemple = context.inTemple;
	// While a script's cinema bars have taken the interface, the buttons' presses don't reach the hand
	const bool handTakesPresses = _world->HandTakesPresses();
	// Holding the right button on a creature holds the hand to it to stroke or slap it; clicking it puts the leash on.
	// While the player's creature fights, clicks on the creatures and the arena's ground direct the fight instead.
	// The hand holds one thing at a time: a thing it picked up, or a miracle
	const bool hasGrab = _world->HasHandGrab();
	const bool handHoldsThing = hasGrab && _world->HandHoldsThing();
	// The interface level of a lesson can stop the Action button from taking hold of creatures and totems. It can also
	// stop an empty hand from acting on anything but a highlight that a script made.
	const auto interfaceLimits = _world->Limits();
	const bool mayHold = script::interface_levels::MayHold(interfaceLimits);
	ActionLetGo(event, rightLetGo, inTemple, handTakesPresses, hasGrab, handHoldsThing);
	ActionPress(event, inTemple, handTakesPresses, hasGrab, handHoldsThing, interfaceLimits, mayHold);
	MovePress(event, inTemple, handTakesPresses, context);
	// When the player releases the button of the press, the blow that the press queued gets its charge. The fight
	// system also gets the release when the fight ended before it, because the hold of a press on a body continues.
	if (_fightButton.has_value() && ((*_fightButton == creature_fight::Button::Move && !_buttons.left) ||
	                                 (*_fightButton == creature_fight::Button::Action && !_buttons.right)))
	{
		_world->FightRelease(_world->Ticks(), _world->Turn());
		_fightButton.reset();
	}
	// Letting go of the right button lets go of the creature, also of a hold that a command started. Let go quickly,
	// having neither stroked nor slapped it, the press was a click, which puts the leash on the player's creature.
	if (creature_hand::LetsGoOfCreature(rightLetGo,
	                                    event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT) &&
	    _world->HeldCreature().has_value())
	{
		// The click puts the leash on, and then the creature feels how the hand treated it. Both are commands of the
		// player, which the world does at the start of a later turn. The release sends them in a network game, and the
		// draw of the hand sends them in a game alone.
		_world->ReleaseCreature();
	}
	const bool onCreature = _world->HeldCreature().has_value();
	// After the end of the interface lock stopped the grip of the land, the select button grips nothing until the player
	// lets go of it
	if (_landGripStopped && !_buttons.left)
	{
		_landGripStopped = false;
	}
	auto gripButtons = _buttons;
	gripButtons.left = gripButtons.left && !_landGripStopped;
	_handButtons = input::HandButtonsOf(gripButtons, inTemple, onCreature, _world->FightPressed());
	_selectPress = input::select_press::Next(_selectPress, {
	                                                           .event = SelectPressEvent(event),
	                                                           .interfaceOn = handTakesPresses,
	                                                           .gripsLandAlone = _handButtons.selectGrip && !_buttons.right,
	                                                           .selectDown = _buttons.left,
	                                                       });
}

void HandPressSystem::ActionLetGo(const SDL_Event& event, bool rightLetGo, bool inTemple, bool handTakesPresses, bool hasGrab,
                                  bool handHoldsThing)
{
	const bool actionLetGo = rightLetGo || (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT);
	// Letting go of the Action button lets go of the tip bubble
	if (_world->TipBubbleGripped() && actionLetGo)
	{
		_world->LetGoOfTipBubble();
	}
	// Letting go of the Action button lets go of a town's totem, leaving it where it was slid
	if (_world->TownTotemGripped() && actionLetGo)
	{
		_world->LetGoOfTownTotem();
		_totemPointer.reset();
	}
	// It also lets go of a puzzle totem, at the height it was slid to
	if (_world->PuzzleTotemGripped() && actionLetGo)
	{
		_world->LetGoOfPuzzleTotem();
		_totemPointer.reset();
	}
	// Letting go of the Action button lets go of what the hand was taking, or puts down or throws what it holds
	if (hasGrab && actionLetGo)
	{
		_world->GrabRelease(_world->Ticks(), _world->Turn());
		// Let go with an empty hand that did nothing with the press, it clicks the thing or the place under it, for the
		// scripts; not while the game is paused
		if (handTakesPresses && !_actionPressTaken && !handHoldsThing && !_world->MagicHandBusy() && !inTemple &&
		    !_world->Paused())
		{
			_world->GrabClickReleased();
		}
	}
	// The action button (the right) casts the miracle in the hand, which comes before the creatures: pressed, it arms,
	// locks on or casts it, and let go it throws an armed one or lets a locked one go. Without a miracle it takes hold
	// of the creature under the hand; with the leash on, pressed on another creature it ties the leash to that one
	// instead (see the leash's input).
	if (actionLetGo)
	{
		// Letting go of the rope of the tied leash: the leash's length becomes how far the hand is from what it is tied to,
		// and the creature goes to the place under the hand
		if (_world->RopeHand(_world->Player()) == leash_snap::Hand::Holding && _world->HasPicking())
		{
			_world->LetGoOfRope(_world->Player(), _world->PickedPoint());
		}
		_world->MagicReleaseAction();
		// Let go quickly on a fireball, it is tapped
		_world->MagicReleaseFireBall(_world->Ticks(), _world->Turn());
		// Let go while a press on a creature waits, the press is a tap. The held leash takes it as an order to act on the
		// creature.
		if (const auto tapped = _world->ReleaseWait(); tapped.has_value())
		{
			_world->LeashTapThing(_world->Player(), *tapped);
		}
	}
}

void HandPressSystem::ActionPress(const SDL_Event& event, bool inTemple, bool handTakesPresses, bool hasGrab,
                                  bool handHoldsThing, const components::InterfaceLimits& interfaceLimits, bool mayHold)
{
	if (!handTakesPresses || inTemple || event.type != SDL_MOUSEBUTTONDOWN || event.button.button != SDL_BUTTON_RIGHT ||
	    _world->MouseOverDebugWindow())
	{
		return;
	}
	const auto cursor = glm::vec2(event.button.x, event.button.y);
	// After the button let go of a creature, the Action button does nothing until the world ends the lock on it
	_actionPressTaken = _world->WaitsForLockEnd() || (_world->MagicHandBusy() && _world->MagicPressAction());
	// While the camera shows a duel of the player's creature and the hand is empty, every Action press goes to the
	// fight and to nothing else
	const bool fightTakesPress = _world->FightDirects();
	// Holding a thing, the hand makes ready to throw it
	if (!_actionPressTaken && handHoldsThing)
	{
		_actionPressTaken = _world->GrabPress(_world->Ticks(), _world->Turn());
	}
	// With an empty hand and the leash held, a quick second press near the first is a double tap, which ties the leash
	// to the thing under the cursor or unties it, before anything else takes it
	if (!_actionPressTaken && !handHoldsThing && !_world->MagicHandBusy())
	{
		glm::vec3 rayOrigin;
		glm::vec3 rayDirection;
		_world->Ray(cursor, rayOrigin, rayDirection);
		_actionPressTaken = _world->LeashPressAction(_world->Player(), rayOrigin, rayDirection, cursor, _world->Ticks());
	}
	// With an empty hand on the tip bubble, the hand grips it and the words follow the hand until the button is let go
	if (!_actionPressTaken && !fightTakesPress && !handHoldsThing && !_world->MagicHandBusy() && _world->TipBubbleUnderHand())
	{
		_world->GripTipBubble();
		_actionPressTaken = true;
	}
	// With an empty hand over the rope of the tied leash, the hand takes hold of the rope
	if (!_actionPressTaken && !fightTakesPress && !handHoldsThing && !_world->MagicHandBusy() &&
	    _world->PressRope(_world->Player()))
	{
		_actionPressTaken = true;
	}
	// A lesson can let an empty hand act only on a highlight that a script made. This limit also stops a press on a
	// fireball, which is not a highlight.
	const std::optional<entt::entity> picked = _world->PickedObject();
	const bool highlightUnderHand = picked.has_value() && _world->HasScriptHighlight(*picked);
	const bool mayPress = handHoldsThing || script::interface_levels::MayPress(interfaceLimits, highlightUnderHand);
	// With an empty hand, a fireball under it takes the press. A one-shot globe is taken hold of as any thing is.
	if (!_actionPressTaken && !fightTakesPress && !handHoldsThing && !_world->MagicHandBusy() && mayPress)
	{
		_actionPressTaken = _world->MagicPressFireBall(_world->Ticks(), _world->Turn());
	}
	if (!_actionPressTaken)
	{
		glm::vec3 rayOrigin;
		glm::vec3 rayDirection;
		_world->Ray(cursor, rayOrigin, rayDirection);
		// While the player's creature duels, the Action button adds a move to its queue
		if (_world->FightPress(rayOrigin, rayDirection, creature_fight::Button::Action, _world->Ticks(), _world->Turn()))
		{
			_actionPressTaken = true;
			_fightButton = creature_fight::Button::Action;
		}
		// A press that the fight refuses does nothing: the hand does not take hold of a creature or a thing then
		else if (fightTakesPress)
		{
			_actionPressTaken = true;
		}
		const auto own = _world->PlayersCreature(_world->Player());
		const auto under = _world->CreatureUnderCursor();
		const bool tying =
		    under.has_value() && own.has_value() &&
		    creature_hand::PressOn(*under == *own, _world->IsLeashed(*own)) == creature_hand::CreaturePress::Leash;
		if (!_actionPressTaken && under.has_value() && !tying && mayHold)
		{
			_actionPressTaken = true;
			if (hasGrab)
			{
				_world->GrabClickThing(*under);
			}
			// With the leash of the player's creature held loose (not tied), a press on a creature that the hand may
			// hold first waits for a tap. A press on a creature that the hand may not hold sends no leash click.
			const bool leashHeldLoose = own.has_value() && _world->HolderPoint(*own).has_value();
			switch (creature_hand::PressStartOn(_world->MayHoldCreature(*under), leashHeldLoose))
			{
			case creature_hand::PressStart::Hold:
				_world->GrabCreature();
				break;
			case creature_hand::PressStart::WaitForTap:
				_world->WaitForTap(*under, _world->Ticks(), _world->Turn());
				break;
			case creature_hand::PressStart::TapAtOnce:
				// The leash takes the tap as an order to act on the creature
				_world->LeashTapThing(_world->Player(), *under);
				break;
			case creature_hand::PressStart::Nothing:
				break;
			}
		}
	}
	// A town's totem under the hand is taken hold of, to slide it up and down
	if (!_actionPressTaken && !_world->MagicHandBusy() && !handHoldsThing && mayHold && _world->HasTownTotems() &&
	    _world->HasPicking())
	{
		if (picked.has_value())
		{
			if (const auto totem = _world->TownTotemOf(*picked);
			    totem.has_value() && _world->GripTownTotem(*totem, _world->Player()))
			{
				_actionPressTaken = true;
				_totemPointer = _mousePosition;
			}
		}
	}
	// A puzzle totem under the hand is taken hold of, to slide it up out of the land and down into it
	if (!_actionPressTaken && !_world->MagicHandBusy() && !handHoldsThing && mayHold && picked.has_value() &&
	    _world->HasPuzzleTotems() && _world->HasPicking())
	{
		const auto point = _world->PickedPoint();
		if (point.has_value() && _world->GripPuzzleTotem(*picked, _world->Player(), *point))
		{
			_actionPressTaken = true;
			_totemPointer = _mousePosition;
		}
	}
	// Otherwise the hand takes hold of a thing under it, if the lesson lets it
	if (!_actionPressTaken && !_world->MagicHandBusy() && hasGrab && mayPress)
	{
		_actionPressTaken = _world->GrabPress(_world->Ticks(), _world->Turn());
	}
	// A press that the lesson refuses does nothing more: the leash gives no order and taps no post
	if (!_actionPressTaken && !mayPress)
	{
		_actionPressTaken = true;
	}
}

void HandPressSystem::MovePress(const SDL_Event& event, bool inTemple, bool handTakesPresses, const PressContext& context)
{
	if (!handTakesPresses || _world->MagicHandBusy() || inTemple || event.type != SDL_MOUSEBUTTONDOWN ||
	    event.button.button != SDL_BUTTON_LEFT || _buttons.middle)
	{
		return;
	}
	const auto cursor = glm::vec2(event.button.x, event.button.y);
	glm::vec3 rayOrigin;
	glm::vec3 rayDirection;
	_world->Ray(cursor, rayOrigin, rayDirection);
	// The second press of a double click on a creature, anyone's, locks the camera onto it
	const bool doubleClicked = _world->CreatureModePress(
	    {.milliseconds = event.button.timestamp, .screen = cursor, .creature = _world->CreatureUnderCursor()});
	// While the player's creature duels, the Move button makes a move at once in place of those queued
	if (!doubleClicked &&
	    _world->FightPress(rayOrigin, rayDirection, creature_fight::Button::Move, _world->Ticks(), _world->Turn()))
	{
		_fightButton = creature_fight::Button::Move;
	}
	else if (!doubleClicked && context.playGrabSound)
	{
		context.playGrabSound();
	}
}

void HandPressSystem::StopLandGrip()
{
	if (!_handButtons.selectGrip)
	{
		return;
	}
	// The grip of the land by the select button stops. The middle button still grips and turns.
	_beforeGripStop = _handButtons;
	_landGripStopped = true;
	_handButtons.selectGrip = false;
	_handButtons.gripping = _buttons.middle;
	_handButtons.rotating = _buttons.middle;
}

void HandPressSystem::GripLandAgain()
{
	if (!_landGripStopped)
	{
		return;
	}
	_landGripStopped = false;
	_handButtons = _beforeGripStop;
}

bool HandPressSystem::TakeActionPress()
{
	return std::exchange(_actionPressTaken, false);
}

bool HandPressSystem::TakeDialogueClick()
{
	return std::exchange(_dialogueClick, false);
}

void HandPressSystem::MovePointer(glm::ivec2 position)
{
	_mousePosition = position;
	_handPointer.Moved();
}

void HandPressSystem::FramePointer(std::optional<glm::ivec2> gamePointer)
{
	_mousePosition = _handPointer.Frame(_mousePosition, gamePointer);
}
