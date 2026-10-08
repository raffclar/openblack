/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::night_lights
{
struct State;
} // namespace openblack::night_lights

namespace openblack::ecs::systems
{
/// The night lights' images and textures, the village lights, their clocks and the cell luminosities
/// (Locator::villageLightSystem)
class VillageLightSystemInterface
{
public:
	virtual ~VillageLightSystemInterface() = default;

	[[nodiscard]] virtual openblack::night_lights::State& GetState() noexcept = 0;
};
} // namespace openblack::ecs::systems
