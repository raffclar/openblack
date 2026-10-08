/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Locator.h"

namespace openblack::test
{
/// The services every test starts with: the RNG and the clock at their start state, and the particle system's empty
/// state. The listener in TestServices.cpp calls these around each TEST; a suite that keeps a Game across its tests
/// calls EmplaceTestServices before destroying it, so the Game's shutdown finds them
void EmplaceTestServices();
void ResetTestServices();

/// The production villager and map queries: villagerFields, villagerFishFarms, villagerBuildingSites, villagerStores,
/// villagerTentQueries, townCellObjects, mapShapeProvider and meshBoxProvider. A fixture whose code reaches them calls
/// EmplaceMapAndVillagerDefaults in its SetUp, before it puts in fakes of its own, and ResetMapAndVillagerDefaults in
/// its TearDown
void EmplaceMapAndVillagerDefaults();
void ResetMapAndVillagerDefaults();

/// A DefaultFileSystem in Locator::filesystem while it lives, for the code that reads real files through the file
/// system (MusicBank::Register): the file system from before is put back when it goes
class ScopedDefaultFileSystem
{
public:
	ScopedDefaultFileSystem();
	~ScopedDefaultFileSystem();
	ScopedDefaultFileSystem(const ScopedDefaultFileSystem&) = delete;
	ScopedDefaultFileSystem& operator=(const ScopedDefaultFileSystem&) = delete;
	ScopedDefaultFileSystem(ScopedDefaultFileSystem&&) = delete;
	ScopedDefaultFileSystem& operator=(ScopedDefaultFileSystem&&) = delete;

private:
	Locator::filesystem::node_type _previous;
};
} // namespace openblack::test
