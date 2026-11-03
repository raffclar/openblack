/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

#include "ECS/Systems/PuzzleTotemSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class PuzzleTotemSystem final: public PuzzleTotemSystemInterface
{
public:
	void SetHeight(entt::entity totem, int32_t height) override;
	void Place(entt::entity totem) override;
	bool Grip(entt::entity totem, PlayerNames player, glm::vec3 hand) override;
	void Slide(float upPixels, float screenHeight) override;
	void LetGo() override;
	[[nodiscard]] std::optional<entt::entity> GetGripped() const override;
	[[nodiscard]] std::optional<VillageTotemSystemInterface::HandHold> GetHandHold() const override;
	void DoHeld(entt::entity totem, bool held) override;
	void DoShare(entt::entity totem, float share) override;

private:
	/// The share of its greatest height that the totem the hand holds stands at, sent to the turn
	void SendShare() const;

	/// The totem the hand of this computer holds
	entt::entity _gripped {entt::null};
	/// Where the hand holds it, as an offset below its place, and the height that this gives
	float _hold {0.0f};
	int32_t _heldHeight {0};
};

} // namespace openblack::ecs::systems
