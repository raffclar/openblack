/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FightProviders.h"

#include <algorithm>
#include <ranges>
#include <set>
#include <string>
#include <utility>

#include "Debug/FightAutopilotInterface.h"
#include "ECS/Systems/FightTranscriptSystemInterface.h"
#include "ECS/Systems/InspectorSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::inspector;

namespace
{
/// A transcript answer gives this many records unless asked for more; ask again from "next" for the rest. An answer is
/// cut at the inspector's size cap (max_bytes), so a larger page needs a larger cap.
constexpr size_t k_DefaultRecords = 30;
constexpr size_t k_MostRecords = 5000;
constexpr size_t k_DefaultLogEntries = 60;

QueryDescription Query(std::string name, std::string description, std::vector<ParameterDescription> parameters = {},
                       bool writes = false)
{
	return {.name = std::move(name),
	        .description = std::move(description),
	        .parameters = std::move(parameters),
	        .kind = ResultKind::Object,
	        .needsNear = false,
	        .writes = writes};
}

template <typename T>
std::optional<T> Param(const Json& params, std::string_view name)
{
	if (!params.is_object())
	{
		return std::nullopt;
	}
	const auto found = params.find(name);
	if (found == params.end() || found->is_null())
	{
		return std::nullopt;
	}
	try
	{
		return found->get<T>();
	}
	catch (const Json::exception&)
	{
		return std::nullopt;
	}
}

fight_autopilot::AutopilotInterface* Autopilot()
{
	return Locator::inspector::has_value() ? Locator::inspector::value().GetFightAutopilot() : nullptr;
}

QueryResult NoAutopilot()
{
	return QueryResult::Error("there is no fight autopilot: it runs only in a game with the inspector");
}

QueryResult NoTranscript()
{
	return QueryResult::Error("there is no fight transcript: it is kept only while the inspector runs");
}

Json StatusJson(const fight_autopilot::Status& status)
{
	Json result {{"active", status.active},
	             {"policy", fight_autopilot::Name(status.policy)},
	             {"stage", fight_autopilot::Name(status.stage)},
	             {"phase", status.phase},
	             {"presses", status.presses},
	             {"accepted", status.accepted},
	             {"blows_landed", status.landed},
	             {"blows_blocked", status.blocked},
	             {"blows_missed", status.missed},
	             {"fight", status.fight.has_value() ? Json(*status.fight) : Json(nullptr)}};
	if (status.lesson.has_value())
	{
		const auto index = [](const std::optional<uint64_t>& value) {
			return value.has_value() ? Json(*value) : Json(nullptr);
		};
		result["lesson"] = {{"passed", status.lesson->InOrder()},
		                    {"hit", index(status.lesson->hit)},
		                    {"hits", index(status.lesson->hits)},
		                    {"block", index(status.lesson->block)},
		                    {"step", index(status.lesson->step)}};
	}
	return result;
}
} // namespace

