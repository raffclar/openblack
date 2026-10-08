/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// src/3D/AffineMatrix.h against the original's operation orders (Mul, MultiplyReversed, Inverse, WorldToCamera and the
// per-frame camera update): every cell pinned as bits. The expected values come from the same formulas in float32,
// one rounding per operation

#include <cstdint>

#include <array>
#include <bit>
#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/AffineMatrix.h"

using openblack::affine::AffineMatrix;

namespace
{
AffineMatrix FromBits(const std::array<uint32_t, 12>& bits)
{
	AffineMatrix m;
	for (size_t i = 0; i < bits.size(); ++i)
	{
		m.m[i] = std::bit_cast<float>(bits[i]);
	}
	return m;
}

void ExpectBits(const AffineMatrix& actual, const std::array<uint32_t, 12>& expected)
{
	for (size_t i = 0; i < expected.size(); ++i)
	{
		EXPECT_EQ(std::bit_cast<uint32_t>(actual.m[i]), expected[i]) << "cell " << i;
	}
}

const std::array<uint32_t, 12> k_A {0x3F4CCCCDu, 0x3DCCCCCDu, 0xBE99999Au, 0x3E4CCCCDu, 0x3F666666u, 0x3ECCCCCDu,
                                    0x3E800000u, 0xBEB33333u, 0x3F8CCCCDu, 0x41480000u, 0xC0500000u, 0x40F80000u};
const std::array<uint32_t, 12> k_B {0x3F99999Au, 0xBECCCCCDu, 0x3E19999Au, 0x3E99999Au, 0x3F333333u, 0xBE4CCCCDu,
                                    0xBDCCCCCDu, 0x3EE66666u, 0x3F733333u, 0xC0D00000u, 0x40000000u, 0x3FA00000u};
const std::array<uint32_t, 12> k_MulAB {0x3F828F5Cu, 0xBEC51EBAu, 0xBE3D70A3u, 0x3EF0A3D8u, 0x3F3AE147u, 0x3E6B851Fu,
                                        0x3DAE147Cu, 0x3E19999Bu, 0x3F93851Fu, 0x40D80002u, 0xBFE4CCCCu, 0x41323333u};
const std::array<uint32_t, 12> k_MulPreAB {0x3F6AE148u, 0xBE95C28Fu, 0xBEB5C290u, 0x3EA8F5C3u, 0x3F3AE148u, 0xBCF5C290u,
                                           0x3E7D70A4u, 0x3D800000u, 0x3FA0A3D6u, 0x41003333u, 0xC0226667u, 0x413E0000u};
const std::array<uint32_t, 12> k_InverseA {0x3F93843Eu, 0xBBA71938u, 0x3EA1E068u, 0xBDFAA5CDu, 0x3F795799u, 0xBEC66DECu,
                                           0xBE9A0B3Au, 0x3E9F4404u, 0x3F36C38Eu, 0xC1478D1Fu, 0x3F518698u, 0xC12BE98Cu};
const std::array<uint32_t, 12> k_WorldToCamera {0x3F79AAAAu, 0x3D16EC0Bu, 0x3E5F32A5u, 0xB1800000u, 0x3F7C6B37u, 0xBE2AAE42u,
                                                0xBE625D4Du, 0x3E267547u, 0x3F762C90u, 0xC48C5B30u, 0xC408A690u, 0xC53697F2u};
const std::array<uint32_t, 12> k_WorldToClipping {0x3FB247CDu, 0x3D8FB154u, 0x3E5F32A5u, 0xB1B6CD8Eu, 0x3FF053DFu, 0xBE2AAE42u,
                                                  0xBEA1A417u, 0x3E9E7C0Cu, 0x3F762C90u, 0xC4C8730Eu, 0xC4821ADAu, 0xC53697F2u};
constexpr uint32_t k_Sx = 0x3FB6CD8Eu;
constexpr uint32_t k_Sy = 0x3FF3BCBEu;
// the guards of WorldToCamera: straight down, and the eye on the target. In both |dx| and |dz| are
// under 1e-4, so dx becomes -1e-4 (dx = 0 is not > 0), and the signed zeros are part of the result
const std::array<uint32_t, 12> k_StraightDown {0x00000000u, 0xBF800000u, 0xB727C5ACu, 0x00000000u, 0x00000000u, 0xBF800000u,
                                               0x3F800000u, 0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0x41200000u};
const std::array<uint32_t, 12> k_EyeAtTarget {0x00000000u, 0x00000000u, 0xBF800000u, 0x80000000u, 0x3F800000u, 0x00000000u,
                                              0x3F800000u, 0x00000000u, 0x00000000u, 0xC0E00000u, 0xC0C00000u, 0x40A00000u};
// the 70-degree lens (glm::radians(70.0f)) at 1024 x 768, and that camera's
// W and W^-1 at Land 1's flight camera
constexpr uint32_t k_Fov = 0x3F9C61AAu;
constexpr uint32_t k_Aspect = 0x3FAAAAABu;
constexpr uint32_t k_LensSx = 0x3FB6CD8Eu;
constexpr uint32_t k_LensSy = 0x3FF3BCBEu;
const std::array<uint32_t, 12> k_FrameClipping {0x3FB247CDu, 0x3D8FB154u, 0x3E5F32A5u, 0xB1B6CD8Eu, 0x3FF053DFu, 0xBE2AAE42u,
                                                0xBEA1A417u, 0x3E9E7C0Cu, 0x3F762C90u, 0xC4C8730Eu, 0xC4821ADAu, 0xC53697F2u};
const std::array<uint32_t, 12> k_FrameClippingInverse {0x3F2ED188u, 0xB13C4587u, 0xBE1E808Au, 0x3C9E83E0u,
                                                       0x3F048F26u, 0x3DAED536u, 0x3E5F32A7u, 0xBE2AAE41u,
                                                       0x3F762C91u, 0x44DAFFFFu, 0x42500008u, 0x4525A001u};
// a point where the three sites' orders round apart, through k_FrameClipping
constexpr std::array<uint32_t, 3> k_Point {0x44F1A74Fu, 0x42D02547u, 0x453DBC4Cu};
constexpr std::array<uint32_t, 3> k_PointScreenTest {0x4302A100u, 0x4365E560u, 0x43C8E458u};
constexpr std::array<uint32_t, 3> k_PointBoxTest {0x4302A100u, 0x4365E568u, 0x43C8E450u};
constexpr std::array<uint32_t, 3> k_PointShadowBlocks {0x4302A100u, 0x4365E560u, 0x43C8E450u};
// an object turned and tilted at Land 1, and a box centre where the original's order rounds
// apart from glm's (cx: glm's is one ulp higher)
constexpr std::array<uint32_t, 12> k_Object {0x3F7A11BDu, 0x3E652957u, 0xBF53FCFCu, 0xBE0367ECu, 0x3FA3B5A4u, 0x3E46EE9Au,
                                             0x3F591F3Cu, 0xBD838D5Cu, 0x3F7BAE68u, 0x44E13000u, 0x41F20000u, 0x452C8C00u};
constexpr std::array<uint32_t, 3> k_BoxCentre {0xC034FEA1u, 0x40131BFCu, 0xBF47DEAFu};
constexpr std::array<uint32_t, 3> k_BoxCentreThroughObject {0x44E0B8F8u, 0x42026DADu, 0x452CAC55u};
// an object where the bone root's order and Mul(object, W) round apart (cell 3: Mul is one ulp
// off), and a two-bone chain under it
constexpr std::array<uint32_t, 12> k_ObjectAt {0x3F5ED21Cu, 0x3EBB1C05u, 0xBF64CF81u, 0xBE54FF49u, 0x3F9F7D4Fu, 0x3E9D2244u,
                                               0x3F7164F9u, 0xBD7FE1AFu, 0x3F648946u, 0x44E13000u, 0x41F20000u, 0x452C8C00u};
constexpr std::array<uint32_t, 12> k_ObjectThroughW {0x3FBF4AE0u, 0x3EF0ECA0u, 0xBF3B0DF2u, 0xBEC5F0E0u,
                                                     0x401ADF13u, 0x3D2C6F84u, 0x3F8408B5u, 0x3E669A86u,
                                                     0x3F8985E3u, 0x4207EBE0u, 0xC0454400u, 0x42F1D3E0u};
constexpr std::array<uint32_t, 12> k_Bone0 {0x3F7AE148u, 0x00000000u, 0xBE4CCCCDu, 0x00000000u, 0x3F800000u, 0x00000000u,
                                            0x3E4CCCCDu, 0x00000000u, 0x3F7AE148u, 0x3DCCCCCDu, 0x3FD9999Au, 0xBD4CCCCDu};
constexpr std::array<uint32_t, 12> k_Bone1 {0x3F800000u, 0x3D4CCCCDu, 0x00000000u, 0xBD4CCCCDu, 0x3F800000u, 0x00000000u,
                                            0x00000000u, 0x00000000u, 0x3F800000u, 0x00000000u, 0xBEE66666u, 0x3DF5C28Fu};
constexpr std::array<uint32_t, 12> k_Skinned0 {0x3FA10F51u, 0x3ED50BA4u, 0xBF6E5295u, 0xBEC5F0E0u, 0x401ADF13u, 0x3D2C6F84u,
                                               0x3FA7A6DFu, 0x3EA12E3Eu, 0x3F68225Au, 0x4205AF19u, 0x3F889D04u, 0x42F1B79Au};
constexpr std::array<uint32_t, 12> k_Skinned1 {0x3F9E95E8u, 0x3F097F3Cu, 0xBF6DC8A2u, 0xBEE62724u, 0x40198A34u, 0x3DB58BFEu,
                                               0x3FA7A6DFu, 0x3EA12E3Eu, 0x3F68225Au, 0x42070231u, 0x3C842100u, 0x42F1E59Du};
// one box per column where the original's order rounds apart from glm's or from the (x, z, y) order: cx from both
// (they are one ulp lower), cy and cz from (x, z, y) (one ulp lower; glm sums cy and cz in the original's order)
constexpr std::array<std::array<uint32_t, 3>, 3> k_ColumnBoxes {{{0xBEE6662Eu, 0xBFBF5073u, 0x401C5DAFu},
                                                                 {0xBFB1386Bu, 0xBF94AD3Bu, 0x3FFF9606u},
                                                                 {0xC03FF2E1u, 0xBF84E043u, 0x3FFF861Cu}}};
constexpr std::array<std::array<uint32_t, 3>, 3> k_ColumnBoxCentres {{{0x44E16A62u, 0x41E0A559u, 0x452CB3C0u},
                                                                      {0x44E13FAFu, 0x41E29C49u, 0x452CBA25u},
                                                                      {0x44E10CB1u, 0x41E0FBA6u, 0x452CCFEAu}}};

void ExpectBits3(const glm::vec3& actual, const std::array<uint32_t, 3>& expected)
{
	for (int i = 0; i < 3; ++i)
	{
		EXPECT_EQ(std::bit_cast<uint32_t>(actual[i]), expected[static_cast<size_t>(i)]) << "component " << i;
	}
}
} // namespace

