/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The rules that make atoms: once, a ball of them at once, emitters of several shapes over time, and a trail behind a
// moving parent

#include <cmath>

#include <algorithm>
#include <numbers>

#include <glm/geometric.hpp>

#include "Common/PortableMath.h"
#include "Common/X87Vector.h"
#include "ParticleClassRegistry.h"
#include "ParticleSounds.h"

using namespace openblack::particles;
using openblack::psys::ParticleObject;
namespace x87 = openblack::x87;

namespace
{
constexpr float k_TwoPi = 2.0f * std::numbers::pi_v<float>;
/// A conical emitter's atoms leave at between these fractions of its speed
constexpr float k_ConicalSpeedMinimum = 0.66f;
constexpr float k_ConicalSpeedRange = 0.33f;
/// The sound radii a single atom's sound is sized by, when the file gives none
constexpr float k_DefaultSmallSoundRadius = 200.0f;
constexpr float k_DefaultMediumSoundRadius = 500.0f;

/// What every rule that makes atoms has: the creator of its atoms and the groups made under each
class CreateRule: public Modifier
{
public:
	explicit CreateRule(const ParticleObject& object)
	    : creator(object.String("PCreator"))
	    , nextGroups(object.IntArray("NextGroups"))
	{
	}
	[[nodiscard]] bool Creates() const override { return true; }

	std::string creator;
	std::vector<int> nextGroups;
};

/// One atom at the collection's spawn point and an offset, then the rule is done. Its sound is sized by how far the
/// effect reaches, when the file says how to measure it.
class CreateRuleAnAtom final: public CreateRule
{
public:
	explicit CreateRuleAnAtom(const ParticleObject& object)
	    : CreateRule(object)
	    , offset(object.Float("OffsetX", 0.0f), object.Float("OffsetY", 0.0f), object.Float("OffsetZ", 0.0f))
	    , scale(object.String("InitScaleFP"))
	    , sound({.action = object.Sound("SoundOfCreate")})
	    , soundRadius(object.String("SoundRadiusFP"))
	    , soundSmall(object.Float("SoundRadiusSmall", k_DefaultSmallSoundRadius))
	    , soundMedium(object.Float("SoundRadiusMedium", k_DefaultMediumSoundRadius))
	{
	}
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		const auto* atomCreator = effect.FindCreator(creator);
		if (atomCreator == nullptr)
		{
			return false;
		}
		auto& atom = effect.NewAtom(collection, atomCreator, nextGroups);
		atom.baseScale *= effect.FloatProvider(scale, 1.0f);
		atom.position += offset;
		auto played = sound;
		if (!soundRadius.empty())
		{
			played.size = maths::SoundSizeFromRadius(effect.FloatProvider(soundRadius, 0.0f), soundSmall, soundMedium);
		}
		StartAtomSound(effect, atom, played);
		return false;
	}

	glm::vec3 offset;
	std::string scale;
	ParticleSound sound;
	std::string soundRadius;
	float soundSmall;
	float soundMedium;
};

/// A number of atoms at once, each somewhere in a ball round the spawn point, then the rule is done. They may start on
/// frames counting down by their order, and the first one plays the sound.
class CreateRuleSphere final: public CreateRule
{
public:
	explicit CreateRuleSphere(const ParticleObject& object)
	    : CreateRule(object)
	    , count(object.Int("NumAtoms", 1))
	    , radius(object.Float("Radius", 1.0f))
	    , radiusScale(object.String("RadiusScaleFP"))
	    , scale(object.String("InitScaleFP"))
	    , offset(object.Float("OffsetX", 0.0f), object.Float("OffsetY", 0.0f), object.Float("OffsetZ", 0.0f))
	    , frameFromIndex(object.Bool("SetInitFrameFromIndex", false))
	    , sound({.action = object.Sound("SoundOfCreate")})
	{
	}
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		const auto* atomCreator = effect.FindCreator(creator);
		if (atomCreator == nullptr)
		{
			return false;
		}
		const float r = radius * effect.FloatProvider(radiusScale, 1.0f);
		for (int i = 0; i < count; ++i)
		{
			auto& atom = effect.NewAtom(collection, atomCreator, nextGroups);
			atom.baseScale *= effect.FloatProvider(scale, 1.0f);
			if (frameFromIndex && atomCreator->numFrames > 0)
			{
				atom.frame = static_cast<float>((count - i - 1) % atomCreator->numFrames);
			}
			glm::vec3 place(0.0f);
			if (r != 0.0f)
			{
				place = effect.RandomInBall() * r;
			}
			atom.position += place + offset;
			if (i == 0)
			{
				StartAtomSound(effect, atom, sound);
			}
		}
		return false;
	}

	int count;
	float radius;
	std::string radiusScale;
	std::string scale;
	glm::vec3 offset;
	bool frameFromIndex;
	ParticleSound sound;
};

