/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// CHL natives of the influence (ECS/Influence), called from CHLApi.cpp. They pop and push the VM stack themselves.

namespace openblack::magic::script
{
/// 060 INFLUENCE_OBJECT
void InfluenceObject();
/// 061 INFLUENCE_POSITION
void InfluencePosition();
/// 062 GET_INFLUENCE
void GetInfluence();
} // namespace openblack::magic::script
