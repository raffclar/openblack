/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <functional>

namespace openblack::ecs::systems
{

/// Which script task has the dialogue: the advisors' talk and the help text. A task takes it for its cut scenes and
/// help, and gives it back when done or when it stops. The help system hears of it through the hooks.
class DialogueControlSystemInterface
{
public:
	/// What the help system does as the dialogue changes hands
	struct Hooks
	{
		/// Both advisors are sent home; `helpScript` when it is a help script that sends them
		std::function<void(bool helpScript)> sendSpiritsHome;
		/// A task has taken the dialogue: the help text on screen is cleared
		std::function<void()> taken;
		/// The task with the dialogue let it go: the help system puts its advisors and text away; `helpScript` when the
		/// task was a help script
		std::function<void(bool helpScript)> released;
	};

	virtual ~DialogueControlSystemInterface() = default;

	virtual void SetHooks(Hooks hooks) = 0;

	/// The task with the dialogue, 0 for none
	[[nodiscard]] virtual uint32_t GetOwner() const = 0;
	/// Whether the dialogue is taken: by a task, or by the cinema bars a task has brought in (`wideScreenOwner`, 0 for
	/// none)
	[[nodiscard]] virtual bool IsControlled(uint32_t wideScreenOwner) const = 0;
	/// A task asks for the dialogue: it gets it unless it is taken, and the help text is cleared
	virtual void Request(uint32_t task, uint32_t wideScreenOwner) = 0;
	/// A task gives the dialogue back, if it has it; true when it did
	virtual bool Release(uint32_t task, bool helpScript) = 0;
	/// Both advisors are sent home
	virtual void SendSpiritsHome(bool helpScript) = 0;

	/// As a new land opens: nobody has the dialogue
	virtual void Reset() = 0;
};

} // namespace openblack::ecs::systems
