/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Shield.h"

#include <cmath>

#include <algorithm>
#include <bit>
#include <numbers>

#include <glm/geometric.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include "Audio/Services/SpellSounds.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "Locator.h"
#include "Particles/PSys.h"
#include "Particles/PSysManagerState.h"
#include "Particles/PSysRegistry.h"
#include "Particles/SoundAction.h"
#include "Particles/SpellLink.h"

using namespace openblack::psys;

namespace
{
constexpr float k_TwoPi = 2.0f * std::numbers::pi_v<float>;

/// The spheres, the newest first, in the particle system's state (openblack::Locator::particleSystem)
std::vector<shields::DefensiveSphere>& Spheres()
{
	return openblack::Locator::particleSystem::value().GetState().spheres;
}

/// An entity kept in a float of the per-atom / per-slot data, bit for bit
float EntityBits(entt::entity entity)
{
	return std::bit_cast<float>(static_cast<uint32_t>(entity));
}
entt::entity EntityFromBits(float bits)
{
	return static_cast<entt::entity>(std::bit_cast<uint32_t>(bits));
}

/// The atom's last global movement: the drawn position minus the one before (0 until it was drawn twice)
glm::vec3 LastGlobalMovement(const Atom& atom)
{
	return atom.drawn ? atom.current.position - atom.previous.position : glm::vec3(0.0f);
}

/// From the effect's targets: the last point taken out, else the last object taken out (its position); false when empty
bool TakeTargetPosition(Effect& effect, glm::vec3& out)
{
	if (effect.TakeTargetPoint(out))
	{
		return true;
	}
	const auto object = effect.TakeTarget();
	if (object == entt::null)
	{
		return false;
	}
	auto& registry = openblack::Locator::entitiesRegistry::value();
	if (const auto* transform = registry.TryGet<const openblack::ecs::components::Transform>(object); transform != nullptr)
	{
		out = transform->position; // the object's position as a point
	}
	return true;
}

/// The orientation the spark and vapour rules give an atom from a point about the sphere's centre: the identity turned
/// by the colatitude pi/2 - atan2(y, |xz|) (x' = c x + s y, y' = c y - s x on every row), then by the yaw atan2(z, x)
/// (x' = c x - s z, z' = c z + s x). The Y row ends up along the point's direction.
glm::mat3 OrientAlong(const glm::vec3& p, const glm::mat3& start)
{
	const float yaw = std::atan2(p.z, p.x);
	const float colatitude = std::numbers::pi_v<float> / 2.0f - std::atan2(p.y, std::sqrt(p.z * p.z + p.x * p.x));
	glm::mat3 m = start; // the columns are the original matrix rows
	const float cc = std::cos(colatitude);
	const float sc = std::sin(colatitude);
	for (int i = 0; i < 3; ++i)
	{
		const float x = m[i].x;
		const float y = m[i].y;
		m[i].x = cc * x + sc * y;
		m[i].y = cc * y - sc * x;
	}
	const float cy = std::cos(yaw);
	const float sy = std::sin(yaw);
	for (int i = 0; i < 3; ++i)
	{
		const float x = m[i].x;
		const float z = m[i].z;
		m[i].x = cy * x - sy * z;
		m[i].z = cy * z + sy * x;
	}
	return m;
}

// ---- the rules ----

/// UR_AddDefensiveSphere (SphereRadius, IsMagical). The first time the collection runs it a DefensiveSphere is made at
/// the parent's position with the provider's value; after that only its radius follows the provider. The centre never
/// moves.
class AddDefensiveSphere final: public Modifier
{
public:
	explicit AddDefensiveSphere(const Object& object)
	    : radius(object.String("SphereRadius"))
	    , magical(object.Bool("IsMagical", false))
	{
	}
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& slot) const override
	{
		// slot.state.x: the sphere's id (bits)
		if (slot.first)
		{
			slot.first = false;
			// the parent atom's position (in the collection's frame), or the origin
			const glm::vec3 centre = collection.parent != nullptr ? collection.parent->position : effect.GetOrigin();
			slot.state.x =
			    std::bit_cast<float>(shields::AddDefensiveSphere(effect, centre, effect.FloatProvider(radius, 0.0f)));
			return true;
		}
		if (auto* sphere = shields::Find(std::bit_cast<uint32_t>(slot.state.x)); sphere != nullptr)
		{
			sphere->radius = effect.FloatProvider(radius, sphere->radius);
		}
		return true;
	}
	std::string radius;
	bool magical; ///< read; the code does not look at it
};

