/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "Particles/Rules/ExplodeObject.h"
#include "Resources/Loaders.h"

using openblack::resources::SourceMeshLoader;

TEST(SourceMeshLoader, BytesThatAreNotAnL3DAreRejected)
{
	EXPECT_FALSE(openblack::psys::explode_object::ReadSourceMesh(3, {}).has_value());
	const std::vector<uint8_t> junk(64, 0xAB);
	EXPECT_FALSE(openblack::psys::explode_object::ReadSourceMesh(3, junk).has_value());
	EXPECT_THROW((void)SourceMeshLoader {}(SourceMeshLoader::FromBufferTag {}, 3, junk), std::runtime_error);
}
