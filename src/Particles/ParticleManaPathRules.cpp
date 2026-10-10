/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The mana path: sparks let out along a path, as the hand shows what it keeps of its player's influence past the
// border. Each spark the effect is given becomes an atom, no more than a hundred at once, in the spark's colour, turned
// along its path. Every step it moves on along the path, weaving from side to side by noise and kept a little above the
// land, and it goes the step after it arrives.

#include <cmath>
#include <cstddef>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include <ParticleFile.h>

#include "ManaPathMaths.h"
#include "ParticleClassRegistry.h"
#include "ParticleMaths.h"

using namespace openblack::particles;
using openblack::psys::ParticleObject;

namespace
{
/// No more atoms are made while the collection has this many; the sparks given meanwhile are let go
constexpr size_t k_MostSparks = 100;

class ManaPath final: public Modifier
{
public:
	explicit ManaPath(const ParticleObject& object)
	    : creator(object.String("PCreator"))
	    , nextGroups(object.IntArray("NextGroups"))
	    , settings({
	          .speed = object.Float("GlowPosSpeed", 0.2f),
	          .timeToTravel = object.Float("TimeToTravel", 2.0f),
	          .noiseFrequency = object.Float("NoiseFrequency", 1.0f),
	          .noiseAmplitude = object.Float("NoiseAmplitude", 1.0f),
	          .height = object.Float("Height", 0.3f),
	          .constantSpeed = object.Bool("UseConstantSpeed", true),
	      })
	{
	}

	[[nodiscard]] bool Creates() const override { return true; }
	/// Waits for sparks for as long as the effect lasts
	[[nodiscard]] bool KeepsAlive() const override { return true; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		// Without a creator for its atoms it does nothing at all
		const auto* spriteCreator = effect.FindCreator(creator);
		if (spriteCreator == nullptr)
		{
			return true;
		}
		while (const auto spark = effect.TakeManaPathSpark())
		{
			if (collection.atoms.size() < k_MostSparks)
			{
				LetOut(effect, collection, *spriteCreator, *spark);
			}
		}
		for (size_t i = 0; i < collection.atoms.size();)
		{
			auto& atom = *collection.atoms[i];
			const auto found = atom.data.find(this);
			if (found == atom.data.end() || found->second.held == nullptr)
			{
				++i;
				continue;
			}
			// One that arrived at the last step goes
			if (found->second.a.x > 1.0f)
			{
				collection.atoms.erase(collection.atoms.begin() + static_cast<std::ptrdiff_t>(i));
				continue;
			}
			Move(effect, atom, found->second);
			++i;
		}
		return true;
	}

private:
	void LetOut(Effect& effect, Collection& collection, const Creator& spriteCreator, const mana_path::Spark& spark) const
	{
		auto& atom = effect.NewAtom(collection, &spriteCreator, nextGroups);
		// Drawn in the order the game draws them: the frequency's, the size's, then where along the noise it starts
		const float frequency = effect.Random(0.8f);
		const float amplitude = effect.Random(0.8f);
		const float phase = effect.Random(1.0f);
		const auto path = mana_path::MakePath(spark.from, spark.to, settings,
		                                      {.frequency = frequency, .amplitude = amplitude, .phase = phase});
		// The spark's colour, the creator's opacity
		atom.rgba = {static_cast<uint8_t>((spark.rgb >> 16u) & 0xFFu), static_cast<uint8_t>((spark.rgb >> 8u) & 0xFFu),
		             static_cast<uint8_t>(spark.rgb & 0xFFu), atom.rgba[3]};
		atom.rotation = maths::AngleY(mana_path::Heading(path));
		if (std::abs(path.length) < mana_path::k_ShortestPath)
		{
			collection.atoms.pop_back();
			return;
		}
		auto& data = atom.data[this];
		data.held = std::make_shared<const mana_path::Path>(path);
		data.a.x = 0.0f;
	}

	void Move(Effect& effect, Atom& atom, AtomRuleData& data) const
	{
		const auto& path = *std::static_pointer_cast<const mana_path::Path>(data.held);
		const float progress = mana_path::Progress(path, settings, effect.AtomAge(atom));
		data.a.x = progress;
		const float along = std::clamp(progress, 0.0f, 1.0f);
		const auto point = mana_path::PointOnPath(path, settings, progress);
		const float noise = effect.Services().noise.Smooth(mana_path::WeaveNoiseAt(path, along));
		const auto woven = mana_path::Woven(path, point, along, noise);
		const auto ground = mana_path::GroundPoint(woven);
		atom.position = {woven.x, effect.Services().world.LandHeight(ground) + settings.height, woven.y};
	}

	std::string creator;
	std::vector<int> nextGroups;
	mana_path::Settings settings;
};
} // namespace

void openblack::particles::RegisterManaPathRules(ParticleClassRegistry& registry)
{
	registry.AddModifier("UR_ManaPathNew", ParticleClassRegistry::Make<ManaPath>);
}
