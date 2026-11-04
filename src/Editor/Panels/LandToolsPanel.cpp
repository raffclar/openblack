/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandToolsPanel.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <glm/gtc/constants.hpp>
#include <imgui.h>

#include "3D/FlatLand.h"
#include "3D/LandBlock.h"
#include "3D/LandData.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Camera/Camera.h"
#include "Common/UserData.h"
#include "ECS/Archetypes/StreamSegmentArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/Stream.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/PlacedTurn.h"
#include "ECS/Registry.h"
#include "ECS/Systems/EditorSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerHome.h"
#include "ECS/Systems/LandSystemInterface.h"
#include "ECS/Systems/SaveGameSystemInterface.h"
#include "ECS/VillagerAge.h"
#include "Editor/EditorEntities.h"
#include "Editor/EditorStyle.h"
#include "Editor/LandSave.h"
#include "FileSystem/FileSystemInterface.h"
#include "InfoConstants.h"
#include "LHScriptX/FeatureScriptWriter.h"
#include "Locator.h"
#include "Magic/MagicTables.h"

namespace openblack::editor
{

using Tool = ecs::systems::EditorSystemInterface::Tool;
using namespace ecs::components;

namespace
{

/// The folder of the profile folder that the editor saves into
constexpr std::string_view k_SaveFolder = "Editor";
/// The points round a circle of the brush that the overlay draws
constexpr int k_CirclePoints = 48;
constexpr ImU32 k_InnerCircle = IM_COL32(255, 220, 90, 230);
constexpr ImU32 k_OuterCircle = IM_COL32(255, 220, 90, 120);
constexpr ImU32 k_FootpathLine = IM_COL32(230, 190, 120, 240);
constexpr ImU32 k_StreamLine = IM_COL32(90, 170, 255, 240);
constexpr ImU32 k_MarkerColour = IM_COL32(255, 120, 220, 240);

/// Whether a building can keep the footpaths that lead from it, as the land's footpath file links them
bool CanHoldLink(const ecs::Registry& registry, entt::entity entity)
{
	return registry.Valid(entity) && registry.AllOf<Transform>(entity) &&
	       registry.AnyOf<Abode, Field, FishFarm, WorshipSite, BigForest, Temple>(entity);
}

/// A mobile static or a mobile object that the land script makes, as the game compares them: the game writes one line
/// for all the things of one row at the same place and height
struct WrittenThing
{
	bool object {false};
	int32_t row {0};
	glm::ivec2 place {0, 0};
	float altitude {0.0f};

