/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The flock miracles' rule: an atom glued to each animal the miracle gives its effect, which carries the animal's trail
// of sparkles, smoke or dust at the animal's size. The flock calls once, as its first animal appears.

#include <memory>
#include <string>
#include <vector>

#include <ParticleFile.h>

#include "ParticleClassRegistry.h"
#include "ParticleSounds.h"

using namespace openblack::particles;
using openblack::psys::ParticleObject;

namespace
{
/// One atom for each target the miracle hands the effect, kept where its object is; once the object has gone the atom
/// stays where it last was, unless the file says it goes too
class FollowTargets final: public Modifier
{
public:
	explicit FollowTargets(const ParticleObject& object)
	    : _creator(object.String("PCreator"))
	    , _nextGroups(object.IntArray("NextGroups"))
	    , _sound({.action = object.Sound("SoundCreate")})
	    , _soundOneOnly(object.Bool("SoundOneOnly", true))
	    , _removeAtomWhenTargetDies(object.Bool("RemoveAtomWhenTargetDies", false))
	{
	}

	/// The effect waits for the animals its miracle makes, until the miracle closes down
	[[nodiscard]] bool KeepsAlive() const override { return true; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		auto& world = effect.Services().world;
		// One new target a step
		if (const auto target = effect.TakeTarget(); target.has_value())
		{
			if (const auto info = world.Target(*target, false); info.has_value())
			{
				auto& atom = effect.NewAtom(collection, effect.FindCreator(_creator), _nextGroups);
				atom.data[this].object = *target;
				atom.position = effect.GlobalToLocal(collection, info->position);
				// Its trail is as big as its animal
				atom.ruleScale = info->scale;
				// The call is heard once, for the first atom, unless the file asks for one each
				if (!_soundOneOnly || collection.atoms.size() == 1)
				{
					StartAtomSound(effect, atom, _sound);
				}
			}
		}
		std::erase_if(collection.atoms, [&](const AtomPtr& atom) {
			auto* const found = atom->data.find(this);
			if (found == atom->data.end() || found->second.object == entt::null)
			{
				return false;
			}
			const auto info = world.Target(found->second.object, false);
			if (!info.has_value())
			{
				// Its animal gone, the atom stays where it was
				found->second.object = entt::null;
				return _removeAtomWhenTargetDies;
			}
			// Only the position follows the animal: the speed of the atom does not change
			atom->position = effect.GlobalToLocal(collection, info->position);
			return false;
		});
		return true;
	}

private:
	std::string _creator;
	std::vector<int> _nextGroups;
	ParticleSound _sound;
	bool _soundOneOnly;
	bool _removeAtomWhenTargetDies;
};
} // namespace

void openblack::particles::RegisterFlockRules(ParticleClassRegistry& registry)
{
	registry.AddModifier("UR_FollowTargets", ParticleClassRegistry::Make<FollowTargets>);
}
