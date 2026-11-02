/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The forest miracle's effect: the path its flocks circle on over the new trees, the butterflies or bats they are by the
// caster's alignment, and the caster's camera following the forest's own camera path

#include <cmath>

#include <array>
#include <memory>
#include <numbers>
#include <string>

#include <ParticleFile.h>
#include <glm/gtx/transform.hpp>

#include "Camera/CameraPathControl.h"
#include "CameraPathLink.h"
#include "Common/X87.h"
#include "KeyPointSpline.h"
#include "Magic/ForestRules.h"
#include "ParticleClassRegistry.h"
#include "ParticleFlocking.h"

using namespace openblack::particles;
using openblack::psys::ParticleObject;

namespace
{
constexpr float k_TwoPi = 2.0f * std::numbers::pi_v<float>;
/// The path's radius and height when the file gives no curve
constexpr std::array<float, 4> k_FlatAtOne {0.0f, 1.0f, 1.0f, 1.0f};
constexpr std::array<float, 4> k_FlatAtZero {0.0f, 0.0f, 1.0f, 0.0f};
/// The camera waits this long before following its path when the file doesn't say
constexpr float k_DefaultPauseBeforePlay = 4.0f;

std::span<const float> CurveOf(const ParticleObject& object, std::string_view key, std::span<const float> fallback)
{
	return object.Has(key) ? object.FloatArray(key) : fallback;
}

/// Each atom circles the parent on a squashed sphere whose size and height follow curves over the collection's age, its
/// two angles turning steadily from where each atom started
class ForestPath final: public Modifier
{
public:
	explicit ForestPath(const ParticleObject& object)
	    : thetaSpeed(object.Float("ThetaSpeed", 0.0f))
	    , phiSpeed(object.Float("PhiSpeed", 0.0f))
	    , sphereRadius(object.Float("SphereRadius", 1.0f))
	    , scaleSphereRadius(object.String("ScaleSphereRadius"))
	    , scale(object.Float("ScaleX", 1.0f), object.Float("ScaleY", 1.0f), object.Float("ScaleZ", 1.0f))
	    , radiusCurve(CurveOf(object, "RadiusSpline", k_FlatAtOne))
	    , heightCurve(CurveOf(object, "HeightSpline", k_FlatAtZero))
	{
	}

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		const float age = effect.CollectionAge(collection);
		const float radiusShare = radiusCurve.Evaluate(age, 1.0f);
		const float height = heightCurve.Evaluate(age, 0.0f);
		float radius = sphereRadius * radiusShare;
		if (!scaleSphereRadius.empty())
		{
			radius *= effect.FloatProvider(scaleSphereRadius, 1.0f);
		}
		const auto parent = effect.GlobalToLocal(
		    collection, collection.parent != nullptr ? effect.GlobalPosition(*collection.parent) : effect.GetOrigin());
		const float dt = effect.GetDt();
		for (auto& atom : collection.atoms)
		{
			auto& data = atom->data[this];
			if (!data.started)
			{
				// Three starting angles are drawn, as the game does, though only two are used. The draws occur in
				// this order, one statement each, because the arguments of a call have no fixed order of evaluation.
				data.started = true;
				data.a.x = effect.Random(k_TwoPi);
				data.a.y = effect.Random(k_TwoPi);
				data.a.z = effect.Random(k_TwoPi);
				data.a.w = 0.0f;
			}
			auto point = forest_path::PathPoint(radius, effect.AtomAge(*atom), thetaSpeed, phiSpeed, data.a.x, data.a.y, scale);
			point += parent;
			point.y += height;
			atom->velocity = dt > 0.0f ? (point - atom->position) / dt : glm::vec3(0.0f);
			atom->position = point;
		}
		return true;
	}

	float thetaSpeed;
	float phiSpeed;
	float sphereRadius;
	std::string scaleSphereRadius;
	glm::vec3 scale;
	KeyPointSpline radiusCurve;
	KeyPointSpline heightCurve;
};

/// Hands each new atom to one of two creators: the evil one when the effect's player is more evil than its switch, the
/// good one otherwise, and without a player
class GoodEvilCreator final: public Creator
{
public:
	explicit GoodEvilCreator(const ParticleObject& object)
	    : evil(object.String("PCreatorEvil"))
	    , good(object.String("PCreatorGood"))
	    , alignmentSwitch(object.Float("AlignmentSwitch", openblack::magic::forest::k_EvilAlignment))
	{
		kind = Kind::Point;
	}

