/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/PlayerProfileSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

/// Profiles are not kept on disk yet: there are none, so no creature has been kept either
class PlayerProfileSystem final: public PlayerProfileSystemInterface
{
public:
	[[nodiscard]] size_t GetProfileCount() const final { return 0; }
	[[nodiscard]] bool CurrentProfileHasCreature() const final { return false; }
};

} // namespace openblack::ecs::systems
