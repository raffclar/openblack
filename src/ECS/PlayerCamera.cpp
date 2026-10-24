/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PlayerCamera.h"

#include "Camera/Camera.h"
#include "ECS/Components/ReportedHand.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Locator.h"

namespace openblack::ecs::player_camera
{

View Of(PlayerNames player)
{
	const bool networkGame = Locator::time::has_value() && Locator::time::value().IsNetworkGame();
	if (!networkGame)
	{
		if (!Locator::camera::has_value())
		{
			return {};
		}
		const auto& camera = Locator::camera::value();
		return {.origin = camera.GetOrigin(), .focus = camera.GetFocus()};
	}
	// The last report of the player's hand has the camera of the player, the same on every computer
	if (!Locator::handSystem::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return {};
	}
	const auto hands = Locator::handSystem::value().GetHandsOf(player);
	if (!hands.has_value())
	{
		return {};
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto hand = (*hands)[static_cast<size_t>(systems::HandSystemInterface::Side::Left)];
	const auto* reported = registry.Valid(hand) ? registry.TryGet<const components::ReportedHand>(hand) : nullptr;
	return reported != nullptr ? View {.origin = reported->camera, .focus = reported->focus} : View {};
}

} // namespace openblack::ecs::player_camera
