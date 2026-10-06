/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>

#include "FormTable.h"

namespace openblack::lhvm::chl
{

std::span<const StatementForm> SupplementaryForms()
{
	// ":CAMERA" is a camera enum, named with or without its challenge's prefix. ":CURRENT_CHALLENGE" is the id of the challenge
	// being compiled ("challenge NAME"), which isn't written
	static constexpr auto k_Forms = std::to_array<StatementForm>({
	    {.native = "CREATE_HIGHLIGHT", .pattern = "create highlight $0 at $1 $2:CURRENT_CHALLENGE"},
	    {.native = "GET_INFLUENCE", .pattern = "influence $0=1 $1=0 at $2"},
	    // "marker at camera ENUM": the focus of a camera enum is the marker's position
	    {.native = "CONVERT_CAMERA_FOCUS", .pattern = "camera $0:CAMERA"},
	});
	return k_Forms;
}

} // namespace openblack::lhvm::chl
