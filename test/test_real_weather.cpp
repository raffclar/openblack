/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstring>

#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/RealWeatherRules.h"
#include "ECS/Systems/LobbyClientSystemInterface.h"
#include "ECS/Systems/RealWeatherSystemInterface.h"

// Enable this define because the test makes the system itself
#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/RealWeatherSystem.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs;
using openblack::ecs::components::WeatherInfo;

namespace
{

/// The bytes of one preset, as the file holds them
class PresetBytes
{
public:
	explicit PresetBytes(uint16_t type, bool storm = false)
	{
		std::memcpy(bytes.data(), &type, sizeof(type));
		bytes[2] = storm ? 1 : 0;
	}
	/// A change at a place: the operation in its byte, the amount in the 4 bytes after the next 4
	PresetBytes& Change(size_t offset, uint8_t operation, float amount)
	{
		bytes.at(offset) = operation;
		const auto amountAt = offset == 0x03 ? size_t {0x04} : offset + 4;
		std::memcpy(bytes.data() + amountAt, &amount, sizeof(amount));
		return *this;
	}
	std::array<uint8_t, real_weather::k_PresetBytes> bytes {};
};

std::vector<uint8_t> FileOf(std::initializer_list<PresetBytes> presets)
{
	std::vector<uint8_t> file;
	for (const auto& preset : presets)
	{
		file.insert(file.end(), preset.bytes.begin(), preset.bytes.end());
	}
	return file;
}

/// The places of the changes in a preset
constexpr size_t k_Rain = 0x03;
constexpr size_t k_SnowCover = 0x08;
constexpr size_t k_Snowfall = 0x10;
constexpr size_t k_WindX = 0x18;
constexpr size_t k_WindZ = 0x20;
constexpr size_t k_RainSpeed = 0x28;
constexpr size_t k_ThunderLeast = 0x30;
constexpr size_t k_BoltMost = 0x48;
constexpr size_t k_Overcast = 0x50;
constexpr size_t k_CloudShade = 0x58;
constexpr size_t k_CloudCount = 0x60;
constexpr size_t k_CloudHeight = 0x68;

class FakeLand final: public RealWeatherLandInterface
{
public:
	[[nodiscard]] WeatherInfo GetBackground() const override { return background; }
	void SetBackground(const WeatherInfo& weather) override { background = weather; }
	entt::entity AddStorm(const script_weather::LandStorm& made) override
	{
		storm = made;
		living = true;
		return entt::entity {42};
	}
	[[nodiscard]] bool StormLives(entt::entity /*storm*/) const override { return living; }
	void EndStorm(entt::entity /*storm*/) override
	{
		living = false;
		++ends;
	}
	[[nodiscard]] glm::vec3 IslandCentre() const override { return {2560.0f, 0.0f, 1280.0f}; }
	[[nodiscard]] bool IsMultiplayer() const override { return multiplayer; }

	WeatherInfo background {};
	std::optional<script_weather::LandStorm> storm;
	bool living {false};
	int ends {0};
	bool multiplayer {false};
};

class FakeLobby final: public LobbyClientSystemInterface
{
public:
	void Update() override {}
	[[nodiscard]] bool IsConfigured() const override { return configured; }
	[[nodiscard]] LobbySessionState GetState() const override { return state; }
	[[nodiscard]] std::optional<lobby_api::AccountRef> GetAccount() const override { return std::nullopt; }
	[[nodiscard]] bool IsPushOpen() const override { return false; }
	void Login(std::string, std::string, LobbyCallback<lobby_api::AccountRef>) override {}
	void ResumeSession(LobbyCallback<lobby_api::AccountRef> callback) override { resume = std::move(callback); }
	void Logout(bool, LobbyCallback<LobbyDone>) override {}
	void GetMe(LobbyCallback<lobby_api::Me>) override {}
	void GetServers(LobbyCallback<lobby_api::ServerList>) override {}
	[[nodiscard]] std::span<const lobby_api::ServerEntry> GetKnownServers() const override { return {}; }
	void WatchServers(bool) override {}
	void WatchChannel(std::string, bool) override {}
	void RequestJoinTicket(std::string, LobbyCallback<lobby_api::JoinTicket>) override {}
	void SendChatLine(std::string, std::string, LobbyCallback<LobbyDone>) override {}
	void UseClan(std::string, std::optional<int64_t>, LobbyCallback<lobby_api::UsedClan>) override {}
	void CheckAccountName(std::string, LobbyCallback<lobby_api::AccountNameCheck>) override {}
	void GetMyClans(LobbyCallback<lobby_api::ClanList>) override {}
	void GetClan(int64_t, LobbyCallback<lobby_api::ClanDetails>) override {}
	void GetClanLogo(int64_t, LobbyCallback<std::vector<uint8_t>>) override {}
	void GetPlayerRanking(int32_t, LobbyCallback<lobby_api::RankingPage>) override {}
	void GetClanRanking(int32_t, LobbyCallback<lobby_api::RankingPage>) override {}
	void GetPlayer(int64_t, LobbyCallback<lobby_api::PlayerDetails>) override {}
	void GetWeather(lobby_api::Date date, LobbyCallback<lobby_api::RealWeather> callback) override
	{
		askedFor = date;
		weather = std::move(callback);
	}
	void UploadCreature(lobby_api::CreatureUpload, LobbyCallback<lobby_api::CreatureUploadReply>) override {}
	uint32_t AddEventListener(LobbyEventListener) override { return 0; }
	void RemoveEventListener(uint32_t) override {}

