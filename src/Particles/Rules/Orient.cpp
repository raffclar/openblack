/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// UR_OrientSpriteWithVelocity (the flames of SF_FireBallInHand): the sprite's roll follows its smoothed velocity as the
// camera sees it. Wiki: docs/bw1-notes/miracles.md, "Lightning explosion and missing PSys classes".

#include <cmath>

#include <memory>

#include "3D/Billboard.h"
#include "3D/ObjectMatrix.h"
#include "Camera/Camera.h"
#include "Locator.h"
#include "Particles/PSys.h"
#include "Particles/PSysFile.h"
#include "Particles/PSysRegistry.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
/// The per-atom data of UR_OrientSpriteWithVelocity: first, the smoothed velocity, its rate
struct OrientData
{
	bool first {true};
	glm::vec3 velocity {0.0f};
	float rate {0.0f};
};

/// UR_OrientSpriteWithVelocity, per atom (properties SmoothFactor, ProportionDefault)
class OrientSpriteWithVelocity final: public Modifier
{
public:
	explicit OrientSpriteWithVelocity(const Object& object)
	    : smoothFactor(object.Float("SmoothFactor", 0.0f))
	    , proportionDefault(object.Float("ProportionDefault", 0.0f))
	{
	}
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		auto& data = AtomDataOf<OrientData>(atom, this);
		if (data.first)
		{
			// the first time: the velocity as it is, rate = -10 ln(1 - SmoothFactor)
			data.first = false;
			data.velocity = atom.velocity;
			data.rate = std::log(1.0f - smoothFactor) * -10.0f;
		}
		// v += (velocity - v) (1 - e^(-dt rate)), dt the step
		data.velocity += (atom.velocity - data.velocity) * (1.0f - std::exp(-effect.GetDt() * data.rate));
		// u = -v + (0, ProportionDefault, 0), in the camera's frame (the world-to-camera rotation: x = u . right,
		// y = u . up). (inferred) the vector u: the original's code for it was not read
		const glm::vec3 u(-data.velocity.x, -data.velocity.y + proportionDefault, -data.velocity.z);
		if (!Locator::camera::has_value())
		{
			return true;
		}
		const auto& camera = Locator::camera::value();
		// the Y angle atan2(-y, x) + pi / 2, billboard::ScreenVelocity
		atom.rotation = affine::AngleY(graphics::billboard::ScreenVelocity(u, camera.GetRight(), camera.GetUp()));
		return true;
	}
	float smoothFactor, proportionDefault;
};
} // namespace

void openblack::psys::RegisterOrientRules()
{
	RegisterModifier("UR_OrientSpriteWithVelocity", MakeModifierOf<OrientSpriteWithVelocity>);
}
