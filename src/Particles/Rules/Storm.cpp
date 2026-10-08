/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The storm miracle's rules: UR_CloudMoverNew, UR_CloudGather, UR_Tornado and UR_StormCast. Every constant below is
// the original rule's default or a literal of the code it comes from; the spell files replace the defaults with their
// properties. See docs/bw1-notes/miracles.md, "Tormenta".

#include "Storm.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <LNDFile.h>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Audio/Services/SpellSounds.h"
#include "Common/GUtilsDistance.h"
#include "ECS/AnimalAI.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/Life.h"
#include "ECS/Map.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectResources.h"
#include "ECS/Physics/ParticleCarriedObjects.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/PotResource.h"
#include "ECS/Registry.h"
#include "ECS/StoragePitStore.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Trees.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/VillagerAnimations.h"
#include "ECS/Weather/Weather.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/MagicTables.h"
#include "Particles/Noise.h"
#include "Particles/PSys.h"
#include "Particles/PSysFile.h"
#include "Particles/PSysManager.h"
#include "Particles/PSysRegistry.h"
#include "Particles/Rules/Shield.h"
#include "Particles/SoundAction.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
constexpr float k_TwoPi = 6.28318548f; ///< 2 pi as a float (the range of every random angle here)

float LandAt(float x, float z)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)) : 0.0f;
}

/// a + rand(b - a)
float RandRange(Effect& effect, float a, float b)
{
	return effect.Random(a, b);
}

float Clamp01(float x)
{
	return std::clamp(x, 0.0f, 1.0f);
}

/// The parent atom's position, the manager's origin without one
glm::vec3 ParentPosition(const Effect& effect, const Collection& collection)
{
	return collection.parent != nullptr ? effect.GlobalPosition(*collection.parent) : effect.GetOrigin();
}

/// The process info's cameraForward with y = 0, not normalised
glm::vec3 CurrentHeading(const Effect& effect)
{
	const auto& forward = effect.GetProcessInfo().cameraForward;
	return {forward.x, 0.0f, forward.z};
}

entt::entity SpellOf(const Effect& effect)
{
	return effect.GetSink() != nullptr ? effect.GetSink()->SpellEntity() : entt::null;
}

/// The storm test hooks' strike count, in the debug hooks' store (Locator::debugHooks)
struct StormRulesDebugHooksState
{
	size_t strikes {0}; ///< every gather's strikes since the debug hooks were made
};

StormRulesDebugHooksState& StormRulesDebugHooksData()
{
	if (!Locator::debugHooks::has_value())
	{
		std::fputs("storm rules: no debug hooks in the locator (Locator::debugHooks)\n", stderr);
		std::abort();
	}
	return Locator::debugHooks::value().Get<StormRulesDebugHooksState>();
}

/// Set when the program exits: the effects (Locator::particleSystem) may outlive the storm registry and the carried
/// set, so the data destructors below must not touch them then (port bookkeeping, not in the original). ExitGuard is a
/// function-local static made on first use, after every namespace-scope static, so it is destroyed before them.
/// It stays although a normal exit clears the effects first (ShutDownServices): an exit that skips that, or a test that
/// resets its services without clearing them, still destroys the effects with the locator's statics, and while a
/// locator slot is being reset it is already empty, so those destructors could not reach the services they use.
bool g_Exiting = false;
struct ExitGuard
{
	ExitGuard() = default;
	ExitGuard(const ExitGuard&) = delete;
	ExitGuard& operator=(const ExitGuard&) = delete;
	ExitGuard(ExitGuard&&) = delete;
	ExitGuard& operator=(ExitGuard&&) = delete;
	~ExitGuard() { g_Exiting = true; }
};
void TouchExitGuard()
{
	static ExitGuard guard;
}

// ================================================================================================================
// UR_CloudMoverNew
// ================================================================================================================

/// UR_CloudMoverNew's collection data: whether the next call is the first
struct CloudMoverData
{
	bool first {true};
};

class CloudMoverNew final: public Modifier
{
public:
	explicit CloudMoverNew(const Object& object)
	    : delayBeforeMove(object.Float("DelayBeforeMove", 0.0f))
	    , windDamping(object.Float("WindDamping", 0.06f))
	    , windMagnification(object.Float("WindMagnification", 60.0f))
	{
	}

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		auto& data = CollectionDataOf<CloudMoverData>(collection, this);
		// the first call puts every atom at the manager's origin
		if (data.first)
		{
			data.first = false;
			for (auto& atom : collection.atoms)
			{
				atom->position = effect.GetOrigin();
			}
		}
		// nothing before the manager's age reaches DelayBeforeMove
		if (effect.GetAge() < delayBeforeMove)
		{
			return true;
		}
		const float dt = effect.GetDt();
		const bool script = effect.GetSink() != nullptr && effect.GetSink()->IsScriptCasting();
		for (auto& atom : collection.atoms)
		{
			const glm::vec3 old = atom->position;
			// no wind for a script's storm; else the smoothed climate wind / 8
			const glm::vec3 wind = script ? glm::vec3(0.0f) : weather::GetWindAt(old, true);
			// T = wind x WindMagnification x 0.1; v += WindDamping x (T - v) x dt
			const glm::vec3 target = wind * windMagnification * 0.1f;
			atom->velocity += windDamping * (target - atom->velocity) * dt;
			// the original measures the slope between here and the next step and scales a copy of the horizontal
			// velocity by 1 - 0.9 x (angle / (pi / 2)): that copy is never read (the position below uses the velocity
			// just stored), so the slope has no effect
			// p += v x dt
			atom->position = old + atom->velocity * dt;
			// the spell's position follows the core
			const auto spell = SpellOf(effect);
			if (spell != entt::null)
			{
				auto& registry = Locator::entitiesRegistry::value();
				if (auto* component = registry.TryGet<ecs::components::Spell>(spell); component != nullptr)
				{
					component->position = magic::ToMap(atom->position);
				}
			}
			// the tornado's cores bounce off shields
			if (effect.PowerUpLevel() == 1)
			{
				shields::DoAnyShieldDeflections(effect, *atom, old);
			}
		}
		return true;
	}

private:
	float delayBeforeMove;
	float windDamping;
	float windMagnification;
};

// ================================================================================================================
// UR_CloudGather
// ================================================================================================================

/// UR_CloudGather's data on each cloud
struct CloudAtomData
{
	float radius {0.0f};
	float theta {0.0f};
	bool flashing {false};   ///< the cloud lit by its lightning
	float flashStart {0.0f}; ///< the atom age at the strike
	float height {0.0f};     ///< rand(HeightVaryAmount)
};

/// UR_CloudGather's collection data
struct CloudGatherData
{
	float emitted {0.0f};
	int count {0};
	bool first {true};
	float rate {0.0f};       ///< atoms per second
	float nextStrike {0.0f}; ///< collection age of the next lightning
	float strikeEnd {0.0f};  ///< collection age at which the strike ends
	float spin {1.0f};
	Collection* lightning {nullptr}; ///< the LightningGroup collection
	Atom* lightningCloud {nullptr};  ///< the cloud it hangs from while it strikes
	bool detached {false};           ///< the lightning collection is in no atom (then this data owns it)
	bool lightningOn {false};        ///< power-up level != -1
	bool rainOn {false};
	bool registered {false}; ///< this collection registers the storm
	glm::vec3 heading {0.0f};
	weather::storms::StormId storm {weather::storms::k_NoStorm}; ///< the weather storm
	std::unique_ptr<Collection> orphan; ///< the owner of the lightning collection while it hangs from no atom

	CloudGatherData() { TouchExitGuard(); }
	CloudGatherData(const CloudGatherData&) = delete;
	CloudGatherData& operator=(const CloudGatherData&) = delete;
	CloudGatherData(CloudGatherData&&) = delete;
	CloudGatherData& operator=(CloudGatherData&&) = delete;
	/// A detached lightning collection is deleted, and the storm is marked for deletion
	~CloudGatherData()
	{
		orphan.reset();
		if (storm != weather::storms::k_NoStorm && !g_Exiting)
		{
			weather::storms::MarkForDeletion(storm);
		}
	}
};

/// What this module keeps between calls (Locator::particleSystem)
struct StormRulesState
{
	/// The clouds of the current collection that are past half formed (shared by every gather, emptied at each call)
	std::vector<Atom*> clouds {};
	/// The objects the tornadoes carry now; never cleared (the ids outlive a land, as they always did). A set here, not
	/// a component on each object, so that those stale ids stay
	std::unordered_set<entt::entity> carried;
};

StormRulesState& StormRulesData()
{
	return openblack::Locator::particleSystem::value().Module<StormRulesState>();
}

/// Takes `collection` out of whichever cloud of `clouds` holds it (nullptr: none does)
std::unique_ptr<Collection> Detach(Collection& clouds, const Collection* collection)
{
	for (auto& atom : clouds.atoms)
	{
		auto& subs = atom->subCollections;
		const auto it =
		    std::find_if(subs.begin(), subs.end(), [collection](const auto& sub) { return sub.get() == collection; });
		if (it != subs.end())
		{
			auto detached = std::move(*it);
			subs.erase(it);
			detached->parent = nullptr;
			return detached;
		}
	}
	return nullptr;
}

/// The cloud of `clouds` that holds `collection`, or nullptr
Atom* HolderOf(Collection& clouds, const Collection* collection)
{
	for (auto& atom : clouds.atoms)
	{
		for (const auto& sub : atom->subCollections)
		{
			if (sub.get() == collection)
			{
				return atom.get();
			}
		}
	}
	return nullptr;
}

