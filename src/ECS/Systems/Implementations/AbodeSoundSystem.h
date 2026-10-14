/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/AbodeSoundSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class AbodeSoundSystem final: public AbodeSoundSystemInterface
{
public:
	void Update(std::chrono::duration<float, std::milli> gameTime) override;
	[[nodiscard]] uint32_t GetSoundCount() const override { return _sounds; }

private:
	uint32_t _sounds {0};
};

} // namespace openblack::ecs::systems
