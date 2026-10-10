/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptChallengeSnapshots.h"

#include <algorithm>

namespace openblack::script::challenge_snapshots
{

namespace
{
glm::vec3 PopVector(const Stack& stack)
{
	const auto z = stack.pop().value.floatVal;
	const auto y = stack.pop().value.floatVal;
	const auto x = stack.pop().value.floatVal;
	return {x, y, z};
}
} // namespace

size_t MaxArguments(Call call)
{
	switch (call)
	{
	case Call::Start:
		return 12;
	case Call::Update:
		return 11;
	case Call::Picture:
		return 0;
	}
	return 0;
}

float ClampAlignment(float alignment)
{
	return alignment >= -1.0f ? std::min(alignment, 1.0f) : -1.0f;
}

float ClampSuccess(float success)
{
	return success >= 0.0f ? std::min(success, 1.0f) : 0.0f;
}

Snapshot Read(Call call, const Stack& stack)
{
	Snapshot snapshot;
	snapshot.challenge = stack.pop().value.uintVal;
	if (call == Call::Picture)
	{
		snapshot.takingPicture = stack.pop().value.intVal != 0;
	}
	else
	{
		// Every argument is taken, however many there are; no more than a full stack can have been given
		const auto count = std::min<size_t>(stack.pop().value.uintVal, lhvm::VMStack::k_Size);
		snapshot.tooManyArguments = count > MaxArguments(call);
		snapshot.arguments.reserve(count);
		for (size_t i = 0; i < count; ++i)
		{
			snapshot.arguments.push_back(stack.pop());
		}
		snapshot.reminderScript = stack.text(stack.pop().value.uintVal);
	}
	snapshot.title = stack.pop().value.uintVal;
	snapshot.alignment = ClampAlignment(stack.pop().value.floatVal);
	snapshot.success = ClampSuccess(stack.pop().value.floatVal);
	if (call != Call::Update)
	{
		snapshot.focus = PopVector(stack);
		snapshot.position = PopVector(stack);
	}
	if (call == Call::Start)
	{
		snapshot.quest = stack.pop().value.intVal != 0;
	}
	return snapshot;
}

} // namespace openblack::script::challenge_snapshots
