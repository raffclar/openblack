/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DebugThingActs.h"

#include <cmath>

#include <algorithm>
#include <numbers>

#include "Common/PortableMath.h"
#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "Particles/ParticleDrawPath.h"
#include "Particles/ParticleTypes.h"

namespace openblack::debug_thing_acts
{

using player_commands::DebugParticle;
using player_commands::DebugPhysics;
using player_commands::DebugTempleVillager;
using player_commands::DebugVillagerPath;

static_assert(k_ParticleTypes == particles::k_ParticleTypeCount, "The acts take each particle type");
static_assert(k_DrawPaths == static_cast<uint8_t>(particles::draw::DrawPath::_Count), "The acts take each way to draw");
static_assert(k_FirstLocalEffect == ecs::systems::ParticleRuleSystemInterface::k_FirstLocalEffect,
              "The acts know the effects of this computer");
static_assert(k_FirstSimulationEffect == ecs::systems::k_FirstEffectHandle, "The acts know the effects of the simulation");

namespace
{

[[nodiscard]] bool Finite(float value) noexcept
{
	return std::isfinite(value);
}

[[nodiscard]] bool Finite(const glm::vec3& value) noexcept
{
	return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] bool Finite(const glm::vec2& value) noexcept
{
	return std::isfinite(value.x) && std::isfinite(value.y);
}

} // namespace

std::string_view NameOf(DebugParticle::Act act) noexcept
{
	switch (act)
	{
	case DebugParticle::Act::Start:
		return "start";
	case DebugParticle::Act::Pause:
		return "pause";
	case DebugParticle::Act::CloseAll:
		return "close_all";
	case DebugParticle::Act::DeleteAll:
		return "delete_all";
	case DebugParticle::Act::Close:
		return "close";
	case DebugParticle::Act::Delete:
		return "delete";
	}
	return "unknown";
}

std::string_view NameOf(DebugTempleVillager::Act act) noexcept
{
	switch (act)
	{
	case DebugTempleVillager::Act::Share:
		return "share";
	case DebugTempleVillager::Act::SendOne:
		return "send_one";
	case DebugTempleVillager::Act::CallOneBack:
		return "call_one_back";
	case DebugTempleVillager::Act::Make:
		return "make";
	case DebugTempleVillager::Act::SendToWorship:
		return "send_to_worship";
	case DebugTempleVillager::Act::TakeAway:
		return "take_away";
	}
	return "unknown";
}

std::string_view NameOf(DebugVillagerPath::Act act) noexcept
{
	switch (act)
	{
	case DebugVillagerPath::Act::Teleport:
		return "teleport";
	case DebugVillagerPath::Act::MoveTo:
		return "move_to";
	case DebugVillagerPath::Act::Goal:
		return "goal";
	case DebugVillagerPath::Act::Speed:
		return "speed";
	}
	return "unknown";
}

std::optional<std::string_view> PhysicsRefusal(const DebugPhysics& act) noexcept
{
	if (act.type >= static_cast<uint16_t>(MobileStaticInfo::_COUNT))
	{
		return "the type is not a type of a static thing";
	}
	if (!Finite(act.height) || !Finite(act.scale) || !Finite(act.place) || !Finite(act.velocity))
	{
		return "a value is not a number";
	}
	if (act.scale <= 0.0f)
	{
		return "the scale is not more than 0";
	}
	return std::nullopt;
}

std::optional<std::string_view> ParticleRefusal(const DebugParticle& act) noexcept
{
	switch (act.act)
	{
	case DebugParticle::Act::Start:
		if (act.file.size() > player_commands::k_MostParticleFileBytes)
		{
			return "the name of the file is too long";
		}
		if (!std::ranges::all_of(act.file, [](char character) { return character > ' ' && character <= '~'; }))
		{
			return "the name of the file has a character that is not a letter, a digit or a sign";
		}
		if (act.file.empty() && act.type >= k_ParticleTypes)
		{
			return "the type is not a particle type";
		}
		if (!Finite(act.position) || !Finite(act.magnitude))
		{
			return "a value is not a number";
		}
		if (act.magnitude < 0.0f || act.magnitude > k_MostMagnitude)
		{
			return "the magnitude is not in the range of the window";
		}
		if (act.player >= k_EffectPlayers || act.drawPath >= k_DrawPaths || act.targets > k_MostTargets)
		{
			return "the player, the draw or the count of the targets is not in the range of the window";
		}
		return std::nullopt;
	case DebugParticle::Act::Close:
	case DebugParticle::Act::Delete:
		if (!IsSharedEffect(act.effect))
		{
			return "the effect is not an effect of each computer";
		}
		return std::nullopt;
	case DebugParticle::Act::Pause:
	case DebugParticle::Act::CloseAll:
	case DebugParticle::Act::DeleteAll:
		return std::nullopt;
	}
	return "the act is not an act of the window";
}

std::optional<std::string_view> TempleVillagerRefusal(const DebugTempleVillager& act) noexcept
{
	switch (act.act)
	{
	case DebugTempleVillager::Act::Share:
		if (!Finite(act.value) || act.value < 0.0f || act.value > 1.0f)
		{
			return "the share is not from 0 to 1";
		}
		return std::nullopt;
	case DebugTempleVillager::Act::Make:
		if (act.tribe >= static_cast<uint8_t>(Tribe::_COUNT) || act.number >= static_cast<uint8_t>(VillagerNumber::_COUNT))
		{
			return "the tribe or the job is not a tribe or a job of a villager";
		}
		if (act.age < k_YoungestAge || act.age > k_OldestAge || act.count < 1 || act.count > k_MostMade)
		{
			return "the age or the count is not in the range of the window";
		}
		if (!Finite(act.home))
		{
			return "a value is not a number";
		}
		return std::nullopt;
	case DebugTempleVillager::Act::TakeAway:
		if (act.villagers.empty() || act.villagers.size() > player_commands::k_MostTakenVillagers)
		{
			return "the count of the villagers is not in the range of the command";
		}
		return std::nullopt;
	case DebugTempleVillager::Act::SendOne:
	case DebugTempleVillager::Act::CallOneBack:
	case DebugTempleVillager::Act::SendToWorship:
		return std::nullopt;
	}
	return "the act is not an act of the window";
}

std::optional<std::string_view> VillagerPathRefusal(const DebugVillagerPath& act) noexcept
{
	if (!Finite(act.position) || !Finite(act.speed))
	{
		return "a value is not a number";
	}
	return std::nullopt;
}

std::optional<std::string_view> RefusalOf(const player_commands::Action& action) noexcept
{
	if (const auto* physics = std::get_if<DebugPhysics>(&action))
	{
		return PhysicsRefusal(*physics);
	}
	if (const auto* particle = std::get_if<DebugParticle>(&action))
	{
		return ParticleRefusal(*particle);
	}
	if (const auto* temple = std::get_if<DebugTempleVillager>(&action))
	{
		return TempleVillagerRefusal(*temple);
	}
	if (const auto* path = std::get_if<DebugVillagerPath>(&action))
	{
		return VillagerPathRefusal(*path);
	}
	return std::nullopt;
}

std::vector<glm::vec2> MadePlaces(glm::vec2 home, uint8_t count)
{
	std::vector<glm::vec2> places;
	places.reserve(count);
	for (uint8_t i = 0; i < count; ++i)
	{
		// The portable sine and cosine give the same places on each computer
		const float angle = 2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(count);
		places.push_back(home + k_MadeRing * glm::vec2(portable_math::Cos(angle), portable_math::Sin(angle)));
	}
	return places;
}

ActResult Apply(ThingTarget& target, const DebugPhysics& act, PlayerNames player)
{
	if (PhysicsRefusal(act).has_value())
	{
		return {};
	}
	return {.done = target.Throw(act, player)};
}

ActResult Apply(ThingTarget& target, const DebugParticle& act)
{
	if (ParticleRefusal(act).has_value())
	{
		return {};
	}
	switch (act.act)
	{
	case DebugParticle::Act::Start:
		return {.done = target.StartEffect(act) != 0};
	case DebugParticle::Act::Pause:
		target.SetEffectsPaused(act.paused);
		break;
	case DebugParticle::Act::CloseAll:
		target.CloseAllEffects();
		break;
	case DebugParticle::Act::DeleteAll:
		target.DeleteAllEffects();
		break;
	case DebugParticle::Act::Close:
		target.CloseEffect(act.effect);
		break;
	case DebugParticle::Act::Delete:
		target.DeleteEffect(act.effect);
		break;
	}
	return {.done = true};
}

ActResult Apply(ThingTarget& target, const DebugTempleVillager& act)
{
	if (TempleVillagerRefusal(act).has_value())
	{
		return {};
	}
	switch (act.act)
	{
	case DebugTempleVillager::Act::Share:
		return {.done = target.SetTownShare(act.town, act.value)};
	case DebugTempleVillager::Act::SendOne:
		return {.done = target.AdjustWorshippers(act.town, 1)};
	case DebugTempleVillager::Act::CallOneBack:
		return {.done = target.AdjustWorshippers(act.town, -1)};
	case DebugTempleVillager::Act::Make:
	{
		ActResult result;
		for (const auto place : MadePlaces(glm::vec2(act.home.x, act.home.z), act.count))
		{
			if (const auto villager = target.MakeVillager(act, place))
			{
				result.made.push_back(*villager);
			}
		}
		// The new villagers go to worship after each of them is in the town
		if (act.send)
		{
			for (const auto villager : result.made)
			{
				target.SendToWorship(villager);
			}
		}
		result.done = !result.made.empty();
		return result;
	}
	case DebugTempleVillager::Act::SendToWorship:
		return {.done = target.SendToWorship(act.villager)};
	case DebugTempleVillager::Act::TakeAway:
	{
		bool done = false;
		for (const auto villager : act.villagers)
		{
			done = target.TakeAway(villager) || done;
		}
		return {.done = done};
	}
	}
	return {};
}

ActResult Apply(ThingTarget& target, const DebugVillagerPath& act)
{
	if (VillagerPathRefusal(act).has_value())
	{
		return {};
	}
	return {.done = target.MoveVillager(act)};
}

std::vector<DebugTempleVillager> TakeAwayActs(std::vector<entt::entity> villagers)
{
	std::vector<DebugTempleVillager> acts;
	for (size_t first = 0; first < villagers.size(); first += player_commands::k_MostTakenVillagers)
	{
		const auto last = std::min(villagers.size(), first + player_commands::k_MostTakenVillagers);
		acts.push_back({.act = DebugTempleVillager::Act::TakeAway,
		                .villagers = std::vector<entt::entity>(villagers.begin() + static_cast<std::ptrdiff_t>(first),
		                                                       villagers.begin() + static_cast<std::ptrdiff_t>(last))});
	}
	return acts;
}

} // namespace openblack::debug_thing_acts
