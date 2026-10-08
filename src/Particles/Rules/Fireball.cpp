/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The fireball's rules (SF_FireBallThrow / PU / PU2): the throw
// (CreateWithInitialDirection), the flight and bounce (UpdateRuleGravityWithFloor), the game object that burns
// (AttatchFireBallToAtom -> Magic/Objects/MagicFireBall), the per-step event (EventAlways), the deflection flag and its
// conditions, the spin, the trail, the light's fade with height and the steam sub-collections. See
// docs/bw1-notes/miracles.md, "Bola de fuego".

#include <cmath>
#include <cstring>

#include <algorithm>
#include <array>
#include <numbers>
#include <unordered_map>
#include <vector>

#include <LNDFile.h>
#include <glm/geometric.hpp>

#include "3D/LandIslandInterface.h"
#include "Audio/Audio.h"
#include "Audio/Services/SpellSounds.h"
#include "Camera/Camera.h"
#include "ECS/Physics/PhysicsBody.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Weather/Weather.h"
#include "Locator.h"
#include "Magic/Objects/MagicFireBall.h"
#include "Particles/PSys.h"
#include "Particles/PSysFile.h"
#include "Particles/PSysManager.h"
#include "Particles/PSysRegistry.h"
#include "Particles/PSysWaterRings.h"
#include "Particles/Rules/Shield.h"
#include "Particles/SoundAction.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
constexpr uint32_t k_Deflected = 0x08; // the atom's deflected flag (bit 3)

float LandAt(float x, float z)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)) : 0.0f;
}

/// Whether the map cell at a world point is water: ecs::sea_cells::IsWater
bool IsWaterAt(const glm::vec3& point)
{
	return ecs::sea_cells::IsWater(point);
}

/// The atom, or one of its parents, has been deflected (EC_DeflectionInAtomsHierarchy)
bool DeflectedInHierarchy(const Atom* atom)
{
	for (; atom != nullptr; atom = atom->collection != nullptr ? atom->collection->parent : nullptr)
	{
		if ((atom->flags & k_Deflected) != 0)
		{
			return true;
		}
	}
	return false;
}

// ---- conditions ----

/// EventConditionAtomHasBeenDeflected: the atom's flag bit 3
bool HasBeenDeflected(const Effect& /*effect*/, const Object& /*object*/, const Atom* atom, const Collection& /*c*/)
{
	return atom != nullptr && (atom->flags & k_Deflected) != 0;
}

/// EC_DeflectionInAtomsHierarchy (an atom) / EC_DeflectionInCollectionsHierarchy (a collection: its parent atom and
/// theirs)
bool DeflectionInHierarchy(const Effect& /*effect*/, const Object& /*object*/, const Atom* atom, const Collection& collection)
{
	return DeflectedInHierarchy(atom != nullptr ? atom : collection.parent);
}

/// EventConditionAtomCloseWater: not deflected, lower than CutOffHeight above the land
/// and over water (or outside the land)
bool CloseWater(const Effect& effect, const Object& object, const Atom* atom, const Collection& /*c*/)
{
	if (atom == nullptr || (atom->flags & k_Deflected) != 0)
	{
		return false;
	}
	const auto p = effect.GlobalPosition(*atom);
	if (!(p.y - LandAt(p.x, p.z) - object.Float("CutOffHeight", 0.0f) < 0.0f))
	{
		return false;
	}
	return IsWaterAt(p);
}

/// EventConditionFireBallSteam: not deflected and raining or snowing there
bool FireBallSteam(const Effect& effect, const Object& /*object*/, const Atom* atom, const Collection& /*c*/)
{
	if (atom == nullptr || (atom->flags & k_Deflected) != 0)
	{
		return false;
	}
	return weather::GetMaxRainingOrSnowingAt(effect.GlobalPosition(*atom)) > 0.0f;
}

// ---- motion ----

