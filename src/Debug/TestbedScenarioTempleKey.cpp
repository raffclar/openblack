/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed scenarios for the keys of the world camera: the temple key, the realm key and the clear view, and the
// shake of a script

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
	all.push_back({
	    .id = "hand.clear_view",
	    .name = "Ctrl and Shift held together ease the camera to a close view round the hand",
	    .facet = Facet::Hand,
	    .description = "The camera looks at the testbed, with a temple 120 metres to the south. Put the hand "
	                   "on the land or on the temple. Then hold Ctrl and Shift together for a few seconds, and release "
	                   "them.",
	    .expected = "Over half a second the camera comes in to the point under the hand: 15 metres from it (50 when the "
	                "camera was more than 300 metres from what it looked at, 10 when it was nearer than 50), from the "
	                "same heading, and more steeply down. Over a thing, the view follows the thing, and the hand stays "
	                "on the land at the thing. The wheel does not "
	                "zoom. When you release a key, the camera eases back to where it was in half a second.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .temples = {{.offset = glm::vec2 {0.0f, -120.0f}}},
	});
	all.push_back({
	    .id = "hand.script_shake",
	    .name = "A script shakes the camera for three seconds",
	    .facet = Facet::Hand,
	    .description = "After 2 seconds the scenario runs SHAKE_CAMERA at the middle of the testbed with a radius of "
	                   "2000 metres, a strength of 2 and a time of 3 seconds.",
	    .expected = "The eye and the focus of the camera move by a new random distance each frame, along all three axes. "
	                "The shake is strongest at the start and decreases steadily to nothing after 3 seconds. It also "
	                "decreases while the game is paused.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 2.0f,
	                  .script = "SHAKE_CAMERA",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Position, .point = {0.0f, 0.0f}},
	                                      {.number = 2000.0f},
	                                      {.number = 2.0f},
	                                      {.number = 3.0f}}}},
	});
}
