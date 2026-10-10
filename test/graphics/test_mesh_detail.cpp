/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

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
