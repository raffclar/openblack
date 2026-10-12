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
	[[nodiscard]] bool IsClick() const override;
	std::optional<HandPose> Update(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, glm::vec2 cursor,
	                               float screenWidth, float seconds) override;
	void Release() override;
	bool Stroke(entt::entity creature, creature_feedback::BodyPart part) override;
	bool Slap(entt::entity creature, float heightShare, bool gentle, bool sweepsRight) override;
	[[nodiscard]] std::optional<entt::entity> GetCreature() const override;
	[[nodiscard]] bool IsHeldByCommand() const override;
	[[nodiscard]] std::optional<entt::entity> CreatureUnderCursor() const override;
	[[nodiscard]] float GetFeedbackSum() const override;
	[[nodiscard]] float GetLastFeedbackSum() const override;
	bool TakeFocusTip() override { return std::exchange(_focusTip, false); }

private:
	/// Takes hold of a creature by a command, keeping the session if already held to it
	components::HandOnCreature* HoldByCommand(entt::entity creature);
	/// Taking hold of a creature with the button turns the camera onto it, and letting go gives the camera back
	void TakeCamera(entt::entity creature);
	void GiveCameraBack();
	/// What a step of the hand on the creature goes on
	struct StepInput
	{
		/// Where the hand is beside the body, and where it touches the body if it does
		glm::vec3 point;
		std::optional<glm::vec3> touch;
		glm::vec2 cursor;
		float screenWidth;
		float stepMs;
		glm::vec3 rayOrigin;
		glm::vec3 rayDirection;
	};
	/// A step of stroking and slapping
	void Step(components::HandOnCreature& contact, const StepInput& input);
	/// The hand's up this frame, easing as the hand rests on the body or leaves it
	static glm::vec3 TurnHand(components::HandOnCreature& contact, bool onBody, bool cursorMovedAcross, const glm::vec3& normal,
	                          const glm::vec3& creaturePosition, float seconds);
	/// A third hard slap in a row at one height bruises the creature where the line of sight meets its skin
	static void Bruise(entt::entity creature, const glm::vec3& rayOrigin, const glm::vec3& rayDirection, uint8_t column);

	/// The camera on the creature while the hand holds it, and where the player's camera was going when it took over
	CreatureInteractCameraModel* _cameraModel {nullptr};
	std::optional<creature_interact::View> _returnTo;
	bool _focusTip {false};
};

} // namespace openblack::ecs::systems