/// CheckShieldDeflections: an atom not yet deflected (flag 0x08) that is inside a shield sparks there and sends
/// SpellEvent 4 {its position, the last move, 1, the shield's spell}; on 0 it takes the reflected velocity and, per its
/// properties, the deflected flag, another group and the close down of its effect. No ported spell file uses the class.
class CheckShieldDeflections final: public Modifier
{
public:
	explicit CheckShieldDeflections(const Object& object)
	    : groupIfDeflected(object.Int("GroupToMoveToIfDeflected", -1))
	    , closeDownIfDeflected(object.Bool("CloseDownSpellIfDeflected", false))
	    , setDeflected(object.Bool("SetDeflectedWhenDeflected", false))
	    , movementOnly(object.Bool("CheckMovementOnly", false))
	{
	}
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		const glm::vec3 global = effect.GlobalPosition(atom);
		if ((atom.flags & 8u) != 0)
		{
			return true;
		}
		const auto* sphere = shields::FindShieldContainingPoint(global, 0.0f);
		if (sphere == nullptr)
		{
			return true;
		}
		shields::AddImpactTarget(*sphere, global);
		const SpellEventInfo event {.type = SpellEventInfo::Type::HitSpell,
		                            .position = global,
		                            .velocity = LastGlobalMovement(atom),
		                            .strength = 1.0f,
		                            .checkShields = false,
		                            .target = shields::SpellOf(*sphere)};
		if (effect.SendSpellEvent(event) != 0)
		{
			return true;
		}
		shields::DeflectOffShield(*sphere, global, atom.velocity);
		if (setDeflected)
		{
			atom.flags |= 8u;
		}
		// not ported: GroupToMoveToIfDeflected (moving the atom to another group); the property is read and ignored
		if (closeDownIfDeflected)
		{
			effect.CloseDown();
		}
		return true;
	}
	int groupIfDeflected;
	bool closeDownIfDeflected, setDeflected, movementOnly; ///< CheckMovementOnly is read, not used here
};

/// UpdateRuleShieldSpark, a rule that creates atoms: NextGroups, PCreator, SphereRadius, MaxNumAtomsForCollection,
/// SparkLife, WiggleAmpl, SoundSpark
class ShieldSpark final: public Modifier
{
public:
	explicit ShieldSpark(const Object& object)
	    : creator(object.String("PCreator"))
	    , nextGroups(object.Array("NextGroups"))
	    , radius(object.String("SphereRadius"))
	    , maxAtoms(object.Int("MaxNumAtomsForCollection", -1))
	    , sparkLife(object.Float("SparkLife", 1.0f))
	    , wiggle(object.Float("WiggleAmpl", 0.0f))
	    , sound(ReadSoundAction(object, "SoundSpark"))
	{
	}
	[[nodiscard]] bool Creates() const override { return true; }

