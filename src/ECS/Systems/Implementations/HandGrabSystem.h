/******************************************************************************
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
#include <string>
#include <utility>

#include "ECS/Systems/HandGrabSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

#include "Hand/HandGrabRules.h"
#include "Hand/HandGrabWorld.h"

namespace openblack::ecs::components
{
struct HandClicked;
struct HandGrab;
} // namespace openblack::ecs::components

namespace openblack::ecs::systems
{

class HandGrabSystem final: public HandGrabSystemInterface
{
public:
	/// The hand in the game's world
	HandGrabSystem();
	/// The hand in a world of its own, as tests give it
	explicit HandGrabSystem(std::unique_ptr<hand_grab::HandGrabWorldInterface> world);
	~HandGrabSystem() override;

	bool Press(uint32_t nowMs, uint32_t turn) override;
	std::optional<entt::entity> Release(uint32_t nowMs, uint32_t turn) override;
	void ClickReleased(uint32_t turn) override;
	void ClickThing(entt::entity object, uint32_t turn) override;
	glm::vec3 UpdateFrame(const Frame& frame) override;
	void ProcessTurn() override;
	/// The turn of the hand that HandGrabWorldInterface::Hand gives now
	void ProcessHandTurn();
	void PlaceHeldForTurn() override;
	void PlaceOthersHeldForFrame(int32_t frameMs) override;
	void DoTwist(PlayerNames player, entt::entity object, glm::vec3 moved) override;
	void DoLock(PlayerNames player, entt::entity object) override;
	void DoApply(PlayerNames player, entt::entity target) override;
	void DoUnlock(PlayerNames player, entt::entity object) override;
	void ForceDrop() override;
	void SetRecordedThrow(glm::vec3 velocity) override;
	[[nodiscard]] std::optional<PullPose> GetPullPose() const override;
	[[nodiscard]] bool IsInInfluence() const override { return HandInInfluence(); }
	[[nodiscard]] std::string WhyNotTake(entt::entity object) const override;
	[[nodiscard]] float HangOf(entt::entity object) const override;
	[[nodiscard]] std::optional<uint32_t> TakeAmountToolTip() override { return std::exchange(_amountToolTip, std::nullopt); }
	void Reset() override;
	void InterfaceTurn(uint32_t nowMs, uint32_t turn) override;
	void DoTake(PlayerNames player, entt::entity object) override;
	void DoLetGo(PlayerNames player, const player_commands::HandLetGo& letGo) override;
	void DoTap(PlayerNames player, entt::entity object) override;
	void DoGive(PlayerNames player, entt::entity creature, entt::entity object) override;

	[[nodiscard]] std::optional<entt::entity> GetHeld() const override;
	[[nodiscard]] std::optional<entt::entity> GetLockedOn() const override;
	[[nodiscard]] bool IsAboutToThrow() const override;
	[[nodiscard]] bool CreatureTakesHeld(entt::entity creature) const override;
	[[nodiscard]] bool IsBusy() const override;
	[[nodiscard]] bool IsPulling() const override;
	[[nodiscard]] std::optional<HeldPose> GetHeldPose() const override;
	[[nodiscard]] float GetCursorRaise() const override;
	bool HandOver(entt::entity object) override;
	bool TakeIntoHand(entt::entity object) override;
	bool TakeIntoHandOf(PlayerNames player, entt::entity object) override;
	bool LetGoAt(glm::vec3 velocity) override;
	bool GiveHeldTo(entt::entity creature) override;
	[[nodiscard]] std::optional<entt::entity> GetOfferedCreature() const override;
	[[nodiscard]] bool OffersToCreature() const override;
	[[nodiscard]] float GetOfferTurn() const override;

private:
	/// What the hand last took from a pile, until the interface has shown it
	std::optional<uint32_t> _amountToolTip;
	/// The hand's grab, made on the hand when first needed; none without a hand
	[[nodiscard]] components::HandGrab* Grab();
	[[nodiscard]] const components::HandGrab* Grab() const;
	[[nodiscard]] bool Exists(entt::entity object) const;
	/// How the hand treats a thing
	[[nodiscard]] hand_grab::GrabKind KindOf(entt::entity object) const;
	/// What deciding whether the hand may hold a thing needs to know of it
	[[nodiscard]] hand_grab::Holdable HoldableOf(entt::entity object) const;
	/// Whether the hand may take a thing now: what it is, what a script says of it, and the hand's influence
	[[nodiscard]] hand_grab::Gate GateOf(entt::entity object) const;
	[[nodiscard]] hand_grab::Gate GateAt(entt::entity object, std::optional<glm::vec3> point) const;
	[[nodiscard]] bool MayTake(entt::entity object) const;
	/// A press that doesn't take the thing taps it, when the player may touch it
	void Tap(entt::entity object);
	/// What the player clicked, made on the hand when first needed; none without a hand
	[[nodiscard]] components::HandClicked* Clicked();
	/// Whether a click on a thing marks it as clicked
	[[nodiscard]] bool MarksThing(entt::entity object) const;
	/// How a thing hangs in the hand
	[[nodiscard]] hand_grab::HoldFacts HoldOfObject(entt::entity object) const;
	/// The hand starts pulling at a thing: its base, the plane of the land the pull works in, and its lean
	void StartPull(components::HandGrab& grab, const Frame& frame);
	/// A frame of pulling; where the hand is, on the thing
	glm::vec3 Pull(components::HandGrab& grab, const Frame& frame);
	/// The hand takes a thing it waited for or pulled free; or, not from the world, a handful it made as it scoops. With
	/// sounds, the hand also plays the sounds of the take.
	/// Without askInfluence, the hand takes the thing outside its player's influence too (the debug tools only)
	void Take(components::HandGrab& grab, entt::entity object, bool fromTheWorld = true, bool sounds = true,
	          bool askInfluence = true);
	/// The hand sends the command that takes a thing that it waited for or pulled free, and plays the sounds of the take
	void SendTake(components::HandGrab& grab, entt::entity object);
	/// The pick-up sound of a thing, and the cry of a villager
	void PlayTakeSounds(entt::entity object);
	/// A press on a pile scoops a handful out of it, which grows while the button is held; whether it did
	/// Whether a press on a thing scoops from it: a pile, a field or a fish farm with something to give, inside the
	/// player's influence. The frame asks it, and sends the lock.
	[[nodiscard]] bool Scoopable(entt::entity source) const;
	/// Whether the held thing can be used on a target, as ApplyTo uses it, with no change. The frame asks it, and sends the
	/// use.
	[[nodiscard]] bool AppliesTo(const components::HandGrab& grab, entt::entity target) const;
	bool StartScoop(components::HandGrab& grab, entt::entity source);
	/// A game turn of scooping: whether the scoop goes on
	bool Scoop(components::HandGrab& grab);
	using FieldFacts = hand_grab::HandGrabWorldInterface::FieldFacts;
	/// A field gives its food the same way, half of it once ripe
	bool StartFieldScoop(components::HandGrab& grab, entt::entity field, const FieldFacts& facts);
	bool ScoopField(components::HandGrab& grab, const FieldFacts& facts);
	/// A fish farm gives a first handful whatever fish it has, and then its fish as the scoop ramps up
	bool StartFishFarmScoop(components::HandGrab& grab, entt::entity farm, const fish_farm::Type& type);
	bool ScoopFishFarm(components::HandGrab& grab, const fish_farm::Type& type);
	/// The scoop ends: its stream stops, and the hand holds its handful as anything else
	/// The scoop's stream and sound stop
	void StopScoopEffects(components::HandGrab& grab);
	void EndScoop(components::HandGrab& grab);
	/// The second press with the pointer on something the held thing is used on (a store, a pile of the same): it is
	/// given to it at once, without being thrown. Whether it was.
	bool ApplyTo(components::HandGrab& grab, entt::entity target);
	/// Sends the gift of what it holds to the creature it offers it to, or to a creature under the cursor that would take
	/// it. Whether it sent the gift.
	bool Give(components::HandGrab& grab, std::optional<entt::entity> under);
	/// Each frame it holds something: the offer to a creature under it is kept up or runs down, the held thing turns over
	/// to offer it, and while the creature reaches for it the hand is drawn to the creature's hand. Where the hand is to
	/// be, before it rises for what it holds.
	glm::vec3 UpdateOffer(components::HandGrab& grab, const Frame& frame);
	static void ForgetOffer(components::HandGrab& grab);
	/// The hand lets go of what it holds at a velocity, put down or thrown; made to, it starts from where it is held
	/// and is never planted again
	/// Lets go of the thing that the hand holds. With no start, the hand must let go: the thing goes from where it is.
	void LetGo(components::HandGrab& grab, glm::vec3 velocity, const std::optional<player_commands::ThrowStart>& start);
	/// Where a thing that the hand lets go of at a velocity starts, as this computer drew it last
	[[nodiscard]] player_commands::ThrowStart ThrowStartOf(entt::entity object, glm::vec3 velocity);
	/// The hand holds nothing, having let go before taking anything or let go of what it held
	void Empty(components::HandGrab& grab);

	/// Whether the hand's point on the land is in its player's influence
	[[nodiscard]] bool HandInInfluence() const;

	std::unique_ptr<hand_grab::HandGrabWorldInterface> _world;
};

} // namespace openblack::ecs::systems
