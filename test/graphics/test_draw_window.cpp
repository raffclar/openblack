/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "Graphics/DrawWindow.h"

using namespace openblack::graphics;

namespace
{

constexpr uint16_t k_Parted = 3;
constexpr uint16_t k_Other = 7;
constexpr std::array<uint16_t, 1> k_Views {k_Parted};

// The indices of the draws into the parted view that go, of the count that come
[[nodiscard]] std::vector<uint32_t> Admitted(DrawWindow& window, uint32_t count)
{
	std::vector<uint32_t> admitted;
	for (uint32_t i = 0; i < count; ++i)
	{
		if (window.Admit(k_Parted))
		{
			admitted.push_back(i);
		}
	}
	return admitted;
}

} // namespace

// A closed window lets every draw go
TEST(DrawWindow, ClosedLetsAllGo)
{
	DrawWindow window;
	EXPECT_FALSE(window.IsOpen());
	EXPECT_EQ(Admitted(window, 5).size(), 5u);
}

// An open window lets only its run of the draws into its views go, and counts them all
TEST(DrawWindow, OpenLetsItsRunGo)
{
	DrawWindow window;
	window.Open(2, 3, k_Views);
	EXPECT_EQ(Admitted(window, 8), (std::vector<uint32_t> {2, 3, 4}));
	EXPECT_EQ(window.Close(), 8u);
	EXPECT_FALSE(window.IsOpen());
	EXPECT_EQ(Admitted(window, 4).size(), 4u);
}

// Draws into other views always go and the window does not count them
TEST(DrawWindow, OtherViewsAlwaysGo)
{
	DrawWindow window;
	window.Open(0, 1, k_Views);
	EXPECT_TRUE(window.Admit(k_Parted));
	EXPECT_TRUE(window.Admit(k_Other));
	EXPECT_FALSE(window.Admit(k_Parted));
	EXPECT_TRUE(window.Admit(k_Other));
	EXPECT_EQ(window.Close(), 2u);
}

// The window starts its count again each time it opens
TEST(DrawWindow, OpenStartsTheCountAgain)
{
	DrawWindow window;
	window.Open(0, 2, k_Views);
	EXPECT_EQ(Admitted(window, 5).size(), 2u);
	window.Open(2, 2, k_Views);
	EXPECT_EQ(Admitted(window, 5), (std::vector<uint32_t> {2, 3}));
}

// The parts of a warm-up cover every draw once, and the last part says that no draws are left
TEST(DrawWindow, WarmUpPartsCoverEveryDrawOnce)
{
	constexpr uint32_t k_PerPart = 4;
	for (const uint32_t total : {0u, 1u, 4u, 5u, 11u, 12u})
	{
		DrawWindow window;
		std::vector<uint32_t> drawn;
		uint32_t parts = 0;
		for (uint32_t part = 0;; ++part)
		{
			const auto slice = WarmUpPartWindow(part, k_PerPart);
			window.Open(slice.first, slice.count, k_Views);
			for (const auto index : Admitted(window, total))
			{
				drawn.push_back(index);
			}
			++parts;
			if (!WarmUpPartsLeft(part, window.Close(), k_PerPart))
			{
				break;
			}
		}
		std::vector<uint32_t> expected(total);
		for (uint32_t i = 0; i < total; ++i)
		{
			expected[i] = i;
		}
		EXPECT_EQ(drawn, expected) << total;
		EXPECT_EQ(parts, total == 0 ? 1u : (total + k_PerPart - 1) / k_PerPart) << total;
	}
}
