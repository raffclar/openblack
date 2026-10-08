/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WorshipPercentage.h"

#include <cmath>

#include <algorithm>
#include <vector>

#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "Audio/Audio.h"
#include "Common/GUtilsDistance.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TotemStatue.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Life.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerWorship.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "TownMagic.h"
#include "WorshipSite.h"
#include "WorshipTrace.h"

using namespace openblack;
using namespace openblack::worship;
using namespace openblack::ecs::components;

namespace
{
/// The totem statue's two samples of the in-game bank: the rising loop and the stop
constexpr int k_TotemRisingSample = 0xB;
constexpr int k_TotemStopSample = 0x1E;

auto& Registry()
{
	return Locator::entitiesRegistry::value();
}

TownMagic* MagicOf(entt::entity town)
{
	if (town == entt::null || !Registry().Valid(town))
	{
		return nullptr;
	}
	return Registry().TryGet<TownMagic>(town);
}

/// The statue of the town's centre
entt::entity TotemOf(entt::entity town)
{
	const auto centre = town::TownCentreOf(town);
	if (centre == entt::null)
	{
		return entt::null;
	}
	entt::entity found = entt::null;
	Registry().Each<const TotemStatue>([&](entt::entity entity, const TotemStatue& statue) {
		if (statue.townCentre == centre)
		{
			found = entity;
		}
	});
	return found;
}

/// The statue's rise Zoomer moves in |change| x 5200 ms, and the Zoomer runs in milliseconds (the statue's draw
/// updates it with ms): the time is given in ms, not in seconds, which matters to the threshold 0.001 and to
/// Inverse's clamp (T < 0.0493 ms, not s). The original inlines Zoomer::SetDestinationWithSpeedAndTime here,
/// threshold included
void SetTotemPercentage(entt::entity totem, float percentage)
{
	auto& registry = Registry();
	if (registry.TryGet<TotemWorship>(totem) == nullptr)
	{
		// the statue's draw has been storing the last time every frame since the creation (the constructor's 0 is
		// never the one the first advance sees)
		registry.Assign<TotemWorship>(totem).lastMs = game_clock::EngineMs();
	}
	auto& worship = registry.Get<TotemWorship>(totem);
	const float milliseconds = std::abs(worship.percentage - percentage) * 5200.0f;
	worship.percentage = percentage;
	worship.rise.SetDestinationWithSpeedAndTime(percentage, 0.0f, milliseconds);
	// both branches: rising, the stop sound's loop released, the rising loop tagged (mode 2, 3D, in-game bank)
	worship.rising = true;
	audio::ReleaseLoop(audio::Owner::Thing(totem), k_TotemStopSample, audio::SfxBank::InGame);
	audio::tags::Create(totem, k_TotemRisingSample, false, 2, 0, false, true, audio::SfxBank::InGame, 0);
}

/// The Zoomer advance inlined in the statue's draw and process (one float step at a time; the value's sum in their
/// order, not Zoomer::Update's): dt the engine ms since the last time (the unsigned difference), then the last time
/// is now
void AdvanceRise(TotemWorship& worship)
{
	const int32_t now = game_clock::EngineMs();
	const auto dt = static_cast<float>(static_cast<uint32_t>(now - worship.lastMs));
	auto& z = worship.rise;
	const float t = dt + z.time;
	z.time = t;
	if (!(t < z.duration))
	{
		z.value = z.destination;
		z.speed = z.destinationSpeed;
		z.time = z.duration; // the original also clears a second time field, not kept by openblack's Zoomer
	}
	else
	{
		const float a = (t * t) * 0.5f;
		const float b = (t * a) * 0.33333334f;
		z.speed = ((t * z.c2 + a * z.c3) + b * z.c4) + z.startSpeed;
		const float c = (a * a) * 0.16666667f;
		z.value = (((c * z.c4 + z.startSpeed * t) + b * z.c3) + a * z.c2) + z.startValue;
	}
	worship.lastMs = now;
}

std::vector<entt::entity> VillagersOf(entt::entity town)
{
	std::vector<entt::entity> villagers;
	Registry().Each<const Villager>([&](entt::entity entity, const Villager& villager) {
		if (villager.town == town)
		{
			villagers.push_back(entity);
		}
	});
	return villagers;
}
} // namespace

void percentage::SetWorshipPercentage(entt::entity town, float percentage)
{
	auto* magic = MagicOf(town);
	if (magic == nullptr)
	{
		return;
	}
	if (magic->worshipSite == entt::null)
	{
		magic->worshipPercentage = 0.0f;
		return;
	}
	magic->worshipPercentage = percentage;
	if (const auto totem = TotemOf(town); totem != entt::null)
	{
		SetTotemPercentage(totem, percentage);
	}
	const int need = GetWorshipersNeeded(town, true, true, nullptr);
	if (trace::Enabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Worship trace: town {} worship {:.2f}, {} villagers needed",
		                   Registry().Get<const Town>(town).id, percentage, need);
	}
	if (need != 0)
	{
		AdjustWorshipersWorshipping(town, need, true, false);
	}
}

