/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Guidance.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <deque>
#include <string>
#include <string_view>
#include <utility>

#include <spdlog/spdlog.h>

#include "Audio/Game/AudioSystem.h"
#include "Audio/GameQueries.h"
#include "Audio/Services/Confirmation.h"
#include "Audio/Services/SpookyVoices.h"
#include "Audio/Services/Voices.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "Debug/DebugEnv.h"
#include "ECS/Systems/AudioStateInterface.h"
#include "GameClock.h"
#include "Locator.h"

// Every function is described in Guidance.h; the comments here explain the order of the arithmetic where it matters.
// Notes: docs/bw1-notes/audio.md.

using namespace openblack;
using namespace openblack::audio;
using namespace openblack::audio::guidance;

namespace
{
constexpr float k_DesireThreshold = 0.3f;
constexpr float k_DesireDistance = 200.0f; // the camera range and the max distance factor
constexpr float k_DesireTurns = 50.0f;
constexpr float k_ThingTurns = 300.0f;
constexpr uint32_t k_ThingForgetTurns = 600;
constexpr float k_ResourceTownDistance = 100.0f;
constexpr float k_ResourcePleased = 0.5f;
constexpr float k_ResourceDispleased = 0.25f;
constexpr float k_ResourceMaxDistance = 200.0f;
constexpr float k_AttackMaxDistance = 200.0f;
constexpr float k_BeliefMaxDistance = 200.0f;
constexpr float k_BeliefVisibility = 0.3f;
constexpr float k_HelpSpritesRange = 300.0f;
constexpr float k_HeartBeatMaxDistance = 500.0f;
constexpr int k_HeartBeatSample = 45;             // G_HeartBeat (InGame)
constexpr uint32_t k_DeathInVillageText = 0x1658; // HELP_TEXT_DEATH_IN_VILLAGE_06
constexpr std::string_view k_ScriptWithText = "MultiHelpJustTalkWithText";
constexpr std::string_view k_ScriptWithNoText = "MultiHelpJustTalkWithNoText";

struct GuidanceState: State
{
	/// The remembered things {thing, turn}, newest first
	std::deque<std::pair<uint32_t, uint32_t>> things;
	std::array<SpriteList, k_SpriteLists> lists {};
	RandomFn random;
};

/// The Guidance state (Locator::audioState)
GuidanceState& GuidanceData()
{
	return openblack::Locator::audioState::value().Get<GuidanceState>();
}

bool Trace()
{
	static const bool k_Trace = debug_env::GuidanceTrace();
	return k_Trace;
}

uint32_t Turn()
{
	const auto& queries = Queries();
	return queries.turn ? queries.turn() : 0;
}

uint32_t LocalPlayer()
{
	const auto& queries = Queries();
	return queries.localPlayerNumber ? queries.localPlayerNumber() : 0;
}

/// The owner the original gives its samples: the player's number as a key, 0 = none
Owner PlayerOwner(uint32_t number)
{
	return number == 0 ? Owner::None() : Owner::Key(number);
}

/// The game's map distance in metres (gutils::GetDistanceInMetres): the points are world points that the original holds
/// as fixed-point map coordinates, so they are truncated to 16.16 first
float Distance(glm::vec3 a, glm::vec3 b)
{
	return gutils::GetDistanceInMetres(a, b);
}

std::optional<glm::vec3> CameraPosition()
{
	const auto& queries = Queries();
	if (!queries.camera)
	{
		return std::nullopt;
	}
	const auto camera = queries.camera();
	return camera ? std::optional<glm::vec3>(camera->position) : std::nullopt;
}

/// The hand's distance test of the HelpSprites remarks (< 300)
bool NearHand(glm::vec3 point)
{
	const auto& queries = Queries();
	const auto hand = queries.handPosition ? queries.handPosition() : std::nullopt;
	return hand && Distance(point, *hand) < k_HelpSpritesRange;
}

/// The list's random text -> SpiritSay (the shape of every HelpSprites remark, after its CanPlaySpiritRemark)
void SayFromList(Type type, size_t list)
{
	SpiritSay(GetRandomSample(list), type);
}

/// The entries before the first 0; all 34 set count 33
uint32_t ListCount(const SpriteList& list)
{
	uint32_t n = 0;
	while (n < list.size())
	{
		if (list.at(n) == 0)
		{
			return n;
		}
		++n;
	}
	return n - 1;
}

/// The cube as the original computes it: x * x * x. The game runs the FPU at 24-bit precision, so every add, subtract,
/// multiply and divide of this file rounds to a float and the arithmetic is written in float; a double constant is
/// applied exactly and the result rounded (static_cast<float>)
float Cube(float x)
{
	return x * x * x;
}

/// The nearest town's best desire sample and its score
void CheckTownDesires(uint32_t& sample, float& value, std::optional<glm::vec3>& thing, glm::vec3 camera)
{
	const auto& queries = Queries();
	if (!queries.desireTowns)
	{
		return;
	}
	const auto towns = queries.desireTowns();
	// The best distance starts at 200; only a town with a storage pit and people counts
	float best = k_DesireDistance;
	const DesireTown* chosen = nullptr;
	for (const auto& town : towns)
	{
		if (!town.storagePit || town.population == 0)
		{
			continue;
		}
		const float d = Distance(*town.storagePit, camera); // the pit to the camera
		if (Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Guidance: town {} pit ({:.0f}, {:.0f}) pop {} at {:.1f}", town.id,
			                   town.storagePit->x, town.storagePit->z, town.population, d);
		}
		if (d < best)
		{
			best = d;
			chosen = &town;
		}
	}
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Guidance: town desires: {} towns, nearest pit {} at {:.1f}", towns.size(),
		                   chosen != nullptr ? chosen->id : 0, best);
	}
	if (chosen == nullptr)
	{
		return;
	}
	thing = chosen->position; // the thing is the town
	// The 17 desires {value, type}
	for (const auto& desire : chosen->desires)
	{
		const float raw = desire.raw; // the raw desire of the type
		const uint32_t text = DesireSample(desire.type, desire.value);
		if (text == 0)
		{
			continue;
		}
		const float score = DesireScore(chosen->id, best, raw, text);
		if (score > value) // strictly greater
		{
			value = score;
			sample = text;
		}
	}
}

