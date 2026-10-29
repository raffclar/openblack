/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WonderRules.h"

#include "3D/MapCoords.h"

using namespace openblack::ecs;

float wonder::Reach(float wonderDesire)
{
	const float reach = wonderDesire * k_ReachPerDesire;
	return reach > k_ReachPerDesire ? reach : k_ReachPerDesire;
}

uint32_t wonder::SearchSteps(float reach)
{
	// The cells across, as an unsigned count: a count under 1 becomes 1
	auto across = static_cast<uint32_t>(map_coords::FtoL(reach * k_CellsPerReach));
	if (across < 1)
	{
		across = 1;
	}
	return across * across;
}

float wonder::Power(float wonderDesire, std::span<const Neighbour> inCell)
{
	const float reach = Reach(wonderDesire);
	const auto steps = SearchSteps(reach);
	float power = wonderDesire;
	for (uint32_t step = 0; step < steps; ++step)
	{
		for (const auto& thing : inCell)
		{
			if (thing.distance < reach)
			{
				power = thing.artefactWorth + power;
				power = thing.impressiveness * k_ImpressivenessShare + power;
			}
		}
	}
	return k_LeastPower > power ? k_LeastPower : power;
}

float wonder::Scale(float power)
{
	const float capped = power < k_MostPowerForScale ? power : k_MostPowerForScale;
	return capped * (1.0f - capped * k_ScaleLossPerPower);
}

float wonder::WithWonder(float mostTribalPower, float wonderPower)
{
	return wonderPower + mostTribalPower;
}

float wonder::WithoutWonder(float mostTribalPower, float wonderPower)
{
	return mostTribalPower - wonderPower;
}

float wonder::SettledTribalPower(float mostTribalPower)
{
	return mostTribalPower > k_LeastTribalPower ? mostTribalPower : k_LeastTribalPower;
}
