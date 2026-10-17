/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Flocking: the atoms of a collection fly as a flock round their parent, the butterflies or bats over a forest and the
// flies round a creature. The flock as a whole is drawn towards the parent; each atom keeps towards the flock's middle,
// away from its nearest neighbour, at the flock's speed, and banks into its turns.

#include <cmath>

#include <algorithm>
#include <numbers>
#include <string>

#include <ParticleFile.h>
#include <glm/geometric.hpp>

#include "Common/X87.h"
#include "Common/X87Vector.h"
#include "ParticleClassRegistry.h"
#include "ParticleFlocking.h"

using namespace openblack::particles;
using openblack::psys::ParticleObject;
namespace x87 = openblack::x87;

namespace
{
/// Distances are never taken as shorter than this
constexpr float k_MinimumDistance = 0.01f;
constexpr float k_QuarterTurn = std::numbers::pi_v<float> * 0.5f;

/// How the pull of something falls off with distance: not at all, with the distance or its square, as a strength or,
/// inverted, as a weakness
enum class Falloff : uint8_t
{
	None,
	Linear,
	Square,
};

Falloff FalloffOf(float value)
{
	const auto type = static_cast<int>(value);
	return type == 1 ? Falloff::Linear : (type == 2 ? Falloff::Square : Falloff::None);
}

class Flocking final: public Modifier
{
public:
	explicit Flocking(const ParticleObject& object)
	    : velocityMatching(object.Float("K_VelocityMatching", 0.0f))
	    , centralAttraction(object.Float("K_CentralAttraction", 0.0f))
	    , neighbourAccn(object.Float("K_NeighbourAccn", 0.0f))
	    , neighbourAvoidance(object.Float("K_NeighbourAvoidance", 0.0f))
	    , damping(object.Float("K_Damping", 0.0f))
	    , flockDamping(object.Float("K_FlockDamping", 0.0f))
	    , idealVel(object.Float("K_IdealVel", 0.0f))
	    , maxAccn(object.Float("F_MaxAccn", 0.0f))
	    , maxVel(object.Float("F_MaxVel", 0.0f))
	    , gravityForBanking(object.Float("GravityForBanking", 0.0f))
	    , reducePitchBy(object.Float("ReducePitchBy", 0.0f))
	    , scaleModifier(object.Float("ScaleModifier", 1.0f))
	    , centralFalloff(FalloffOf(object.Float("AxisChosen", 0.0f)))
	    , neighbourFalloff(FalloffOf(object.Float("NeghbourAccnType", 0.0f)))
	    , idealFalloff(FalloffOf(object.Float("IdealVelAccnType", 0.0f)))
	    , invertIdeal(object.Bool("F_InvertAccnIdealVel", false))
	    , invertCentral(object.Bool("F_InvertAccn", false))
	    , spriteRotation(object.Bool("SpriteRotation", false))
	    , invertNeighbour(object.Bool("NeighbourAccnInvert", false))
	    , localScale(object.String("LocalScaleFP"))
	{
	}

