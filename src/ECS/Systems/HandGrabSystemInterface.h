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
#include <string>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Commands/PlayerCommands.h"
#include "Enums.h"

namespace openblack::ecs::systems
{

/// The player's hand picking things up, carrying them and letting them go: putting them down where it is, or throwing
/// them with the speed its spring drags it at. Miracles in the hand and creatures held to it are the magic's and the
/// creature hand's.
class HandGrabSystemInterface
{
public:
	virtual ~HandGrabSystemInterface() = default;

	/// What the frame tells the hand
	struct Frame
	{
		/// Where the hand should be, before it rises for what it holds
		glm::vec3 target {0.0f};
		/// The line through the cursor
		glm::vec3 rayOrigin {0.0f};
		glm::vec3 rayDirection {0.0f};
		/// Where the camera is: the pull measures its plane from it
		glm::vec3 camera {0.0f};
		/// The point picked on the land under the cursor, none over nothing
		std::optional<glm::vec3> cursorGround;
		/// The hand's size against its standard
		float handSize {1.0f};
		/// The frame's time: the clock's for the hand's fades, the game's for the spring
		float seconds {0.0f};
		uint32_t gameMs {0};
		/// The clock now, and the game's turn
		uint32_t nowMs {0};
		uint32_t turn {0};
	};

	/// The Action button is pressed: whether the hand took the press, to take hold of the thing the interface picked
	/// under the cursor or to make ready to throw what it holds
	virtual bool Press(uint32_t nowMs, uint32_t turn) = 0;
	/// The Action button is let go: what the hand was taking is let be, or what it holds is put down or thrown. The thing a
	/// press tapped, a press too short to take it.
	virtual std::optional<entt::entity> Release(uint32_t nowMs, uint32_t turn) = 0;
	/// The Action button is let go while the hand did nothing with the press: the thing under the hand is clicked, or
	/// else the place on the land under it, for the scripts to ask about (see hand_click)
	virtual void ClickReleased(uint32_t turn) = 0;
	/// A press of the Action button on a thing the hand won't pick up, which something else takes (a creature the hand
	/// holds to): the thing is clicked
	virtual void ClickThing(entt::entity thing, uint32_t turn) = 0;
	/// Each frame: the hand takes what it waited for, its spring drags it, and the twist of a throw is given. The hand's
	/// position, risen for what it holds, or dragged by its spring.
	virtual glm::vec3 UpdateFrame(const Frame& frame) = 0;
	/// Each game turn: what it holds stays where the hand is for the game, and is dropped once it is gone
	virtual void ProcessTurn() = 0;
	/// At the start of each turn: what the hand holds is where the last report put the hand, at its own turn and size
	virtual void PlaceHeldForTurn() {}
	/// The hand is made to let go of what it holds, which is put down where it is and never planted again
	virtual void ForceDrop() = 0;
	/// A hand demonstration's recorded throw: if the hand throws before its spring next moves it, what it holds leaves
	/// at this velocity rather than the spring's
	virtual void SetRecordedThrow(glm::vec3 velocity) = 0;
	/// The world is cleared: the hand holds nothing
	virtual void Reset() = 0;