class CloudGather final: public Modifier
{
public:
	explicit CloudGather(const Object& object)
	    : creator(object.String("PCreator"))
	    , maxRadius(object.Float("MaxRadius", 100.0f)) // (read by no code)
	    , numAtoms(object.Int("NumAtoms", 20))
	    , maxAngularSpeed(object.Float("MaxAngularSpeed", 0.3f))
	    , timeToForm(object.Float("TimeToForm", 10.0f))
	    , fracToMaxSize(object.Float("FracToMaxSize", 0.5f))
	    , maxColor(object.Int("MaxColor", 255))
	    , minColor(object.Int("MinColor", 50))
	    , minScaleFactor(object.Float("MinScaleFactor", 0.0f))
	    , maxScaleFactor(object.Float("MaxScaleFactor", 1.0f))
	    , minAlpha(object.Int("MinAlpha", 100))
	    , maxAlpha(object.Int("MaxAlpha", 100))
	    , lightningGroup(object.Int("LightningGroup", -1))
	    , specLife(object.Float("SpecLife", 0.2f))
	    , lightningLife(object.Float("LightningLife", 0.5f))
	    , lightningDelay(object.Float("LightningDelay", 4.0f))
	    , switchLife(object.Float("SwitchLife", 0.5f))
	    , heightVaryAmount(object.Float("HeightVaryAmount", 0.0f))
	    , cloudRatioMaxCollection(object.Float("CloudRatioMaxCollection", 2.0f))
	    , collectionRadiusInitialScale(object.Float("CollectionRadiusInitialScale", 3.0f))
	    , maxCloudRatio(object.Float("MaxCloudRatio", 5.0f))
	    , minCloudRatio(object.Float("MinCloudRatio", 1.0f))
	    , createAllAtOnce(object.Bool("CreateAllAtOnce", false))
	    , tornadoGroup(object.Int("TornadoGroup", 10))
	    , windMaxSpeed(object.Float("WindMaxSpeed", 100.0f))
	    , windMinSpeed(object.Float("WindMinSpeed", 40.0f))
	    , magnitudeForWindMaxSpeed(object.Float("MagnitudeForWindMaxSpeed", 100.0f))
	    , magnitudeForWindMinSpeed(object.Float("MagnitudeForWindMinSpeed", 20.0f))
	    , radiusProvider(object.String("RadiusFloatProvider"))
	    , scaleProvider(object.String("ScaleFloatProvider"))
	    , cloudHeightProvider(object.String("CloudHeight"))
	    , soundLightning(ReadSoundAction(object, "SoundLightning"))
	{
	}

	[[nodiscard]] bool Creates() const override { return true; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		auto& clouds = StormRulesData().clouds;
		// the three float providers must exist
		if (radiusProvider.empty() || scaleProvider.empty() || cloudHeightProvider.empty())
		{
			return false;
		}
		auto& data = CollectionDataOf<CloudGatherData>(collection, this);
		// closing: the storm is marked for deletion and no longer registered
		if (effect.Closing())
		{
			if (data.storm != weather::storms::k_NoStorm)
			{
				weather::storms::MarkForDeletion(data.storm);
				data.storm = weather::storms::k_NoStorm;
			}
			data.registered = false;
		}
		const glm::vec3 parentPos = ParentPosition(effect, collection);
		const int level = effect.PowerUpLevel();
		const auto* particleCreator = effect.FindCreator(creator);
		const float dt = effect.GetDt();
		if (data.first)
		{
			if (particleCreator == nullptr)
			{
				return false;
			}
			data.lightningOn = level != -1;
			data.rainOn = true;
			data.registered = false;
			data.heading = CurrentHeading(effect);
			// only the gather under the first cloud core registers the storm (the parent's index in its collection is
			// 0), and at level 1 it gets the tornado too
			if (collection.parent != nullptr && collection.parent->collection != nullptr &&
			    !collection.parent->collection->atoms.empty() &&
			    collection.parent->collection->atoms.front().get() == collection.parent)
			{
				data.registered = true;
				if (level == 1)
				{
					effect.AddSubCollections(*collection.parent, {tornadoGroup});
				}
			}
			data.rate = static_cast<float>(numAtoms) / timeToForm;
			data.spin = effect.Random(1.0f) < 0.5f ? -1.0f : 1.0f;
			data.nextStrike = RandRange(effect, 0.5f, 1.0f) * switchLife + lightningDelay;
			if (createAllAtOnce)
			{
				CreateAllAtOnce(effect, collection, parentPos, particleCreator);
			}
		}
		// emit at the rate while the count is below the emitted amount and NumAtoms
		data.emitted += dt * data.rate;
		while (static_cast<float>(data.count) < data.emitted && static_cast<int>(collection.atoms.size()) < numAtoms &&
		       particleCreator != nullptr)
		{
			++data.count;
			auto& atom = effect.NewAtom(collection, particleCreator, {});
			InitCloud(effect, atom, AtomDataOf<CloudAtomData>(atom, this));
		}
		// the strike ends: the lightning collection is let go of its cloud; it stays with the
		// gather, not updated, until the next strike
		if (data.lightningCloud != nullptr && effect.CollectionAge(collection) > data.strikeEnd)
		{
			if (auto detached = Detach(collection, data.lightning); detached != nullptr)
			{
				data.orphan = std::move(detached);
			}
			data.lightningCloud = nullptr;
			data.detached = true;
		}
		// the collection's own ramps, over its first 10 s
		const float ratioScale = storm::CollectionScale(effect.CollectionAge(collection), cloudRatioMaxCollection);
		const float radiusScale = storm::CollectionScale(effect.CollectionAge(collection), collectionRadiusInitialScale);
		clouds.clear();
		const float scaleValue = effect.FloatProvider(scaleProvider, 0.0f);
		const float cloudHeight = effect.FloatProvider(cloudHeightProvider, 0.0f);
		const float invSpecLife = 1.0f / specLife;
		for (auto& atom : collection.atoms)
		{
			auto& ad = AtomDataOf<CloudAtomData>(*atom, this);
			float age = effect.AtomAge(*atom);
			// a cloud older than TimeToForm starts again (age 0)
			if (age > timeToForm)
			{
				InitCloud(effect, *atom, ad);
				age = effect.AtomAge(*atom);
			}
			const float f = age / timeToForm;
			// past half formed (the original also computes a bias(0.2, 2 - 2f) there and discards it)
			if (f > 0.5f)
			{
				clouds.push_back(atom.get());
			}
			// the angular speed peaks at half formed (x (1 - (2f - 1)^2))
			const float spin = data.spin * maxAngularSpeed;
			const float w = spin * (1.0f - (2.0f * f - 1.0f) * (2.0f * f - 1.0f));
			ad.theta += w * dt;
			ad.theta = std::fmod(ad.theta, 6.2831854820251465f); // 2 pi as a double
			// the radius wobbles by 1 + 0.3 cos(collection age x 0.1 + theta)
			const float wobble = 1.0f + 0.3f * std::cos(effect.CollectionAge(collection) * 0.1f + ad.theta);
			const float r = (1.0f - f) * ad.radius * wobble * radiusScale;
			glm::vec3 position(std::cos(ad.theta) * r + parentPos.x, parentPos.y, std::sin(ad.theta) * r + parentPos.z);
			const auto look = storm::CloudLookAt(f, {fracToMaxSize, minCloudRatio, maxCloudRatio, minColor, maxColor, minAlpha,
			                                         maxAlpha, minScaleFactor, maxScaleFactor, scaleValue});
			// the colour (alpha, c, c, c), the scale, and the ratio x the collection's ratio scale
			atom->colour = {static_cast<uint8_t>(look.colour), static_cast<uint8_t>(look.colour),
			                static_cast<uint8_t>(look.colour), static_cast<uint8_t>(look.alpha)};
			atom->ruleScale = look.scale;
			atom->stretch = look.ratio * ratioScale;
			// y = baseScale x height x ruleScale + the CloudHeight provider (the land is added below)
			position.y = atom->baseScale * ad.height * atom->ruleScale + cloudHeight;
			atom->position = position;
			// the lightning's light on its cloud, SpecLife long: specular (A, R, G, B) = (v, 200v/256, 200v/256, v) >> 8
			// with v = trunc((1 - t / SpecLife) x 255). The mist creator adds it to the vertices as specular light:
			// Creators/Mist.cpp
			if (ad.flashing && age - ad.flashStart > specLife)
			{
				ad.flashing = false;
				atom->specular = 0xFF000000u;
			}
			if (ad.flashing)
			{
				const auto v =
				    static_cast<uint32_t>(static_cast<int>((1.0f - (age - ad.flashStart) * invSpecLife) * 255.0f)) & 0xFFu;
				const uint32_t a = (v * 255u) >> 8u;
				const uint32_t rg = (v * 200u) >> 8u;
				atom->specular = (a << 24u) | (rg << 16u) | (rg << 8u) | a;
			}
		}
		// the next strike
		if (data.lightningOn && !clouds.empty() && effect.CollectionAge(collection) - data.nextStrike > 0.0f &&
		    !effect.Closing())
		{
			Strike(effect, collection, data);
		}
		// the clouds sit at the mean land altitude of a 3 x 3 grid of 0.33 x radius around the
		// core (the tornado's: at the manager's origin, the tornado's base)
		float height = effect.GetOrigin().y;
		if (level != 1)
		{
			const float r = effect.FloatProvider(radiusProvider, 0.0f) * 0.33f;
			height = 0.0f;
			for (int i = -1; i < 2; ++i)
			{
				for (int j = -1; j < 2; ++j)
				{
					height += LandAt(parentPos.x + static_cast<float>(i) * r, parentPos.z + static_cast<float>(j) * r);
				}
			}
			height *= 0.111111f; // 1 / 9
		}
		// every cloud up by that height
		for (auto& atom : collection.atoms)
		{
			atom->position.y += height;
		}
		// the weather storm, registered once by the first core's gather, kept alive at the core every step
		if (data.storm == weather::storms::k_NoStorm && data.registered)
		{
			storm::GatherStormInput input {
			    .radius = effect.FloatProvider(radiusProvider, 0.0f),
			    .magnitude = effect.GetMagnitude(),
			    .power = effect.GetProcessInfo().power,
			    .heading = data.heading,
			    .rainAmount = RainAmountOf(effect),
			    .rainOn = data.rainOn,
			    .timeToForm = timeToForm,
			    .cloudHeight = cloudHeight,
			    .windMinSpeed = windMinSpeed,
			    .windMaxSpeed = windMaxSpeed,
			    .magnitudeForWindMinSpeed = magnitudeForWindMinSpeed,
			    .magnitudeForWindMaxSpeed = magnitudeForWindMaxSpeed,
			};
			data.storm = weather::storms::Create(storm::GatherStormDescriptor(input));
			if (storm::TraceEnabled())
			{
				const auto* created = weather::storms::Find(data.storm);
				SPDLOG_LOGGER_INFO(spdlog::get("game"),
				                   "Storm: gather registers storm {} at ({:.1f}, {:.1f}): inner {:.1f} outer {:.1f} rain {} "
				                   "overcast {} wind ({}, {}) fade in {:.1f} s, level {}",
				                   data.storm, parentPos.x, parentPos.z, created->descriptor.innerRadius,
				                   created->descriptor.outerRadius, created->descriptor.weather.rain,
				                   created->descriptor.weather.overcast, created->descriptor.weather.windX,
				                   created->descriptor.weather.windZ, created->descriptor.fadeInTime, level);
			}
		}
		if (data.storm != weather::storms::k_NoStorm)
		{
			// gone or marked -> none (and the next step registers a new one); else it follows the core
			auto* registeredStorm = weather::storms::Find(data.storm);
			if (registeredStorm == nullptr)
			{
				data.storm = weather::storms::k_NoStorm;
			}
			else
			{
				registeredStorm->descriptor.position = parentPos;
			}
		}
		data.first = false;
		return true;
	}

private:
	/// The storm spell's rain amount; < 0 without one
	static float RainAmountOf(const Effect& effect)
	{
		const auto spell = SpellOf(effect);
		auto& registry = Locator::entitiesRegistry::value();
		if (spell == entt::null || !registry.Valid(spell) || !Locator::infoConstants::has_value())
		{
			return -1.0f;
		}
		const auto& component = registry.Get<const ecs::components::Spell>(spell);
		if (component.spellClass != ecs::components::SpellClass::StormAndTornado)
		{
			return -1.0f; // not a storm spell
		}
		const auto* info =
		    magic::GetMagicInfoAs<GMagicStormAndTornadoInfo>(Locator::infoConstants::value(), component.magicType);
		return info != nullptr ? info->rainAmount : -1.0f;
	}

