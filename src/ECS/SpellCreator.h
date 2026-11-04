/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

#include "ECS/Components/Spell.h"

/// The thing that made a miracle, as the scripts see it (OBJECT_CAST_BY_OBJECT). The game keeps one thing for each
/// miracle:
/// - A creature or another object that casts a miracle is its creator.
/// - A hand that casts a seed made at a spell icon gives the icon as the creator.
/// - Other miracles of a player (a seed made at no icon, or a script for the neutral player) have the player as the
///   creator. A script can not hold a player, so openblack keeps no thing for them.
/// The miracle loses its creator when it closes down because its caster is gone.
namespace openblack::ecs::spell_creator
{

/// The creator of a miracle that this caster casts, none for a player's own miracle
[[nodiscard]] constexpr entt::entity CreatorOf(const components::SpellCaster& caster)
{
	using Kind = components::SpellCaster::Kind;
	switch (caster.kind)
	{
	case Kind::Creature:
	case Kind::Object:
		return caster.entity;
	case Kind::Player:
		return caster.madeBySpellIcon ? caster.icon : entt::null;
	case Kind::None:
		break;
	}
	return entt::null;
}

/// Whether the thing made the miracle. No thing made a miracle that has no creator.
[[nodiscard]] constexpr bool CastBy(const components::Spell& spell, entt::entity thing)
{
	return thing != entt::null && spell.creator == thing;
}

} // namespace openblack::ecs::spell_creator
