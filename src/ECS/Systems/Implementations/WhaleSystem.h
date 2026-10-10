/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/WhaleSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class WhaleSystem final: public WhaleSystemInterface
{
public:
	void ProcessTurn() override;
	void Update(std::chrono::duration<float, std::milli> gameTime, float turnFraction) override;
	[[nodiscard]] int32_t GetWakeTimer() const override { return _wakeTimer; }

private:
	int32_t _wakeTimer {0};
};

} // namespace openblack::ecs::systems
