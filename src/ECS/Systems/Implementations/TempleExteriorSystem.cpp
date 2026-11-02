/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "TempleExteriorSystem.h"

#include <condition_variable>

#include <algorithm>
#include <future>
#include <iterator>
#include <memory>
#include <mutex>
#include <ranges>
#include <string>
#include <vector>

#include <L3DFile.h>
#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSkinTexels.h"
#include "3D/TempleExteriorMorph.h"
#include "3D/TempleLanterns.h"
#include "Common/Bitmap16B.h"
#include "Common/WorkerPool.h"
#include "ECS/Archetypes/VillageLightArchetype.h"
#include "ECS/BuildingConstruction.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TempleExterior.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/UndrawnPickModel.h"
#include "ECS/Components/VillageLight.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/UndrawnPicks.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Visual/UploadMode.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

namespace
{
/// The mesh the temples' are made from, which the outsides are blended into
constexpr std::string_view k_FirstTemple = "temple/b_first_temple_l3d";

/// The name of the temple's own mesh in the cache
std::string MeshNameOf(entt::entity entity)
{
	return TempleExteriorMorph::OwnMeshName(static_cast<uint32_t>(entity));
}

entt::id_type OwnMeshId(entt::entity entity)
{
	return entt::hashed_string(MeshNameOf(entity).c_str()).value();
}

/// What the temple's blend comes from: its look now and the textures of its player
/// The set of textures of the player's temples
uint32_t TextureSetOf(PlayerNames owner)
{
	return static_cast<uint32_t>(owner) & 3;
}

TempleExteriorMorph::Blend BlendOf(const TempleExterior& exterior, PlayerNames owner)
{
	return {.look = {exterior.size, exterior.alignment}, .textureSet = TextureSetOf(owner)};
}
} // namespace

TempleExteriorSystem::TempleExteriorSystem() = default;

TempleExteriorSystem::~TempleExteriorSystem() = default;

void TempleExteriorSystem::UpdateTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& alignment = Locator::alignmentSystem::value();
	MadeBlends made;
	registry.Each<const Temple, TempleExterior, Mesh>(
	    [this, &registry, &alignment, &made](const entt::entity entity, const Temple& temple, TempleExterior& exterior,
	                                         Mesh& mesh) {
		    // Its model shows as much of it built as its heart is. Once it shows all of it, the temple is taken out of the
		    // map's cells and put back in them.
		    const auto* progress = registry.TryGet<const BuildProgress>(entity);
		    const auto follow =
		        building_construction::FollowBuilt(exterior.drawnBuilt, progress != nullptr ? progress->built : 1.0f);
		    exterior.drawnBuilt = follow.drawn;
		    // The two lanterns at its entrance are made the first time it shows more than 7% built, each lit with a town
		    // village light
		    if (!exterior.lanternsMade && temple_lanterns::Present(exterior.drawnBuilt))
		    {
			    exterior.lanternsMade = true;
			    const auto placed = registry.Get<const Transform>(entity);
			    for (size_t i = 0; i < temple_lanterns::k_Positions.size(); ++i)
			    {
				    const auto position = temple_lanterns::WorldPosition(i, placed.position, placed.rotation, placed.scale);
				    const auto lantern = registry.Create();
				    registry.Assign<Transform>(lantern, position, glm::mat3(1.0f), glm::vec3(1.0f));
				    registry.Assign<Mesh>(lantern, resources::HashIdentifier(MeshId::ObjectTownLight), static_cast<int8_t>(0),
				                          static_cast<int8_t>(1));
				    const auto light = ecs::archetypes::VillageLightArchetype::Create(position, VillageLight::Kind::Town);
				    registry.Assign<TempleLantern>(lantern, entity, light);
			    }
		    }
		    // From the first turn its player has a temple standing fully built, their temple entrances take the cursor
		    auto& entrancesTakeCursor = registry.Context().templeEntrancesTakeCursor[static_cast<size_t>(temple.owner)];
		    entrancesTakeCursor =
		        undrawn_picks::EntranceTakesCursor(entrancesTakeCursor, progress != nullptr ? progress->built : 1.0f);
		    if (follow.refile && Locator::entitiesMap::has_value())
		    {
			    Locator::entitiesMap::value().Refile(entity);
		    }
		    // Each turn the temple grows toward its player's goodness, and toward the size that the turn of its player
		    // gave it
		    exterior.alignmentTarget = (alignment.GetPlayerAlignment(temple.owner) + 1.0f) * 0.5f;
		    exterior.alignment = TempleExteriorMorph::Step(exterior.alignment, exterior.alignmentTarget);
		    exterior.size = TempleExteriorMorph::Step(exterior.size, exterior.sizeTarget);
		    const glm::vec2 look {exterior.size, exterior.alignment};
		    // A loaded game has the look but not the blended mesh, which is made again
		    const bool meshLoaded = Locator::resources::value().GetMeshes().Contains(mesh.id);
		    if (TempleExteriorMorph::NeedsBlend(exterior.morphed, look, meshLoaded))
		    {
			    Morph(entity, mesh, exterior, temple.owner, made);
			    exterior.morphed = look;
		    }
	    });
	FinishMorphs(made);
	// Its entrances take the cursor while their player's do
	registry.Each<const TempleEntrance, UndrawnPickModel>(
	    [&registry](const entt::entity, const TempleEntrance& entrance, UndrawnPickModel& pick) {
		    const auto* temple = registry.Valid(entrance.temple) ? registry.TryGet<const Temple>(entrance.temple) : nullptr;
		    pick.takesCursor =
		        temple != nullptr && registry.Context().templeEntrancesTakeCursor[static_cast<size_t>(temple->owner)];
	    });
}

