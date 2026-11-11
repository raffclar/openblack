/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureMindTrees.h"

#include <cmath>

#include <algorithm>
#include <iterator>
#include <ranges>
#include <string>

#include <entt/entity/entity.hpp>
#include <fmt/format.h>

#include "Creature/CreatureMindView.h"
#include "Creature/CreatureTreeView.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::debug::gui;
using creature_desires::Desire;
using creature_mind_model::TreeKind;
using openblack::ecs::components::CreatureMindState;

namespace
{
/// The size of a node box and the space between the columns and the rows of the graph, at a zoom of 1
constexpr ImVec2 k_Box {124.0f, 34.0f};
constexpr float k_ColumnStep = 136.0f;
constexpr float k_RowStep = 78.0f;
constexpr float k_GraphHeight = 300.0f;
constexpr float k_MinZoom = 0.15f;
constexpr float k_MaxZoom = 3.0f;
/// Below this zoom the boxes show no text; hold the pointer over a box to read it
constexpr float k_TextZoom = 0.45f;
/// The changed nodes blink for this many seconds after the tree grew, and then keep a thin mark
constexpr double k_BlinkSeconds = 6.0;
constexpr ImU32 k_InnerFill = IM_COL32(45, 60, 85, 255);
constexpr ImU32 k_Line = IM_COL32(150, 150, 160, 255);
constexpr ImU32 k_PlannerPath = IM_COL32(255, 215, 60, 255);
constexpr ImU32 k_PickedPath = IM_COL32(70, 210, 255, 255);
constexpr ImU32 k_Changed = IM_COL32(255, 140, 30, 255);
constexpr ImVec4 k_PlannerText {1.0f, 0.85f, 0.25f, 1.0f};
constexpr ImVec4 k_PickedText {0.3f, 0.82f, 1.0f, 1.0f};
constexpr ImVec4 k_ChangedText {1.0f, 0.55f, 0.12f, 1.0f};

const char* KindName(TreeKind kind)
{
	return kind == TreeKind::Activity ? "Activity" : "Act on";
}

ImU32 ToImGui(creature_tree_view::Colour colour)
{
	return IM_COL32(colour.r, colour.g, colour.b, 255);
}

ImVec4 ToVec4(creature_tree_view::Colour colour)
{
	return {static_cast<float>(colour.r) / 255.0f, static_cast<float>(colour.g) / 255.0f, static_cast<float>(colour.b) / 255.0f,
	        1.0f};
}

bool Contains(const std::vector<size_t>& path, size_t node)
{
	return std::ranges::find(path, node) != path.end();
}

/// Whether the edge from a node to its child is a step of the path
bool OnPath(const std::vector<size_t>& path, size_t from, size_t to)
{
	const auto found = std::ranges::find(path, from);
	return found != path.end() && std::next(found) != path.end() && *std::next(found) == to;
}

std::string Attributes(const creature_tree::Belief& belief)
{
	std::string text;
	for (const auto attribute : creature_tree::SlotsFor(belief.type))
	{
		const auto value = belief.Value(attribute);
		text += fmt::format("{}{}={}", text.empty() ? "" : ", ", creature_tree::Name(attribute),
		                    value.has_value() ? creature_tree_view::ValueName(attribute, *value) : std::string("-"));
	}
	return text;
}

std::optional<entt::entity> EntityOf(std::optional<uint32_t> id)
{
	if (!id.has_value())
	{
		return std::nullopt;
	}
	const auto entity = static_cast<entt::entity>(*id);
	return Locator::entitiesRegistry::value().Valid(entity) ? std::optional(entity) : std::nullopt;
}

/// What the creature believes about a thing, as the tree of the kind tests it
std::optional<creature_tree::Belief> BeliefFor(entt::entity creature, entt::entity thing, TreeKind kind)
{
	if (!Locator::creatureMindSystem::has_value() || !Locator::entitiesRegistry::value().Valid(thing))
	{
		return std::nullopt;
	}
	const auto& minds = Locator::creatureMindSystem::value();
	return kind == TreeKind::ActOn ? minds.ActedOnBeliefAbout(creature, thing) : minds.BeliefAbout(creature, thing);
}
} // namespace

