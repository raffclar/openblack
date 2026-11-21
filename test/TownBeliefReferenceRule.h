/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// A copy of the rule of the symbols over the village centres before the rule became faster. The tests give the same
// villages to this copy and to the rule of the game, and compare each value of each symbol.

#include <cmath>

#include <algorithm>
#include <any>
#include <functional>
#include <numbers>
#include <numeric>
#include <span>
#include <vector>

#include <ParticleFile.h>

#include "Common/ExactRemainder.h"
#include "Common/PortableMath.h"
#include "Common/X87Vector.h"
#include "Particles/ParticleArchive.h"
#include "Particles/ParticleCreators.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticlePrefetch.h"
#include "Particles/TownBeliefMaths.h"

namespace openblack::particles::town_belief_reference
{
using openblack::psys::ParticleObject;
using town_belief::k_TiltShare;
using town_belief::Look;
using town_belief::Params;
using town_belief::Rank;
using town_belief::Shown;
using town_belief::Symbol;
namespace x87 = openblack::x87;

constexpr float k_TwoPi = 2.0f * std::numbers::pi_v<float>;
/// A fight's next wait and length are its usual ones times a random share between these
constexpr float k_RandomShareMin = 0.5f;
constexpr float k_RandomShareMax = 1.5f;

inline float Lerp(float at0, float at1, float belief)
{
	return (at1 - at0) * belief + at0;
}

/// Rises from 0 to 1 halfway through and falls back to 0 at the end
inline float Swell(float t)
{
	const float centred = std::clamp(t, 0.0f, 1.0f) * 2.0f - 1.0f;
	return 1.0f - centred * centred;
}

inline float Wrapped(float angle)
{
	return exact_remainder::Fmod(angle, k_TwoPi);
}

inline std::vector<int> RanksBefore(std::span<const float> beliefs)
{
	std::vector<int> ranks(beliefs.size(), 0);
	std::vector<float> shown(beliefs.size());
	std::ranges::transform(beliefs, shown.begin(), Shown);
	// A belief that is not a number does not order with the others, so each rank is then found alone
	if (std::ranges::any_of(shown, [](float belief) { return std::isnan(belief); }))
	{
		for (size_t player = 0; player < beliefs.size(); ++player)
		{
			ranks[player] = Rank(beliefs, player);
		}
		return ranks;
	}
	// The most believed first, and of equal beliefs the later player first: a player's place in this order is the
	// count of players that rank above it
	std::vector<size_t> order(beliefs.size());
	std::iota(order.begin(), order.end(), size_t {0});
	std::ranges::sort(order, [&shown](size_t a, size_t b) { return shown[a] > shown[b] || (shown[a] == shown[b] && a > b); });
	for (size_t place = 0; place < order.size(); ++place)
	{
		ranks[order[place]] = static_cast<int>(place);
	}
	return ranks;
}

inline Look StepBefore(Symbol& symbol, const Params& params, float belief, int rank, float dt, float distanceScale,
                       const std::function<float(float, float)>& random)
{
	const float shown = Shown(belief);
	Look look;
	look.scale = shown == 0.0f ? 0.0f : Lerp(params.scaleAt0, params.scaleAt1, shown);
	// The most believed sits still over the totem; the others circle at a speed across the ground
	float radius = 0.0f;
	float speed = 0.0f;
	if (rank != 0)
	{
		radius = Lerp(params.radiusAt0, params.radiusAt1, shown);
		speed = Lerp(params.speedAt0, params.speedAt1, shown) / radius;
		// The second most believed fights the first now and then
		if (rank == 1)
		{
			if (symbol.untilFight > 0.0f)
			{
				symbol.untilFight -= dt;
			}
			else
			{
				symbol.intoFight += dt;
				if (symbol.intoFight > symbol.fightLength)
				{
					symbol.untilFight = random(k_RandomShareMin, k_RandomShareMax) *
					                    Lerp(params.timeBetweenFightsAt0, params.timeBetweenFightsAt1, shown);
					symbol.intoFight = 0.0f;
					symbol.fightLength = random(k_RandomShareMin, k_RandomShareMax) *
					                     Lerp(params.fightDurationAt0, params.fightDurationAt1, shown);
				}
				look.fight = Swell(symbol.intoFight / symbol.fightLength);
			}
			speed *= (params.speedUpDuringFight - 1.0f) * look.fight + 1.0f;
		}
	}
	const float level = static_cast<float>(rank) * params.heightPerLevel;
	symbol.sway = Wrapped(symbol.sway + dt * params.angleSpeedPhaseSpeed);
	const float swayed = (portable_math::Cos(symbol.sway) + 1.0f) * 0.25f + 0.5f;
	symbol.tilt = Wrapped(symbol.tilt + swayed * speed * dt * k_TiltShare);
	symbol.around = Wrapped(symbol.around + speed * dt);
	const float flat = portable_math::Cos(symbol.tilt) * radius;
	// Fighting, it comes down to the first's level
	look.offset = glm::vec3(portable_math::Cos(symbol.around) * flat,
	                        (1.0f - look.fight) * level + portable_math::Sin(symbol.tilt) * radius,
	                        portable_math::Sin(symbol.around) * flat) *
	              distanceScale;
	return look;
}

/// A village's symbols are drawn this many times their size before the belief scales them
constexpr float k_SymbolBaseScale = 1.5f;

inline town_belief::Params ReadParams(const ParticleObject& object)
{
	const town_belief::Params defaults;
	return {
	    .radiusAt0 = object.Float("RadiusAt0", defaults.radiusAt0),
	    .radiusAt1 = object.Float("RadiusAt1", defaults.radiusAt1),
	    .speedAt0 = object.Float("SpeedAt0", defaults.speedAt0),
	    .speedAt1 = object.Float("SpeedAt1", defaults.speedAt1),
	    .scaleAt0 = object.Float("ScaleAt0", defaults.scaleAt0),
	    .scaleAt1 = object.Float("ScaleAt1", defaults.scaleAt1),
	    .heightAt1 = object.Float("HeightAt1", defaults.heightAt1),
	    .heightPerLevel = object.Float("HeightPerLevel", defaults.heightPerLevel),
	    .useFixedScale = object.Bool("UseFixedScale", defaults.useFixedScale),
	    .fixedScale = object.Float("FixedScale", defaults.fixedScale),
	    .timeBetweenFightsAt0 = object.Float("TimeBetweenFightsAt0", defaults.timeBetweenFightsAt0),
	    .timeBetweenFightsAt1 = object.Float("TimeBetweenFightsAt1", defaults.timeBetweenFightsAt1),
	    .fightDurationAt0 = object.Float("FightDurationAt0", defaults.fightDurationAt0),
	    .fightDurationAt1 = object.Float("FightDurationAt1", defaults.fightDurationAt1),
	    .speedUpDuringFight = object.Float("SpeedUpDuringFight", defaults.speedUpDuringFight),
	    .fightFracWhenEmitting = object.Float("FightFracWhenEmitting", defaults.fightFracWhenEmitting),
	    .angleSpeedPhaseSpeed = object.Float("AngleSpeedPhaseSpeed", defaults.angleSpeedPhaseSpeed),
	};
}

class TownCentreBeliefRule final: public Modifier
{
public:
	void SaveSlotData(const Collection::Slot& slot, EffectWriter& out) const override
	{
		out.Write(std::any_cast<entt::entity>(slot.data));
	}
	void LoadSlotData(Collection::Slot& slot, EffectReader& in) const override
	{
		auto target = entt::entity {entt::null};
		in.Read(target);
		slot.data = target;
	}