void TempleExteriorSystem::NoteSizeTarget(PlayerNames player, std::optional<float> share)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto target = TempleExteriorMorph::SizeTarget(registry.Context().mapScriptGlobals.landNumber, share);
	registry.Each<const Temple, TempleExterior>([player, target](const Temple& temple, TempleExterior& exterior) {
		if (temple.owner == player)
		{
			exterior.sizeTarget = target;
		}
	});
}

void TempleExteriorSystem::FindAfterLoad()
{
	// The save keeps the look that each temple's mesh was blended for, but not the blended mesh. The mesh is made again
	// before the first frame is drawn, with no step toward the targets. A mesh in the cache does not prevent this,
	// because this computer can have blended it for the world before the load.
	// The mesh that this computer blended for the same look and textures stays, as a world state of the same world
	// brings it again.
	auto& registry = Locator::entitiesRegistry::value();
	const auto& meshes = Locator::resources::value().GetMeshes();
	MadeBlends made;
	registry.Each<const Temple, const TempleExterior, Mesh>(
	    [this, &meshes, &made](const entt::entity entity, const Temple& temple, const TempleExterior& exterior, Mesh& mesh) {
		    const auto kept = _blends.find(mesh.id);
		    const bool inCache = kept != _blends.end() && mesh.id == OwnMeshId(entity) && meshes.Contains(mesh.id);
		    if (TempleExteriorMorph::BlendsAfterLoad(exterior.morphed, inCache ? std::optional(kept->second) : std::nullopt,
		                                             BlendOf(exterior, temple.owner)))
		    {
			    Morph(entity, mesh, exterior, temple.owner, made);
		    }
	    });
	FinishMorphs(made);
}

void TempleExteriorSystem::ForgetLand()
{
	// The meshes made for the temples of the land go with the temples. The next land's temples get new meshes.
	if (Locator::resources::has_value())
	{
		auto& resources = Locator::resources::value();
		auto& meshes = resources.GetMeshes();
		auto& files = resources.GetL3DFiles();
		for (const auto templeMesh : _blends | std::views::keys)
		{
			for (const auto made : TempleExteriorMorph::MeshesMadeFor(templeMesh))
			{
				meshes.Erase(made);
			}
			files.Erase(templeMesh);
		}
	}
	_blends.clear();
}

