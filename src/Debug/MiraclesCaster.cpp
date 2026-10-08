/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MiraclesCaster.h"

#include "Magic/CastRules.h"
#include "Magic/Core/OneOffSpellSeed.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellCreator.h"
#include "Worship/SpellDispenser.h"

using namespace openblack;
using namespace openblack::debug::miracles;

entt::entity GameSpellCaster::CastAtPoint(const CastPlan& plan, PlayerNames player, glm::vec3 point)
{
	auto cast = plan.cast;
	entt::entity spell = entt::null;
	// the spells take a map position: x, z on the map and y above the land, as the hand's and the scripts' casts do
	magic::CastAtPos(plan.type, magic::creator::OfPlayer(player), magic::ToMap(point), &spell, &cast, plan.process);
	return spell;
}

entt::entity GameSpellCaster::CastOnObject(const CastPlan& plan, PlayerNames player, entt::entity target)
{
	auto cast = plan.cast;
	entt::entity spell = entt::null;
	magic::CastAtObject(plan.type, magic::creator::OfPlayer(player), target, &spell, &cast, plan.process);
	return spell;
}

bool GameSpellCaster::CanCastAt(MagicType type, [[maybe_unused]] PlayerNames player, glm::vec3 point) const
{
	return magic::cast_rules::CanCastAt(type, magic::ToMap(point));
}

entt::entity GameDispenserCreator::CreateOneShot(SpellSeedType seed, int powerUpLevel, glm::vec3 point)
{
	return magic::one_off::Create(point, seed, powerUpLevel, 1.0f);
}

entt::entity GameDispenserCreator::CreateOneShotInHand(SpellSeedType seed, int powerUpLevel, PlayerNames player)
{
	return magic::one_off::CreateSpellIntoHand(player, seed, powerUpLevel, 1.0f);
}

entt::entity GameDispenserCreator::CreatePermanent(AbodeInfo abode, MagicType magic, uint32_t periodTurns, glm::vec3 point)
{
	constexpr int k_NearestTown = -1;
	const auto dispenser = worship::dispenser::Create(point, abode, k_NearestTown, 0.0f, 1.0f);
	if (dispenser != entt::null)
	{
		// its miracle, an orb at once, and the period (0 leaves it inactive)
		worship::dispenser::SetMagicAndPeriod(dispenser, magic, periodTurns);
	}
	return dispenser;
}
