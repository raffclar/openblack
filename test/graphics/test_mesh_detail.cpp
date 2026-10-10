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
#include "Graphics/MeshDetail.h"

using namespace openblack::graphics;
using mesh_detail::Mesh;

TEST(MeshDetail, ReachGrowsWithSizeImportanceAndDetail)
{
	EXPECT_FLOAT_EQ(mesh_detail::Reach(0.0f, 1.0f, 0.5f), 0.5f);
	EXPECT_FLOAT_EQ(mesh_detail::Reach(0.025f, 2.0f, 0.5f), 1.025f);
	EXPECT_FLOAT_EQ(mesh_detail::Reach(0.0f, 1.0f, 0.2f), 0.2f);
	// However big, no more than 5
	EXPECT_FLOAT_EQ(mesh_detail::Reach(1.0f, 100.0f, 0.5f), 5.0f);
}

TEST(MeshDetail, ModelDetailByLevel)
{
	EXPECT_FLOAT_EQ(detail_level::ModelDetail(0), 0.2f);
	EXPECT_FLOAT_EQ(detail_level::ModelDetail(2), 0.4f);
	EXPECT_FLOAT_EQ(detail_level::ModelDetail(detail_level::k_Default), 0.5f);
	EXPECT_FLOAT_EQ(detail_level::ModelDetail(200), 0.5f);
}

TEST(MeshDetail, BandsSwitchAtOnce)
{
	// A reach of 1: high below 23.33, standard below 66.67, low below 86.67
	EXPECT_EQ(mesh_detail::Choose(0.0f, 1.0f, true).mesh, Mesh::High);
	EXPECT_EQ(mesh_detail::Choose(23.3f, 1.0f, true).mesh, Mesh::High);
	EXPECT_EQ(mesh_detail::Choose(23.34f, 1.0f, true).mesh, Mesh::Standard);
	EXPECT_EQ(mesh_detail::Choose(66.6f, 1.0f, true).mesh, Mesh::Standard);
	EXPECT_EQ(mesh_detail::Choose(66.7f, 1.0f, true).mesh, Mesh::Low);
	const auto low = mesh_detail::Choose(86.6f, 1.0f, true);
	EXPECT_EQ(low.mesh, Mesh::Low);
	EXPECT_FALSE(low.alpha.has_value());
	// Behind the eye counts as close
	EXPECT_EQ(mesh_detail::Choose(-10.0f, 1.0f, true).mesh, Mesh::High);
}

TEST(MeshDetail, FadesOutThenGoes)
{
	const auto start = mesh_detail::Choose(86.666664f, 1.0f, true);
	EXPECT_EQ(start.mesh, Mesh::Low);
	EXPECT_EQ(start.alpha, 255);
	// Part way through, rounded to the nearest: 68.65 of 255
	const auto part = mesh_detail::Choose(150.0f, 1.0f, true);
	EXPECT_EQ(part.mesh, Mesh::Low);
	EXPECT_EQ(part.alpha, 69);
	const auto late = mesh_detail::Choose(170.0f, 1.0f, true);
	EXPECT_EQ(late.alpha, 10);
	// Gone from 173.33 on
	const auto gone = mesh_detail::Choose(173.4f, 1.0f, true);
	EXPECT_FALSE(gone.mesh.has_value());
	EXPECT_FALSE(gone.alpha.has_value());
}

TEST(MeshDetail, WhatNeverDisappearsStaysLow)
{
	const auto fading = mesh_detail::Choose(130.0f, 1.0f, false);
	EXPECT_EQ(fading.mesh, Mesh::Low);
	EXPECT_FALSE(fading.alpha.has_value());
	const auto far = mesh_detail::Choose(10000.0f, 1.0f, false);
	EXPECT_EQ(far.mesh, Mesh::Low);
	EXPECT_FALSE(far.alpha.has_value());
}

TEST(MeshDetail, BandsScaleWithReach)
{
	EXPECT_EQ(mesh_detail::Choose(11.0f, 0.5f, true).mesh, Mesh::High);
	EXPECT_EQ(mesh_detail::Choose(12.0f, 0.5f, true).mesh, Mesh::Standard);
	EXPECT_FALSE(mesh_detail::Choose(87.0f, 0.5f, true).mesh.has_value());
}

TEST(MeshDetail, VillagerImportanceIsAThousandthOfItsAge)
{
	EXPECT_FLOAT_EQ(mesh_detail::VillagerImportance(0), 0.0f);
	EXPECT_FLOAT_EQ(mesh_detail::VillagerImportance(25), 0.025f);
}

namespace
{
/// A made-up person: a box whose diagonal is 2 long, so its sphere's radius is 1 at scale 1
constexpr glm::vec3 k_PersonBox {1.2f, 1.6f, 0.0f};
/// An adult of 20 at full size, and a young child at about half size
constexpr float k_AdultImportance = 0.02f;
constexpr float k_ChildImportance = 0.005f;
constexpr float k_ChildScale = 0.6f;
} // namespace