	/// A cloud's radius, angle, age 0, no flash, its height variation
	void InitCloud(Effect& effect, Atom& atom, CloudAtomData& ad) const
	{
		// rand(0.7, 1.0) + 0.7, times the Radius provider: 1.4..1.7 x R
		ad.radius = (RandRange(effect, 0.7f, 1.0f) + 0.7f) * effect.FloatProvider(radiusProvider, 0.0f);
		ad.theta = effect.Random(k_TwoPi);
		atom.birth = effect.GetAge(); // the atom's age is 0
		ad.flashing = false;
		ad.height = effect.Random(heightVaryAmount);
		atom.specular = 0xFF000000u;
	}

	/// NumAtoms clouds at once, each at a random age up to TimeToForm
	void CreateAllAtOnce(Effect& effect, Collection& collection, const glm::vec3& /*parentPos*/,
	                     const Creator* particleCreator) const
	{
		auto& data = CollectionDataOf<CloudGatherData>(collection, this);
		for (int i = 0; i < numAtoms; ++i)
		{
			++data.count;
			auto& atom = effect.NewAtom(collection, particleCreator, {});
			InitCloud(effect, atom, AtomDataOf<CloudAtomData>(atom, this));
			atom.birth = effect.GetAge() - effect.Random(timeToForm);
		}
	}

	/// A strike from a random formed cloud
	void Strike(Effect& effect, Collection& collection, CloudGatherData& data) const
	{
		auto& clouds = StormRulesData().clouds;
		// the next one in rand(0.5, 1) x SwitchLife / max(TribalPower, 1)
		float interval = RandRange(effect, 0.5f, 1.0f) * switchLife;
		const auto spell = SpellOf(effect);
		if (spell != entt::null)
		{
			interval /= std::max(magic::GetTribalPower(spell), 1.0f);
		}
		data.nextStrike = effect.CollectionAge(collection) + interval;
		// a random formed cloud
		const auto index = static_cast<size_t>(effect.Rand(static_cast<int32_t>(clouds.size())));
		Atom* cloud = clouds[index];
		data.lightningCloud = nullptr;
		if (data.lightning == nullptr)
		{
			// the first strike: AddSubCollection(LightningGroup) on the cloud; its first sub-collection is the new one
			if (lightningGroup < 0)
			{
				return;
			}
			effect.AddSubCollections(*cloud, {lightningGroup});
			if (cloud->subCollections.empty())
			{
				return;
			}
			data.lightning = cloud->subCollections.back().get();
			data.detached = false;
		}
		// the lightning collection moves to this cloud
		std::unique_ptr<Collection> moving;
		if (data.orphan != nullptr && data.orphan.get() == data.lightning)
		{
			moving = std::move(data.orphan);
		}
		else if (Atom* owner = HolderOf(collection, data.lightning); owner == nullptr)
		{
			data.lightning = nullptr; // its cloud went (openblack guard; the original keeps the stale pointer)
			return;
		}
		else if (owner != cloud)
		{
			moving = Detach(collection, data.lightning);
		}
		if (moving != nullptr)
		{
			moving->parent = cloud;
			cloud->subCollections.push_back(std::move(moving));
		}
		// the cloud's sounds stop, then the thunder with a random size class (< 0.33 -> 3, < 0.66 -> 2, else 1) and
		// flags |= 0x22 (delayed by the distance, snapped to the ground)
		audio::spell_sounds::StopAllSounds(*cloud);
		auto thunder = soundLightning;
		const float r = effect.Random(1.0f);
		thunder.size = r < 0.33f ? 3 : (r < 0.66f ? 2 : 1);
		thunder.flags |= 0x22u;
		audio::spell_sounds::StartSound(effect, *cloud, thunder);
		data.lightningCloud = cloud;
		data.detached = false;
		++StormRulesDebugHooksData().strikes;
		data.strikeEnd = effect.CollectionAge(collection) + RandRange(effect, 0.5f, 1.0f) * lightningLife;
		auto& ad = AtomDataOf<CloudAtomData>(*cloud, this);
		ad.flashing = true;
		ad.flashStart = effect.AtomAge(*cloud);
		if (storm::TraceEnabled())
		{
			SPDLOG_LOGGER_INFO(
			    spdlog::get("game"),
			    "Storm: lightning from cloud {} of {} at ({:.1f}, {:.1f}, {:.1f}), next in {:.2f} s, life {:.2f} s", index,
			    clouds.size(), cloud->position.x, cloud->position.y, cloud->position.z, interval,
			    data.strikeEnd - effect.CollectionAge(collection));
		}
	}

	std::string creator;
	float maxRadius;
	int numAtoms;
	float maxAngularSpeed;
	float timeToForm;
	float fracToMaxSize;
	int maxColor, minColor;
	float minScaleFactor, maxScaleFactor;
	int minAlpha, maxAlpha;
	int lightningGroup;
	float specLife, lightningLife, lightningDelay, switchLife;
	float heightVaryAmount;
	float cloudRatioMaxCollection, collectionRadiusInitialScale;
	float maxCloudRatio, minCloudRatio;
	bool createAllAtOnce;
	int tornadoGroup;
	float windMaxSpeed, windMinSpeed, magnitudeForWindMaxSpeed, magnitudeForWindMinSpeed;
	std::string radiusProvider, scaleProvider, cloudHeightProvider;
	SoundAction soundLightning;
};

// ================================================================================================================
// UR_Tornado
// ================================================================================================================

/// UR_Tornado's collection data. The original keeps the one being processed and its collection in statics.
struct TornadoData
{
	bool first {true};
	glm::vec3 base {0.0f};
	glm::vec3 top {0.0f};
	glm::vec3 baseVelocity {0.0f};
	glm::vec3 topVelocity {0.0f};
	float tornadoScale {1.0f}; ///< the TornadoScale provider (1 without one)
	float strength {1.0f};     ///< 1 every step
	uint8_t alpha {0};         ///< the collections' alpha
	float fade {1.0f};         ///< the close-down fade
	bool closing {false};
};

