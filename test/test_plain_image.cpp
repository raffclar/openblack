/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <vector>

#include <EDTFile.h>
#include <LHVMTypes.h>
#include <entt/meta/context.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/CameraTrack.h"
#include "3D/DayNightClock.h"
#include "3D/SkyDome.h"
#include "Animals/Zoomer.h"
#include "Common/Zoomer.h"
#include "Creature/CreatureFight.h"
#include "Creature/FightMarks.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Reflection/PlainImage.h"
#include "ECS/ScriptObjectTable.h"
#include "Hand/HandGrabRules.h"
#include "Magic/CastInput.h"
#include "Magic/HandMotion.h"
#include "Particles/LightningMaths.h"
#include "Particles/ManaPathMaths.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleMaths.h"
#include "Particles/ParticleObjectEffects.h"
#include "Particles/ParticleSoundRelease.h"
#include "Particles/ParticleSpellLink.h"
#include "Particles/PlasmaCommand.h"
#include "Particles/RisingSparks.h"
#include "Particles/SurfaceOfRevolution.h"

using namespace openblack;
using namespace openblack::ecs::reflection;

namespace
{

/// The memory for a value, with each byte set to a pattern before the value is made in it
template <typename Type>
struct alignas(Type) Filled
{
	std::array<std::byte, sizeof(Type)> bytes;

	explicit Filled(std::byte pattern) { bytes.fill(pattern); }
	/// Makes the value with its default fields: the gaps keep the pattern
	Type* Make() { return ::new (static_cast<void*>(bytes.data())) Type; }
};

/// The bytes of a value that its fields cover
template <typename Type>
std::vector<bool> CoveredBytes(const Type& value)
{
	std::vector<bool> covered(sizeof(Type), false);
	Type::VisitPlainFields(value, [&covered, &value](const auto&... fields) {
		const auto cover = [&covered, &value](const auto& field) {
			const auto offset =
			    static_cast<size_t>(reinterpret_cast<const std::byte*>(&field) - reinterpret_cast<const std::byte*>(&value));
			std::fill_n(covered.begin() + static_cast<std::ptrdiff_t>(offset), sizeof(field), true);
		};
		(cover(fields), ...);
	});
	return covered;
}

/// Checks a plain type with gaps: two copies with other bytes in the gaps give the same bytes for a save, and the fields
/// that the type gives cover every byte that is not a gap
template <typename Type>
void ExpectZeroGaps()
{
	Filled<Type> low(std::byte {0x00});
	Filled<Type> high(std::byte {0xFF});
	const auto* lowValue = low.Make();
	const auto* highValue = high.Make();

	const auto lowImage = PlainImage(*lowValue);
	const auto highImage = PlainImage(*highValue);
	EXPECT_EQ(lowImage.bytes, highImage.bytes);

	// A byte that differs between the two copies is a gap; each other byte is in a field
	const auto covered = CoveredBytes(*lowValue);
	for (size_t i = 0; i < sizeof(Type); ++i)
	{
		const bool gap = low.bytes.at(i) != high.bytes.at(i);
		EXPECT_TRUE(gap || covered.at(i)) << "byte " << i << " is in no field that the type gives";
		if (gap)
		{
			EXPECT_EQ(lowImage.bytes.at(i), std::byte {0}) << "byte " << i;
		}
	}
}

/// Checks a plain type with no gaps: the fields that the type gives cover every byte of it
template <typename Type>
void ExpectNoGaps()
{
	const Type value {};
	const auto covered = CoveredBytes(value);
	EXPECT_TRUE(std::ranges::all_of(covered, [](bool byte) { return byte; }));
}

} // namespace

TEST(PlainImage, TheGapsOfEachPlainTypeAreZero)
{
	ExpectZeroGaps<particles::ProcessInfo>();
	ExpectZeroGaps<particles::PlasmaCommand>();
	ExpectZeroGaps<particles::maths::EmitterClock>();
	ExpectZeroGaps<particles::SoundLetGo>();
	ExpectZeroGaps<magic::CastInput>();
	ExpectZeroGaps<magic::PourSettings>();
	ExpectZeroGaps<magic::PourPose>();
	ExpectZeroGaps<magic::PourState>();
	ExpectZeroGaps<creature_fight::Move>();
	ExpectZeroGaps<creature_fight::QueuedMove>();
	ExpectZeroGaps<creature_fight::MoveQueue>();
	ExpectZeroGaps<creature_fight::marks::Mark>();
	ExpectZeroGaps<creature_fight::marks::MarkList>();
	ExpectZeroGaps<hand_grab::HandSpring>();
	ExpectZeroGaps<ecs::script_objects::Place>();
	ExpectZeroGaps<ecs::script_objects::Table>();
}

