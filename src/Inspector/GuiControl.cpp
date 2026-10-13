/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GuiControl.h"

#include <utility>

using namespace openblack::inspector;

namespace
{

ParameterDescription Required(std::string name, std::string type, std::string description)
{
	return {.name = std::move(name), .type = std::move(type), .description = std::move(description), .required = true};
}

QueryDescription Write(std::string name, std::string description, std::vector<ParameterDescription> parameters)
{
	return {.name = std::move(name),
	        .description = std::move(description),
	        .parameters = std::move(parameters),
	        .kind = ResultKind::Object,
	        .needsNear = false,
	        .writes = true};
}

Json WindowJson(const WindowInfo& window)
{
	Json json = {{"name", window.name}, {"kind", window.kind}, {"open", window.open}};
	if (!window.page.empty())
	{
		json["page"] = window.page;
	}
	if (!window.buttons.empty())
	{
		json["buttons"] = window.buttons;
	}
	if (!window.selected.empty())
	{
		json["selected"] = window.selected;
	}
	return json;
}

Json OverlaysJson(const OverlayInfo& overlays)
{
	return {{"names", overlays.names},        {"details", overlays.details},
	        {"name_fade", overlays.nameFade}, {"details_for_one", overlays.detailsForOne},
	        {"labels", overlays.labels},      {"keys_act", overlays.keysAct},
	        {"hidden_by", overlays.hiddenBy}};
}

/// The window's state after a change, by its name
Json After(const GuiTargetInterface& gui, std::string_view name)
{
	for (const auto& window : gui.Windows())
	{
		if (window.name == name)
		{
			return WindowJson(window);
		}
	}
	return {{"name", name}};
}

} // namespace

std::unique_ptr<ProviderInterface> openblack::inspector::MakeGuiProvider(GuiTargetInterface& gui)
{
	auto provider = std::make_unique<FunctionProvider>("gui");
	provider->Add({.name = "windows",
	               .description = "The debug windows and the game's own (its menu, the start-of-game question skip_box, "
	                              "the tattoo_editor, the Creature Cave): name, kind, open; the menu's page (or question), "
	                              "the controls of the game's dialogs that can be pressed by name, and the skip box's "
	                              "answer picked",
	               .parameters = {},
	               .kind = ResultKind::List,
	               .needsNear = false},
	              [&gui](const QueryContext& /*context*/) {
		              Json items = Json::array();
		              for (const auto& window : gui.Windows())
		              {
			              items.push_back(WindowJson(window));
		              }
		              return QueryResult::Value(std::move(items));
	              });
	const auto openOrClose = [&gui](bool open) {
		return [&gui, open](const QueryContext& context) {
			const auto name = StringMember(context.params, "window").value_or("");
			if (auto why = open ? gui.Open(name) : gui.Close(name); !why.empty())
			{
				return QueryResult::Error(why);
			}
			return QueryResult::Value(After(gui, name));
		};
	};
	provider->Add(
	    Write("open", "Opens a window by its name, from gui.windows", {Required("window", "string", "The window's name")}),
	    openOrClose(true));
	provider->Add(Write("close",
	                    "Closes a window by its name. The start-of-game question (skip_box) closes as the player closes "
	                    "it, by answering it: its OK is pressed through the game's input, keeping the answer picked",
	                    {Required("window", "string", "The window's name")}),
	              openOrClose(false));
	provider->Add(
	    Write("press",
	          "Presses a button of a window by its label, as a click on it would. The game's menu: any control of the page "
	          "it shows (buttons, check boxes, sliders in their middle, tabs) or of the question it asks, as gui.windows "
	          "lists them; path [n] for the n-th of a name. The start-of-game question (skip_box): OK, or an answer's "
	          "radio button by its answer or by normal, creature, story, old or cow. The tattoo_editor: OK or Cancel. The "
	          "game's dialogs are clicked through the game's input, in order behind any input still to come. A debug "
	          "window's button inside a table row or another scope its window pushed needs path: the labels and numbers "
	          "leading to it. The press is made at the window's next frame; read what it changed after",
	          {Required("window", "string", "The window's name"),
	           Required("button", "string", "The button's label"),
	           {.name = "path",
	            .type = "array",
	            .description = "The scopes before the button: strings and numbers, e.g. [\"Creatures\", 3]",
	            .required = false}}),
	    [&gui](const QueryContext& context) {
		    const auto window = StringMember(context.params, "window").value_or("");
		    const auto button = StringMember(context.params, "button");
		    if (!button.has_value() || button->empty())
		    {
			    return QueryResult::Error("gui.press needs the button's label");
		    }
		    std::vector<ButtonPathStep> path;
		    if (const auto it = context.params.find("path"); it != context.params.end())
		    {
			    if (!it->is_array())
			    {
				    return QueryResult::Error("path is a list of labels and numbers");
			    }
			    for (const auto& step : *it)
			    {
				    if (step.is_string())
				    {
					    path.emplace_back(step.get<std::string>());
				    }
				    else if (step.is_number_integer())
				    {
					    path.emplace_back(static_cast<int32_t>(step.get<int64_t>()));
				    }
				    else
				    {
					    return QueryResult::Error("path is a list of labels and numbers");
				    }
			    }
		    }
		    path.emplace_back(*button);
		    if (auto why = gui.Press(window, path); !why.empty())
		    {
			    return QueryResult::Error(why);
		    }
		    return QueryResult::Value({{"window", window}, {"pressed", *button}});
	    });
	provider->Add({.name = "overlays",
	               .description = "The labels over the villagers' heads: names (the N key's named villagers' names), "
	                              "details (the S key's details of every villager), name_fade (the names' size, 0 to 1), "
	                              "details_for_one (villagers whose details S put up on their own), labels (how many were "
	                              "drawn last frame), keys_act (whether N and S act now) and hidden_by (what keeps every "
	                              "label off the screen now: cinema bars, temple)",
	               .parameters = {},
	               .kind = ResultKind::Object,
	               .needsNear = false},
	              [&gui](const QueryContext& /*context*/) { return QueryResult::Value(OverlaysJson(gui.Overlays())); });
	provider->Add(
	    Write("set_overlays",
	          "Puts the villagers' names (N) or every villager's details (S) up or down directly, even while "
	          "the keys don't act; they show once nothing hides them (gui.overlays hidden_by). Answers as "
	          "gui.overlays; labels counts the next drawn frame's",
	          {{.name = "names", .type = "boolean", .description = "The named villagers' names up", .required = false},
	           {.name = "details", .type = "boolean", .description = "Every villager's details up", .required = false}}),
	    [&gui](const QueryContext& context) {
		    const auto names = BoolMember(context.params, "names");
		    const auto details = BoolMember(context.params, "details");
		    if (!names.has_value() && !details.has_value())
		    {
			    return QueryResult::Error("gui.set_overlays needs names or details (true or false)");
		    }
		    if (auto why = gui.SetOverlays(names, details); !why.empty())
		    {
			    return QueryResult::Error(why);
		    }
		    return QueryResult::Value(OverlaysJson(gui.Overlays()));
	    });
	return provider;
}
