/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The rules that keep atoms on the casting hand: UR_FollowLocalHand (the in-hand effects SF_*InHand) and
// UR_FollowCastPosn (the gesture trail, the selection, SF_CreatureGesture). Wiki: docs/bw1-notes/magic.md, "La mano".

#include <algorithm>

#include <glm/mat3x3.hpp>

#include "Particles/PSys.h"
#include "Particles/PSysFile.h"
#include "Particles/PSysRegistry.h"

using namespace openblack::psys;

namespace
{
/// Into the parent atom's frame in a hierarchy, else the point stays global
glm::vec3 GlobalToLocal(const Effect& effect, const Atom& atom, const glm::vec3& global)
{
	const auto* collection = atom.collection;
	if (collection != nullptr && collection->hierarchy && collection->parent != nullptr)
	{
		const auto& parent = *collection->parent;
		return glm::transpose(parent.rotation) * (global - effect.GlobalPosition(parent));
	}
	return global;
}

/// UR_FollowLocalHand: the atom goes to the gesture position (ProcessInfo::handPos) and its velocity is the move
/// x 1 / the step. UseGraspPos is read, not used here.
class FollowLocalHand final: public Modifier
{
public:
	explicit FollowLocalHand(const Object& object)
	    : useGraspPos(object.Bool("UseGraspPos", false))
	{
	}
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		const auto local = GlobalToLocal(effect, atom, effect.GetProcessInfo().handPos);
		// the max(dt, eps) is a port guard: the original multiplies by 1/dt directly
		atom.velocity = (local - atom.position) * (1.0f / std::max(effect.GetDt(), 1e-4f));
		atom.position = local;
		return true;
	}
	bool useGraspPos;
};

/// UR_FollowCastPosn: the atom goes to the gesture position (no velocity). For this computer's interface the
/// collection's interpolation flag is cleared and the atom gets a draw offset that draws it at the hand between steps:
/// not ported (the atoms are drawn where the step left them).
class FollowCastPosn final: public Modifier
{
public:
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		atom.position = GlobalToLocal(effect, atom, effect.GetProcessInfo().handPos);
		return true;
	}
};
} // namespace

void openblack::psys::RegisterHandFollowRules()
{
	RegisterModifier("UR_FollowLocalHand", MakeModifierOf<FollowLocalHand>);
	RegisterModifier("UR_FollowCastPosn", MakeModifierOf<FollowCastPosn>);
}