	void Answer(lobby_api::RealWeather value)
	{
		ASSERT_TRUE(weather);
		auto callback = std::move(weather);
		weather = nullptr;
		callback(LobbyResult<lobby_api::RealWeather> {.value = std::move(value), .error = {}});
	}

	bool configured {true};
	LobbySessionState state {LobbySessionState::LoggedIn};
	LobbyCallback<lobby_api::AccountRef> resume;
	LobbyCallback<lobby_api::RealWeather> weather;
	std::optional<lobby_api::Date> askedFor;
};

constexpr lobby_api::Date k_Today {.year = 2026, .month = 10, .day = 22};

lobby_api::RealWeather Weather(int32_t type, int32_t temperature = 7, int32_t visibility = 60)
{
	return {.temperature = temperature,
	        .visibility = visibility,
	        .type = type,
	        .city = "Guildford",
	        .area = "Surrey",
	        .country = "gb"};
}

/// A system with a file of presets: type 2 with a storm, type 4 that sets the overcast and the rain
RealWeatherSystem MakeSystem(FakeLobby& lobby, bool turnedOn = true)
{
	auto file = FileOf({
	    PresetBytes(2, true).Change(k_Rain, 0, 40.0f).Change(k_CloudCount, 0, 2.0f).Change(k_CloudHeight, 0, 200.0f),
	    PresetBytes(4).Change(k_Overcast, 0, 30.0f).Change(k_Rain, 0, 10.0f),
	});
	return RealWeatherSystem(turnedOn, lobby, [file]() { return std::optional(file); }, []() { return k_Today; });
}

} // namespace

TEST(RealWeatherRules, PresetsReadFromTheBytesOfTheFile)
{
	const auto file = FileOf({
	    PresetBytes(11, true)
	        .Change(k_Rain, 2, 5.0f)
	        .Change(k_SnowCover, 0, 100.0f)
	        .Change(k_Snowfall, 0, 90.0f)
	        .Change(k_WindX, 1, 1.0f)
	        .Change(k_WindZ, 0, -1.0f)
	        .Change(k_RainSpeed, 0, 6.0f)
	        .Change(k_ThunderLeast, 0, 10.0f)
	        .Change(k_BoltMost, 0, 25.0f)
	        .Change(k_Overcast, 1, 3.0f)
	        .Change(k_CloudShade, 0, 0.25f)
	        .Change(k_CloudCount, 0, 6.0f)
	        .Change(k_CloudHeight, 0, 140.0f),
	});
	auto withTail = file;
	withTail.push_back(0xFF);
	const auto presets = real_weather::ParsePresets(withTail);
	ASSERT_EQ(presets.size(), 1u);
	const auto& preset = presets[0];
	EXPECT_EQ(preset.type, 11);
	EXPECT_TRUE(preset.makesStorm);
	EXPECT_EQ(preset.rain.operation, 2);
	EXPECT_EQ(preset.rain.amount, 5.0f);
	EXPECT_EQ(preset.snowCover.amount, 100.0f);
	EXPECT_EQ(preset.snowfall.amount, 90.0f);
	EXPECT_EQ(preset.windX.operation, 1);
	EXPECT_EQ(preset.windZ.amount, -1.0f);
	EXPECT_EQ(preset.rainSpeed.amount, 6.0f);
	EXPECT_EQ(preset.thunderLeast.amount, 10.0f);
	EXPECT_EQ(preset.boltMost.amount, 25.0f);
	EXPECT_EQ(preset.overcast.operation, 1);
	EXPECT_EQ(preset.cloudShade.amount, 0.25f);
	EXPECT_EQ(preset.cloudCount.amount, 6.0f);
	EXPECT_EQ(preset.cloudHeight.amount, 140.0f);
}

