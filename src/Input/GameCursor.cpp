/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameCursor.h"

#include <cstdio>
#include <cstdlib>

#include <utility>

#include "Debug/DebugEnv.h"
#include "ECS/Systems/InputStateInterface.h"
#include "Locator.h"

namespace openblack::input
{
namespace
{
struct GameCursorState
{
	glm::ivec2 position {0, 0};
};

/// The test hook's replacement for OPENBLACK_MOUSE_AT, if it set one
struct MouseAtState
{
	std::optional<std::string> override;
};
} // namespace

glm::ivec2 GameCursor()
{
	if (!Locator::inputState::has_value())
	{
		return glm::ivec2(0);
	}
	return Locator::inputState::value().Get<GameCursorState>().position;
}

glm::ivec2& GameCursorRef()
{
	if (!Locator::inputState::has_value())
	{
		std::fputs("input: no input state in the locator (Locator::inputState)\n", stderr);
		std::abort();
	}
	return Locator::inputState::value().Get<GameCursorState>().position;
}

HandButtons& HandButtonsRef()
{
	if (!Locator::inputState::has_value())
	{
		std::fputs("input: no input state in the locator (Locator::inputState)\n", stderr);
		std::abort();
	}
	return Locator::inputState::value().Get<HandButtons>();
}

const char* MouseAt()
{
	static const debug_env::Variable k_MouseAt("OPENBLACK_MOUSE_AT");
	const char* environment = k_MouseAt.Get();
	if (!Locator::inputState::has_value())
	{
		return environment;
	}
	return MouseAtFrom(Locator::inputState::value().Get<MouseAtState>().override, environment);
}

void OverrideMouseAt(std::string value)
{
	if (!Locator::inputState::has_value())
	{
		std::fputs("input: no input state in the locator (Locator::inputState)\n", stderr);
		std::abort();
	}
	Locator::inputState::value().Get<MouseAtState>().override = std::move(value);
}

const char* MouseAtFrom(const std::optional<std::string>& override, const char* environment)
{
	if (!override.has_value())
	{
		return environment;
	}
	// as setting the variable to nothing, which removes it
	return override->empty() ? nullptr : override->c_str();
}

} // namespace openblack::input
