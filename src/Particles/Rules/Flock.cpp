/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The flock miracles' particle rules (SF_FlockFlyingRainGood / Evil, SF_FlockGroundDust): UR_FollowTargets, one
// atom on each target the spell gives its effect (every bird, every wolf), with the glow trail under it, and
// EventConditionAtomNearVillagers (declared by SF_FlockGroundDust, used by no rule of the shipped files); and the boids
// of the particle flocks, UR_Flocking (the butterflies of SF_Forest / SF_Butterflies*, the flies of SF_Flies*, the itch
// of SF_CreatureSpellItch*). Wiki: docs/bw1-notes/miracles.md ("Flocks").

#include "Flock.h"

#include <cmath>

#include <array>
#include <numbers>
#include <string>
#include <string_view>
#include <vector>

#include <glm/geometric.hpp>

#include "3D/Billboard.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Audio/Services/SpellSounds.h"
#include "Camera/Camera.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Map.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
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
bool Available(entt::entity object)
{
	// available (ecs::IsAvailable), with a Transform
	return object != entt::null && Locator::entitiesRegistry::has_value() && ecs::IsAvailable(object) &&
	       Locator::entitiesRegistry::value().AllOf<ecs::components::Transform>(object);
}

/// UR_FollowTargets' data on an atom: the target (the original also keeps the game turn it was last seen, unused)
struct FollowData
{
	entt::entity target {entt::null};
};

/// UR_FollowTargets (a create rule's properties plus its own). Flags 2 without 4: no creator, but the effect waits for
/// its targets until it closes.
class FollowTargets final: public Modifier
{
public:
	explicit FollowTargets(const Object& object)
	    : creator(object.String("PCreator"))
	    , nextGroups(object.Array("NextGroups"))
	    , removeTarget(object.Bool("RemoveTargetFromManager", true))
	    , removeAtomWhenTargetDies(object.Bool("RemoveAtomWhenTargetDies", false))
	    , usePointTargets(object.Bool("UseLHPointTargets", false))
	    , soundOneOnly(object.Bool("SoundOneOnly", true))
	    , soundCreate(ReadSoundAction(object, "SoundCreate"))
	{
	}
	[[nodiscard]] bool KeepsAlive() const override { return true; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		// no PCreator: the rule detaches
		if (creator.empty() || effect.FindCreator(creator) == nullptr)
		{
			return false;
		}
		// not closing: one new atom per step, on a target object of the spell or, with UseLHPointTargets and no object
		// left, on a point
		if (!effect.Closing())
		{
			if (!effect.GetTargets().empty())
			{
				// RemoveTargetFromManager: the last one added, taken out; else the first, left in the list
				const auto target = removeTarget ? effect.TakeTarget() : effect.GetTargets().front();
				if (target != entt::null)
				{
					NewFollower(effect, collection, target);
				}
			}
			else if (usePointTargets && effect.TargetPointCount() > 0)
			{
				// taken or read through a cyclic cursor: the point is read and not used; the atom follows nothing
				glm::vec3 point;
				if (removeTarget)
				{
					effect.TakeTargetPoint(point);
				}
				NewFollower(effect, collection, entt::null);
			}
		}
		// every atom sits on its target (its map position as a world point: the land + its height, GlobalToLocal); a
		// target that went is forgotten, and with RemoveAtomWhenTargetDies its atom goes too
		for (size_t i = 0; i < collection.atoms.size();)
		{
			auto& atom = *collection.atoms[i];
			auto& data = DataOf(atom);
			if (data.target == entt::null)
			{
				++i; // no target (a point's atom, or one already lost): left where it is
				continue;
			}
			if (!Available(data.target))
			{
				data.target = entt::null;
				if (removeAtomWhenTargetDies)
				{
					collection.atoms.erase(collection.atoms.begin() + static_cast<std::ptrdiff_t>(i));
					continue;
				}
				++i;
				continue;
			}
			const auto& position =
			    Locator::entitiesRegistry::value().Get<const ecs::components::Transform>(data.target).position;
			atom.position = effect.GlobalToLocal(collection, position);
			++i;
		}
		return true;
	}

	std::string creator;
	std::vector<int> nextGroups;
	bool removeTarget;             ///< RemoveTargetFromManager (default 1)
	bool removeAtomWhenTargetDies; ///< default 0
	bool usePointTargets;          ///< UseLHPointTargets (default 0)
	bool soundOneOnly;             ///< SoundOneOnly (default 1)
	SoundAction soundCreate;       ///< SoundCreate

private:
	/// The data of this rule on the atom (found, or new)
	FollowData& DataOf(Atom& atom) const { return AtomDataOf<FollowData>(atom, this); }