/// UpdateRuleGravityWithFloor: the gravity rule's flight (gravity, damping, wind) with a bounce on the land, the impact
/// sound and ripple, and shield deflections. The spell files that use it: SF_ExplodeObject (fragments, NO_SOUND: no
/// impact, no ripple) and SF_FireBallThrow / PU / PU2 (the thrown fireball).
class GravityWithFloor final: public Modifier
{
public:
	/// the original defaults where the file has no such property
	explicit GravityWithFloor(const Object& object)
	    : gravity(object.Float("Gravity", 10.0f))
	    , maxSpeed(object.Float("MaxSpeed", 100.0f))
	    , damping(object.Float("Damping", 0.0f))
	    , windMagnification(object.Float("WindMagnification", 100.0f))
	    , useDamping(object.Bool("UseDamping", false))
	    , useWind(object.Bool("UseWind", true))
	    , disableWindForNonHuman(object.Bool("DisableWindForNonHuman", false))
	    , disableDampingForNonHuman(object.Bool("DisableDampingForNonHuman", false))
	    , horizontalBounce(object.Float("DampingHorozontalBounce", 0.5f))
	    , verticalBounce(object.Float("DampingVerticalBounce", 0.5f))
	    , groundDrag(object.Float("GroundDrag", 0.0f))
	    , impactSound(ReadSoundAction(object, "ImpactSound")) // (NO_SOUND: action -1)
	    , impactSoundCondition(object.String("ImpactSoundCondition"))
	    , impactSmall(object.Float("ImpactSpeedSmall", 5.0f))
	    , impactMedium(object.Float("ImpactSpeedMedium", 20.0f))
	    , impactLarge(object.Float("ImpactSpeedLarge", 40.0f))
	    , minAlpha(object.Int("MinAlphaForImpactSoundOrRipple", 60))
	    , useSurface(object.Bool("UseSurfaceForBounce", false))
	    , checkShields(object.Bool("CheckShieldDeflections", false))
	{
	}
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		const bool human = effect.GetSink() != nullptr && effect.GetSink()->IsHumanPlayerCasting();
		const bool damped = useDamping && (!disableDampingForNonHuman || human);
		const bool windy = useWind && (!disableWindForNonHuman || human);
		const float dt = effect.GetDt();
		for (auto& atomPointer : collection.atoms)
		{
			auto& atom = *atomPointer;
			glm::vec3 v = atom.velocity;
			const auto global = effect.GlobalPosition(atom);
			if (windy)
			{
				// v += (0.1 x magnification x wind - v) x damping x dt (the smoothed wind)
				const glm::vec3 wind = weather::GetWindAt(global, true) * windMagnification * 0.1f;
				v += (wind - v) * damping * dt;
			}
			else if (damped)
			{
				v *= 1.0f - dt * damping;
			}
			atom.velocity = v;
			atom.position += v * dt;
			// the new global position against the land (the original can also use a surface object, not ported)
			auto p = effect.GlobalPosition(atom);
			const float land = LandAt(p.x, p.z);
			// the particle's lowest point is its global y (every kind but meshes; a mesh's own lowest point is not used
			// here)
			if (!(p.y < land))
			{
				// in the air: v.y -= clamp(v.y + MaxSpeed, 0, 1) x Gravity x atom gravity x dt
				v.y -= std::clamp(v.y + maxSpeed, 0.0f, 1.0f) * gravity * atom.gravity * dt;
			}
			else
			{
				// under the land: back on it, and a bounce when it moves into the slope
				p.y += land - p.y;
				SetGlobal(effect, atom, p);
				const glm::vec3 normal = ecs::physics::LandscapeNormal(p);
				const float into = glm::dot(v, normal);
				if (into < 0.0f)
				{
					ImpactSound(effect, atom, p, std::abs(into));
					const glm::vec3 normalPart = normal * into;
					glm::vec3 tangent = v - normalPart;
					glm::vec3 direction = tangent;
					float length = glm::length(tangent);
					// a tangent of almost zero length: the fallback direction (1e-4, 0, 0)
					if (glm::dot(tangent, tangent) < 0.0001f)
					{
						direction = glm::vec3(0.0001f, 0.0f, 0.0f);
						length = 0.0001f;
					}
					direction = length > 0.0f ? direction / glm::length(direction) : glm::vec3(0.0f);
					// the ground drag takes dt x GroundDrag off the sliding speed (at most all of it)
					float drag = dt * groundDrag;
					drag = drag > 0.0f ? (drag < length ? drag : length) : 0.0f;
					tangent -= direction * drag;
					float bounce = horizontalBounce;
					if (useSurface)
					{
						// the surface's factor (1, 0.2 off the land and on water, 1.25 for 9)
						static constexpr std::array<float, 10> k_Surface = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
						                                                    1.0f, 0.2f, 0.2f, 1.0f, 1.25f};
						const int surface = ecs::sea_cells::GetSurfaceType(p);
						bounce *= surface >= 0 && surface < 10 ? k_Surface[surface] : 1.0f;
					}
					v = tangent * bounce + normalPart * -verticalBounce;
				}
			}
			atom.velocity = v;
			if (checkShields)
			{
				// from the global position taken before the move
				shields::DoAnyShieldDeflections(effect, atom, global);
			}
		}
		return true;
	}

	/// The impact sound and the ripple on water (always on, no property) when the atom is visible enough, hit fast
	/// enough and has not got that sound yet
	void ImpactSound(Effect& effect, Atom& atom, const glm::vec3& position, float speed) const
	{
		if (static_cast<int>(atom.colour[3]) < minAlpha || impactSound.action == -1 || speed < impactSmall)
		{
			return;
		}
		if (audio::spell_sounds::GetSoundOfAction(atom, impactSound.action) != nullptr)
		{
			return;
		}
		if (!impactSoundCondition.empty() && !effect.ConditionForAtom(impactSoundCondition, atom))
		{
			return;
		}
		auto action = impactSound;
		action.size = audio::spell_sounds::SizeFromImpactSpeed(speed, impactMedium, impactLarge);
		audio::spell_sounds::StartSound(effect, atom, action);
		// the ripple (its last ripple (0, 0, 0) when made), only on water. The atom's radius: base scale x rule scale
		// with a creator, else 1
		const float radius = atom.creator != nullptr ? atom.baseScale * atom.ruleScale : 1.0f;
		auto& data = atom.data[this];
		glm::vec3 last(data.x, data.y, data.z);
		water_rings::AddParticleRipple(position, radius, last, k_RippleDistance);
		data = glm::vec4(last, data.w);
	}

	static constexpr float k_RippleDistance = 2.0f; ///< a fixed value, no property

	/// A global position into the atom's local position
	static void SetGlobal(const Effect& effect, Atom& atom, const glm::vec3& global)
	{
		const auto* collection = atom.collection;
		if (collection != nullptr && collection->hierarchy && collection->parent != nullptr)
		{
			const auto& parent = *collection->parent;
			atom.position = glm::transpose(parent.rotation) * (global - effect.GlobalPosition(parent));
			return;
		}
		atom.position = global;
	}

	float gravity, maxSpeed, damping, windMagnification;
	bool useDamping, useWind, disableWindForNonHuman, disableDampingForNonHuman;
	float horizontalBounce, verticalBounce, groundDrag;
	SoundAction impactSound;
	std::string impactSoundCondition;
	float impactSmall, impactMedium, impactLarge;
	int minAlpha;
	bool useSurface, checkShields;
};