/// The nearest worship site's desire sample and its score, which replace the town's when they scored
void CheckWorshipSiteDesires(uint32_t& sample, float& value, std::optional<glm::vec3>& thing, glm::vec3 camera)
{
	const auto& queries = Queries();
	if (!queries.worshipSites)
	{
		return;
	}
	const auto citadel = queries.worshipSites();
	if (Trace())
	{
		const auto count =
		    citadel ? std::ranges::count_if(citadel->sites, [](const auto& candidate) { return static_cast<bool>(candidate); })
		            : 0;
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Guidance: worship sites: {} ({} sites)", citadel ? "citadel" : "no citadel",
		                   count);
	}
	if (!citadel)
	{
		return; // the player has no citadel
	}
	float best = k_DesireDistance;
	const WorshipDesire::Site* site = nullptr;
	for (const auto& candidate : citadel->sites)
	{
		if (!candidate)
		{
			continue;
		}
		const float d = Distance(candidate->position, camera);
		if (Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"),
			                   "Guidance: worship site {} ({:.0f}, {:.0f}) worshippers {} food {:.3f} need {:.3f} at {:.1f}",
			                   candidate->id, candidate->position.x, candidate->position.z, candidate->worshippers,
			                   candidate->foodDesire, citadel->need, d);
		}
		if (!candidate->worshippers)
		{
			continue;
		}
		if (d < best)
		{
			best = d;
			site = &*candidate;
		}
	}
	if (site == nullptr)
	{
		return;
	}
	const float food = site->foodDesire;
	// Capped at 1: kept when below or unordered (a NaN too), else 1
	const float need = !(citadel->need >= 1.0f) ? citadel->need : 1.0f;
	// Both texts are drawn with the food value
	const uint32_t foodText = DesireSample(17, food);
	const uint32_t needText = DesireSample(18, food);
	const float foodScore = foodText != 0 ? DesireScore(site->id, best, food, foodText) : 0.0f;
	const float needScore = needText != 0 ? DesireScore(site->id, best, need, needText) : 0.0f;
	if (foodScore > needScore)
	{
		sample = foodText;
		value = foodScore;
		thing = citadel->citadelPosition;
	}
	if (needScore != 0.0f) // the need wins whenever it scored, even below the food's
	{
		sample = needText;
		value = needScore;
		thing = citadel->citadelPosition;
	}
}
} // namespace

// ---- random numbers -----------------------------------------------------------------------------------------------

void guidance::SetRandom(RandomFn random)
{
	GuidanceData().random = std::move(random);
}

uint32_t guidance::SeededRandom(uint32_t n, uint32_t& seed)
{
	return game_random::SeededRandom(n, seed);
}

uint32_t guidance::LocalRand(uint32_t n)
{
	auto& state = GuidanceData();
	if (n == 0)
	{
		return 0;
	}
	if (state.random)
	{
		return state.random(n); // the tests' generator
	}
	// The one local stream of the game (n as the signed long it is)
	return game_random::LocalRand(static_cast<int32_t>(n));
}

float guidance::LocalFloatRand(float x)
{
	if (x == 0.0f)
	{
		return 0.0f;
	}
	if (!GuidanceData().random)
	{
		return game_random::LocalFloatRand(x); // on the local stream
	}
	// The tests' generator: a draw in [0, 65535] as an exact integer, times x, times about 1/65535
	const float k_Scale = game_random::FloatRandScale();
	return static_cast<float>(LocalRand(0xFFFF)) * x * k_Scale; // the integer exact, two float multiplies
}

void guidance::SetSpriteLists(const std::array<SpriteList, k_SpriteLists>& lists)
{
	GuidanceData().lists = lists;
}

// ---- the core -----------------------------------------------------------------------------------------------------

void guidance::Init()
{
	auto& state = GuidanceData();
	// New sample play options: bank Guidance, 2D, no tracking, mode 2
	state.options = sample_play::Options {};
	state.options.is3D = false;
	state.options.track = false;
	state.options.mode = 2;
	state.sample = {Bank(SfxBank::Guidance), 0};
	// Each type's last play spread back by up to its base
	const uint32_t turn = Turn();
	for (size_t t = 0; t < k_TypeCount; ++t)
	{
		const auto first = static_cast<int32_t>(turn - LocalRand(k_Types.at(t).base));
		state.lastPlayed.at(t) = first > 0 ? turn - LocalRand(k_Types.at(t).base) : 0;
	}
	// The remembered things emptied
	state.things.clear();
	state.lastDesireSample = 0;
	state.lastBeliefSample = 0;
	state.heartBeatPlaying = false;
	state.heartBeatOverride = 0.0f;
	state.heartBeatPhase = 0.0f;
	state.heartBeatPulse = 0.0f;
	state.heartBeatPulsePrevious = 0.0f;
	state.believers = 0.0f;
	state.beliefShare = 0.0f;
	state.totemHeight = 0.0f;
	state.alignmentChange = 0.0f;
	state.heartBeatPitch = 30.0f;
	state.oneOffs.fill(false); // all 7 flags
	state.enabled = true;
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Guidance: Init at turn {}", turn);
	}
}

