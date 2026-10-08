/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CameraHelp.h"

#include "ECS/Systems/ScriptStateInterface.h"
#include "Locator.h"

namespace openblack::camera_help
{
namespace
{
/// What this module keeps between calls (Locator::scriptState)
struct CameraHelpState
{
	int32_t enabledFeatures {k_NormalFeatures};
	float autoPitchAngle {k_DefaultAutoPitchAngle};
	float autoPitchDistance {k_DefaultAutoPitchDistance};
};

CameraHelpState& CameraHelpData()
{
	return openblack::Locator::scriptState::value().Get<CameraHelpState>();
}
} // namespace

void EnableCameraFeatures(int32_t features, int32_t mask)
{
	auto& state = CameraHelpData();
	state.enabledFeatures = (state.enabledFeatures & ~mask) | features;
}

void SetAutoPitch(float angle, float distance, bool on)
{
	auto& state = CameraHelpData();
	// features = on ? 0x40 : 0, mask = on ? 0 : 0x40
	EnableCameraFeatures(on ? Bit(Feature::AutoPitch) : 0, on ? 0 : Bit(Feature::AutoPitch));
	state.autoPitchAngle = angle;
	state.autoPitchDistance = distance;
}

int32_t GetEnabledFeatures()
{
	return CameraHelpData().enabledFeatures;
}

bool IsFeatureEnabled(Feature feature)
{
	return (CameraHelpData().enabledFeatures & Bit(feature)) != 0;
}

float GetAutoPitchAngle()
{
	return CameraHelpData().autoPitchAngle;
}

float GetAutoPitchDistance()
{
	return CameraHelpData().autoPitchDistance;
}

void Reset()
{
	auto& state = CameraHelpData();
	state.enabledFeatures = k_NormalFeatures;
	state.autoPitchAngle = k_DefaultAutoPitchAngle;
	state.autoPitchDistance = k_DefaultAutoPitchDistance;
}

} // namespace openblack::camera_help
