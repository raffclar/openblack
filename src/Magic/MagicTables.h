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
#include <optional>
#include <string_view>
#include <type_traits>

#include "Enums.h"
#include "InfoConstants.h"

// The miracles' info.dat tables: GMagicInfo* per MAGIC_TYPE, GMagicEffectInfo[42], GSpellSeedInfo[30] and their
// non-virtual helpers. Wiki: docs/bw1-notes/magic.md.

namespace openblack::magic
{
constexpr size_t k_MagicTypeCount = 42;
constexpr size_t k_SpellSeedCount = 30;
constexpr int k_MagicTypeNotFound = 42; ///< what callers pass on when GetInfoFromText finds none
constexpr int k_SpellSeedNotFound = 30; ///< what callers pass on when the seed lookups by name or type find none

/// The info.dat section, i.e. the GMagicInfo class, of a MAGIC_TYPE. load_variables creates the objects in MAGIC_TYPE
/// order, one class per section, so the sections in file order give MAGIC_TYPE 0..41.
enum class MagicInfoSection : uint8_t
{
	General,         ///< 0-9 GMagicInfo
	Heal,            ///< 10-11 GMagicHealInfo
	Teleport,        ///< 12 GMagicTeleportInfo
	Forest,          ///< 13 GMagicForestInfo
	Food,            ///< 14-15 GMagicResourceInfo
	StormAndTornado, ///< 16-18 GMagicStormAndTornadoInfo
	Shield,          ///< 19-20 GMagicShieldInfo
	Wood,            ///< 21 GMagicResourceInfo
	Water,           ///< 22-23 GMagicWaterInfo
	FlockFlying,     ///< 24 GMagicFlockFlyingInfo
	FlockGround,     ///< 25 GMagicFlockGroundInfo
	CreatureSpell,   ///< 26-41 GMagicCreatureSpellInfo