CreatureMindTrees::Watch& CreatureMindTrees::WatchOf(TreeKind kind, Desire desire) noexcept
{
	return _watches.at(static_cast<size_t>(kind)).at(static_cast<size_t>(desire));
}

void CreatureMindTrees::Observe(entt::entity creature, uint32_t turn) noexcept
{
	if (_observed != creature)
	{
		_observed = creature;
		_primed = false;
		_watches = {};
		_views = {};
		_desire.reset();
		_shownDesire.reset();
	}
	const auto* mind = Locator::entitiesRegistry::value().TryGet<const CreatureMindState>(creature);
	if (mind == nullptr || !mind->learnt.has_value())
	{
		return;
	}
	const auto now = ImGui::GetTime();
	for (size_t k = 0; k < creature_mind_model::k_TreeKinds; ++k)
	{
		for (size_t d = 0; d < creature_desires::k_DesireCount; ++d)
		{
			const auto& tree = mind->learnt->trees.at(k).at(d);
			const auto& episodes = mind->learnt->episodes.at(k).at(d);
			auto& watch = _watches.at(k).at(d);
			if (!_primed)
			{
				watch.tree = tree;
				watch.episodes = episodes;
				watch.seen = creature_tree_view::SeenTurns({}, {}, episodes, turn, true);
				continue;
			}
			const bool sameExamples = std::ranges::equal(watch.episodes, episodes, creature_tree_view::SameEpisode);
			const bool sameTree = creature_tree_view::SameTree(watch.tree, tree);
			if (sameExamples && sameTree)
			{
				continue;
			}
			// A tree grows again from scratch after each new example, so a change of the examples is a growth too, even
			// when the new tree is the same as the old one
			watch.changed = creature_tree_view::ChangedNodes(watch.tree, tree);
			watch.grownTurn = turn;
			watch.grownAt = now;
			watch.seen = creature_tree_view::SeenTurns(watch.episodes, watch.seen, episodes, turn, false);
			watch.tree = tree;
			watch.episodes = episodes;
		}
	}
	_primed = true;
}

