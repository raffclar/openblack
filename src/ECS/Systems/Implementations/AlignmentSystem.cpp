/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "AlignmentSystem.h"

#include "ECS/Effects/Alignment.h"

using namespace openblack;
using namespace openblack::ecs::systems;

float AlignmentSystem::GetPlayerAlignment(PlayerNames player) const
{
	return ecs::effects::alignment::Get(player);
}

void AlignmentSystem::SetPlayerAlignment(PlayerNames player, float alignment)
{
	ecs::effects::alignment::SetClamped(player, alignment);
}

void AlignmentSystem::AddPlayerAlignment(PlayerNames player, float change)
{
	ecs::effects::alignment::AddClamped(player, change);
}

void AlignmentSystem::AddPendingAlignment(PlayerNames player, float change)
{
	// The player's turn folds it in (ecs::effects::alignment::ProcessForPlayer), so it is not applied here
	ecs::effects::alignment::Of(player).pending += change;
}

float AlignmentSystem::GetPendingAlignment(PlayerNames player) const
{
	return ecs::effects::alignment::Of(player).pending;
}

void AlignmentSystem::UpdateTurn()
{
	ecs::effects::alignment::UpdateInterfaceAlignment();
}

void AlignmentSystem::Update(std::chrono::duration<float, std::milli> gameTime)
{
	// The same target and step as the renderer's sky
	_sky.Update(Clouds::InfluentialPlayerAlignment(), gameTime.count());
}

float AlignmentSystem::GetCameraAlignment() const
{
	// The interface alignment is kept from 0, evil, to 1, good
	return (ecs::effects::alignment::GetInterfaceAlignment() * 2.0f) - 1.0f;
}