	/// A spark atom per impact the shield's effect was given (while fewer than MaxNum),
	/// at the impact in the collection's frame, with its sound, three random angles and a wiggle count 2..5; the sparks
	/// older than SparkLife go, the others draw their arcs (ModifySubCollection)
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		const auto* particleCreator = effect.FindCreator(creator);
		if (particleCreator == nullptr)
		{
			return false;
		}
		glm::vec3 impact;
		while (effect.TargetPointCount() + effect.GetTargets().size() > 0 && TakeTargetPosition(effect, impact))
		{
			if (maxAtoms != -1 && maxAtoms <= static_cast<int>(collection.atoms.size()))
			{
				continue;
			}
			const glm::vec3 local = effect.GlobalToLocal(collection, impact);
			auto& atom = effect.NewAtom(collection, particleCreator, nextGroups);
			atom.position = local;
			openblack::audio::spell_sounds::StartSound(effect, atom, sound);
			// The atom's data: Random(2 pi) x 3; Random(4) + 2 truncated toward zero (a float draw, not Rand); the
			// orientation (rebuilt from the position each step: the spark atom never moves)
			const float a1 = effect.Random(k_TwoPi);
			const float a2 = effect.Random(k_TwoPi);
			const float a3 = effect.Random(k_TwoPi);
			const auto wiggles = static_cast<int>(effect.Random(4.0f) + 2.0f);
			atom.data[this] = glm::vec4(a1, a2, a3, static_cast<float>(wiggles));
		}
		for (size_t i = 0; i < collection.atoms.size();)
		{
			auto& atom = *collection.atoms[i];
			if (effect.AtomAge(atom) > sparkLife)
			{
				collection.atoms.erase(collection.atoms.begin() + static_cast<std::ptrdiff_t>(i));
				continue;
			}
			if (!atom.subCollections.empty())
			{
				ModifySubCollection(effect, atom, *atom.subCollections.front());
			}
			++i;
		}
		return true;
	}

	/// The spark's atoms make an arc from the sphere's centre to the impact: atom i of N
	/// (counted down from N) at u = i / N along the impact's direction, u R out, wiggling across it by
	/// R WiggleAmpl sin(k u pi) sin(4 t + a3) and R WiggleAmpl sin((k + 1) u pi) sin(2 t + a3); scale 0.04 R (1.5 + 0.4
	/// (1 + sin(a3 - 6 t))), alpha clamp((1 - t / SparkLife) 510, 0, 255)
	void ModifySubCollection(Effect& effect, const Atom& spark, Collection& sub) const
	{
		if (radius.empty() || sub.atoms.empty())
		{
			return;
		}
		const auto data = spark.data.at(this);
		const float a3 = data.z;
		const int wiggles = static_cast<int>(data.w);
		const float r = effect.FloatProvider(radius, 0.0f);
		const auto n = static_cast<float>(sub.atoms.size());
		const float t = effect.AtomAge(spark);
		const float scale = ((std::sin(a3 - 6.0f * t) + 1.0f) * 0.4f + 1.5f) * 0.04f * r;
		const float alpha = std::clamp((1.0f - t / sparkLife) * 510.0f, 0.0f, 255.0f);
		const glm::mat3 m = OrientAlong(spark.position, glm::mat3(1.0f));
		const float pi = std::numbers::pi_v<float>;
		float index = n;
		for (auto& atomPtr : sub.atoms)
		{
			auto& atom = *atomPtr;
			const float u = index / n;
			const float w1 = std::sin(static_cast<float>(wiggles) * u * pi) * std::sin(4.0f * t + a3) * wiggle;
			const float w2 = std::sin(static_cast<float>(wiggles + 1) * u * pi) * std::sin(2.0f * t + a3) * wiggle;
			const glm::vec3 p = m[0] * (r * w1) + m[1] * (u * r) + m[2] * (r * w2);
			// (the code then tests LocalToGlobal(p) against the land at the atom's scale and writes back the same point)
			atom.ruleScale = scale;
			// the max(dt, eps) is a port guard: the original multiplies by 1/dt directly
			atom.velocity = (p - atom.position) / std::max(effect.GetDt(), 1e-4f);
			atom.position = p;
			atom.colour[3] = static_cast<uint8_t>(alpha);
			index -= 1.0f;
		}
	}

	std::string creator;
	std::vector<int> nextGroups;
	std::string radius;
	int maxAtoms;
	float sparkLife;
	float wiggle;
	SoundAction sound;
};

/// UR_InitialSpin (ScaleAngularVelocity, MaxAngularVelocity, TimeToFade, all three with the editor range 0..1 that
/// nothing applies). Once the effect has a player: spin = the process info's curl x ScaleAngularVelocity, clamped to
/// +-MaxAngularVelocity. Every step the atom turns about its own Y axis by spin (1 - clamp(age / TimeToFade, 0, 1)) dt.
/// The defaults are the original's (1.0, 1.0 and 2.0); SF_DefenseSphere.txt gives all three anyway (1, 5 and 5)
class InitialSpin final: public Modifier
{
public:
	explicit InitialSpin(const Object& object)
	    : scale(object.Float("ScaleAngularVelocity", 1.0f))
	    , maxSpeed(object.Float("MaxAngularVelocity", 1.0f))
	    , timeToFade(object.Float("TimeToFade", 2.0f))
	{
	}
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		auto& data = atom.data[this]; // x: spin, y: set
		if (data.y == 0.0f)
		{
			if (effect.GetPlayer() < 0)
			{
				return true; // no player: tried again next step
			}
			data.x = std::clamp(effect.GetProcessInfo().curl * scale, -maxSpeed, maxSpeed);
			data.y = 1.0f;
		}
		const float k = std::clamp(effect.AtomAge(atom) / timeToFade, 0.0f, 1.0f);
		const float angle = (data.x - data.x * k) * effect.GetDt();
		const float c = std::cos(angle);
		const float s = std::sin(angle);
		// rows 0 and 2: r0' = c r0 + s r2, r2' = c r2 - s r0
		auto& m = atom.rotation;
		const glm::vec3 r0 = m[0];
		const glm::vec3 r2 = m[2];
		m[0] = c * r0 + s * r2;
		m[2] = c * r2 - s * r0;
		return true;
	}
	float scale, maxSpeed, timeToFade;
};