/// UR_Tornado's data on each flying atom
struct FlyingAtomData
{
	int state {2};      ///< 0 a game object, 1 a pretend object, 2 a funnel sprite
	float blend {0.0f}; ///< the height fraction it rises to (x 0.1 per second)
	float speed {0.0f}; ///< rand(1) + 0.5
};

/// UR_Tornado's debris and flying collection data: the atoms made and to make
struct EmitterData
{
	float made {0.0f};
	float owed {0.0f};
};

/// The game objects the tornados carry: the atom's creator here, one per atom, kept in the atom's data so that it goes
/// with the atom
struct TornadoCarrier final: Creator
{
	entt::entity object {entt::null};
	std::optional<PlayerNames> player; ///< the manager's player, if it has one
	bool killOnRelease {true};
	bool released {false};

	TornadoCarrier() { TouchExitGuard(); }
	TornadoCarrier(const TornadoCarrier&) = delete;
	TornadoCarrier& operator=(const TornadoCarrier&) = delete;
	TornadoCarrier(TornadoCarrier&&) = delete;
	TornadoCarrier& operator=(TornadoCarrier&&) = delete;
	~TornadoCarrier() override;
	/// Lets the object go (the destructor, when the atom goes)
	void Release(const glm::vec3& position);
	/// where the atom was last seen (each tornado step, and each frame by UpdateCarriedObjects)
	mutable glm::vec3 lastPosition {0.0f};
};

/// The objects some tornado carries (never destroyed: see g_Exiting)
std::unordered_set<entt::entity>& Carried()
{
	return StormRulesData().carried;
}
/// The key of an atom's TornadoCarrier in its data (the FlyingAtomData is under the tornado rule itself)
const char k_CarrierKeyByte = 0;
const auto* const k_CarrierKey = reinterpret_cast<const Modifier*>(&k_CarrierKeyByte);

bool IsLiving(entt::entity object)
{
	return Locator::entitiesRegistry::value().AnyOf<ecs::components::Villager, ecs::components::Animal>(object);
}

void TornadoCarrier::Release(const glm::vec3& position)
{
	if (released)
	{
		return;
	}
	released = true;
	if (g_Exiting)
	{
		return;
	}
	Carried().erase(object);
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	// not available (a dying villager too): only let go (the erase above), no kill or deletion
	if (!ecs::physics::particle_carried_objects::IsAvailable(object))
	{
		return;
	}
	if (killOnRelease && IsLiving(object))
	{
		// physics::particle_carried_objects::Release: back on the map where the atom is (its angles from the matrix, altitude
		// 0), physics ended, the carried mark cleared; then it is destroyed by the effect (player, 1.0): a villager or an
		// animal dies there. The object's matrix is the atom's (UpdateCarriedObjects)
		if (const auto* transform = registry.TryGet<const ecs::components::Transform>(object); transform != nullptr)
		{
			ecs::physics::particle_carried_objects::Release(object, transform->rotation, position);
		}
		// (openblack, guard) the Living's EndPhysics may have deleted it (off the map)
		if (!registry.Valid(object))
		{
			return;
		}
		if (registry.AllOf<ecs::components::Villager>(object))
		{
			// the manager's player, 1.0: the villager dies by a spell
			ecs::villager::DestroyedByEffect(object, player, 1.0f);
		}
		else
		{
			ecs::animal_ai::Kill(object); // the animal dies (a spell animal fades)
		}
		return;
	}
	// anything else the tornado took is deleted
	if (registry.AllOf<ecs::components::Tree>(object))
	{
		ecs::DeleteTree(object);
		return;
	}
	ecs::fire::traits::DestroyedByEffect(object); // deleted
}

TornadoCarrier::~TornadoCarrier()
{
	Release(lastPosition);
}

/// The terrain material at a point: the second material of the cell's altitude in its country (the reading of
/// Audio/Services/SoundMap.cpp), and its tornado dust colour as 0xFFRRGGBB
uint32_t TornadoDustColour(const glm::vec3& point)
{
	if (!Locator::terrainSystem::has_value() || !Locator::infoConstants::has_value())
	{
		return 0xFFFFFFFFu;
	}
	const auto& island = Locator::terrainSystem::value();
	const auto& countries = island.GetCountries();
	const auto& materials = island.GetMaterialInfo();
	// the cell of the tornado's position (openblack keeps the check against the island's side: the original passes it
	// on unchecked)
	const auto dustCell = map_coords::CellOf(point);
	if (!map_coords::InBounds(dustCell, static_cast<uint32_t>(island.GetCellsPerSide())))
	{
		return 0xFFFFFFFFu;
	}
	const auto& cell = island.GetCell(glm::u16vec2(dustCell.x, dustCell.y));
	if (cell.properties.country >= countries.size())
	{
		return 0xFFFFFFFFu;
	}
	const auto altitude = std::min<uint16_t>(island.GetCellAltitude(cell), 255);
	const auto material = countries[cell.properties.country].materials[altitude].indices[1];
	if (material >= materials.size())
	{
		return 0xFFFFFFFFu;
	}
	const auto& info = Locator::infoConstants::value().terrainMaterial;
	const auto type = materials[material].type;
	if (type >= info.size())
	{
		return 0xFFFFFFFFu;
	}
	const auto& c = info[type].tornadoDustColorRGB;
	return 0xFF000000u | ((c.x & 0xFFu) << 16u) | ((c.y & 0xFFu) << 8u) | (c.z & 0xFFu);
}

/// The objects of one 10 m cell in the tornado's walk order: the fixed list first, then the mobile one, each from its
/// head (ecs::map_cells). The pots and piles (type 21, counted as fixed) are at the tail of the fixed list. The caller
/// has already checked map_coords::InBounds
void CellObjects(const glm::ivec2& cell, std::vector<entt::entity>& out)
{
	out = ecs::map_cells::ObjectsInCell(cell);
}

class Tornado final: public Modifier
{
public:
	explicit Tornado(const Object& object)
	    : creator(object.String("PCreator"))
	    , topHeight(object.Float("TopHeight", 100.0f))
	    , wiggleCount(object.Int("WiggleCount", 5))
	    , wiggleAmplitude(object.Float("WiggleAmplitude", 10.0f))
	    , fadeOutTime(object.Float("FadeOutTime", 10.0f))
	    , fadeInTime(object.Float("FadeInTime", 4.0f))
	    , baseRadius(object.Float("BaseRadius", 3.0f))
	    , topRadius(object.Float("TopRadius", 50.0f))
	    , baseScale(object.Float("BaseScale", 1.0f))
	    , topScale(object.Float("TopScale", 10.0f))
	    , baseThetaDot(object.Float("BaseThetaDot", 15.0f))
	    , topThetaDot(object.Float("TopThetaDot", 2.0f))
	    , meshThetaDot(object.Float("MeshThetaDot", 2.0f))
	    , thetaBias(object.Float("ThetaBias", 0.5f))
	    , funnelBend(object.Float("FunnelBendParameter", 0.5f))
	    , topMoveFreq(object.Float("TopMoveFreq", 0.2f))
	    , topMoveAmp(object.Float("TopMoveAmp", 40.0f))
	    , groupFlying(object.Int("GroupFlying", -1))
	    , groupDebris(object.Int("GroupDebris", -1))
	    , groupMesh(object.Int("GroupMesh", -1))
	    , groupOnceDone(object.Int("GroupToMoveToOnceDone", -1))
	    , groupOnCloseDown(object.Int("GroupToMoveToOnCloseDown", -1))
	    , showVelocityField(object.Bool("ShowVelocityField", true))
	    , scaleProvider(object.String("TornadoScaleFloatProvider"))
	    , delayBeforeMove(object.Float("DelayBeforeMove", 5.0f))
	    , damping(object.Float("Damping", 0.1f))
	    , debrisRadius(object.Float("DebrisRadius", 10.0f))
	    , debrisHeight(object.Float("DebrisHeight", 10.0f))
	    , debrisSpeed(object.Float("DebrisSpeed", 10.0f))
	    , debrisEmitRate(object.Float("DebrisEmitRate", 2.0f))
	    , debrisSpreadAngle(object.Float("DebrisSpreadAngle", 1.0367f))
	    , pretendRadius(object.Float("PretendRadius", 10.0f))
	    , pretendSpeed(object.Float("PretendSpeed", 10.0f))
	    , pretendEmitRate(object.Float("PretendEmitRate", 2.0f))
	    , pretendSpreadAngle(object.Float("PretendSpreadAngle", 1.0367f))
	    , pretendGravity(object.Float("PretendGravity", 10.0f))
	    , pretendBlendTime(object.Float("PretendBlendTime", 4.0f))
	    , resourceMin(object.Float("ResourceAmountRemoveMin", 150.0f))
	    , resourceMax(object.Float("ResourceAmountRemoveMax", 650.0f))
	    , debrisCreator(object.String("DebrisSpriteCreator"))
	    , pretendSmall(object.String("PretendObjectCreatorSmall"))
	    , pretendMedium(object.String("PretendObjectCreatorMedium"))
	    , pretendLarge(object.String("PretendObjectCreatorLarge"))
	    , soundTornado(ReadSoundAction(object, "SoundTornado"))
	{
		// read by no code of the original: PretendHeight, MaxSearchDistance, MaxSearchDistanceWhenNoTargets,
		// LocalSearchDistance, PauseBeforeAffectsGameObjects, UseTornadoStrength, K1_Accn, ScaleWhipUp. SpriteCreator
		// with NumAtomsToCreate makes funnel sprites on the first call; the only file with UR_Tornado,
		// SF_LightningStormPush, has 0 of them, so that is not ported.
		numAtomsToCreate = object.Int("NumAtomsToCreate", 100);
	}