void TempleExteriorSystem::Morph(entt::entity entity, Mesh& mesh, const TempleExterior& exterior, PlayerNames owner,
                                 MadeBlends& made)
{
	using namespace TempleExteriorMorph;
	auto& resources = Locator::resources::value();
	auto& files = resources.GetL3DFiles();
	const auto firstTemple = entt::hashed_string(k_FirstTemple.data()).value();
	if (!files.Contains(firstTemple))
	{
		return;
	}

	// The temple's vertices, each the blend of the four meshes' about its look. A temple with the same look as a temple
	// before it in the call uses the same blend.
	const glm::vec2 look {exterior.size, exterior.alignment};
	auto shape = std::ranges::find_if(made.shapes, [&look](const auto& kept) { return kept.first == look; });
	if (shape == made.shapes.end())
	{
		auto blended = *files.Handle(firstTemple);
		auto vertices = blended.EditVertices();
		std::vector<l3d::L3DVertex> sum(vertices.size());
		for (const auto& corner : Corners(exterior.size, exterior.alignment))
		{
			const auto id = entt::hashed_string(fmt::format("temple/{}", MeshName(corner.size, corner.stage)).c_str()).value();
			if (corner.weight == 0.0f || !files.Contains(id))
			{
				continue;
			}
			const auto& source = files.Handle(id)->GetVertices();
			if (source.size() != sum.size())
			{
				SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The temple's mesh {} isn't the shape of the first temple's",
				                    MeshName(corner.size, corner.stage));
				return;
			}
			const auto add = [&corner](l3d::L3DPoint& to, const l3d::L3DPoint& from) {
				to.x += from.x * corner.weight;
				to.y += from.y * corner.weight;
				to.z += from.z * corner.weight;
			};
			for (size_t i = 0; i < sum.size(); ++i)
			{
				add(sum[i].position, source[i].position);
				add(sum[i].normal, source[i].normal);
				sum[i].texCoord.x += source[i].texCoord.x * corner.weight;
				sum[i].texCoord.y += source[i].texCoord.y * corner.weight;
			}
		}
		std::ranges::copy(sum, vertices.begin());
		made.shapes.emplace_back(look, std::move(blended));
		shape = std::prev(made.shapes.end());
	}
	const auto shapeIndex = static_cast<size_t>(std::distance(made.shapes.begin(), shape));

	// The temple's own mesh in the cache. The temples of the same blend in the call share one mesh, which the call makes
	// or changes after the other temples (see FinishMorphs).
	auto name = MeshNameOf(entity);
	const auto id = entt::hashed_string(name.c_str()).value();
	mesh.id = id;
	// The same look and textures always give the same blend
	_blends.insert_or_assign(id, BlendOf(exterior, owner));
	made.changes.push_back(
	    {.id = id, .name = std::move(name), .shape = shapeIndex, .alignment = exterior.alignment, .owner = owner});
}