/// What the emitters share: their schedule (emitters make atoms over time, at a frequency, up to limits)
class Emitter: public CreateRule
{
public:
	explicit Emitter(const ParticleObject& object)
	    : CreateRule(object)
	    , _limits({
	          .frequency = object.Float("EmissionFreq", 1.0f),
	          .maxAlive = object.Int("MaxAtoms", -1),
	          .maxTotal = object.Int("MaxTotalAtomsToEmit", -1),
	          .randomise = object.Bool("Randomise", true),
	      })
	    , _initiallyVisible(object.Bool("InitiallyVisible", true))
	    , _multiple(object.Bool("AllowMultipleEmits", false))
	{
	}
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& slot) const override
	{
		const auto* atomCreator = effect.FindCreator(creator);
		if (atomCreator == nullptr)
		{
			return true;
		}
		if (!AsksAgain())
		{
			if (Due(effect, collection, slot))
			{
				Emit(effect, effect.NewAtom(collection, atomCreator, nextGroups), collection);
			}
			return true;
		}
		// It asks again after each atom whether another is due, which moves its schedule on, but makes another only
		// when it may emit several in a step: otherwise an atom due a second time in the step is lost
		bool another = true;
		while (Due(effect, collection, slot) && another)
		{
			another = _multiple;
			Emit(effect, effect.NewAtom(collection, atomCreator, nextGroups), collection);
		}
		return true;
	}

protected:
	/// The emitter's shape: where its new atom goes and how fast
	virtual void Emit(Effect& effect, Atom& atom, const Collection& collection) const = 0;
	/// Whether it asks after each atom whether another is due, emitting as many in a step as AllowMultipleEmits lets it;
	/// otherwise it makes at most one atom a step
	[[nodiscard]] virtual bool AsksAgain() const { return false; }

	maths::EmitterLimits _limits;
	bool _initiallyVisible;
	bool _multiple;

private:
	bool Due(Effect& effect, const Collection& collection, Collection::Slot& slot) const
	{
		return maths::ShouldEmit(slot.emitter, _limits, effect.CollectionAge(collection), effect.GetDt(),
		                         static_cast<int>(collection.atoms.size()), [&effect] { return effect.Random(0.5f); });
	}
};

/// Atoms that leave in any direction at up to its speed, turned as their parent is when asked
class EmitterRuleSimple final: public Emitter
{
public:
	explicit EmitterRuleSimple(const ParticleObject& object)
	    : Emitter(object)
	    , _speed(object.Float("Speed", 0.0f))
	    , _orientWithParent(object.Bool("OrientWithParent", false))
	{
	}

protected:
	void Emit(Effect& effect, Atom& atom, const Collection& collection) const override
	{
		if (_speed != 0.0f)
		{
			glm::vec3 direction = effect.RandomInBall();
			const float s = effect.Random(_speed);
			if (direction != glm::vec3(0.0f))
			{
				direction = x87::Scaled<x87::SumOrder::Zyx>(direction);
			}
			atom.velocity = direction * s;
		}
		if (_orientWithParent && collection.parent != nullptr)
		{
			atom.rotation = collection.parent->rotation;
		}
		atom.visible = _initiallyVisible;
	}

