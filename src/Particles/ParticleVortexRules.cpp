/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The rule of a vortex that leads away from the land. The vortex hands its effect the things it takes; each step the
// rule first starts carrying each of them with a particle of its own, from where it stands, then moves every particle it
// carries round and down into the middle (see vortex::crossing::Attract). A thing that reaches the middle is gone with
// its particle, while one that can't be destroyed is thrown back out at a random angle.

#include <memory>
#include <numbers>
#include <utility>
#include <vector>

#include <ParticleFile.h>

#include "Magic/VortexCrossingRules.h"
#include "ParticleClassRegistry.h"

using namespace openblack::particles;
using openblack::psys::ParticleObject;
using namespace openblack::vortex::crossing;

namespace
{
constexpr float k_TwoPi = 2.0f * std::numbers::pi_v<float>;

class VortexAttract final: public Modifier
{
public:
	explicit VortexAttract(const ParticleObject& object)
	{
		settings.radialSpeed = object.Float("RadialSpeed", settings.radialSpeed);
		settings.thetaSpeed = object.Float("ThetaSpeed", settings.thetaSpeed);
		settings.deletionRadius = object.Float("DeletionRadius", settings.deletionRadius);
		settings.minRadius = object.Float("MinRadius", settings.minRadius);
		settings.blendTime = object.Float("BlendTime", settings.blendTime);
		settings.blendTimeVelocity = object.Float("BlendTimeVel", settings.blendTimeVelocity);
		settings.yOffset = object.Float("YParam_YOffset", settings.yOffset);
		settings.rZeroHeight = object.Float("YParam_RZeroHeight", settings.rZeroHeight);
	}

	/// It waits for things to be handed to it for as long as its vortex lasts
	[[nodiscard]] bool Creates() const override { return true; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		auto& world = effect.Services().world;
		// The things handed over since the last step, the last handed first
		while (const auto object = effect.TakeTarget())
		{
			Adopt(effect, collection, *object);
		}
		const float dt = effect.GetDt();
		if (!(dt > 0.0f))
		{
			return true;
		}
		const auto origin = effect.GetOrigin();
		const GroundHeight ground = [&world](glm::vec2 xz) { return world.LandHeight(xz); };
		std::vector<std::pair<Atom*, bool>> gone;
		for (auto& owned : collection.atoms)
		{
			auto& atom = *owned;
			const auto kept = atom.data.find(this);
			if (kept == atom.data.end() || !kept->second.started)
			{
				continue;
			}
			const AttractAtom state {.position = atom.position,
			                         .takenVelocity = glm::vec3(kept->second.a),
			                         .startHeight = kept->second.a.w,
			                         .age = effect.AtomAge(atom),
			                         .indestructible = kept->second.b.x != 0.0f};
			if (const auto step = Attract(settings, state, origin, dt, ground))
			{
				atom.position = step->position;
				atom.velocity = step->velocity;
				continue;
			}
			gone.emplace_back(&atom, state.indestructible);
		}
		for (const auto& [atom, indestructible] : gone)
		{
			if (indestructible && atom->carried != nullptr)
			{
				// Thrown back out of the middle, from where it was
				atom->carried->position = atom->position;
				atom->carried->rotation = atom->rotation;
				const auto object = atom->carried->object;
				atom->carried->atom = nullptr;
				atom->carried.reset();
				world.ThrowOutOfVortex(object, effect.Random(k_TwoPi), settings.throwScale);
			}
			// Apple's clang can't capture a structured binding: take a plain copy of it
			const auto* leaving = atom;
			std::erase_if(collection.atoms, [leaving](const auto& owned) { return owned.get() == leaving; });
		}
		return true;
	}

private:
	/// A particle starts carrying the thing from where it stands, as it stands
	void Adopt(Effect& effect, Collection& collection, entt::entity object) const
	{
		auto& world = effect.Services().world;
		auto carried =
		    std::make_shared<CarriedObject>(CarriedObject {.object = object, .carrier = CarriedObject::Carrier::Vortex});
		const auto entry = world.EnterVortex(carried);
		if (!entry.has_value())
		{
			return;
		}
		auto& atom = effect.NewAtom(collection, nullptr, {});
		atom.position = entry->position;
		atom.rotation = entry->rotation;
		// The particle itself starts still: the velocity the thing was taken with only steers its moves
		atom.velocity = glm::vec3(0.0f);
		atom.carried = carried;
		carried->atom = &atom;
		carried->position = atom.position;
		carried->rotation = atom.rotation;
		auto& kept = atom.data[this];
		kept.started = true;
		// The velocity it was taken with, and its height above the land then
		kept.a = glm::vec4(entry->velocity, entry->position.y - world.LandHeight({entry->position.x, entry->position.z}));
		kept.b.x = entry->indestructible ? 1.0f : 0.0f;
	}

	AttractSettings settings;
};
} // namespace

void openblack::particles::RegisterVortexRules(ParticleClassRegistry& registry)
{
	registry.AddModifier("UR_VortexAttract", ParticleClassRegistry::Make<VortexAttract>);
}
