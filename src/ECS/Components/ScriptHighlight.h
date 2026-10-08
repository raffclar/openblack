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

#include <optional>

#include "Enums.h"

namespace openblack::ecs::components
{

/// A script highlight (a single-map fixed object): the scroll or did-you-know sign a script puts on the map
/// (CREATE_HIGHLIGHT 272) or an abode puts by its door (when it creates its surrounding objects). The entity also has
/// a Transform (the position the model gets each frame) and a Mesh (the info's normal or active mesh). Logic:
/// ECS/ScriptHighlight.h.
struct ScriptHighlight
{
	/// The script highlight info row, 0..3 (ecs::script_highlight::Info)
	uint32_t infoIndex {0};
	/// Where it stands (x, z) and its altitude over the ground, which Process sets every turn (a ground query or the
	/// draw height) and SET_PROPERTY YPOS writes
	float x {0.0f};
	float z {0.0f};
	float altitude {0.0f};
	/// The fixed object's scale (Create's last argument, 1.0 from CREATE_HIGHLIGHT)
	float scale {1.0f};
	/// Nothing is drawn while it is set; Save / Load keep it. (pending) no writer found besides a reset to 0 and Load
	int32_t hidden {0};
	/// Set by SetActivated, read by IsActive
	bool active {false};
	/// The glints, the info's glint effect made at creation; psys::manager id
	uint32_t glintsEffect {0};
	/// The active effect, the info's active effect (SetActivated); psys::manager id
	uint32_t activeEffect {0};
	/// The script id. Create's third argument (CREATE_HIGHLIGHT's challenge id), then HIGHLIGHT_PROPERTIES' text
	/// (SetScriptId): the HelpText of a did-you-know, the challenge of a scroll
	uint32_t scriptId {0};
	/// SetDrawHeight (SET_PROPERTY YPOS): Process keeps the altitude at drawHeight; none until it is set
	std::optional<float> drawHeight;
	/// SetScriptId's DYK_CATEGORY
	DykCategory category {DykCategory::Navigation};
	/// The model's Y angle, which Draw turns by the frame's game time x pi / 1000 every frame
	float drawAngle {0.0f};
};

} // namespace openblack::ecs::components
