/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cctype>

#include <algorithm>
#include <ranges>
#include <string>
#include <utility>

#include <LHVM.h>

#include "CHLApi.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Registry.h"
#include "ECS/Systems/BoatLaunchSystemInterface.h"
#include "ECS/Systems/ScriptHighlightSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "GameProviderShared.h"
#include "Locator.h"
#include "ScriptControl.h"

using namespace openblack;
using namespace openblack::inspector;
using namespace openblack::ecs::components;
using namespace openblack::inspector::game_providers;

// The scripts

namespace openblack::inspector::game_providers
{

std::unique_ptr<ProviderInterface> ScriptProvider(ScriptTargetInterface& scripts)
{
	auto provider = std::make_unique<FunctionProvider>("script");
	provider->Add(Query("vm", "The script machine: its scripts, tasks, globals and the task running"),
	              Serve<Locator::vm>("the script machine", [](const lhvm::LHVM& vm, const QueryContext& /*c*/) {
		              return Json {{"scripts", vm.GetScripts().size()},
		                           {"tasks", vm.GetTasks().size()},
		                           {"variables", vm.GetVariables().size()},
		                           {"instructions", vm.GetInstructions().size()},
		                           {"current_task", vm.GetCurrentTaskNumber()}};
	              }));
	provider->Add(
	    Query("tasks",
	          "The script tasks running: their scripts, where they are, whether they sleep and how many values are on their "
	          "stack; with a name, only the tasks whose names hold it (any case), each with its local variables",
	          {{.name = "name",
	            .type = "string",
	            .description = "Part of the task's name: only these tasks, with their local variables",
	            .required = false}},
	          ResultKind::List),
	    Serve<Locator::vm>("the script machine", [](const lhvm::LHVM& vm, const QueryContext& context) {
		    const auto upper = [](std::string text) {
			    std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
			    return text;
		    };
		    const bool filtered = context.params.is_object() && context.params.contains("name");
		    const auto filter = filtered ? upper(context.params.value("name", std::string())) : std::string();
		    Json items = Json::array();
		    for (const auto& [number, task] : vm.GetTasks())
		    {
			    if (filtered && upper(task.name).find(filter) == std::string::npos)
			    {
				    continue;
			    }
			    Json item {{"id", number},
			               {"name", task.name},
			               {"script", task.scriptId},
			               {"address", task.instructionAddress},
			               {"sleeping", task.sleeping},
			               {"waiting_for", task.waitingTaskId},
			               {"stack", task.stack.count},
			               {"held", vm.IsTaskHeld(number)}};
			    if (filtered)
			    {
				    // Each local variable by its type: a number, a truth, or the object's number in the script table
				    Json locals = Json::array();
				    for (const auto& variable : task.localVars)
				    {
					    Json value;
					    switch (variable.type)
					    {
					    case lhvm::DataType::Int:
						    value = variable.value.intVal;
						    break;
					    case lhvm::DataType::Object:
						    value = variable.value.uintVal;
						    break;
					    case lhvm::DataType::Boolean:
						    value = variable.value.floatVal != 0.0f;
						    break;
					    default:
						    value = variable.value.floatVal;
						    break;
					    }
					    locals.push_back({{"name", variable.name},
					                      {"type", static_cast<uint32_t>(variable.type)},
					                      {"value", std::move(value)}});
				    }
				    item["locals"] = std::move(locals);
			    }
			    items.push_back(std::move(item));
		    }
		    return items;
	    }));
	provider->Add(
	    Query("natives", "The script natives called that openblack hasn't written, most called first", {}, ResultKind::List),
	    Serve<Locator::chlapi>("the script natives", [](const chlapi::CHLApi& api, const QueryContext& /*c*/) {
		    auto entries = api.GetStubCalls().Entries();
		    std::ranges::stable_sort(entries, [](const auto& a, const auto& b) { return a.calls > b.calls; });
		    Json items = Json::array();
		    for (const auto& entry : entries)
		    {
			    items.push_back({{"native", entry.native},
			                     {"detail", entry.detail.has_value() ? Json(*entry.detail) : Json(nullptr)},
			                     {"calls", entry.calls}});
		    }
		    return items;
	    }));
	provider->Add(
	    Query("objects",
	          "How many things scripts hold and control, and the scripts' object table: places taken (of 511), times it "
	          "was full, and its places by kind (taken, referenced, made by a script, object gone); places: true lists "
	          "every taken place",
	          {{.name = "places", .type = "boolean", .description = "List every taken place", .required = false}}),
	    ServeRegistry([](const ecs::Registry& registry, const QueryContext& context) {
		    Json result {{"in_script", CountOf<InScript>(registry)},
		                 {"script_controlled", CountOf<ScriptControlled>(registry)},
		                 {"system", Locator::scriptObjects::has_value()}};
		    if (!Locator::scriptObjects::has_value())
		    {
			    return result;
		    }
		    const auto& scripts = Locator::scriptObjects::value();
		    const auto places = scripts.Places();
		    const bool listed = context.params.is_object() && context.params.value("places", false);
		    Json byKind = Json::object();
		    Json list = Json::array();
		    for (const auto& place : places)
		    {
			    const bool gone = place.object == entt::null || !registry.Valid(place.object);
			    const auto kind = gone ? std::string("gone") : Describe(registry, place.object, Info()).kind;
			    auto& counts = byKind[kind];
			    if (counts.is_null())
			    {
				    counts = {{"places", 0}, {"referenced", 0}, {"made_by_script", 0}};
			    }
			    counts["places"] = counts["places"].get<int>() + 1;
			    counts["referenced"] = counts["referenced"].get<int>() + (place.references != 0 ? 1 : 0);
			    counts["made_by_script"] = counts["made_by_script"].get<int>() + (place.createdByScript ? 1 : 0);
			    if (listed)
			    {
				    list.push_back({{"place", place.place},
				                    {"id", gone ? Json(nullptr) : Json(ToId(place.object))},
				                    {"kind", kind},
				                    {"references", place.references},
				                    {"made_by_script", place.createdByScript}});
			    }
		    }
		    result["places_taken"] = places.size();
		    result["times_full"] = scripts.TimesFull();
		    result["by_kind"] = std::move(byKind);
		    if (listed)
		    {
			    result["places"] = std::move(list);
		    }
		    return result;
	    }));
	provider->Add(
	    Query("highlights",
	          "The scrolls and signs scripts put up: kind, challenge or text, started, height, where drawn; the beat and "
	          "the switch that shows the scrolls",
	          {}, ResultKind::List),
	    Serve<Locator::scriptHighlightSystem>(
	        "the script highlights",
	        [](const ecs::systems::ScriptHighlightSystemInterface& highlights, const QueryContext& /*c*/) {
		        Json items = Json::array();
		        const auto* registry = Registry();
		        if (registry == nullptr)
		        {
			        return items;
		        }
		        const bool scrollsDrawn = !Locator::chlapi::has_value() || Locator::chlapi::value().IsHighlightDrawOn();
		        const auto& pulse = highlights.GetPulse();
		        registry->Each<const ScriptHighlight>([&](entt::entity entity, const ScriptHighlight& highlight) {
			        auto item = Listed(*registry, entity);
			        item["kind"] = static_cast<int>(highlight.kind);
			        item["script_id"] = highlight.scriptId;
			        item["category"] = highlight.category;
			        item["active"] = highlight.active;
			        item["draw_height"] = highlight.drawHeight.has_value() ? Json(*highlight.drawHeight) : Json(nullptr);
			        item["height_above"] = highlight.heightAbove;
			        item["y_angle"] = highlight.yAngle;
			        item["centre"] = Point(highlight.centre);
			        item["radius"] = highlight.radius;
			        item["glints"] = highlight.glints;
			        item["active_effect"] = highlight.activeEffect;
			        item["hidden"] = highlight.hidden;
			        item["scrolls_drawn"] = scrollsDrawn;
			        item["pulse"] = pulse.level;
			        items.push_back(std::move(item));
		        });
		        return items;
	        }));
	AddScriptControls(*provider, scripts);
	return provider;
}

} // namespace openblack::inspector::game_providers
