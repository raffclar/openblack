/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <utility>
#include <vector>

#include <entt/signal/sigh.hpp>

#include "ECS/Systems/WorkshopSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class WorkshopSystem final: public WorkshopSystemInterface
{
public:
	WorkshopSystem();

	void ProcessWorkshop(entt::entity workshop) override;
	[[nodiscard]] int32_t RoomOf(entt::entity workshop) const override;
	entt::entity WoodPileOf(entt::entity workshop) override;
	entt::entity CreateScaffold(glm::vec3 place, entt::entity town, entt::entity workshop, float yAngle, float scale) override;
	[[nodiscard]] bool CanTake(entt::entity scaffold) const override;
	void TakenByHand(entt::entity scaffold, PlayerNames player) override;
	void LetGoByHand(entt::entity scaffold, bool forced) override;
	void NotPutDownByHand(entt::entity scaffold) override;
	void CameToRest(entt::entity scaffold, bool fromHand, bool inMap) override;
	[[nodiscard]] bool CanMove(entt::entity scaffold) const override;
	[[nodiscard]] entt::entity TownForBuilding(glm::vec3 place) const override;

private:
	/// The workshop starts a scaffold: it uses the wood and its work sound starts
	void StartScaffold(entt::entity workshop);
	/// The workshop finishes its scaffold, which stands in the first free place of its yard
	void FinishScaffold(entt::entity workshop);
	/// The workshop stops to count the scaffold as its own
	void ReleaseScaffold(entt::entity scaffold);
	/// The scaffold goes into a place of a workshop's yard when it lies near one; false when it does not
	bool SnapToPlace(entt::entity workshop, entt::entity scaffold);
	/// The scaffold stands in its place of its workshop's yard, turned as the place is turned
	void MoveToPlace(entt::entity workshop, entt::entity scaffold);

	void OnScaffoldGone(entt::registry& registry, entt::entity scaffold);
	void OnWorkshopGone(entt::registry& registry, entt::entity workshop);

	std::vector<entt::scoped_connection> _connections;
};

} // namespace openblack::ecs::systems
