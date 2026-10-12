/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <gtest/gtest.h>

#include "Graphics/DetailLevel.h"

using namespace openblack::graphics;

TEST(DetailLevel, TheDefaultIsMaximumDetail)
{
	EXPECT_EQ(detail_level::k_Default, detail_level::k_Maximum);
	const auto settings = detail_level::For(detail_level::k_Default);
	EXPECT_FLOAT_EQ(settings.modelDetail, 0.5f);
	EXPECT_FLOAT_EQ(settings.waterTiling, 0.8f);
	EXPECT_EQ(settings.rainSplashes, 8);
	EXPECT_TRUE(settings.clouds);
	EXPECT_TRUE(settings.cloudShadows);
	EXPECT_TRUE(settings.landReflection);
	EXPECT_TRUE(settings.templeReflections);
	EXPECT_TRUE(settings.templeVolumeLight);
	EXPECT_TRUE(settings.templeGlows);
	EXPECT_TRUE(settings.templePeople);
	EXPECT_TRUE(settings.templeLightmaps);
	EXPECT_TRUE(settings.fog);
	EXPECT_TRUE(settings.weather);
	EXPECT_TRUE(settings.objectLighting);
	EXPECT_TRUE(settings.objectShadows);
	EXPECT_FALSE(settings.lowResTextures);
	EXPECT_FALSE(settings.coarseLand);
}

TEST(DetailLevel, MinimumDetailDropsEverything)
{
	const auto settings = detail_level::For(0);
	EXPECT_FLOAT_EQ(settings.modelDetail, 0.2f);
	EXPECT_FLOAT_EQ(settings.waterTiling, 0.0f);
	EXPECT_EQ(settings.rainSplashes, 0);
	EXPECT_FALSE(settings.clouds);
	EXPECT_FALSE(settings.cloudShadows);
	EXPECT_FALSE(settings.landReflection);
	EXPECT_FALSE(settings.templeReflections);
	EXPECT_FALSE(settings.templeVolumeLight);
	EXPECT_FALSE(settings.templeGlows);
	EXPECT_FALSE(settings.templePeople);
	EXPECT_FALSE(settings.templeLightmaps);
	EXPECT_FALSE(settings.fog);
	EXPECT_FALSE(settings.weather);
	EXPECT_FALSE(settings.objectLighting);
	EXPECT_FALSE(settings.objectShadows);
	EXPECT_TRUE(settings.lowResTextures);
	EXPECT_TRUE(settings.coarseLand);
	// The bump texture is on at every level
	EXPECT_TRUE(settings.smallBump);
}

TEST(DetailLevel, EachLevelAddsItsFeatures)
{
	// Low: the temple's light maps and the land's own painted textures
	EXPECT_TRUE(detail_level::For(1).templeLightmaps);
	EXPECT_FALSE(detail_level::For(1).coarseLand);
	EXPECT_TRUE(detail_level::For(1).lowResTextures);
	EXPECT_FALSE(detail_level::For(1).objectLighting);
	// Medium: full textures, lit models, the temple's glows and worshippers, a few rain splashes
	const auto medium = detail_level::For(2);
	EXPECT_FALSE(medium.lowResTextures);
	EXPECT_TRUE(medium.objectLighting);
	EXPECT_TRUE(medium.templeGlows);
	EXPECT_TRUE(medium.templePeople);
	EXPECT_EQ(medium.rainSplashes, 3);
	EXPECT_FALSE(medium.fog);
	EXPECT_FALSE(medium.objectShadows);
	// High: clouds, haze, weather, the land's reflection, moving shadows and the temple's light shafts
	const auto high = detail_level::For(3);
	EXPECT_TRUE(high.clouds);
	EXPECT_TRUE(high.fog);
	EXPECT_TRUE(high.weather);
	EXPECT_TRUE(high.landReflection);
	EXPECT_TRUE(high.objectShadows);
	EXPECT_TRUE(high.templeVolumeLight);
	EXPECT_FALSE(high.templeReflections);
	EXPECT_EQ(high.rainSplashes, 5);
	// Maximum: the temple's reflections
	EXPECT_TRUE(detail_level::For(4).templeReflections);
}

TEST(DetailLevel, TheSeaTilesMoreFinelyWithDetail)
{
	EXPECT_FLOAT_EQ(detail_level::For(1).waterTiling, 0.2f);
	EXPECT_FLOAT_EQ(detail_level::For(3).waterTiling, 0.6f);
	// Custom starts halfway, the super level is the finest
	EXPECT_FLOAT_EQ(detail_level::For(detail_level::k_Custom).waterTiling, 0.5f);
	EXPECT_FLOAT_EQ(detail_level::For(6).waterTiling, 1.0f);
}

TEST(DetailLevel, SuperDetailOnlyChangesTheSea)
{
	auto super = detail_level::For(6);
	const auto maximum = detail_level::For(detail_level::k_Maximum);
	super.waterTiling = maximum.waterTiling;
	EXPECT_FLOAT_EQ(super.modelDetail, maximum.modelDetail);
	EXPECT_EQ(super.rainSplashes, maximum.rainSplashes);
	EXPECT_EQ(super.templeReflections, maximum.templeReflections);
	EXPECT_EQ(super.objectShadows, maximum.objectShadows);
}

TEST(DetailLevel, LevelsBeyondTheLastAreTheLast)
{
	EXPECT_FLOAT_EQ(detail_level::For(200).waterTiling, detail_level::For(6).waterTiling);
}

TEST(DetailLevel, TheMenuWrapsTheHigherLevels)
{
	EXPECT_EQ(detail_level::MenuLevel(0), 0);
	EXPECT_EQ(detail_level::MenuLevel(4), 4);
	EXPECT_EQ(detail_level::MenuLevel(5), 0);
	EXPECT_EQ(detail_level::MenuLevel(6), 1);
}

TEST(DetailLevel, TheTempleLightsNeedTheirShafts)
{
	// Medium has glows but no light shafts, so the room itself shows no glows
	const auto medium = detail_level::For(2);
	EXPECT_FALSE(detail_level::TempleLightShafts(medium));
	EXPECT_FALSE(detail_level::TempleGlowsShown(medium, false));
	EXPECT_TRUE(detail_level::TempleGlowsShown(medium, true));
	const auto high = detail_level::For(3);
	EXPECT_TRUE(detail_level::TempleLightShafts(high));
	EXPECT_TRUE(detail_level::TempleGlowsShown(high, false));
	EXPECT_FALSE(detail_level::TempleGlowsShown(detail_level::For(1), true));
}

TEST(DetailLevel, TheTempleFloorReflectsAtMaximumWithLightMaps)
{
	EXPECT_FALSE(detail_level::TempleReflected(detail_level::For(3)));
	EXPECT_TRUE(detail_level::TempleReflected(detail_level::For(4)));
	auto noLightmaps = detail_level::For(4);
	noLightmaps.templeLightmaps = false;
	EXPECT_FALSE(detail_level::TempleReflected(noLightmaps));
}