/// The hand's direction pitched up 0.3 rad (clamped to +-pi/2)
glm::vec3 PitchedUp(const glm::vec3& d)
{
	const float yaw = std::atan2(d.z, d.x);
	float pitch = std::atan2(d.y, std::sqrt(d.x * d.x + d.z * d.z)) + 0.3f;
	pitch = std::clamp(pitch, -std::numbers::pi_v<float> * 0.5f, std::numbers::pi_v<float> * 0.5f);
	return {std::cos(yaw) * std::cos(pitch), std::sin(pitch), std::sin(yaw) * std::cos(pitch)};
}

/// The hand's speed through (0 -> 0, 50 -> 50, 450 -> 200), 200 above 450
float SpeedFromHand(float speed)
{
	if (!(speed < 450.0f))
	{
		return 200.0f;
	}
	static constexpr std::array<float, 3> k_In = {0.0f, 50.0f, 450.0f};
	static constexpr std::array<float, 3> k_Out = {0.0f, 50.0f, 200.0f};
	const int i = speed < 50.0f ? 0 : 1;
	return (speed - k_In[i]) / (k_In[i + 1] - k_In[i]) * (k_Out[i + 1] - k_Out[i]) + k_Out[i];
}

/// CreateWithInitialDirection (a once-only create rule)
/// (inferred) the property defaults below
class CreateWithInitialDirection final: public Modifier
{
public:
	explicit CreateWithInitialDirection(const Object& object)
	    : creator(object.String("PCreator"))
	    , nextGroups(object.Array("NextGroups"))
	    , numAtoms(object.Int("NumAtoms", 1))
	    , sound(ReadSoundAction(object, "SoundOfCreate"))
	    , predictFraction(object.Float("PredictFraction", 0.0f))
	    , elevate(object.Bool("Elevate", false))
	    , predictStartPos(object.Bool("PredictStartPos", false))
	    , verticalScatterMin(object.Float("VerticalScatterAtMinSpeed", 0.0f))
	    , verticalScatterMax(object.Float("VerticalScatterAtMaxSpeed", 0.0f))
	    , horizontalScatterMin(object.Float("HorozScatterAtMinSpeed", 0.0f))
	    , horizontalScatterMax(object.Float("HorozScatterAtMaxSpeed", 0.0f))
	    , speedRandomFrac(object.Float("SpeedRandomFrac", 0.0f))
	    , initScale(object.String("InitScaleFP"))
	{
	}
	[[nodiscard]] bool Creates() const override { return true; }
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		const auto* pcreator = effect.FindCreator(creator);
		if (pcreator == nullptr)
		{
			return false;
		}
		const glm::vec3 gesture = effect.GetProcessInfo().handPos; // the gesture position
		glm::vec3 direction(0.0f);
		float speed = 0.0f;
		const bool human = effect.GetSink() != nullptr && effect.GetSink()->IsHumanPlayerCasting();
		if (!human)
		{
			// a script, computer or creature cast: a ballistic arc from the gesture position to 0.8 of the way to the origin,
			// in max(0.025 D, 0.5) s, or at 30 degrees (0.5237 rad) when that would be steeper: v = delta / t - (0, -g x
			// 0.5 t, 0) (the target at 0.8 and the origin fallback are unverified). g = the group's gravity, 30 without
			// one
			float g = 30.0f;
			for (const auto& slot : collection.modifiers)
			{
				if (const auto* rule = dynamic_cast<const GravityWithFloor*>(slot.modifier); rule != nullptr)
				{
					g = rule->gravity;
					break;
				}
			}
			glm::vec3 origin = effect.GetOrigin();
			const glm::vec3 flat(origin.x - gesture.x, 0.0f, origin.z - gesture.z);
			if (flat.x * flat.x + flat.z * flat.z < 0.1f)
			{
				origin = glm::vec3(gesture.x + 0.1f, gesture.y, gesture.z + 0.1f);
			}
			const glm::vec3 target = gesture + (origin - gesture) * 0.8f;
			const glm::vec3 delta = target - gesture;
			const float distance = std::sqrt(delta.x * delta.x + delta.z * delta.z);
			float time = distance * 0.025f;
			if (time < 0.5f)
			{
				time = 0.5f;
			}
			glm::vec3 v = delta / time - glm::vec3(0.0f, -g * 0.5f * time, 0.0f);
			const float horizontal2 = v.x * v.x + v.z * v.z;
			// the re-solve for any launch that moves: |v|^2 > 0.01 (as a double), then steeper than 30 degrees: the tan
			// of the double 0.52370351552963257, compared in double precision with v.y / sqrt(horizontal2);
			// horizontal2 is rounded to a float first, then + y x y in double precision
			if (static_cast<double>(horizontal2) + static_cast<double>(v.y) * static_cast<double>(v.y) > 0.01)
			{
				const double slope = std::tan(0.52370351552963257);
				if (static_cast<double>(v.y) / std::sqrt(static_cast<double>(horizontal2)) > slope)
				{
					// (delta.y - distance x tan) x (-2 / g), all in double precision
					double time2 = (static_cast<double>(delta.y) - static_cast<double>(distance) * slope) *
					               (-2.0 / static_cast<double>(g));
					if (time2 < 0.0) // below 0: 0.1, then the square root
					{
						time2 = static_cast<double>(0.1f);
					}
					time = static_cast<float>(std::sqrt(time2));
					v = delta / time - glm::vec3(0.0f, -g * 0.5f * time, 0.0f);
				}
			}
			speed = glm::length(v);
			direction = speed > 0.0f ? v / speed : glm::vec3(0.0f);
		}
		else
		{
			const glm::vec3 raw = effect.GetDirection(); // the hand's movement
			const float length = glm::length(raw);
			direction = PitchedUp(length > 0.0f ? raw / length : glm::vec3(0.0f));
			speed = SpeedFromHand(length);
		}
		// the sound's size from the speed / 200, > 0.6 large, > 0.3 medium
		const float fraction = std::clamp(speed / 200.0f, 0.0f, 1.0f);
		auto action = sound;
		action.size = audio::spell_sounds::SizeFromThrow(fraction);
		glm::vec3 start = gesture;
		if (predictStartPos)
		{
			start += effect.GetDirection() * effect.GetDt() * predictFraction;
		}
		for (int i = 0; i < numAtoms; ++i)
		{
			auto& atom = effect.NewAtom(collection, pcreator, nextGroups);
			atom.baseScale *= effect.FloatProvider(initScale, 1.0f);
			glm::vec3 d = direction;
			float s = speed;
			if (elevate)
			{
				float yaw = std::atan2(d.z, d.x);
				float pitch = std::atan2(d.y, std::sqrt(d.x * d.x + d.z * d.z));
				if (i >= 1)
				{
					// the others around the first, on a circle of the scatter at this speed
					const float a =
					    static_cast<float>(i - 1) * 2.0f * std::numbers::pi_v<float> / static_cast<float>(numAtoms - 1);
					pitch += std::sin(a) * ((verticalScatterMax - verticalScatterMin) * fraction + verticalScatterMin);
					yaw += std::cos(a) * ((horizontalScatterMax - horizontalScatterMin) * fraction + horizontalScatterMin);
					if (speedRandomFrac != 0.0f)
					{
						s = (1.0f - effect.Random(speedRandomFrac)) * speed;
					}
				}
				pitch = std::clamp(pitch, -std::numbers::pi_v<float> * 0.5f, std::numbers::pi_v<float> * 0.5f);
				d = glm::vec3(std::cos(yaw) * std::cos(pitch), std::sin(pitch), std::sin(yaw) * std::cos(pitch));
			}
			atom.position = start;
			atom.velocity = d * s;
			// not ported: for the local interface's cast the atom gets a draw offset that draws it from the hand to the
			// start, so here the ball appears at the gesture point
			audio::spell_sounds::StartSound(effect, atom, action);
		}
		return false; // once only
	}
	std::string creator;
	std::vector<int> nextGroups;
	int numAtoms;
	SoundAction sound;
	float predictFraction;
	bool elevate, predictStartPos;
	float verticalScatterMin, verticalScatterMax, horizontalScatterMin, horizontalScatterMax, speedRandomFrac;
	std::string initScale;
};

