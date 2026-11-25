/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Resources/ResourcesInterface.h"

namespace openblack::graphics
{

/// While it lives, the resource caches load nothing: the draw finds only the resources that are loaded, and does not
/// change the caches. The fill of the frame loads the resources first. A block that is not active does nothing, for
/// example in the warm-up, which draws while the resources load.
class DrawLoadBlock
{
public:
	DrawLoadBlock(resources::ResourcesInterface& resources, bool active)
	    : _resources(active ? &resources : nullptr)
	{
		if (_resources != nullptr)
		{
			_resources->BlockLoads(true);
		}
	}
	~DrawLoadBlock()
	{
		if (_resources != nullptr)
		{
			_resources->BlockLoads(false);
		}
	}
	DrawLoadBlock(const DrawLoadBlock&) = delete;
	DrawLoadBlock& operator=(const DrawLoadBlock&) = delete;
	DrawLoadBlock(DrawLoadBlock&&) = delete;
	DrawLoadBlock& operator=(DrawLoadBlock&&) = delete;

private:
	resources::ResourcesInterface* _resources;
};

} // namespace openblack::graphics