	/// A new atom (the creator's init, into the collection with NextGroups), its data with the target, its rule scale =
	/// the target's scale; SoundCreate unless SoundOneOnly and this is not the collection's only atom
	void NewFollower(Effect& effect, Collection& collection, entt::entity target) const
	{
		auto& atom = effect.NewAtom(collection, effect.FindCreator(creator), nextGroups);
		DataOf(atom).target = target;
		if (target != entt::null)
		{
			if (Available(target))
			{
				atom.ruleScale = Locator::entitiesRegistry::value().Get<const ecs::components::Transform>(target).scale.x;
			}
			if (!soundOneOnly || collection.atoms.size() == 1)
			{
				audio::spell_sounds::StartSound(effect, atom, soundCreate);
			}
		}
	}
};

/// A matrix's 3 x 3 part as the original stores it (row-major; it multiplies row vectors: M' = M x R)
using Rows = std::array<std::array<float, 3>, 3>;

Rows Multiply(const Rows& a, const Rows& b)
{
	Rows out {};
	for (size_t i = 0; i < 3; ++i)
	{
		for (size_t j = 0; j < 3; ++j)
		{
			out[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j];
		}
	}
	return out;
}

/// The original's Y rotation layout: rows (c, 0, s), (0, 1, 0), (-s, 0, c); its matrix product uses the same
Rows RotationY(float angle)
{
	const float c = std::cos(angle);
	const float s = std::sin(angle);
	return {{{c, 0.0f, s}, {0.0f, 1.0f, 0.0f}, {-s, 0.0f, c}}};
}

/// The atom's rotation (translation cleared): openblack's columns are the original's rows
glm::mat3 ToAtomRotation(const Rows& m)
{
	return {glm::vec3(m[0][0], m[0][1], m[0][2]), glm::vec3(m[1][0], m[1][1], m[1][2]), glm::vec3(m[2][0], m[2][1], m[2][2])};
}

/// v set to that length (0 stays 0)
glm::vec3 SetLength(glm::vec3 v, float length)
{
	if (v.x == 0.0f && v.y == 0.0f && v.z == 0.0f)
	{
		return v;
	}
	return v * (length / std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z));
}

/// The rule's normalisation: a zero vector stays, its length 0
glm::vec3 Unit(glm::vec3 v, float& length)
{
	if (v.x == 0.0f && v.y == 0.0f && v.z == 0.0f)
	{
		length = 0.0f;
		return v;
	}
	length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	return v * (1.0f / length);
}

