/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The heal miracle's rules (SF_HealChakra, SF_HealChakraPU, SF_HealChakraInHand, SF_HealChakraOnHolder):
// UR_HealSpellChakra, one chakra atom per target that heals it (event 5) and makes it glow, CreateRuleFusedSphericalExplode,
// the burst of sprites under each chakra, and UR_HealInHand, the in-hand wiggle. Wiki: docs/bw1-notes/miracles.md, "Heal".

#include "Heal.h"

#include <cmath>

#include <algorithm>
#include <memory>
#include <numbers>
#include <vector>

#include <glm/geometric.hpp>

#include "Audio/Services/SpellSounds.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/ChakraMark.h"
#include "ECS/Components/SpecularColour.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "Locator.h"
#include "Particles/PSys.h"
#include "Particles/PSysFile.h"
#include "Particles/PSysRegistry.h"
#include "Particles/SoundAction.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
/// Whether some chakra is on `object` (components::ChakraMark), so that two heal spells don't chakra the same villager
bool IsChakraed(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(object) && registry.AllOf<ecs::components::ChakraMark>(object);
}

bool Available(entt::entity object)
{
	// available (ecs::IsAvailable), with a Transform
	return object != entt::null && Locator::entitiesRegistry::has_value() && ecs::IsAvailable(object) &&
	       Locator::entitiesRegistry::value().AllOf<ecs::components::Transform>(object);
}

/// The chakra also ends when its target is unavailable for a state change. The only thing that sets that flag is
/// placing the object in the hand, so it is the villager the player has picked up. (approximate) openblack has no such
/// flag: the hand's held object stands for it.
bool UnavailableForStateChange(entt::entity object)
{
	if (!Locator::handSystem::has_value())
	{
		return false;
	}
	const auto held = Locator::handSystem::value().GetHeldObject();
	return held.has_value() && *held == object;
}

/// A living thing's own specular colour (other objects ignore it): the whole dword. The chakra's fade always writes
/// alpha 0xFF, so even an RGB of 0 keeps the dword non-zero and the villager and animal draws still see an own specular
/// (it wins over the poison tint): the component stays. Only the chakra data's destructor clears it.
void SetSpecularColour(entt::entity object, glm::u8vec3 colour)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.AnyOf<ecs::components::Villager, ecs::components::Animal>(object))
	{
		return;
	}
	registry.AssignOrReplace<ecs::components::SpecularColour>(object, colour);
}

/// The specular colour set to 0, the whole dword: no own specular any more
void ClearSpecularColour(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AnyOf<ecs::components::Villager, ecs::components::Animal>(object) &&
	    registry.AllOf<ecs::components::SpecularColour>(object))
	{
		registry.Remove<ecs::components::SpecularColour>(object);
	}
}

/// UR_HealSpellChakra's data on an atom
struct ChakraData
{
	ChakraData() = default;
	ChakraData(const ChakraData&) = delete;
	ChakraData(ChakraData&&) = delete;
	ChakraData& operator=(const ChakraData&) = delete;
	ChakraData& operator=(ChakraData&&) = delete;
	/// Out of the global list; a target still there loses its glow
	~ChakraData()
	{
		if (listed != entt::null && Locator::entitiesRegistry::has_value())
		{
			auto& registry = Locator::entitiesRegistry::value();
			if (registry.Valid(listed))
			{
				if (auto* mark = registry.TryGet<ecs::components::ChakraMark>(listed); mark != nullptr && --mark->chakras == 0)
				{
					registry.Remove<ecs::components::ChakraMark>(listed);
				}
			}
		}
		if (Available(target))
		{
			ClearSpecularColour(target);
		}
	}
	/// The target, and the one put in the global list, at its front
	void SetTarget(entt::entity object)
	{
		target = object;
		listed = object;
		if (object != entt::null && Locator::entitiesRegistry::value().Valid(object))
		{
			auto& registry = Locator::entitiesRegistry::value();
			if (auto* mark = registry.TryGet<ecs::components::ChakraMark>(object); mark != nullptr)
			{
				++mark->chakras;
			}
			else
			{
				registry.Assign<ecs::components::ChakraMark>(object);
			}
		}
	}

	entt::entity listed {entt::null};
	entt::entity target {entt::null}; ///< (the original also keeps the game turn it was last seen: unused)
	bool done {false};                ///< the chakra ends: the atom goes
	bool fresh {true};                ///< its first step (the burst under it is not made yet)
	bool soundStarted {false};
	float radius {1.0f}; ///< the target's Get2DRadius
	float height {1.0f}; ///< the target's GetHeight
};

/// The object's map position as a world point (the land's altitude + its height above it), and with
/// TakeCentrePos half its height higher
glm::vec3 TargetPosition(entt::entity object, bool centre)
{
	auto position = Locator::entitiesRegistry::value().Get<const ecs::components::Transform>(object).position;
	if (centre)
	{
		position.y += ecs::object::GetHeight(object) * 0.5f;
	}
	return position;
}