	explicit TownCentreBeliefRule(const ParticleObject& object)
	    : params(ReadParams(object))
	    , nextGroups(object.IntArray("NextGroups"))
	    , sparkGroup(object.Int("GroupFightSparkle", -1))
	{
		symbol.kind = Creator::Kind::Symbol;
		symbol.className = "UR_TownCentreBelief";
	}

	[[nodiscard]] bool Creates() const override { return true; }
	/// The symbols keep their creator through a save and a load, so that the load draws them again
	[[nodiscard]] const Creator* OwnCreator() const override { return &symbol; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& slot) const override
	{
		auto& world = effect.Services().world;
		if (!slot.data.has_value())
		{
			const auto centre = effect.TakeTarget();
			if (!centre.has_value())
			{
				return true;
			}
			slot.data = *centre;
			for (const auto player : world.PlayersInGame())
			{
				auto& atom = effect.NewAtom(collection, &symbol, nextGroups);
				atom.baseScale = k_SymbolBaseScale;
				atom.player = player;
				auto& data = atom.data[this];
				// Each starts at a random tilt and a random way round
				data.a.y = effect.Random(2.0f * std::numbers::pi_v<float>);
				data.a.x = effect.Random(2.0f * std::numbers::pi_v<float>);
			}
		}
		const auto belief = world.TownCentreBeliefOf(std::any_cast<entt::entity>(slot.data));
		if (!belief.has_value())
		{
			return true;
		}
		const glm::vec3 base = belief->totemTop + glm::vec3(0.0f, params.heightAt1, 0.0f);
		const float distanceScale =
		    town_belief::DistanceScale(params, x87::Distance<x87::SumOrder::Xzy>(world.CameraPosition(), base));
		const auto random = [&effect](float min, float max) { return effect.Random(min, max); };
		// The village's beliefs and the rank of each player are the same for each symbol
		const auto beliefs = belief->OfPlayers();
		const auto ranks = RanksBefore(beliefs);
		for (size_t a = 0; a < collection.atoms.size(); ++a)
		{
			prefetch::AtomsAhead(collection.atoms, a, 1, prefetch::Reach::Collections);
			auto& atom = collection.atoms[a];
			auto* const found = atom->data.find(this);
			if (found == atom->data.end() || atom->player < 0 || static_cast<size_t>(atom->player) >= beliefs.size())
			{
				continue;
			}
			auto& data = found->second;
			town_belief::Symbol state {.around = data.a.x,
			                           .tilt = data.a.y,
			                           .sway = data.a.z,
			                           .untilFight = data.b.x,
			                           .intoFight = data.b.y,
			                           .fightLength = data.b.z};
			const auto player = static_cast<size_t>(atom->player);
			const auto look = StepBefore(state, params, beliefs[player], ranks[player], effect.GetDt(), distanceScale, random);
			data.a = {state.around, state.tilt, state.sway, 0.0f};
			data.b = {state.untilFight, state.intoFight, state.fightLength, 0.0f};
			atom->position = base + look.offset;
			atom->ruleScale = look.scale;
			for (auto& sparks : atom->subCollections)
			{
				if (sparks->group == sparkGroup)
				{
					sparks->alpha = static_cast<float>(town_belief::SparkAlpha(look.fight));
					sparks->emitting = town_belief::Sparking(params, look.fight);
				}
			}
		}
		return true;
	}

private:
	town_belief::Params params;
	std::vector<int> nextGroups;
	int sparkGroup;
	SymbolCreator symbol;
};
} // namespace openblack::particles::town_belief_reference