/// UR_VapourEndEffect (ScaleFactor): the magic shield's surface patches. The atom sits
/// on its parent (the sphere tracer), turned the first time so that its Y axis points out from the centre and after
/// that by the angle between the parent's last two positions (axis last x now, acos of their normalised dot) when it
/// moved more than 1e-4. Its scale is ScaleFactor and its alpha clamp(age x 30, 0, 255).
class VapourEndEffect final: public Modifier
{
public:
	explicit VapourEndEffect(const Object& object)
	    : scaleFactor(object.String("ScaleFactor"))
	{
	}
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		if (scaleFactor.empty())
		{
			return true;
		}
		const auto* parent = atom.collection->parent;
		// the parent's position, or the origin
		const glm::vec3 p = parent != nullptr ? parent->position : effect.GetOrigin();
		auto& data = atom.data[this]; // x: done the first time
		glm::mat3 m = atom.rotation;
		if (data.x == 0.0f)
		{
			data.x = 1.0f;
			m = OrientAlong(p, m);
		}
		else
		{
			// the parent's velocity x dt
			const glm::vec3 last = p - (parent != nullptr ? parent->velocity : glm::vec3(0.0f)) * effect.GetDt();
			const glm::vec3 d = glm::abs(last - p);
			if (d.x > 1e-4f || d.y > 1e-4f || d.z > 1e-4f)
			{
				const float lengths = glm::length(last) * glm::length(p);
				const glm::vec3 axis = glm::cross(last, p);
				if (lengths > 0.0f && glm::dot(axis, axis) > 0.0f)
				{
					const float angle = std::acos(std::clamp(glm::dot(last, p) / lengths, -1.0f, 1.0f));
					// The quaternion (cos(a/2), sin(a/2) n), its matrix (m1 = xy + wz: the rows of the right-handed
					// turn by +a, as glm::rotate(+a, n)) and the rows times it (r_k' = r_k M), so every row turns by +a
					// about n = cross(last, p). (approximate) glm::rotate is not the quaternion's cell arithmetic: the
					// last bits
					const glm::vec3 n = glm::normalize(axis);
					for (int i = 0; i < 3; ++i)
					{
						m[i] = glm::rotate(m[i], angle, n);
					}
				}
			}
		}
		atom.position = p;
		atom.rotation = m;
		atom.ruleScale = effect.FloatProvider(scaleFactor, 1.0f);
		atom.colour[3] = static_cast<uint8_t>(std::clamp(effect.AtomAge(atom) * 30.0f, 0.0f, 255.0f));
		return true;
	}
	std::string scaleFactor;
};

/// SetCollectionAlpha (Alpha): the collection's alpha byte =
/// clamp(provider, 0, 255); detached without a provider
class SetCollectionAlpha final: public Modifier
{
public:
	explicit SetCollectionAlpha(const Object& object)
	    : alpha(object.String("Alpha"))
	{
	}
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		if (alpha.empty())
		{
			return false;
		}
		collection.alpha = std::floor(std::clamp(effect.FloatProvider(alpha, 255.0f), 0.0f, 255.0f)); // truncated toward zero
		return true;
	}
	std::string alpha;
};