void TempleExteriorSystem::FinishMorphs(MadeBlends& made)
{
	if (made.changes.empty())
	{
		return;
	}
	auto& meshes = Locator::resources::value().GetMeshes();
	const auto count = made.changes.size();

	// The temples of each blend, in groups in the order of their first temple, and a number for each mesh that the temples
	// have now
	struct Group
	{
		size_t shape;
		uint32_t set;
		std::vector<size_t> temples;
	};
	std::vector<Group> groups;
	std::vector<const graphics::L3DMesh*> objects;
	const auto numberOf = [&meshes, &objects](entt::id_type id) -> std::optional<size_t> {
		const auto* found = meshes.Find(id);
		if (found == nullptr)
		{
			return std::nullopt;
		}
		const auto known = std::ranges::find(objects, found);
		if (known != objects.end())
		{
			return static_cast<size_t>(std::distance(objects.begin(), known));
		}
		objects.push_back(found);
		return objects.size() - 1;
	};
	std::vector<TempleExteriorMorph::MeshChange> changes;
	changes.reserve(count);
	for (size_t i = 0; i < count; ++i)
	{
		const auto& change = made.changes[i];
		const auto set = TextureSetOf(change.owner);
		auto group = std::ranges::find_if(
		    groups, [&change, set](const Group& kept) { return kept.shape == change.shape && kept.set == set; });
		if (group == groups.end())
		{
			groups.push_back({.shape = change.shape, .set = set, .temples = {}});
			group = std::prev(groups.end());
		}
		group->temples.push_back(i);
		changes.push_back({.group = static_cast<size_t>(std::distance(groups.begin(), group)), .mesh = numberOf(change.id)});
	}
	// The meshes of the temples that do not change in the call
	std::vector<size_t> others;
	for (const auto id : _blends | std::views::keys)
	{
		if (std::ranges::none_of(made.changes, [id](const MadeBlends::Change& change) { return change.id == id; }))
		{
			if (const auto number = numberOf(id); number.has_value())
			{
				others.push_back(*number);
			}
		}
	}
	const auto kept = TempleExteriorMorph::MeshesToKeep(changes, groups.size(), others);

	// The work on the processor for the groups that get a new mesh: the mesh of each blended shape one time, then a copy
	// of the mesh and of the shape for each group. Each job reads only the shapes and the meshes of the shapes, and
	// writes only its own result.
	struct Prepared
	{
		graphics::PreparedL3DMesh mesh;
		std::shared_ptr<l3d::L3DFile> copy;
	};
	std::vector<size_t> fresh;
	for (size_t group = 0; group < groups.size(); ++group)
	{
		if (!kept[group].has_value())
		{
			fresh.push_back(group);
		}
	}
	// The shapes of the new meshes, in the order of their first group, and the shape of each new mesh in that list
	std::vector<size_t> shapes;
	std::vector<size_t> shapeOf(fresh.size());
	for (size_t i = 0; i < fresh.size(); ++i)
	{
		const auto found = std::ranges::find(shapes, groups[fresh[i]].shape);
		shapeOf[i] = static_cast<size_t>(std::distance(shapes.begin(), found));
		if (found == shapes.end())
		{
			shapes.push_back(groups[fresh[i]].shape);
		}
	}
	std::vector<graphics::PreparedL3DMesh> shapeMeshes(shapes.size());
	std::vector<Prepared> prepared(fresh.size());
	// Each copy is ready when its job ends. All of them stop when the work stops with an exception.
	std::vector<bool> ready(fresh.size(), false);
	bool failed = false;
	std::mutex readyMutex;
	std::condition_variable readyChanged;
	struct MarkReady
	{
		~MarkReady()
		{
			{
				const std::scoped_lock lock(mutex);
				ready[index] = true;
			}
			changed.notify_all();
		}
		std::vector<bool>& ready;
		size_t index;
		std::mutex& mutex;
		std::condition_variable& changed;
	};
	const auto settings =
	    fresh.empty() ? graphics::L3DMesh::LoadSettings {} : graphics::L3DMesh::SettingsNow(graphics::ConfiguredUploads());
	const auto prepareShape = [&made, &shapes, &shapeMeshes, &settings](size_t index, size_t) {
		shapeMeshes[index] = graphics::L3DMesh::Prepare(made.shapes[shapes[index]].second, settings);
	};
	const auto copyForGroup = [&](size_t index, size_t) {
		const MarkReady mark {.ready = ready, .index = index, .mutex = readyMutex, .changed = readyChanged};
		prepared[index].mesh = shapeMeshes[shapeOf[index]];
		prepared[index].copy = std::make_shared<l3d::L3DFile>(made.shapes[groups[fresh[index]].shape].second);
	};
	// One shape gives the threads no work to share, so this thread does it and does not wait for threads to start
	std::future<void> jobs;
	if (shapes.size() == 1)
	{
		prepareShape(0, 0);
		for (size_t index = 0; index < fresh.size(); ++index)
		{
			copyForGroup(index, 0);
		}
	}
	// The threads take the copies in the order of the groups. A thread of its own gives them the jobs, so this thread can
	// make the device objects of each mesh when it is ready.
	else if (!shapes.empty())
	{
		auto& workers = Workers();
		jobs = std::async(std::launch::async, [&]() {
			try
			{
				workers.ForEach(shapes.size(), prepareShape);
				workers.ForEach(fresh.size(), copyForGroup);
			}
			catch (...)
			{
				{
					const std::scoped_lock lock(readyMutex);
					failed = true;
				}
				readyChanged.notify_all();
				throw;
			}
		});
	}

	// The mesh of each group, in the order of the groups: the mesh that the group keeps, changed here, or the new mesh,
	// with its objects on the graphics device made here. Then each temple of the group shares it.
	size_t next = 0;
	for (size_t group = 0; group < groups.size(); ++group)
	{
		const auto& temples = groups[group].temples;
		const auto& shape = made.shapes[groups[group].shape].second;
		const auto& first = made.changes[temples.front()];
		entt::id_type source = 0;
		if (kept[group].has_value())
		{
			const auto* object = objects[*kept[group]];
			const auto holder = std::ranges::find_if(
			    temples, [&meshes, &made, object](size_t i) { return meshes.Find(made.changes[i].id) == object; });
			source = made.changes[*holder].id;
			meshes.Handle(source)->UpdateVertices(shape);
			Dress(source, shape, nullptr, first.alignment, first.owner, made);
		}
		else
		{
			const auto index = next++;
			{
				std::unique_lock lock(readyMutex);
				readyChanged.wait(lock, [&ready, &failed, index]() { return ready[index] || failed; });
				if (failed)
				{
					break;
				}
			}
			source = first.id;
			Unbind(source);
			meshes.Load(source, resources::L3DLoader::FromPreparedDynamicTag {}, first.name, std::move(prepared[index].mesh));
			Dress(source, shape, std::move(prepared[index].copy), first.alignment, first.owner, made);
		}
		for (const auto i : temples)
		{
			Share(made.changes[i].id, source);
		}
	}
	if (jobs.valid())
	{
		jobs.get();
	}
	made.changes.clear();
}

