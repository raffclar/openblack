/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack
{
class ScreenFade;
}

namespace openblack::ecs::systems
{
/// The script fade and the cinema bars (3D/ScreenFade.h), made with the Game and kept until it goes
/// (Locator::screenFade). The game turn steps the fade, every frame slides the bars
class ScreenFadeSystemInterface
{
public:
	virtual ~ScreenFadeSystemInterface() = default;

	[[nodiscard]] virtual ScreenFade& Fade() noexcept = 0;
	[[nodiscard]] virtual const ScreenFade& Fade() const noexcept = 0;
};
} // namespace openblack::ecs::systems
