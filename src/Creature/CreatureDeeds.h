/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

namespace openblack::creature_watching
{
/// The deeds of a player that the player's creature watches and may copy. Each is the row of the same name in the
/// game's table of mimicry rules (InfoConstants::creatureMimic), in the same order.
enum class Deed : uint8_t
{
	PutFoodInWorshipSite = 0,
	CastMagicFoodInWorshipSite = 1,
	PutFoodInStoragePit = 2,
	CastMagicFoodInStoragePit = 3,
	PutWoodInStoragePit = 4,
	CastMagicWoodInStoragePit = 5,
	BuildHouse = 6,
	PutWoodInBuildingSite = 7,
	CastMagicWoodByBuildingSite = 8,
	PutWoodInWorkshop = 9,
	CastMagicWoodByWorkshop = 10,
	PlantTree = 11,
	GiveTownProtectionWithShield = 12,
	BringPeopleToWorship = 13,
	MakeArtefact = 14,
	DamageByThrowing = 15,
	DamageByThrowingAt = 16,
	DamageWithFire = 17,
	DamageWithMagic = 18,
	ImpressByThrowing = 19,
	ImpressWithMagic = 20,
	ThrowInTheSea = 21,
	MakeDiscipleFarmer = 22,
	MakeDiscipleForester = 23,
	MakeDiscipleFisherman = 24,
	MakeDiscipleBuilder = 25,
	MakeDiscipleBreeder = 26,
	MakeDiscipleProtection = 27,
	MakeDiscipleMissionary = 28,
	MakeDiscipleCraftsman = 29,
	MakeDiscipleChangeHouse = 30,
	MakeDiscipleWorship = 31,
	TakeObjectHome = 32,
	CastWaterOnCrops = 33,
	CastWaterToPutOutFire = 34,
	StealObjectAndPutInTown = 35,
	StealObjectAndPutByCitadel = 36,
	BreakRocks = 37,
	ThrowFootballInGoal = 38,
	CatchFootball = 39,
	Sacrifice = 40,
	PlayWithToy = 41,
	Heal = 42,
	StealFoodFromFarm = 43,
	StealFoodFromStoragePit = 44,
	StealWoodFromStoragePit = 45,
};

/// How many deeds there are, one per row of the mimicry table
constexpr size_t k_DeedCount = 46;
static_assert(static_cast<size_t>(Deed::StealWoodFromStoragePit) + 1 == k_DeedCount);

} // namespace openblack::creature_watching
