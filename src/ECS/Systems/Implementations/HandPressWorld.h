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

#include <optional>

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureFight.h"
#include "Creature/CreatureMode.h"
#include "Creature/LeashSnap.h"
#include "ECS/Components/InterfaceLimits.h"
#include "Enums.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

/// The services that the routing of the presses reads and calls. The game gives the services of the locator. The tests
/// give fakes. A service that is not there answers as if it had nothing to do.
class HandPressWorld
{
public:
	virtual ~HandPressWorld() = default;

	// The facts of the game
	/// The cinema bars of a script do not hold the interface
	[[nodiscard]] virtual bool HandTakesPresses() const = 0;
	[[nodiscard]] virtual bool MouseOverDebugWindow() const = 0;
	/// The limits of the interface for the player of this computer. All presses are free before the player is there.
	[[nodiscard]] virtual components::InterfaceLimits Limits() const = 0;
	[[nodiscard]] virtual bool Paused() const = 0;
	[[nodiscard]] virtual uint32_t Ticks() const = 0;
	[[nodiscard]] virtual uint32_t Turn() const = 0;
	[[nodiscard]] virtual PlayerNames Player() const = 0;
	/// The line of sight from the camera through a point of the window
	virtual void Ray(glm::vec2 cursor, glm::vec3& origin, glm::vec3& direction) const = 0;
	[[nodiscard]] virtual bool HasPicking() const = 0;
	[[nodiscard]] virtual std::optional<entt::entity> PickedObject() const = 0;
	[[nodiscard]] virtual std::optional<glm::vec3> PickedPoint() const = 0;
	/// A script put a highlight on the thing, and the thing is still there
	[[nodiscard]] virtual bool HasScriptHighlight(entt::entity thing) const = 0;

	// The tip bubble
	[[nodiscard]] virtual bool TipBubbleGripped() const = 0;
	virtual void LetGoOfTipBubble() = 0;
	[[nodiscard]] virtual bool TipBubbleUnderHand() const = 0;
	virtual void GripTipBubble() = 0;

	// The totems of the towns and of the puzzles
	[[nodiscard]] virtual bool HasTownTotems() const = 0;
	[[nodiscard]] virtual bool TownTotemGripped() const = 0;
	virtual void LetGoOfTownTotem() = 0;
	[[nodiscard]] virtual std::optional<entt::entity> TownTotemOf(entt::entity picked) const = 0;
	virtual bool GripTownTotem(entt::entity totem, PlayerNames player) = 0;
	[[nodiscard]] virtual bool HasPuzzleTotems() const = 0;
	[[nodiscard]] virtual bool PuzzleTotemGripped() const = 0;
	virtual void LetGoOfPuzzleTotem() = 0;
	virtual bool GripPuzzleTotem(entt::entity totem, PlayerNames player, glm::vec3 hand) = 0;

	// The hand that takes hold of things
	[[nodiscard]] virtual bool HasHandGrab() const = 0;
	[[nodiscard]] virtual bool HandHoldsThing() const = 0;
	virtual bool GrabPress(uint32_t nowMs, uint32_t turn) = 0;
	virtual void GrabRelease(uint32_t nowMs, uint32_t turn) = 0;
	virtual void GrabClickReleased() = 0;
	virtual void GrabClickThing(entt::entity thing) = 0;
	/// The interface waits after a gift of the held thing, or after a use of the held thing or the miracle on a thing,
	/// until the button is let go. In these waits the presses of the buttons do nothing.
	[[nodiscard]] virtual bool WaitIgnoresPresses() const = 0;
	/// The hand does an action other than the normal one, for example a take, a lock, a wait, a throw, a grip of a totem
	/// or of the tip bubble, or a hold of the rope. A press of the select button then grips no land.
	[[nodiscard]] virtual bool DoesAction() const = 0;

	// The miracles in the hand
	[[nodiscard]] virtual bool MagicHandBusy() const = 0;
	virtual bool MagicPressAction() = 0;
	virtual void MagicReleaseAction() = 0;
	virtual bool MagicPressFireBall(uint32_t nowMs, uint32_t turn) = 0;
	virtual void MagicReleaseFireBall(uint32_t nowMs, uint32_t turn) = 0;

	// The leash
	[[nodiscard]] virtual leash_snap::Hand RopeHand(PlayerNames player) const = 0;
	virtual void LetGoOfRope(PlayerNames player, std::optional<glm::vec3> place) = 0;
	virtual void LeashTapThing(PlayerNames player, entt::entity thing) = 0;
	virtual bool LeashPressAction(PlayerNames player, const glm::vec3& rayOrigin, const glm::vec3& rayDirection,
	                              glm::vec2 cursor, uint32_t nowMs) = 0;
	virtual bool PressRope(PlayerNames player) = 0;
	[[nodiscard]] virtual std::optional<entt::entity> PlayersCreature(PlayerNames player) const = 0;
	[[nodiscard]] virtual bool IsLeashed(entt::entity creature) const = 0;
	[[nodiscard]] virtual std::optional<glm::vec3> HolderPoint(entt::entity creature) const = 0;

	// The hand on the creatures
	[[nodiscard]] virtual std::optional<entt::entity> HeldCreature() const = 0;
	virtual void ReleaseCreature() = 0;
	virtual std::optional<entt::entity> ReleaseWait() = 0;
	[[nodiscard]] virtual bool WaitsForLockEnd() const = 0;
	[[nodiscard]] virtual std::optional<entt::entity> CreatureUnderCursor() const = 0;
	[[nodiscard]] virtual bool MayHoldCreature(entt::entity creature) const = 0;
	virtual void GrabCreature() = 0;
	virtual void WaitForTap(entt::entity creature, uint32_t nowMs, uint32_t turn) = 0;

	// The fight of the player's creature
	[[nodiscard]] virtual bool FightDirects() const = 0;
	virtual bool FightPress(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, creature_fight::Button button,
	                        uint32_t nowMs, uint32_t turn) = 0;
	[[nodiscard]] virtual bool FightPressed() const = 0;
	virtual void FightRelease(uint32_t nowMs, uint32_t turn) = 0;

	/// A press of the left button for the creature mode. True when it is the second press of a double click on a
	/// creature.
	virtual bool CreatureModePress(const creature_mode::Press& press) = 0;
};

} // namespace openblack::ecs::systems
