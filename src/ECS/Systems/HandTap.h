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

#include <algorithm>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "ECS/PotResource.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandTapRegistryInterface.h"
#include "ECS/ToBeDeleted.h"
#include "Locator.h"

// The hand's tap on an object, one handler per class: whether the object can be tapped (default false), the tap
// itself (default 1), which the interface's tap and its network packet call, and whether the hand must be in the
// player's influence to tap it (default true). Each owner registers the classes it ports; an object of no registered
// class cannot be tapped, as with the defaults. The hand checks the influence and IsCannotBePickedUp before.
// Wiki: docs/bw1-notes/hand-and-interface.md, "Tapping objects".
namespace openblack::ecs::hand_tap
{

/// Whether the object can be tapped: `is` is the tapping interface (the hand's Dropper)
using ValidToTapFn = bool (*)(entt::entity object, const pot_resource::Dropper& is);
/// The tap: `handPos` is the hand's point
using TapFn = uint32_t (*)(entt::entity object, const pot_resource::Dropper& is, glm::vec3 handPos);

struct Handler
{
	bool (*isClass)(entt::entity object);
	ValidToTapFn validToTap;
	TapFn tap;
	/// The hand must be in the player's influence to tap it
	bool needsInfluence {true};
};

inline std::vector<Handler>& Handlers()
{
	return Locator::handTapRegistry::value().Handlers();
}

/// A class told by a test of its own (a class with no component, such as Rock: Rocks::IsRock). The classes are tested in
/// registration order and the first one the object belongs to answers.
inline void Register(bool (*isClass)(entt::entity object), ValidToTapFn validToTap, TapFn tap, bool needsInfluence = true)
{
	// once per class: its first archetype registers it, and the later ones find it there
	auto& handlers = Handlers();
	if (std::ranges::any_of(handlers, [isClass](const Handler& handler) { return handler.isClass == isClass; }))
	{
		return;
	}
	handlers.push_back({isClass, validToTap, tap, needsInfluence});
}

/// One class = one component
template <typename Component>
void Register(ValidToTapFn validToTap, TapFn tap, bool needsInfluence = true)
{
	Register([](entt::entity object) { return Locator::entitiesRegistry::value().AllOf<Component>(object); }, validToTap, tap,
	         needsInfluence);
}

/// The handler of the object's class, nullptr when none is registered (the defaults: not tappable)
inline const Handler* Find(entt::entity object)
{
	if (object == entt::null || !ecs::IsAvailable(object))
	{
		return nullptr;
	}
	for (const auto& handler : Handlers())
	{
		if (handler.isClass(object))
		{
			return &handler;
		}
	}
	return nullptr;
}

/// False for an object of no registered class
inline bool ValidToTap(entt::entity object, const pot_resource::Dropper& is)
{
	const auto* handler = Find(object);
	return handler != nullptr && handler->validToTap(object, is);
}

/// True for an object of no registered class
inline bool NeedsInfluence(entt::entity object)
{
	const auto* handler = Find(object);
	return handler == nullptr || handler->needsInfluence;
}

/// Whether the hand's tap goes out as a tap packet: the hand in the player's influence or an object that does not need
/// it, the object valid to tap, and not flagged cannot be picked up
[[nodiscard]] constexpr bool SendsTap(bool inInfluence, bool needsInfluence, bool validToTap, bool cannotBePickedUp)
{
	return (inInfluence || !needsInfluence) && validToTap && !cannotBePickedUp;
}

/// 1 for an object of no registered class
inline uint32_t Tap(entt::entity object, const pot_resource::Dropper& is, glm::vec3 handPos)
{
	const auto* handler = Find(object);
	return handler != nullptr ? handler->tap(object, is, handPos) : 1;
}

} // namespace openblack::ecs::hand_tap
