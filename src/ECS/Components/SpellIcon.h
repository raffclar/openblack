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

#include <array>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"
#include "Magic/MagicTables.h"
#include "Magic/SpellIcons.h"

namespace openblack::ecs::components
{

/// The miracles a town offers, on the town's entity: the magic types it holds, given by its land's script. Its player may
/// cast them while the town is theirs.
struct TownMagic
{
	magic::spell_icons::HeldMagic held {};
};

/// A stone hand on a village centre, holding up one of the miracles the town offers. Tapped by its player's hand, it
/// charges that miracle at the town's worship site.
struct TownCentreIcon
{
	entt::entity centre {entt::null};
	entt::entity town {entt::null};
	/// Which of the centre's six hands it is
	uint8_t hand {0};
	SpellSeedType seed {SpellSeedType::None};
	/// The power-up levels of its miracle the town offers too
	std::array<bool, magic::spell_icons::k_PowerUpLevels> powerUps {};
};

/// A working village centre's stone hands, on the centre's entity: the one standing on each of its six places, none where
/// there is none
struct CentreHands
{
	std::array<entt::entity, magic::spell_icons::k_CentreHands> hands {entt::null, entt::null, entt::null,
	                                                                   entt::null, entt::null, entt::null};
};

/// A worship site's icons, on the site's entity, in the order they were made
struct SiteIcons
{
	std::vector<entt::entity> icons;
};

/// A worship site's icon of a miracle its towns offer, where the miracle is charged from the site's prayer power for the
/// hand that asked for it
struct WorshipIcon
{
	entt::entity site {entt::null};
	SpellSeedType seed {SpellSeedType::None};
	/// It is charging its miracle, at a power-up level, for its player's hand, since a turn
	bool charging {false};
	int powerUp {magic::k_BasePowerUpLevel};
	PlayerNames charger {PlayerNames::NEUTRAL};
	uint32_t chargeStartTurn {0};
	/// The prayer power it has stored for its miracle
	float store {0.0f};
	/// The seed made from it while that is out, in a hand or cast
	entt::entity seedOut {entt::null};
};

/// The miracle's seed spinning above an icon's palm, as a globe's spins
struct IconSeed
{
	SpellSeedType seed {SpellSeedType::None};
	/// Where it spins: above the first point of the icon's model
	glm::vec3 middle {0.0f};
	/// The highest power-up level shown, with a ring round it for each
	int powerUp {magic::k_BasePowerUpLevel};
	float spin {0.0f};
	float ringSpin {0.0f};
	/// A creature spell's phial runs through its texture and pulses
	float phialFrame {0.0f};
	float phialPhase {0.0f};
};

} // namespace openblack::ecs::components