TEST(RealWeatherRules, TheLastPresetOfATypeIsTheOneUsed)
{
	const auto presets = real_weather::ParsePresets(FileOf({
	    PresetBytes(3).Change(k_Rain, 0, 1.0f),
	    PresetBytes(3).Change(k_Rain, 0, 2.0f),
	    PresetBytes(5),
	}));
	const auto* found = real_weather::FindPreset(presets, 3);
	ASSERT_NE(found, nullptr);
	EXPECT_EQ(found->rain.amount, 2.0f);
	EXPECT_EQ(real_weather::FindPreset(presets, 7), nullptr);
	EXPECT_EQ(real_weather::FindPreset(presets, real_weather::k_NoType), nullptr);
}

TEST(RealWeatherRules, ChangesSetAddAndSubtract)
{
	EXPECT_EQ((real_weather::Change {.operation = 0, .amount = 4.0f}).Apply(10.0f), 4.0f);
	EXPECT_EQ((real_weather::Change {.operation = 1, .amount = 4.0f}).Apply(10.0f), 14.0f);
	EXPECT_EQ((real_weather::Change {.operation = 2, .amount = 4.0f}).Apply(10.0f), 6.0f);
	EXPECT_EQ((real_weather::Change {.operation = 7, .amount = 4.0f}).Apply(10.0f), 4.0f);
}

TEST(RealWeatherRules, TheTemperatureIsRealAndTheVisibilityGivesWayToThePreset)
{
	const auto presets = real_weather::ParsePresets(FileOf({PresetBytes(4).Change(k_Overcast, 0, 30.0f)}));
	const auto applied = real_weather::ApplyPreset(&presets[0], WeatherInfo {.temperature = 1, .overcast = 9}, -4, 60,
	                                               real_weather::PresetStorm({}));
	EXPECT_EQ(applied.background.temperature, -4);
	EXPECT_EQ(applied.background.overcast, 30);
	EXPECT_FALSE(applied.storm.has_value());

	// With no preset only the temperature and the visibility change
	const auto none = real_weather::ApplyPreset(nullptr, WeatherInfo {.rain = 5}, 12, 60, real_weather::PresetStorm({}));
	EXPECT_EQ(none.background.temperature, 12);
	EXPECT_EQ(none.background.overcast, 60);
	EXPECT_EQ(none.background.rain, 5);
}

TEST(RealWeatherRules, EachChangeStartsFromTheResultOfTheChangeBeforeIt)
{
	// The game keeps one number for the changes: an addition to the snow cover adds to the overcast that came before
	const auto presets = real_weather::ParsePresets(FileOf({
	    PresetBytes(6).Change(k_Overcast, 0, 20.0f).Change(k_SnowCover, 1, 5.0f).Change(k_Snowfall, 2, 15.0f),
	}));
	const auto applied =
	    real_weather::ApplyPreset(&presets[0], WeatherInfo {.snow = 50, .snowCover = 50}, 0, 0, real_weather::PresetStorm({}));
	EXPECT_EQ(applied.background.overcast, 20);
	EXPECT_EQ(applied.background.snowCover, 25);
	EXPECT_EQ(applied.background.snow, 10);
	// The rain and the winds set nothing, so they become 0
	EXPECT_EQ(applied.background.rain, 0);
	EXPECT_EQ(applied.background.windX, 0);
}

TEST(RealWeatherRules, TheStormsNumberOfCloudsComesFirst)
{
	// As in the shipped presets with a storm: the overcast adds 10 to the number of clouds of the storm
	const auto presets = real_weather::ParsePresets(FileOf({
	    PresetBytes(6, true).Change(k_CloudCount, 0, 4.0f).Change(k_Overcast, 1, 10.0f).Change(k_Rain, 0, 50.0f),
	}));
	const auto applied = real_weather::ApplyPreset(&presets[0], {}, 15, 80, real_weather::PresetStorm({}));
	EXPECT_EQ(applied.background.overcast, 14);
	ASSERT_TRUE(applied.storm.has_value());
	EXPECT_EQ(applied.storm->cloudCount, 4);
}