/// UR_Flocking (its collection data, the flock's velocity, is kept in the slot's state here), its banking and the
/// sprite turn
class Flocking final: public Modifier
{
public:
	explicit Flocking(const Object& object)
	    : velocityMatching(object.Float("K_VelocityMatching", 1.0f))
	    , centralAttraction(object.Float("K_CentralAttraction", 1.0f))
	    , neighbourAccn(object.Float("K_NeighbourAccn", 0.0f))
	    , neighbourAvoidance(object.Float("K_NeighbourAvoidance", 1.0f))
	    , damping(object.Float("K_Damping", 0.0f))
	    , flockDamping(object.Float("K_FlockDamping", 0.0f))
	    , idealVel(object.Float("K_IdealVel", 0.0f))
	    , maxAccn(object.Float("F_MaxAccn", 10.0f))
	    , maxVel(object.Float("F_MaxVel", 100.0f))
	    , gravityForBanking(object.Float("GravityForBanking", 10.0f))
	    , reducePitchBy(object.Float("ReducePitchBy", 1.0f))
	    , scaleModifier(object.Float("ScaleModifier", 1.0f))
	    , axisChosen(Type(object, "AxisChosen", 0))
	    , neighbourAccnType(Type(object, "NeghbourAccnType", 2))
	    , idealVelAccnType(Type(object, "IdealVelAccnType", 1))
	    , invertAccnIdealVel(object.Bool("F_InvertAccnIdealVel", false))
	    , invertAccn(object.Bool("F_InvertAccn", false))
	    , spriteRotation(object.Bool("SpriteRotation", true))
	    , neighbourAccnInvert(object.Bool("NeighbourAccnInvert", false))
	    , localScale(object.String("LocalScaleFP"))
	{
	}

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& slot) const override
	{
		const size_t n = collection.atoms.size();
		if (n == 0)
		{
			return true;
		}
		const float dt = effect.GetDt();
		const glm::vec3 oldFlock(slot.state);
		// the parent atom's position (the effect's origin without one) in this collection's frame
		const glm::vec3 target = effect.GlobalToLocal(
		    collection, collection.parent != nullptr ? effect.GlobalPosition(*collection.parent) : effect.GetOrigin());
		// the mean velocity and position of the atoms
		glm::vec3 meanVelocity(0.0f);
		glm::vec3 centre(0.0f);
		for (const auto& atom : collection.atoms)
		{
			meanVelocity += atom->velocity;
			centre += atom->position;
		}
		const float inverse = 1.0f / static_cast<float>(n);
		meanVelocity *= inverse;
		centre *= inverse;
		// the flock's velocity: V (1 - dt K_FlockDamping) + unit(target - centre) K_IdealVel f(|target - centre|) dt
		float length = 0.0f;
		const glm::vec3 toTarget = Unit(target - centre, length);
		const float ideal = AccelerationLaw(length, idealVelAccnType, invertAccnIdealVel) * idealVel;
		const glm::vec3 newFlock = oldFlock * (1.0f - dt * flockDamping) + toTarget * ideal * dt;
		slot.state = glm::vec4(newFlock, 0.0f);
		const float scale = localScale.empty() ? 1.0f : effect.FloatProvider(localScale, 1.0f); // LocalScaleFP
		for (auto& atomPointer : collection.atoms)
		{
			auto& a = *atomPointer;
			// every other atom pushes (K_NeighbourAccn): unit(b - a) x f(|b - a| x LocalScale); the nearest is kept
			glm::vec3 neighbours(0.0f);
			const Atom* nearest = nullptr;
			float best = 0.0f;
			for (const auto& other : collection.atoms)
			{
				if (other.get() == &a)
				{
					continue;
				}
				const glm::vec3 r = other->position - a.position;
				const float d2 = r.x * r.x + r.y * r.y + r.z * r.z;
				float distance = 0.0f;
				const glm::vec3 unit = Unit(r, distance);
				neighbours += unit * AccelerationLaw(distance * scale, neighbourAccnType, neighbourAccnInvert);
				if (nearest == nullptr || d2 < best)
				{
					best = d2;
					nearest = other.get();
				}
			}
			neighbours *= neighbourAccn / static_cast<float>(n);
			if (nearest == nullptr)
			{
				continue; // a lone atom keeps its velocity
			}
			const glm::vec3 relative = a.velocity - oldFlock;
			// the avoidance of the nearest: unit(nearest - a) x f(|..| x LocalScale, type 2, not inverted: 1 / x^2) x K
			float nearestDistance = 0.0f;
			const glm::vec3 toNearest = Unit(nearest->position - a.position, nearestDistance);
			const glm::vec3 avoid = toNearest * (AccelerationLaw(nearestDistance * scale, 2, false) * neighbourAvoidance);
			// the attraction to the centre: AxisChosen is its type, F_InvertAccn its inversion
			float centreDistance = 0.0f;
			const glm::vec3 toCentre = Unit(centre - a.position, centreDistance);
			const glm::vec3 attract = toCentre * (AccelerationLaw(centreDistance, axisChosen, invertAccn) * centralAttraction);
			// velocity matching: (mean velocity - relative velocity) x K, x LocalScale
			glm::vec3 matching = (meanVelocity - relative) * velocityMatching;
			if (!localScale.empty())
			{
				matching *= scale;
			}
			glm::vec3 accn = neighbours + (matching + (attract - avoid));
			if (std::sqrt(accn.x * accn.x + accn.y * accn.y + accn.z * accn.z) > maxAccn)
			{
				accn = SetLength(accn, maxAccn);
			}
			glm::vec3 velocity = relative * (1.0f - dt * damping) + accn * dt;
			velocity += newFlock;
			if (maxVel * maxVel < velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z)
			{
				velocity = SetLength(velocity, maxVel);
			}
			if (spriteRotation)
			{
				TurnSprite(a, velocity);
			}
			else
			{
				UpdateBanking(a, velocity, (velocity - a.velocity) * (1.0f / dt));
			}
			a.velocity = velocity;
		}
		// then every atom moves: position += velocity x dt
		for (auto& atom : collection.atoms)
		{
			atom->position += atom->velocity * dt;
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
	int axisChosen; ///< the centre attraction's type
	int neighbourAccnType;
	int idealVelAccnType;
	bool invertAccnIdealVel;
	bool invertAccn;
	bool spriteRotation;
	bool neighbourAccnInvert;
	std::string localScale;

private:
	/// The type properties take 0..2 only (the files write them as FLOAT)
	static int Type(const Object& object, std::string_view key, int fallback)
	{
		const int value = static_cast<int>(object.Float(key, static_cast<float>(fallback)));
		return value >= 0 && value <= 2 ? value : fallback;
	}

	/// flocking::AccelerationLaw (Flock.h) with the rule's ScaleModifier
	float AccelerationLaw(float distance, int type, bool invert) const
	{
		return flocking::AccelerationLaw(distance, type, invert, scaleModifier);
	}

	/// A Y rotation by atan2(-y, x) + pi / 2 of the velocity as the camera sees it (the world-to-camera rotation:
	/// x = v . right, y = v . up; billboard::ScreenVelocity)
	static void TurnSprite(Atom& atom, const glm::vec3& velocity)
	{
		if (!Locator::camera::has_value())
		{
			return;
		}
		const auto& camera = Locator::camera::value();
		atom.rotation =
		    ToAtomRotation(RotationY(graphics::billboard::ScreenVelocity(velocity, camera.GetRight(), camera.GetUp())));
	}

	/// flocking::BankingRotation (Flock.h)
	void UpdateBanking(Atom& atom, const glm::vec3& v, const glm::vec3& a) const
	{
		atom.rotation = flocking::BankingRotation(v, a, reducePitchBy, gravityForBanking);
	}
};

/// EventConditionAtomNearVillagers: the atom's position ((inferred) taken as metres x, z; its local position) as map
/// coordinates (x 6553.6, truncated) is on the map and some object of that 10 m cell (of any type) is a villager. No
/// collection condition of its own: false.
bool AtomNearVillagers(const Effect& /*effect*/, const Object& /*object*/, const Atom* atom, const Collection& /*collection*/)
{
	if (atom == nullptr || !Locator::terrainSystem::has_value())
	{
		return false;
	}
	const auto cell = map_coords::CellOf(atom->position); // int(x * 6553.6f), the high words
	if (!map_coords::InBounds(cell, Locator::terrainSystem::value().GetCellsPerSide()))
	{
		return false; // off the map
	}
	auto& registry = Locator::entitiesRegistry::value();
	// every object of the cell: the fixed list, then the mobile one (ecs::map_cells; one read snapshot for the whole
	// walk)
	const ecs::map_cells::ReadBatch batch;
	for (auto object = ecs::map_cells::FindType(cell, ObjectType::Any); object != entt::null;
	     object = ecs::map_cells::FindType(cell, ObjectType::Any, object))
	{
		if (registry.AllOf<ecs::components::Villager>(object))
		{
			return true;
		}
	}
	return false;
}
} // namespace

