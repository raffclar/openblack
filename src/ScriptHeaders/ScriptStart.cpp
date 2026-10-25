/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptStart.h"

#include "ECS/Systems/TimeSystemInterface.h"
#include "Locator.h"

namespace openblack::script::start
{

lhvm::ScriptType StartableScripts()
{
	return StartableScripts(Locator::time::has_value() && Locator::time::value().IsNetworkGame());
}

} // namespace openblack::script::start