TEST(RealWeatherRules, ValuesStopAtOneHundredFromTheirLowByte)
{
	const auto presets = real_weather::ParsePresets(FileOf({
	    PresetBytes(10, true)
	        .Change(k_CloudCount, 0, 150.0f)
	        .Change(k_Overcast, 0, 110.0f)
	        .Change(k_SnowCover, 0, 300.0f)
	        .Change(k_Snowfall, 0, 99.9f)
	        .Change(k_CloudShade, 0, 1.5f),
	}));
	const auto applied = real_weather::ApplyPreset(&presets[0], {}, 0, 0, real_weather::PresetStorm({}));
	EXPECT_EQ(applied.background.overcast, 100);
	// 300 is 44 in its low byte
	EXPECT_EQ(applied.background.snowCover, 44);
	EXPECT_EQ(applied.background.snow, 99);
	ASSERT_TRUE(applied.storm.has_value());
	// 150 is -106 in its low byte
	EXPECT_EQ(applied.storm->cloudCount, -106);
	EXPECT_EQ(applied.storm->cloudShade, 1.0f);
}

TEST(RealWeatherRules, TheStormStandsOverTheMiddleWithTheNewBackgroundWeather)
{
	const auto presets = real_weather::ParsePresets(FileOf({
	    PresetBytes(2, true)
	        .Change(k_Rain, 0, 40.0f)
	        .Change(k_CloudCount, 0, 2.0f)
	        .Change(k_CloudHeight, 1, 40.0f)
	        .Change(k_Overcast, 1, 0.0f),
	}));
	const glm::vec3 centre {100.0f, 0.0f, 200.0f};
	const auto applied = real_weather::ApplyPreset(&presets[0], {}, 9, 50, real_weather::PresetStorm(centre));
	ASSERT_TRUE(applied.storm.has_value());
	const auto& storm = *applied.storm;
	EXPECT_EQ(storm.position, centre);
	EXPECT_EQ(storm.innerRadius, 1000.0f);
	EXPECT_EQ(storm.outerRadius, 2000.0f);
	EXPECT_EQ(storm.fadeSeconds, 1.0f);
	EXPECT_EQ(storm.lastsFor, 800000.0f);
	EXPECT_EQ(storm.strength, 1.0f);
	// An addition to the height starts from the height of a storm before the change
	EXPECT_EQ(storm.cloudHeight, 200.0f);
	EXPECT_EQ(storm.effect.temperature, 9);
	EXPECT_EQ(storm.effect.rain, 40);
	EXPECT_EQ(storm.effect.overcast, 2);
}

TEST(RealWeatherRules, PicturesAndPlaces)
{
	const std::array<uint8_t, 14> pictures {1, 0, 4, 5, 1, 8, 4, 7, 2, 3, 8, 6, 2, 1};
	for (int32_t type = 0; type < static_cast<int32_t>(pictures.size()); ++type)
	{
		EXPECT_EQ(real_weather::PictureOf(type), pictures.at(static_cast<size_t>(type))) << type;
	}
	EXPECT_EQ(real_weather::PictureOf(-1), 1);
	EXPECT_EQ(real_weather::PlaceText("Guildford", "Surrey", "Great Britain"), "Guildford (Surrey,Great Britain)");
	EXPECT_EQ(real_weather::PlaceText("Oslo", "", "Norway"), "Oslo (Norway)");
	EXPECT_EQ(real_weather::PlaceText("Oslo", "(null)", "Norway"), "Oslo (Norway)");
}

TEST(RealWeatherSystem, OffByDefaultItAsksNothing)
{
	FakeLobby lobby;
	FakeLand land;
	auto system = MakeSystem(lobby, false);
	system.StartGame(land);
	system.ProcessTurn(land);
	EXPECT_FALSE(lobby.weather);
	EXPECT_FALSE(system.IsApplied());
	EXPECT_EQ(land.background.temperature, 0);
}