float percentage::GetWorshipPercentage(entt::entity town)
{
	const auto* magic = MagicOf(town);
	return magic != nullptr ? magic->worshipPercentage : 0.0f;
}

int percentage::GetWorshipersNeeded(entt::entity town, bool countOnWay, bool countGoHome, bool* out)
{
	const auto* magic = MagicOf(town);
	if (magic == nullptr)
	{
		return 0;
	}
	const auto population = static_cast<float>(town::Population(town));
	const int current = magic->worshipping + (countOnWay ? magic->onWayToWorship : 0);
	int requests = 0;
	if (countGoHome && magic->worshipSite != entt::null && Registry().Valid(magic->worshipSite))
	{
		requests = site::VillagersRequestingToGoHome(Registry().Get<const WorshipSite>(magic->worshipSite));
	}
	int target = 0;
	if (magic->worshipPercentage > 0.0f)
	{
		target = std::max(1, static_cast<int>(population * magic->worshipPercentage + 0.5f));
	}
	const int result = target - current + requests;
	if (out != nullptr)
	{
		*out = result > 0 && current >= target;
	}
	return result;
}

void percentage::AdjustWorshipersWorshipping(entt::entity town, int count, bool skipLifeCheck, bool requireReachable)
{
	const auto& registry = Registry();
	for (int pass = 0; pass < 2 && count != 0; ++pass)
	{
		// the villagers of the town's abodes, then the homeless: openblack keeps them by town
		const auto villagers = VillagersOf(town);
		if (count > 0)
		{
			std::vector<std::pair<float, entt::entity>> candidates;
			for (const auto villager : villagers)
			{
				if (ecs::villager_worship::IsAvailableForWorshipSite(villager, pass != 0))
				{
					candidates.emplace_back(WorshipScore(villager), villager);
				}
			}
			if (trace::Enabled())
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Worship trace: pass {}, {} of {} villagers available", pass,
				                   candidates.size(), villagers.size());
			}
			// the highest score (the nearest) first: a new one goes before the first whose score is lower
			std::stable_sort(candidates.begin(), candidates.end(),
			                 [](const auto& a, const auto& b) { return a.first > b.first; });
			// the threshold of each villager's own GVillagerInfo; (approximate): openblack keeps no info per villager
			// and takes info 0 (every villager info has 0.3 today)
			const float threshold = Locator::infoConstants::value().villager.at(0).damageThresholdToGoHome;
			for (const auto& [score, villager] : candidates)
			{
				if (count == 0)
				{
					break;
				}
				if (!registry.Valid(villager))
				{
					continue;
				}
				if (!skipLifeCheck && !(ecs::life::LifeOf(villager) > threshold))
				{
					continue;
				}
				if (ecs::villager_worship::CheckWorshipActivity(villager, requireReachable))
				{
					--count;
				}
			}
		}
		else
		{
			std::vector<std::pair<float, entt::entity>> candidates;
			for (const auto villager : villagers)
			{
				if (ecs::villager_worship::IsAtOrOnTheWayToWorshipSite(villager))
				{
					candidates.emplace_back(WorshipScore(villager), villager);
				}
			}
			// the lowest score (the farthest) first: a new one goes before the first whose score is higher
			std::stable_sort(candidates.begin(), candidates.end(),
			                 [](const auto& a, const auto& b) { return a.first < b.first; });
			for (const auto& [score, villager] : candidates)
			{
				if (count == 0)
				{
					break;
				}
				// state 163, then ++n only when a check on the villager passes. (approximate): that check is not
				// identified yet, so every villager sent back counts
				ecs::villager_worship::SendBackToTown(villager);
				++count;
			}
		}
	}
}

void percentage::AddVillagerOnWay(entt::entity town, entt::entity villager)
{
	auto* magic = MagicOf(town);
	if (magic == nullptr || std::ranges::find(magic->onWayVillagers, villager) != magic->onWayVillagers.end())
	{
		return;
	}
	magic->onWayVillagers.insert(magic->onWayVillagers.begin(), villager);
	++magic->onWayToWorship;
}

void percentage::RemoveVillagerOnWay(entt::entity town, entt::entity villager)
{
	auto* magic = MagicOf(town);
	if (magic == nullptr)
	{
		return;
	}
	auto& list = magic->onWayVillagers;
	const auto it = std::ranges::find(list, villager);
	if (it == list.end())
	{
		return;
	}
	list.erase(it);
	--magic->onWayToWorship;
}

void percentage::AddWorshipper(entt::entity town)
{
	if (auto* magic = MagicOf(town); magic != nullptr)
	{
		++magic->worshipping;
	}
}