/// UR_AtomsAtEPTarget (NextGroups, PCreator, NumExtraPoints): the first time, an object target taken from the effect's
/// targets (the physical shield gives itself) and one atom per extra point of its mesh (5 when 0); every step the atoms
/// go to the object's extra points (else the object's position). When the object is gone its atoms are deleted.
class AtomsAtEPTarget final: public Modifier
{
public:
	explicit AtomsAtEPTarget(const Object& object)
	    : creator(object.String("PCreator"))
	    , nextGroups(object.Array("NextGroups"))
	{
	}
	[[nodiscard]] bool Creates() const override { return true; }
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& slot) const override
	{
		auto& registry = openblack::Locator::entitiesRegistry::value();
		// slot.state: x the target's entity (bits), y 1 once it has one, z the game turn (not used)
		if (slot.state.y == 0.0f && !effect.GetTargets().empty())
		{
			const auto target = effect.TakeTarget();
			if (target != entt::null && registry.Valid(target))
			{
				slot.state.x = EntityBits(target);
				slot.state.y = 1.0f;
				const auto* transform = registry.TryGet<const openblack::ecs::components::Transform>(target);
				if (transform == nullptr)
				{
					return false; // the object has no 3D object
				}
				// the mesh's extra points: none in the meshes that use it here (MSH_S_SOLID_SHIELD), so the default 5
				constexpr int k_DefaultPoints = 5;
				for (int i = 0; i < k_DefaultPoints; ++i)
				{
					effect.NewAtom(collection, effect.FindCreator(creator), nextGroups);
				}
			}
		}
		if (slot.state.y == 0.0f)
		{
			return true;
		}
		const auto target = EntityFromBits(slot.state.x);
		const auto* transform =
		    registry.Valid(target) ? registry.TryGet<const openblack::ecs::components::Transform>(target) : nullptr;
		if (transform == nullptr)
		{
			slot.state.y = 0.0f;
			collection.atoms.clear(); // each deleted from the atom list
			return true;
		}
		// no extra points: the 3D object's position (its matrix translation)
		for (auto& atom : collection.atoms)
		{
			atom->position = effect.GlobalToLocal(collection, transform->position);
		}
		return true;
	}
	std::string creator;
	std::vector<int> nextGroups;
};
} // namespace

// ---- the registry ----

uint32_t shields::AddDefensiveSphere(const Effect& owner, const glm::vec3& centre, float radius)
{
	DefensiveSphere sphere {
	    .id = openblack::Locator::particleSystem::value().GetState().nextSphere++,
	    .owner = &owner,
	    .centre = centre,
	    .radius = radius,
	};
	Spheres().insert(Spheres().begin(), sphere);
	return sphere.id;
}

void shields::RemoveDefensiveSphere(uint32_t id)
{
	std::erase_if(Spheres(), [id](const DefensiveSphere& sphere) { return sphere.id == id; });
}

void shields::RemoveAllOf(const Effect* owner)
{
	// (safety net) an effect destroyed after the service: its spheres went with it. The shutdown clears the effects
	// first (ShutDownServices)
	if (!openblack::Locator::particleSystem::has_value())
	{
		return;
	}
	std::erase_if(Spheres(), [owner](const DefensiveSphere& sphere) { return sphere.owner == owner; });
}

shields::DefensiveSphere* shields::Find(uint32_t id)
{
	const auto it = std::find_if(Spheres().begin(), Spheres().end(), [id](const DefensiveSphere& s) { return s.id == id; });
	return it != Spheres().end() ? &*it : nullptr;
}

const std::vector<shields::DefensiveSphere>& shields::All()
{
	return Spheres();
}

bool shields::IsPointInShield(const DefensiveSphere& sphere, const glm::vec3& point, float margin)
{
	const glm::vec3 d = point - sphere.centre;
	const float r = margin + sphere.radius;
	return r * r > glm::dot(d, d);
}

bool shields::HasCrossedIntoShield(const DefensiveSphere& sphere, const glm::vec3& from, const glm::vec3& to, float margin)
{
	return IsPointInShield(sphere, to, margin) && !IsPointInShield(sphere, from, margin);
}

bool shields::FindIntersect(const DefensiveSphere& sphere, const glm::vec3& from, const glm::vec3& to, float margin,
                            glm::vec3& out)
{
	// dir = normalize(to - from) (0 when to == from), w = from - c, A = dir.dir,
	// B = 2 w.dir, C = w.w - R^2, R = r + margin; t1 <= t2 the roots (x A x 0.5, A being 1 or 0); L = |to - from|
	out = to;
	glm::vec3 dir = to - from;
	float length = 0.0f;
	if (dir.x != 0.0f || dir.y != 0.0f || dir.z != 0.0f)
	{
		length = std::sqrt(glm::dot(dir, dir));
		dir *= 1.0f / length;
	}
	const glm::vec3 w = from - sphere.centre;
	const float a = glm::dot(dir, dir);
	const float b = 2.0f * glm::dot(w, dir);
	const float r = margin + sphere.radius;
	const float disc = b * b - 4.0f * a * (glm::dot(w, w) - r * r);
	if (disc < 0.0f)
	{
		return false;
	}
	const float root = std::sqrt(disc);
	const float t1 = (-b - root) * a * 0.5f;
	const float t2 = (root - b) * a * 0.5f;
	if (t2 < 0.0f || length < t1)
	{
		return false;
	}
	if (t1 < 0.0f && length < t2)
	{
		out = from; // both ends inside
		return true;
	}
	if (0.0f < t1)
	{
		if (t2 < length || length < t2)
		{
			out = from + dir * t1;
			return true;
		}
	}
	else if (t1 < 0.0f && t2 < length)
	{
		out = from;
	}
	return true;
}