void guidance::Close()
{
	GuidanceData().things.clear();
}

void guidance::ResetForTests()
{
	auto& state = GuidanceData();
	auto random = std::move(state.random);
	auto lists = state.lists;
	state = GuidanceState {};
	state.random = std::move(random);
	state.lists = lists;
	state.enabled = true;
	state.options.is3D = false;
	state.options.track = false;
	state.options.mode = 2;
	state.sample = {Bank(SfxBank::Guidance), 0};
}

const State& guidance::GetState()
{
	return GuidanceData();
}

uint32_t guidance::TimeSinceLastPlayed(Type type)
{
	return Turn() - GuidanceData().lastPlayed.at(static_cast<size_t>(type));
}

uint32_t guidance::Interval(Type type)
{
	// r = LocalFloatRand(1.0) kept as a float; 5 base as an exact integer times (1 - r^3)
	const float r = LocalFloatRand(1.0f);
	const uint32_t base = k_Types.at(static_cast<size_t>(type)).base;
	const auto five = static_cast<float>(static_cast<int32_t>(base * 5u));
	const auto spread = static_cast<int32_t>(five * (1.0f - Cube(r))); // truncation
	return LocalRand(static_cast<uint32_t>(spread)) + base;
}

bool guidance::PlayNow(Type type)
{
	const auto& info = k_Types.at(static_cast<size_t>(type));
	const auto& queries = Queries();
	if (!info.always)
	{
		const int land = queries.landNumber ? queries.landNumber() : 0;
		const bool multiplayer = queries.multiplayerGame && queries.multiplayerGame();
		const bool playground = queries.playgroundGame && queries.playgroundGame();
		if (land == 1 && !multiplayer && !playground)
		{
			return false;
		}
	}
	const int level = queries.helpLevel ? queries.helpLevel() : 3;
	if (level < info.helpLevel)
	{
		return false;
	}
	const uint32_t interval = Interval(type); // drawn before the time since the last play is read
	return TimeSinceLastPlayed(type) > interval;
}

bool guidance::CanPlaySpiritRemark(Type type)
{
	return PlayNow(Type::HelpSprites) && PlayNow(type);
}

Channel guidance::PlaySample(bool is3D, uint32_t textOrSample, uint32_t owner, int type, int volume, int pitch, int field2C,
                             std::optional<glm::vec3> point, float maxDistance, bool isText)
{
	auto& state = GuidanceData();
	auto& options = state.options;
	options.volume = volume;
	options.pitch = pitch;
	// Default 90: kept, not modelled (sample_play::Options has no such field; its use in sample playback is unknown)
	state.field2C = field2C;
	// A text is looked up in the voice table
	state.sample.number = static_cast<int>(isText ? voices::Table().Get(textOrSample).sample : textOrSample);
	options.owner = PlayerOwner(owner);
	if (is3D)
	{
		options.is3D = true;
		if (!point)
		{
			return k_NoChannel; // no position, nothing (and lastPlayed stays)
		}
		// The point in world units (altitude + height), max = maxDistance, min = maxDistance x 0.333
		options.position = *point;
		options.maxDistance = maxDistance;
		options.minDistance = maxDistance * 0.333f;
		options.callerMask = 0x180;
		options.track = false;
	}
	else
	{
		options.is3D = false;
	}
	PlayOptions play;
	static_cast<sample_play::Options&>(play) = options;
	play.sound = 0;
	play.sample = state.sample;
	const auto channel = PlaySoundEffect(play);
	if (type < static_cast<int>(k_TypeCount))
	{
		state.lastPlayed.at(static_cast<size_t>(type)) = Turn();
	}
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Guidance: type {} {} {} {} -> sample {} {} (channel {})", type,
		                   isText ? "text" : "sample", textOrSample, is3D ? "3D" : "2D", BankGroup(play.sample.bank),
		                   play.sample.number, channel);
	}
	return channel;
}

void guidance::SpiritSay(uint32_t text, Type type)
{
	auto& state = GuidanceData();
	const auto& queries = Queries();
	const auto script = type == Type::JustTalkNoText ? k_ScriptWithNoText : k_ScriptWithText;
	const bool started = queries.helpRunMessage && queries.helpRunMessage(text, text, script);
	if (queries.helpTriggerCategory)
	{
		queries.helpTriggerCategory(8);
	}
	const uint32_t turn = Turn();
	if (static_cast<size_t>(type) < k_TypeCount)
	{
		state.lastPlayed.at(static_cast<size_t>(type)) = turn;
	}
	state.lastSpiritSay = turn;
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Guidance: HelpSpiritSay({}, type {}) {} {}", text, static_cast<int>(type),
		                   script, started ? "started" : "not started");
	}
}

void guidance::OneOff(int index)
{
	if (index < 0 || index >= static_cast<int>(k_OneOffs.size()))
	{
		return; // (openblack) the original indexes its 7 flags unchecked
	}
	auto& done = GuidanceData().oneOffs.at(static_cast<size_t>(index));
	if (done)
	{
		return;
	}
	const auto& oneOff = k_OneOffs.at(static_cast<size_t>(index));
	if (LocalFloatRand(1.0f) > oneOff.probability) // skipped when r > p
	{
		return;
	}
	SpiritSay(oneOff.text, Type::OneOff);
	done = true;
}

uint32_t guidance::GetRandomSample(size_t list)
{
	if (list >= k_SpriteLists)
	{
		return 0;
	}
	const auto& entries = GuidanceData().lists.at(list);
	return entries.at(LocalRand(ListCount(entries)));
}

