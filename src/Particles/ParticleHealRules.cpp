/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The heal miracle's rules: a chakra over each person the miracle heals, which heals them as it appears, follows them
// and lights them as the burst of sparks under it rises and fades; the burst itself, sparks flying out of a point once
// its collection is old enough; and the chakras' wiggle while the miracle is in the hand

#include <cmath>

#include <memory>
#include <numbers>
#include <string>
#include <vector>

#include <ParticleFile.h>
#include <glm/geometric.hpp>

#include "Common/X87.h"
#include "Common/X87Vector.h"
#include "ParticleArchive.h"
#include "ParticleClassRegistry.h"
#include "ParticleMaths.h"
#include "ParticleSounds.h"

using namespace openblack;
using namespace openblack::particles;
using openblack::psys::ParticleObject;

namespace
{
constexpr float k_TwoPi = 2.0f * std::numbers::pi_v<float>;

/// Held by a chakra for as long as it lives. When the chakra goes for any cause (its burst is over, its target went, or
/// its effect closed with it), the target is free for another heal and loses the chakra's glow.
class TargetHold
{
public:
	TargetHold(ParticleWorldInterface& world, entt::entity target)
	    : _world(world)
	    , _target(target)
	{
	}
	TargetHold(const TargetHold&) = delete;
	TargetHold& operator=(const TargetHold&) = delete;
	TargetHold(TargetHold&&) = delete;
	TargetHold& operator=(TargetHold&&) = delete;
	~TargetHold()
	{
		_world.ClaimTarget(_target, false);
		_world.SetTargetGlow(_target, glm::u8vec3(0));
	}

private:
	ParticleWorldInterface& _world;
	entt::entity _target;
};

/// A chakra over each target the miracle gives the effect that no other effect has, which heals it as it appears. Each
/// chakra follows its target, scaled to it when asked, and fades with the burst under it: up to full at one age of the
/// burst and back to nothing at another, lighting the target in its colour as it does. A chakra ends when its burst has
/// gone, or its target has gone or been picked up. It keeps an effect waiting for targets until the effect closes.
class HealSpellChakra final: public Modifier
{
public:
	explicit HealSpellChakra(const ParticleObject& object)
	    : _creator(object.String("PCreator"))
	    , _nextGroups(object.IntArray("NextGroups"))
	    , _scaleToObject(object.Bool("ScalePropObjectSize", false))
	    , _takeCentre(object.Bool("TakeCentrePos", false))
	    , _soundHeal({.action = object.Sound("SoundHeal")})
	    , _maxAlpha(object.Float("MaxAlpha", 255.0f))
	    , _ageMaxAlpha(object.Float("AtomAgeMaxAlpha", 1.0f))
	    , _ageZeroAlpha(object.Float("AtomAgeZeroAlpha", 3.0f))
	    , _glow(static_cast<float>(object.Int("SpecularColorR", 0)), static_cast<float>(object.Int("SpecularColorG", 0)),
	            static_cast<float>(object.Int("SpecularColorB", 0)))
	{
	}
	[[nodiscard]] bool KeepsAlive() const override { return true; }
	/// The hold is not written: a load takes it up again for the target the chakra follows, which the world's own record
	/// keeps claimed
	void SaveHeld(const AtomRuleData& /*data*/, EffectWriter& /*out*/) const override {}
	void LoadHeld(AtomRuleData& data, EffectReader& in) const override
	{
		if (data.object != entt::null)
		{
			data.held = std::make_shared<const TargetHold>(in.CurrentEffect().Services().world, data.object);
		}
	}

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		auto& world = effect.Services().world;
		for (auto target = effect.TakeTarget(); target.has_value(); target = effect.TakeTarget())
		{
			const auto info = world.Target(*target, _takeCentre);
			if (!info.has_value() || world.IsTargetClaimed(*target))
			{
				continue;
			}
			effect.SendSpellEvent({.type = SpellEventInfo::Type::Object,
			                       .position = info->position,
			                       .velocity = glm::vec3(0.0f),
			                       .strength = 1.0f,
			                       .checkShields = false,
			                       .target = *target});
			auto& atom = effect.NewAtom(collection, effect.FindCreator(_creator), _nextGroups);
			auto& data = atom.data[this];
			data.object = *target;
			atom.position = info->position;
			world.ClaimTarget(*target, true);
			data.held = std::make_shared<const TargetHold>(world, *target);
		}
		// The newest chakra plays the heal once it has lived a step
		const Atom* newest = collection.atoms.empty() ? nullptr : collection.atoms.back().get();
		for (size_t i = 0; i < collection.atoms.size();)
		{
			auto& atom = *collection.atoms[i];
			auto* const found = atom.data.find(this);
			if (found == atom.data.end())
			{
				++i;
				continue;
			}
			auto& data = found->second;
			// A chakra from a save that did not keep the hold takes it up again
			if (data.object != entt::null && data.held == nullptr)
			{
				data.held = std::make_shared<const TargetHold>(world, data.object);
			}
			if (&atom == newest && !data.started && effect.AtomAge(atom) > 0.0f)
			{
				data.started = true;
				StartAtomSound(effect, atom, _soundHeal);
			}
			bool done = false;
			const auto info = data.object != entt::null ? world.Target(data.object, _takeCentre) : std::nullopt;
			if (info.has_value() && !world.IsTargetHeld(data.object))
			{
				atom.position = info->position;
				if (_scaleToObject)
				{
					atom.ruleScale = info->radius;
				}
				if (atom.subCollections.empty())
				{
					// A chakra without a burst under it lets the rule go
					return false;
				}
				done = Fade(effect, *atom.subCollections.front(), atom, data);
			}
			else
			{
				done = true;
			}
			if (done)
			{
				// The chakra's hold frees its target and puts out its glow as the chakra goes
				collection.atoms.erase(collection.atoms.begin() + static_cast<std::ptrdiff_t>(i));
				continue;
			}
			++i;
		}
		return true;
	}

private:
	/// The burst's fade and the target's glow; true once the burst is over. The burst is made on the chakra's first
	/// step, so it is only over once the chakra has lived a step.
	bool Fade(const Effect& effect, Collection& burst, const Atom& chakra, const AtomRuleData& data) const
	{
		if (burst.atoms.empty())
		{
			return effect.AtomAge(chakra) > 0.0f;
		}
		const float t = maths::ChakraFade(effect.CollectionAge(burst), _ageMaxAlpha, _ageZeroAlpha);
		const auto alpha = maths::TruncateToByte(t * _maxAlpha);
		effect.Services().world.SetTargetGlow(data.object, glm::u8vec3(maths::TruncateToByte(_glow.r * t),
		                                                               maths::TruncateToByte(_glow.g * t),
		                                                               maths::TruncateToByte(_glow.b * t)));
		for (auto& spark : burst.atoms)
		{
			spark->rgba[3] = alpha;
		}
		return false;
	}

