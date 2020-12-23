/*****************************************************************************
 * Copyright (c) 2018-2020 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include "3D/L3DMesh.h"
#include "Graphics/Mesh.h"
#include "Graphics/Texture2D.h"

#include <Sound/Sound.h>
#include <memory>

namespace openblack
{
enum class HandRenderType
{
	Model,
	Symbol
};

class Hand
{
public:
	void Init();
	static const std::vector<audio::SoundId>& GrabLandSoundIds() { return Hand::_grabLandSounds; };
private:
	HandRenderType handType;
	std::unique_ptr<L3DMesh> m_HandModel;
	static const std::vector<audio::SoundId> _grabLandSounds;
};
} // namespace openblack
