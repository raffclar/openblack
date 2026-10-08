/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Components/MapScriptGlobals.h"

namespace openblack
{
using ecs::components::MapScriptGlobals;
} // namespace openblack

namespace openblack::ecs::systems
{
/// The map script's globals (VERSION, SET_LAND_NUMBER, the influence multipliers, the firefly rewards, the tutorial-skip
/// bits), made with the Game and kept until it goes (Locator::mapScriptSystem). The land load and the script reboot
/// reset their parts
class MapScriptSystemInterface
{
public:
	virtual ~MapScriptSystemInterface() = default;

	[[nodiscard]] virtual MapScriptGlobals& Globals() noexcept = 0;
	[[nodiscard]] virtual const MapScriptGlobals& Globals() const noexcept = 0;
};
} // namespace openblack::ecs::systems
