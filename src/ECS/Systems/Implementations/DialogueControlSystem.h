/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <utility>

#include "ECS/Systems/DialogueControlSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class DialogueControlSystem final: public DialogueControlSystemInterface
{
public:
	void SetHooks(Hooks hooks) override { _hooks = std::move(hooks); }

	[[nodiscard]] uint32_t GetOwner() const override { return _owner; }
	[[nodiscard]] bool IsControlled(uint32_t wideScreenOwner) const override;
	void Request(uint32_t task, uint32_t wideScreenOwner) override;
	bool Release(uint32_t task, bool helpScript) override;
	void SendSpiritsHome(bool helpScript) override;

	void Reset() override { _owner = 0; }

private:
	uint32_t _owner {0};
	Hooks _hooks;
};

} // namespace openblack::ecs::systems
