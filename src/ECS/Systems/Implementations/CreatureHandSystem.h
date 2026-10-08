/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

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
	bool Grab(entt::entity creature) override;
	[[nodiscard]] bool MayHold(entt::entity creature) const override;
	std::optional<HandPose> Update(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, glm::vec2 cursor,
	                               float seconds) override;
	[[nodiscard]] std::optional<Feedback> Release() override;
	[[nodiscard]] std::optional<HandPose> Pose() const override { return _pose; }
	[[nodiscard]] std::optional<entt::entity> GetCreature() const override;
	[[nodiscard]] std::optional<CreatureHit> CreatureAlong(const glm::vec3& rayOrigin,
	                                                       const glm::vec3& rayDirection) const override;
	[[nodiscard]] std::optional<entt::entity> CreatureUnderHand() const override { return _underHand; }
	void SetCreatureUnderHand(std::optional<entt::entity> creature) override { _underHand = creature; }
	[[nodiscard]] float GetFeedbackSum() const override;
	[[nodiscard]] float GetLastFeedbackSum() const override;

private:
	/// Where the hand was drawn by the last Update while held to a creature
	std::optional<HandPose> _pose;
	/// The creature the hand's pick found under the hand this frame
	std::optional<entt::entity> _underHand;
};

} // namespace openblack::ecs::systems