TEST(MeshDetail, RadiusIsHalfTheBoxDiagonalAtScale)
{
	EXPECT_FLOAT_EQ(mesh_detail::ScaledRadius(k_PersonBox, 1.0f), 1.0f);
	EXPECT_FLOAT_EQ(mesh_detail::ScaledRadius(k_PersonBox, 0.5f), 0.5f);
}

TEST(MeshDetail, DepthIsAlongTheViewNotTheStraightDistance)
{
	const glm::vec3 eye {10.0f, 5.0f, 0.0f};
	const glm::vec3 forward {0.0f, 0.0f, 1.0f};
	EXPECT_FLOAT_EQ(mesh_detail::ViewDepth({10.0f, 5.0f, 40.0f}, eye, forward), 40.0f);
	// Off to the side and up, at the same depth: 50 away in a straight line, still 40 deep
	EXPECT_FLOAT_EQ(mesh_detail::ViewDepth({40.0f, 5.0f, 40.0f}, eye, forward), 40.0f);
	EXPECT_FLOAT_EQ(mesh_detail::ViewDepth({10.0f, 5.0f, -3.0f}, eye, forward), -3.0f);
	// So the person to the side is drawn as the one straight ahead
	const float reach = mesh_detail::Reach(k_AdultImportance, mesh_detail::ScaledRadius(k_PersonBox, 1.0f),
	                                       detail_level::ModelDetail(detail_level::k_Default));
	EXPECT_EQ(mesh_detail::Choose(mesh_detail::ViewDepth({40.0f, 5.0f, 40.0f}, eye, forward), reach, true).alpha,
	          mesh_detail::Choose(mesh_detail::ViewDepth({10.0f, 5.0f, 40.0f}, eye, forward), reach, true).alpha);
}

TEST(MeshDetail, AnAdultsBandsAtTheDefaultDetail)
{
	// 1.02 x 1 x 0.5: high to about 11.9 deep, standard to 34, low to 44.2, then fading out to 88.4
	const float reach = mesh_detail::Reach(k_AdultImportance, mesh_detail::ScaledRadius(k_PersonBox, 1.0f),
	                                       detail_level::ModelDetail(detail_level::k_Default));
	EXPECT_FLOAT_EQ(reach, 0.51f);
	EXPECT_EQ(mesh_detail::Choose(11.8f, reach, true).mesh, Mesh::High);
	EXPECT_EQ(mesh_detail::Choose(12.0f, reach, true).mesh, Mesh::Standard);
	EXPECT_EQ(mesh_detail::Choose(33.9f, reach, true).mesh, Mesh::Standard);
	EXPECT_EQ(mesh_detail::Choose(34.1f, reach, true).mesh, Mesh::Low);
	EXPECT_FALSE(mesh_detail::Choose(44.1f, reach, true).alpha.has_value());
	const auto fading = mesh_detail::Choose(66.2f, reach, true);
	EXPECT_EQ(fading.mesh, Mesh::Low);
	EXPECT_EQ(fading.alpha, 128);
	EXPECT_EQ(mesh_detail::Choose(88.3f, reach, true).mesh, Mesh::Low);
	EXPECT_FALSE(mesh_detail::Choose(88.5f, reach, true).mesh.has_value());
}

TEST(MeshDetail, AChildGoesSoonerThanAnAdult)
{
	const float modelDetail = detail_level::ModelDetail(detail_level::k_Default);
	const float adult = mesh_detail::Reach(k_AdultImportance, mesh_detail::ScaledRadius(k_PersonBox, 1.0f), modelDetail);
	const float child =
	    mesh_detail::Reach(k_ChildImportance, mesh_detail::ScaledRadius(k_PersonBox, k_ChildScale), modelDetail);
	// The child's reach is 1.005 x 0.6 x 0.5 = 0.3015: gone from about 52.3 deep, where the adult is still fading
	EXPECT_FLOAT_EQ(child, 0.3015f);
	EXPECT_FALSE(mesh_detail::Choose(60.0f, child, true).mesh.has_value());
	EXPECT_TRUE(mesh_detail::Choose(60.0f, adult, true).alpha.has_value());
	EXPECT_EQ(mesh_detail::Choose(8.0f, child, true).mesh, Mesh::Standard);
	EXPECT_EQ(mesh_detail::Choose(8.0f, adult, true).mesh, Mesh::High);
}

TEST(MeshDetail, LowerDetailSettingsSwapSooner)
{
	const float radius = mesh_detail::ScaledRadius(k_PersonBox, 1.0f);
	const float lowest = mesh_detail::Reach(k_AdultImportance, radius, detail_level::ModelDetail(0));
	// 1.02 x 0.2: the standard mesh from about 4.8 deep, gone from 35.4
	EXPECT_EQ(mesh_detail::Choose(5.0f, lowest, true).mesh, Mesh::Standard);
	EXPECT_FALSE(mesh_detail::Choose(36.0f, lowest, true).mesh.has_value());
	// Every setting from the middle one up gives the same bands as the default
	for (uint8_t level = 3; level <= 6; ++level)
	{
		EXPECT_FLOAT_EQ(detail_level::ModelDetail(level), detail_level::ModelDetail(detail_level::k_Default));
	}
}