uint32_t guidance::RandomSampleForValue(size_t list, float value)
{
	if (list >= k_SpriteLists)
	{
		return 0;
	}
	// The value clamped to 0..1
	if (value < 0.0f)
	{
		value = 0.0f;
	}
	else if (value > 1.0f)
	{
		value = 1.0f;
	}
	const auto& entries = GuidanceData().lists.at(list);
	const auto n = static_cast<int32_t>(static_cast<float>(ListCount(entries)) * value); // truncated
	return entries.at(LocalRand(static_cast<uint32_t>(n)));                              // r < n <= ListCount <= 33
}

uint32_t guidance::TimeSinceThingSeen(uint32_t thing)
{
	auto& state = GuidanceData();
	const uint32_t turn = Turn();
	for (auto& [id, seen] : state.things)
	{
		if (id == thing)
		{
			if (turn - seen > k_ThingForgetTurns) // unsigned, strictly past
			{
				seen = turn;
			}
			return turn - seen;
		}
	}
	state.things.emplace_front(thing, turn); // the new node is the head
	return 0;
}

uint32_t guidance::DesireSample(uint32_t desire, float value)
{
	// x = value - LocalFloatRand(value x 1/3)
	const float x = value - LocalFloatRand(value * 0.33333334f);
	if (desire >= k_DesireTexts.size())
	{
		return 0; // (openblack) the original reads past the table
	}
	const auto& texts = k_DesireTexts.at(desire);
	if (x >= 0.85f)
	{
		return texts[0];
	}
	if (x >= 0.65f)
	{
		return texts[1];
	}
	if (x >= 0.45f)
	{
		return texts[2];
	}
	return 0;
}

float guidance::DesireScore(uint32_t thing, float distance, float value, uint32_t sample)
{
	// t0 = min(TimeSince(TownDesire) / 50, 1), kept as a float
	float t0 = static_cast<float>(TimeSinceLastPlayed(Type::TownDesire)) / k_DesireTurns;
	if (!(t0 < 1.0f))
	{
		t0 = 1.0f;
	}
	// t1 = min(TimeSinceThingSeen(thing) / 300, 1); the function is called again for the value kept
	float t1 = static_cast<float>(TimeSinceThingSeen(thing)) / k_ThingTurns;
	if (t1 < 1.0f)
	{
		t1 = static_cast<float>(TimeSinceThingSeen(thing)) / k_ThingTurns;
	}
	else
	{
		t1 = 1.0f;
	}
	const float seen = 1.0f - Cube(t1);
	// d = min(distance / 200, 1), 1 - d^2
	float d = distance / k_DesireDistance;
	if (!(d < 1.0f))
	{
		d = 1.0f;
	}
	const float near = 1.0f - d * d;
	// v = min(value, 1) (stored as a float), v^3
	const float v = value < 1.0f ? value : 1.0f;
	const float v3 = Cube(v);
	// The sample said last time -> t0^3
	const float s = sample == GuidanceData().lastDesireSample ? Cube(t0) : 1.0f;
	// 2 t0 (s v^3 near seen), in the original's order
	const float r = seen * (v3 * s * near);
	return 2.0f * (r * t0);
}

// ---- the turn ------------------------------------------------------------------------------------------------------

void guidance::UpdateTownDesireRemarks()
{
	if (Turn() % 10 != 0)
	{
		return;
	}
	if (!PlayNow(Type::TownDesire))
	{
		return;
	}
	const auto camera = CameraPosition(); // the interface's camera
	if (!camera)
	{
		return; // (openblack) the original always has the interface's camera
	}
	uint32_t sample = 0;
	float value = 0.0f;
	std::optional<glm::vec3> thing;
	CheckTownDesires(sample, value, thing, *camera);
	CheckWorshipSiteDesires(sample, value, thing, *camera);
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Guidance: desire sample {} value {:.3f}", sample, value);
	}
	if (!(value > k_DesireThreshold))
	{
		return;
	}
	// x = value - LocalFloatRand(value x 0.5)
	const float x = value - LocalFloatRand(value * 0.5f);
	if (!thing)
	{
		return;
	}
	PlaySample(true, sample, LocalPlayer(), static_cast<int>(Type::TownDesire), 127, 100, 90, thing, k_DesireDistance * x,
	           true);
	GuidanceData().lastDesireSample = sample;
}

void guidance::UpdateHeartBeat()
{
	auto& g = GuidanceData();
	// Off the tenth turns the value is kept and the beat still runs
	if (Turn() % 10 != 0)
	{
		HeartBeat(g.heartBeatValue);
		return;
	}
	const auto& queries = Queries();
	const auto input = queries.heartBeat ? queries.heartBeat() : HeartBeatInput {};
	// The sum of the towns' raw protection desire
	g.heartBeatValue = 0.0f + input.protectionDesire;
	const float p = input.believers;
	const float q = input.beliefShare;
	// ((beliefShare + 0.001) / (q + 0.001) - 1) + ((believers + 0.001) / (p + 0.001) - 1) + value, all with float
	// constants; the FPU is at 24 bits, so each step is a float operation (q already has a float's precision)
	constexpr float k_Small = 0.001f;
	const float shareTerm = (g.beliefShare + k_Small) / (q + k_Small) - 1.0f;
	const float believersTerm = (g.believers + k_Small) / (p + k_Small) - 1.0f;
	g.heartBeatValue = (shareTerm + believersTerm) + g.heartBeatValue;
	// Both smoothed by 0.1
	g.believers = (p - g.believers) * 0.1f + g.believers;
	g.beliefShare = (q - g.beliefShare) * 0.1f + g.beliefShare;
	// Another player's creature near the local player's town: 1 - max(d - 100, 0) / 400 when d < 400 (the integer
	// distance converted exactly; 400 and 100 are floats)
	for (const uint32_t distance : input.enemyCreatureDistances)
	{
		const auto d = static_cast<float>(distance);
		if (!(d < 400.0f))
		{
			continue;
		}
		float over = d - 100.0f;
		if (!(over > 0.0f))
		{
			over = 0.0f;
		}
		g.heartBeatValue = (1.0f - over / 400.0f) + g.heartBeatValue;
	}
	// Clamped to 0..1
	if (g.heartBeatValue < 0.0f)
	{
		g.heartBeatValue = 0.0f;
	}
	else if (g.heartBeatValue > 1.0f)
	{
		g.heartBeatValue = 1.0f;
	}
	HeartBeat(g.heartBeatValue); // (then the debug line "HeartBeatvalue: %.2f")
}

