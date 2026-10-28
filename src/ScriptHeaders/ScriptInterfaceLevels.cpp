/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptInterfaceLevels.h"

#include "ECS/Registry.h"

namespace openblack::script::interface_levels
{

namespace
{
/// The normal level, and the levels that let the player rotate and interact, with or without the zoom
constexpr int32_t k_Normal = 0;
constexpr int32_t k_JustRotateInteract = 6;
constexpr int32_t k_JustRotateInteractAndZoom = 7;
/// The level in the middle of the list that the game does not know
constexpr int32_t k_Unknown = 9;
/// The level that lets the hand interact but not move the camera
constexpr int32_t k_JustHandInteraction = 12;
/// The last level that the game knows
constexpr int32_t k_Last = 15;
} // namespace

ecs::components::InterfaceLimits AfterLevel(ecs::components::InterfaceLimits limits, int32_t level)
{
	if (level < k_Normal || level > k_Last || level == k_Unknown)
	{
		return limits;
	}
	if (level == k_Normal || level == k_JustRotateInteract || level == k_JustRotateInteractAndZoom)
	{
		return {};
	}
	if (level != k_JustHandInteraction)
	{
		limits.onlyHighlights = true;
	}
	limits.noHolding = true;
	return limits;
}

bool MayHold(const ecs::components::InterfaceLimits& limits)
{
	return !limits.noHolding;
}

bool MayPress(const ecs::components::InterfaceLimits& limits, bool highlightUnderHand)
{
	return !limits.onlyHighlights || highlightUnderHand;
}

void Apply(ecs::Registry& registry, entt::entity player, int32_t level)
{
	if (player == entt::null || !registry.Valid(player))
	{
		return;
	}
	const auto* now = registry.TryGet<const ecs::components::InterfaceLimits>(player);
	const auto after = AfterLevel(now != nullptr ? *now : ecs::components::InterfaceLimits {}, level);
	registry.AssignOrReplace<ecs::components::InterfaceLimits>(player, after);
}

ecs::components::InterfaceLimits LimitsOf(const ecs::Registry& registry, std::optional<entt::entity> player)
{
	if (!player.has_value() || !registry.Valid(*player))
	{
		return {};
	}
	const auto* limits = registry.TryGet<const ecs::components::InterfaceLimits>(*player);
	return limits != nullptr ? *limits : ecs::components::InterfaceLimits {};
}

} // namespace openblack::script::interface_levels