/// UR_HealSpellChakra. Flags 2 without 4: not a creator, but the effect waits for its targets until it closes.
/// SoundSpacing (0.2), ScaleRadius and ScaleHeight are read and never used.
class HealSpellChakra final: public Modifier
{
public:
	explicit HealSpellChakra(const Object& object)
	    : creator(object.String("PCreator"))
	    , nextGroups(object.Array("NextGroups"))
	    , scaleToObject(object.Bool("ScalePropObjectSize", false))
	    , takeCentre(object.Bool("TakeCentrePos", false))
	    , soundHeal(ReadSoundAction(object, "SoundHeal"))
	    , maxAlpha(object.Float("MaxAlpha", 255.0f))
	    , ageMaxAlpha(object.Float("AtomAgeMaxAlpha", 1.0f))
	    , ageZeroAlpha(object.Float("AtomAgeZeroAlpha", 3.0f))
	    , specular(static_cast<float>(object.Int("SpecularColorR", 0)), static_cast<float>(object.Int("SpecularColorG", 0)),
	               static_cast<float>(object.Int("SpecularColorB", 0)))
	{
	}
	[[nodiscard]] bool KeepsAlive() const override { return true; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		// every target of the spell becomes a chakra, unless another one already has it
		for (auto target = effect.TakeTarget(); target != entt::null; target = effect.TakeTarget())
		{
			// an available object with a 3D object
			if (!Available(target) || IsChakraed(target))
			{
				continue;
			}
			const auto position = TargetPosition(target, takeCentre);
			if (effect.GetSink() != nullptr)
			{
				// the heal itself: SpellEvent{5, the target's centre, no movement, strength 1, no shield check, target}
				const SpellEventInfo event {
				    .type = SpellEventInfo::Type::Object, .position = position, .strength = 1.0f, .target = target};
				effect.SendSpellEvent(event);
			}
			auto& atom = effect.NewAtom(collection, effect.FindCreator(creator), nextGroups);
			auto data = std::make_shared<ChakraData>();
			data->SetTarget(target);
			data->radius = ecs::object::Get2DRadius(target);
			data->height = ecs::object::GetHeight(target);
			atom.modifierData.insert_or_assign(this, data);
			atom.position = position;
		}
		// every chakra follows its target and fades with the burst under it; the newest atom plays the heal sound
		for (size_t i = 0; i < collection.atoms.size();)
		{
			auto& atom = *collection.atoms[i];
			const auto found = atom.modifierData.find(this);
			if (found == atom.modifierData.end())
			{
				++i;
				continue;
			}
			auto& data = *std::static_pointer_cast<ChakraData>(found->second);
			// the collection's first atom, sound not started and age > 0 -> StartSound. The original's list grows at
			// its head, so its first atom is the newest one; openblack's vector grows at the back (PSys.cpp NewAtom),
			// so the newest is the last. A brand new atom has age 0, so the sound waits for the next step either way
			if (i + 1 == collection.atoms.size() && !data.soundStarted && effect.AtomAge(atom) > 0.0f)
			{
				data.soundStarted = true;
				audio::spell_sounds::StartSound(effect, atom, soundHeal);
			}
			if (data.target != entt::null && !Available(data.target))
			{
				data.target = entt::null; // gone, and the atom forgets it
			}
			// picked up by the hand ends the chakra, but the target is kept, so the destructor
			// still takes its glow away
			if (data.target != entt::null && !UnavailableForStateChange(data.target))
			{
				atom.position = TargetPosition(data.target, takeCentre);
				if (scaleToObject)
				{
					atom.ruleScale = ecs::object::Get2DRadius(data.target);
				}
			}
			else
			{
				data.done = true;
			}
			if (!data.done)
			{
				if (atom.subCollections.empty())
				{
					return false; // a chakra with no burst under it detaches the rule
				}
				Fade(effect, *atom.subCollections.front(), data);
				data.fresh = false;
			}
			if (data.done)
			{
				collection.atoms.erase(collection.atoms.begin() + static_cast<std::ptrdiff_t>(i));
				continue;
			}
			++i;
		}
		return true;
	}

	std::string creator;
	std::vector<int> nextGroups;
	bool scaleToObject; ///< ScalePropObjectSize
	bool takeCentre;    ///< TakeCentrePos
	SoundAction soundHeal;
	float maxAlpha;
	float ageMaxAlpha;
	float ageZeroAlpha;
	glm::vec3 specular; ///< R, G, B

private:
	/// The burst's age gives t, up to 1 at AtomAgeMaxAlpha and back to 0 at AtomAgeZeroAlpha; its sprites
	/// get alpha t x MaxAlpha and the target glows with t x SpecularColor. An empty burst ends the chakra (not on its
	/// first step, before the burst is made).
	void Fade(const Effect& effect, Collection& burst, ChakraData& data) const
	{
		if (burst.atoms.empty())
		{
			if (!data.fresh)
			{
				data.done = true;
			}
			return;
		}
		const float t = heal::ChakraFade(effect.CollectionAge(burst), ageMaxAlpha, ageZeroAlpha);
		// truncated
		const auto alpha = static_cast<uint8_t>(static_cast<int>(t * maxAlpha));
		if (data.target != entt::null)
		{
			SetSpecularColour(data.target, glm::u8vec3(static_cast<uint8_t>(static_cast<int>(specular.r * t)),
			                                           static_cast<uint8_t>(static_cast<int>(specular.g * t)),
			                                           static_cast<uint8_t>(static_cast<int>(specular.b * t))));
		}
		for (auto& sprite : burst.atoms)
		{
			sprite->colour[3] = alpha; // the colour's alpha byte
		}
	}
};