	[[nodiscard]] bool KeepsAlive() const override { return true; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		auto& data = CollectionDataOf<TornadoData>(collection, this);
		// the TornadoScale provider (1 without one) and strength 1
		data.tornadoScale = scaleProvider.empty() ? 1.0f : effect.FloatProvider(scaleProvider, 1.0f);
		data.strength = 1.0f;
		UpdateBaseAndTopPoints(effect, collection, data);
		if (data.first)
		{
			const auto* pointCreator = effect.FindCreator(creator);
			if (pointCreator == nullptr)
			{
				return false;
			}
			// the funnel's atom, with the flying, debris and mesh groups under it, and the loop
			auto& atom = effect.NewAtom(collection, pointCreator, {groupFlying, groupDebris, groupMesh});
			audio::spell_sounds::StartSound(effect, atom, soundTornado);
			atom.position = data.base;
		}
		// the parent (the tornado root) and the funnel's atom sit at the base
		if (collection.parent != nullptr)
		{
			collection.parent->position = data.base;
		}
		if (!collection.atoms.empty())
		{
			auto& funnel = *collection.atoms.front();
			funnel.position = data.base;
			for (auto& sub : funnel.subCollections)
			{
				if (sub->group == groupFlying)
				{
					UpdateFlyingAtoms(effect, *sub, data, effect.CollectionAge(collection));
				}
				else if (sub->group == groupMesh)
				{
					UpdateMeshAtoms(effect, *sub, data);
				}
				else if (sub->group == groupDebris)
				{
					UpdateDebrisAtoms(effect, *sub, data);
				}
			}
		}
		data.first = false;
		return true;
	}

private:
	/// The fade, the top and the wandering base, their velocities, and the spell event at the base
	void UpdateBaseAndTopPoints(Effect& effect, Collection& collection, TornadoData& data) const
	{
		data.fade = 1.0f;
		data.closing = data.closing || effect.Closing();
		if (data.closing)
		{
			const float t = effect.GetAge() - effect.GetCloseAge();
			data.fade = t < 0.0f ? 1.0f : (t > fadeOutTime ? 0.0f : 1.0f - t / fadeOutTime);
		}
		const float in = Clamp01(effect.CollectionAge(collection) / fadeInTime);
		const float a = Clamp01(in < data.fade ? in : data.fade);
		data.alpha = static_cast<uint8_t>(static_cast<int>(a * 255.0f));
		if (data.first)
		{
			data.top = effect.GetOrigin();
			data.base = data.top;
		}
		const glm::vec3 oldBase = data.base;
		const glm::vec3 oldTop = data.top;
		// after DelayBeforeMove the top follows the parent (the tornado root, which UR_FollowParent keeps on the core)
		if (!(effect.CollectionAge(collection) < delayBeforeMove))
		{
			data.top = ParentPosition(effect, collection);
		}
		data.base = data.top;
		// the base wanders round the top: SignedValueNoise at n, 1.3n, 2n, 2.6n with n = age x TopMoveFreq
		const float n = effect.CollectionAge(collection) * topMoveFreq;
		const float n1 = noise::SignedValueNoise(n);
		const float n13 = noise::SignedValueNoise(n * 1.3f);
		const float n2 = noise::SignedValueNoise(n + n);
		const float n26 = noise::SignedValueNoise(n * 2.6f);
		const float amplitude = data.tornadoScale * topMoveAmp;
		data.base.x += (n1 + 0.5f * n2) * amplitude;
		data.base.z += (n13 + 0.5f * n26) * amplitude;
		data.base.y = LandAt(data.base.x, data.base.z);
		effect.SetOrigin(data.base);
		data.top.y = data.base.y + data.tornadoScale * topHeight;
		if (data.first)
		{
			data.baseVelocity = glm::vec3(0.0f);
			data.topVelocity = glm::vec3(0.0f);
		}
		else
		{
			const float invDt = 1.0f / effect.GetDt();
			data.baseVelocity = (data.base - oldBase) * invDt;
			data.topVelocity = (data.top - oldTop) * invDt;
		}
		// every step while not closing: SpellEvent 2 at the base with its velocity (strength 1, no shields, no target)
		if (!data.closing)
		{
			const SpellEventInfo event {.type = SpellEventInfo::Type::Point,
			                            .position = data.base,
			                            .velocity = data.baseVelocity,
			                            .strength = 1.0f,
			                            .checkShields = false,
			                            .target = entt::null};
			effect.SendSpellEvent(event);
		}
	}

	[[nodiscard]] float Radius(float h, const TornadoData& data) const
	{
		return storm::FunnelRadius(h, baseRadius, topRadius, data.tornadoScale);
	}

	/// The sprites' scale at h, (BaseScale + (TopScale - BaseScale) h^2) x tornadoScale
	[[nodiscard]] float SpriteScale(float h, const TornadoData& data) const
	{
		h = Clamp01(h);
		return (baseScale + (topScale - baseScale) * h * h) * data.tornadoScale;
	}

	/// The angular speed at h, lerp(BaseThetaDot, TopThetaDot, bias(ThetaBias, h)) x speed x strength
	/// (it also writes ThetaBias clamped to 0..1 back into the rule)
	[[nodiscard]] float ThetaDot(float h, const FlyingAtomData& ad, const TornadoData& data) const
	{
		const float bias = Clamp01(thetaBias);
		return (baseThetaDot + (topThetaDot - baseThetaDot) * storm::Bias(bias, h)) * ad.speed * data.strength;
	}

	/// The funnel's centre line at h: y from base to top, x and z by gain(FunnelBend, h), plus the wiggle
	/// h x WiggleCount x pi, of tornadoScale x WiggleAmplitude
	[[nodiscard]] glm::vec3 Centre(float h, const TornadoData& data) const
	{
		h = h > 0.0f ? (h < 1.0f ? h : 1.0f) : 0.0f;
		glm::vec3 out;
		out.y = data.base.y + (data.top.y - data.base.y) * h;
		const float g = storm::Gain(funnelBend, h);
		out.x = data.base.x + (data.top.x - data.base.x) * g;
		out.z = data.base.z + (data.top.z - data.base.z) * g;
		const float angle = h * static_cast<float>(wiggleCount) * 3.14159f;
		const float amplitude = data.tornadoScale * wiggleAmplitude;
		out.x += std::cos(angle) * amplitude;
		out.z += std::sin(angle) * amplitude;
		return out;
	}

	/// The funnel meshes at the base, the collection alpha, scaled, each spinning (1 + i x 0.13) times
	/// faster
	void UpdateMeshAtoms(Effect& effect, Collection& collection, const TornadoData& data) const
	{
		collection.alpha = data.alpha;
		const float spin = data.strength * meshThetaDot;
		int i = 0;
		for (auto& atom : collection.atoms)
		{
			// every axis turned about Y, x' = c x - s z, z' = s x + c z
			const float angle = (static_cast<float>(i) * 0.13f + 1.0f) * effect.GetDt() * spin;
			const float c = std::cos(angle);
			const float s = std::sin(angle);
			for (int k = 0; k < 3; ++k)
			{
				auto& axis = atom->rotation[k];
				const float x = axis.x;
				axis.x = c * x - s * axis.z;
				axis.z = s * x + c * axis.z;
			}
			atom->position = data.base;
			atom->ruleScale = data.tornadoScale;
			++i;
		}
	}

	/// The velocity a debris or pretend object is thrown out with: baseVelocity + tornadoScale x speed x
	/// (cos th sin ph, cos ph, sin th sin ph), ph = rand(spread), th = rand(2 pi), speed = rand(0.33, 0.66) x the rule's
	glm::vec3 ThrowVelocity(Effect& effect, const TornadoData& data, float spread, float speed) const
	{
		const float phi = effect.Random(spread);
		const float theta = effect.Random(k_TwoPi);
		const float v = RandRange(effect, 0.33f, 0.66f) * speed;
		const float s = data.tornadoScale;
		return data.baseVelocity + glm::vec3(s * std::cos(theta) * v * std::sin(phi), s * v * std::cos(phi),
		                                     s * std::sin(theta) * v * std::sin(phi));
	}

	/// Dust in the terrain's tornado colour, DebrisEmitRate per second, at most 50 alive
	void UpdateDebrisAtoms(Effect& effect, Collection& collection, const TornadoData& data) const
	{
		const auto* debris = effect.FindCreator(debrisCreator);
		if (debris == nullptr)
		{
			return;
		}
		collection.alpha = data.alpha;
		auto& emitter = CollectionDataOf<EmitterData>(collection, this);
		emitter.owed += effect.GetDt() * debrisEmitRate;
		if (data.closing)
		{
			return;
		}
		const uint32_t dust = TornadoDustColour(data.base);
		while (emitter.owed > emitter.made && collection.atoms.size() < 50)
		{
			emitter.made += 1.0f;
			auto& atom = effect.NewAtom(collection, debris, {});
			// each channel of the creator's colour x the dust's >> 8, the alpha kept
			const auto channel = [](uint8_t c, uint32_t d) { return static_cast<uint8_t>((c * (d & 0xFFu)) >> 8u); };
			atom.colour = {channel(atom.colour[0], dust >> 16u), channel(atom.colour[1], dust >> 8u),
			               channel(atom.colour[2], dust), atom.colour[3]};
			const float r1 = RandRange(effect, -debrisRadius, debrisRadius);
			const float ry = effect.Random(debrisHeight);
			const float r3 = RandRange(effect, -debrisRadius, debrisRadius);
			const float s = data.tornadoScale;
			atom.position = data.base + glm::vec3(s * r3, s * ry, s * r1);
			atom.baseScale *= data.tornadoScale;
			atom.velocity = ThrowVelocity(effect, data, debrisSpreadAngle, debrisSpeed);
		}
	}

