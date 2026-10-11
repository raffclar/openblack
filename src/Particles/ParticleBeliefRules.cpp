/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The symbols of belief that rise from the people a miracle impresses. Each symbol waiting is made an atom where it
// rises from, in the believed player's colour, the more belief the more opaque; one worth more than a little plays its
// sound. Each rises at its own speed, wiggling across the land more as it gets going, pulses in size, fades out after a
// second and a half and is gone soon after.
//
// Over each village centre's totem, a symbol of each player in the game shows how much the village believes in them
// (see town_belief).

#include <cmath>

#include <algorithm>
#include <any>
#include <numbers>
#include <string>
#include <vector>

#include <ParticleFile.h>

#include "ParticleClassRegistry.h"
#include "ParticleCreators.h"
#include "ParticleSounds.h"
#include "TownBeliefMaths.h"

using namespace openblack::particles;
using openblack::psys::ParticleObject;

namespace
{
/// Each symbol rises up to this much faster for each of its thirteen steps of its own number
constexpr float k_RiseStep = 0.0249f;
constexpr uint32_t k_RiseSteps = 13;
/// The symbol's own number makes its size pulse at its own pace
constexpr float k_PulsePhase = 0.23f;
constexpr float k_PulsePace = 0.3f / 256.0f;
/// A symbol is gone this long after it has faded out
constexpr float k_GoneAfterFading = 0.3f;

class BeliefSpriteRule final: public Modifier
{
public:
	explicit BeliefSpriteRule(const ParticleObject& object)
	    : creator(object.String("PCreator"))
	    , riseSpeed(object.Float("RiseSpeed", 12.0f))
	    , maxWiggle(object.Float("MaxWiggleOffset", 2.0f))
	    , ageAtMaxOffset(object.Float("AgeAtMaxOffset", 0.5f))
	    , noiseFrequency(object.Float("NoiseFreq", 2.0f))
	    , scaleMax(object.Float("ScaleMax", 1.0f))
	    , scaleMin(object.Float("ScaleMin", 0.0f))
	    , scaleFrequency(object.Float("ScaleFreq", 12.0f))
	    , fadeTimeStart(object.Float("FadeTimeStart", 1.5f))
	    , fadeTimeEnd(object.Float("FadeTimeEnd", 2.5f))
	    , alphaMin(object.Int("AlphaMin", 20))
	    , alphaMax(object.Int("AlphaMax", 150))
	    , valueAlphaMin(object.Int("ValueAlphaMin", 10))
	    , valueAlphaMax(object.Int("ValueAlphaMax", 100))
	    , sound({.action = object.Sound("SoundOfBelief")})
	{
	}

