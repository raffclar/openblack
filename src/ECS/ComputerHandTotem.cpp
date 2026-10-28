/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ECS/ComputerHand.h"
#include "ECS/ComputerHandSteps.h"

using namespace openblack;
using namespace openblack::computer_hand;
using namespace openblack::computer_hand::steps;

namespace
{

/// The god raises the totem to the share at the start of the step. The other parts of the step do nothing.
Result RaiseStart(HandAction& /*action*/, const World& world, const Step& step)
{
	world.totem.RaiseTotem(ArgumentAt(step, 0).object, ArgumentAt(step, 1).value);
	return Result::Next;
}

} // namespace

void openblack::computer_hand::steps::AddTotemSteps(StepTable& table)
{
	constexpr auto k_Totem = StepOwner::Totem;
	AddStep(table, k_Totem, StepKind::RaiseTotem, {.made = true, .start = RaiseStart});
	// No action of the god lowers a totem, so that kind of step is not made
}