float guidance::MoonPhase(int64_t unixTime)
{
	// The whole days (truncated) - 10962 (exact), times the double 0.03386318012808897 (1 / 29.5306); the fraction by
	// truncation and (1 - it) times the double 6.2831854820251465 (the float 2 pi kept as a double); 1 is a double. With
	// the FPU at 24 bits every multiply / subtract rounds to a float, the doubles themselves do not
	const auto days = static_cast<int32_t>(unixTime / 86400) - 0x2AD2;
	const auto cycles = static_cast<float>(static_cast<double>(days) * 0.03386318012808897);
	const float fraction = cycles - static_cast<float>(static_cast<int32_t>(cycles));
	const auto rest = static_cast<float>(1.0 - static_cast<double>(fraction));
	return static_cast<float>(static_cast<double>(rest) * 6.2831854820251465);
}

void guidance::RemarkOnMoonPhase()
{
	auto& countdown = GuidanceData().moonCountdown;
	countdown = static_cast<uint32_t>(static_cast<int32_t>(countdown) - 1); // signed countdown
	if (static_cast<int32_t>(countdown) >= 1)
	{
		return;
	}
	const auto& queries = Queries();
	if (!(queries.visualNight && queries.visualNight()))
	{
		return; // only at visual night
	}
	// The moon's phase - pi, kept as a float
	const float x = MoonPhase(spooky::UnixTime()) - 3.14159274f;
	if (spooky::NightNow(spooky::LocalTime()) && std::abs(static_cast<double>(x)) < 0.15000000596046448) // a double
	{
		OneOff(5);
		countdown = 0x927C0; // 600000
		return;
	}
	countdown = static_cast<uint32_t>(static_cast<int32_t>(x * x * 12000.0f)); // float steps at 24 bits
}

void guidance::ProcessGameTurn()
{
	// OPENBLACK_TEST_GUIDANCE_SAY=<turn>:<HELP_TEXT> (openblack test hook): SpiritSay(text, OneOff) once at that turn
	if (static const char* k_Say = std::getenv("OPENBLACK_TEST_GUIDANCE_SAY"); k_Say != nullptr)
	{
		const std::string_view hook(k_Say);
		if (const auto colon = hook.find(':'); colon != std::string_view::npos)
		{
			const auto turn = static_cast<uint32_t>(std::strtoul(std::string(hook.substr(0, colon)).c_str(), nullptr, 10));
			const auto text = static_cast<uint32_t>(std::strtoul(std::string(hook.substr(colon + 1)).c_str(), nullptr, 10));
			if (Turn() == turn)
			{
				SPDLOG_LOGGER_INFO(spdlog::get("audio"), "OPENBLACK_TEST_GUIDANCE_SAY: HelpSpiritSay({}) at turn {}", text,
				                   turn);
				SpiritSay(text, Type::OneOff);
			}
		}
	}
	spooky::Process();
	RemarkOnMoonPhase();
	UpdateTownDesireRemarks();
	// The confirmation: the angle and pitch "yes" (START_ANGLE_SOUND 285 / 348, Services/Confirmation.h)
	confirmation::Process(Turn());
	// The local interface's turn: (approximate) its place in the turn
	UpdateHeartBeat();
}

// ---- the events ----------------------------------------------------------------------------------------------------

uint32_t guidance::ResourceDropSample(float need, RainType type)
{
	// Three texts from LocalRand(3) (0, 1, 2)
	const auto pick = [](uint32_t first) { return first + LocalRand(3); };
	switch (type)
	{
	case RainType::Food:
		if (!(need < k_ResourcePleased))
		{
			return pick(0x1352); // PLEASED_FOOD_01..03
		}
		return need < k_ResourceDispleased ? pick(0x135B) : 0; // DISPLEASED_FOOD_01..03
	case RainType::Wood:
		if (!(need < k_ResourcePleased))
		{
			return pick(0x1355); // PLEASED_WOOD_01..03
		}
		return need < k_ResourceDispleased ? pick(0x1355) : 0; // the same
	case RainType::Rain:
		if (!(need < k_ResourcePleased))
		{
			return pick(0x1358); // PLEASED_RAIN_01..03
		}
		return need < k_ResourceDispleased ? pick(0x1358) : 0; // the same
	default:
		return 0;
	}
}

void guidance::PlayResourceDropRemark(glm::vec3 point, RainType type)
{
	if (!PlayNow(Type::ResourceDrop))
	{
		return;
	}
	const auto& queries = Queries();
	// The nearest town within 100 of the point
	const auto town = queries.nearestTownAt ? queries.nearestTownAt(point, k_ResourceTownDistance) : std::nullopt;
	if (!town)
	{
		return; // no town
	}
	// ResourceDropSample's reads of the town (openblack: nullopt while they are not ported, silent)
	const auto needs = queries.townResourceNeeds ? queries.townResourceNeeds(*town) : std::nullopt;
	if (!needs)
	{
		return;
	}
	if (type == RainType::None || type > RainType::Rain)
	{
		return; // ResourceDropSample's default: 0
	}
	const uint32_t text = ResourceDropSample(needs->at(static_cast<size_t>(type) - 1), type);
	if (text == 0)
	{
		return;
	}
	PlaySample(true, text, LocalPlayer(), static_cast<int>(Type::ResourceDrop), 127, 100, 90, point, k_ResourceMaxDistance,
	           true);
}

