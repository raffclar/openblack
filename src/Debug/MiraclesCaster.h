/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "MiraclesModel.h"

namespace openblack::debug::miracles
{

/// The Miracles window's caster over the game's spells: a cast as the player's, as the hand's cast is. Its points are
/// world points; it turns them into map positions for the spells
class GameSpellCaster final: public CasterInterface
{
public:
	entt::entity CastAtPoint(const CastPlan& plan, PlayerNames player, glm::vec3 point) override;
	entt::entity CastOnObject(const CastPlan& plan, PlayerNames player, entt::entity target) override;
	[[nodiscard]] bool CanCastAt(MagicType type, PlayerNames player, glm::vec3 point) const override;
};

/// The Miracles window's dispensers over the game's own: a one-shot orb as the map script's one-shot with a power-up
/// makes it, the seed into the hand as the script's one-shot in the hand does, and a dispenser as the map script's
/// spell dispenser command makes it (the nearest town, angle 0, scale 1)
class GameDispenserCreator final: public DispenserCreatorInterface
{
public:
	entt::entity CreateOneShot(SpellSeedType seed, int powerUpLevel, glm::vec3 point) override;
	entt::entity CreateOneShotInHand(SpellSeedType seed, int powerUpLevel, PlayerNames player) override;
	entt::entity CreatePermanent(AbodeInfo abode, MagicType magic, uint32_t periodTurns, glm::vec3 point) override;
};

} // namespace openblack::debug::miracles