	[[nodiscard]] float Weight(float distance, Falloff falloff, bool invert) const
	{
		return flocking::Falloff(distance, static_cast<int>(falloff), invert, scaleModifier);
	}

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& slot) const override
	{
		const auto n = collection.atoms.size();
		if (n == 0)
		{
			return true;
		}
		const float dt = effect.GetDt();
		// The flock is drawn towards its parent
		const auto origin = effect.GlobalToLocal(
		    collection, collection.parent != nullptr ? effect.GlobalPosition(*collection.parent) : effect.GetOrigin());
		glm::vec3 averagePosition(0.0f);
		glm::vec3 averageVelocity(0.0f);
		for (const auto& atom : collection.atoms)
		{
			averagePosition += atom->position;
			averageVelocity += atom->velocity;
		}
		const float share = 1.0f / static_cast<float>(n);
		averagePosition *= share;
		averageVelocity *= share;
		float toOrigin = 0.0f;
		const auto originDirection = flocking::Direction<x87::SumOrder::Xzy>(origin - averagePosition, toOrigin);
		const auto ideal = originDirection * (Weight(toOrigin, idealFalloff, invertIdeal) * idealVel);
		// The flock's own velocity, kept in the collection's slot
		const glm::vec3 oldFlock(slot.state);
		const glm::vec3 flock = (ideal * dt) + (oldFlock * (1.0f - (dt * flockDamping)));
		slot.state = glm::vec4(flock, 0.0f);
		const float scale = localScale.empty() ? 1.0f : effect.FloatProvider(localScale, 1.0f);
		const bool scaled = !localScale.empty();

		std::vector<glm::vec3> velocities;
		velocities.reserve(n);
		for (const auto& atom : collection.atoms)
		{
			glm::vec3 neighbours(0.0f);
			const Atom* nearest = nullptr;
			float nearestSquared = 0.0f;
			for (const auto& other : collection.atoms)
			{
				if (other == atom)
				{
					continue;
				}
				const auto offset = other->position - atom->position;
				float distance = 0.0f;
				const auto direction = flocking::Direction<x87::SumOrder::Xzy>(offset, distance);
				if (scaled)
				{
					distance *= scale;
				}
				neighbours += direction * Weight(distance, neighbourFalloff, invertNeighbour);
				const float squared = x87::Dot<x87::SumOrder::Xzy>(offset, offset);
				if (nearest == nullptr || squared < nearestSquared)
				{
					nearest = other.get();
					nearestSquared = squared;
				}
			}
			if (nearest == nullptr)
			{
				// Alone, it keeps flying as it was
				velocities.push_back(atom->velocity);
				continue;
			}
			neighbours *= neighbourAccn / static_cast<float>(n);
			const auto relative = atom->velocity - oldFlock;
			float nearDistance = 0.0f;
			const auto nearDirection =
			    flocking::Direction<x87::SumOrder::Zyx>(nearest->position - atom->position, nearDistance);
			if (scaled)
			{
				nearDistance *= scale;
			}
			const auto avoid = nearDirection * (Weight(nearDistance, Falloff::Square, false) * neighbourAvoidance);
			float toMiddle = 0.0f;
			const auto middleDirection = flocking::Direction<x87::SumOrder::Zyx>(averagePosition - atom->position, toMiddle);
			const auto central = middleDirection * (Weight(toMiddle, centralFalloff, invertCentral) * centralAttraction);
			auto matching = (averageVelocity - relative) * velocityMatching;
			if (scaled)
			{
				matching *= scale;
			}
			const auto acceleration = flocking::LimitedLength((central - avoid) + matching + neighbours, maxAccn);
			auto velocity = (relative * (1.0f - (dt * damping))) + (acceleration * dt) + flock;
			velocity = flocking::LimitedSquaredLength(velocity, maxVel);
			if (spriteRotation)
			{
				// Turned on the screen to face where it flies
				atom->rotation =
				    flocking::SpriteTurn(velocity, effect.Services().world.CameraRight(), effect.Services().world.CameraUp());
			}
			else
			{
				const auto accelerating = dt > 0.0f ? (velocity - atom->velocity) * (1.0f / dt) : glm::vec3(0.0f);
				atom->rotation = flocking::Banking(velocity, accelerating, reducePitchBy, gravityForBanking);
			}
			velocities.push_back(velocity);
		}
		for (size_t i = 0; i < n; ++i)
		{
			auto& atom = *collection.atoms[i];
			atom.velocity = velocities[i];
			atom.position += atom.velocity * dt;
		}
		return true;
	}

	float velocityMatching;
	float centralAttraction;
	float neighbourAccn;
	float neighbourAvoidance;
	float damping;
	float flockDamping;
	float idealVel;
	float maxAccn;
	float maxVel;
	float gravityForBanking;
	float reducePitchBy;
	float scaleModifier;
	Falloff centralFalloff;
	Falloff neighbourFalloff;
	Falloff idealFalloff;
	bool invertIdeal;
	bool invertCentral;
	bool spriteRotation;
	bool invertNeighbour;
	std::string localScale;
};
} // namespace