	friend bool operator==(const WrittenThing&, const WrittenThing&) = default;
};

/// The height of a thing above the land under its place on the map
float AltitudeOf(glm::vec3 position, glm::ivec2 place)
{
	if (!Locator::terrainSystem::has_value())
	{
		return 0.0f;
	}
	const glm::vec2 metres {map_coords::ToMetres(place.x), map_coords::ToMetres(place.y)};
	return position.y - Locator::terrainSystem::value().GetHeightAt(metres);
}

/// The number of a town, -1 for none
int32_t TownNumber(const ecs::Registry& registry, entt::entity town)
{
	if (town == entt::null || !registry.Valid(town))
	{
		return -1;
	}
	const auto* found = registry.TryGet<const Town>(town);
	return found != nullptr ? static_cast<int32_t>(found->id) : -1;
}

/// The name of the effect of a miracle, empty for none
std::string MiracleName(const InfoConstants& info, MagicType type)
{
	if (type == MagicType::None)
	{
		return {};
	}
	return magic::GetMagicEffectInfo(info, type).debugString.data();
}

/// The line that makes a mobile static, a mobile object, a villager, a miracle dispenser, a one-shot miracle or a
/// creature again. None for a thing of another kind. An empty line for a thing that the game writes no line for:
/// - A thing in a hand.
/// - A mobile static or a mobile object of the same row at the same place and height as a thing with a line.
/// - A miracle dispenser with no town, or a creature with no god.
std::optional<std::string> ThingText(const ecs::Registry& registry, entt::entity entity, std::vector<WrittenThing>& written)
{
	const auto* transform = registry.TryGet<const Transform>(entity);
	if (transform == nullptr || !Locator::infoConstants::has_value())
	{
		return std::nullopt;
	}
	const bool knownKind =
	    registry.AnyOf<MobileStatic, MobileObject, Villager, SpellDispenser, OneOffSpellSeed, Creature>(entity);
	if (!knownKind)
	{
		return std::nullopt;
	}
	if (registry.AnyOf<InHand>(entity))
	{
		return std::string {};
	}
	const auto& info = Locator::infoConstants::value();
	const auto place = paths::PlaceOf(transform->position);
	const auto angles = ecs::placed_turn::AnglesOf(registry, entity);
	const auto scale = transform->scale.x;
	const auto* mobileStatic = registry.TryGet<const MobileStatic>(entity);
	const auto* mobileObject = registry.TryGet<const MobileObject>(entity);
	if (mobileStatic != nullptr || mobileObject != nullptr)
	{
		const WrittenThing thing {
		    .object = mobileObject != nullptr,
		    .row =
		        mobileObject != nullptr ? static_cast<int32_t>(mobileObject->type) : static_cast<int32_t>(mobileStatic->type),
		    .place = place,
		    .altitude = AltitudeOf(transform->position, place),
		};
		if (std::ranges::find(written, thing) != written.end())
		{
			return std::string {};
		}
		written.push_back(thing);
		if (thing.object)
		{
			return land_save::MobileObjectText({.place = place, .row = thing.row, .yAngle = angles.y, .scale = scale});
		}
		return land_save::MobileStaticText(
		    {.place = place, .row = thing.row, .altitude = thing.altitude, .angles = angles, .scale = scale});
	}
	if (const auto* villager = registry.TryGet<const Villager>(entity); villager != nullptr)
	{
		const auto row = static_cast<size_t>(villager->kind);
		if (row >= info.villager.size())
		{
			return std::string {};
		}
		const auto& kind = info.villager.at(row);
		land_save::VillagerLine line {
		    .place = place,
		    .town = TownNumber(registry, villager->town),
		    .special = kind.tribeType == Tribe::NONE,
		    .row = static_cast<int32_t>(row),
		    .age = static_cast<int32_t>(ecs::villager_age::AgeNow(*villager)),
		};
		if (!line.special)
		{
			line.name = fmt::format("{}_{}", k_TribeStrs.at(static_cast<size_t>(kind.tribeType)),
			                        k_VillagerNumberStrs.at(static_cast<size_t>(kind.villagerNumber)));
			if (villager->abode != entt::null && registry.Valid(villager->abode))
			{
				const auto door = ecs::villager_home::DoorPosition(villager->abode);
				line.door = paths::PlaceOf(glm::vec3(door.x, 0.0f, door.y));
			}
		}
		return land_save::VillagerText(line);
	}
	if (const auto* dispenser = registry.TryGet<const SpellDispenser>(entity); dispenser != nullptr)
	{
		const auto* tribe = dispenser->town != entt::null && registry.Valid(dispenser->town)
		                        ? registry.TryGet<const Tribe>(dispenser->town)
		                        : nullptr;
		const auto row = static_cast<size_t>(dispenser->building);
		if (tribe == nullptr || *tribe == Tribe::NONE || row >= info.abode.size())
		{
			return std::string {};
		}
		return land_save::DispenserText({
		    .town = TownNumber(registry, dispenser->town),
		    .place = place,
		    .building =
		        fmt::format("{}_{}", k_TribeStrs.at(static_cast<size_t>(*tribe)), info.abode.at(row).debugString.data()),
		    .miracle = MiracleName(info, dispenser->magicType),
		    .yAngle = angles.y,
		    .scale = scale,
		    .period = dispenser->timer.period,
		});
	}
	if (const auto* seed = registry.TryGet<const OneOffSpellSeed>(entity); seed != nullptr)
	{
		if (seed->seedType == SpellSeedType::None)
		{
			return std::string {};
		}
		const auto type = magic::GetMagicTypeFromPowerUpLevel(magic::GetSpellSeedInfo(info, seed->seedType), seed->powerUp);
		return land_save::OneShotMiracleText(paths::PlaceOf(seed->position), MiracleName(info, type));
	}
	const auto& creature = registry.Get<const Creature>(entity);
	if (creature.owner == PlayerNames::NEUTRAL)
	{
		return std::string {};
	}
	return land_save::CreatureText(place, static_cast<int32_t>(creature.species), static_cast<int32_t>(creature.owner));
}

/// Where a point of the world is on the screen, in pixels, none behind the camera
std::optional<ImVec2> OnScreen(glm::vec3 point)
{
	if (!Locator::camera::has_value())
	{
		return std::nullopt;
	}
	const auto display = ImGui::GetIO().DisplaySize;
	const glm::vec4 viewport {0.0f, 0.0f, display.x, display.y};
	glm::vec3 screen;
	if (!Locator::camera::value().ProjectWorldToScreen(point, viewport, screen))
	{
		return std::nullopt;
	}
	return ImVec2(screen.x, screen.y);
}

/// A circle on the land round a point, drawn over the world
void DrawLandCircle(glm::vec2 centre, float radius, ImU32 colour, float thickness)
{
	auto* drawList = ImGui::GetBackgroundDrawList();
	std::optional<ImVec2> previous;
	for (int i = 0; i <= k_CirclePoints; ++i)
	{
		const auto angle = glm::two_pi<float>() * static_cast<float>(i) / static_cast<float>(k_CirclePoints);
		const glm::vec2 point = centre + radius * glm::vec2(std::cos(angle), std::sin(angle));
		const auto screen = OnScreen({point.x, LandHeight(point), point.y});
		if (screen.has_value() && previous.has_value())
		{
			drawList->AddLine(*previous, *screen, colour, thickness);
		}
		previous = screen;
	}
}

bool WriteFile(const std::filesystem::path& path, std::span<const uint8_t> bytes)
{
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (!file)
	{
		return false;
	}
	file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	return static_cast<bool>(file);
}

/// The land's data as its landscape file holds it, or the generated flat land for a land with no file, as the testbed
std::optional<LandData> SourceLand()
{
	if (!Locator::landSystem::has_value())
	{
		return std::nullopt;
	}
	const auto& path = Locator::landSystem::value().GetLandscapePath();
	if (path.empty())
	{
		return flat_land::Build();
	}
	auto& fileSystem = Locator::filesystem::value();
	const auto fixed = fileSystem.FindPath(filesystem::FileSystemInterface::FixPath(path));
	if (!fileSystem.Exists(fixed))
	{
		return std::nullopt;
	}
	lnd::LNDFile lnd;
	const auto stream = fileSystem.GetData(fixed);
	if (stream == nullptr || lnd.ReadFile(*stream) != lnd::LNDResult::Success)
	{
		return std::nullopt;
	}
	return LandData::FromLnd(lnd);
}

/// The name of the land that is open, as the saved files take it
std::string LandName()
{
	if (!Locator::landSystem::has_value())
	{
		return "land";
	}
	const auto stem = Locator::landSystem::value().GetLandPath().stem().string();
	return stem.empty() ? std::string("Testbed") : stem;
}

std::optional<std::filesystem::path> SaveFolder()
{
	const auto profile = user_data::ProfileFolder();
	if (!profile.has_value())
	{
		return std::nullopt;
	}
	auto folder = *profile / k_SaveFolder;
	std::error_code error;
	std::filesystem::create_directories(folder, error);
	return error ? std::nullopt : std::optional(folder);
}

} // namespace

land_brush::Brush& LandToolsPanel::GetBrush() noexcept
{
	return Locator::editorSystem::value().GetLandBrush();
}

void LandToolsPanel::SetPathKind(paths::Kind kind) noexcept
{
	if (kind != _pathKind)
	{
		_pathKind = kind;
		_draft.Clear();
		_firstHolder.reset();
		_lastHolder.reset();
	}
}

void LandToolsPanel::Stroke(glm::vec2 centre) noexcept
{
	if (!Locator::terrainSystem::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& land = Locator::terrainSystem::value();
	auto& registry = Locator::entitiesRegistry::value();
	const auto cellAt = [&land](glm::u16vec2 corner) { return land.FindCell(corner); };
	if (GetBrush().mode == land_brush::Mode::Paint)
	{
		const auto changes =
		    land_brush::Paint(GetBrush(), centre, static_cast<int>(land.GetCountries().size()), [&cellAt](glm::u16vec2 corner) {
			    const auto* cell = cellAt(corner);
			    return cell != nullptr ? std::optional<uint8_t>(cell->properties.country) : std::nullopt;
		    });
		for (const auto& change : changes)
		{
			land.SetCellCountry(change.corner, change.country);
		}
		land.CommitAltitudeChanges();
		_status = fmt::format("Painted {} corners with country {}", changes.size(), GetBrush().country);
		return;
	}
	const auto changes = land_brush::Stroke(GetBrush(), centre, [&cellAt](glm::u16vec2 corner) {
		const auto* cell = cellAt(corner);
		return cell != nullptr ? std::optional<uint8_t>(cell->altitude) : std::nullopt;
	});
	if (changes.empty())
	{
		return;
	}
	// The things on the land in reach keep their heights above it, so that they are not buried or left in the air
	const auto reach = glm::vec2(GetBrush().outerRadius + land_brush::k_CornerSpacing);
	const auto lowest = centre - reach;
	const auto highest = centre + reach;
	std::vector<std::pair<entt::entity, float>> standing;
	registry.Each<const Transform>([&](entt::entity entity, const Transform& transform) {
		const glm::vec2 at {transform.position.x, transform.position.z};
		if (glm::any(glm::lessThan(at, lowest)) || glm::any(glm::greaterThan(at, highest)) ||
		    registry.AnyOf<Hand, InHand, InPhysics>(entity))
		{
			return;
		}
		standing.emplace_back(entity, land.GetHeightAt(at));
	});
	for (const auto& change : changes)
	{
		land.SetCellAltitude(change.corner, change.altitude);
	}
	land.CommitAltitudeChanges();
	for (const auto& [entity, before] : standing)
	{
		auto& transform = registry.Get<Transform>(entity);
		transform.position.y += land.GetHeightAt({transform.position.x, transform.position.z}) - before;
	}
	_status = fmt::format("{} {} corners", GetBrush().mode == land_brush::Mode::Raise ? "Raised" : "Lowered", changes.size());
}

void LandToolsPanel::PressAt(EditorContext& context, std::optional<glm::vec3> land, std::optional<entt::entity> picked) noexcept
{
	if (!land.has_value())
	{
		_status = "The click is not on the land";
		return;
	}
	if (context.system.GetTool() == Tool::Land)
	{
		Stroke({land->x, land->z});
		_lastStroke = glm::vec2(land->x, land->z);
		return;
	}
	if (context.system.GetTool() != Tool::Paths)
	{
		return;
	}
	auto point = *land;
	point.y = LandHeight({point.x, point.z});
	if (!_draft.Add(point))
	{
		return;
	}
	if (_pathKind != paths::Kind::Footpath)
	{
		return;
	}
	// A footpath's ends that are put on a building lead from that building
	const auto holder = picked.has_value() && CanHoldLink(context.registry, *picked) ? picked : std::nullopt;
	if (_draft.Points().size() == 1)
	{
		_firstHolder = holder;
	}
	_lastHolder = holder;
}

void LandToolsPanel::DragTo(std::optional<glm::vec3> land) noexcept
{
	if (!land.has_value() || !_lastStroke.has_value())
	{
		return;
	}
	const glm::vec2 now {land->x, land->z};
	if (land_brush::MovedFarEnough(*_lastStroke, now))
	{
		Stroke(now);
		_lastStroke = now;
	}
}

void LandToolsPanel::FinishPath(EditorContext& context) noexcept
{
	if (!_draft.CanFinish() || !Locator::terrainSystem::has_value())
	{
		_status = "A path needs two points or more";
		return;
	}
	auto& registry = context.registry;
	const auto& land = Locator::terrainSystem::value();
	const auto points = _draft.Points();
	if (_pathKind == paths::Kind::Stream)
	{
		auto& streams = registry.Context().streams;
		std::vector<int> ids;
		ids.reserve(streams.size());
		for (const auto& [id, entity] : streams)
		{
			ids.push_back(id);
		}
		const auto id = paths::NextStreamId(ids);
		const auto entity = registry.Create();
		auto& stream = registry.Assign<Stream>(entity, id);
		streams.insert({id, entity});
		for (const auto point : points)
		{
			stream.points.push_back(point);
			if (stream.points.size() >= 2)
			{
				ecs::archetypes::StreamSegmentArchetype::Create(stream.points[stream.points.size() - 2], stream.points.back());
			}
		}
		context.system.GetDrawnPaths().streams.push_back(id);
		_status = fmt::format("Made stream {} of {} points", id, points.size());
	}
	else
	{
		const auto entity = registry.Create();
		auto& footpath = registry.Assign<Footpath>(entity);
		for (const auto point : points)
		{
			// The node goes on the land at its place, as the land's script puts its nodes
			const auto place = paths::PlaceOf(point);
			const auto ground = map_coords::ToMetres(map_coords::MapCoords {.x = place.x, .z = place.y, .altitude = 0.0f});
			footpath.nodes.push_back({.position = {ground.x, land.GetHeightAt(ground), ground.y}, .place = place});
		}
		context.system.GetDrawnPaths().footpaths.push_back(entity);
		const auto id = static_cast<Footpath::Id>(entity);
		size_t links = 0;
		for (const auto holder : {_firstHolder, _lastHolder})
		{
			if (!holder.has_value() || !CanHoldLink(registry, *holder) || (links == 1 && _firstHolder == _lastHolder))
			{
				continue;
			}
			// The newest link comes first, as the land's script links them
			if (auto* link = registry.TryGet<FootpathLink>(*holder))
			{
				link->footpaths.insert(link->footpaths.begin(), id);
			}
			else
			{
				registry.Assign<FootpathLink>(*holder, registry.Get<const Transform>(*holder).position,
				                              std::vector<Footpath::Id> {id});
			}
			++links;
		}
		_status = fmt::format("Made a footpath of {} nodes, linked to {} buildings", points.size(), links);
	}
	_draft.Clear();
	_firstHolder.reset();
	_lastHolder.reset();
}

std::string LandToolsPanel::Save(EditorContext& context) noexcept
{
	namespace writer = lhscriptx::writer;
	const auto folder = SaveFolder();
	if (!folder.has_value() || !Locator::terrainSystem::has_value())
	{
		return "No profile folder to save into";
	}
	const auto name = LandName();
	std::vector<std::string> wrote;

	// The land's cells, with their altitudes and countries as they are now, in a landscape file
	if (auto data = SourceLand())
	{
		std::vector<lnd::LNDBlock> blocks;
		for (const auto& block : Locator::terrainSystem::value().GetBlocks())
		{
			if (const auto& lndBlock = block.GetLndBlock())
			{
				blocks.push_back(*lndBlock);
			}
		}
		land_save::CopyCells(*data, blocks);
		const auto path = *folder / land_save::FileName(name, "lnd");
		if (WriteFile(path, data->LndBytes()))
		{
			wrote.push_back(path.filename().string());
		}
	}

	// What the editor placed and drew, as a land script. The game writes its streams before the things in the map's
	// cells. A thing of a kind that the file does not write is counted.
	auto& registry = context.registry;
	std::string script = writer::VersionLine(writer::k_Version);
	int32_t streamId = 0;
	for (const auto& [id, entity] : registry.Context().streams)
	{
		const auto* stream = registry.TryGet<const Stream>(entity);
		if (stream == nullptr ||
		    std::ranges::find(context.system.GetDrawnPaths().streams, id) == context.system.GetDrawnPaths().streams.end())
		{
			continue;
		}
		std::vector<glm::ivec2> places;
		places.reserve(stream->points.size());
		for (const auto point : stream->points)
		{
			places.push_back(paths::PlaceOf(point));
		}
		script += land_save::StreamText(streamId++, places);
	}
	size_t trees = 0;
	size_t features = 0;
	size_t others = 0;
	size_t notWritten = 0;
	std::vector<WrittenThing> written;
	const auto& history = context.system.GetHistory();
	for (const auto ref : history.MadeRefs())
	{
		const auto entity = history.Resolve(ref);
		if (entity == entt::null || !registry.Valid(entity))
		{
			continue;
		}
		const auto* tree = registry.TryGet<const Tree>(entity);
		const auto* feature = registry.TryGet<const Feature>(entity);
		const auto* transform = registry.TryGet<const Transform>(entity);
		const auto featureRow = static_cast<size_t>(feature != nullptr ? feature->type : FeatureInfo::None);
		if (transform != nullptr && feature != nullptr && Locator::infoConstants::has_value() &&
		    featureRow < Locator::infoConstants::value().feature.size())
		{
			// The script takes the angle about the up axis that the thing keeps
			script += land_save::FeatureText({
			    .place = paths::PlaceOf(transform->position),
			    .name = Locator::infoConstants::value().feature.at(featureRow).debugString.data(),
			    .yAngle = ecs::placed_turn::AnglesOf(registry, entity).y,
			    .scale = transform->scale.x,
			});
			++features;
			continue;
		}
		if (auto text = ThingText(registry, entity, written); text.has_value())
		{
			script += *text;
			others += text->empty() ? 0 : 1;
			notWritten += text->empty() ? 1 : 0;
			continue;
		}
		if (tree == nullptr || transform == nullptr)
		{
			++notWritten;
			continue;
		}
		// The editor puts its trees in no forest; the script takes the turn the other way from the thing's rotation
		script += land_save::TreeText({
		    .forest = -1,
		    .place = paths::PlaceOf(transform->position),
		    .type = static_cast<int32_t>(tree->type),
		    .nonScenic = tree->nonScenic,
		    .yAngle = ecs::placed_turn::AnglesOf(registry, entity).y,
		    .scale = transform->scale.x,
		    .maxScale = tree->maxSize,
		});
		++trees;
	}
	size_t footpaths = 0;
	for (const auto entity : context.system.GetDrawnPaths().footpaths)
	{
		if (const auto* footpath = registry.TryGet<const Footpath>(entity))
		{
			std::vector<glm::ivec2> places;
			places.reserve(footpath->nodes.size());
			for (const auto& node : footpath->nodes)
			{
				places.push_back(node.place);
			}
			script += land_save::FootpathText(places);
			++footpaths;
		}
	}
	const auto scriptPath = *folder / land_save::FileName(name, "txt");
	if (WriteFile(scriptPath, std::span(reinterpret_cast<const uint8_t*>(script.data()), script.size())))
	{
		wrote.push_back(
		    fmt::format("{} ({} streams, {} trees, {} features, {} other things, {} footpaths, {} things not in it)",
		                scriptPath.filename().string(), streamId, trees, features, others, footpaths, notWritten));
	}

	// The whole world, to load again in the editor
	if (Locator::saveGameSystem::has_value())
	{
		const auto bytes = Locator::saveGameSystem::value().WorldState();
		const auto worldPath = *folder / land_save::FileName(name, "world");
		if (!bytes.empty() && WriteFile(worldPath, bytes))
		{
			wrote.push_back(worldPath.filename().string());
		}
	}
	if (wrote.empty())
	{
		return "Saved nothing";
	}
	std::string result = fmt::format("Saved in {}:", folder->generic_string());
	for (const auto& file : wrote)
	{
		result += "\n  " + file;
	}
	return result;
}

std::string LandToolsPanel::LoadWorld() noexcept
{
	const auto folder = SaveFolder();
	if (!folder.has_value() || !Locator::saveGameSystem::has_value())
	{
		return "No profile folder to load from";
	}
	const auto path = *folder / land_save::FileName(LandName(), "world");
	std::ifstream file(path, std::ios::binary);
	if (!file)
	{
		return fmt::format("No world file {}", path.filename().string());
	}
	const std::vector<uint8_t> bytes {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
	return Locator::saveGameSystem::value().LoadWorldState(bytes) ? fmt::format("Loaded {}", path.filename().string())
	                                                              : fmt::format("Could not load {}", path.filename().string());
}

void LandToolsPanel::Draw(EditorContext& context) noexcept
{
	auto& system = context.system;
	const auto tool = system.GetTool();
	if (ImGui::RadioButton("Land brush", tool == Tool::Land))
	{
		system.SetTool(Tool::Land);
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("Draw paths", tool == Tool::Paths))
	{
		system.SetTool(Tool::Paths);
	}
	ImGui::SameLine();
	ImGui::TextColored(style::k_Muted, tool == Tool::Land    ? "Click or drag on the land"
	                                   : tool == Tool::Paths ? "Click the land for each point"
	                                                         : "Pick a tool to change the land");

	if (ImGui::BeginTable("LandTools", 3, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchSame))
	{
		ImGui::TableNextColumn();
		ImGui::SeparatorText("Brush");
		auto mode = static_cast<int>(GetBrush().mode);
		ImGui::RadioButton("Raise", &mode, static_cast<int>(land_brush::Mode::Raise));
		ImGui::SameLine();
		ImGui::RadioButton("Lower", &mode, static_cast<int>(land_brush::Mode::Lower));
		ImGui::SameLine();
		ImGui::RadioButton("Paint country", &mode, static_cast<int>(land_brush::Mode::Paint));
		GetBrush().mode = static_cast<land_brush::Mode>(mode);
		ImGui::SliderFloat("Inner radius", &GetBrush().innerRadius, 0.0f, land_brush::k_MostRadius, "%.0f m");
		ImGui::SliderFloat("Outer radius", &GetBrush().outerRadius, 0.0f, land_brush::k_MostRadius, "%.0f m");
		GetBrush().outerRadius = std::max(GetBrush().outerRadius, GetBrush().innerRadius);
		ImGui::SliderInt("Step", &GetBrush().step, 1, land_brush::k_MostStep);
		ImGui::SetItemTooltip("The altitude steps that the inner circle goes up or down for each stroke");
		if (Locator::terrainSystem::has_value())
		{
			const auto& countries = Locator::terrainSystem::value().GetCountries();
			const auto count = std::min(static_cast<int>(countries.size()), land_brush::k_MostCountries);
			const auto label = [&countries](int i) {
				return fmt::format("Country {} (ground {})", i, countries.at(static_cast<size_t>(i)).type);
			};
			if (ImGui::BeginCombo("Country", GetBrush().country < count ? label(GetBrush().country).c_str() : "None"))
			{
				for (int i = 0; i < count; ++i)
				{
					if (ImGui::Selectable(label(i).c_str(), GetBrush().country == i))
					{
						GetBrush().country = static_cast<uint8_t>(i);
					}
				}
				ImGui::EndCombo();
			}
		}

		ImGui::TableNextColumn();
		ImGui::SeparatorText("Paths");
		auto kind = static_cast<int>(_pathKind);
		ImGui::RadioButton("Footpath", &kind, static_cast<int>(paths::Kind::Footpath));
		ImGui::SameLine();
		ImGui::RadioButton("Stream", &kind, static_cast<int>(paths::Kind::Stream));
		SetPathKind(static_cast<paths::Kind>(kind));
		ImGui::Text("%zu points, %.0f m", _draft.Points().size(), paths::Length(_draft.Points()));
		ImGui::BeginDisabled(!_draft.CanFinish());
		if (ImGui::Button("Finish path"))
		{
			FinishPath(context);
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(_draft.Points().empty());
		if (ImGui::Button("Remove last point"))
		{
			_draft.RemoveLast();
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear"))
		{
			_draft.Clear();
		}
		ImGui::EndDisabled();
		ImGui::TextColored(style::k_Muted, "A footpath that starts or ends on a building leads from it");

		ImGui::TableNextColumn();
		ImGui::SeparatorText("Save");
		if (ImGui::Button("Save land files"))
		{
			_status = Save(context);
		}
		ImGui::SetItemTooltip("A landscape file, a land script of what the editor placed and drew, and a world file");
		ImGui::SameLine();
		if (ImGui::Button("Load world file"))
		{
			_status = LoadWorld();
		}
		ImGui::TextColored(style::k_Muted, "Into the profile folder's %s folder", std::string(k_SaveFolder).c_str());
		ImGui::EndTable();
	}
	if (!_status.empty())
	{
		ImGui::TextColored(style::k_Muted, "%s", _status.c_str());
	}
}

void LandToolsPanel::DrawOverlays(const EditorContext& context, std::optional<glm::vec3> mouseLand) const noexcept
{
	auto* drawList = ImGui::GetBackgroundDrawList();
	// The scripts' markers, which the game does not draw
	context.registry.Each<const ScriptMarker, const Transform>(
	    // A marker keeps no data, so the walk does not give it
	    [drawList](entt::entity /*entity*/, const Transform& transform) {
		    if (const auto screen = OnScreen(transform.position))
		    {
			    constexpr float k_Size = 6.0f;
			    drawList->AddQuad(ImVec2(screen->x, screen->y - k_Size), ImVec2(screen->x + k_Size, screen->y),
			                      ImVec2(screen->x, screen->y + k_Size), ImVec2(screen->x - k_Size, screen->y), k_MarkerColour,
			                      2.0f);
			    drawList->AddText(ImVec2(screen->x + k_Size + 2.0f, screen->y - k_Size), k_MarkerColour, "Marker");
		    }
	    });
	const auto tool = context.system.GetTool();
	if (tool == Tool::Land && mouseLand.has_value() && !ImGui::GetIO().WantCaptureMouse)
	{
		const glm::vec2 centre {mouseLand->x, mouseLand->z};
		DrawLandCircle(centre, GetBrush().innerRadius, k_InnerCircle, 2.0f);
		DrawLandCircle(centre, GetBrush().outerRadius, k_OuterCircle, 1.5f);
	}
	if (tool != Tool::Paths)
	{
		return;
	}
	const auto colour = _pathKind == paths::Kind::Stream ? k_StreamLine : k_FootpathLine;
	std::optional<ImVec2> previous;
	for (const auto point : _draft.Points())
	{
		const auto screen = OnScreen(point);
		if (screen.has_value())
		{
			drawList->AddCircleFilled(*screen, 4.0f, colour);
			if (previous.has_value())
			{
				drawList->AddLine(*previous, *screen, colour, 2.5f);
			}
		}
		previous = screen;
	}
	// The next stretch, to the mouse
	if (previous.has_value() && mouseLand.has_value() && !ImGui::GetIO().WantCaptureMouse)
	{
		if (const auto screen = OnScreen(*mouseLand))
		{
			drawList->AddLine(*previous, *screen, colour, 1.0f);
		}
	}
}

} // namespace openblack::editor
