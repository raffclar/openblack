/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The CHL influence natives 60-62 (CHLApi.cpp forwards to them). The arguments are popped in the handlers' order.

#include "CHLInfluence.h"

#include <cstdint>

#include <LHVM.h>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "ECS/Influence/Influence.h"
#include "Enums.h"
#include "Locator.h"
#include "Magic/Script/ScriptPlayer.h"

using namespace openblack;

namespace
{
/// The game's player index (0..7 = PlayerNames); no player for 8 and more
bool GamePlayer(int32_t index, PlayerNames& player)
{
	if (index < 0 || index >= static_cast<int32_t>(PlayerNames::_COUNT))
	{
		return false;
	}
	player = static_cast<PlayerNames>(index);
	return true;
}

glm::vec3 PopPosition(lhvm::LHVM& vm)
{
	const auto z = vm.Popf();
	const auto y = vm.Popf();
	const auto x = vm.Popf();
	return {x, y, z};
}
} // namespace

namespace openblack::magic::script
{
void InfluenceObject()
{
	// anti, player (the game index, not a script player), radius, object; pushes the ring or 0
	auto& vm = Locator::vm::value();
	const auto anti = vm.Pop().intVal;
	const auto playerIndex = vm.Pop().intVal;
	const auto radius = vm.Popf();
	const auto object = static_cast<entt::entity>(vm.Pop().uintVal);
	PlayerNames player {};
	entt::entity ring = entt::null;
	if (GamePlayer(playerIndex, player))
	{
		ring = influence::CreateRingOnObject(object, player, radius, anti != 0);
	}
	if (ring == entt::null)
	{
		// (a null player makes a ring nobody's influence reads; openblack does not make it)
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "INFLUENCE_OBJECT: thing not valid. Could not make influence ring!");
		vm.Pusho(0);
		return;
	}
	vm.Pusho(static_cast<uint32_t>(ring));
}

void InfluencePosition()
{
	// anti, player (game index), radius, position -> a new influence ring
	auto& vm = Locator::vm::value();
	const auto anti = vm.Pop().intVal;
	const auto playerIndex = vm.Pop().intVal;
	const auto radius = vm.Popf();
	const auto position = PopPosition(vm);
	PlayerNames player {};
	if (!GamePlayer(playerIndex, player))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "INFLUENCE_POSITION: Could not make influence ring! (player {})",
		                   playerIndex);
		vm.Pusho(0);
		return;
	}
	vm.Pusho(static_cast<uint32_t>(influence::CreateRing(position, player, radius, anti != 0)));
}

void GetInfluence()
{
	// position, raw, player (a float, a script player: 0 = the neutral player; n = game player n - 1) ->
	// CalculatePlayerInfluence(pos, player, allies = raw == 0)
	auto& vm = Locator::vm::value();
	const auto position = PopPosition(vm);
	const auto raw = vm.Pop().intVal;
	const auto scriptPlayer = static_cast<int32_t>(vm.Popf());
	PlayerNames player {};
	if (!magic::ScriptPlayerToGamePlayer(scriptPlayer, player))
	{
		vm.Pushf(0.0f); // no player: 0
		return;
	}
	vm.Pushf(influence::CalculatePlayerInfluence(player, position, influence::CalcType::Default, raw == 0));
}
} // namespace openblack::magic::script
