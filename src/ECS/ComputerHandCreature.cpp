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

void openblack::computer_hand::steps::AddCreatureSteps(StepTable& /*table*/)
{
	// MoveHandToCreatureHead, GiveObjectInHandToCreature, SetLeash, AttachLeashToObject, SlapCreature,
	// StrokeCreature, ForceCreatureToPerformAction, ForceCreatureDominantDesire and SetCreatureSpeedForJourney are not made
	// yet
}