	[[nodiscard]] bool Creates() const override { return true; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		const auto* spriteCreator = effect.FindCreator(creator);
		if (spriteCreator == nullptr)
		{
			return true;
		}
		auto& world = effect.Services().world;
		for (auto sprite = world.TakeBeliefSprite(); sprite.has_value(); sprite = world.TakeBeliefSprite())
		{
			auto& atom = effect.NewAtom(collection, spriteCreator, {});
			auto& data = atom.data[this];
			data.a = glm::vec4(sprite->position, 0.0f);
			// The more belief, the more opaque it starts
			const float share = std::clamp(static_cast<float>(sprite->amount - valueAlphaMin) /
			                                   static_cast<float>(valueAlphaMax - valueAlphaMin),
			                               0.0f, 1.0f);
			data.b.x = std::round(static_cast<float>(alphaMax - alphaMin) * share + static_cast<float>(alphaMin));
			atom.position = sprite->position;
			atom.rgba = {static_cast<uint8_t>(sprite->colour >> 16u), static_cast<uint8_t>(sprite->colour >> 8u),
			             static_cast<uint8_t>(sprite->colour), atom.rgba[3]};
			atom.ruleScale = 1.0f;
			if (sprite->amount > valueAlphaMin)
			{
				StartAtomSound(effect, atom, sound);
			}
		}
		const auto& noise = effect.Services().noise;
		for (size_t i = 0; i < collection.atoms.size();)
		{
			auto& atom = *collection.atoms[i];
			const auto found = atom.data.find(this);
			if (found == atom.data.end())
			{
				++i;
				continue;
			}
			const auto& data = found->second;
			const float age = effect.AtomAge(atom);
			const auto id = static_cast<float>(atom.random);
			const float along = age * noiseFrequency;
			const float wiggleX = noise.Smooth(id + along * 2.0f) * 0.5f + noise.Smooth(id + along);
			const float wiggleZ = noise.Smooth(id * 0.3f + along * 2.0f) * 0.5f + noise.Smooth(id * 0.3f + along);
			const float ramp = std::clamp(age / ageAtMaxOffset, 0.0f, 1.0f);
			const float rise = (static_cast<float>(atom.random % k_RiseSteps) * k_RiseStep + 1.0f) * riseSpeed;
			const glm::vec3 start(data.a);
			atom.position = {start.x + ramp * maxWiggle * wiggleX, start.y + rise * age, start.z + ramp * maxWiggle * wiggleZ};
			// Faded out between the fade's start and end
			float fade = 0.0f;
			if (age > fadeTimeStart)
			{
				fade = std::clamp((age - fadeTimeStart) / (fadeTimeEnd - fadeTimeStart), 0.0f, 1.0f);
			}
			atom.rgba[3] = static_cast<uint8_t>(std::round(data.b.x - data.b.x * fade));
			const float pulse = std::sin(id * k_PulsePhase + (id * k_PulsePace + 1.0f) * age * scaleFrequency);
			atom.ruleScale = (pulse * 0.5f + 1.0f) * (scaleMax - scaleMin) + scaleMin;
			if (age > fadeTimeEnd + k_GoneAfterFading)
			{
				collection.atoms.erase(collection.atoms.begin() + static_cast<std::ptrdiff_t>(i));
				continue;
			}
			++i;
		}
		return true;
	}

private:
	std::string creator;
	float riseSpeed;
	float maxWiggle;
	float ageAtMaxOffset;
	float noiseFrequency;
	float scaleMax;
	float scaleMin;
	float scaleFrequency;
	float fadeTimeStart;
	float fadeTimeEnd;
	int alphaMin;
	int alphaMax;
	int valueAlphaMin;
	int valueAlphaMax;
	ParticleSound sound;
};
/// A village's symbols are drawn this many times their size before the belief scales them
constexpr float k_SymbolBaseScale = 1.5f;

town_belief::Params ReadParams(const ParticleObject& object)
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

/// The symbols over the totem of the village centre the effect is given as its target: made the first step it has the
/// centre, one for each player in the game, each placed every step by how much the village believes in its player. The
/// sparks under each symbol show while it fights.
class TownCentreBeliefRule final: public Modifier
{
public:
	explicit TownCentreBeliefRule(const ParticleObject& object)
	    : params(ReadParams(object))
	    , nextGroups(object.IntArray("NextGroups"))
	    , sparkGroup(object.Int("GroupFightSparkle", -1))
	{
		symbol.kind = Creator::Kind::Symbol;
		symbol.className = "UR_TownCentreBelief";
	}

	[[nodiscard]] bool Creates() const override { return true; }

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
		const float distanceScale = town_belief::DistanceScale(params, glm::distance(world.CameraPosition(), base));
		const auto random = [&effect](float min, float max) { return effect.Random(min, max); };
		for (auto& atom : collection.atoms)
		{
			const auto found = atom->data.find(this);
			if (found == atom->data.end() || atom->player < 0)
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
			const auto look =
			    town_belief::Step(state, params, belief->beliefs.at(player), town_belief::Rank(belief->beliefs, player),
			                      effect.GetDt(), distanceScale, random);
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
} // namespace

void openblack::particles::RegisterBeliefRules(ParticleClassRegistry& registry)
{
	registry.AddModifier("UR_BeliefSprite", ParticleClassRegistry::Make<BeliefSpriteRule>);
	registry.AddModifier("UR_TownCentreBelief", ParticleClassRegistry::Make<TownCentreBeliefRule>);
}
