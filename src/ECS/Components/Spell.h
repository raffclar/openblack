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

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"
#include "Particles/SpellLink.h"

namespace openblack::ecs::components
{

/// The spell's creator: the thing that pays for and positions a spell (it maintains it and updates its info).
/// The original keeps a pointer; here the kind says which class it is.
struct SpellCreator
{
	enum class Kind : uint8_t
	{
		None,             ///< creator gone (its maintain request clears it)
		Player,           ///< a player: only the neutral player refills
		WorshipSpellIcon, ///< the worship site's battery (src/Worship)
		Creature,         ///< the creature's energy
		Thing,            ///< any other thing: it pays everything
	};
	Kind kind {Kind::None};
	PlayerNames player {PlayerNames::NEUTRAL}; ///< the player it belongs to
	entt::entity entity {entt::null};          ///< the icon / creature / thing (entt::null for a player)
};

/// The spell's class (the magic info class that allocated it): which SpellOps run it
enum class SpellClass : uint8_t
{
	General, ///< fireball, lightning bolt, beam explosion (the behaviour is the PSys)
	Heal,
	Teleport,
	Forest,
	Resource, ///< food, wood
	StormAndTornado,
	Shield,
	Water,
	FlockFlying,
	FlockGround,
	Creature,

	_COUNT
};

/// A cast spell. Positions are MapCoords as
/// metres: x and z on the map, y the altitude above the land.
struct Spell
{
	MagicType magicType {MagicType::None};
	SpellClass spellClass {SpellClass::General};
	glm::vec3 position {0.0f};                      ///< the cast position, then each applied event's (y 0)
	uint32_t reaction {0};                          ///< the Reaction it made (ECS/Effects/Reactions.h id), 0 none
	glm::vec3 movementDirection {1.0f, 0.0f, 0.0f}; ///< the last applied event's velocity
	float chants {0.0f};                            ///< the prayer power left in the spell (its life)
	float initialChants {0.0f};                     ///< the chants at the last SetChants
	bool closedDown {false};
	bool isMyInterfaceCasting {false}; ///< cast by the local player's interface
	bool isCreatureCasting {false};    ///< the creator is a creature
	bool isHumanPlayerCasting {false}; ///< the creator's player is human
	float duration {-1.0f};            ///< seconds; < 0 no limit
	float manaPathPerTurn {0.0f};      ///< the spell point accumulators
	float manaPathPerEvent {0.0f};
	bool free {false}; ///< paying returns true without paying (the setter has no caller)
	psys::ProcessInfo processInfo {};
	SpellCreator creator {};
	/// NEUTRAL when hasPlayer is false: the reactions, the effect and the last-cast record use it as the no-player value,
	/// so it stays a value next to the flag rather than an optional
	PlayerNames player {PlayerNames::NEUTRAL};
	bool hasPlayer {false};         ///< false when a null creator leaves it unset
	bool castFromInterface {false}; ///< cast from the hand (set after the cast)
	entt::entity seed {entt::null}; ///< the SpellSeed that cast it
	uint32_t psys {0};              ///< PSysInterface (psys::manager id), 0 none
	float age {0.0f};               ///< seconds
	/// the radius: from the cast data's magnitude (40 with no cast data)
	float magnitude {1.0f};
	glm::vec3 originalCastPos {0.0f};
	glm::vec3 castPos {0.0f};   ///< follows the hand for in-hand spells
	glm::vec3 direction {0.0f}; ///< = processInfo.direction at init
	float strengthMultiplier {1.0f};
	int maxObjectsToCreate {-1}; ///< kept by the spells that create objects
};

} // namespace openblack::ecs::components
