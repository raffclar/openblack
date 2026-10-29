/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "UploadMode.h"

#include "EngineConfig.h"
#include "Locator.h"

openblack::graphics::UploadMode openblack::graphics::ConfiguredUploads()
{
	return Locator::config::has_value() ? Locator::config::value().resourceUploads : UploadMode::Device;
}