int guidance::StrongestEffect(const std::array<float, 7>& effects)
{
	// The best starts at 0 and 7; 3, 5 and 6 are skipped
	int best = 7;
	float strongest = 0.0f;
	for (int i = 0; i < 7; ++i)
	{
		if (i == 3 || i == 5 || i == 6)
		{
			continue;
		}
		float v = effects.at(static_cast<size_t>(i));
		if (i == 0)
		{
			v = v * 0.001f;
			if (!(v < 1.0f))
			{
				v = 1.0f;
			}
		}
		if (v > strongest) // strictly greater
		{
			strongest = v;
			best = i;
		}
	}
	return best;
}

uint32_t guidance::AttackerSample(Attacker attacker)
{
	switch (attacker)
	{
	case Attacker::LightningSpell:
		return 0x132E + LocalRand(10); // LIGHTNING_01..10
	case Attacker::Rock:
		return 0x1342 + LocalRand(10); // ROCKS_01..10
	case Attacker::Creature:
		return 0x1338 + LocalRand(10); // MONSTER_01..10
	case Attacker::OtherSpell:
	case Attacker::Thing:
	default:
		return 0;
	}
}

void guidance::PlayTownAttackRemark(const TownAttack& attack)
{
	if (PlayNow(Type::TownAttack) && attack.population != 0)
	{
		const int effect = StrongestEffect(attack.effects);
		if (effect != 7)
		{
			// A list whose new nodes go first
			std::deque<uint32_t> list;
			for (uint32_t text = 0x131A; text <= 0x1323; ++text) // ATTACK_01..10
			{
				list.push_front(text);
			}
			if (effect == 0)
			{
				for (uint32_t text = 0x1324; text <= 0x132D; ++text) // FIRE_01..10
				{
					list.push_front(text);
				}
			}
			if (attack.attacker)
			{
				if (const uint32_t sample = AttackerSample(*attack.attacker); sample != 0)
				{
					for (int i = 0; i < 10; ++i)
					{
						list.push_front(sample);
					}
				}
			}
			// maxDistance factor min(severity x 0.2, 1) + 1
			float factor = attack.severity * 0.2f;
			if (!(factor < 1.0f))
			{
				factor = 1.0f;
			}
			factor += 1.0f;
			// The entry at LocalRand(count)
			const uint32_t index = LocalRand(static_cast<uint32_t>(list.size()));
			const uint32_t text = index < list.size() ? list.at(index) : 0;
			PlaySample(true, text, LocalPlayer(), static_cast<int>(Type::TownAttack), 127, 100, 90, attack.position,
			           k_AttackMaxDistance * factor, true);
		}
	}
	// Always, whether a sample played or not
	if (attack.townIsMine)
	{
		WarnTownUnderAttack(attack);
	}
}

void guidance::WarnTownUnderAttack(const TownAttack& attack)
{
	if (!attack.townOfLocalPlayer || !attack.causedPlayer || attack.causedIsLocalPlayer || attack.population == 0)
	{
		return;
	}
	if (!PlayNow(Type::TownBeingAttacked))
	{
		return;
	}
	const auto n = static_cast<size_t>(*attack.causedPlayer);
	if (n >= attack.aggression.size() || !(attack.aggression.at(n) > 1.0f))
	{
		return;
	}
	SayFromList(Type::TownBeingAttacked, 0);
}

void guidance::StartTotemRaiseSound(float height)
{
	GuidanceData().totemHeight = height;
}

void guidance::EndTotemRaiseSound(float height)
{
	if (height > GuidanceData().totemHeight && PlayNow(Type::RaiseTotem))
	{
		// AlignmentClass, whose result picks nothing
	}
}

int guidance::AlignmentClass(float alignment)
{
	if (alignment > 0.55f)
	{
		return 1;
	}
	if (alignment < -0.55f)
	{
		return 2;
	}
	return 0;
}

uint32_t guidance::DiscipleText(uint32_t disciple, int alignmentClass)
{
	if (disciple == 10)
	{
		if (alignmentClass < 0)
		{
			return 0;
		}
		if (alignmentClass <= 1)
		{
			return LocalFloatRand(1.0f) > 0.5f ? 0x12FB : 0x12FC; // GOOD_LIVE_HERE_01 / _02
		}
		if (alignmentClass == 2)
		{
			return LocalFloatRand(1.0f) > 0.5f ? 0x12FD : 0x12FE; // EVIL_LIVE_HERE_01 / _02
		}
		return 0;
	}
	// The texts of VILLAGER_DISCIPLE 0..9
	constexpr std::array<uint32_t, 10> k_Texts {0, 4863, 4865, 4868, 4867, 4869, 0, 4870, 4864, 4866};
	return disciple < k_Texts.size() ? k_Texts.at(disciple) : 0;
}

void guidance::PlayDiscipleRemark(uint32_t disciple, float localAlignment)
{
	if (!PlayNow(Type::Disciple))
	{
		return;
	}
	const uint32_t text = DiscipleText(disciple, AlignmentClass(localAlignment));
	PlaySample(false, text, LocalPlayer(), static_cast<int>(Type::Disciple), 85, 100, 90, std::nullopt, 0.0f, true);
}

