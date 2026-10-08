/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "FireGraphicSystem.h"

using namespace openblack::ecs::systems;

FireGraphicSystem::Graphics& FireGraphicSystem::All()
{
	return _graphics;
}

void FireGraphicSystem::Set(uint32_t fire, std::unique_ptr<fire::graphic::Graphic> graphic)
{
	_graphics[fire] = std::move(graphic);
}

void FireGraphicSystem::Erase(uint32_t fire)
{
	_graphics.erase(fire);
}

void FireGraphicSystem::Clear()
{
	_graphics.clear();
}

bool FireGraphicSystem::SourceAdded() const
{
	return _sourceAdded;
}

void FireGraphicSystem::SetSourceAdded()
{
	_sourceAdded = true;
}