	/// The hand's own work of the turn, after the commands of the turn: the hand takes a thing that it waited for long
	/// enough, and gives what it holds to a creature after a long enough press. The hand does the same work each frame.
	virtual void InterfaceTurn(uint32_t /*nowMs*/, uint32_t /*turn*/) {}
	/// The commands of the hand, which the world does at the start of a turn, for the player that sent them. The hand
	/// takes the thing that it waited for.
	virtual void DoTake(PlayerNames /*player*/, entt::entity /*object*/) {}
	/// The hand lets go of the thing that it holds, at the velocity of the throw, from the start in the command
	virtual void DoLetGo(PlayerNames /*player*/, const player_commands::HandLetGo& /*letGo*/) {}
	/// The hand taps a thing
	virtual void DoTap(PlayerNames /*player*/, entt::entity /*object*/) {}
	/// At the turn: the twist of a thing that the hand let go (player_commands::HandTwist)
	virtual void DoTwist(PlayerNames /*player*/, entt::entity /*object*/, glm::vec3 /*moved*/) {}
	/// The hand gives the thing that it holds to a creature
	virtual void DoGive(PlayerNames /*player*/, entt::entity /*creature*/, entt::entity /*object*/) {}
	/// A creature takes a thing the hand gave it: the hand lets go of it into the creature's hand, without dropping it.
	/// Whether the hand still held it to give.
	virtual bool HandOver(entt::entity /*object*/) { return false; }
	/// For the debug tools: the empty hand takes a thing at once, as if picked up from where it stands; and gives what it
	/// holds to a creature, as the Action button pressed over the creature does. Whether it could.
	virtual bool TakeIntoHand(entt::entity /*object*/) { return false; }
	virtual bool GiveHeldTo(entt::entity /*creature*/) { return false; }
	/// For the debug tools: the hand releases the thing that it holds, as a release of the Action button does. With no
	/// velocity, the hand puts the thing on the land below it. Else, the hand throws the thing at the velocity. This
	/// function returns true if the hand held a thing.
	virtual bool LetGoAt(glm::vec3 /*velocity*/) { return false; }
	/// The creature the hand offers what it holds to, while it does; the creature reaching for it once given
	[[nodiscard]] virtual std::optional<entt::entity> GetOfferedCreature() const { return std::nullopt; }
	/// Whether the hand holds a thing and offers it to a creature while the offer lasts
	[[nodiscard]] virtual bool OffersToCreature() const { return false; }
	/// How far round what the hand holds is turned as it is offered, rolled with the hand about the line to the camera,
	/// in radians
	[[nodiscard]] virtual float GetOfferTurn() const { return 0.0f; }

	/// What the hand holds, none when it holds nothing of its own
	[[nodiscard]] virtual std::optional<entt::entity> GetHeld() const = 0;
	/// The field, fish farm or pile the hand is locked onto while it scoops from it, none when it scoops from nothing
	[[nodiscard]] virtual std::optional<entt::entity> GetLockedOn() const { return std::nullopt; }
	/// Whether the Action button is down to throw what the hand holds
	[[nodiscard]] virtual bool IsAboutToThrow() const { return false; }
	/// Whether the creature under the pointer can take what the hand holds as a gift
	[[nodiscard]] virtual bool CreatureTakesHeld(entt::entity /*creature*/) const { return false; }
	/// Whether the hand is taking, holding or about to throw a thing, so that other uses of the button leave it be
	[[nodiscard]] virtual bool IsBusy() const = 0;
	/// Whether the hand is pulling a thing out of the ground, from the press until it holds it
	[[nodiscard]] virtual bool IsPulling() const = 0;
	/// Whether the hand's point on the land is in its player's influence
	[[nodiscard]] virtual bool IsInInfluence() const = 0;
	/// Why a press of the Action button on a thing wouldn't take it (out of the influence, a thing the hand can't hold,
	/// a script holding it out of reach...); empty when it would
	[[nodiscard]] virtual std::string WhyNotTake(entt::entity object) const = 0;
	/// How far a thing hangs below a hand that holds it: the lowering of its kind times its height. A creature's hand
	/// uses the same distance.
	[[nodiscard]] virtual float HangOf(entt::entity object) const = 0;
	/// The amount of food or wood the hand holds, which the hand shows for the player: after each turn that a scoop from
	/// a pile, a field or a fish farm takes some, and when the player lets go of a scoop from a pile or a fish farm. It is
	/// given once, and none until the next time
	[[nodiscard]] virtual std::optional<uint32_t> TakeAmountToolTip() { return std::nullopt; }
	/// How what it holds hangs: its hold, how far below the hand its model hangs, and how far it spreads out of the hand
	struct HeldPose
	{
		entt::entity object {entt::null};
		HoldType hold {HoldType::Above};
		float hang {0.0f};
		float reach {0.0f};
	};
	[[nodiscard]] virtual std::optional<HeldPose> GetHeldPose() const = 0;
	/// While the hand pulls a thing free it takes the pose the thing is held with: its hold and how far it spreads
	struct PullPose
	{
		HoldType hold {HoldType::Above};
		float reach {0.0f};
	};
	[[nodiscard]] virtual std::optional<PullPose> GetPullPose() const = 0;
	/// How far the point the hand picks on the land is raised for what it holds
	[[nodiscard]] virtual float GetCursorRaise() const = 0;
};

} // namespace openblack::ecs::systems
