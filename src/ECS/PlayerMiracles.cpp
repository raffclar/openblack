/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PlayerMiracles.h"

#include <cstddef>

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
[[nodiscard]] bool Known(MagicType type)
{
	return static_cast<size_t>(type) < Player::k_MagicTypeCount;
}
} // namespace

void player_miracles::SetMagicTypeEnabled(Player::Miracles& miracles, MagicType type, bool enable)
{
	if (!Known(type))
	{
		return;
	}
	const auto index = static_cast<size_t>(type);
	auto& count = miracles.enabled.at(index);
	if (enable)
	{
		++count;
		miracles.everEnabled.at(index) = true;
	}
	else if (count > 0)
	{
		--count;
	}
}

bool player_miracles::IsMagicTypeEnabled(const Player::Miracles& miracles, MagicType type)
{
	if (miracles.allEnabled)
	{
		return true;
	}
	return Known(type) && miracles.enabled.at(static_cast<size_t>(type)) > 0;
}

Player::Miracles player_miracles::AfterLandCleared(const Player::Miracles& miracles)
{
	auto after = miracles;
	after.enabled.fill(0);
	after.tribalPower = Player::k_UsualTribalPower;
	after.maxTribalPower = Player::k_UsualTribalPower;
	return after;
}
