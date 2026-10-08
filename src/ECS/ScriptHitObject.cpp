/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptHitObject.h"

#include "ECS/Registry.h"
#include "ECS/Systems/ScriptStateInterface.h"
#include "ECS/ToBeDeleted.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
bool Available(entt::entity thing)
{
	return thing != entt::null && Locator::entitiesRegistry::has_value() && Locator::entitiesRegistry::value().Valid(thing) &&
	       ecs::IsAvailable(thing);
}

/// A stored thing while it is still there
entt::entity StillThere(entt::entity thing)
{
	return thing != entt::null && Locator::entitiesRegistry::has_value() && Locator::entitiesRegistry::value().Valid(thing)
	           ? thing
	           : entt::entity {entt::null};
}
} // namespace

void script_hit::Record(ScriptHitObjects& objects, entt::entity hit, bool hitAvailable, entt::entity hitter,
                        bool hitterAvailable)
{
	objects.hit = hit != entt::null && hitAvailable ? hit : entt::entity {entt::null};
	objects.hitter = hitter != entt::null && hitterAvailable ? hitter : entt::entity {entt::null};
}

void script_hit::SetHitObject(entt::entity hit, entt::entity hitter)
{
	if (!Locator::scriptState::has_value())
	{
		return;
	}
	Record(Locator::scriptState::value().Get<ScriptHitObjects>(), hit, Available(hit), hitter, Available(hitter));
}

entt::entity script_hit::HitObject()
{
	return Locator::scriptState::has_value() ? StillThere(Locator::scriptState::value().Get<ScriptHitObjects>().hit)
	                                         : entt::entity {entt::null};
}

entt::entity script_hit::ObjectWhichHit()
{
	return Locator::scriptState::has_value() ? StillThere(Locator::scriptState::value().Get<ScriptHitObjects>().hitter)
	                                         : entt::entity {entt::null};
}