float flocking::Falloff(float distance, int type, bool invert, float scaleModifier)
{
	const float x = (distance < k_MinimumDistance ? k_MinimumDistance : distance) * scaleModifier;
	const float base = type == 1 ? x : (type == 2 ? x * x : 1.0f);
	return invert ? base : 1.0f / base;
}

glm::vec3 flocking::LimitedLength(const glm::vec3& v, float most)
{
	if (v == glm::vec3(0.0f) || !(x87::LengthZyx(v) > most))
	{
		return v;
	}
	return x87::Scaled<x87::SumOrder::Zyx>(v, most);
}

glm::vec3 flocking::LimitedSquaredLength(const glm::vec3& v, float most)
{
	if (v == glm::vec3(0.0f) || !(most * most < x87::DotZyx(v, v)))
	{
		return v;
	}
	return x87::Scaled<x87::SumOrder::Zyx>(v, most);
}

glm::mat3 flocking::Banking(glm::vec3 velocity, glm::vec3 acceleration, float reducePitchBy, float gravityForBanking)
{
	const float across = std::sqrt((velocity.x * velocity.x) + (velocity.z * velocity.z));
	const float yaw = x87::RoundToFloat(x87::Atan2(velocity.z, velocity.x));
	const float pitch = x87::Mul(x87::Atan2(velocity.y, across), reducePitchBy);
	const float lateral = across == 0.0f ? 0.0f : ((acceleration.z * velocity.x) - (acceleration.x * velocity.z)) / across;
	// The game divides by the gravity with no test for zero
	const double bank = x87::Atan(lateral / gravityForBanking);
	// Lying along its heading: a quarter turn back about the vertical, with the turn kept as a double
	const double lying = -static_cast<double>(k_QuarterTurn);
	const float c0 = x87::RoundToFloat(x87::Cos(lying));
	const float s0 = x87::RoundToFloat(x87::Sin(lying));
	// Banked about its length. The game adds and takes away the zero parts of the first turn.
	const float cb = x87::RoundToFloat(x87::Cos(bank));
	const double sb = x87::Sin(bank);
	glm::mat3 m(glm::vec3(c0 - 0.0f, x87::Mul(sb, s0) + 0.0f, (s0 * cb) - 0.0f),
	            glm::vec3(0.0f, (0.0f * cb) + cb, x87::Sub(0.0f, sb)),
	            glm::vec3(0.0f - s0, x87::Mul(sb, c0) + 0.0f, (c0 * cb) - 0.0f));
	// Pitched by its climb, then turned to its heading
	const float down = -pitch;
	const float cp = x87::RoundToFloat(x87::Cos(down));
	const double sp = x87::Sin(down);
	const double cy = x87::Cos(yaw);
	const double sy = x87::Sin(yaw);
	for (int i = 0; i < 3; ++i)
	{
		const float x = x87::Mul(sp, m[i].y) + (m[i].x * cp);
		const float y = (m[i].y * cp) - x87::Mul(sp, m[i].x);
		m[i].x = x87::Mul(cy, x) - x87::Mul(sy, m[i].z);
		m[i].z = x87::Mul(cy, m[i].z) + x87::Mul(sy, x);
		m[i].y = y;
	}
	return m;
}

glm::mat3 flocking::SpriteTurn(glm::vec3 velocity, glm::vec3 right, glm::vec3 up)
{
	const float across = x87::Dot<x87::SumOrder::Zyx>(velocity, right);
	const float along = x87::Dot<x87::SumOrder::Zxy>(velocity, up);
	const float angle = x87::Add(x87::Atan2(-along, across), k_QuarterTurn);
	const float c = x87::RoundToFloat(x87::Cos(angle));
	const float s = x87::RoundToFloat(x87::Sin(angle));
	return {glm::vec3(c, 0.0f, s), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(-s, 0.0f, c)};
}

void openblack::particles::RegisterFlockingRules(ParticleClassRegistry& registry)
{
	registry.AddModifier("UR_Flocking", ParticleClassRegistry::Make<Flocking>);
}