	/// The pretend objects (chickens and bushes), PretendEmitRate x tornadoScale per second while the
	/// base is on dry land and the tornado fully faded in (alpha > 250)
	void EmitPretendObjects(Effect& effect, Collection& collection, const TornadoData& data) const
	{
		const auto* small = effect.FindCreator(pretendSmall);
		const auto* medium = effect.FindCreator(pretendMedium);
		const auto* large = effect.FindCreator(pretendLarge);
		if (small == nullptr || medium == nullptr || large == nullptr)
		{
			return;
		}
		auto& emitter = CollectionDataOf<EmitterData>(collection, this);
		if (data.closing || data.alpha <= 0xFA)
		{
			return;
		}
		const float rate = ecs::pot_resource::IsDryLand(data.base) ? data.tornadoScale * pretendEmitRate : 0.0f;
		emitter.owed += rate * effect.GetDt();
		while (emitter.owed > emitter.made)
		{
			emitter.made += 1.0f;
			const float r = effect.Random(1.0f);
			const auto* meshCreator = r < 0.33f ? small : (r < 0.66f ? medium : large);
			auto& atom = effect.NewAtom(collection, meshCreator, {});
			const float r1 = RandRange(effect, -pretendRadius, pretendRadius);
			const float r2 = RandRange(effect, -pretendRadius, pretendRadius);
			const float s = data.tornadoScale;
			atom.position = data.base + glm::vec3(s * r2, 0.0f, s * r1);
			if (data.tornadoScale < 1.0f)
			{
				atom.baseScale *= data.tornadoScale;
			}
			atom.velocity = ThrowVelocity(effect, data, pretendSpreadAngle, pretendSpeed);
			auto& ad = AtomDataOf<FlyingAtomData>(atom, this);
			ad.speed = effect.Random(1.0f) + 0.5f;
			ad.blend = 1.0f;
			ad.state = 1;
		}
	}

	/// The tornado can take it whole: it can be a physics object, has a 3D object, and fits the funnel
	/// (2 r(0) > its 2D radius and r(1) > it)
	bool CanSuckUp(entt::entity object, const TornadoData& data) const
	{
		auto& registry = Locator::entitiesRegistry::value();
		const float radius = ecs::object::Get2DRadius(object);
		// a pot or pile can become a physics object when its pot info says so; the other classes as the physics have
		// them
		if (const auto* pot = registry.TryGet<const ecs::components::Pot>(object); pot != nullptr)
		{
			const auto& pots = Locator::infoConstants::value().pot;
			const auto index = static_cast<size_t>(pot->type);
			if (index >= pots.size() || pots[index].canBecomeAPhysicsObject == 0)
			{
				return false;
			}
		}
		else if (!ecs::physics::PhysicsObjects::CanBecomeAPhysicsObject(object))
		{
			return false;
		}
		// one object flag of the original (not identified) is not checked (inferred: no openblack state behind it)
		if (!registry.AllOf<ecs::components::Mesh>(object))
		{
			return false;
		}
		return Radius(0.0f, data) + Radius(0.0f, data) > radius && Radius(1.0f, data) > radius;
	}

	/// One object per call, every third game turn once the tornado has faded in
	void PickUp(Effect& effect, Collection& flying, const TornadoData& data, float tornadoAge) const
	{
		// the tornado collection's age; the game turn % 3
		if (tornadoAge < fadeInTime || data.closing || magic::CurrentTurn() % 3 != 0)
		{
			return;
		}
		float reach = (Radius(0.0f, data) + Radius(1.0f, data)) * 0.5f;
		reach += reach;
		const auto spell = SpellOf(effect);
		if (spell != entt::null)
		{
			reach *= std::clamp(magic::GetTribalPower(spell), 1.0f, 5.0f);
		}
		auto& registry = Locator::entitiesRegistry::value();
		// the tornado's map coordinates, and a copy of them walked in a spiral from dir = count = 1, checking each cell
		// is on the map
		const auto start = map_coords::FromMetres(glm::vec2(data.base.x, data.base.z));
		const int side = map_coords::FtoL(std::ceil(reach / 10.0f)) + 2;
		const int cells = side * side;
		auto cell = start;
		map_coords::Spiral spiral;
		std::vector<entt::entity> objects;
		entt::entity taken = entt::null;
		for (int i = 0; i < cells && taken == entt::null; ++i)
		{
			if (!map_coords::InBounds(cell))
			{
				map_coords::AddCells(cell, spiral.Next());
				continue;
			}
			CellObjects(map_coords::Cell(cell), objects);
			for (const auto object : objects)
			{
				if (taken != entt::null)
				{
					break;
				}
				if (!registry.Valid(object) || Carried().contains(object) ||
				    !registry.AllOf<ecs::components::Transform, ecs::components::Mesh>(object))
				{
					continue;
				}
				// the object's map coordinates
				const auto own = ecs::object::MapCoordsOf(object);
				// the object's own cell is this one (a fixed object in several cells is seen once)
				if (!ecs::map_cells::IsOwnCell(own, map_coords::Cell(cell)))
				{
					continue;
				}
				// the distance from the tornado's map coordinates against the object's 2D radius + reach
				const float reachOfObject = ecs::object::Get2DRadius(object) + reach;
				if (!(gutils::GetDistanceInMetres(start, own) < reachOfObject))
				{
					continue;
				}
				if (CanSuckUp(object, data))
				{
					// a can-destroy spell event at the object, strength 1, the object as the target
					const SpellEventInfo event {.type = SpellEventInfo::Type::CanDestroy,
					                            .position = map_coords::ToWorld(own), // the map coordinates as a point
					                            .velocity = glm::vec3(0.0f),
					                            .strength = 1.0f,
					                            .checkShields = false,
					                            .target = object};
					if (effect.SendSpellEvent(event) == 1)
					{
						taken = object;
					}
				}
				else if (ecs::object_resources::IsPileResource(object)) // ECS/ObjectResources.h
				{
					taken = SplitPile(effect, object, data);
				}
				// a creature has its own handling (no creature in openblack)
			}
			map_coords::AddCells(cell, spiral.Next());
		}
		if (taken != entt::null)
		{
			Carry(effect, flying, taken);
		}
	}