std::unique_ptr<ProviderInterface> openblack::inspector::MakeFightTranscriptProvider()
{
	auto provider = std::make_unique<FunctionProvider>("fights");
	provider->Add(Query("transcripts",
	                    "The creature fights that the transcript recorded: for each, its number, the two creatures, when it "
	                    "started and ended, and its number of records; with the version of the records and the folder "
	                    "that the files go to"),
	              [](const QueryContext& /*context*/) {
		              if (!Locator::fightTranscript::has_value())
		              {
			              return NoTranscript();
		              }
		              const auto& transcript = Locator::fightTranscript::value();
		              Json fights = Json::array();
		              for (const auto& fight : transcript.GetRecorder().Fights())
		              {
			              fights.push_back(Json::parse(fight_transcript::SummaryJson(fight)));
		              }
		              const auto folder = transcript.GetOutputFolder();
		              return QueryResult::Value(
		                  {{"v", fight_transcript::k_Version},
		                   {"fights", std::move(fights)},
		                   {"folder", folder.has_value() ? Json(folder->generic_string()) : Json(nullptr)}});
	              });
	provider->Add(
	    Query("transcript",
	          "The records of one fight in order, from an index onward. Each record has the version (v), the fight, its "
	          "index (i), the frame, the game turn, the game time, its kind, the index of the record that caused it, "
	          "and its fields. Poll it while a fight runs: ask again from \"next\".",
	          {{.name = "fight", .type = "integer", .description = "The number of the fight; the last fight when not given"},
	           {.name = "from", .type = "integer", .description = "The first index to give, 0 when not given"},
	           {.name = "records",
	            .type = "integer",
	            .description = "The most records to give, 30 when not given; raise max_bytes for more"},
	           {.name = "kinds",
	            .type = "string",
	            .description = "Only these kinds of record, divided by commas (for example \"blow,level,press\")"}}),
	    [](const QueryContext& context) {
		    if (!Locator::fightTranscript::has_value())
		    {
			    return NoTranscript();
		    }
		    const auto& recorder = Locator::fightTranscript::value().GetRecorder();
		    const auto fights = recorder.Fights();
		    if (fights.empty())
		    {
			    return QueryResult::Error("no fight is recorded yet");
		    }
		    const auto number = Param<uint32_t>(context.params, "fight").value_or(fights.back().id);
		    const auto* fight = recorder.Find(number);
		    if (fight == nullptr)
		    {
			    return QueryResult::Error("no fight " + std::to_string(number) + "; ask fights.transcripts for them");
		    }
		    const auto from = Param<uint64_t>(context.params, "from").value_or(0);
		    const auto limit =
		        std::min<size_t>(Param<size_t>(context.params, "records").value_or(k_DefaultRecords), k_MostRecords);
		    std::set<std::string, std::less<>> kinds;
		    if (const auto text = Param<std::string>(context.params, "kinds"); text.has_value())
		    {
			    for (const auto part : *text | std::views::split(','))
			    {
				    kinds.emplace(part.begin(), part.end());
			    }
		    }
		    Json records = Json::array();
		    auto next = from;
		    for (; next < fight->entries.size() && records.size() < limit; ++next)
		    {
			    const auto& entry = fight->entries.at(next);
			    if (kinds.empty() || kinds.contains(entry.kind))
			    {
				    records.push_back(Json::parse(fight_transcript::ToJsonLine(*fight, entry)));
			    }
		    }
		    auto summary = Json::parse(fight_transcript::SummaryJson(*fight));
		    summary["from"] = from;
		    summary["next"] = next;
		    summary["complete"] = next >= fight->entries.size();
		    summary["entries"] = std::move(records);
		    return QueryResult::Value(std::move(summary));
	    });
	provider->Add(
	    Query("clear_transcripts", "Removes every fight from the transcript. New fights continue the numbers.", {}, true),
	    [](const QueryContext& /*context*/) {
		    if (!Locator::fightTranscript::has_value())
		    {
			    return NoTranscript();
		    }
		    Locator::fightTranscript::value().Clear();
		    return QueryResult::Value({{"cleared", true}});
	    });
	provider->Add(Query("transcript_folder",
	                    "Writes each fight from now on to a file of JSON lines, fight-<number>.jsonl, in a folder; an empty "
	                    "folder stops the files",
	                    {{.name = "folder", .type = "string", .description = "The folder, for example E:/openblack/fights"}},
	                    true),
	              [](const QueryContext& context) {
		              if (!Locator::fightTranscript::has_value())
		              {
			              return NoTranscript();
		              }
		              auto& transcript = Locator::fightTranscript::value();
		              const auto folder = Param<std::string>(context.params, "folder").value_or("");
		              transcript.SetOutputFolder(folder.empty() ? std::nullopt : std::optional(std::filesystem::path(folder)));
		              const auto now = transcript.GetOutputFolder();
		              return QueryResult::Value({{"folder", now.has_value() ? Json(now->generic_string()) : Json(nullptr)}});
	              });
	return provider;
}

