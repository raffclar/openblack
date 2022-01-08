/*****************************************************************************
 * Copyright (c) 2018-2021 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include <chrono>

#include <entt/entt.hpp>

#include "ECS//Registry.h"

namespace openblack::ecs::systems
{

class EntitySystemInterface
{
public:
	explicit EntitySystemInterface(bool debug)
	    : _debug(debug)
	{
	}
	virtual void Connect(Registry& registry) = 0;
	virtual bool Update(std::chrono::duration<float, std::milli> dt) = 0;

protected:
	bool _debug;
};
} // namespace openblack::ecs::systems