/// The fireball atoms' state (Locator::particleSystem): each atom's MagicFireBall, by key, so that a float in the atom data
/// can name it
struct FireballBallsState
{
	uint32_t nextKey {1};
	std::unordered_map<uint32_t, entt::entity> balls;
};

FireballBallsState& FireballBalls()
{
	return openblack::Locator::particleSystem::value().Module<FireballBallsState>();
}

/// AttatchFireBallToAtom: each atom carries a MagicFireBall (made on its first step with the fireball info of row
/// power-up level == 0 ? 1 : 0) that follows it and deflects it once it cools below the info's deletion
/// temperature; and the fly-by sound when it passes the camera fast
class AttatchFireBallToAtom final: public Modifier
{
public:
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		const uint32_t effectId = manager::IdOf(&effect);
		for (auto& atomPointer : collection.atoms)
		{
			auto& atom = *atomPointer;
			auto& data = atom.data[this]; // x: the key of its ball in the balls (0: none), y: made
			const auto global = effect.GlobalPosition(atom);
			if (data.y == 0.0f)
			{
				data.y = 1.0f;
				const int row = effect.PowerUpLevel() == 0 ? 1 : 0;
				bool hasPlayer = false;
				int player = 0;
				bool script = false;
				if (const auto* sink = effect.GetSink(); sink != nullptr)
				{
					hasPlayer = sink->Player(player);
					script = sink->IsScriptCasting();
				}
				const uint32_t key = FireballBalls().nextKey++ & 0xFFFFFF;
				const auto ball = magic::fireball::Create(
				    global, row, effectId, key, hasPlayer ? std::optional(static_cast<PlayerNames>(player)) : std::nullopt,
				    script, entt::null);
				FireballBalls().balls[key] = ball;
				data.x = static_cast<float>(key);
			}
			if (data.x == 0.0f)
			{
				continue;
			}
			const auto key = static_cast<uint32_t>(data.x);
			auto& balls = FireballBalls().balls;
			const auto found = balls.find(key);
			const auto ball = found != balls.end() ? found->second : entt::null;
			if (ball == entt::null || !Locator::entitiesRegistry::value().Valid(ball))
			{
				data.x = 0.0f; // = 0 once it is no longer available
				balls.erase(key);
				continue;
			}
			// the ball at the atom's local (= global, the root group) position, its height above the land, scaled by
			// base scale x rule scale
			if (magic::fireball::FollowAtom(ball, global, atom.baseScale * atom.ruleScale, 0))
			{
				atom.flags |= k_Deflected;
			}
			FlyBySound(atom);
		}
		return true;
	}

	/// The whoosh: the drawn position within 40 m of the camera now (squared < 1600) and not the step before, and
	/// faster than 20 (squared > 400): one of the five G_FireballPast sounds (0x40 + tick count % 5), a 2D one
	static void FlyBySound(const Atom& atom)
	{
		if (!Locator::camera::has_value() || !atom.drawn)
		{
			return;
		}
		const glm::vec3 camera = Locator::camera::value().GetOrigin();
		const glm::vec3 now = atom.current.position - camera;
		const glm::vec3 before = atom.previous.position - camera;
		if (!(glm::dot(now, now) < 1600.0f) || !(glm::dot(before, before) > 1600.0f) ||
		    !(glm::dot(atom.velocity, atom.velocity) > 400.0f))
		{
			return;
		}
		audio::PlaySoundEffect(audio::Owner::None(), 0x40 + static_cast<int>(audio::TickCount() % 5), 2, 0, false, false,
		                       audio::SfxBank::InGame);
	}
};

