/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LanternSounds.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <limits>
#include <map>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "Audio/Audio.h"
#include "Audio/Game/AudioSystem.h"
#include "Audio/GameQueries.h"
#include "ECS/Systems/AudioStateInterface.h"
#include "Locator.h"

// A caller of the audio core (the lanterns are ECS things): the lanterns come from GameQueries::streetLanterns, so this
// file reads no ECS component either.

using namespace openblack;
using namespace openblack::audio;

namespace
{
/// LH_SAMPLE_G_LANTERN_01 (InGame.sad 147)
constexpr int k_LanternSample = 0x93;

/// What this module keeps between calls (Locator::audioState)
struct LanternSoundsState
{
	/// Dark enough for the lanterns to be heard
	bool on {false};
	/// The sound tag of each lantern. A map in this state, not a component on each lantern: the tags are switched on and
	/// off in lantern order every turn, the entries of lanterns that went are dropped only when their tag is gone, and a
	/// land load clears them all with the tags
	std::map<entt::entity, tags::TagId> tags {};
	/// The trace's turn counter
	uint32_t turn {0};
};

LanternSoundsState& LanternSoundsData()
{
	return openblack::Locator::audioState::value().Get<LanternSoundsState>();
}

bool Trace()
{
	static const bool k_Trace = std::getenv("OPENBLACK_LANTERN_SOUND_TRACE") != nullptr;
	return k_Trace;
}
} // namespace

void lantern_sounds::SetOn(bool on)
{
	auto& state = LanternSoundsData();
	// only when the flag changes does it walk the lanterns (a gone lantern is no longer in their list, so
	// its dead object's tag is not reached)
	if (on == state.on)
	{
		return;
	}
	state.on = on;
	for (const auto& [lantern, tag] : state.tags)
	{
		if (Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Lantern sound: tag {} of lantern {} {}", tag,
			                   static_cast<uint32_t>(lantern), on ? "on" : "off");
		}
		tags::SetActive(tag, on);
	}
}

void lantern_sounds::ProcessTurn()
{
	auto& state = LanternSoundsData();
	const auto& query = Queries().streetLanterns;
	if (!query)
	{
		return;
	}
	const auto lanterns = query();
	// a deleted lantern only forgets its tag and leaves the list: its tag sees the thing gone itself
	std::erase_if(state.tags, [&lanterns](const auto& entry) {
		return !tags::Exists(entry.second) ||
		       std::none_of(lanterns.begin(), lanterns.end(),
		                    [&entry](const StreetLantern& lantern) { return lantern.thing == entry.first; });
	});
	float nearest = std::numeric_limits<float>::max();
	const auto camera = ListenerPoint();
	for (const auto& lantern : lanterns)
	{
		if (camera)
		{
			nearest = std::min(nearest, glm::distance(lantern.position + glm::vec3(0.0f, lantern.height, 0.0f), *camera));
		}
		if (state.tags.contains(lantern.thing))
		{
			continue;
		}
		// as on the lantern's creation: a tag at (0, its height, 0), active when it is dark
		const auto tag = tags::Create(lantern.thing, glm::vec3(0.0f, lantern.height, 0.0f), k_LanternSample, false, 2, -1,
		                              false, true, SfxBank::InGame, 0);
		tags::SetActive(tag, state.on);
		state.tags.emplace(lantern.thing, tag);
		if (Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Lantern sound: tag {} for lantern {} (height {:.2f}, {})", tag,
			                   static_cast<uint32_t>(lantern.thing), lantern.height, state.on ? "on" : "off");
		}
	}
	if (Trace() && !lanterns.empty() && state.turn++ % 50 == 0)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Lantern sound: {} lanterns, dark {}, nearest at {:.1f} (max {:.1f})",
		                   lanterns.size(), state.on, nearest, MaxDistance({Bank(SfxBank::InGame), k_LanternSample}));
	}
}

void lantern_sounds::Clear()
{
	auto& state = LanternSoundsData();
	// the tags themselves go with the map (tags::Clear)
	state.tags.clear();
	state.on = false;
}
