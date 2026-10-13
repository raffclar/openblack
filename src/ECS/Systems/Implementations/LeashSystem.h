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
#include <vector>

#include "Creature/LeashOrders.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Systems/LeashSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class LeashSystem final: public LeashSystemInterface
{
public:
	void ProcessTurn() override;
	void Update(float seconds) override;
	void ShowPosts(bool handShown) override;
	void HandleInput(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, bool actionTaken) override;

	[[nodiscard]] bool Knows(entt::entity creature, LeashType type) const override;
	void SetKnown(entt::entity creature, LeashType type, bool known) override;
	[[nodiscard]] bool IsLeashable(entt::entity creature) const override;
	bool SetLeashable(entt::entity creature, bool leashable) override;
	void SetOwner(entt::entity creature, PlayerNames owner) override;
	void ClaimOnArrival(entt::entity creature) override;
	[[nodiscard]] creature_leash::Refusal WhyNot(PlayerNames player, entt::entity creature, LeashType type) const override;
	[[nodiscard]] std::optional<Refused> LastRefusal() const override;
	bool PutOn(entt::entity creature, LeashType type) override;
	void TakeOff(entt::entity creature) override;
	bool Toggle(entt::entity creature) override;
	bool ChangeType(entt::entity creature, LeashType type) override;
	bool TieTo(entt::entity creature, entt::entity object) override;
	void UntieToHand(entt::entity creature) override;
	void SetWorks(entt::entity creature, bool works) override;
	void SetDrawn(bool drawn) override;
	void ConfineToHome(entt::entity creature, float radius) override;
	void ClearConfinement(entt::entity creature) override;
	[[nodiscard]] bool FreeOfHome(entt::entity creature) const override;
	[[nodiscard]] bool IsLeashed(entt::entity creature) const override;
	[[nodiscard]] std::optional<entt::entity> TiedTo(entt::entity creature) const override;
	[[nodiscard]] std::optional<glm::vec3> HolderPoint(entt::entity creature) const override;
	[[nodiscard]] LeashType TypeOf(entt::entity creature) const override;
	[[nodiscard]] std::optional<entt::entity> PlayersCreature(PlayerNames player) const override;
	bool OrderAt(PlayerNames player, const glm::vec3& place) override;
	bool OrderOn(PlayerNames player, entt::entity object) override;
	[[nodiscard]] std::optional<glm::vec3> OrderTarget(entt::entity creature) const override;
	bool PressAction(PlayerNames player, const glm::vec3& rayOrigin, const glm::vec3& rayDirection, glm::vec2 cursor,
	                 uint32_t milliseconds) override;
	bool TapThing(PlayerNames player, entt::entity object) override;
	[[nodiscard]] std::optional<float> RopeUnderCursor(PlayerNames player, const RopeFrame& frame) override;
	void SetRopeHovered(PlayerNames player, bool hovered) override;
	[[nodiscard]] leash_snap::Hand RopeHand(PlayerNames player) const override;
	bool PressRope(PlayerNames player) override;
	void LetGoOfRope(PlayerNames player, std::optional<glm::vec3> place) override;
	[[nodiscard]] std::optional<uint32_t> ToolTip(PlayerNames player, std::optional<entt::entity> hovered) const override;
	void PlacePosts(PlayerNames owner, const std::array<glm::vec3, 3>& points) override;
	bool TapPost(entt::entity post) override;
	bool PressKey(PlayerNames player, creature_leash::LeashKey key) override;
	bool TapCreature(PlayerNames player, entt::entity creature) override;
	bool Shake(PlayerNames player) override;

private:
	/// Puts the leash on for the player, unless the rules refuse it, which is logged and remembered
	bool PutOnFor(PlayerNames player, entt::entity creature, LeashType type);
	/// Carries out what a shortcut does to the player's creature
	bool Carry(PlayerNames player, entt::entity creature, const creature_leash::KeyCommand& command);
	void Refuse(PlayerNames player, entt::entity creature, creature_leash::Refusal why);
	/// The creatures as the one-each assignment sees them
	[[nodiscard]] std::vector<creature_leash::Claim> Claims() const;
	/// The posts of each temple whose heart mesh has the three points to hang them at, once the temples are there
	void PlacePostsAtTemples();
	/// A taut rope in the hand pulls the creature to the hand
	/// Whether the creature must walk back into the area that holds it in this turn
	[[nodiscard]] bool ShouldWalkBack(entt::entity creature) const;
	void WalkBack(entt::entity creature);
	/// What the worn leash does to the creature's desires this turn, and to those of a creature it is tied to
	void UpdateMoods(entt::entity creature, const components::CreatureLeash::Worn& worn);
	/// The creature takes an order on the land or on a thing, unless it refuses
	/// Before an order: a creature on its way to a fight takes none (false), and a duelling one's fight is called off
	bool TakeOutOfFight(entt::entity creature);
	bool TakeGroundOrder(entt::entity creature, const glm::vec3& place);
	bool TakeThingOrder(entt::entity creature, entt::entity object);
	/// The creature answers an order: its player hears it acknowledged, and it nods and makes its acknowledging sound
	void Acknowledge(entt::entity creature);
	/// The order taken is remembered and marked: the sparkles over its target, and the ring its player sees
	void MarkOrder(entt::entity creature, std::optional<entt::entity> object, const glm::vec3& point, bool fight);
	/// The order is over: its marker goes
	void EndOrder(entt::entity creature);
	/// Whether the order the creature took is still being carried out
	[[nodiscard]] bool OrderInForce(entt::entity creature) const;
	/// The ring of the player's own creature's order follows its target and throbs
	void UpdateMarkers(float seconds);
	/// The player's worn leash while it is tied to something, or none (which drops any rope the hand held)
	components::CreatureLeash::Worn* TiedLeash(PlayerNames player);
	/// The player's tied leash while the hand holds its rope
	components::CreatureLeash::Worn* HeldLeash(PlayerNames player);
	/// The temple's leashes hang if the creature knows them, tumble and glow, the picked one carried in the hand
	void UpdatePosts(float seconds);
	/// Whether the posts have been looked for at the temples yet
	bool _postsPlaced {false};
	/// Whether the hand was out when the posts were last shown
	bool _handShown {true};
	/// The two leash-tying sounds play in turn
	bool _secondAttachSound {false};
	std::optional<Refused> _lastRefusal;
	/// Tells the Action button's double taps from single ones
	creature_leash_orders::DoubleTaps _doubleTaps;
	/// The last press of the Action button was the second of a double tap
	bool _lastPressDouble {false};
	/// What the local player's hand does about the rope of their tied leash
	leash_snap::Hand _ropeHand {leash_snap::Hand::Away};
	/// What the thing under the cursor is to an Action tap: the object the interface picked, a creature along the ray when
	/// it picked none, and the land
	struct Under
	{
		std::optional<entt::entity> object;
		std::optional<glm::vec3> land;
	};
	[[nodiscard]] Under TappedUnder(const glm::vec3& rayOrigin, const glm::vec3& rayDirection) const;
	/// A single or double tap of the Action button with the leash, acting on what is under the cursor
	void ActOnTap(PlayerNames player, entt::entity creature, const Under& under, bool doubleTap);
	/// The clock every order's ring throbs by, in milliseconds of game time
	float _markerClockMs {0.0f};
};

} // namespace openblack::ecs::systems