/// EventAlways: every step a type-2 event at the atom (its global position, the last global
/// movement, strength 1, no shield check)
class EventAlways final: public Modifier
{
public:
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		const SpellEventInfo event {.type = SpellEventInfo::Type::Point,
		                            .position = effect.GlobalPosition(atom),
		                            .velocity =
		                                atom.velocity * effect.GetDt(), // the last global movement (inferred: the step's move)
		                            .strength = 1.0f,
		                            .checkShields = false};
		effect.SendSpellEvent(event);
		return true;
	}
};

/// SetAtomHasBeenDeflected: flag bit 3
class SetAtomHasBeenDeflected final: public Modifier
{
public:
	bool ModifyAtom(Effect& /*effect*/, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		atom.flags |= k_Deflected;
		return true;
	}
};

/// UR_SideSpin: the first time, with a player behind the effect, w = curl x ScaleAngularVelocity, clamped to
/// +-MaxAngularVelocity, then w (w / max)^2; every step the velocity turns about y by w (1 - age / TimeToFade) dt.
/// (inferred) the defaults (the fireball file sets Scale 0.5, Max 1.2, TimeToFade 1.5)
class SideSpin final: public Modifier
{
public:
	explicit SideSpin(const Object& object)
	    : scale(object.Float("ScaleAngularVelocity", 0.0f))
	    , maximum(object.Float("MaxAngularVelocity", 1.0f))
	    , timeToFade(object.Float("TimeToFade", 1.0f))
	{
	}
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		auto& data = atom.data[this]; // x: w, y: done
		if (data.y == 0.0f)
		{
			if (effect.GetPlayer() < 0)
			{
				return true;
			}
			float w = effect.GetProcessInfo().curl * scale;
			w = std::clamp(w, -maximum, maximum);
			const float ratio = w / maximum;
			data = glm::vec4(ratio * ratio * w, 1.0f, 0.0f, 0.0f);
		}
		const float fade = std::clamp(effect.AtomAge(atom) / timeToFade, 0.0f, 1.0f);
		const float angle = (data.x - fade * data.x) * effect.GetDt();
		const float c = std::cos(angle);
		const float s = std::sin(angle);
		const glm::vec3 v = atom.velocity;
		atom.velocity = glm::vec3(c * v.x - s * v.z, v.y, c * v.z + s * v.x);
		return true;
	}
	float scale, maximum, timeToFade;
};