	/// A pile gives lerp(ResourceAmountRemoveMin, Max, clamp(tornadoScale, 0, 1)) (rounded) as a new pile of its kind,
	/// scaled by rand(0.7, 1.2) x clamp(tornadoScale, 0.2, 1)
	entt::entity SplitPile(Effect& effect, entt::entity pile, const TornadoData& data) const
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto& pot = registry.Get<ecs::components::Pot>(pile);
		const float k = Clamp01(data.tornadoScale);
		auto amount = static_cast<uint32_t>(std::lrint(resourceMin + (resourceMax - resourceMin) * k));
		// at most the pile's resource (a store pile answers with its store's total, as the hand reads it)
		const auto store = ecs::StoragePitStore::OwnerOf(pile);
		const auto& pots = Locator::infoConstants::value().pot;
		const auto resource = pots[static_cast<size_t>(pot.type)].resourceType;
		const uint32_t available = store != entt::null ? ecs::StoragePitStore::GetResource(store, resource) : pot.amount;
		amount = std::min<uint32_t>(amount, available);
		if (amount == 0)
		{
			return entt::null;
		}
		// removed from the store, or from the pile (approximate: the amount and the sink offset, as HandHolding does;
		// the pile's own empty handling is not ported)
		if (store != entt::null)
		{
			ecs::StoragePitStore::RemoveResource(store, resource, amount);
		}
		else
		{
			pot.amount = static_cast<uint16_t>(pot.amount - amount);
			ecs::archetypes::PotArchetype::SetSize(pile, true);
		}
		// a new pot of the hand's own pile kind (food: HAND_FOOD, wood: HAND_WOOD), at the pile's position
		const auto position = registry.Get<const ecs::components::Transform>(pile).position;
		const auto handType = resource == ResourceType::Wood ? PotInfo::HandWood : PotInfo::HandFood;
		const auto piece = ecs::archetypes::PotArchetype::Create(position, 0.0f, handType, static_cast<int32_t>(amount));
		if (piece == entt::null)
		{
			return entt::null;
		}
		const float s = data.tornadoScale < 0.2f ? 0.2f : (data.tornadoScale < 1.0f ? data.tornadoScale : 1.0f);
		auto& transform = registry.Get<ecs::components::Transform>(piece);
		transform.scale *= RandRange(effect, 0.7f, 1.2f) * s;
		// into its cell at once, as on creation
		ecs::map_cells::InsertMapObject(piece);
		return piece;
	}

	/// A flying atom that carries the object, from the object's world matrix, rising to rand(0.7) of the funnel's
	/// height
	void Carry(Effect& effect, Collection& flying, entt::entity object) const
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto carried = std::make_shared<TornadoCarrier>();
		carried->kind = Creator::Kind::Other;
		carried->className = "RenderParticleGameObject";
		carried->object = object;
		int player = 0;
		if (effect.GetSink() != nullptr && effect.GetSink()->Player(player))
		{
			carried->player = static_cast<PlayerNames>(player);
		}
		// not available (a dying villager too) -> the atom carries nothing; already carried by a particle system -> no
		// take, but the atom keeps it; in physics (flying) -> nothing; else physics::particle_carried_objects::Take (physics
		// with no body, a living one flying, out of the map cells, marked as carried), which may refuse it too
		if (!ecs::physics::particle_carried_objects::IsAvailable(object))
		{
			carried->object = entt::null;
		}
		else if (ecs::physics::particle_carried_objects::IsCarried(object))
		{
			Carried().insert(object);
		}
		else if (ecs::physics::PhysicsObjects::IsFlying(object) || !ecs::physics::particle_carried_objects::Take(object))
		{
			carried->object = entt::null;
		}
		else
		{
			Carried().insert(object);
		}
		auto& atom = effect.NewAtom(flying, carried.get(), {});
		const auto& transform = registry.Get<const ecs::components::Transform>(object);
		// position, ruleScale = |row 1| (the matrix's Y axis), rotation = the rows / that
		atom.position = transform.position;
		atom.ruleScale = glm::length(transform.rotation[1] * transform.scale.y);
		atom.baseScale = 1.0f;
		atom.rotation = transform.rotation;
		carried->lastPosition = transform.position;
		atom.modifierData.insert_or_assign(k_CarrierKey, std::static_pointer_cast<void>(carried));
		auto& ad = AtomDataOf<FlyingAtomData>(atom, this);
		ad.state = 0;
		ad.blend = effect.Random(0.7f);
		ad.speed = effect.Random(1.0f) + 0.5f;
		if (storm::TraceEnabled())
		{
			const char* kind = registry.AllOf<ecs::components::Villager>(object) ? "villager"
			                   : registry.AllOf<ecs::components::Animal>(object) ? "animal"
			                   : registry.AllOf<ecs::components::Tree>(object)   ? "tree"
			                   : registry.AllOf<ecs::components::Pot>(object)    ? "pile/pot"
			                                                                     : "object";
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Storm: tornado takes {} {} at ({:.1f}, {:.1f}), rises to {:.2f}", kind,
			                   static_cast<uint32_t>(object), transform.position.x, transform.position.z, ad.blend);
		}
	}

	/// The flying atoms: the pretend objects and the objects picked up, spun round the funnel and lifted
	void UpdateFlyingAtoms(Effect& effect, Collection& collection, const TornadoData& data, float tornadoAge) const
	{
		collection.alpha = data.alpha;
		EmitPretendObjects(effect, collection, data);
		PickUp(effect, collection, data, tornadoAge);
		const float dt = effect.GetDt();
		const float invDt = 1.0f / dt;
		for (size_t i = 0; i < collection.atoms.size();)
		{
			Atom& atom = *collection.atoms[i];
			auto& ad = AtomDataOf<FlyingAtomData>(atom, this); // a new one: state 2
			const glm::vec3 p = atom.position;
			const float h = Clamp01((p.y - data.base.y) / (data.top.y - data.base.y));
			if (ad.state == 0 || ad.state == 1)
			{
				// closing: to GroupToMoveToOnCloseDown (or gone); risen past 0.9: to GroupToMoveToOnceDone (or gone)
				if (data.closing)
				{
					MoveOrDelete(effect, collection, i, groupOnCloseDown);
					continue;
				}
				ad.blend = Clamp01(ad.blend + dt * 0.1f);
				if (h > 0.7f && h > 0.9f)
				{
					MoveOrDelete(effect, collection, i, groupOnceDone);
					continue;
				}
			}
			const glm::vec3 centre = Centre(h, data);
			const glm::vec3 d = p - centre;
			const float rho = std::sqrt(d.x * d.x + d.z * d.z);
			const float r = Radius(h, data);
			const float w = ThetaDot(h, ad, data);
			const float rho2 = rho + storm::RadialRate(r, rho, dt) * dt;
			// up or down towards lerp(base.y, top.y, blend), k = 0.1 for game objects, 0.3 for the others
			const float k = ad.state == 0 ? 0.1f : 0.3f;
			const float targetY = data.base.y + (data.top.y - data.base.y) * ad.blend;
			const float vy = (targetY - p.y) * k;
			const float phi = std::atan2(d.z, d.x);
			// outside the funnel wall the spin slows by r / (rho + 0.1)
			const float spin = rho2 > r ? r / (rho2 + 0.1f) * w : w;
			const float phi2 = phi + spin * dt;
			const glm::vec3 next(centre.x + rho2 * std::cos(phi2), d.y + vy * dt + centre.y, centre.z + rho2 * std::sin(phi2));
			// plus the funnel's own velocity at that height
			const glm::vec3 v = (next - p) * invDt + data.baseVelocity + (data.topVelocity - data.baseVelocity) * h;
			if (ad.state == 1)
			{
				// thrown out with gravity, then drawn into the vortex over PretendBlendTime
				const glm::vec3 gravity(0.0f, -pretendGravity, 0.0f);
				const glm::vec3 acceleration = (v - atom.velocity) * invDt;
				const float t = Clamp01(effect.AtomAge(atom) / pretendBlendTime);
				atom.velocity += (gravity + (acceleration - gravity) * t) * dt;
			}
			else if (showVelocityField)
			{
				atom.velocity = v;
			}
			else
			{
				atom.velocity += (v - atom.velocity) * damping * dt;
			}
			atom.position += atom.velocity * dt;
			if (auto* carried = dynamic_cast<TornadoCarrier*>(const_cast<Creator*>(atom.creator)); carried != nullptr)
			{
				carried->lastPosition = atom.position;
			}
			if (ad.state == 2)
			{
				if (data.fade < 0.0001f)
				{
					collection.atoms.erase(collection.atoms.begin() + static_cast<std::ptrdiff_t>(i));
					continue;
				}
				atom.ruleScale = SpriteScale(h, data) * data.fade;
			}
			++i;
		}
	}

	void MoveOrDelete(Effect& effect, Collection& collection, size_t& index, int group) const
	{
		if (group == -1)
		{
			collection.atoms.erase(collection.atoms.begin() + static_cast<std::ptrdiff_t>(index));
			return;
		}
		effect.MoveToBaseGroup(collection, *collection.atoms[index], group);
	}

	std::string creator;
	float topHeight;
	int wiggleCount;
	float wiggleAmplitude;
	float fadeOutTime, fadeInTime;
	float baseRadius, topRadius, baseScale, topScale;
	float baseThetaDot, topThetaDot, meshThetaDot;
	float thetaBias, funnelBend;
	float topMoveFreq, topMoveAmp;
	int groupFlying, groupDebris, groupMesh, groupOnceDone, groupOnCloseDown;
	bool showVelocityField;
	std::string scaleProvider;
	float delayBeforeMove, damping;
	float debrisRadius, debrisHeight, debrisSpeed, debrisEmitRate, debrisSpreadAngle;
	float pretendRadius, pretendSpeed, pretendEmitRate, pretendSpreadAngle, pretendGravity, pretendBlendTime;
	float resourceMin, resourceMax;
	std::string debrisCreator, pretendSmall, pretendMedium, pretendLarge;
	SoundAction soundTornado;
	int numAtomsToCreate {100};
};

// ================================================================================================================
// UR_StormCast
// ================================================================================================================

/// UR_StormCast's collection data
struct StormCastData
{
	bool first {true};
	glm::vec3 position {0.0f};
	glm::vec3 heading {0.0f};
	float radius {0.0f};
};

/// UR_StormCast's data on each atom
struct StormCastAtomData
{
	float theta {0.0f};
	float thetaFactor {0.0f};
	float radiusFactor {0.0f};
};

class StormCast final: public Modifier
{
	static constexpr float k_HeadingLength = 20.0f;

public:
	explicit StormCast(const Object& object)
	    : creator(object.String("PCreator"))
	    , nextGroups(object.Array("NextGroups"))
	    , numAtoms(object.Int("NumAtoms", 30))
	    , maxRadius(object.Float("MaxRadius", 1.0f))
	    , minRadius(object.Float("MinRadius", 0.2f))
	    , thetaDotMinRadius(object.Float("ThetaDotMinRadius", 1.0f))
	    , thetaDotMaxRadius(object.Float("ThetaDotMaxRadius", 1.0f))
	    , radiusDot(object.Float("RadiusDot", 2.0f))
	    , initHeight(object.Float("InitHeight", 5.0f))
	    , thetaDotSpread(object.Float("ThetaDotSpread", 0.5f))
	    , initScaleSpread(object.Float("InitScaleSpread", 0.5f))
	    , initRadiusSpread(object.Float("InitRadiusSpread", 0.7f))
	    , dispersalAge(object.Float("DispersalAge", 6.0f))
	    , accnStartTime(object.Float("AccnStartTime", 1.0f))
	    , accnEndTime(object.Float("AccnEndTime", 4.0f))
	    , fadeOutTime(object.Float("FadeOutTime", 2.0f))
	{
	}