	void InitAtom(Effect& effect, Atom& atom) const override
	{
		const auto player = effect.GetPlayer();
		const bool isEvil = player >= 0 && effect.Services().world.PlayerAlignment(player) < alignmentSwitch && !evil.empty();
		const auto* chosen = effect.FindCreator(isEvil ? evil : good);
		if (chosen == nullptr || chosen == this)
		{
			return;
		}
		// The atom is the chosen creator's, made as it makes its own
		atom.creator = chosen;
		atom.rgba = chosen->rgba;
		atom.baseScale = chosen->initialScale;
		atom.stretch = chosen->stretch;
		chosen->InitAtom(effect, atom);
	}

	std::string evil;
	std::string good;
	float alignmentSwitch;
};

/// An atom that draws nothing but, for the player casting, takes the camera along a camera path placed where it is: the
/// camera glides onto the path's start while the atom waits, then follows the path at the time of the atom's animation
/// frame
class AnimWithCameraCreator final: public Creator
{
public:
	explicit AnimWithCameraCreator(const ParticleObject& object)
	    : cameraFile(object.String("CameraFileName"))
	    , animFile(object.String("AnimFileName"))
	    , pauseBeforePlay(object.Float("PauseBeforePlay", k_DefaultPauseBeforePlay))
	    , speedUp(object.Float("SpeedUpFactor", 1.0f))
	{
		ReadCreatorProperties(object, *this);
		kind = Kind::Point;
		numFrames = openblack::camera_path::k_AnimationFrames;
	}

	void InitAtom(Effect& effect, Atom& atom) const override
	{
		// One play of the animation is a thousand frames over its cycle time, shortened by the speed-up. The frames start
		// to move on once the pause has gone by. Every computer moves them on alike.
		const auto cycle = effect.Services().world.AnimationCycleMilliseconds(animFile);
		if (!cycle.has_value() || *cycle <= 0)
		{
			return;
		}
		atom.frame = 0.0f;
		atom.frameRate = openblack::camera_path::FrameRate(*cycle, speedUp);
		atom.playAnim = false;
		// Only the camera of the player who casts follows the path
		if (effect.IsMyInterfaceCasting() && !cameraFile.empty() && cameraFile != "NULL_STRING")
		{
			atom.cameraPath = std::make_shared<CameraPathLink>(
			    CameraPathLink {.cycleMilliseconds = *cycle, .frames = numFrames, .loop = loopAnim});
		}
	}

	void AfterStep(Effect& effect, Atom& atom) const override
	{
		const bool pauseOver = effect.AtomAge(atom) > pauseBeforePlay;
		if (auto& link = atom.cameraPath; link != nullptr)
		{
			link->previousFrame = atom.previous.frame;
			link->currentFrame = atom.current.frame;
			link->shown = atom.visible && (atom.steps >= 2 || atom.drawOnFirstUpdate);
			if (!link->started)
			{
				// The first step takes the camera onto the path, placed where the atom is. No rule moves, turns or scales
				// the atom, so this place stays right.
				link->started = true;
				const auto position = effect.GlobalPosition(atom);
				const glm::mat4 placement = glm::translate(position) * glm::mat4(atom.rotation) *
				                            glm::scale(glm::vec3(atom.baseScale * atom.ruleScale));
				effect.Services().world.FollowCameraPath(cameraFile, placement, pauseBeforePlay, link);
			}
			else if (pauseOver)
			{
				link->playing = true;
			}
		}
		// The frame moves on from the next step
		if (pauseOver)
		{
			atom.playAnim = true;
		}
	}

	std::string cameraFile;
	std::string animFile;
	float pauseBeforePlay;
	float speedUp;
};
} // namespace

glm::vec3 forest_path::PathPoint(float radius, float atomAge, float thetaSpeed, float phiSpeed, float theta0, float phi0,
                                 glm::vec3 scale)
{
	// The remainders are exact. The cosine of phi stays wide for the two multiplies that use it
	const float theta = std::fmod(atomAge * thetaSpeed + theta0, k_TwoPi);
	const float phi = std::fmod(atomAge * phiSpeed + phi0, k_TwoPi);
	const double cosPhi = x87::Cos(phi);
	const float x = x87::Mul(cosPhi, x87::Mul(x87::Cos(theta), scale.x));
	const float y = x87::Mul(x87::Sin(phi), scale.y);
	const float z = x87::Mul(cosPhi, x87::Mul(x87::Sin(theta), scale.z));
	return {x * radius, y * radius, z * radius};
}

void openblack::particles::RegisterForestRules(ParticleClassRegistry& registry)
{
	registry.AddModifier("UR_ForestPath", ParticleClassRegistry::Make<ForestPath>);
	registry.AddCreator("ParticleGoodEvilCreator",
	                    [](const ParticleObject& object) { return std::make_unique<GoodEvilCreator>(object); });
	registry.AddCreator("ParticleAnimWithCameraCreator",
	                    [](const ParticleObject& object) { return std::make_unique<AnimWithCameraCreator>(object); });
}