/// AR_FadeAlphaWithHeightAboveLandscape: alpha from AlphaAtZero on the land to AlphaAtRefHeight at RefHeight above it
/// (and beyond). (inferred) the defaults
class FadeAlphaWithHeight final: public Modifier
{
public:
	explicit FadeAlphaWithHeight(const Object& object)
	    : alphaAtZero(static_cast<float>(object.Int("AlphaAtZero", 255)))
	    , alphaAtRef(static_cast<float>(object.Int("AlphaAtRefHeight", 255)))
	    , refHeight(object.Float("RefHeight", 1.0f))
	{
	}
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		const auto p = effect.GlobalPosition(atom);
		const float height = p.y - LandAt(p.x, p.z);
		float alpha = alphaAtRef;
		if (height < 0.0f)
		{
			alpha = alphaAtZero;
		}
		else if (!(height > refHeight))
		{
			alpha = (alphaAtRef - alphaAtZero) * height / refHeight + alphaAtZero;
		}
		atom.colour[3] = static_cast<uint8_t>(std::clamp(alpha, 0.0f, 255.0f));
		return true;
	}
	float alphaAtZero, alphaAtRef, refHeight;
};

/// AddSubCollectionsToAtom: once per atom, the NextGroups under it
class AddSubCollectionsToAtom final: public Modifier
{
public:
	explicit AddSubCollectionsToAtom(const Object& object)
	    : nextGroups(object.Array("NextGroups"))
	{
	}
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		auto& data = atom.data[this];
		if (data.x == 0.0f)
		{
			data.x = 1.0f;
			effect.AddSubCollections(atom, nextGroups);
		}
		return true;
	}
	std::vector<int> nextGroups;
};

/// UR_Trail (an atom create rule): NumAtoms atoms behind the parent atom along the history of its last
/// One position of a trail
struct TrailSample
{
	glm::vec3 position;
	float scale;
	glm::mat3 rotation;
};

/// A trail's ring of samples
struct TrailHistory
{
	std::vector<TrailSample> samples;
	int count {0};
	int head {-1};
};

/// The trails' state (Locator::particleSystem): the collection data (the ring); the collections of an effect die with it,
/// so stale keys are only reused addresses: the first step (slot.first) re-initialises them
struct TrailState
{
	std::unordered_map<const Collection*, TrailHistory> histories;
};

