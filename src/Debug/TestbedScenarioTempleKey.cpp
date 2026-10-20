/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed scenarios for the temple key and the realm key of the world camera

#include <vector>

#include "Input/BindableActions.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

void testbed_scenarios::AddTempleKeyScenarios(std::vector<Scenario>& all)
{
	using Kind = Command::Kind;
	const auto press = [](float delaySeconds, input::BindableActionMap key) {
		return Command {.kind = Kind::PressKey, .delaySeconds = delaySeconds, .value = static_cast<size_t>(key)};
	};
	constexpr auto k_Temple = input::BindableActionMap::ZOOM_TO_TEMPLE;
	constexpr auto k_Realm = input::BindableActionMap::ZOOM_TO_REALM;
	all.push_back({
	    .id = "hand.temple_key_taps",
	    .name = "The temple key: a single tap, a double tap and the flight back",
	    .facet = Facet::Hand,
	    .description = "The player's temple stands 120 metres south of the middle of the testbed. After 3 seconds the "
	                   "scenario presses the temple key (space) once. After 4 more seconds it presses the key once, "
	                   "then once more 0.2 seconds later. After 6 more seconds it presses the key twice again.",
	    .expected = "Each single tap flies the camera to a view 50 metres from the land ahead, at a pitch of 30 degrees, "
	                "with no rise. The double tap flies to the temple from 130 metres at 45 degrees, and the flight "
	                "rises at its half way point. The last double tap flies back to the view before the temple flight.",
	    .environment = {.dispenserGrid = false, .temple = glm::vec2 {0.0f, -120.0f}},
	    .framing = {.shot = Shot::Testbed},
	    .commands = {press(3.0f, k_Temple), press(4.0f, k_Temple), press(0.2f, k_Temple), press(6.0f, k_Temple),
	                 press(0.2f, k_Temple)},
	});
	all.push_back({
	    .id = "hand.realm_key_flight",
	    .name = "The realm key flies over the middle of the island and back",
	    .facet = Facet::Hand,
	    .description = "After 3 seconds the scenario presses the realm key. After 6 more seconds it presses the key again.",
	    .expected = "The first press flies the camera 2200 metres from the middle of the island, at a pitch of 22.5 "
	                "degrees. The middle is half way between the first and the last block of land. The second press "
	                "flies back to the view before.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .commands = {press(3.0f, k_Realm), press(6.0f, k_Realm)},
	});
}