std::unique_ptr<ProviderInterface> openblack::inspector::MakeFightAutopilotProvider()
{
	auto provider = std::make_unique<FunctionProvider>("autopilot");
	provider->Add(
	    Query(
	        "start",
	        "Starts the fight autopilot, which fights for the player with the real mouse each frame: hits on the "
	        "opponent's body, a block on the player's creature, steps on the arena's ground, and a double click into "
	        "the arena to show the fight",
	        {{.name = "policy",
	          .type = "string",
	          .description = "lesson (the guide's fight lesson, stage by stage), aggressive (hits only), random (a seeded "
	                         "mix) or sequence (the moves given)",
	          .required = true},
	         {.name = "seed", .type = "integer", .description = "The seed of the random choices, 1 when not given"},
	         {.name = "stage", .type = "string", .description = "The lesson stage to start at: hit, hits, block, step or bout"},
	         {.name = "sequence",
	          .type = "string",
	          .description = "For the sequence policy: seconds:move pairs divided by commas, for example "
	                         "\"1:high,2.5:block,4:step_back\"; moves high, middle, low, block, step_forward, "
	                         "step_back, step_left, step_right"},
	         {.name = "enter_view",
	          .type = "boolean",
	          .description = "Whether it shows the fight with a double click when the camera does not; true when not "
	                         "given"}},
	        true),
	    [](const QueryContext& context) {
		    auto* autopilot = Autopilot();
		    if (autopilot == nullptr)
		    {
			    return NoAutopilot();
		    }
		    const auto policyName = Param<std::string>(context.params, "policy").value_or("");
		    const auto policy = fight_autopilot::PolicyFromName(policyName);
		    if (!policy.has_value())
		    {
			    return QueryResult::Error("no policy " + policyName + ": use lesson, aggressive, random or sequence");
		    }
		    fight_autopilot::Settings settings {.policy = *policy,
		                                        .seed = Param<uint32_t>(context.params, "seed").value_or(1),
		                                        .stage = std::nullopt,
		                                        .sequence = {},
		                                        .enterView = Param<bool>(context.params, "enter_view").value_or(true)};
		    if (const auto stage = Param<std::string>(context.params, "stage"); stage.has_value())
		    {
			    settings.stage = fight_autopilot::LessonStageFromName(*stage);
			    if (!settings.stage.has_value())
			    {
				    return QueryResult::Error("no stage " + *stage + ": use hit, hits, block, step or bout");
			    }
		    }
		    if (const auto text = Param<std::string>(context.params, "sequence"); text.has_value())
		    {
			    const auto sequence = fight_autopilot::ParseSequence(*text);
			    if (!sequence.has_value())
			    {
				    return QueryResult::Error("the sequence does not read: use seconds:move pairs divided by commas");
			    }
			    settings.sequence = *sequence;
		    }
		    if (const auto error = autopilot->Start(std::move(settings)); !error.empty())
		    {
			    return QueryResult::Error(error);
		    }
		    return QueryResult::Value(StatusJson(autopilot->GetStatus()));
	    });
	provider->Add(Query("stop", "Stops the fight autopilot and gives the mouse back", {}, true),
	              [](const QueryContext& /*context*/) {
		              auto* autopilot = Autopilot();
		              if (autopilot == nullptr)
		              {
			              return NoAutopilot();
		              }
		              autopilot->Stop("asked");
		              return QueryResult::Value(StatusJson(autopilot->GetStatus()));
	              });
	provider->Add(Query("state",
	                    "The fight autopilot: whether it runs, its policy, the lesson stage, what the hand does, its counts "
	                    "of presses and blows, the fight it plays and the check of the lesson"),
	              [](const QueryContext& /*context*/) {
		              const auto* autopilot = Autopilot();
		              return autopilot != nullptr ? QueryResult::Value(StatusJson(autopilot->GetStatus())) : NoAutopilot();
	              });
	provider->Add(
	    Query("log",
	          "Each input that the fight autopilot gave and each result that it saw, from an index onward: the frame, the "
	          "game turn, the game time since it started, the event, the detail and the pointer's pixel",
	          {{.name = "from", .type = "integer", .description = "The first index to give, 0 when not given"},
	           {.name = "entries",
	            .type = "integer",
	            .description = "The most entries to give, 60 when not given; raise max_bytes for more"}}),
	    [](const QueryContext& context) {
		    const auto* autopilot = Autopilot();
		    if (autopilot == nullptr)
		    {
			    return NoAutopilot();
		    }
		    const auto log = autopilot->GetLog();
		    const auto from = Param<size_t>(context.params, "from").value_or(0);
		    const auto limit =
		        std::min<size_t>(Param<size_t>(context.params, "entries").value_or(k_DefaultLogEntries), k_MostRecords);
		    Json entries = Json::array();
		    auto next = from;
		    for (; next < log.size() && entries.size() < limit; ++next)
		    {
			    const auto& entry = log[next];
			    Json item {{"i", next},
			               {"frame", entry.frame},
			               {"turn", entry.turn},
			               {"elapsed_ms", entry.elapsedMs},
			               {"event", entry.event},
			               {"detail", entry.detail}};
			    if (entry.pointer.has_value())
			    {
				    item["pointer"] = {entry.pointer->first, entry.pointer->second};
			    }
			    entries.push_back(std::move(item));
		    }
		    return QueryResult::Value({{"from", from}, {"next", next}, {"entries", std::move(entries)}});
	    });
	return provider;
}
