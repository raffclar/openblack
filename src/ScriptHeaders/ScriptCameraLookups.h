/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Camera/ScriptCameraModel.h"

namespace openblack::chlapi
{

/// How the script's camera finds its things and its track again after a load of a saved game, as the scripts find them
[[nodiscard]] ScriptCameraModel::Lookups ScriptCameraLookups();

} // namespace openblack::chlapi