void CreatureMindTrees::Draw(entt::entity creature) noexcept
{
	const auto* mind = Locator::entitiesRegistry::value().TryGet<const CreatureMindState>(creature);
	if (mind == nullptr || !mind->learnt.has_value())
	{
		ImGui::TextUnformatted("The creature has learnt nothing yet: its mind has not thought");
		return;
	}
	if (!_desire.has_value())
	{
		_desire = mind->planner.current.has_value() ? mind->planner.current->desire : Desire::Hunger;
	}
	DrawThingPicker();
	const float listWidth = 200.0f;
	if (ImGui::BeginChild("tree desires", ImVec2(listWidth, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX))
	{
		DrawDesireList(creature);
	}
	ImGui::EndChild();
	ImGui::SameLine();
	if (ImGui::BeginChild("tree view", ImVec2(0.0f, 0.0f)))
	{
		ImGui::Text("%s", std::string(creature_desires::Name(*_desire)).c_str());
		// A new desire opens on the tree with more examples
		const auto countOf = [&](TreeKind kind) {
			return mind->learnt->episodes.at(static_cast<size_t>(kind)).at(static_cast<size_t>(*_desire)).size();
		};
		std::optional<TreeKind> select;
		if (_shownDesire != _desire)
		{
			_shownDesire = _desire;
			select = countOf(TreeKind::ActOn) > countOf(TreeKind::Activity) ? TreeKind::ActOn : TreeKind::Activity;
		}
		if (ImGui::BeginTabBar("tree kinds"))
		{
			for (const auto kind : {TreeKind::Activity, TreeKind::ActOn})
			{
				const auto label = fmt::format("{} ({} examples)###{}", KindName(kind), countOf(kind), KindName(kind));
				if (ImGui::BeginTabItem(label.c_str(), nullptr,
				                        select == kind ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None))
				{
					DrawTree(creature, kind);
					ImGui::EndTabItem();
				}
			}
			ImGui::EndTabBar();
		}
	}
	ImGui::EndChild();
}

void CreatureMindTrees::DrawThingPicker() noexcept
{
	if (_thingFollowsHand && Locator::pickingSystem::has_value())
	{
		const auto& picked = Locator::pickingSystem::value().GetPick().object;
		if (picked.has_value())
		{
			_thing = *picked;
			_thingId = static_cast<int>(entt::to_integral(*picked));
		}
	}
	if (_thing.has_value() && !Locator::entitiesRegistry::value().Valid(*_thing))
	{
		_thing.reset();
	}
	ImGui::TextUnformatted("Test a thing:");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(110.0f);
	ImGui::InputInt("##thing", &_thingId);
	ImGui::SameLine();
	if (ImGui::Button("Use"))
	{
		_thing = EntityOf(static_cast<uint32_t>(std::max(_thingId, 0)));
		_thingFollowsHand = false;
	}
	ImGui::SameLine();
	if (ImGui::Button("Under the hand") && Locator::pickingSystem::has_value())
	{
		const auto& picked = Locator::pickingSystem::value().GetPick().object;
		_thing = picked;
		_thingId = picked.has_value() ? static_cast<int>(entt::to_integral(*picked)) : _thingId;
	}
	ImGui::SameLine();
	ImGui::Checkbox("Follow the hand##thing", &_thingFollowsHand);
	ImGui::SameLine();
	if (ImGui::Button("Clear"))
	{
		_thing.reset();
		_thingFollowsHand = false;
	}
	ImGui::TextDisabled("Path of the planner's thing: yellow. Path of the thing tested: blue. Changed nodes: orange.");
}

void CreatureMindTrees::DrawDesireList(entt::entity creature) noexcept
{
	const auto& mind = Locator::entitiesRegistry::value().Get<const CreatureMindState>(creature);
	const auto& learnt = *mind.learnt;
	const auto rows = creature_mind_view::ActiveDesires(*mind.desires, nullptr, 1.0f);
	const auto now = ImGui::GetTime();
	const auto item = [&](Desire desire, std::optional<float> value) {
		const auto d = static_cast<size_t>(desire);
		const auto activity = learnt.episodes.at(static_cast<size_t>(TreeKind::Activity)).at(d).size();
		const auto actOn = learnt.episodes.at(static_cast<size_t>(TreeKind::ActOn)).at(d).size();
		const bool grew = std::ranges::any_of(_watches, [d, now](const auto& kind) {
			const auto& watch = kind.at(d);
			return watch.grownTurn.has_value() && now - watch.grownAt < k_BlinkSeconds;
		});
		const auto label = fmt::format("{}{} {}/{}{}", grew ? "* " : "", creature_desires::Name(desire), activity, actOn,
		                               value.has_value() ? fmt::format(" ({:.2f})", *value) : std::string());
		const bool empty = activity == 0 && actOn == 0;
		if (empty)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		}
		if (ImGui::Selectable(fmt::format("{}##{}", label, d).c_str(), _desire == desire))
		{
			_desire = desire;
		}
		if (empty)
		{
			ImGui::PopStyleColor();
		}
	};
	ImGui::SeparatorText("Active desires");
	std::array<bool, creature_desires::k_DesireCount> listed {};
	for (const auto& row : rows)
	{
		listed.at(static_cast<size_t>(row.desire)) = true;
		item(row.desire, row.value);
	}
	ImGui::SeparatorText("Other desires");
	for (size_t d = 0; d < creature_desires::k_DesireCount; ++d)
	{
		if (!listed.at(d))
		{
			item(static_cast<Desire>(d), std::nullopt);
		}
	}
	ImGui::TextDisabled("Examples: activity / act on");
}

