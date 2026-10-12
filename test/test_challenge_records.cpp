/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <vector>

#include <ECS/ChallengeRecords.h>
#include <ECS/Components/ChallengeRecord.h>
#include <ECS/Registry.h>
#include <gtest/gtest.h>

namespace
{
namespace records = openblack::ecs::challenge_records;
namespace snapshots = openblack::script::challenge_snapshots;
using openblack::ecs::components::ChallengeRecord;

constexpr uint32_t k_LostBrother = 19;
constexpr uint32_t k_Title = 2433;

snapshots::Snapshot LostBrotherSnapshot(float success, float alignment, uint32_t reminderText)
{
	snapshots::Snapshot snapshot;
	snapshot.challenge = k_LostBrother;
	snapshot.title = k_Title;
	snapshot.reminderScript = "StandardReminder";
	snapshots::Value text;
	text.value.uintVal = reminderText;
	text.type = openblack::lhvm::DataType::Int;
	snapshot.arguments = {text};
	snapshot.success = success;
	snapshot.alignment = alignment;
	snapshot.position = glm::vec3 {1.0f, 2.0f, 3.0f};
	snapshot.focus = glm::vec3 {4.0f, 5.0f, 6.0f};
	snapshot.quest = true;
	return snapshot;
}
} // namespace

TEST(ChallengeRecords, TheFirstSnapshotStartsTheRecordWithoutMovingTheAlignment)
{
	openblack::ecs::Registry registry;
	const auto outcome = records::Record(registry, LostBrotherSnapshot(0.0f, 0.0f, 4255), snapshots::Call::Start, 100, {});
	EXPECT_FALSE(outcome.alignment.has_value());
	ASSERT_TRUE(records::Find(registry, k_LostBrother).has_value());
	const auto& record = registry.Get<ChallengeRecord>(outcome.record);
	EXPECT_EQ(record.title, k_Title);
	EXPECT_EQ(record.reminderScript, "StandardReminder");
	ASSERT_EQ(record.arguments.size(), 1u);
	EXPECT_EQ(record.arguments[0].value.uintVal, 4255u);
	EXPECT_TRUE(record.quest);
	EXPECT_EQ(record.startedTurn, 100u);
	EXPECT_FALSE(record.doneTurn.has_value());
}

TEST(ChallengeRecords, LaterSnapshotsUpdateItAndMoveTheAlignment)
{
	openblack::ecs::Registry registry;
	records::Record(registry, LostBrotherSnapshot(0.0f, 0.0f, 4255), snapshots::Call::Start, 100, {});
	auto update = LostBrotherSnapshot(0.5f, -0.4f, 1297);
	update.position.reset();
	update.focus.reset();
	const auto outcome = records::Record(registry, update, snapshots::Call::Update, 200, {});
	ASSERT_TRUE(outcome.alignment.has_value());
	EXPECT_FLOAT_EQ(*outcome.alignment, -0.4f);
	const auto& record = registry.Get<ChallengeRecord>(outcome.record);
	EXPECT_FLOAT_EQ(record.success, 0.5f);
	EXPECT_EQ(record.arguments[0].value.uintVal, 1297u);
	// An update keeps where the camera was when the challenge started
	ASSERT_TRUE(record.position.has_value());
	EXPECT_EQ(*record.position, glm::vec3(1.0f, 2.0f, 3.0f));
}

TEST(ChallengeRecords, OnceDoneItStaysDone)
{
	openblack::ecs::Registry registry;
	records::Record(registry, LostBrotherSnapshot(0.0f, 0.0f, 4255), snapshots::Call::Start, 100, {});
	records::Record(registry, LostBrotherSnapshot(1.0f, 0.9f, 4255), snapshots::Call::Start, 300, {});
	const auto outcome = records::Record(registry, LostBrotherSnapshot(0.5f, -0.3f, 4255), snapshots::Call::Update, 400, {});
	const auto& record = registry.Get<ChallengeRecord>(outcome.record);
	EXPECT_FLOAT_EQ(record.success, 1.0f);
	EXPECT_EQ(record.doneTurn, 400u);
}

TEST(ChallengeRecords, AnUpdateOfAChallengeNotYetStartedStartsItsRecord)
{
	openblack::ecs::Registry registry;
	const auto outcome = records::Record(registry, LostBrotherSnapshot(0.7f, 0.5f, 4255), snapshots::Call::Update, 100, {});
	EXPECT_FALSE(outcome.alignment.has_value());
	const auto& record = registry.Get<ChallengeRecord>(outcome.record);
	EXPECT_FALSE(record.quest);
	EXPECT_FALSE(record.position.has_value());
}

