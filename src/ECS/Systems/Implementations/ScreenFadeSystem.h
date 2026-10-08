/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "3D/ScreenFade.h"
#include "ECS/Systems/ScreenFadeSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// Owns the script fade and the cinema bars, at their start values when made
class ScreenFadeSystem final: public ScreenFadeSystemInterface
{
public:
	[[nodiscard]] ScreenFade& Fade() noexcept override { return _fade; }
	[[nodiscard]] const ScreenFade& Fade() const noexcept override { return _fade; }

private:
	ScreenFade _fade;
};
} // namespace openblack::ecs::systems
