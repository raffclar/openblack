/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The condition of an atom that is true while a villager is in the map cell of the atom

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <ParticleFile.h>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/GameRandom.h"
#include "Particles/ParticleClassRegistry.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleMaths.h"

using namespace openblack;
using namespace openblack::particles;

namespace
{
class FakeWorld final: public ParticleWorldInterface
{
public:
	[[nodiscard]] float LandHeight(glm::vec2 /*xz*/) const override { return 0.0f; }
	[[nodiscard]] uint32_t PlayerColour(int /*player*/) const override { return 0xFF000000u; }
	[[nodiscard]] glm::vec3 CameraRight() const override { return {1.0f, 0.0f, 0.0f}; }
	[[nodiscard]] glm::vec3 CameraUp() const override { return {0.0f, 1.0f, 0.0f}; }
	[[nodiscard]] std::optional<TargetInfo> Target(entt::entity /*target*/, bool /*centre*/) const override
	{
		return std::nullopt;
	}
	[[nodiscard]] bool VillagerInCell(glm::vec3 point) const override
	{
		asked.push_back(point);
		const auto cell = map_coords::CellOf(point);
		return map_coords::InBounds(cell) && cell == villagerCell;
	}

	glm::ivec2 villagerCell {12, 34};
	mutable std::vector<glm::vec3> asked;
};

class FakeRandom final: public GameRandomInterface
{
public:
	uint32_t GameRand(uint32_t /*n*/) override { return 0; }
	float GameFloatRand(float /*x*/) override { return 0.0f; }
	uint32_t LocalRand(int32_t /*n*/) override { return 0; }
	float LocalFloatRand(float /*x*/) override { return 0.0f; }
	int32_t CrtRand() override { return 0; }
	void CrtSrand(uint32_t /*seed*/) override {}
	[[nodiscard]] GameRandomSeeds GetSeeds() const override { return {}; }
	void SetSeeds(GameRandomSeeds /*seeds*/) override {}
	[[nodiscard]] ParticleRandomStream GetParticleStream() const override { return ParticleRandomStream::None; }
	void SetParticleStream(ParticleRandomStream /*stream*/) override {}
};

constexpr std::string_view k_File = "BEGINPROPERTIES\nPROPERTY DeleteOnCloseDown BOOL 1\nPROPERTY Hierarchies ARRAY SIZE 1 0\n"
                                    "PROPERTY InitiallyCreated ARRAY SIZE 1 0\nPROPERTY MaxSpellAge FLOAT -1\nENDPROPERTIES\n"
                                    "BEGINCLASS EventConditionAtomNearVillagers EventConditionAtomNearVillagers0\n"
                                    "BEGINPROPERTIES\nENDPROPERTIES\nENDCLASS\n";
} // namespace

TEST(ParticleNearVillagers, TheConditionIsTrueOnlyInTheMapCellOfAVillager)
{
	auto file = psys::ParticleFile::Parse(k_File);
	ASSERT_TRUE(file.has_value());
	const auto shared = std::make_shared<const psys::ParticleFile>(std::move(*file));
	const auto* object = shared->Find("EventConditionAtomNearVillagers0");
	ASSERT_NE(object, nullptr);
	ParticleClassRegistry classes {ParticleClassRegistry::WithAllClasses()};
	FakeWorld world;
	FakeRandom random;
	maths::ValueNoise noise;
	const Effect effect(shared, EffectServices {classes, world, random, noise}, glm::vec3(0.0f), 1.0f, false);
	const auto* condition = classes.FindAtomCondition("EventConditionAtomNearVillagers");
	ASSERT_NE(condition, nullptr);
	Atom atom;
	// A cell is ten metres across
	atom.position = {125.0f, 3.0f, 349.0f};
	EXPECT_TRUE((*condition)(effect, *object, atom));
	atom.position = {131.0f, 3.0f, 349.0f};
	EXPECT_FALSE((*condition)(effect, *object, atom));
	ASSERT_EQ(world.asked.size(), 2u);
	EXPECT_EQ(world.asked[0], glm::vec3(125.0f, 3.0f, 349.0f));
}