void guidance::PlayBeliefRemark(const std::array<float, 8>& beliefs, uint32_t player, glm::vec3 point, float distanceToCamera,
                                int alignment)
{
	// The strongest belief, from 0
	float strongest = 0.0f;
	for (const float belief : beliefs)
	{
		if (strongest < belief)
		{
			strongest = belief;
		}
	}
	if (player >= beliefs.size())
	{
		return;
	}
	const float mine = beliefs.at(player);
	if (!(mine < strongest)) // only a player below the strongest
	{
		return;
	}
	const float value = (mine + 0.0001f) / (strongest + 0.0001f);
	PlayBeliefSample(point, distanceToCamera, value, alignment);
}

void guidance::PlayBeliefSample(glm::vec3 point, float distance, float value, int alignment)
{
	if (!PlayNow(Type::Belief))
	{
		return;
	}
	// x = value - LocalFloatRand(value x 1/3), kept as a float
	const float x = value - LocalFloatRand(value * 0.33333334f);
	if (alignment != 1 && alignment != 2)
	{
		alignment = LocalRand(2) != 0 ? 2 : 1; // a coin
	}
	const uint32_t first = alignment == 1 ? 0x134C : 0x134F; // GOOD_AWE / EVIL_AWE
	uint32_t text = 0;
	if (!(x < 0.7f))
	{
		text = first;
	}
	else if (!(x < 0.4f))
	{
		text = first + 1;
	}
	else if (x > 0.05f)
	{
		text = first + 2;
	}
	else
	{
		return;
	}
	if (!(BeliefVisibility(distance, text) > k_BeliefVisibility))
	{
		return;
	}
	PlaySample(true, text, LocalPlayer(), static_cast<int>(Type::Belief), 127, 100, 90, point, k_BeliefMaxDistance, true);
}

float guidance::BeliefVisibility(float distance, uint32_t text)
{
	float t0 = static_cast<float>(TimeSinceLastPlayed(Type::TownDesire)) / k_DesireTurns;
	if (!(t0 < 1.0f))
	{
		t0 = 1.0f;
	}
	float d = distance / k_BeliefMaxDistance;
	if (!(d < 1.0f))
	{
		d = 1.0f;
	}
	const float near = 1.0f - d * d;
	if (text == GuidanceData().lastBeliefSample)
	{
		return Cube(t0) * near * t0;
	}
	return near * t0;
}

void guidance::PlayVillageDeathRemark()
{
	if (!PlayNow(Type::DeathInVillage))
	{
		return;
	}
	const uint32_t text = k_DeathInVillageText + LocalRand(5);
	PlaySample(false, text, LocalPlayer(), static_cast<int>(Type::DeathInVillage), 127, 100, 90, std::nullopt, 0.0f, true);
}

void guidance::SetHeartBeatOverride(float value)
{
	GuidanceData().heartBeatOverride = value;
}

void guidance::HeartBeat(float value)
{
	auto& g = GuidanceData();
	if (g.heartBeatOverride != 0.0f) // x 100 (float) x 0.4 (double)
	{
		g.heartBeatPitch = static_cast<float>(static_cast<double>(g.heartBeatOverride * 100.0f) * 0.4);
	}
	else
	{
		g.heartBeatPitch = (value * 70.0f + 30.0f - g.heartBeatPitch) * 0.1f + g.heartBeatPitch; // eased towards
	}
	// The phase advances by pitch x 0.025 x the milliseconds per turn x 0.001, brought to <= 1
	const float rate = g.heartBeatPitch * 0.025f;
	// game_clock::MsPerTurn() (the integer exact, the product rounded)
	const auto turn = static_cast<float>(static_cast<double>(rate) * static_cast<double>(game_clock::MsPerTurn()));
	g.heartBeatPhase = turn * 0.001f + g.heartBeatPhase;
	if (g.heartBeatPhase > 1.0f)
	{
		do
		{
			g.heartBeatPhase -= 1.0f;
		} while (g.heartBeatPhase > 1.0f);
	}
	g.heartBeatPulsePrevious = g.heartBeatPulse;
	// Times the float 2 pi, cosine at full precision, 1 minus it, times 0.5
	const float angle = g.heartBeatPhase * 6.2831855f;
	g.heartBeatPulse = static_cast<float>(1.0 - std::cos(static_cast<double>(angle))) * 0.5f;
	// The local interface (openblack's only one) with a living citadel heart
	const auto& queries = Queries();
	const auto input = queries.heartBeat ? queries.heartBeat() : HeartBeatInput {};
	if (!input.citadelHeart)
	{
		return;
	}
	const uint32_t owner = LocalPlayer();
	const auto pitch = static_cast<int>(g.heartBeatPitch); // truncated
	const auto guidanceBank = g.sample.bank;
	g.sample.bank = Bank(SfxBank::InGame);
	g.options.loops = -1;
	g.options.mode = 2;
	PlaySample(true, k_HeartBeatSample, owner, static_cast<int>(Type::HeartBeat), 127, pitch, 90, input.citadelHeart,
	           k_HeartBeatMaxDistance, false);
	g.sample.bank = guidanceBank; // the Guidance bank again
	g.options.loops = 0;
	SetPitch(Bank(SfxBank::InGame), PlayerOwner(owner), k_HeartBeatSample, pitch); // the playing beat follows the pitch
}

void guidance::StopHeartBeat()
{
	StopSoundEffect(k_HeartBeatSample, PlayerOwner(LocalPlayer()), SfxBank::InGame);
	GuidanceData().heartBeatPlaying = false;
}

float guidance::HeartBeatPulse(float turnFraction)
{
	auto& state = GuidanceData();
	return (state.heartBeatPulse - state.heartBeatPulsePrevious) * turnFraction + state.heartBeatPulsePrevious;
}