	[[nodiscard]] bool KeepsAlive() const override { return true; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		auto& data = CollectionDataOf<StormCastData>(collection, this);
		Atom* parent = collection.parent;
		if (parent == nullptr)
		{
			return true;
		}
		const float dt = effect.GetDt();
		if (data.first)
		{
			data.first = false;
			data.position += effect.GetOrigin();
			data.radius = maxRadius;
			// the heading scaled to a length of 20 (a fixed value, no property)
			data.heading = CurrentHeading(effect);
			if (data.heading != glm::vec3(0.0f))
			{
				data.heading = glm::normalize(data.heading) * k_HeadingLength;
			}
			const auto* particleCreator = effect.FindCreator(creator);
			std::vector<int> groups(nextGroups.begin(), nextGroups.end());
			for (int i = 0; i < numAtoms; ++i)
			{
				auto& atom = effect.NewAtom(collection, particleCreator, groups);
				auto& ad = AtomDataOf<StormCastAtomData>(atom, this);
				atom.baseScale *= RandRange(effect, 1.0f - initScaleSpread, 1.0f + initScaleSpread);
				ad.theta = effect.Random(k_TwoPi);
				ad.thetaFactor = RandRange(effect, -thetaDotSpread, thetaDotSpread) + 1.0f;
				ad.radiusFactor = RandRange(effect, 1.0f - initRadiusSpread, 1.0f);
			}
			parent->ruleScale = effect.GetMagnitude();
		}
		const float s = parent->ruleScale;
		// the parent moves along the heading, accelerating from AccnStartTime to AccnEndTime (0 -> 20 m/s)
		const float age = effect.CollectionAge(collection);
		const float a = Clamp01((age - accnStartTime) / (accnEndTime - accnStartTime));
		parent->position += data.heading * dt * a;
		// the ring shrinks to MinRadius, then after DispersalAge grows and fades out over FadeOutTime
		if (age < dispersalAge)
		{
			data.radius = std::max(data.radius - dt * radiusDot, minRadius);
		}
		else
		{
			data.radius += dt * radiusDot;
			const float f = age - dispersalAge;
			if (!(f <= fadeOutTime))
			{
				collection.atoms.clear();
				return false;
			}
			collection.alpha = static_cast<float>(static_cast<uint8_t>(static_cast<int>(255.0f - f / fadeOutTime * 255.0f)));
		}
		const float rn = Clamp01((data.radius - minRadius) / (maxRadius - minRadius));
		const float spin = thetaDotMinRadius + (thetaDotMaxRadius - thetaDotMinRadius) * rn;
		for (auto& atom : collection.atoms)
		{
			auto& ad = AtomDataOf<StormCastAtomData>(*atom, this);
			ad.theta += dt * ad.thetaFactor * spin;
			const float r = ad.radiusFactor * data.radius;
			glm::vec3 local(std::cos(ad.theta) * r, 0.0f, std::sin(ad.theta) * r);
			// LocalToGlobal, the land's height + InitHeight x the parent's scale, GlobalToLocal
			glm::vec3 global = effect.LocalToGlobal(collection, local);
			global.y = LandAt(global.x, global.z) + s * initHeight;
			atom->position = effect.GlobalToLocal(collection, global);
		}
		return true;
	}

private:
	std::string creator;
	std::vector<int> nextGroups;
	int numAtoms;
	float maxRadius, minRadius, thetaDotMinRadius, thetaDotMaxRadius, radiusDot, initHeight;
	float thetaDotSpread, initScaleSpread, initRadiusSpread, dispersalAge, accnStartTime, accnEndTime, fadeOutTime;
};
} // namespace

// ================================================================================================================
// the formulas
// ================================================================================================================

weather::storms::StormDescriptor storm::GatherStormDescriptor(const GatherStormInput& input)
{
	weather::storms::StormDescriptor d; // the position stays 0: the gather sets it next
	// inner = max(R, 60), outer = max(max(2.5 R, inner + 20), 80): each comparison keeps the value only when it is
	// above the constant
	float inner = input.radius;
	float outer = input.radius * 2.5f;
	if (!(inner > 60.0f))
	{
		inner = 60.0f;
	}
	if (inner + 20.0f > outer)
	{
		outer = inner + 20.0f;
	}
	if (!(outer > 80.0f))
	{
		outer = 80.0f;
	}
	// the wind: speed = power x lerp(WindMinSpeed, WindMaxSpeed, clamp((mag - MagMin) / (MagMax - MagMin), 0, 1)),
	// along the heading, each component clamped to -128..128 and rounded into a byte (128 wraps to -128)
	const float f = Clamp01((input.magnitude - input.magnitudeForWindMinSpeed) /
	                        (input.magnitudeForWindMaxSpeed - input.magnitudeForWindMinSpeed));
	const float a = input.power * input.windMinSpeed;
	const float speed = a + f * (input.power * input.windMaxSpeed - a);
	const auto windByte = [](float w) {
		w = !(w > -128.0f) ? -128.0f : (w < 128.0f ? w : 128.0f);
		return static_cast<int8_t>(static_cast<uint8_t>(static_cast<int32_t>(std::nearbyint(w)) & 0xFF));
	};
	d.weather.windX = windByte(input.heading.x * speed);
	d.weather.windZ = windByte(input.heading.z * speed);
	d.innerRadius = inner;
	d.outerRadius = outer;
	d.fadeInTime = input.timeToForm * 0.5f;
	d.lifeTime = 1e9f; // for ever
	d.strength = 1.0f;
	d.numClouds = 0; // the weather draws no clouds of its own for the miracle
	d.elevation = input.cloudHeight;
	d.weather.temperature = 20;
	// rain on -> min(trunc(power x rainAmount), 100) as an unsigned byte (100 without a storm spell); off -> 0
	int rain = 0;
	if (input.rainOn)
	{
		rain = 100;
		if (!(input.rainAmount < 0.0f))
		{
			const auto value = static_cast<uint32_t>(static_cast<int32_t>(input.power * input.rainAmount)) & 0xFFu;
			rain = value <= 100u ? static_cast<int>(value) : 100;
		}
	}
	d.weather.rain = static_cast<int8_t>(rain);
	d.weather.snowCover = 0;
	d.weather.snow = 0;
	d.weather.overcast = 80;
	return d;
}

float storm::FunnelRadius(float h, float base, float top, float tornadoScale)
{
	h = h > 0.0f ? (h < 1.0f ? h : 1.0f) : 0.0f;
	return (base + (top - base) * (h * h)) * tornadoScale;
}

float storm::Bias(float b, float x)
{
	// x^(ln b / ln 0.5)
	return std::pow(x, std::log(b) * (1.0f / std::log(0.5f)));
}

float storm::Gain(float g, float x)
{
	if (x < 0.5f)
	{
		return Bias(1.0f - g, x + x) * 0.5f;
	}
	return 1.0f - Bias(1.0f - g, 2.0f - (x + x)) * 0.5f;
}

float storm::RadialRate(float r, float rho, float dt)
{
	const float k1 = (rho - r) * -0.5f;
	const float k2 = (k1 * dt + rho - r) * -0.5f;
	return (k1 + k2) * 0.5f;
}

storm::CloudLook storm::CloudLookAt(float f, const CloudLookParams& p)
{
	CloudLook look {};
	look.grow =
	    f < p.fracToMaxSize ? f * (1.0f / p.fracToMaxSize) : 1.0f - (f - p.fracToMaxSize) * (1.0f / (1.0f - p.fracToMaxSize));
	look.ratio = (p.minCloudRatio - p.maxCloudRatio) * f + p.maxCloudRatio;
	look.colour = static_cast<int>(static_cast<float>(p.minColor - p.maxColor) * f + static_cast<float>(p.maxColor)) & 0xFF;
	look.scale = ((p.maxScaleFactor - p.minScaleFactor) * look.grow + p.minScaleFactor) * p.scaleProvider;
	look.alpha =
	    static_cast<int>(static_cast<float>(p.maxAlpha - p.minAlpha) * look.grow + static_cast<float>(p.minAlpha)) & 0xFF;
	return look;
}

float storm::CollectionScale(float collectionAge, float c)
{
	const float t = Clamp01(collectionAge * 0.1f);
	return (1.0f - c) * ((3.0f - (t + t)) * t * t) + c;
}

void storm::UpdateCarriedObjects()
{
	if (Carried().empty() || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto& drawable : manager::Collect(Creator::Kind::Other))
	{
		for (const auto& atom : drawable.atoms)
		{
			const auto* carried = dynamic_cast<const TornadoCarrier*>(atom.creator);
			if (carried == nullptr || !ecs::IsAvailable(carried->object))
			{
				continue;
			}
			carried->lastPosition = atom.position;
			// the object's matrix is the atom's (rotation and scale)
			if (auto* transform = registry.TryGet<ecs::components::Transform>(carried->object); transform != nullptr)
			{
				transform->position = atom.position;
				transform->rotation = atom.rotation;
				transform->scale = glm::vec3(atom.scale);
			}
		}
	}
	registry.SetDirty();
}

size_t storm::StrikeCount()
{
	return StormRulesDebugHooksData().strikes;
}

size_t storm::CarriedObjectCount()
{
	return Carried().size();
}

bool storm::TraceEnabled()
{
	// OPENBLACK_STORM_TRACE, read at the first call
	static const bool k_Trace = [] {
		const char* value = std::getenv("OPENBLACK_STORM_TRACE");
		return value != nullptr && value[0] != '\0' && value[0] != '0';
	}();
	return k_Trace;
}

void openblack::psys::RegisterStormRules()
{
	RegisterModifier("UR_CloudMoverNew", MakeModifierOf<CloudMoverNew>);
	RegisterModifier("UR_CloudGather", MakeModifierOf<CloudGather>);
	RegisterModifier("UR_Tornado", MakeModifierOf<Tornado>);
	RegisterModifier("UR_StormCast", MakeModifierOf<StormCast>);
}
