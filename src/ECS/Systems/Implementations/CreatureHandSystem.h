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
#include <utility>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Camera/CreatureInteractCameraModel.h"
#include "Creature/CreatureSentReactions.h"
#include "ECS/Components/HandOnCreature.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class CreatureHandSystem final: public CreatureHandSystemInterface
{
public:
	bool Grab() override;
	[[nodiscard]] bool MayHold(entt::entity creature) const override;
	[[nodiscard]] creature_hand::InteractTip InteractTipOver(entt::entity creature, bool handInInfluence) const override;
	[[nodiscard]] bool IsClick() const override;
	std::optional<HandPose> Update(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, glm::vec2 cursor,
	                               float screenWidth, const input_steps::Steps& steps) override;
	void Release() override;
	bool Stroke(entt::entity creature, creature_feedback::BodyPart part) override;
	bool Slap(entt::entity creature, float heightShare, bool gentle, bool sweepsRight) override;
	[[nodiscard]] std::optional<entt::entity> GetCreature() const override;
	[[nodiscard]] std::optional<entt::entity> GetLockedCreature(PlayerNames player) const override;
	[[nodiscard]] bool IsLocked(entt::entity creature) const override;
	void DoLock(PlayerNames player, entt::entity creature) override;
	void DoUnlock(PlayerNames player, entt::entity creature) override;
	void DoBruise(PlayerNames player, const player_commands::CreatureBruise& bruise) override;
	void DoReaction(PlayerNames player, const player_commands::CreatureReaction& reaction) override;
	[[nodiscard]] bool IsHeldByCommand() const override;
	[[nodiscard]] std::optional<entt::entity> CreatureUnderCursor() const override;
	[[nodiscard]] float GetFeedbackSum() const override;
	[[nodiscard]] float GetLastFeedbackSum() const override;
	bool TakeFocusTip() override { return std::exchange(_focusTip, false); }
	void LookAtReportedHand() override;

private:
	/// The creature that a player holds looks at the hand that the player last reported
	void LookAtReportedHandOf(PlayerNames player);
	/// Whether the hand of the player may take hold of the creature
	[[nodiscard]] static bool MayHoldFor(PlayerNames player, entt::entity creature);
	/// Takes hold of a creature by a command, keeping the session if already held to it
	components::HandOnCreature* HoldByCommand(entt::entity creature);
	/// Taking hold of a creature with the button turns the camera onto it, and letting go gives the camera back
	void TakeCamera(entt::entity creature);
	void GiveCameraBack();
	/// What a step of the hand on the creature goes on
	struct StepInput
	{
		/// Where the hand showed at the last frame, which gives the height of a slap
		glm::vec3 shownAt;
		/// The places at the hand's distance on the lines through the cursor now and at the last step
		glm::vec3 here;
		glm::vec3 before;
		/// Where the line of sight touches the body, if it does
		std::optional<glm::vec3> touch;
		glm::vec2 cursor;
		float screenWidth;
		float stepMs;
		glm::vec3 rayOrigin;
		glm::vec3 rayDirection;
	};
	/// A step of stroking and slapping, which tells whether the hand slapped
	bool Step(components::HandOnCreature& contact, const StepInput& input);
	/// The hand's up this frame, easing as the hand rests on the body or leaves it
	static glm::vec3 TurnHand(components::HandOnCreature& contact, bool onBody, bool cursorMovedAcross, const glm::vec3& normal,
	                          const glm::vec3& creaturePosition, float seconds);
	/// The hand's up as it eases now, upright when it has no length
	[[nodiscard]] static glm::vec3 UpOf(const components::HandOnCreature& contact);
	/// A third hard slap in a row at one height bruises the creature where the line of sight meets its skin
	/// The world takes a bruise: the slapping player's own creature loses some life and, slapped itself, takes the wound
	static void ApplyBruise(PlayerNames player, const player_commands::CreatureBruise& bruise);
	static void Bruise(entt::entity creature, const glm::vec3& rayOrigin, const glm::vec3& rayDirection, uint8_t column);
	/// The held creature reacts to a stroke or a slap: at once with no commands, else at a later turn on every computer.
	/// Whether the creature takes the reaction now.
	bool React(entt::entity creature, size_t animation, bool mirrored, std::optional<creature_face::Request> face,
	           float interruptsAfter);

	/// The camera on the creature while the hand holds it, and where the player's camera was going when it took over
	CreatureInteractCameraModel* _cameraModel {nullptr};
	std::optional<creature_interact::View> _returnTo;
	bool _focusTip {false};
	/// The reactions that the hand sent, which wait for a turn. Input of this computer only: the world does not read it.
	creature_hand::SentReactions _sentReactions;
};

} // namespace openblack::ecs::systems
