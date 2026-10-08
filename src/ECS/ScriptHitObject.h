/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

/// The scripts' hit object: the last thing a body in the physics found itself hit by another this turn, and what hit
/// it. One pair for all the scripts; the last body the physics' turn end goes through wins. Kept in
/// Locator::scriptState (ScriptHitObjects).
namespace openblack::ecs::script_hit
{

/// The pair the scripts read with GET_HIT_OBJECT and GET_OBJECT_WHICH_HIT
struct ScriptHitObjects
{
	entt::entity hit {entt::null};
	entt::entity hitter {entt::null};
};

/// Each side is kept only while its thing is available (not being deleted); otherwise that side is cleared
void Record(ScriptHitObjects& objects, entt::entity hit, bool hitAvailable, entt::entity hitter, bool hitterAvailable);

/// The physics' end of a turn: `hit` was struck by `hitter` (Locator::scriptState's pair; each side as available as
/// ecs::IsAvailable says)
void SetHitObject(entt::entity hit, entt::entity hitter);
/// The stored hit object, or null when there is none or it has gone since
[[nodiscard]] entt::entity HitObject();
/// What hit it, or null when there is none or it has gone since
[[nodiscard]] entt::entity ObjectWhichHit();

} // namespace openblack::ecs::script_hit