TEST(AffineMatrix, MulMatchesTheReferenceBits)
{
	ExpectBits(openblack::affine::Mul(FromBits(k_A), FromBits(k_B)), k_MulAB);
}

TEST(AffineMatrix, MulPreMatchesTheReferenceBits)
{
	ExpectBits(openblack::affine::MultiplyReversed(FromBits(k_A), FromBits(k_B)), k_MulPreAB);
}

TEST(AffineMatrix, SetInverseMatchesReference)
{
	ExpectBits(openblack::affine::Inverse(FromBits(k_A)), k_InverseA);
}

TEST(AffineMatrix, SetInverseClampsATinyDeterminant)
{
	// a zero matrix: det 0 -> +1e-10 (0 counts as positive), so every cell is 0 * 1e10 = 0, no inf / nan
	const auto inverse = openblack::affine::Inverse(AffineMatrix {{}});
	for (const float cell : inverse.m)
	{
		EXPECT_EQ(cell, 0.0f);
	}
}

TEST(AffineMatrix, WorldToCameraMatchesReference)
{
	// Land 1's flight camera (OPENBLACK_CAMERA_FLY's two points)
	ExpectBits(openblack::affine::WorldToCamera({1752.0f, 52.0f, 2650.0f}, {1786.0f, 26.0f, 2800.0f}), k_WorldToCamera);
}

