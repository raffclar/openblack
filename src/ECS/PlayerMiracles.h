/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Components/Player.h"
#include "Enums.h"

/// The miracles a player may cast and the power the tribes give them. Things such as towns and worship sites enable a
/// magic type for their player, each counted, and the type stays enabled while any of them does.
namespace openblack::player_miracles
{

/// One more thing enables the magic type for the player, which marks it as ever enabled; or one fewer does, never
/// below none. Types beyond the game's are left alone.
void SetMagicTypeEnabled(ecs::components::Player::Miracles& miracles, MagicType type, bool enable);

/// Whether the player may cast the magic type: while anything enables it, or every type is enabled
[[nodiscard]] bool IsMagicTypeEnabled(const ecs::components::Player::Miracles& miracles, MagicType type);

/// As a land is cleared every player's enabled magic types are cleared and each tribe's power, and the most it has
/// been, go back to the usual. The types ever enabled and every type being enabled stay.
[[nodiscard]] ecs::components::Player::Miracles AfterLandCleared(const ecs::components::Player::Miracles& miracles);

} // namespace openblack::player_miracles
