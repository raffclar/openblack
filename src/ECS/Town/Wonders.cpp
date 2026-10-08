/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Wonders.h"

#include <cstddef>

#include <array>
#include <optional>

#include "Debug/StateHash.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Wonder.h"
#include "ECS/Registry.h"
#include "ECS/Town/AbodeVillagers.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Players.h"

// A wonder's power and the player's per-tribe sum (Wonders.h)

namespace openblack::ecs
{
using namespace components;

namespace
{
constexpr size_t k_Players = static_cast<size_t>(PlayerNames::_COUNT);
constexpr size_t k_Tribes = static_cast<size_t>(Tribe::_COUNT);

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

Wonder* WonderComponent(entt::entity wonder)
{
	auto& registry = Entities();
	return wonder != entt::null && registry.Valid(wonder) ? registry.TryGet<Wonder>(wonder) : nullptr;
}

/// The player's power slot for the wonder's tribe, for AddToPlayer / RemoveFromPlayer, only when built. (inferred) an
/// abode's player is its town's (Town::owner): none without a town. The tribe is never missing for a wonder with an
/// info record
float* PowerSlot(entt::entity wonder)
{
	auto& registry = Entities();
	const auto town = abode_villagers::TownOf(wonder);
	const auto* t = town != entt::null ? registry.TryGet<const Town>(town) : nullptr;
	if (t == nullptr)
	{
		return nullptr;
	}
	const auto* info = abodes::InfoOf(wonder);
	if (info == nullptr)
	{
		return nullptr;
	}
	const auto player = static_cast<size_t>(t->owner);
	const auto tribe = static_cast<size_t>(static_cast<int32_t>(info->tribeType));
	if (player >= k_Players || tribe >= k_Tribes || !abodes::IsBuilt(wonder))
	{
		return nullptr;
	}
	// The player's wonder power for that tribe (one float per tribe)
	return &magic::players::MagicOf(t->owner).wonderPower.at(tribe);
}
} // namespace

bool wonders::IsWonder(entt::entity entity)
{
	return WonderComponent(entity) != nullptr;
}

void wonders::Create(entt::entity wonder, float scale)
{
	auto& registry = Entities();
	if (wonder == entt::null || !registry.Valid(wonder))
	{
		return;
	}
	// The wonder part on top of the abode, zeroed
	registry.AssignOrReplace<Wonder>(wonder);
	// SetPower with Create's fifth argument. (inferred) the scale: Create(pos, info, town, yAngle, scale, ...) as a
	// workshop's creation takes its arguments
	SetPower(wonder, scale);
	// The creation hooks: the abode's (AbodeArchetype), then two calls on the 3D object ((pending) what they do)
	// Already built -> AddToPlayer
	if (abodes::IsBuilt(wonder))
	{
		AddToPlayer(wonder);
	}
}

void wonders::SetPower(entt::entity wonder, float power)
{
	if (auto* w = WonderComponent(wonder); w != nullptr)
	{
		w->power = power;
	}
}

float wonders::GetPower(entt::entity wonder)
{
	const auto* w = WonderComponent(wonder);
	return w != nullptr ? w->power : 0.0f;
}

void wonders::AddToPlayer(entt::entity wonder)
{
	const auto* w = WonderComponent(wonder);
	if (w == nullptr)
	{
		return;
	}
	// slot = power + slot, as a float
	if (auto* slot = PowerSlot(wonder); slot != nullptr)
	{
		*slot = w->power + *slot;
	}
}

void wonders::RemoveFromPlayer(entt::entity wonder)
{
	const auto* w = WonderComponent(wonder);
	if (w == nullptr)
	{
		return;
	}
	// slot = slot - power, as a float
	if (auto* slot = PowerSlot(wonder); slot != nullptr)
	{
		*slot = *slot - w->power;
	}
}

void wonders::Built(entt::entity wonder)
{
	// After the abode's Built (abodes::Built, which calls this at its end): AddToPlayer; the abode's result is
	// returned
	AddToPlayer(wonder);
}

void wonders::DeleteDependants(entt::entity wonder)
{
	// RemoveFromPlayer, then the abode's DeleteDependants (the caller's)
	RemoveFromPlayer(wonder);
}

float wonders::PlayerWonderPower(PlayerNames player, Tribe tribe)
{
	const auto p = static_cast<size_t>(player);
	const auto t = static_cast<size_t>(static_cast<int32_t>(tribe));
	return p < k_Players && t < k_Tribes ? magic::players::MagicOf(player).wonderPower.at(t) : 0.0f;
}

void wonders::RegisterStateHash()
{
	state_hash::Register("wonder_power", [](state_hash::Hasher& h) {
		for (size_t p = 0; p < k_Players; ++p)
		{
			for (const float power : magic::players::MagicOf(static_cast<PlayerNames>(p)).wonderPower)
			{
				h.Float(power);
			}
		}
	});
}
} // namespace openblack::ecs