void CreatureMindTrees::DrawTree(entt::entity creature, TreeKind kind) noexcept
{
	const auto& mind = Locator::entitiesRegistry::value().Get<const CreatureMindState>(creature);
	const auto k = static_cast<size_t>(kind);
	const auto d = static_cast<size_t>(*_desire);
	const auto& tree = mind.learnt->trees.at(k).at(d);
	const auto& watch = WatchOf(kind, *_desire);
	Paths paths;

	// The planner's thing: the thing of the best plan of the desire. The activity tree rates the whole thing that the
	// desire is about, else the thing acted on; the act-on tree rates the thing acted on.
	const auto& best = mind.planner.best.at(d);
	if (best.has_value())
	{
		const auto id = kind == TreeKind::Activity && best->about.has_value() ? best->about : best->object;
		const auto thing = EntityOf(id);
		const auto belief = thing.has_value() ? BeliefFor(creature, *thing, kind) : std::nullopt;
		if (belief.has_value())
		{
			const auto path = creature_tree_view::PathOf(tree, *belief);
			paths.planner = path.nodes;
			const auto utility = creature_tree::Evaluate(tree, *belief);
			ImGui::TextColored(k_PlannerText, "Planner's thing: %s %u, value %+.1f, usefulness %.3f%s",
			                   creature_tree::BeliefName(belief->type), *id, static_cast<double>(utility),
			                   static_cast<double>(creature_tree::Usefulness(utility)),
			                   path.reachesLeaf ? "" : " (no leaf: no branch for it)");
		}
		else
		{
			ImGui::TextDisabled("Planner's thing: %s", id.has_value() ? "of no kind that the creature has beliefs about"
			                                                          : "the best plan acts on no thing");
		}
	}
	else
	{
		ImGui::TextDisabled("Planner's thing: no plan for this desire yet");
	}

	if (_thing.has_value())
	{
		const auto belief = BeliefFor(creature, *_thing, kind);
		if (belief.has_value())
		{
			const auto path = creature_tree_view::PathOf(tree, *belief);
			paths.picked = path.nodes;
			const auto utility = creature_tree::Evaluate(tree, *belief);
			ImGui::TextColored(k_PickedText, "Thing tested: %s %u, value %+.1f, usefulness %.3f%s",
			                   creature_tree::BeliefName(belief->type), entt::to_integral(*_thing),
			                   static_cast<double>(utility), static_cast<double>(creature_tree::Usefulness(utility)),
			                   path.reachesLeaf ? "" : " (no leaf: no branch for it)");
			ImGui::TextWrapped("  %s", Attributes(*belief).c_str());
		}
		else
		{
			ImGui::TextDisabled("Thing tested: %u is of no kind that the creature has beliefs about",
			                    entt::to_integral(*_thing));
		}
	}

	if (watch.grownTurn.has_value())
	{
		const auto changed = std::ranges::count(watch.changed, true);
		ImGui::TextColored(k_ChangedText, "Grew again at turn %u: %td nodes changed", *watch.grownTurn, changed);
	}
	else
	{
		ImGui::TextDisabled("Did not grow again while the view was open");
	}

	if (tree.nodes.empty())
	{
		ImGui::TextUnformatted("The tree is empty: the desire has no examples. Every thing has the value 0.");
		DrawExamples(watch);
		return;
	}
	auto& view = _views.at(k).at(d);
	if (ImGui::SmallButton("Fit"))
	{
		view.fit = true;
	}
	ImGui::SameLine();
	ImGui::TextDisabled("Drag to move, wheel to zoom. %zu nodes.", tree.nodes.size());
	DrawGraph(tree, watch, paths, view);
	if (ImGui::TreeNodeEx("Tree as text", ImGuiTreeNodeFlags_None))
	{
		DrawTextTree(tree, watch, paths, 0, std::nullopt, 0);
		ImGui::TreePop();
	}
	DrawExamples(watch);
}

