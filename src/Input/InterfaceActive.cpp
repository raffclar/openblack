/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "InterfaceActive.h"

#include <cstdio>
#include <cstdlib>

#include "ECS/Systems/InputStateInterface.h"
#include "Locator.h"

namespace openblack::interface_active
{
namespace
{
struct InterfaceActiveState
{
	/// The interface's flags ((inferred) 0 at construction)
	uint8_t flags {0};
};

/// This module's state (Locator::inputState)
InterfaceActiveState& Interface()
{
	if (!Locator::inputState::has_value())
	{
		std::fputs("interface_active: no input state in the locator (Locator::inputState)\n", stderr);
		std::abort();
	}
	return Locator::inputState::value().Get<InterfaceActiveState>();
}
} // namespace

void SetActive(bool active)
{
	// Bit 0 becomes (active == 0)
	auto& flags = Interface().flags;
	flags = static_cast<uint8_t>((flags & ~1u) | (active ? 0u : 1u));
}

bool IsActive()
{
	return (Interface().flags & 1u) == 0;
}

uint8_t GetFlags()
{
	return Interface().flags;
}

void SetFlags(uint8_t flags)
{
	Interface().flags = flags;
}

} // namespace openblack::interface_active