	float _speed;
	bool _orientWithParent;
};

/// Atoms placed round the spawn point on a flat disk at a height: a direction across the ground, and a distance up to
/// the radius along it
class DiskEmitter final: public Emitter
{
public:
	explicit DiskEmitter(const ParticleObject& object)
	    : Emitter(object)
	    , _radius(object.Float("Radius", 0.0f))
	    , _height(object.Float("Height", 0.0f))
	{
	}

protected:
	void Emit(Effect& effect, Atom& atom, const Collection& /*collection*/) const override
	{
		// A point in the unit disk, drawn again until it is in it
		glm::vec2 direction;
		do
		{
			direction.x = effect.Random(2.0f) - 1.0f;
			direction.y = effect.Random(2.0f) - 1.0f;
		} while (glm::dot(direction, direction) > 1.0f);
		const float distance = effect.Random(_radius);
		if (direction != glm::vec2(0.0f))
		{
			direction = glm::normalize(direction);
		}
		atom.position += glm::vec3(direction.x * distance, _height, direction.y * distance);
	}

	float _radius;
	float _height;
};

/// Atoms placed on a ring between two radii, at a height: one a step, or as many as are due when it may emit several
class SpreadingDiskEmitter final: public Emitter
{
public:
	explicit SpreadingDiskEmitter(const ParticleObject& object)
	    : Emitter(object)
	    , _startRadius(object.Float("StartRadius", 0.0f))
	    , _stopRadius(object.Float("StopRadius", 0.0f))
	    , _height(object.Float("Height", 0.0f))
	{
	}

protected:
	void Emit(Effect& effect, Atom& atom, const Collection& /*collection*/) const override
	{
		const float r = effect.Random(_startRadius, _stopRadius);
		const float angle = effect.Random(k_TwoPi);
		atom.position += glm::vec3(openblack::portable_math::Cos(angle) * r, _height, openblack::portable_math::Sin(angle) * r);
		atom.visible = _initiallyVisible;
	}
	[[nodiscard]] bool AsksAgain() const override { return true; }

	float _startRadius;
	float _stopRadius;
	float _height;
};

/// Atoms that leave upwards within a cone, its half angle the spread, at two thirds to all of its speed, from anywhere
/// on a disk of its radius; each plays the emitter's sound
class EmitterRuleConical final: public Emitter
{
public:
	explicit EmitterRuleConical(const ParticleObject& object)
	    : Emitter(object)
	    , _speed(object.Float("Speed", 1.0f))
	    , _spread(object.Float("Spread", 0.0f))
	    , _radius(object.Float("Radius", 0.0f))
	    , _sound({.action = object.Sound("SoundEmission")})
	{
	}

protected:
	void Emit(Effect& effect, Atom& atom, const Collection& /*collection*/) const override
	{
		const float phi = effect.Random(_spread);
		const float theta = effect.Random(k_TwoPi);
		const float extra = effect.Random(_speed * k_ConicalSpeedRange);
		const float s = _speed * k_ConicalSpeedMinimum + extra;
		if (_radius != 0.0f)
		{
			const float angle = effect.Random(k_TwoPi);
			const float distance = effect.Random(_radius);
			atom.position += glm::vec3(openblack::portable_math::Cos(angle) * distance, 0.0f,
			                           openblack::portable_math::Sin(angle) * distance);
		}
		const float across = openblack::portable_math::Sin(phi) * s;
		atom.velocity = {openblack::portable_math::Cos(theta) * across, openblack::portable_math::Cos(phi) * s,
		                 openblack::portable_math::Sin(theta) * across};
		atom.visible = _initiallyVisible;
		StartAtomSound(effect, atom, _sound);
	}
	[[nodiscard]] bool AsksAgain() const override { return true; }

	float _speed;
	float _spread;
	float _radius;
	ParticleSound _sound;
};