TrailState& Trails()
{
	return openblack::Locator::particleSystem::value().Module<TrailState>();
}

/// NumGameTurnsToSpreadOver positions (a ring in the collection data),
/// covering at most MaxTrailLength (x the parent's scale), spaced 0.5 k^2 (UseNonLinearSpacing) or evenly, the newest
/// last. The first and last atoms take the HeadGroup / TrailGroup init (-1: no creator, not drawn).
/// (inferred) the property defaults below
class Trail final: public Modifier
{
public:
	explicit Trail(const Object& object)
	    : creator(object.String("PCreator"))
	    , nextGroups(object.Array("NextGroups"))
	    , maxTrailLength(object.Float("MaxTrailLength", 10.0f))
	    , numAtoms(object.Int("NumAtoms", 5))
	    , turns(object.Int("NumGameTurnsToSpreadOver", 10))
	    , headGroup(object.Int("HeadGroup", -1))
	    , trailGroup(object.Int("TrailGroup", -1))
	    , nonLinear(object.Bool("UseNonLinearSpacing", true))
	    , useAlpha(object.Bool("UseAlpha", true))
	    , useScaling(object.Bool("UseScaling", true))
	    , combinedParentScale(object.Bool("UseCombinedParentScale", false))
	    , useParentAlpha(object.Bool("UseParentAlpha", false))
	    , modifyAlpha(object.Bool("ModifyAlpha", true))
	    , modifyScaling(object.Bool("ModifyScaling", true))
	    , initUsingVelocity(object.Bool("InitTrailUsingVelocity", false))
	    , scaleLengthWithParent(object.Bool("ScaleMaxTrailLengthWithParent", false))
	    , fadeTailAlpha(object.Float("FadeTailAlpha", 1.0f))
	    , fadeTailScale(object.Float("FadeTailScale", 1.0f))
	{
	}
	[[nodiscard]] bool Creates() const override { return true; }
	using Sample = TrailSample;
	using History = TrailHistory;
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& slot) const override
	{
		auto& history = Trails().histories[&collection];
		if (slot.first)
		{
			slot.first = false;
			history.samples.assign(static_cast<size_t>(std::max(turns, 1)), Sample {glm::vec3(0.0f), 1.0f, glm::mat3(1.0f)});
			history.count = 0;
			history.head = -1;
			const auto* pcreator = effect.FindCreator(creator);
			for (int i = 0; i < numAtoms; ++i)
			{
				const bool special = i == 0 ? headGroup < 0 : (i == numAtoms - 1 ? trailGroup < 0 : false);
				effect.NewAtom(collection, special ? nullptr : pcreator, special ? std::vector<int> {} : nextGroups);
			}
			return true;
		}
		const Atom* parent = collection.parent;
		if (parent == nullptr)
		{
			return true;
		}
		const float parentScale = parent->ruleScale * (combinedParentScale ? parent->baseScale : 1.0f);
		const Sample sample {parent->position, parentScale, parent->rotation};
		const int capacity = static_cast<int>(history.samples.size());
		if (initUsingVelocity && history.count == 0)
		{
			// the whole ring back along the velocity
			const glm::vec3 step = parent->velocity * effect.GetDt();
			for (int i = 0; i < capacity; ++i)
			{
				Push(history, Sample {sample.position - step * static_cast<float>(i), sample.scale, sample.rotation});
			}
		}
		else
		{
			Push(history, sample);
		}
		// how much of the history MaxTrailLength covers
		const float maximum = scaleLengthWithParent ? parentScale * maxTrailLength : maxTrailLength;
		float coverage = 1.0f;
		float covered = 0.0f;
		for (int i = 0; i + 1 < history.count; ++i)
		{
			const float segment = glm::length(At(history, i).position - At(history, i + 1).position);
			if (!(segment + covered <= maximum))
			{
				coverage = ((maximum - covered) / segment + static_cast<float>(i)) / static_cast<float>(history.count);
				break;
			}
			covered += segment;
		}
		const int n = static_cast<int>(collection.atoms.size());
		const float half = 0.5f * static_cast<float>(n - 1) * static_cast<float>(n - 1);
		for (int k = 0; k < n; ++k)
		{
			auto& atom = *collection.atoms[static_cast<size_t>(k)];
			float t = 0.0f;
			if (nonLinear)
			{
				t = half > 0.0f
				        ? (half - 0.5f * static_cast<float>(k * k)) * coverage / half * static_cast<float>(history.count - 1)
				        : 0.0f;
			}
			else
			{
				t = static_cast<float>(n - k) * coverage / static_cast<float>(n) * static_cast<float>(history.count - 1);
			}
			const int index = static_cast<int>(std::floor(t));
			const float frac = t - static_cast<float>(index);
			Sample at {glm::vec3(0.0f), 0.0f, glm::mat3(1.0f)};
			if (index >= 0 && index < history.count && frac >= 0.0f && frac <= 1.0f)
			{
				if (index == history.count - 1)
				{
					at = At(history, index);
				}
				else
				{
					// the samples lerped
					const auto& a = At(history, index);
					const auto& b = At(history, index + 1);
					at.position = a.position + (b.position - a.position) * frac;
					at.scale = a.scale + (b.scale - a.scale) * frac;
					at.rotation = a.rotation;
				}
			}
			atom.position = at.position;
			atom.ruleScale = at.scale;
			atom.rotation = at.rotation;
			if (useParentAlpha)
			{
				atom.colour[3] = static_cast<uint8_t>(
				    (static_cast<int>(parent->colour[3]) *
				     static_cast<int>(parent->collection != nullptr ? parent->collection->alpha : 255.0f)) >>
				    8);
			}
			else if (modifyAlpha)
			{
				atom.colour[3] = 0xFF;
			}
		}
		// the fade along the trail: f = k / (n - 1), the tail (k = 0) gone, the head at FadeTail*
		for (int k = 0; k < n; ++k)
		{
			auto& atom = *collection.atoms[static_cast<size_t>(k)];
			const float f = n > 1 ? static_cast<float>(k) / static_cast<float>(n - 1) : 1.0f;
			if (useScaling)
			{
				atom.ruleScale = f * fadeTailScale;
			}
			else if (modifyScaling)
			{
				atom.ruleScale = atom.ruleScale * fadeTailScale * f;
			}
			float alpha = -1.0f;
			if (useAlpha)
			{
				alpha = (k == n - 1 ? 255.0f : fadeTailAlpha * 255.0f) * f;
			}
			else if (modifyAlpha)
			{
				alpha =
				    (k == n - 1 ? 255.0f : fadeTailAlpha * 255.0f) * static_cast<float>(atom.colour[3]) * f * (1.0f / 255.0f);
			}
			if (alpha >= 0.0f)
			{
				atom.colour[3] = static_cast<uint8_t>(std::clamp(alpha, 0.0f, 255.0f));
			}
		}
		return true;
	}

	/// The i-th newest sample
	static const Sample& At(const History& history, int i)
	{
		const int capacity = static_cast<int>(history.samples.size());
		return history.samples[static_cast<size_t>(((history.head - i) % capacity + capacity) % capacity)];
	}
	static void Push(History& history, const Sample& sample)
	{
		const int capacity = static_cast<int>(history.samples.size());
		history.head = (history.head + 1) % capacity;
		if (history.count < capacity)
		{
			++history.count;
		}
		history.samples[static_cast<size_t>(history.head)] = sample;
	}

	std::string creator;
	std::vector<int> nextGroups;
	float maxTrailLength;
	int numAtoms, turns, headGroup, trailGroup;
	bool nonLinear, useAlpha, useScaling, combinedParentScale, useParentAlpha, modifyAlpha, modifyScaling, initUsingVelocity,
	    scaleLengthWithParent;
	float fadeTailAlpha, fadeTailScale;
};
} // namespace

