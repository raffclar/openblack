/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "Creature/LeashKeys.h"
#include "Creature/LeashOwnership.h"
#include "Enums.h"

namespace openblack::ecs::systems
{

/// The leashes the player leads creatures with (see components::CreatureLeash). Once a frame each worn leash's rope
/// swings between the hand, or what the leash is tied to, and the creature's collar. Once a game turn a rope pulled taut
/// in the hand makes the creature stop and walk to the hand, a tied leash keeps the creature near what it is tied to,
/// and the leash's feelings and lessons are passed to the creature's mind. The player picks a leash with the hotkeys,
/// puts it on and takes it off with the leash key, and ties the leash to things and unties it again.
///
/// Who may lead which creature is decided here and nowhere else (see creature_leash::WhyNot): each player leads only
/// their one leashable creature, so the hand, the shortcuts, the scripts, the debug windows and the scenarios all go
/// through these calls. A refusal is logged and kept on the player's entity as their last refusal, for the debug windows
/// to show.
///
/// The game reads the leash keys once a frame and passes them to PressKey (ECS/CreatureLoop.h). The citadel's leash
/// posts and the shake gesture are not here yet: they come with the citadel's leash object and the game's gestures.
class LeashSystemInterface
{
public:
	/// A leash that was refused: who wanted it on which creature, and why not
	struct Refused
	{
		PlayerNames player;
		entt::entity creature;
		creature_leash::Refusal why;
	};

	virtual ~LeashSystemInterface() = default;

	/// Once a game turn
	virtual void ProcessTurn() = 0;
	/// Once a frame, some seconds of game time on: the ropes swing
	virtual void Update(float seconds) = 0;

	/// Whether the creature knows a leash, which it must before it can wear it; the learning leash before any
	[[nodiscard]] virtual bool Knows(entt::entity creature, LeashType type) const = 0;
	virtual void SetKnown(entt::entity creature, LeashType type, bool known) = 0;

	/// Whether the creature is the one its owner can lead
	[[nodiscard]] virtual bool IsLeashable(entt::entity creature) const = 0;
	/// Makes the creature the one its owner can lead, which stops their other creature being it and takes that one's
	/// leash off; or stops it being it, taking its leash off. A creature that belongs to nobody can't be made leashable.
	virtual bool SetLeashable(entt::entity creature, bool leashable) = 0;
	/// Gives the creature to another player, taking its leash off. It stays leashable only if the new owner has no
	/// leashable creature of their own.
	virtual void SetOwner(entt::entity creature, PlayerNames owner) = 0;
	/// A new creature becomes its owner's leashable one when they have none yet
	virtual void ClaimOnArrival(entt::entity creature) = 0;
	/// Why the player may not put the leash on the creature, or creature_leash::Refusal::None when they may
	[[nodiscard]] virtual creature_leash::Refusal WhyNot(PlayerNames player, entt::entity creature, LeashType type) const = 0;
	/// The last leash the player was refused, for the debug windows. None for a player with no entity on this land
	[[nodiscard]] virtual std::optional<Refused> LastRefusal(PlayerNames player) const = 0;

	/// Puts a leash on the creature, held in its owner's hand. Returns whether it could: it must be its owner's leashable
	/// creature and know the leash.
	virtual bool PutOn(entt::entity creature, LeashType type) = 0;
	virtual void TakeOff(entt::entity creature) = 0;
	/// Unties a tied leash back to the hand, or else puts the picked leash on or takes it off. This is all the game's leash
	/// key and its toggle-leash script command do, however many leashes the creature knows.
	virtual bool Toggle(entt::entity creature) = 0;
	/// Swaps the leash worn for another the creature knows, or picks the one to put on next
	virtual bool ChangeType(entt::entity creature, LeashType type) = 0;
	/// Ties the worn leash to something, or puts the picked leash on tied to it
	virtual bool TieTo(entt::entity creature, entt::entity object) = 0;
	virtual void UntieToHand(entt::entity creature) = 0;
	/// Whether the leash makes the creature do anything, as scripts set
	virtual void SetWorks(entt::entity creature, bool works) = 0;

	/// Keeps the creature within a radius of its home, as it is while it starts to grow up
	virtual void ConfineToHome(entt::entity creature, float radius) = 0;
	virtual void ClearConfinement(entt::entity creature) = 0;
	/// Sets where the creature's home is, as a script does; anything but a creature is left alone
	virtual void SetHome(entt::entity creature, const glm::vec3& home) = 0;
	/// Whether the creature is near enough its home, its player having a temple, to roam
	[[nodiscard]] virtual bool FreeOfHome(entt::entity creature) const = 0;

	[[nodiscard]] virtual bool IsLeashed(entt::entity creature) const = 0;
	[[nodiscard]] virtual std::optional<entt::entity> TiedTo(entt::entity creature) const = 0;
	[[nodiscard]] virtual LeashType TypeOf(entt::entity creature) const = 0;
	/// The leash picked for the creature: the one it wears, or else the one to put on next, whether or not it wears one;
	/// none for a thing that has no leashes
	[[nodiscard]] virtual LeashType Picked(entt::entity creature) const = 0;
	/// The player's creature: the one creature they can lead, if they have one
	[[nodiscard]] virtual std::optional<entt::entity> PlayersCreature(PlayerNames player) const = 0;

	/// A player presses a leash shortcut, which acts on their creature. Returns whether it did anything.
	virtual bool PressKey(PlayerNames player, creature_leash::LeashKey key) = 0;
	/// Puts the picked leash on the player's own creature; any other is refused. Returns whether the leash went on. For
	/// the debug windows: the hand's click on the creature works the leash key instead (PressKey)
	virtual bool TapCreature(PlayerNames player, entt::entity creature) = 0;
	/// A leash held in the player's hand comes off, as the hand's scribble gesture takes it off; one tied to something
	/// stays. Returns whether it came off.
	virtual bool TakeOffHeldLeash(PlayerNames player) = 0;
};

} // namespace openblack::ecs::systems
