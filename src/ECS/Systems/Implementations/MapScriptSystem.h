/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/MapScriptSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// Owns the map script's globals, at their start values when made
class MapScriptSystem final: public MapScriptSystemInterface
{
public:
	[[nodiscard]] MapScriptGlobals& Globals() noexcept override { return _globals; }
	[[nodiscard]] const MapScriptGlobals& Globals() const noexcept override { return _globals; }

private:
	MapScriptGlobals _globals;
};
} // namespace openblack::ecs::systems
