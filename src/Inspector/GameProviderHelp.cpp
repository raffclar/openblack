/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <algorithm>
#include <string>

#include "CHLApi.h"
#include "Camera/CameraZones.h"
#include "Creature/LeashOrders.h"
#include "ECS/Components/ChimneySmoke.h"
#include "ECS/Components/CreatureHelp.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Firefly.h"
#include "ECS/Components/GuidanceHeard.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/VillageLight.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/AdvisorVoiceSystemInterface.h"
#include "ECS/Systems/BoatLaunchSystemInterface.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/DialogueControlSystemInterface.h"
#include "ECS/Systems/GestureEventsInterface.h"
#include "ECS/Systems/GuidanceSystemInterface.h"
#include "ECS/Systems/HandDemoSystemInterface.h"
#include "ECS/Systems/HelpMessageSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/HelpSpeechSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/IntroSystemInterface.h"
#include "ECS/Systems/TipBubbleSystemInterface.h"
#include "GameProviderShared.h"
#include "Help/AdvisorModel.h"
#include "Help/AdvisorVoices.h"
#include "Help/DialogueText.h"
#include "Help/HelpProfile.h"
#include "Help/Spirits.h"
#include "Locator.h"
#include "Nature/Fireflies.h"
#include "Online/NetworkSession.h"
#include "Sound/CustomMusicPlayerInterface.h"
#include "Sound/GameMusic.h"
#include "Sound/LoadedSound.h"
#include "Sound/SoundRequestInterface.h"

using namespace openblack;
using namespace openblack::inspector;
using namespace openblack::ecs::components;
using namespace openblack::inspector::game_providers;

// The help: the advisors and the scripts' dialogue

