/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Graphics/DeviceDataGivers.h"
#include "Resources/Rereads.h"

using namespace openblack;

namespace
{

/// A reader that counts the reads of each file and gives the bytes of the files that it has
class FakeFiles
{
public:
	std::vector<uint8_t> Read(const std::filesystem::path& path)
	{
		reads.push_back(path.generic_string());
		if (path == "colours.raw")
		{
			return {1, 2, 3};
		}
		return {};
	}

	std::vector<std::string> reads;
};

} // namespace

TEST(DeviceDataGivers, EachOwnerGivesItsDataOneTimeInTheOrderOfTheAdds)
{
	graphics::DeviceDataGivers givers;
	std::vector<int> order;
	const int first = 0;
	const int second = 0;
	givers.Add(&first, [&order](resources::Rereads&) { order.push_back(1); });
	givers.Add(&second, [&order](resources::Rereads&) { order.push_back(2); });
	EXPECT_EQ(givers.Count(), 2U);

	FakeFiles files;
	resources::Rereads rereads([&files](const std::filesystem::path& path) { return files.Read(path); });
	givers.GiveAll(rereads);
	EXPECT_EQ(order, (std::vector<int> {1, 2}));
}

TEST(DeviceDataGivers, AnOwnerHasOneGiverAndARemovedOwnerGivesNothing)
{
	graphics::DeviceDataGivers givers;
	std::vector<int> order;
	const int owner = 0;
	givers.Add(&owner, [&order](resources::Rereads&) { order.push_back(1); });
	// A second add replaces the giver of the owner
	givers.Add(&owner, [&order](resources::Rereads&) { order.push_back(2); });
	EXPECT_EQ(givers.Count(), 1U);

	FakeFiles files;
	resources::Rereads rereads([&files](const std::filesystem::path& path) { return files.Read(path); });
	givers.GiveAll(rereads);
	EXPECT_EQ(order, (std::vector<int> {2}));

	givers.Remove(&owner);
	EXPECT_EQ(givers.Count(), 0U);
	givers.GiveAll(rereads);
	EXPECT_EQ(order, (std::vector<int> {2}));
}

TEST(DeviceDataGivers, EachGiveOfAllCountsOneRestore)
{
	// Data that no owner holds, as a texture kept for a later use, compares the count to know of a restore
	graphics::DeviceDataGivers givers;
	EXPECT_EQ(givers.Restores(), 0U);
	FakeFiles files;
	resources::Rereads rereads([&files](const std::filesystem::path& path) { return files.Read(path); });
	givers.GiveAll(rereads);
	givers.GiveAll(rereads);
	EXPECT_EQ(givers.Restores(), 2U);
}

TEST(Rereads, AFileIsReadEachTimeAndAPackThatCannotBeReadIsReadOneTime)
{
	FakeFiles files;
	resources::Rereads rereads([&files](const std::filesystem::path& path) { return files.Read(path); });
	EXPECT_EQ(rereads.File("colours.raw"), (std::vector<uint8_t> {1, 2, 3}));
	EXPECT_EQ(rereads.File("colours.raw"), (std::vector<uint8_t> {1, 2, 3}));

	// The pack is not there: the rereads keep the result and do not read the pack again
	EXPECT_EQ(rereads.Pack("AllMeshes.g3d"), nullptr);
	EXPECT_EQ(rereads.Pack("AllMeshes.g3d"), nullptr);
	EXPECT_EQ(files.reads, (std::vector<std::string> {"colours.raw", "colours.raw", "AllMeshes.g3d"}));
	EXPECT_EQ(rereads.ReadCount(), 3U);
}

TEST(Rereads, BytesThatAreNotAPackGiveNoPack)
{
	resources::Rereads rereads([](const std::filesystem::path&) { return std::vector<uint8_t> {0, 1, 2, 3}; });
	EXPECT_EQ(rereads.Pack("broken.g3d"), nullptr);
	EXPECT_EQ(rereads.Pack("broken.g3d"), nullptr);
	EXPECT_EQ(rereads.ReadCount(), 1U);
}