	_COUNT
};

struct MagicInfoSlot
{
	MagicInfoSection section;
	uint8_t index; ///< in the section's array
};

/// MAGIC_TYPE -> its section and index (valid for 0..41)
[[nodiscard]] MagicInfoSlot SlotOf(MagicType type);

// ---- GMagicInfo (one per MAGIC_TYPE) ----

/// The MAGIC_TYPE's entry: the section record, as its GMagicInfo base
[[nodiscard]] const GMagicInfo& GetMagicInfo(const InfoConstants& info, MagicType type);

/// The record as its class; nullptr when the magic type is of another section
template <class T>
[[nodiscard]] const T* GetMagicInfoAs(const InfoConstants& info, MagicType type);

/// The magic type's effect row
[[nodiscard]] const GMagicEffectInfo& GetMagicEffectInfo(const InfoConstants& info, MagicType type);

/// Case-insensitive match on the effect's debugString; none when no effect has that name
[[nodiscard]] std::optional<int> GetInfoFromText(const InfoConstants& info, std::string_view text);

/// FOREST, SHIELD, PHYSICAL_SHIELD
[[nodiscard]] bool IsMaintainedSpell(MagicType type);

/// costToCreate (also the script's mana for a spell)
[[nodiscard]] float GetChantsRequiredToCreate(const InfoConstants& info, MagicType type);

// The timer getters, seconds (-1 = no limit)
[[nodiscard]] float GetTimerWhenOneShot(const InfoConstants& info, MagicType type);
[[nodiscard]] float GetTimerWhenPlayerCasting(const InfoConstants& info, MagicType type);
[[nodiscard]] float GetTimerWhenCreatureCasting(const InfoConstants& info, MagicType type);
[[nodiscard]] float GetTimerWhenComputerPlayerCasting(const InfoConstants& info, MagicType type);

/// isCreatureCastFromAbove == 1
[[nodiscard]] bool IsCreatureCastFromAbove(const InfoConstants& info, MagicType type);

/// 1 when agressiveRangeMin <= distance <= agressiveRangeMax, else 0
[[nodiscard]] float IsInAggressiveRange(const InfoConstants& info, MagicType type, float distance);

/// The product of the player's TribalPower[t] (1.0 in vanilla) over the tribes the effect flags, clamped to
/// [0.5, 100]; 1 without a player (tribalPower nullptr)
[[nodiscard]] float GetTribalPower(const GMagicEffectInfo& effect, const std::array<float, 9>* tribalPower);

/// The first flagged tribe whose power is over 1; none without one (or without a player)
[[nodiscard]] std::optional<int> GetTribalPowerTribe(const GMagicEffectInfo& effect, const std::array<float, 9>* tribalPower);

// ---- GSpellSeedInfo ----

/// The seed type's entry
[[nodiscard]] const GSpellSeedInfo& GetSpellSeedInfo(const InfoConstants& info, SpellSeedType seed);

/// -1 for magicTypes[0], 0/1/2 for magicTypes[1..3], -1 if none
/// (a seed whose slot 3 is 0 gives 2 for MAGIC_TYPE NONE, as the original)
[[nodiscard]] int GetPowerUpFromMagicType(const GSpellSeedInfo& seed, MagicType type);

/// The number of power-up levels, 1 + the non-zero powerUpGestures
[[nodiscard]] int GetNumPowerUpLevels(const GSpellSeedInfo& seed);

/// -1 -> magicTypes[0], pu -> magicTypes[pu + 1]
[[nodiscard]] MagicType MagicTypeForPowerUpLevel(const GSpellSeedInfo& seed, int powerUp);

/// That level's GMagicInfo, the base one when the level has no magic type (0)
[[nodiscard]] const GMagicInfo& MagicInfoForPowerUpLevel(const InfoConstants& info, const GSpellSeedInfo& seed, int powerUp);

/// A power-up's gesture and level
struct PowerUpGesture
{
	GestureType gesture {GestureType::None};
	int level {-1}; ///< -1 for the base type (no gesture) or a type the seed does not have
};

/// The gesture that powers the seed up to that magic type's level, and the level
[[nodiscard]] PowerUpGesture GetPowerUpGesture(const GSpellSeedInfo& seed, MagicType type);

/// Any of magicTypes[0..3]
[[nodiscard]] bool SpellSeedIsOfMagicType(const GSpellSeedInfo& seed, MagicType type);

/// SpellSeedType::None (-1) when no seed has it
[[nodiscard]] SpellSeedType GetFirstSpellSeedForMagicType(const InfoConstants& info, MagicType type);

/// The seed a magic's info names while the game runs. The data leaves the field at none in every row; once the tables
/// are loaded the game writes there the first seed of each magic type but NONE, when a seed has it. NONE and a type
/// with no seed keep the row's own value; a type outside the table names none
[[nodiscard]] SpellSeedType GetSpellSeedOfMagicInfo(const InfoConstants& info, MagicType type);

/// GetPowerUpGesture on the first seed of that magic type (no gesture and -1 without one)
[[nodiscard]] PowerUpGesture GetPowerUpGestureForMagicType(const InfoConstants& info, MagicType type);

/// The gesture a magic's info names while the game runs. The data leaves the field at none in every row; once the
/// tables are loaded the game writes there, for each magic type but NONE, the gesture that powers the type's first seed
/// up to it, when it has one. The other rows keep their own value; a type outside the table names none
[[nodiscard]] GestureType GetGestureOfMagicInfo(const InfoConstants& info, MagicType type);

/// The power-up level a magic's info names while the game runs (-1 base, 0 PU one, 1 PU two): written at load with
/// that gesture, so the rows that get no gesture keep their own value (-1 in the data); a type outside the table
/// names -1. It is the level a spell of that type runs at (for the PSys, the storm, the fireball rows)
[[nodiscard]] int GetPowerUpLevelOfMagicInfo(const InfoConstants& info, MagicType type);

/// The first existing seed whose iconIndex is that one; none without one
[[nodiscard]] std::optional<int> GetSpellSeedFromIconIndex(const InfoConstants& info, uint32_t iconIndex);

/// Case-insensitive match on the seed's debugString; none when no seed has that name
[[nodiscard]] std::optional<int> GetSpellSeedFromText(const InfoConstants& info, std::string_view text);

/// The first seed with that magic type; none without one
[[nodiscard]] std::optional<int> GetSpellSeedForMagicType(const InfoConstants& info, MagicType type);

// ---- template definition ----

namespace detail
{
[[nodiscard]] const GMagicInfo* SectionRecord(const InfoConstants& info, MagicInfoSlot slot);

/// Whether a record of that section is a T (the resource and radius bases cover two sections each)
template <class T>
constexpr bool IsOfSection(MagicInfoSection section)
{
	using S = MagicInfoSection;
	if constexpr (std::is_same_v<T, GMagicGeneralInfo>)
	{
		return section == S::General;
	}
	else if constexpr (std::is_same_v<T, GMagicHealInfo>)
	{
		return section == S::Heal;
	}
	else if constexpr (std::is_same_v<T, GMagicTeleportInfo>)
	{
		return section == S::Teleport;
	}
	else if constexpr (std::is_same_v<T, GMagicForestInfo>)
	{
		return section == S::Forest;
	}
	else if constexpr (std::is_same_v<T, GMagicFoodInfo>)
	{
		return section == S::Food;
	}
	else if constexpr (std::is_same_v<T, GMagicWoodInfo>)
	{
		return section == S::Wood;
	}
	else if constexpr (std::is_same_v<T, GMagicResourceInfo>)
	{
		return section == S::Food || section == S::Wood;
	}
	else if constexpr (std::is_same_v<T, GMagicStormAndTornadoInfo>)
	{
		return section == S::StormAndTornado;
	}
	else if constexpr (std::is_same_v<T, GMagicShieldInfo>)
	{
		return section == S::Shield;
	}
	else if constexpr (std::is_same_v<T, GMagicRadiusSpellInfo>)
	{
		return section == S::StormAndTornado || section == S::Shield;
	}
	else if constexpr (std::is_same_v<T, GMagicWaterInfo>)
	{
		return section == S::Water;
	}
	else if constexpr (std::is_same_v<T, GMagicFlockFlyingInfo>)
	{
		return section == S::FlockFlying;
	}
	else if constexpr (std::is_same_v<T, GMagicFlockGroundInfo>)
	{
		return section == S::FlockGround;
	}
	else if constexpr (std::is_same_v<T, GMagicCreatureSpellInfo>)
	{
		return section == S::CreatureSpell;
	}
	else
	{
		static_assert(sizeof(T) == 0, "not a GMagicInfo section class");
	}
}
} // namespace detail

template <class T>
const T* GetMagicInfoAs(const InfoConstants& info, MagicType type)
{
	const auto slot = SlotOf(type);
	if (!detail::IsOfSection<T>(slot.section))
	{
		return nullptr;
	}
	return static_cast<const T*>(detail::SectionRecord(info, slot));
}
} // namespace openblack::magic