float openblack::psys::flocking::AccelerationLaw(float distance, int type, bool invert, float scaleModifier)
{
	if (distance < 0.01f)
	{
		distance = 0.01f; // the original's float 0.01
	}
	const float x = distance * scaleModifier;
	float f = 1.0f;
	if (type == 1)
	{
		f = x;
	}
	else if (type == 2)
	{
		f = x * x;
	}
	// (another type would read the bool argument as a float: the property setters never allow it)
	return invert ? f : 1.0f / f;
}

glm::mat3 openblack::psys::flocking::BankingRotation(const glm::vec3& v, const glm::vec3& a, float reducePitchBy,
                                                     float gravityForBanking)
{
	const float horizontal = std::sqrt(v.x * v.x + v.z * v.z);
	const float yaw = std::atan2(v.z, v.x);
	const float pitch = std::atan2(v.y, horizontal) * reducePitchBy;
	const float turn = horizontal != 0.0f ? (a.z * v.x - a.x * v.z) / horizontal : 0.0f;
	const float bank = std::atan2(turn / gravityForBanking, 1.0f);
	const float cb = std::cos(bank);
	const float sb = std::sin(bank);
	const Rows rollX {{{1.0f, 0.0f, 0.0f}, {0.0f, cb, -sb}, {0.0f, sb, cb}}};
	const float cp = std::cos(-pitch);
	const float sp = std::sin(-pitch);
	const Rows pitchZ {{{cp, -sp, 0.0f}, {sp, cp, 0.0f}, {0.0f, 0.0f, 1.0f}}};
	// the original's constant is the double -pi / 2
	const auto m = Multiply(Multiply(Multiply(RotationY(-std::numbers::pi_v<float> * 0.5f), rollX), pitchZ), RotationY(yaw));
	return ToAtomRotation(m);
}

void openblack::psys::RegisterFlockRules()
{
	RegisterModifier("UR_FollowTargets", MakeModifierOf<FollowTargets>);
	RegisterModifier("UR_Flocking", MakeModifierOf<Flocking>);
	RegisterCondition("EventConditionAtomNearVillagers", &AtomNearVillagers);
}