/// Atoms let out along the path its parent atom (or the effect's origin) moved over the step, as many as its rate and,
/// when asked, the distance moved call for; each starts aged by how far back along the path it was let out. MaxAtoms
/// sets the rate over the atoms' life rather than capping them. What a file leaves out takes the game's defaults: speed
/// 1 up to 100, random speed 1, no rate at all, atoms deleted once older than their die age, drawn on their first step.
class WillowWisp final: public CreateRule
{
public:
	explicit WillowWisp(const ParticleObject& object)
	    : CreateRule(object)
	    , _maxAtoms(object.Int("MaxAtoms", -1))
	    , _dieAge(object.Float("DieAge", 1.0f))
	    , _speed(object.Float("Speed", 1.0f))
	    , _maxSpeed(object.Float("MaxSpeed", 100.0f))
	    , _randomSpeed(object.Float("RandomSpeed", 1.0f))
	    , _randomRadiusMin(object.Float("RandomRadiusMin", 0.0f))
	    , _randomRadiusMax(object.Float("RandomRadiusMax", 0.0f))
	    , _deleteAtDieAge(object.Bool("DeleteAtomsAtDieAge", true))
	    , _moving(object.Bool("EmitDueToMoving", false))
	    , _movingDistance(object.Float("EmitDueToMovingDist", 10.0f))
	    , _movingMaxRate(object.Float("EmitDueToMovingMaxRate", 0.0f))
	    , _randomiseOrientation(object.Bool("RandomiseInitOrientation", false))
	    , _useParentScale(object.Bool("UseParentScale", false))
	    , _addCastVelocity(object.Bool("AddCastVelToInitPos", false))
	    , _drawOnFirstUpdate(object.Bool("DrawOnFirstUpdate", true))
	    , _drawOffsets(object.Bool("DoDrawOffsets", false))
	    , _emitCondition(object.String("EmitConditionOfParent"))
	    , _adjustScale(object.String("AdjustInitialScale"))
	    , _adjustRandomVelocity(object.String("AdjustInitialRandomVel"))
	    , _sound({.action = object.Sound("SoundEmission")})
	{
	}
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& slot) const override
	{
		if (_deleteAtDieAge)
		{
			std::erase_if(collection.atoms, [&](const auto& atom) { return effect.AtomAge(*atom) > _dieAge; });
		}
		const glm::vec3 parent = collection.parent != nullptr ? effect.GlobalPosition(*collection.parent) : effect.GetOrigin();
		const glm::vec3 parentVelocity = collection.parent != nullptr ? collection.parent->velocity : glm::vec3(0.0f);
		glm::vec3 base = parentVelocity * _speed;
		if (_maxSpeed * _maxSpeed < x87::Dot<x87::SumOrder::Xzy>(base, base) && base != glm::vec3(0.0f))
		{
			base = x87::Scaled<x87::SumOrder::Xzy>(base, _maxSpeed);
		}
		if (slot.first)
		{
			slot.state = glm::vec4(parent, 0.0f);
		}
		const glm::vec3 last(slot.state);
		const float dt = effect.GetDt();
		const float rate = static_cast<float>(_maxAtoms) / _dieAge;
		// Tested on the parent atom: without one nothing is let out
		const bool emit = _emitCondition.empty() ||
		                  (collection.parent != nullptr && effect.ConditionForAtom(_emitCondition, *collection.parent));
		const auto* atomCreator = effect.FindCreator(creator);
		if (emit && atomCreator != nullptr)
		{
			float amount = dt * rate;
			if (_moving && _movingDistance != 0.0f)
			{
				float moved = x87::Distance<x87::SumOrder::Zyx>(parent, last) * effect.FloatProvider(_adjustScale, 1.0f) /
				              _movingDistance;
				if (_movingMaxRate != 0.0f)
				{
					moved = std::min(moved / dt, _movingMaxRate) * dt;
				}
				amount = std::max(amount, moved);
			}
			// slot.extra: x how much has been let out in all, y how many atoms that made
			const float before = slot.extra.x;
			slot.extra.x += amount;
			const float after = slot.extra.x;
			while (after - 1.0f > slot.extra.y)
			{
				slot.extra.y += 1.0f;
				auto& atom = effect.NewAtom(collection, atomCreator, nextGroups);
				Place(effect, collection, atom, parent, last, base, (slot.extra.y - before) / (after - before),
				      after != before);
			}
		}
		slot.first = false;
		slot.state = glm::vec4(parent, 0.0f);
		return true;
	}