TEST(RealWeatherSystem, AMultiplayerGameAsksNothing)
{
	FakeLobby lobby;
	FakeLand land;
	land.multiplayer = true;
	auto system = MakeSystem(lobby);
	system.StartGame(land);
	EXPECT_FALSE(lobby.weather);
}

TEST(RealWeatherSystem, WithoutThePresetsItAsksNothing)
{
	FakeLobby lobby;
	FakeLand land;
	RealWeatherSystem system(true, lobby, []() { return std::optional<std::vector<uint8_t>> {}; }, []() { return k_Today; });
	system.StartGame(land);
	EXPECT_FALSE(lobby.weather);
}

TEST(RealWeatherSystem, TheWeatherOfTodayChangesTheLandAtTheNextTurn)
{
	FakeLobby lobby;
	FakeLand land;
	land.background = {.temperature = 3, .snowCover = 7};
	auto system = MakeSystem(lobby);
	system.StartGame(land);
	ASSERT_TRUE(lobby.askedFor.has_value());
	EXPECT_EQ(lobby.askedFor->day, 22);
	system.ProcessTurn(land);
	EXPECT_FALSE(system.IsApplied());

	lobby.Answer(Weather(2, 11));
	system.ProcessTurn(land);
	EXPECT_TRUE(system.IsApplied());
	EXPECT_EQ(land.background.temperature, 11);
	EXPECT_EQ(land.background.rain, 40);
	ASSERT_TRUE(land.storm.has_value());
	EXPECT_EQ(land.storm->position, glm::vec3(2560.0f, 0.0f, 1280.0f));
	EXPECT_EQ(land.storm->cloudHeight, 200.0f);
	EXPECT_EQ(system.GetStorm(), entt::entity {42});

	// It changes the land one time only
	land.background.rain = 0;
	system.ProcessTurn(land);
	EXPECT_EQ(land.background.rain, 0);
}

TEST(RealWeatherSystem, OffPutsTheLandsWeatherBackAndEndsTheStorm)
{
	FakeLobby lobby;
	FakeLand land;
	land.background = {.temperature = 3, .rain = 1, .snowCover = 7};
	const auto before = land.background;
	auto system = MakeSystem(lobby);
	system.StartGame(land);
	lobby.Answer(Weather(2));
	system.ProcessTurn(land);
	ASSERT_TRUE(land.living);

	system.Request(false);
	system.ProcessTurn(land);
	EXPECT_FALSE(system.IsApplied());
	EXPECT_EQ(land.ends, 1);
	EXPECT_TRUE(system.GetStorm() == entt::null);
	EXPECT_EQ(land.background.temperature, before.temperature);
	EXPECT_EQ(land.background.rain, before.rain);
	EXPECT_EQ(land.background.snowCover, before.snowCover);

	system.Request(true);
	system.ProcessTurn(land);
	EXPECT_TRUE(system.IsApplied());
}

TEST(RealWeatherSystem, NoTypeChangesNothing)
{
	FakeLobby lobby;
	FakeLand land;
	auto system = MakeSystem(lobby);
	system.StartGame(land);
	lobby.Answer(Weather(real_weather::k_NoType, 20));
	system.ProcessTurn(land);
	EXPECT_FALSE(system.IsApplied());
	EXPECT_EQ(land.background.temperature, 0);
}

TEST(RealWeatherSystem, ItResumesTheSessionFirst)
{
	FakeLobby lobby;
	lobby.state = LobbySessionState::LoggedOut;
	FakeLand land;
	auto system = MakeSystem(lobby);
	system.StartGame(land);
	ASSERT_TRUE(lobby.resume);
	EXPECT_FALSE(lobby.weather);
	lobby.resume(LobbyResult<lobby_api::AccountRef> {.value = lobby_api::AccountRef {.id = 1, .name = "abc"}, .error = {}});
	EXPECT_TRUE(lobby.weather);
}

TEST(RealWeatherSystem, AnAnswerForAnEarlierGameIsIgnored)
{
	FakeLobby lobby;
	FakeLand land;
	auto system = MakeSystem(lobby);
	system.StartGame(land);
	auto late = std::move(lobby.weather);
	system.StartGame(land);
	late(LobbyResult<lobby_api::RealWeather> {.value = Weather(4, 30), .error = {}});
	system.ProcessTurn(land);
	EXPECT_FALSE(system.IsApplied());
	EXPECT_EQ(land.background.temperature, 0);
}
