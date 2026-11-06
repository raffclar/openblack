/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#ifndef LOCATOR_IMPLEMENTATIONS
#error "Locator interface implementations should only be included in Locator.cpp"
#endif

#include <vector>

#include "ECS/Systems/LandDetailSystemInterface.h"

namespace openblack::ecs::systems
{

class LandDetailSystem final: public LandDetailSystemInterface
{
public:
	[[nodiscard]] const graphics::land_detail::FrameDetail& For(const Camera& camera) override;
	[[nodiscard]] uint64_t Classifications() const override { return _memo.Classifications(); }

private:
	graphics::land_detail::FrameMemo _memo;
	/// The places of the land's blocks, kept from call to call so that no call allocates them again
	std::vector<graphics::land_detail::BlockPlace> _places;
};

} // namespace openblack::ecs::systems
