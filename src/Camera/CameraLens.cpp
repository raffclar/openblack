/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CameraLens.h"

#include <cmath>

#include <algorithm>

namespace openblack::camera_lens
{

float VerticalFieldOfView(float lens, float aspect)
{
	// A wider screen than 4:3 sees up and down what a 4:3 screen sees
	return std::atan(std::tan(lens / 2.0f) / std::min(aspect, k_ReferenceAspect)) * 2.0f;
}

float HorizontalFieldOfView(float lens, float aspect)
{
	if (aspect <= k_ReferenceAspect)
	{
		return lens;
	}
	return std::atan(std::tan(VerticalFieldOfView(lens, aspect) / 2.0f) * aspect) * 2.0f;
}

} // namespace openblack::camera_lens