/// CreateRuleFusedSphericalExplode: once the collection is FuseTime old, NumAtoms atoms fly out in random directions
/// (the upper half with OnlyHemisphere) at one random speed in [MinSpeed, MaxSpeed), y x ScaleYSpeed; the first plays
/// SoundExplode and DisableParent hides the parent atom (flag 0x10). Then the rule detaches.
class FusedSphericalExplode final: public Modifier
{
public:
	explicit FusedSphericalExplode(const Object& object)
	    : creator(object.String("PCreator"))
	    , nextGroups(object.Array("NextGroups"))
	    , count(object.Int("NumAtoms", 100))
	    , minSpeed(object.String("MinSpeed"))
	    , maxSpeed(object.String("MaxSpeed"))
	    , fuseTime(object.Float("FuseTime", 1.0f))
	    , scaleYSpeed(object.Float("ScaleYSpeed", 1.0f))
	    , hemisphere(object.Bool("OnlyHemisphere", false))
	    , disableParent(object.Bool("DisableParent", true))
	    , sound(ReadSoundAction(object, "SoundExplode"))
	{
	}
	[[nodiscard]] bool Creates() const override { return true; }
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		const auto* source = effect.FindCreator(creator);
		if (source == nullptr || minSpeed.empty() || maxSpeed.empty())
		{
			return false;
		}
		if (effect.CollectionAge(collection) < fuseTime)
		{
			return true;
		}
		// the providers' current values; one speed for the whole burst
		const float low = effect.FloatProvider(minSpeed, 0.0f);
		const float speed = effect.Random(effect.FloatProvider(maxSpeed, 0.0f) - low) + low;
		for (int i = 0; i < count; ++i)
		{
			auto& atom = effect.NewAtom(collection, source, nextGroups);
			glm::vec3 direction(0.0f);
			while (glm::dot(direction, direction) == 0.0f)
			{
				direction = effect.RandomInBall();
			}
			if (hemisphere)
			{
				direction.y = std::abs(direction.y);
			}
			direction *= 1.0f / std::sqrt(glm::dot(direction, direction));
			atom.velocity = glm::vec3(direction.x * speed, direction.y * speed * scaleYSpeed, direction.z * speed);
			if (i == 0)
			{
				audio::spell_sounds::StartSound(effect, atom, sound);
			}
		}
		if (disableParent && collection.parent != nullptr)
		{
			collection.parent->visible = false;
		}
		return false;
	}
	std::string creator;
	std::vector<int> nextGroups;
	int count;
	std::string minSpeed;
	std::string maxSpeed;
	float fuseTime;
	float scaleYSpeed;
	bool hemisphere;
	bool disableParent;
	SoundAction sound;
};

/// UR_HealInHand (WiggleFreq 1 by default): each atom goes to the parent's position (the parent atom's local position,
/// or the effect's origin) times sin(age x WiggleFreq x 2 pi), the odd ones with the opposite sign
class HealInHand final: public Modifier
{
public:
	explicit HealInHand(const Object& object)
	    : wiggle(object.Float("WiggleFreq", 1.0f))
	{
	}
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		const glm::vec3 parent = collection.parent != nullptr ? collection.parent->position : effect.GetOrigin();
		int index = 0;
		for (auto& atom : collection.atoms)
		{
			const float sign = index % 2 == 1 ? -1.0f : 1.0f;
			const float s = std::sin(effect.CollectionAge(collection) * wiggle * (2.0f * std::numbers::pi_v<float>)) * sign;
			atom->position = parent * s;
			++index;
		}
		return true;
	}
	float wiggle;
};
} // namespace

float openblack::psys::heal::ChakraFade(float age, float ageMaxAlpha, float ageZeroAlpha)
{
	const float t = age < ageMaxAlpha ? age / ageMaxAlpha : 1.0f - (age - ageMaxAlpha) / (ageZeroAlpha - ageMaxAlpha);
	if (!(t > 0.0f))
	{
		return 0.0f;
	}
	return t < 1.0f ? t : 1.0f;
}

void openblack::psys::RegisterHealRules()
{
	RegisterModifier("UR_HealSpellChakra", MakeModifierOf<HealSpellChakra>);
	RegisterModifier("CreateRuleFusedSphericalExplode", MakeModifierOf<FusedSphericalExplode>);
	RegisterModifier("UR_HealInHand", MakeModifierOf<HealInHand>);
}