void CreatureMindTrees::DrawGraph(const creature_tree::Tree& tree, const Watch& watch, const Paths& paths,
                                  GraphView& view) noexcept
{
	const auto layout = creature_tree_view::LayOut(tree);
	const ImVec2 size {std::max(ImGui::GetContentRegionAvail().x, 100.0f), k_GraphHeight};
	const auto corner = ImGui::GetCursorScreenPos();
	const ImVec2 end {corner.x + size.x, corner.y + size.y};
	ImGui::InvisibleButton("graph", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
	const bool hovered = ImGui::IsItemHovered();
	auto& io = ImGui::GetIO();
	const float width = static_cast<float>(layout.columns) * k_ColumnStep;
	const float height = static_cast<float>(layout.rows) * k_RowStep;
	if (view.fit)
	{
		view.zoom = std::clamp(std::min(size.x / std::max(width, 1.0f), size.y / std::max(height, 1.0f)), k_MinZoom, 1.0f);
		view.pan = {(size.x - width * view.zoom) * 0.5f, (size.y - height * view.zoom) * 0.5f};
		view.fit = false;
	}
	if (ImGui::IsItemActive())
	{
		view.pan.x += io.MouseDelta.x;
		view.pan.y += io.MouseDelta.y;
	}
	if (hovered && io.MouseWheel != 0.0f)
	{
		// Zoom about the pointer, so that the point under it stays there
		const float old = view.zoom;
		view.zoom = std::clamp(view.zoom * std::pow(1.15f, io.MouseWheel), k_MinZoom, k_MaxZoom);
		const ImVec2 pointer {io.MousePos.x - corner.x, io.MousePos.y - corner.y};
		view.pan.x = pointer.x - (pointer.x - view.pan.x) * (view.zoom / old);
		view.pan.y = pointer.y - (pointer.y - view.pan.y) * (view.zoom / old);
	}

	const float zoom = view.zoom;
	const auto centre = [&](size_t node) {
		const auto& placed = layout.nodes.at(node);
		return ImVec2 {corner.x + view.pan.x + (placed.x * k_ColumnStep + k_ColumnStep * 0.5f) * zoom,
		               corner.y + view.pan.y + (placed.y * k_RowStep + k_RowStep * 0.5f) * zoom};
	};
	const ImVec2 half {k_Box.x * 0.5f * zoom, k_Box.y * 0.5f * zoom};
	const float fontSize = ImGui::GetFontSize() * std::min(zoom, 1.0f);
	const bool showText = zoom >= k_TextZoom;
	const bool blinkOn = std::fmod(ImGui::GetTime(), 0.5) < 0.25;
	const bool blinking = watch.grownTurn.has_value() && ImGui::GetTime() - watch.grownAt < k_BlinkSeconds;
	const bool marksFit = watch.changed.size() == tree.nodes.size();

	auto* draw = ImGui::GetWindowDrawList();
	draw->PushClipRect(corner, end, true);
	draw->AddRectFilled(corner, end, IM_COL32(20, 22, 28, 255));

	// The branches first, under the boxes
	for (size_t i = 0; i < tree.nodes.size(); ++i)
	{
		const auto from = centre(i);
		for (const auto& [value, child] : tree.nodes[i].children)
		{
			if (child >= tree.nodes.size())
			{
				continue;
			}
			const auto to = centre(child);
			const bool planner = OnPath(paths.planner, i, child);
			const bool picked = OnPath(paths.picked, i, child);
			const ImVec2 a {from.x, from.y + half.y};
			const ImVec2 b {to.x, to.y - half.y};
			if (planner && picked)
			{
				draw->AddLine({a.x - 2.0f, a.y}, {b.x - 2.0f, b.y}, k_PlannerPath, 3.0f);
				draw->AddLine({a.x + 2.0f, a.y}, {b.x + 2.0f, b.y}, k_PickedPath, 3.0f);
			}
			else
			{
				draw->AddLine(a, b, planner ? k_PlannerPath : picked ? k_PickedPath : k_Line, planner || picked ? 3.0f : 1.0f);
			}
			if (showText)
			{
				const auto label = creature_tree_view::ValueName(*tree.nodes[i].test, value);
				const ImVec2 middle {(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f};
				const auto textSize = ImGui::CalcTextSize(label.c_str());
				const ImVec2 at {middle.x - textSize.x * fontSize / ImGui::GetFontSize() * 0.5f, middle.y - fontSize * 0.5f};
				draw->AddRectFilled({at.x - 2.0f, at.y},
				                    {at.x + textSize.x * fontSize / ImGui::GetFontSize() + 2.0f, at.y + fontSize},
				                    IM_COL32(20, 22, 28, 220));
				draw->AddText(ImGui::GetFont(), fontSize, at, IM_COL32(220, 220, 220, 255), label.c_str());
			}
		}
	}

	std::optional<size_t> under;
	for (size_t i = 0; i < tree.nodes.size(); ++i)
	{
		const auto& node = tree.nodes[i];
		const auto c = centre(i);
		const ImVec2 min {c.x - half.x, c.y - half.y};
		const ImVec2 max {c.x + half.x, c.y + half.y};
		const auto fill =
		    node.test.has_value() ? k_InnerFill : ToImGui(creature_tree_view::LeafColour(creature_tree_view::LeafValue(node)));
		draw->AddRectFilled(min, max, fill, 4.0f * zoom);
		if (marksFit && watch.changed[i] && (!blinking || blinkOn))
		{
			draw->AddRect({min.x - 3.0f, min.y - 3.0f}, {max.x + 3.0f, max.y + 3.0f}, k_Changed, 5.0f * zoom, ImDrawFlags_None,
			              blinking ? 3.0f : 1.5f);
		}
		if (Contains(paths.picked, i))
		{
			draw->AddRect(min, max, k_PickedPath, 4.0f * zoom, ImDrawFlags_None, 2.5f);
		}
		if (Contains(paths.planner, i))
		{
			draw->AddRect({min.x + 2.0f, min.y + 2.0f}, {max.x - 2.0f, max.y - 2.0f}, k_PlannerPath, 4.0f * zoom,
			              ImDrawFlags_None, 2.5f);
		}
		if (showText)
		{
			const auto label = creature_tree_view::NodeLabel(node);
			const auto textSize = ImGui::CalcTextSize(label.c_str());
			const float scale = fontSize / ImGui::GetFontSize();
			draw->AddText(ImGui::GetFont(), fontSize, {c.x - textSize.x * scale * 0.5f, c.y - fontSize * 0.5f},
			              IM_COL32(255, 255, 255, 255), label.c_str());
		}
		if (hovered && io.MousePos.x >= min.x && io.MousePos.x <= max.x && io.MousePos.y >= min.y && io.MousePos.y <= max.y)
		{
			under = i;
		}
	}
	draw->PopClipRect();

	if (under.has_value())
	{
		const auto& node = tree.nodes.at(*under);
		ImGui::BeginTooltip();
		if (node.test.has_value())
		{
			ImGui::Text("Node %zu tests %s, %zu examples, %zu branches", *under, creature_tree::Name(*node.test), node.examples,
			            node.children.size());
		}
		else
		{
			ImGui::Text("Leaf %zu: value %+.1f, %zu examples", *under, static_cast<double>(creature_tree_view::LeafValue(node)),
			            node.examples);
		}
		if (marksFit && watch.changed.at(*under))
		{
			ImGui::TextColored(k_ChangedText, "Changed at turn %u", watch.grownTurn.value_or(0));
		}
		ImGui::EndTooltip();
	}
}

void CreatureMindTrees::DrawTextTree(const creature_tree::Tree& tree, const Watch& watch, const Paths& paths, size_t index,
                                     std::optional<uint32_t> branch, size_t depth) noexcept
{
	if (index >= tree.nodes.size() || depth > tree.nodes.size())
	{
		return;
	}
	const auto& node = tree.nodes[index];
	const bool planner = Contains(paths.planner, index);
	const bool picked = Contains(paths.picked, index);
	const bool changed = watch.changed.size() == tree.nodes.size() && watch.changed[index];
	auto label = branch.has_value() ? fmt::format("= {} -> ", *branch) : std::string();
	label += node.test.has_value() ? fmt::format("{}?", creature_tree::Name(*node.test)) : creature_tree_view::NodeLabel(node);
	label += fmt::format("{}{}{}###{}", planner ? "  [planner]" : "", picked ? "  [tested]" : "", changed ? "  [changed]" : "",
	                     index);
	const auto colour = !node.test.has_value() ? ToVec4(creature_tree_view::LeafColour(creature_tree_view::LeafValue(node)))
	                    : planner              ? k_PlannerText
	                    : picked               ? k_PickedText
	                                           : ImGui::GetStyleColorVec4(ImGuiCol_Text);
	ImGui::PushStyleColor(ImGuiCol_Text, colour);
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
	if (!node.test.has_value())
	{
		flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}
	if (planner || picked || depth == 0)
	{
		flags |= ImGuiTreeNodeFlags_DefaultOpen;
	}
	const bool open = ImGui::TreeNodeEx(label.c_str(), flags);
	ImGui::PopStyleColor();
	if (!open || !node.test.has_value())
	{
		return;
	}
	for (const auto& [value, child] : node.children)
	{
		DrawTextTree(tree, watch, paths, child, value, depth + 1);
	}
	ImGui::TreePop();
}

void CreatureMindTrees::DrawExamples(const Watch& watch) noexcept
{
	if (!ImGui::TreeNodeEx("Examples", ImGuiTreeNodeFlags_DefaultOpen, "Examples, the oldest first (%zu of at most %zu)",
	                       watch.episodes.size(), creature_tree::k_MaxEpisodes))
	{
		return;
	}
	constexpr auto k_Flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV;
	if (ImGui::BeginTable("examples", 5, k_Flags))
	{
		ImGui::TableSetupColumn("#");
		ImGui::TableSetupColumn("Thing");
		ImGui::TableSetupColumn("Feedback");
		ImGui::TableSetupColumn("Seen");
		ImGui::TableSetupColumn("Attributes", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableHeadersRow();
		const auto turn = Locator::time::has_value() ? std::optional(Locator::time::value().GetTurn()) : std::nullopt;
		for (size_t i = 0; i < watch.episodes.size(); ++i)
		{
			const auto& episode = watch.episodes[i];
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("%zu", i + 1);
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(creature_tree::BeliefName(episode.belief.type));
			ImGui::TableNextColumn();
			ImGui::TextColored(ToVec4(creature_tree_view::LeafColour(episode.feedback)), "%+.2f",
			                   static_cast<double>(episode.feedback));
			ImGui::TableNextColumn();
			const auto seen = i < watch.seen.size() ? watch.seen[i] : std::nullopt;
			if (seen.has_value())
			{
				ImGui::Text("turn %u (%u ago)", *seen, turn.has_value() && *turn >= *seen ? *turn - *seen : 0u);
			}
			else
			{
				ImGui::TextDisabled("before the view");
			}
			ImGui::TableNextColumn();
			ImGui::TextWrapped("%s", Attributes(episode.belief).c_str());
		}
		ImGui::EndTable();
	}
	ImGui::TextDisabled("A full list puts a new example in place of the newest; the oldest stay.");
	ImGui::TreePop();
}