private:
	void Place(Effect& effect, const Collection& collection, Atom& atom, const glm::vec3& parent, const glm::vec3& last,
	           const glm::vec3& base, float fraction, bool alongPath) const
	{
		const float dt = effect.GetDt();
		glm::vec3 position = parent;
		if (alongPath)
		{
			position = last + (parent - last) * fraction;
			atom.birth -= fraction * dt;
		}
		if (_randomRadiusMax > 0.0f)
		{
			const float angle = effect.Random(k_TwoPi);
			const float r = _randomRadiusMin + effect.Random(_randomRadiusMax - _randomRadiusMin);
			position += glm::vec3(openblack::portable_math::Cos(angle) * r, 0.0f, openblack::portable_math::Sin(angle) * r);
		}
		// The point in the ball is drawn before the speed. Two statements keep this order, because the operands of a
		// product have no fixed order of evaluation.
		glm::vec3 random = effect.RandomInBall();
		random *= effect.Random(_randomSpeed);
		random *= effect.FloatProvider(_adjustRandomVelocity, 1.0f);
		atom.velocity = base + random;
		if (_addCastVelocity)
		{
			position += atom.velocity * dt;
		}
		if (!collection.hierarchy)
		{
			atom.position = position;
		}
		// This computer's player sees it come out of their hand, sliding to where it is over two seconds
		if (_drawOffsets && effect.IsMyInterfaceCasting())
		{
			atom.drawOffset = effect.GetProcessInfo().handPosition - position;
			atom.drawOffsetFrom = effect.GetAge();
		}
		if (_randomiseOrientation)
		{
			// Drawn z first, then y, then x
			const float z = effect.Random(k_TwoPi);
			const float y = effect.Random(k_TwoPi);
			const float x = effect.Random(k_TwoPi);
			atom.rotation = maths::AngleXYZ(x, y, z);
		}
		atom.baseScale *= effect.FloatProvider(_adjustScale, 1.0f);
		if (_useParentScale && collection.parent != nullptr)
		{
			atom.baseScale *= collection.parent->ruleScale * collection.parent->baseScale;
		}
		atom.drawOnFirstUpdate = _drawOnFirstUpdate;
		StartAtomSound(effect, atom, _sound);
	}

	int _maxAtoms;
	float _dieAge;
	float _speed;
	float _maxSpeed;
	float _randomSpeed;
	float _randomRadiusMin;
	float _randomRadiusMax;
	bool _deleteAtDieAge;
	bool _moving;
	float _movingDistance;
	float _movingMaxRate;
	bool _randomiseOrientation;
	bool _useParentScale;
	bool _addCastVelocity;
	bool _drawOnFirstUpdate;
	bool _drawOffsets;
	std::string _emitCondition;
	std::string _adjustScale;
	std::string _adjustRandomVelocity;
	ParticleSound _sound;
};
} // namespace

void openblack::particles::RegisterCreateRules(ParticleClassRegistry& registry)
{
	registry.AddModifier("CreateRuleAnAtom", ParticleClassRegistry::Make<CreateRuleAnAtom>);
	registry.AddModifier("CreateRuleSphere", ParticleClassRegistry::Make<CreateRuleSphere>);
	registry.AddModifier("EmitterRuleSimple", ParticleClassRegistry::Make<EmitterRuleSimple>);
	registry.AddModifier("DiskEmitter", ParticleClassRegistry::Make<DiskEmitter>);
	registry.AddModifier("SpreadingDiskEmitter", ParticleClassRegistry::Make<SpreadingDiskEmitter>);
	registry.AddModifier("EmitterRuleConical", ParticleClassRegistry::Make<EmitterRuleConical>);
	registry.AddModifier("UR_WillowWisp", ParticleClassRegistry::Make<WillowWisp>);
}