	std::string _creator;
	std::vector<int> _nextGroups;
	bool _scaleToObject;
	bool _takeCentre;
	ParticleSound _soundHeal;
	float _maxAlpha;
	float _ageMaxAlpha;
	float _ageZeroAlpha;
	glm::vec3 _glow;
};

/// Once its collection is old enough, a number of atoms fly out of the spawn point all at once in random directions,
/// only upwards when asked, at one random speed between two providers' values, the vertical scaled. The first plays the
/// sound, and the parent atom may be hidden. Then the rule is done.
class CreateRuleFusedSphericalExplode final: public Modifier
{
public:
	explicit CreateRuleFusedSphericalExplode(const ParticleObject& object)
	    : _creator(object.String("PCreator"))
	    , _nextGroups(object.IntArray("NextGroups"))
	    , _count(object.Int("NumAtoms", 100))
	    , _minSpeed(object.String("MinSpeed"))
	    , _maxSpeed(object.String("MaxSpeed"))
	    , _fuseTime(object.Float("FuseTime", 1.0f))
	    , _scaleYSpeed(object.Float("ScaleYSpeed", 1.0f))
	    , _hemisphere(object.Bool("OnlyHemisphere", false))
	    , _disableParent(object.Bool("DisableParent", true))
	    , _sound({.action = object.Sound("SoundExplode")})
	{
	}
	[[nodiscard]] bool Creates() const override { return true; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		const auto* atomCreator = effect.FindCreator(_creator);
		if (atomCreator == nullptr || _minSpeed.empty() || _maxSpeed.empty())
		{
			return false;
		}
		if (effect.CollectionAge(collection) < _fuseTime)
		{
			return true;
		}
		const float low = effect.FloatProvider(_minSpeed, 0.0f);
		const float speed = effect.Random(effect.FloatProvider(_maxSpeed, 0.0f) - low) + low;
		for (int i = 0; i < _count; ++i)
		{
			auto& atom = effect.NewAtom(collection, atomCreator, _nextGroups);
			glm::vec3 direction(0.0f);
			while (x87::Dot<x87::SumOrder::Zxy>(direction, direction) == 0.0f)
			{
				direction = effect.RandomInBall();
			}
			atom.velocity = maths::BurstVelocity(direction, speed, _scaleYSpeed, _hemisphere);
			if (i == 0)
			{
				StartAtomSound(effect, atom, _sound);
			}
		}
		if (_disableParent && collection.parent != nullptr)
		{
			collection.parent->visible = false;
		}
		return false;
	}

private:
	std::string _creator;
	std::vector<int> _nextGroups;
	int _count;
	std::string _minSpeed;
	std::string _maxSpeed;
	float _fuseTime;
	float _scaleYSpeed;
	bool _hemisphere;
	bool _disableParent;
	ParticleSound _sound;
};

/// While the miracle is in the hand its chakras swing through the parent's point, out and back, the odd ones the other
/// way, so many times a second
class HealInHand final: public Modifier
{
public:
	explicit HealInHand(const ParticleObject& object)
	    : _wiggle(object.Float("WiggleFreq", 1.0f))
	{
	}
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		const glm::vec3 parent = collection.parent != nullptr ? collection.parent->position : effect.GetOrigin();
		const float swing = x87::RoundToFloat(x87::Sin(effect.CollectionAge(collection) * _wiggle * k_TwoPi));
		bool odd = false;
		// In the game's order, the newest first
		for (auto it = collection.atoms.rbegin(); it != collection.atoms.rend(); ++it)
		{
			(*it)->position = parent * (odd ? -swing : swing);
			odd = !odd;
		}
		return true;
	}

private:
	float _wiggle;
};
} // namespace

void openblack::particles::RegisterHealRules(ParticleClassRegistry& registry)
{
	registry.AddModifier("UR_HealSpellChakra", ParticleClassRegistry::Make<HealSpellChakra>);
	registry.AddModifier("CreateRuleFusedSphericalExplode", ParticleClassRegistry::Make<CreateRuleFusedSphericalExplode>);
	registry.AddModifier("UR_HealInHand", ParticleClassRegistry::Make<HealInHand>);
}