void guidance::WarnCreatureUnderAttack()
{
	if (GuidanceData().enabled && CanPlaySpiritRemark(Type::CreatureBeingAttacked))
	{
		SayFromList(Type::CreatureBeingAttacked, 1);
	}
}

void guidance::RemarkCreatureAttacking()
{
	if (GuidanceData().enabled && CanPlaySpiritRemark(Type::CreatureAttackingThem))
	{
		SayFromList(Type::CreatureAttackingThem, 2);
	}
}

void guidance::WarnLosingVillagers(glm::vec3 villager)
{
	if (CanPlaySpiritRemark(Type::LosingVillagers) && NearHand(villager))
	{
		SayFromList(Type::LosingVillagers, 3);
	}
}

void guidance::RemarkAttackingTown(bool townOfLocalPlayer, uint32_t population)
{
	if (townOfLocalPlayer)
	{
		return;
	}
	if (CanPlaySpiritRemark(Type::AttackingTown) && population != 0)
	{
		SayFromList(Type::AttackingTown, 4);
	}
}

void guidance::WarnLowOnFood(const HelpTown& town, float value)
{
	if (CanPlaySpiritRemark(Type::LowOnFood) && town.population != 0 && town.storagePitFunctional && NearHand(town.position))
	{
		SpiritSay(RandomSampleForValue(5, value), Type::LowOnFood);
	}
}

void guidance::WarnLowOnWood(const HelpTown& town, float value)
{
	if (CanPlaySpiritRemark(Type::LowOnWood) && town.population != 0 && town.storagePitFunctional && NearHand(town.position))
	{
		SpiritSay(RandomSampleForValue(6, value), Type::LowOnWood);
	}
}

void guidance::WarnInjuredPeople(const HelpTown& town)
{
	if (CanPlaySpiritRemark(Type::InjuredPeople) && town.population != 0 && NearHand(town.position))
	{
		SayFromList(Type::InjuredPeople, 7);
	}
}

void guidance::WarnLowOnPeople(const HelpTown& town)
{
	if (CanPlaySpiritRemark(Type::LowOnPeople) && town.population != 0 && NearHand(town.position))
	{
		SayFromList(Type::LowOnPeople, 8);
	}
}

void guidance::WarnVillagersUnhappy(const HelpTown& town)
{
	if (CanPlaySpiritRemark(Type::VillagersUnhappy) && town.population != 0 && town.storagePitFunctional &&
	    NearHand(town.position))
	{
		SayFromList(Type::VillagersUnhappy, 9);
	}
}

void guidance::WarnLosingBelief(uint32_t population)
{
	if (CanPlaySpiritRemark(Type::LosingBelief) && population != 0)
	{
		SayFromList(Type::LosingBelief, 10);
	}
}

void guidance::RemarkOtherVillages(uint32_t population)
{
	if (CanPlaySpiritRemark(Type::OtherVillages) && population != 0)
	{
		SayFromList(Type::OtherVillages, 11);
	}
}

void guidance::RemarkCreatureFight()
{
	if (CanPlaySpiritRemark(Type::CreatureFight))
	{
		SayFromList(Type::CreatureFight, 12);
	}
}

void guidance::RemarkGeneralBad(glm::vec3 point)
{
	const auto& queries = Queries();
	if (CanPlaySpiritRemark(Type::GeneralBad) && queries.pointOnScreen && queries.pointOnScreen(point))
	{
		SayFromList(Type::GeneralBad, 13);
	}
}

void guidance::RemarkGeneralGood(glm::vec3 point)
{
	const auto& queries = Queries();
	if (CanPlaySpiritRemark(Type::GeneralGood) && queries.pointOnScreen && queries.pointOnScreen(point))
	{
		SayFromList(Type::GeneralGood, 14);
	}
}

void guidance::RemarkKillingPeople(bool onScreen)
{
	if (CanPlaySpiritRemark(Type::KillingPeople) && onScreen)
	{
		SayFromList(Type::KillingPeople, 15);
	}
}

void guidance::UpdateAlignmentRemarks(float change, float alignment, float maxChangePerTurn)
{
	auto& g = GuidanceData();
	// The change decays by 0.95 a turn
	g.alignmentChange = 0.95f * g.alignmentChange + change;
	// Only past 2 x the player's maximum change a turn
	if (!(2.0f * maxChangePerTurn < std::abs(g.alignmentChange)))
	{
		return;
	}
	// Moving further the same way, or turning back
	const bool same = alignment * g.alignmentChange > 0.0f;
	const float threshold = same ? 0.75f : 0.4f;
	if (!(threshold < std::abs(alignment)))
	{
		return;
	}
	const bool good = alignment > 0.0f;
	if (same)
	{
		// VeryEvil (type 29, list 20) when good, VeryGood (type 28, list 19) when not
		const auto type = good ? Type::VeryEvil : Type::VeryGood;
		if (CanPlaySpiritRemark(type))
		{
			SayFromList(type, good ? 20 : 19);
		}
		return;
	}
	// Type 26 - good (25 / 26), list 17 - good (16 / 17)
	const auto type = good ? Type::GoodBeingEvil : Type::EvilBeingGood;
	if (CanPlaySpiritRemark(type))
	{
		SayFromList(type, good ? 16 : 17);
	}
}

void guidance::WarnWorshippersDying()
{
	if (CanPlaySpiritRemark(Type::WorshippersDying))
	{
		SayFromList(Type::WorshippersDying, 18);
	}
}

void guidance::RemarkBuildingDestroyed(bool onScreen)
{
	if (CanPlaySpiritRemark(Type::DestroyBuilding) && onScreen)
	{
		SayFromList(Type::DestroyBuilding, 21);
	}
}