TEST(AffineMatrix, WorldToCameraStraightDown)
{
	ExpectBits(openblack::affine::WorldToCamera({0.0f, 10.0f, 0.0f}, {0.0f, 0.0f, 0.0f}), k_StraightDown);
}

TEST(AffineMatrix, WorldToCameraEyeAtTarget)
{
	ExpectBits(openblack::affine::WorldToCamera({5.0f, 6.0f, 7.0f}, {5.0f, 6.0f, 7.0f}), k_EyeAtTarget);
}

TEST(AffineMatrix, WorldToClippingScalesXAndY)
{
	ExpectBits(
	    openblack::affine::WorldToClipping(FromBits(k_WorldToCamera), std::bit_cast<float>(k_Sx), std::bit_cast<float>(k_Sy)),
	    k_WorldToClipping);
}

TEST(AffineMatrix, NoFusedMultiplyAdd)
{
	// out0 = (a2 b6 + a1 b3) + a0 b0 = (0 + -1) + (1 + 2^-12)^2: two roundings give 2^-11, an FMA 2^-11 + 2^-24
	AffineMatrix a {{}};
	AffineMatrix b {{}};
	const float k = 1.0f + 1.0f / 4096.0f;
	a.m[0] = k;
	b.m[0] = k;
	a.m[1] = -1.0f;
	b.m[3] = 1.0f;
	const auto product = openblack::affine::Mul(a, b);
	EXPECT_EQ(std::bit_cast<uint32_t>(product.m[0]), 0x3A000000u); // 2^-11
}