namespace openblack::inspector::game_providers
{

std::unique_ptr<ProviderInterface> HelpProvider()
{
	auto provider = std::make_unique<FunctionProvider>("help");
	provider->Add(
	    Query("advisors",
	          "The good and the evil advisor: what each does, where it hovers, whether it is talking and its mouth's lip sync",
	          {}, ResultKind::List),
	    Serve<Locator::advisorSystem>(
	        "the advisors", [](const ecs::systems::AdvisorSystemInterface& advisors, const QueryContext& /*c*/) -> Json {
		        if (!advisors.IsLoaded())
		        {
			        return Json::array();
		        }
		        const auto& controller = advisors.GetController();
		        const auto* voices =
		            Locator::advisorVoiceSystem::has_value() ? &Locator::advisorVoiceSystem::value().GetVoices() : nullptr;
		        Json items = Json::array();
		        for (int dude = 0; dude < 2; ++dude)
		        {
			        const auto& spirit = controller.Dude(dude);
			        // The mouth: the lip sync's three weights and the mouth clips posed this frame (clip, ms)
			        Json shapes = Json::array();
			        for (const auto& layer : spirit.Layers())
			        {
				        if (layer.kind == help::spirits::AnimLayer::Kind::Add && layer.clip >= help::spirits::anim::k_VowelE &&
				            layer.clip < help::spirits::anim::k_VowelE + 3)
				        {
					        shapes.push_back({{"clip", layer.clip}, {"ms", layer.milliseconds}});
				        }
			        }
			        Json weights = Json::array();
			        if (voices != nullptr)
			        {
				        for (const float w : voices->GetLipSyncKey(dude).weights)
				        {
					        // Not a number after a window of pure silence, as in the game
					        weights.push_back(std::isfinite(w) ? Json(w) : Json(nullptr));
				        }
			        }
			        items.push_back({{"advisor", dude == 0 ? "good" : "evil"},
			                         {"mouth", {{"weights", weights}, {"shapes", shapes}}},
			                         {"tags_left", spirit.TagsLeft()},
			                         {"control_state", static_cast<int>(controller.State(dude))},
			                         {"state", spirit.State()},
			                         {"hover", Point(spirit.Hover())},
			                         {"position", Point(spirit.Position())},
			                         {"alpha", spirit.Alpha()},
			                         {"playing_anim", spirit.IsPlayingAnim()},
			                         {"given_line", voices != nullptr && voices->IsActive(dude)},
			                         {"speaking", voices != nullptr && voices->GetSpeaker() == dude}});
		        }
		        return items;
	        }));
	provider->Add(
	    Query("dialogue", "The scripts' dialogue: the text shown, whether it is read or waits for a click, who holds the "
	                      "dialogue and the line being said"),
	    Serve<Locator::helpTextSystem>(
	        "the dialogue", [](const ecs::systems::HelpTextSystemInterface& help, const QueryContext& /*c*/) -> Json {
		        // A computer that shows nothing plays no voices: no speaker and no line
		        const auto* voices =
		            Locator::advisorVoiceSystem::has_value() ? &Locator::advisorVoiceSystem::value().GetVoices() : nullptr;
		        Json result {{"started", help.IsStarted()},
		                     {"text_read", help.IsTextRead()},
		                     {"speaker", voices != nullptr ? voices->GetSpeaker() : -1},
		                     {"sentence", voices != nullptr ? voices->GetSentence() : 0u},
		                     {"owner", Locator::dialogueControlSystem::has_value()
		                                   ? Json(Locator::dialogueControlSystem::value().GetOwner())
		                                   : Json(nullptr)},
		                     {"speech_banks", Locator::helpSpeechSystem::has_value()
		                                          ? Json(Locator::helpSpeechSystem::value().GetTable().GetBankCount(
		                                                audio::SpeechBank::HelpSprites))
		                                          : Json(nullptr)}};
		        if (const auto* dialogue = help.GetDialogue(); dialogue != nullptr)
		        {
			        result["text"] = dialogue->GetCurrentText();
			        result["narrator"] = help.GetNarrator(dialogue->GetCurrentText());
			        result["waiting_for_click"] = dialogue->IsWaitingForClick();
			        result["end_turn"] = dialogue->GetEndTurn();
			        result["end_ms"] = dialogue->GetEndMs();
			        result["start_turn"] = dialogue->GetStartTurn();
			        result["takes_click"] = dialogue->TakesClick();
			        result["last_click_turn"] = dialogue->GetLastClickTurn();
			        result["drawn"] = dialogue->IsDrawn();
			        result["box_shown"] = dialogue->GetDisplay().IsBoxShown();
		        }
		        return result;
	        }));
	provider->Add(
	    Query("profile",
	          "The help's count of what the player has done: its clock, and each event's total, seconds since "
	          "and rate, for the events that have happened",
	          {}, ResultKind::List),
	    Serve<Locator::helpProfileSystem>(
	        "the help profile", [](const ecs::systems::HelpProfileSystemInterface& system, const QueryContext& /*c*/) -> Json {
		        const auto& profile = system.Get();
		        Json items = Json::array();
		        for (uint32_t event = 0; event < help::profile::k_EventCount; ++event)
		        {
			        // A copy: reading the times may tidy them, and a query changes nothing
			        auto count = profile.Count(event);
			        if (count.Total() == 0)
			        {
				        continue;
			        }
			        items.push_back({{"event", event},
			                         {"clock", profile.Clock()},
			                         {"total", count.Total()},
			                         {"seconds_since", count.SecondsSince(profile.Clock())},
			                         {"per_second", count.PerSecond(profile.Clock())},
			                         {"smoothed_rate", count.SmoothedRate()}});
		        }
		        return items;
	        }));
	provider->Add(Query("creature",
	                    "The advisor's messages about the player's creature: whether they are on, the ones told lately "
	                    "(kind, parameter, text, turn), and the creature's waiting ones and prompts",
	                    {}, ResultKind::Object),
	              Serve<Locator::creatureHelpSystem>(
	                  "the creature help",
	                  [](const ecs::systems::CreatureHelpSystemInterface& system, const QueryContext& /*c*/) -> Json {
		                  Json told = Json::array();
		                  for (const auto& message : system.GetTold())
		                  {
			                  told.push_back({{"creature", entt::to_integral(message.creature)},
			                                  {"kind", static_cast<int>(message.kind)},
			                                  {"parameter", message.parameter},
			                                  {"text", message.text.has_value() ? Json(*message.text) : Json(nullptr)},
			                                  {"turn", message.turn}});
		                  }
		                  Json creatures = Json::array();
		                  const auto& registry = Locator::entitiesRegistry::value();
		                  registry.Each<const CreatureHelpMemory>(
		                      [&creatures](entt::entity entity, const CreatureHelpMemory& help) {
			                      Json waiting = Json::array();
			                      for (size_t kind = 0; kind < help.memory.entries.size(); ++kind)
			                      {
				                      for (const auto& entry : help.memory.entries.at(kind))
				                      {
					                      if (entry.waiting)
					                      {
						                      waiting.push_back({{"kind", kind}, {"parameter", entry.parameter}});
					                      }
				                      }
			                      }
			                      creatures.push_back({{"creature", entt::to_integral(entity)},
			                                           {"waiting", waiting},
			                                           {"hand_over", help.memory.handOver},
			                                           {"body_problem_turn", help.memory.physicalProblemTurn},
			                                           {"tutorial_turn", help.memory.tutorialTurn},
			                                           {"tutorial_done", help.memory.tutorialDone}});
		                      });
		                  return Json {{"on", system.IsHelpOn()}, {"told", told}, {"creatures", creatures}};
	                  }));
	provider->Add(
	    Query("messages",
	          "The help the game starts by itself: the help scripts it started lately (script, texts, a "
	          "town's need) and how many times each message set has been said",
	          {}, ResultKind::Object),
	    Serve<Locator::helpMessageSystem>(
	        "the help messages", [](const ecs::systems::HelpMessageSystemInterface& system, const QueryContext& /*c*/) -> Json {
		        Json started = Json::array();
		        for (const auto& message : system.GetStarted())
		        {
			        started.push_back({{"script", message.script},
			                           {"first", message.first},
			                           {"last", message.last},
			                           {"stat_text", message.statText.has_value() ? Json(*message.statText) : Json(nullptr)},
			                           {"need", message.need.has_value() ? Json(*message.need) : Json(nullptr)},
			                           {"turn", message.turn}});
		        }
		        Json sent = Json::object();
		        const auto& counts = system.GetSentCounts();
		        for (size_t set = 0; set < counts.size(); ++set)
		        {
			        if (counts.at(set) != 0)
			        {
				        sent[std::to_string(set)] = counts.at(set);
			        }
		        }
		        return Json {{"started", started}, {"sets_said", sent}};
	        }));
	provider->Add(
	    Query("guidance",
	          "The advisors' remarks on what the player does (towns attacked, buildings wrecked, alignment drifting, the "
	          "moon): the ones asked for lately (kind, text, turn, whether the advisors got to say it), the turns until "
	          "the moon is looked at again, the player's guidance (each kind's last turn, the alignment's drift, the "
	          "one-offs said), and the night whisper (the sample of the player's name, the count of real-night checks, "
	          "the turns to the next check and the volume of the next whisper)",
	          {}, ResultKind::Object),
	    Serve<Locator::guidanceSystem>(
	        "the guidance", [](const ecs::systems::GuidanceSystemInterface& system, const QueryContext& /*c*/) -> Json {
		        Json remarks = Json::array();
		        for (const auto& remark : system.GetRemarks())
		        {
			        remarks.push_back({{"kind", static_cast<int>(remark.kind)},
			                           {"text", remark.text},
			                           {"turn", remark.turn},
			                           {"started", remark.started}});
		        }
		        Json heard = nullptr;
		        if (Locator::entitiesRegistry::has_value())
		        {
			        Locator::entitiesRegistry::value().Each<const ecs::components::GuidanceHeard>(
			            [&heard](entt::entity entity, const ecs::components::GuidanceHeard& guidance) {
				            heard = {{"player", entt::to_integral(entity)},
				                     {"last_turns", guidance.lastTurn},
				                     {"alignment_drift", guidance.alignmentDrift},
				                     {"one_offs_said", guidance.oneOffSaid}};
			            });
		        }
		        const auto& whisper = system.GetNightWhisper();
		        const Json whisperJson {
		            {"sample", whisper.GetSample().has_value() ? Json(*whisper.GetSample()) : Json(nullptr)},
		            {"count", whisper.GetCount()},
		            {"countdown", whisper.GetCountdown()},
		            {"volume", whisper.GetVolume()}};
		        return Json {{"remarks", remarks},
		                     {"moon_wait", system.GetMoonWait()},
		                     {"heard", heard},
		                     {"night_whisper", whisperJson}};
	        }));
	provider->Add(
	    Query("bubble", "The tip bubble over the \"did you know\" sign tapped last: the sign, its tip, whether it is up, "
	                    "its time to show, the point it points at, its scroll and whether the hand is on it or grips it"),
	    Serve<Locator::tipBubbleSystem>(
	        "the tip bubble", [](const ecs::systems::TipBubbleSystemInterface& bubble, const QueryContext& /*c*/) -> Json {
		        const auto& scroll = bubble.GetScroll();
		        const auto anchor = bubble.GetAnchor();
		        return Json {
		            {"sign", bubble.GetSign() == entt::null ? Json(nullptr) : Json(entt::to_integral(bubble.GetSign()))},
		            {"text", bubble.GetText()},
		            {"up", bubble.IsUp()},
		            {"display_time", bubble.GetDisplayTime()},
		            {"anchor", anchor.has_value() ? Point(*anchor) : Json(nullptr)},
		            {"scroll_lines", scroll.lines},
		            {"content_height", scroll.contentHeight},
		            {"line_height", scroll.lineHeight},
		            {"more_above", scroll.moreAbove},
		            {"more_below", scroll.moreBelow},
		            {"under_hand", bubble.IsUnderHand()},
		            {"gripped", bubble.IsGripped()}};
	        }));
	provider->Add(Query("hand_demo",
	                    "The tutorial's hand demonstration playing: its name, the script task that started it, the record "
	                    "the turns are at, whether it holds at a mark, the record the frames are at, and the camera and "
	                    "hints it has"),
	              Serve<Locator::handDemoSystem>(
	                  "the hand demonstrations",
	                  [](const ecs::systems::HandDemoSystemInterface& demos, const QueryContext& /*c*/) -> Json {
		                  const auto status = demos.GetStatus();
		                  if (!status.has_value())
		                  {
			                  return Json {{"playing", false}};
		                  }
		                  Json result {{"playing", true},
		                               {"name", status->name},
		                               {"task", status->task},
		                               {"record", status->record},
		                               {"records", status->records},
		                               {"pause_on_trigger", status->pauseOnTrigger},
		                               {"trigger_reached", status->triggerReached},
		                               {"holding", status->holding},
		                               {"replaying", status->replaying},
		                               {"replay_record", status->replayRecord},
		                               {"replay_holding", status->replayHolding},
		                               {"hints", demos.GetHints()}};
		                  if (const auto camera = demos.GetCamera(); camera.has_value())
		                  {
			                  result["camera_origin"] = Point(camera->origin);
			                  result["camera_focus"] = Point(camera->focus);
		                  }
		                  return result;
	                  }));
	return provider;
}

} // namespace openblack::inspector::game_providers
