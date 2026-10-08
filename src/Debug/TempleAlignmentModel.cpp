/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleAlignmentModel.h"

#include <algorithm>

#include <entt/entity/entity.hpp>
#include <fmt/format.h>

#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/TempleExteriorSystemInterface.h"

using namespace openblack;
using namespace openblack::debug;

namespace
{
/// Far more turns than the outside ever needs to cross from evil to good, so that the count always ends
constexpr uint32_t k_MaxTurns = 1000;
} // namespace

std::vector<PlayerNames> temple_alignment::PlayersInGame(const PlayerEntityOf& entityOf)
{
	std::vector<PlayerNames> found;
	for (uint8_t p = 0; p < static_cast<uint8_t>(PlayerNames::_COUNT); ++p)
	{
		const auto player = static_cast<PlayerNames>(p);
		if (entityOf(player) != entt::null)
		{
			found.push_back(player);
		}
	}
	return found;
}

void temple_alignment::SetAlignment(ecs::systems::AlignmentSystemInterface& alignment, PlayerNames player, float value)
{
	alignment.SetPlayerAlignment(player, std::clamp(value, -1.0f, 1.0f));
}

float temple_alignment::AlignmentTargetNow(const ecs::systems::AlignmentSystemInterface& alignment, PlayerNames player)
{
	return TempleExteriorMorph::AlignmentTarget(alignment.GetPlayerAlignment(player));
}

void temple_alignment::SnapOutside(const ecs::systems::AlignmentSystemInterface& alignment,
                                   ecs::systems::TempleExteriorSystemInterface& exterior, entt::entity heart,
                                   PlayerNames player)
{
	exterior.SnapAlignment(heart, AlignmentTargetNow(alignment, player));
}

uint32_t temple_alignment::TurnsToReach(float current, float target)
{
	uint32_t turns = 0;
	while (current != target && turns < k_MaxTurns)
	{
		current = TempleExteriorMorph::Step(current, target);
		++turns;
	}
	return turns;
}

std::string temple_alignment::LookStatus(const TempleExteriorMorph::State& look, float targetNow)
{
	auto status = fmt::format("Outside alignment {:.3f}, heading to {:.3f}", look.alignment, look.alignmentTarget);
	if (targetNow != look.alignmentTarget)
	{
		status += fmt::format(" ({:.3f} from the next turn)", targetNow);
	}
	status += fmt::format("\nMesh blended for {:.3f}", look.blendedAlignment);
	const auto turns = TurnsToReach(look.alignment, targetNow);
	status += turns == 0 ? std::string(", there") : fmt::format(", {} turns to go", turns);
	return status;
}