void shields::DeflectOffShield(const DefensiveSphere& sphere, const glm::vec3& point, glm::vec3& velocity)
{
	// n = p - c, normalised unless it is 0; v -= 2 (v.n) n
	glm::vec3 n = point - sphere.centre;
	if (n.x != 0.0f || n.y != 0.0f || n.z != 0.0f)
	{
		n *= 1.0f / std::sqrt(glm::dot(n, n));
	}
	velocity -= 2.0f * glm::dot(velocity, n) * n;
}

const shields::DefensiveSphere* shields::FindShieldContainingPoint(const glm::vec3& point, float margin)
{
	for (const auto& sphere : Spheres())
	{
		if (IsPointInShield(sphere, point, margin))
		{
			return &sphere;
		}
	}
	return nullptr;
}

const shields::DefensiveSphere* shields::FindShieldCrossedInto(const glm::vec3& from, const glm::vec3& to, float margin)
{
	for (const auto& sphere : Spheres())
	{
		if (HasCrossedIntoShield(sphere, from, to, margin))
		{
			return &sphere;
		}
	}
	return nullptr;
}

void shields::AddImpactTarget(const DefensiveSphere& sphere, const glm::vec3& point)
{
	// a target point of the shield's effect
	if (sphere.owner != nullptr)
	{
		const_cast<Effect*>(sphere.owner)->AddTargetPoint(point);
	}
}

entt::entity shields::SpellOf(const DefensiveSphere& sphere)
{
	if (sphere.owner == nullptr || sphere.owner->GetSink() == nullptr)
	{
		return entt::null;
	}
	return sphere.owner->GetSink()->SpellEntity();
}

bool shields::DoAnyShieldDeflections(Effect& effect, Atom& atom, const glm::vec3& oldGlobal)
{
	const glm::vec3 global = effect.GlobalPosition(atom);
	const float margin = 1.25f * atom.baseScale * atom.ruleScale;
	const auto* sphere = FindShieldCrossedInto(oldGlobal, global, margin);
	if (sphere == nullptr)
	{
		return false;
	}
	glm::vec3 hit;
	FindIntersect(*sphere, oldGlobal, global, margin, hit);
	AddImpactTarget(*sphere, hit);
	const SpellEventInfo event {.type = SpellEventInfo::Type::HitSpell,
	                            .position = hit,
	                            .velocity = LastGlobalMovement(atom),
	                            .strength = 1.0f,
	                            .checkShields = false,
	                            .target = SpellOf(*sphere)};
	// the spell event on the atom's own effect: 1 = its spell lets it through
	if (effect.SendSpellEvent(event) != 0)
	{
		return false;
	}
	atom.position = atom.collection != nullptr ? effect.GlobalToLocal(*atom.collection, hit) : hit;
	DeflectOffShield(*sphere, hit, atom.velocity);
	return true;
}

void openblack::psys::RegisterShieldRules()
{
	RegisterModifier("UR_AddDefensiveSphere", MakeModifierOf<AddDefensiveSphere>);
	RegisterModifier("CheckShieldDeflections", MakeModifierOf<CheckShieldDeflections>);
	RegisterModifier("UpdateRuleShieldSpark", MakeModifierOf<ShieldSpark>);
	RegisterModifier("UR_InitialSpin", MakeModifierOf<InitialSpin>);
	RegisterModifier("UR_VapourEndEffect", MakeModifierOf<VapourEndEffect>);
	RegisterModifier("SetCollectionAlpha", MakeModifierOf<SetCollectionAlpha>);
	RegisterModifier("UR_AtomsAtEPTarget", MakeModifierOf<AtomsAtEPTarget>);
}