TEST(PlainImage, EachPlainTypeWithNoGapsGivesEveryByte)
{
	ExpectNoGaps<DayNightClock>();
	ExpectNoGaps<Zoomer>();
	ExpectNoGaps<animals::Zoomer>();
	ExpectNoGaps<camera_track::WayRunner>();
	ExpectNoGaps<lhvm::VMValue>();
	ExpectNoGaps<magic::HandSpin>();
	ExpectNoGaps<particles::BeliefSprite>();
	ExpectNoGaps<particles::ExplodeRecord>();
	ExpectNoGaps<particles::SurfacePoint>();
	ExpectNoGaps<particles::WaveTarget>();
	ExpectNoGaps<particles::DrawState>();
	ExpectNoGaps<particles::ObjectGlow>();
	ExpectNoGaps<particles::surface::Vertex>();
	ExpectNoGaps<particles::mana_path::Spark>();
	ExpectNoGaps<particles::mana_path::Path>();
	ExpectNoGaps<particles::maths::ArcEnds>();
	ExpectNoGaps<particles::rising_sparks::Spark>();
	ExpectNoGaps<particles::rising_sparks::Sparks>();
	ExpectNoGaps<particles::rising_sparks::FrameMilliseconds>();
}

TEST(PlainImage, ATableMadeOnOtherBytesSavesTheSameBytes)
{
	// The memory of each table has other bytes before the table is made in it, as a table on the stack or the heap can
	Filled<ecs::script_objects::Table> low(std::byte {0x00});
	Filled<ecs::script_objects::Table> high(std::byte {0xC9});
	auto* lowTable = low.Make();
	auto* highTable = high.Make();
	for (auto* table : {lowTable, highTable})
	{
		table->Register(42, true);
		table->Register(7, false);
		table->AddReference(2);
	}
	const entt::meta_ctx context;
	BinaryWriter lowOut(context);
	BinaryWriter highOut(context);
	lowOut.WriteRaw(*lowTable);
	highOut.WriteRaw(*highTable);
	EXPECT_EQ(lowOut.Bytes(), highOut.Bytes());
	EXPECT_NE(low.bytes, high.bytes);
}

TEST(PlainImage, AnEmptyOptionIsZero)
{
	Filled<std::optional<float>> low(std::byte {0x00});
	Filled<std::optional<float>> high(std::byte {0xA5});
	EXPECT_EQ(PlainImage(*low.Make()).bytes, PlainImage(*high.Make()).bytes);
	const std::optional<float> held = 2.5f;
	auto image = PlainImage(held);
	std::optional<float> back;
	std::memcpy(static_cast<void*>(&back), image.bytes.data(), sizeof(back));
	ASSERT_TRUE(back.has_value());
	EXPECT_EQ(*back, 2.5f);
}

TEST(PlainImage, AValueComesBackWithEveryField)
{
	magic::PourState pour {};
	pour.active = true;
	pour.settings.loops = false;
	pour.progress = 0.25f;
	pour.previous.pinned = glm::vec3(1.0f, 2.0f, 3.0f);
	pour.current.tilt = 0.5f;
	const entt::meta_ctx context;
	BinaryWriter out(context);
	out.WriteRaw(pour);
	ASSERT_EQ(out.Bytes().size(), sizeof(pour));
	magic::PourState back {};
	std::memcpy(static_cast<void*>(&back), out.Bytes().data(), sizeof(back));
	EXPECT_TRUE(back.active);
	EXPECT_FALSE(back.settings.loops);
	EXPECT_TRUE(back.settings.clampHand);
	EXPECT_EQ(back.progress, 0.25f);
	ASSERT_TRUE(back.previous.pinned.has_value());
	EXPECT_EQ(*back.previous.pinned, glm::vec3(1.0f, 2.0f, 3.0f));
	EXPECT_EQ(back.current.tilt, 0.5f);
	EXPECT_FALSE(back.pinned.has_value());
}
