/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "RenderFrameSystem.h"

#include <utility>

using namespace openblack;
using namespace openblack::ecs::systems;

void RenderFrameSystem::SetLandCells(land_light::Cells cells)
{
	_cells = std::move(cells);
}

void RenderFrameSystem::RestoreLandCells()
{
	_cells.cells = _cells.base;
}

void RenderFrameSystem::SetLandLuminosity(const std::vector<uint8_t>& luminosity)
{
	for (size_t i = 0; i < _cells.cells.size() && i < luminosity.size(); ++i)
	{
		_cells.cells[i] = (_cells.cells[i] & 0x00FFFFFFu) | static_cast<uint32_t>(luminosity[i]) << 24u;
	}
}

void RenderFrameSystem::AddLandStamp(const land_light::Stamp& stamp)
{
	_stamps.push_back(stamp);
}

void RenderFrameSystem::ApplyLandStamps()
{
	for (const auto& stamp : _stamps)
	{
		land_light::ApplyStamp(stamp, _cells.firstCell, _cells.size, _cells.covered, _cells.cells);
	}
}

void RenderFrameSystem::ClearLandStamps() noexcept
{
	_stamps.clear();
	++_stampFrame;
}

void RenderFrameSystem::SubmitMist(const mists::MistDesc& mist)
{
	_submittedMists.push_back(mist);
}

std::vector<mists::MistDesc> RenderFrameSystem::TakeSubmittedMists()
{
	auto submitted = std::move(_submittedMists);
	_submittedMists.clear();
	return submitted;
}