TEST(ChallengeRecords, ThingsTheRecordHoldsAreKeptAliveUntilReplaced)
{
	openblack::ecs::Registry registry;
	std::vector<entt::entity> added;
	std::vector<entt::entity> removed;
	const records::References references {.add = [&added](entt::entity e) { added.push_back(e); },
	                                      .remove = [&removed](entt::entity e) { removed.push_back(e); }};
	auto snapshot = LostBrotherSnapshot(0.0f, 0.0f, 4255);
	snapshots::Value thing;
	thing.value.uintVal = 42;
	thing.type = openblack::lhvm::DataType::Object;
	snapshot.arguments.push_back(thing);
	records::Record(registry, snapshot, snapshots::Call::Start, 1, references);
	ASSERT_EQ(added.size(), 1u);
	EXPECT_EQ(added[0], static_cast<entt::entity>(42));
	records::Record(registry, LostBrotherSnapshot(0.5f, 0.0f, 4255), snapshots::Call::Update, 2, references);
	ASSERT_EQ(removed.size(), 1u);
	EXPECT_EQ(removed[0], static_cast<entt::entity>(42));
}

TEST(ChallengeRecords, TheAlignmentIsScaledThenDampedByTheWayThePlayerLeans)
{
	// A good player doing good is moved less, a good player doing evil more, by half their alignment
	EXPECT_FLOAT_EQ(records::AlignmentChange(0.0f, 0.9f, 0.1f), 0.09f);
	EXPECT_FLOAT_EQ(records::AlignmentChange(0.5f, 0.9f, 0.1f), 0.09f * 0.75f);
	EXPECT_FLOAT_EQ(records::AlignmentChange(0.5f, -0.4f, 0.1f), -0.04f * 1.25f);
	EXPECT_FLOAT_EQ(records::AlignmentChange(-0.5f, -0.4f, 0.1f), -0.04f * 0.75f);
}

TEST(ChallengeRecords, ANewRecordAlwaysTakesItsPicture)
{
	openblack::ecs::Registry registry;
	// Even a plain update that makes the record takes the picture
	const auto outcome = records::Record(registry, LostBrotherSnapshot(0.0f, 0.0f, 4255), snapshots::Call::Update, 100, {});
	EXPECT_TRUE(outcome.takesPicture);
}

TEST(ChallengeRecords, LaterSnapshotsTakeThePictureOnlyWithTheCamera)
{
	openblack::ecs::Registry registry;
	records::Record(registry, LostBrotherSnapshot(0.0f, 0.0f, 4255), snapshots::Call::Start, 100, {});
	EXPECT_FALSE(records::Record(registry, LostBrotherSnapshot(0.2f, 0.0f, 1), snapshots::Call::Update, 110, {}).takesPicture);
	EXPECT_TRUE(records::Record(registry, LostBrotherSnapshot(0.3f, 0.0f, 1), snapshots::Call::Start, 120, {}).takesPicture);
	auto picture = LostBrotherSnapshot(0.4f, 0.0f, 1);
	picture.takingPicture = false;
	EXPECT_FALSE(records::Record(registry, picture, snapshots::Call::Picture, 130, {}).takesPicture);
	picture.takingPicture = true;
	EXPECT_TRUE(records::Record(registry, picture, snapshots::Call::Picture, 140, {}).takesPicture);
}

TEST(ChallengeRecords, TheReminderIsGivenItsArgumentsInTheOrderTheScriptGaveThem)
{
	ChallengeRecord record;
	snapshots::Value first;
	first.value.uintVal = 1;
	snapshots::Value second;
	second.value.uintVal = 2;
	// Kept the last given first
	record.arguments = {second, first};
	const auto arguments = records::ReminderArguments(record);
	ASSERT_EQ(arguments.size(), 2u);
	EXPECT_EQ(arguments[0].value.uintVal, 1u);
	EXPECT_EQ(arguments[1].value.uintVal, 2u);
}

TEST(ChallengeRecords, TheScrollListsThemInTheOrderTheyStarted)
{
	openblack::ecs::Registry registry;
	auto second = LostBrotherSnapshot(1.0f, 0.0f, 0);
	second.challenge = 7;
	second.title = 1200;
	auto third = LostBrotherSnapshot(0.9999f, 0.0f, 0);
	third.challenge = 8;
	third.title = 1300;
	records::Record(registry, LostBrotherSnapshot(0.2f, 0.0f, 0), snapshots::Call::Start, 1, {});
	records::Record(registry, second, snapshots::Call::Start, 2, {});
	records::Record(registry, third, snapshots::Call::Start, 3, {});
	// Updating the first keeps it first
	records::Record(registry, LostBrotherSnapshot(0.5f, 0.0f, 0), snapshots::Call::Update, 4, {});

	const auto listing = records::List(registry);
	EXPECT_EQ(listing.discovered, 3);
	// Only a success of exactly one is done
	EXPECT_EQ(listing.completed, 1);
	EXPECT_EQ(listing.titles, (std::vector<uint32_t> {k_Title, 1200, 1300}));
	const auto ordered = records::InOrder(registry);
	ASSERT_EQ(ordered.size(), 3u);
	EXPECT_EQ(registry.Get<ChallengeRecord>(ordered[2]).challenge, 8u);
}
