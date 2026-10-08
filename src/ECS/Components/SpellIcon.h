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

namespace openblack::ecs::components
{

/// A spell icon (a multi-map fixed object): the floating hand icon (the spell icon info's meshId 203
/// BuildingVillageCentreSpellHand) with the seed's SpellSeedGraphic above it. Both kinds below have it.
struct SpellIcon
{
	uint8_t infoIndex {0};                        ///< The spell icon info: 0 "Spell Icon" (site), 1 "TownSpell Icon"
	SpellSeedType seedType {SpellSeedType::None}; ///< The spell seed info
	PlayerNames player {PlayerNames::NEUTRAL};    ///< The owner (a site icon asks its site)
	entt::entity graphic {entt::null};            ///< The SpellSeedGraphic above it
	entt::entity chargeRing {entt::null};         ///< The charging ring: mesh 561 MSH_S_PULSE_IN
};

/// A worship site's spell icon
struct WorshipSpellIcon
{
	entt::entity site {entt::null};
	int16_t removeTimer {0}; ///< Set to 1000 by a routine with no caller: vestigial
	float savedScale {0.0f};
	bool charging {false};
	int powerUp {-1};                               ///< The power-up level being charged (-1 = the base)
	PlayerNames chargingFor {PlayerNames::NEUTRAL}; ///< The hand that charges it
	bool hasChargingInterface {false};              ///< A hand charges it
	std::vector<entt::entity> seeds;                ///< The SpellSeeds made from it (new ones at the head)
	float chantStore {0.0f};                        ///< The charge
	uint32_t chargeStartTurn {0};
	int16_t slot {0}; ///< The icon slot 10..15 (placement index = slot - 10)
};

/// A town centre's spell icon (a town spell icon)
struct TownCentreSpellIcon
{
	entt::entity town {entt::null};
	entt::entity townCentre {entt::null};
	uint8_t slot {0};                ///< The town centre's icons[6] index
	std::array<bool, 3> powerUps {}; ///< The power-up levels the town holds
};

/// The town centre's six spell icons (on the town centre's Abode entity)
struct TownCentreIcons
{
	std::array<entt::entity, 6> icons {entt::null, entt::null, entt::null, entt::null, entt::null, entt::null};
};

/// The seed's mesh floating above an icon or inside a one-shot orb, with the seed's holder effect (the spell seed
/// info's holderParticle) and its power-up band
struct SpellSeedGraphic
{
	SpellSeedType seedType {SpellSeedType::None};
	PlayerNames player {PlayerNames::NEUTRAL}; ///< The owner
	float scale {1.0f};
	/// The power-up band's size (band scale = 0.2 x this x scale); 1, 0.5 on a worship icon. Not an alpha: nothing else
	/// reads it.
	float bandScale {1.0f};
	bool autoUpdate {true};               ///< The holder effect is stepped by the list every turn
	int powerUp {-1};                     ///< The power-up level (the band when != -1)
	uint32_t psys {0};                    ///< The holder effect (psys::manager)
	entt::entity band {entt::null};       ///< The power-up band
	std::vector<entt::entity> extraBands; ///< The other bands: each level drawn twice, 2(pu + 1) - 1 besides `band`
	glm::vec3 point {0.0f};               ///< The point given: the bands' centre
	glm::vec3 meshPosition {0.0f};        ///< point + the info's mesh offset x scale: the mesh
	glm::vec3 effectPosition {0.0f};      ///< point + the info's effect offset x scale: the holder effect
	float spin {0.0f};                    ///< The mesh's y angle (+2 rad/s)
	/// The creature spell phials' frame, 0..32 at -15 a second (frame_anim::SpellIconFrame); starts at 0
	float uvPhase {0.0f};
	float bandSpin {0.0f};  ///< The bands' angle (+10.3 rad/s)
	float bandSpin2 {0.0f}; ///< +1 rad/s; no reader found
	/// The light on the mesh: a player seed takes the land's colour and specular at its position (no haze:
	/// land_light::ObjectMode::Cell), a creature spell phial the models' light
	bool landCellLight {true};
};

} // namespace openblack::ecs::components
