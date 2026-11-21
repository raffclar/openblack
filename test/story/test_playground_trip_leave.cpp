/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include <SaveSlots.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#define LOCATOR_IMPLEMENTATIONS 1
#include "ECS/Systems/CameraHelpSystemInterface.h"
#include "ECS/Systems/Implementations/PlaygroundTripSystem.h"
#include "ECS/Systems/SaveGameSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{

/// The help of the camera, which keeps the interface level that the trip gives
class FakeCameraHelp final: public CameraHelpSystemInterface
{
public:
	[[nodiscard]] const camera_help::CameraHelp& Get() const override { return _help; }
	[[nodiscard]] camera_help::CameraHelp& Get() override { return _help; }

private:
	camera_help::CameraHelp _help;
};

/// A save system that keeps a record of the loads that the trip does at once and of the loads that it asks for
class RecordingSaves final: public SaveGameSystemInterface
{
public:
	std::vector<uint32_t> loadsNow;
	std::optional<uint32_t> asked;

	bool Save(uint32_t /*slot*/, std::u16string_view /*name*/) override { return false; }
	bool Load(uint32_t slot) override
	{
		loadsNow.push_back(slot);
		return true;
	}
	[[nodiscard]] std::vector<Slot> Slots() const override { return {}; }
	bool Delete(uint32_t /*slot*/) override { return false; }
	[[nodiscard]] std::optional<std::vector<uint8_t>> ReadPictureFile(std::string_view /*name*/) const override
	{
		return std::nullopt;
	}
	[[nodiscard]] temple::save_game_room::SavedGamesStamp GetSavedGamesStamp() const override { return {}; }
	[[nodiscard]] std::vector<VanillaCheck> CheckVanillaRecords() const override { return {}; }
	void TakeTemplePicture() override {}
	bool InstantSave(uint32_t /*slot*/) override { return false; }
	bool SaveOnExit(bool /*quitting*/) override { return false; }
	[[nodiscard]] bool HasExitSave() const override { return true; }
	void EndTurn() override {}
	bool PressQuickSave() override { return false; }
	bool PressQuickLoad() override { return false; }
	bool DoQuickSave() override { return false; }
	void DoQuickLoad() override {}
	void AskLoad(uint32_t slot) override { asked = slot; }
	[[nodiscard]] bool LoadAsked() const override { return asked.has_value(); }
	bool LoadAskedSlot() override { return false; }
	void NewLand() override {}
	[[nodiscard]] bool AnyPictureWaiting() const override { return false; }
	void CollectPictures(const Screens& /*screens*/) override {}
	void PutChallengePicture(uint32_t /*place*/, std::span<const uint8_t> /*picture*/) override {}
	[[nodiscard]] gamesave::ChallengePictureCells ChallengePictureCells(uint32_t /*place*/) override { return {}; }
};

class PlaygroundTripLeave: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_st>("game");
		}
		Locator::playerSystem::reset();
		Locator::cameraHelpSystem::emplace<FakeCameraHelp>();
		_saves = static_cast<RecordingSaves*>(&Locator::saveGameSystem::emplace<RecordingSaves>());
	}

	void TearDown() override
	{
		Locator::saveGameSystem::reset();
		Locator::cameraHelpSystem::reset();
	}

	RecordingSaves* _saves {nullptr};
};

} // namespace

TEST_F(PlaygroundTripLeave, TheYesAsksForTheLoadOfTheSaveOnExit)
{
	PlaygroundTripSystem trip;
	trip.Leave();
	ASSERT_TRUE(_saves->asked.has_value());
	EXPECT_EQ(*_saves->asked, gamesave::k_ExitSlot);
}

TEST_F(PlaygroundTripLeave, TheYesLoadsNothingBeforeTheEndOfTheFrame)
{
	PlaygroundTripSystem trip;
	trip.Leave();
	EXPECT_TRUE(_saves->loadsNow.empty());
}

TEST_F(PlaygroundTripLeave, TheInfluenceIsUsualAndNoLessonRunsAfterTheYes)
{
	PlaygroundTripSystem trip;
	trip.StartLesson();
	trip.Leave();
	EXPECT_FALSE(trip.InfluenceEverywhere());
	EXPECT_FALSE(trip.IsLessonRunning());
}