void percentage::RemoveWorshipper(entt::entity town)
{
	if (auto* magic = MagicOf(town); magic != nullptr)
	{
		--magic->worshipping;
	}
}

float percentage::WorshipScore(entt::entity villager)
{
	const auto& registry = Registry();
	const auto* component = registry.TryGet<const Villager>(villager);
	if (component == nullptr)
	{
		return 0.0f;
	}
	const auto* magic = MagicOf(component->town);
	if (magic == nullptr || magic->worshipSite == entt::null || !registry.Valid(magic->worshipSite))
	{
		return 0.0f;
	}
	// the site's centre
	const auto centre = ecs::object::WorshipSiteCentre(magic->worshipSite);
	const auto flat = [](const glm::vec3& p) { return glm::vec2(p.x, p.z); };
	// two distances in metres, the second + 100
	const float toVillager = gutils::GetDistanceInMetres(flat(registry.Get<const Transform>(villager).position), flat(centre));
	const float toTown =
	    gutils::GetDistanceInMetres(flat(centre), flat(registry.Get<const Transform>(component->town).position)) + 100.0f;
	const float life = ecs::life::LifeOf(villager);
	// GetDistanceModifier(toVillager, toTown) = SigmoidThreshold(0.5, 1 - min / toTown): the threshold is the FIRST
	// argument, so the modifier FALLS with the distance (0.99996 at the centre, 3.6e-5 at toTown and beyond).
	// Then life^3, not life^2: the original multiplies the life in twice more, and by the modifier last
	return life * life * life * gutils::GetDistanceModifier(toVillager, toTown);
}

void percentage::UpdateTotems()
{
	auto& registry = Registry();
	bool changed = false;
	registry.Each<TotemStatue, TotemWorship, Transform>(
	    [&](entt::entity, TotemStatue& statue, TotemWorship& worship, Transform& plinth) {
		    // the engine ms since the last time and the inline advance
		    AdvanceRise(worship);
		    const float rise = TotemStatue::k_WorshipRise * worship.rise.value;
		    if (rise == statue.rise)
		    {
			    return;
		    }
		    statue.rise = rise;
		    plinth.position.y = statue.baseY + rise;
		    if (statue.top != entt::null && registry.Valid(statue.top))
		    {
			    registry.Get<Transform>(statue.top).position.y = plinth.position.y + TotemStatue::k_PlinthTop;
		    }
		    changed = true;
	    });
	if (changed)
	{
		registry.SetDirty();
	}
}

void percentage::ProcessTotem(entt::entity statue)
{
	auto& registry = Registry();
	auto* worship = statue != entt::null && registry.Valid(statue) ? registry.TryGet<TotemWorship>(statue) : nullptr;
	if (worship == nullptr)
	{
		// (openblack) no worship percentage was ever set on it: its Zoomer is still at rest
		return;
	}
	AdvanceRise(*worship);
	// |value - destination| < 0.005 (compared as a double) while rising
	if (!(std::abs(static_cast<double>(worship->rise.value - worship->rise.destination)) < 0.004999999888241291) ||
	    !worship->rising)
	{
		return;
	}
	// not rising any more, the rising loop's tag removed (stopped), the stop sound at its point. (pending) two of the
	// options (1 and 0) are taken as track and extra3DFlag
	worship->rising = false;
	audio::tags::Remove(statue, k_TotemRisingSample, audio::SfxBank::InGame, true);
	audio::PlayOptions options;
	options.sample = {audio::Bank(audio::SfxBank::InGame), k_TotemStopSample};
	options.owner = audio::Owner::Thing(statue);
	options.is3D = true;
	options.track = true;
	options.extra3DFlag = false;
	options.position = map_coords::ToWorld(ecs::object::MapCoordsOf(statue));
	audio::PlaySoundEffect(options);
}

entt::entity percentage::TotemTown(entt::entity statue)
{
	auto& registry = Registry();
	if (statue == entt::null || !registry.Valid(statue))
	{
		return entt::null;
	}
	const auto* totem = registry.TryGet<const TotemStatue>(statue);
	if (totem == nullptr)
	{
		// the icon on the plinth drags the same statue
		registry.Each<const TotemStatue>([&](entt::entity entity, const TotemStatue& s) {
			if (s.top == statue)
			{
				totem = &s;
				statue = entity;
			}
		});
	}
	if (totem == nullptr || totem->townCentre == entt::null)
	{
		return entt::null;
	}
	entt::entity found = entt::null;
	registry.Each<const TownMagic>([&](entt::entity town, const TownMagic& magic) {
		if (found == entt::null && town::TownCentreOf(town) == totem->townCentre && magic.worshipSite != entt::null)
		{
			found = town;
		}
	});
	return found;
}