void openblack::psys::RegisterFireballRules()
{
	RegisterModifier("UpdateRuleGravityWithFloor", MakeModifierOf<GravityWithFloor>);
	RegisterModifier("CreateWithInitialDirection", MakeModifierOf<CreateWithInitialDirection>);
	RegisterModifier("AttatchFireBallToAtom", MakeModifierOf<AttatchFireBallToAtom>);
	RegisterModifier("EventAlways", MakeModifierOf<EventAlways>);
	RegisterModifier("SetAtomHasBeenDeflected", MakeModifierOf<SetAtomHasBeenDeflected>);
	RegisterModifier("UR_SideSpin", MakeModifierOf<SideSpin>);
	RegisterModifier("AR_FadeAlphaWithHeightAboveLandscape", MakeModifierOf<FadeAlphaWithHeight>);
	RegisterModifier("AddSubCollectionsToAtom", MakeModifierOf<AddSubCollectionsToAtom>);
	RegisterModifier("UR_Trail", MakeModifierOf<Trail>);
	RegisterCondition("EventConditionAtomHasBeenDeflected", &HasBeenDeflected);
	RegisterCondition("EC_DeflectionInAtomsHierarchy", &DeflectionInHierarchy);
	RegisterCondition("EC_DeflectionInCollectionsHierarchy", &DeflectionInHierarchy);
	RegisterCondition("EventConditionAtomCloseWater", &CloseWater);
	RegisterCondition("EventConditionFireBallSteam", &FireBallSteam);
}
