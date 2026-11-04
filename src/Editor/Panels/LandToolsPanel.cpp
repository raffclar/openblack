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
#include "ECS/Components/MiracleImpression.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Stream.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/PlacedTurn.h"
#include "ECS/PlayerTowns.h"
#include "ECS/Registry.h"
#include "ECS/Systems/EditorSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerHome.h"
#include "ECS/Systems/LandSystemInterface.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/Systems/SaveGameSystemInterface.h"
#include "ECS/TownDesire.h"
#include "ECS/VillagerAge.h"
#include "Editor/EditorEntities.h"
#include "Editor/EditorStyle.h"
#include "Editor/LandSave.h"
#include "FileSystem/FileSystemInterface.h"
#include "InfoConstants.h"
#include "LHScriptX/FeatureScriptWriter.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "PlayerSlots.h"
#include "Serializer/FotFile.h"

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

/// The name of the effect of the row of a miracle in the magic table
std::string MiracleName(const InfoConstants& info, MagicType type)
{
	// The game writes the name of the effect of the miracle's row, also for a dispenser with no miracle
	return magic::GetMagicEffectInfo(info, magic::GetMagicInfo(info, type).magicType).debugString.data();
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

/// A building and the line that makes it again, with the town that it is in
struct BuildingEntry
{
	entt::entity town {entt::null};
	std::string text;
	/// The game makes a field, not a building, from a building row of the field kind. The field's line comes with the
	/// fields, after the towns.
	bool field {false};
};

/// The food or the wood that a building holds, as a script asks it
uint32_t ResourceOf(entt::entity entity, const Abode& abode, ResourceType type)
{
	if (Locator::resourceStoreSystem::has_value())
	{
		return Locator::resourceStoreSystem::value().GetResource(entity, type);
	}
	return type == ResourceType::Food ? abode.foodAmount : abode.woodAmount;
}

/// The worship share that the totem of a town centre was last set to, 0 when the centre has no totem
float TotemShareOf(const ecs::Registry& registry, entt::entity centre)
{
	const auto totem = registry.FindFirst<const VillageTotem>(
	    [centre](entt::entity, const VillageTotem& found) { return found.townCentre == centre; });
	return totem.has_value() ? registry.Get<const VillageTotem>(*totem).ease.target : 0.0f;
}

/// The line that makes a building again, with its town. None for a thing that is not a building of a town. An empty line
/// for a building that the game writes no line for in this form:
/// - A building in a hand, or a building of a town that is gone or that has no tribe.
/// - A field, a citadel or a miracle dispenser, which have other lines or none.
std::optional<BuildingEntry> BuildingText(const ecs::Registry& registry, entt::entity entity)
{
	const auto* transform = registry.TryGet<const Transform>(entity);
	if (transform == nullptr || !Locator::infoConstants::has_value())
	{
		return std::nullopt;
	}
	const auto& towns = registry.Context().towns;
	// A field gets its line with the fields, after the towns, and its town gets its lines with the towns
	if (const auto* field = registry.TryGet<const Field>(entity); field != nullptr)
	{
		const auto town = towns.find(static_cast<uint32_t>(field->town));
		if (registry.AnyOf<InHand>(entity))
		{
			return BuildingEntry {};
		}
		const auto hasTown = field->town != -1 && town != towns.end() && registry.Valid(town->second);
		return BuildingEntry {
		    .town = hasTown ? town->second : entt::null,
		    .text = land_save::FieldText({.town = hasTown ? field->town : -1,
		                                  .place = paths::PlaceOf(transform->position),
		                                  .row = static_cast<int32_t>(field->type),
		                                  .yAngle = ecs::placed_turn::AnglesOf(registry, entity).y}),
		    .field = true,
		};
	}
	const auto* abode = registry.TryGet<const Abode>(entity);
	if (abode == nullptr || registry.AnyOf<SpellDispenser>(entity))
	{
		return std::nullopt;
	}
	const auto& info = Locator::infoConstants::value();
	const auto row = static_cast<size_t>(abode->info);
	const auto town = towns.find(abode->townId);
	const auto* tribe =
	    town != towns.end() && registry.Valid(town->second) ? registry.TryGet<const Tribe>(town->second) : nullptr;
	if (registry.AnyOf<InHand>(entity) || row >= info.abode.size() || tribe == nullptr || *tribe == Tribe::NONE)
	{
		return BuildingEntry {};
	}
	// A building row of the citadel kind is a plain building, as the game makes it
	const auto& abodeInfo = info.abode.at(row);
	const auto place = paths::PlaceOf(transform->position);
	const auto name = fmt::format("{}_{}", k_TribeStrs.at(static_cast<size_t>(*tribe)), abodeInfo.debugString.data());
	const auto yAngle = ecs::placed_turn::AnglesOf(registry, entity).y;
	const auto scale = transform->scale.x;
	const auto townNumber = static_cast<int32_t>(abode->townId);
	// A building that is not complete has the line of a plan, also a town centre
	const bool planned = registry.AnyOf<BuildProgress>(entity);
	if (abodeInfo.abodeType == AbodeType::TownCentre && !planned)
	{
		return BuildingEntry {
		    .town = town->second,
		    .text = land_save::TownCentreText({.town = townNumber,
		                                       .place = place,
		                                       .name = name,
		                                       .yAngle = yAngle,
		                                       .scale = scale,
		                                       .worshipShare = TotemShareOf(registry, entity)}),
		};
	}
	return BuildingEntry {
	    .town = town->second,
	    .text = land_save::AbodeText({.town = townNumber,
	                                  .place = place,
	                                  .name = name,
	                                  .yAngle = yAngle,
	                                  .scale = scale,
	                                  .food = ResourceOf(entity, *abode, ResourceType::Food),
	                                  .wood = ResourceOf(entity, *abode, ResourceType::Wood),
	                                  .planned = planned}),
	};
}

/// A town as the land script makes it again, from what the town keeps now
land_save::TownLine TownLineOf(const ecs::Registry& registry, entt::entity entity)
{
	const auto& town = registry.Get<const Town>(entity);
	const auto tribe = registry.AnyOf<Tribe>(entity) ? registry.Get<const Tribe>(entity) : Tribe::NONE;
	land_save::TownLine line {
	    .id = static_cast<int32_t>(town.id),
	    .place = paths::PlaceOf(registry.Get<const Transform>(entity).position),
	    .player = player_slots::ScriptNameOf(town.owner),
	    .tribe = static_cast<int32_t>(tribe),
	    .tribeName = tribe != Tribe::NONE ? std::string(k_TribeStrs.at(static_cast<size_t>(tribe))) : std::string {},
	    .uninhabitable = town.uninhabitable,
	};
	if (const auto* magic = registry.TryGet<const TownMagic>(entity); magic != nullptr && Locator::infoConstants::has_value())
	{
		for (size_t type = 0; type < magic->held.size(); ++type)
		{
			if (magic->held.at(type))
			{
				line.miracles.push_back(MiracleName(Locator::infoConstants::value(), static_cast<MagicType>(type)));
			}
		}
	}
	// The game asks each of the eight players of a land in turn, the neutral player last
	if (const auto* impression = registry.TryGet<const TownImpression>(entity))
	{
		line.balanceBeliefScale = impression->belief.scale;
		for (size_t player = 0; player < player_slots::k_LandSlots; ++player)
		{
			line.beliefs.push_back({.player = std::string(k_PlayerNamesStrs.at(player)),
			                        .belief = impression->belief.belief.at(player),
			                        .cap = impression->belief.cap.at(player)});
		}
	}
	if (const auto* desire = registry.TryGet<const TownDesire>(entity))
	{
		for (size_t index = 0; index < desire->boost.size(); ++index)
		{
			line.desires.push_back(
			    {.desire = std::string(ecs::town_desire::DesireName(index)), .boost = desire->boost.at(index)});
		}
	}
	if (town.congregationPos.has_value())
	{
		line.congregation = paths::PlaceOf(glm::vec3(town.congregationPos->x, 0.0f, town.congregationPos->y));
	}
	return line;
}

/// The streams and the footpaths of the world that a saved land script makes, in the order of the script. A path matches
/// when the texts of its places are the texts of the places of a path of the script, in the same order. Each path of the
/// world matches one path of the script at most.
paths::DrawnPaths DrawnPathsIn(const ecs::Registry& registry, const land_save::SavedPaths& saved)
{
	namespace writer = lhscriptx::writer;
	std::vector<std::pair<int, std::vector<std::string>>> streams;
	for (const auto& [id, entity] : registry.Context().streams)
	{
		if (const auto* stream = registry.TryGet<const Stream>(entity))
		{
			std::vector<std::string> texts;
			for (const auto point : stream->points)
			{
				texts.push_back(writer::PlaceText(paths::PlaceOf(point)));
			}
			streams.emplace_back(id, std::move(texts));
		}
	}
	std::vector<std::pair<entt::entity, std::vector<std::string>>> footpaths;
	registry.Each<const Footpath>([&footpaths](entt::entity entity, const Footpath& footpath) {
		std::vector<std::string> texts;
		for (const auto& node : footpath.nodes)
		{
			texts.push_back(writer::PlaceText(node.place));
		}
		footpaths.emplace_back(entity, std::move(texts));
	});
	// Each path of the script takes the first path of the world with the same places, which then leaves the list
	const auto take = [](auto& list, const std::vector<std::string>& texts, auto& into) {
		const auto found = std::ranges::find(list, texts, [](const auto& entry) -> const auto& { return entry.second; });
		if (found != list.end())
		{
			into.push_back(found->first);
			list.erase(found);
		}
	};
	paths::DrawnPaths drawn;
	for (const auto& texts : saved.streams)
	{
		take(streams, texts, drawn.streams);
	}
	for (const auto& texts : saved.footpaths)
	{
		take(footpaths, texts, drawn.footpaths);
	}
	return drawn;
}

/// The height of a point above the land under a place on the map, 0 with no land
float HeightAbove(float y, glm::ivec2 place)
{
	if (!Locator::terrainSystem::has_value())
	{
		return 0.0f;
	}
	const auto ground = map_coords::ToMetres(map_coords::MapCoords {.x = place.x, .z = place.y, .altitude = 0.0f});
	return y - Locator::terrainSystem::value().GetHeightAt(ground);
}

/// The land's footpath file of the world as it is now: a link for each thing that can keep footpaths, then for each plan
/// of the towns, then all the footpaths
std::vector<uint8_t> FootpathFileOf(const ecs::Registry& registry)
{
	std::vector<fot_file::Path> files;
	std::vector<Footpath::Id> ids;
	registry.Each<const Footpath>([&files, &ids](entt::entity entity, const Footpath& footpath) {
		fot_file::Path path {.walkable = footpath.walkable};
		for (const auto& node : footpath.nodes)
		{
			const auto flags =
			    static_cast<uint8_t>(1u | (node.planned ? 2u : 0u) | (node.hidden ? 4u : 0u) | (node.marker ? 8u : 0u));
			path.nodes.push_back({.place = node.place, .altitude = HeightAbove(node.position.y, node.place), .flags = flags});
		}
		files.push_back(std::move(path));
		ids.push_back(static_cast<Footpath::Id>(entity));
	});
	std::vector<entt::entity> holders;
	const auto addAll = [&registry, &holders]<typename Kind>() {
		registry.Each<const Kind, const Transform>([&holders](entt::entity entity, const Kind&, const Transform&) {
			if (std::ranges::find(holders, entity) == holders.end())
			{
				holders.push_back(entity);
			}
		});
	};
	addAll.operator()<Abode>();
	addAll.operator()<Field>();
	addAll.operator()<FishFarm>();
	addAll.operator()<WorshipSite>();
	addAll.operator()<BigForest>();
	addAll.operator()<Temple>();
	for (const auto plan : ecs::player_towns::AllPlansInOrder(registry))
	{
		if (registry.AllOf<Transform>(plan) && std::ranges::find(holders, plan) == holders.end())
		{
			holders.push_back(plan);
		}
	}
	std::vector<fot_file::LinkSave> links;
	links.reserve(holders.size());
	for (const auto holder : holders)
	{
		const auto& position = registry.Get<const Transform>(holder).position;
		const auto* farm = registry.TryGet<const FishFarm>(holder);
		const auto place = farm != nullptr ? farm->place : paths::PlaceOf(position);
		const auto* link = registry.TryGet<const FootpathLink>(holder);
		fot_file::LinkSave save {.place = place,
		                         .altitude = HeightAbove(link != nullptr ? link->position.y : position.y, place)};
		if (link != nullptr)
		{
			std::vector<size_t> indices;
			for (const auto id : link->footpaths)
			{
				if (const auto found = std::ranges::find(ids, id); found != ids.end())
				{
					indices.push_back(static_cast<size_t>(std::distance(ids.begin(), found)));
				}
			}
			save.paths = std::move(indices);
		}
		links.push_back(std::move(save));
	}
	return fot_file::Write(links, files);
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
	std::filesystem::path landscape;

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
			landscape = path;
		}
	}

	// What the editor placed and drew, as a land script. The game writes its streams before the things in the map's
	// cells. A thing of a kind that the file does not write is counted.
	auto& registry = context.registry;
	std::string script = writer::VersionLine(writer::k_Version);
	// The land loads the landscape file that the editor wrote beside the script, so that the land keeps its altitudes
	// and countries. The game writes the file name of the open landscape at this place.
	const auto& globals = registry.Context().mapScriptGlobals;
	script += land_save::GameText({
	    .landNumber = globals.landNumber,
	    .landscape = landscape.empty() ? std::string {} : landscape.generic_string(),
	    .townInfluenceMultiplier = globals.townInfluenceMultiplier,
	    .playerInfluenceMultiplier = globals.playerInfluenceMultiplier,
	});
	const auto& history = context.system.GetHistory();

	// The game writes the towns of each player first, each town with its buildings. The file holds each town that has a
	// building that the editor made, so that the buildings have their towns when the file loads.
	size_t buildings = 0;
	size_t notWritten = 0;
	std::vector<std::pair<entt::entity, std::vector<std::string>>> townBuildings;
	std::string fields;
	for (const auto ref : history.MadeRefs())
	{
		const auto entity = history.Resolve(ref);
		if (entity == entt::null || !registry.Valid(entity))
		{
			continue;
		}
		const auto building = BuildingText(registry, entity);
		if (!building.has_value())
		{
			continue;
		}
		if (building->text.empty())
		{
			++notWritten;
			continue;
		}
		++buildings;
		if (building->field)
		{
			fields += building->text;
		}
		if (building->town == entt::null)
		{
			continue;
		}
		// A field's town comes in the file with the towns, so that the field has its town when the file loads
		auto found = std::ranges::find(townBuildings, building->town, &decltype(townBuildings)::value_type::first);
		if (found == townBuildings.end())
		{
			townBuildings.emplace_back(building->town, std::vector<std::string> {});
			found = std::prev(townBuildings.end());
		}
		if (!building->field)
		{
			found->second.push_back(building->text);
		}
	}
	// The players come in their order, and the towns of a player in the order that the player gained them
	std::ranges::sort(townBuildings, [&registry](const auto& a, const auto& b) {
		const auto& first = registry.Get<const Town>(a.first);
		const auto& second = registry.Get<const Town>(b.first);
		return std::pair(static_cast<uint32_t>(first.owner), first.gained) <
		       std::pair(static_cast<uint32_t>(second.owner), second.gained);
	});
	std::optional<PlayerNames> lastPlayer;
	for (const auto& [town, lines] : townBuildings)
	{
		const auto& townComponent = registry.Get<const Town>(town);
		if (lastPlayer != townComponent.owner)
		{
			script += land_save::PlayerHeading(player_slots::ScriptNameOf(townComponent.owner));
			lastPlayer = townComponent.owner;
		}
		script += land_save::TownHeading(static_cast<int32_t>(townComponent.id));
		script += land_save::TownText(TownLineOf(registry, town));
		for (const auto& line : lines)
		{
			script += line;
		}
	}

	// The game writes the fields after the towns and before the streams
	script += fields;

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
	std::vector<WrittenThing> written;
	for (const auto ref : history.MadeRefs())
	{
		const auto entity = history.Resolve(ref);
		if (entity == entt::null || !registry.Valid(entity) || BuildingText(registry, entity).has_value())
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
		    fmt::format("{} ({} towns, {} buildings, {} streams, {} trees, {} features, {} other things, {} footpaths, {} "
		                "things not in it)",
		                scriptPath.filename().string(), townBuildings.size(), buildings, streamId, trees, features, others,
		                footpaths, notWritten));
	}

	// All the footpaths of the world and the links of the things that keep them, as the land's footpath file
	{
		const auto bytes = FootpathFileOf(registry);
		const auto fotPath = *folder / land_save::FileName(name, "fot");
		if (WriteFile(fotPath, bytes))
		{
			wrote.push_back(fotPath.filename().string());
		}
	}

	// The whole world, to load again in the editor
	if (Locator::saveGameSystem::has_value())
	{
		const auto bytes = Locator::saveGameSystem::value().WorldState();
		const auto worldPath = *folder / land_save::FileName(name, "world");
		if (!bytes.empty() && WriteFile(worldPath, bytes))
		{
			wrote.push_back(worldPath.filename().string());
			// The list of the things that the editor made goes beside the world, so that a save after a load of the
			// world writes them again
			std::vector<entt::entity> made;
			for (const auto ref : history.MadeRefs())
			{
				if (const auto entity = history.Resolve(ref); entity != entt::null && registry.Valid(entity))
				{
					made.push_back(entity);
				}
			}
			const auto madeText = land_save::MadeText(made);
			const auto madePath = *folder / land_save::FileName(name, "made");
			if (WriteFile(madePath, std::span(reinterpret_cast<const uint8_t*>(madeText.data()), madeText.size())))
			{
				wrote.push_back(madePath.filename().string());
			}
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

std::string LandToolsPanel::LoadWorld(EditorContext& context) noexcept
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
	if (!Locator::saveGameSystem::value().LoadWorldState(bytes))
	{
		return fmt::format("Could not load {}", path.filename().string());
	}
	// The land script that Save wrote with the world tells which of its paths the editor drew
	std::ifstream scriptFile(*folder / land_save::FileName(LandName(), "txt"), std::ios::binary);
	const std::string script {std::istreambuf_iterator<char>(scriptFile), std::istreambuf_iterator<char>()};
	auto& drawn = context.system.GetDrawnPaths();
	drawn = DrawnPathsIn(context.registry, land_save::ReadSavedPaths(script));
	drawn.keepAtNextReset = true;
	// The load gives each entity the number that it had, so the list that Save wrote finds the things that the editor
	// made. The history keeps them when it clears for the new world.
	std::ifstream madeFile(*folder / land_save::FileName(LandName(), "made"), std::ios::binary);
	const std::string madeText {std::istreambuf_iterator<char>(madeFile), std::istreambuf_iterator<char>()};
	auto made = land_save::ReadMade(madeText);
	std::erase_if(made, [&context](entt::entity entity) { return !context.registry.Valid(entity); });
	const auto madeCount = made.size();
	context.system.GetHistory().KeepMadeAtNextClear(std::move(made));
	return fmt::format("Loaded {} with {} drawn streams, {} drawn footpaths and {} things that the editor made",
	                   path.filename().string(), drawn.streams.size(), drawn.footpaths.size(), madeCount);
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
		ImGui::SetItemTooltip("A landscape file, a land script of what the editor placed and drew, a footpath file and a "
		                      "world file");
		ImGui::SameLine();
		if (ImGui::Button("Load world file"))
		{
			_status = LoadWorld(context);
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