void TempleExteriorSystem::Unbind(entt::id_type id)
{
	auto& resources = Locator::resources::value();
	auto& meshes = resources.GetMeshes();
	// The worship site mesh first, as it wears the skins of the temple's mesh
	const auto madeFor = TempleExteriorMorph::MeshesMadeFor(id);
	for (const auto made : madeFor | std::views::reverse)
	{
		if (meshes.Contains(made))
		{
			meshes.Erase(made);
		}
	}
	if (auto& files = resources.GetL3DFiles(); files.Contains(id))
	{
		files.Erase(id);
	}
}

void TempleExteriorSystem::Share(entt::id_type id, entt::id_type source)
{
	auto& resources = Locator::resources::value();
	auto& meshes = resources.GetMeshes();
	if (id == source || meshes.Find(id) == meshes.Find(source))
	{
		return;
	}
	Unbind(id);
	meshes.Load(id, resources::L3DLoader::FromSharedTag {}, meshes.Handle(source).handle());
	if (auto& files = resources.GetL3DFiles(); files.Contains(source))
	{
		files.Load(id, resources::L3DFileLoader::FromCopyTag {}, files.Handle(source).handle());
	}
}

void TempleExteriorSystem::Dress(entt::id_type id, const l3d::L3DFile& blended, std::shared_ptr<l3d::L3DFile> copy,
                                 float alignment, PlayerNames owner, MadeBlends& made)
{
	using namespace TempleExteriorMorph;
	auto& resources = Locator::resources::value();
	auto& files = resources.GetL3DFiles();
	auto& temple = *resources.GetMeshes().Handle(id);
	// Its shape is kept too, for what is done at points on its surface
	if (files.Contains(id))
	{
		*files.Handle(id) = blended;
	}
	else if (copy != nullptr)
	{
		files.Load(id, resources::L3DFileLoader::FromCopyTag {}, std::move(copy));
	}
	else
	{
		files.Load(id, resources::L3DFileLoader::FromFileTag {}, blended);
	}

	// Its texture, of its player's set, between the two looks about its alignment
	const auto texture = TextureOf(alignment);
	const auto set = TextureSetOf(owner);
	auto& bitmaps = resources.GetBitmaps();
	const auto from = entt::hashed_string(fmt::format("temple/{}", ImageName(texture.from, set)).c_str()).value();
	const auto to = entt::hashed_string(fmt::format("temple/{}", ImageName(texture.to, set)).c_str()).value();
	const auto& skins = blended.GetSkins();
	if (skins.empty() || !bitmaps.Contains(from) || !bitmaps.Contains(to))
	{
		return;
	}
	auto& fromImage = *bitmaps.Handle(from);
	auto& toImage = *bitmaps.Handle(to);
	const auto texels = skins.front().texels.size();
	if (fromImage.Size() / 2 != texels || toImage.Size() / 2 != texels)
	{
		return;
	}
	// At the lowest detail levels the skin and the pictures have half the side
	const bool halfSize = l3d_skin::SideOf(texels) != temple.GetSkinSide();
	const auto wanted = halfSize ? texels / 4 : texels;
	// A temple with the same texture and set as a temple before it in the call uses the same blend
	auto skin = std::ranges::find_if(made.skins, [&texture, set, wanted](const MadeBlends::Skin& kept) {
		return kept.texture.from == texture.from && kept.texture.to == texture.to && kept.texture.weight == texture.weight &&
		       kept.set == set && kept.texels.size() == wanted;
	});
	if (skin == made.skins.end())
	{
		auto blendedTexels = SkinTexels({fromImage.Data(), texels}, {toImage.Data(), texels}, texture.weight, halfSize);
		if (!blendedTexels.has_value())
		{
			return;
		}
		made.skins.push_back({.texture = texture, .set = set, .texels = std::move(*blendedTexels)});
		skin = std::prev(made.skins.end());
	}
	// The temples whose meshes are the same take the same prepared texels
	if (!skin->prepared.has_value() || !temple.Fits(*skin->prepared))
	{
		skin->prepared = temple.PrepareSkin(skin->texels);
	}
	temple.UpdateSkin(skins.front().id, *skin->prepared);
}

WorkerPool& TempleExteriorSystem::Workers()
{
	if (_workers == nullptr)
	{
		_workers = std::make_unique<WorkerPool>(WorkerPool::DefaultThreadCount());
	}
	return *_workers;
}