TEST(AffineMatrix, Translation)
{
	const auto t = openblack::affine::Translation(FromBits(k_A));
	EXPECT_EQ(t, glm::vec3(12.5f, -3.25f, 7.75f));
}

TEST(AffineMatrix, LensScalesFromFov)
{
	const auto [sx, sy] = openblack::affine::LensScalesFromFov(std::bit_cast<float>(k_Fov), std::bit_cast<float>(k_Aspect));
	EXPECT_EQ(std::bit_cast<uint32_t>(sx), k_LensSx);
	EXPECT_EQ(std::bit_cast<uint32_t>(sy), k_LensSy);
}

TEST(AffineMatrix, FrameMatricesIsUpdateCamera)
{
	const auto m = openblack::affine::FrameMatrices({1752.0f, 52.0f, 2650.0f}, {1786.0f, 26.0f, 2800.0f},
	                                                std::bit_cast<float>(k_Fov), std::bit_cast<float>(k_Aspect));
	ExpectBits(m.worldToCamera, k_WorldToCamera);
	ExpectBits(m.worldToClipping, k_FrameClipping);
	ExpectBits(m.clippingToWorld, k_FrameClippingInverse);
}

TEST(AffineMatrix, ThePointThroughWInEachSitesOrder)
{
	const auto w = FromBits(k_FrameClipping);
	const glm::vec3 p(std::bit_cast<float>(k_Point[0]), std::bit_cast<float>(k_Point[1]), std::bit_cast<float>(k_Point[2]));
	ExpectBits3(openblack::affine::ToClipForScreenTest(w, p), k_PointScreenTest);
	ExpectBits3(openblack::affine::ToClipForBoxTest(w, p), k_PointBoxTest);
	ExpectBits3(openblack::affine::ToClipForShadowBlocks(w, p), k_PointShadowBlocks);
}

TEST(AffineMatrix, BoxCentreMultiplyOrder)
{
	const glm::vec3 b(std::bit_cast<float>(k_BoxCentre[0]), std::bit_cast<float>(k_BoxCentre[1]),
	                  std::bit_cast<float>(k_BoxCentre[2]));
	ExpectBits3(openblack::affine::BoxCentreThroughObject(FromBits(k_Object), b), k_BoxCentreThroughObject);
	for (size_t c = 0; c < 3; ++c)
	{
		const auto& box = k_ColumnBoxes[c];
		const glm::vec3 column(std::bit_cast<float>(box[0]), std::bit_cast<float>(box[1]), std::bit_cast<float>(box[2]));
		ExpectBits3(openblack::affine::BoxCentreThroughObject(FromBits(k_Object), column), k_ColumnBoxCentres[c]);
	}
}

TEST(AffineMatrix, RootBoneMultiplyOrder)
{
	ExpectBits(openblack::affine::BoneRootToClip(FromBits(k_ObjectAt), FromBits(k_FrameClipping)), k_ObjectThroughW);
}

TEST(AffineMatrix, SkinBonesPutsEachBoneUnderItsParent)
{
	std::vector<openblack::affine::AffineMatrix> skinned;
	openblack::affine::SkinBones({FromBits(k_Bone0), FromBits(k_Bone1)}, {UINT32_MAX, 0u}, FromBits(k_ObjectThroughW), skinned);
	ASSERT_EQ(skinned.size(), 2u);
	ExpectBits(skinned[0], k_Skinned0);
	ExpectBits(skinned[1], k_Skinned1);
}

TEST(AffineMatrix, FromModelCopiesTheCells)
{
	glm::mat4 model(1.0f);
	for (int c = 0; c < 4; ++c)
	{
		for (int r = 0; r < 3; ++r)
		{
			model[c][r] = static_cast<float>(3 * c + r + 1);
		}
	}
	const auto m = openblack::affine::FromModel(model);
	for (size_t i = 0; i < 12; ++i)
	{
		EXPECT_EQ(m.m[i], static_cast<float>(i + 1)) << "cell " << i;
	}
}
