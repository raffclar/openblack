/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The meshes thrown to pieces: the two queues, UR_ExplodeObject's step with ExplodeMesh, UR_ExplodeObject2, and the
// always-on EXPLODE_OBJECT effect of the particle utilities (made, run and deleted again); the pieces' draw (mesh_pieces,
// to Graphics/WorldTriangles.h); wiki: docs/bw1-notes/miracles.md, "Lightning explosion and missing PSys classes".

#include "ExplodeObject.h"

#include <cmath>
#include <cstring>

#include <algorithm>
#include <array>
#include <istream>
#include <string>
#include <unordered_map>
#include <utility>

#include <L3DFile.h>
#include <LNDFile.h>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "3D/LandIslandInterface.h"
#include "3D/LandLight.h"
#include "3D/LandLightTable.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "GameClock.h"
#include "Graphics/ModelLight.h"
#include "Graphics/WorldTriangles.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Particles/PSys.h"
#include "Particles/PSysManager.h"
#include "Particles/PSysManagerState.h"
#include "Particles/PSysRegistry.h"
#include "Particles/ParticleTypes.h"
#include "Resources/BlobStream.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::psys;
using namespace openblack::psys::explode_object;

namespace
{
// ---- the CRT's qsort, its short sort and bsearch (the MSVC 6 ones) ----
// The edge comparator of ExplodeMesh never answers 0, so which of two equal edges comes first is the CRT's own
// business: their algorithms are kept to get the same order (and so the same pieces).

template <class T, class Compare>
void ShortSort(T* lo, T* hi, Compare comp)
{
	// the short sort: the largest of lo..hi goes to hi (a later one only if it compares > 0), hi steps down
	while (hi > lo)
	{
		T* max = lo;
		for (T* p = lo + 1; p <= hi; ++p)
		{
			if (comp(*p, *max) > 0)
			{
				max = p;
			}
		}
		std::swap(*max, *hi);
		--hi;
	}
}

template <class T, class Compare>
void CrtQsort(T* base, size_t num, Compare comp)
{
	// qsort: insertion of size <= 8, the middle element as the pivot, the smaller
	// part done first and the other one kept on a stack of 30
	if (num < 2)
	{
		return;
	}
	std::array<T*, 30> lostk;
	std::array<T*, 30> histk;
	int stkptr = 0;
	T* lo = base;
	T* hi = base + (num - 1);
	for (;;)
	{
		const size_t size = static_cast<size_t>(hi - lo) + 1;
		if (size <= 8)
		{
			ShortSort(lo, hi, comp);
		}
		else
		{
			T* mid = lo + size / 2;
			std::swap(*mid, *lo);
			T* loguy = lo;
			T* higuy = hi + 1;
			for (;;)
			{
				do
				{
					++loguy;
				} while (loguy <= hi && comp(*loguy, *lo) <= 0);
				do
				{
					--higuy;
				} while (higuy > lo && comp(*higuy, *lo) >= 0);
				if (higuy < loguy)
				{
					break;
				}
				std::swap(*loguy, *higuy);
			}
			std::swap(*lo, *higuy);
			if (higuy - 1 - lo >= hi - loguy)
			{
				if (lo + 1 < higuy)
				{
					lostk[stkptr] = lo;
					histk[stkptr] = higuy - 1;
					++stkptr;
				}
				if (loguy < hi)
				{
					lo = loguy;
					continue;
				}
			}
			else
			{
				if (loguy < hi)
				{
					lostk[stkptr] = loguy;
					histk[stkptr] = hi;
					++stkptr;
				}
				if (lo + 1 < higuy)
				{
					hi = higuy - 1;
					continue;
				}
			}
		}
		--stkptr;
		if (stkptr < 0)
		{
			return;
		}
		lo = lostk[stkptr];
		hi = histk[stkptr];
	}
}

/// bsearch: halves of num, the middle one at half (num odd) or half - 1 (num even)
template <class T, class Key, class Compare>
const T* CrtBsearch(const Key& key, const T* base, size_t num, Compare comp)
{
	const T* lo = base;
	const T* hi = base + (static_cast<ptrdiff_t>(num) - 1);
	while (num != 0 && lo <= hi)
	{
		const size_t half = num / 2;
		if (half != 0)
		{
			const T* mid = lo + ((num & 1) != 0 ? half : half - 1);
			const int result = comp(key, *mid);
			if (result == 0)
			{
				return mid;
			}
			if (result < 0)
			{
				hi = mid - 1;
				num = (num & 1) != 0 ? half : half - 1;
			}
			else
			{
				lo = mid + 1;
				num = half;
			}
		}
		else
		{
			return comp(key, *lo) != 0 ? nullptr : lo;
		}
	}
	return nullptr;
}

// ---- ExplodeMesh's edges ----

/// An edge: its two ends, the one with the smaller (z + y) + x first (on a tie the second argument), and its squared
/// length ((dz dz + dy dy) + dx dx)
struct Edge
{
	glm::vec3 a;
	glm::vec3 b;
	float lengthSquared;
};

Edge MakeEdge(const glm::vec3& p, const glm::vec3& q)
{
	const float sp = (p.z + p.y) + p.x;
	const float sq = (q.z + q.y) + q.x;
	Edge edge;
	if (sq <= sp)
	{
		// sQ <= sP (a tie included) puts the second point first
		edge.a = q;
		edge.b = p;
	}
	else
	{
		edge.a = p;
		edge.b = q;
	}
	const glm::vec3 d = edge.b - edge.a;
	edge.lengthSquared = (d.z * d.z + d.y * d.y) + d.x * d.x;
	return edge;
}

/// The edge of a triangle at a corner: corner 0 = (t[0], t[1]), 1 = (t[1], t[2]), 2 = (t[2], t[0])
Edge TriangleEdge(const SourcePrimitive& primitive, const std::array<uint16_t, 3>& triangle, int corner)
{
	const auto& v = primitive.positions;
	switch (corner)
	{
	case 0:
		return MakeEdge(v[triangle[0]], v[triangle[1]]);
	case 1:
		return MakeEdge(v[triangle[1]], v[triangle[2]]);
	default:
		return MakeEdge(v[triangle[2]], v[triangle[0]]);
	}
}

/// The same edge: the two ends equal, float by float
bool SameEdge(const Edge& x, const Edge& y)
{
	return x.a == y.a && x.b == y.b;
}

/// The sort comparator: -1 when b.len > a.len, else 1 (equal or unordered lengths too); never 0
int CompareForSort(const Edge& x, const Edge& y)
{
	return !(y.lengthSquared > x.lengthSquared) ? 1 : -1;
}

/// The search comparator: 0 for the same six floats, else 1 when key.len >= elem.len, else -1
int CompareForSearch(const Edge& key, const Edge& elem)
{
	if (SameEdge(key, elem))
	{
		return 0;
	}
	return !(elem.lengthSquared > key.lengthSquared) ? 1 : -1;
}

/// The edge's index: bsearch, and when that misses (the comparator is not an order on equal lengths) the first equal
/// edge of a walk through all of them. -1 when there is none
int FindEdge(const std::vector<Edge>& edges, const Edge& key)
{
	if (edges.empty())
	{
		return -1;
	}
	if (const auto* found = CrtBsearch(key, edges.data(), edges.size(), CompareForSearch); found != nullptr)
	{
		return static_cast<int>(found - edges.data());
	}
	for (size_t i = 0; i < edges.size(); ++i)
	{
		if (SameEdge(key, edges[i]))
		{
			return static_cast<int>(i);
		}
	}
	return -1;
}

// ---- the queues of UR_ExplodeObject and UR_ExplodeObject2 and their effect, in the particle system's state ----
manager::State& ManagerState()
{
	return Locator::particleSystem::value().GetState();
}

/// The rules' defaults: MaxTrigsPerFrag 15, RandomFactor 0.3 (SF_ExplodeObject sets RandomFactor 0.889381);
/// UR_ExplodeObject2's: RandomFactor 0.3, MaxDepth 4
constexpr int k_DefaultMaxTrigsPerFrag = 15;
constexpr float k_DefaultRandomFactor = 0.3f;
constexpr int k_DefaultMaxDepth = 4;
/// The random part's Y is halved
constexpr float k_RandomYFactor = 0.5f;
/// ExplodeMesh takes a sub-mesh of LOD 0 (flag 0x20000000) whose status bits (0x3F0) are clear
constexpr uint32_t k_SubMeshLod0 = 0x20000000u;
constexpr uint32_t k_SubMeshStatus = 0x3F0u;

/// The piece atoms' "creator": the original's atoms have none and carry their mesh piece themselves. Here a creator of
/// Kind::MeshPiece (drawn by mesh_pieces, at once, out of every mesh atom list and Z object) and the piece in the atom's
/// modifier data under k_PieceKey
struct PieceCreator final: Creator
{
	PieceCreator() { kind = Kind::MeshPiece; }
};
const PieceCreator g_PieceCreator;
/// The key of the piece in Atom::modifierData (no modifier owns it)
class PieceKey final: public Modifier
{
};
const PieceKey g_PieceKey;

/// The LH3DMesh of a game object: MeshPack index of its Mesh component (the resources' hashed MeshId), -1 if none
int PackIndexOf(entt::id_type meshId)
{
	static const std::unordered_map<entt::id_type, int> k_Index = [] {
		std::unordered_map<entt::id_type, int> map;
		for (uint32_t i = 0; i < static_cast<uint32_t>(MeshId::_COUNT); ++i)
		{
			map.emplace(resources::HashIdentifier(static_cast<MeshId>(i)), static_cast<int>(i));
		}
		return map;
	}();
	const auto it = k_Index.find(meshId);
	return it != k_Index.end() ? it->second : -1;
}

/// ExplodeMesh (collection, NextGroups, mesh, matrix, MaxTrigsPerFrag, origin, speed): every primitive of every LOD 0
/// sub-mesh in pieces, one atom each
void ExplodeMesh(Effect& effect, Collection& collection, const std::vector<int>& nextGroups, const QueuedMesh& entry,
                 int maxTrigsPerFrag, float randomFactor)
{
	if (!entry.mesh)
	{
		return;
	}
	const auto& mesh = *entry.mesh;
	uint32_t made = 0;
	for (size_t s = 0; s < mesh.subMeshes.size(); ++s)
	{
		const auto& subMesh = mesh.subMeshes[s];
		if ((subMesh.flags & k_SubMeshLod0) == 0 || (subMesh.flags & k_SubMeshStatus) != 0)
		{
			continue;
		}
		for (size_t p = 0; p < subMesh.primitives.size(); ++p)
		{
			const auto& primitive = subMesh.primitives[p];
			for (const auto& triangles : SplitPrimitive(primitive, maxTrigsPerFrag))
			{
				// a new atom carrying its piece (lit by the land light), initialised with the NextGroups sub-collections
				auto& atom = effect.NewAtom(collection, &g_PieceCreator, nextGroups);
				std::vector<glm::vec3> positions;
				std::vector<glm::vec2> uvs;
				std::vector<glm::vec3> normals;
				positions.reserve(triangles.size() * 3);
				for (const auto t : triangles)
				{
					for (const auto index : primitive.triangles[t])
					{
						positions.push_back(primitive.positions[index]);
						uvs.push_back(primitive.uvs[index]);
						normals.push_back(primitive.normals[index]);
					}
				}
				// every vertex through the matrix (x m0 + y m3 + z m6 + m9, ...) into the world, summed; the centroid is
				// the sum x (1 / n)
				glm::vec3 sum(0.0f);
				for (auto& v : positions)
				{
					v = entry.position + entry.axes * v;
					sum += v;
				}
				const float inverse = 1.0f / static_cast<float>(positions.size());
				const glm::vec3 centre = sum * inverse;
				// the vertices about the centroid
				for (auto& v : positions)
				{
					v -= centre;
				}
				// the atom's rotation the identity, its position the centroid
				atom.rotation = glm::mat3(1.0f);
				atom.position = centre;
				// the velocity, with a random point in the unit ball
				atom.velocity = PieceVelocity(centre, entry.origin, entry.speed, randomFactor, effect.RandomInBall());
				auto piece = std::make_shared<Piece>();
				piece->triangles = static_cast<uint32_t>(triangles.size());
				// the source primitive, for its material
				piece->source = entry.mesh;
				piece->subMesh = static_cast<uint16_t>(s);
				piece->primitive = static_cast<uint16_t>(p);
				piece->positions = std::move(positions);
				piece->uvs = std::move(uvs);
				piece->normals = std::move(normals);
				atom.modifierData[&g_PieceKey] = std::move(piece);
				++made;
			}
		}
	}
	if (magic::TraceEnabled())
	{
		SPDLOG_LOGGER_INFO(
		    spdlog::get("game"),
		    "ExplodeObject: mesh {} at ({:.1f}, {:.1f}, {:.1f}) in {} pieces, from ({:.1f}, {:.1f}, {:.1f}) at {:.1f}",
		    mesh.meshId, entry.position.x, entry.position.y, entry.position.z, made, entry.origin.x, entry.origin.y,
		    entry.origin.z, entry.speed);
	}
}

/// UR_ExplodeObject (an atom-creating rule, so it counts as a creator)
class ExplodeObject final: public Modifier
{
public:
	explicit ExplodeObject(const Object& object)
	    : nextGroups(object.Array("NextGroups"))
	    , maxTrigsPerFrag(object.Int("MaxTrigsPerFrag", k_DefaultMaxTrigsPerFrag))
	    , randomFactor(object.Float("RandomFactor", k_DefaultRandomFactor))
	{
	}
	[[nodiscard]] bool Creates() const override { return true; }
	/// The queue emptied from its last entry, each one exploded
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		while (!ManagerState().explodeQueue.empty())
		{
			const QueuedMesh entry = std::move(ManagerState().explodeQueue.back());
			ManagerState().explodeQueue.pop_back();
			ExplodeMesh(effect, collection, nextGroups, entry, maxTrigsPerFrag, randomFactor);
		}
		return true;
	}
	std::vector<int> nextGroups; ///< the creating rule's; empty in SF_ExplodeObject
	int maxTrigsPerFrag;         ///< the triangles a piece may grow past
	float randomFactor;          ///< the random part's length, a share of the speed
};

/// UR_ExplodeObject2: empties the second queue the same way through its own split (RandomFactor, MaxDepth). Nobody fills
/// that queue: every caller of the queueing step (the objects' ones and the rocks) asks for the first queue, so that
/// split never runs and is not ported
class ExplodeObject2 final: public Modifier
{
public:
	explicit ExplodeObject2(const Object& object)
	    : randomFactor(object.Float("RandomFactor", k_DefaultRandomFactor))
	    , maxDepth(object.Int("MaxDepth", k_DefaultMaxDepth))
	{
	}
	[[nodiscard]] bool Creates() const override { return true; }
	bool ModifyCollection(Effect& /*effect*/, Collection& /*collection*/, Collection::Slot& /*slot*/) const override
	{
		ManagerState().explodeQueue2.clear(); // not ported: the game never reaches this rule's own step, see above
		return true;
	}
	float randomFactor;
	int maxDepth;
};

// ---- AllMeshes.g3d read for the CPU (the pack's MESHES block, the same layout components/pack reads) ----
struct PackIndex
{
	std::streamoff block {0};
	uint32_t size {0};
	std::vector<uint32_t> offsets;
};

std::unique_ptr<std::istream> OpenPack()
{
	if (!Locator::filesystem::has_value())
	{
		return nullptr;
	}
	auto& fileSystem = Locator::filesystem::value();
	try
	{
		// the copy the game loaded the pack from (the byte cache)
		return resources::BlobStream(
		    resources::LoadBlob(Locator::resources::value().GetBlobs(),
		                        fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Data>() / "AllMeshes.g3d")));
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "ExplodeObject: no AllMeshes.g3d: {}", e.what());
		return nullptr;
	}
}

/// The pack: "LiOnHeAd", then blocks of a 32-character name, a size and the body; MESHES = "MKJC", the count and the
/// offsets of the meshes in the block (components/pack/src/PackFile.cpp)
bool ReadPackIndex(std::istream& stream, PackIndex& pack)
{
	stream.seekg(8);
	while (stream)
	{
		std::array<char, 32> name {};
		uint32_t size = 0;
		stream.read(name.data(), name.size());
		stream.read(reinterpret_cast<char*>(&size), sizeof(size));
		if (!stream)
		{
			return false;
		}
		const auto body = stream.tellg();
		if (std::strncmp(name.data(), "MESHES", name.size()) == 0)
		{
			std::array<char, 4> magic {};
			uint32_t count = 0;
			stream.read(magic.data(), magic.size());
			stream.read(reinterpret_cast<char*>(&count), sizeof(count));
			pack.offsets.resize(count);
			stream.read(reinterpret_cast<char*>(pack.offsets.data()), static_cast<std::streamsize>(count * sizeof(uint32_t)));
			pack.block = body;
			pack.size = size;
			return static_cast<bool>(stream);
		}
		stream.seekg(body + static_cast<std::streamoff>(size));
	}
	return false;
}
} // namespace

// ---- the split ----

std::vector<std::vector<uint32_t>> explode_object::SplitPrimitive(const SourcePrimitive& primitive, int maxTrigsPerFrag)
{
	std::vector<std::vector<uint32_t>> pieces;
	const auto count = static_cast<uint32_t>(primitive.triangles.size());
	if (count == 0)
	{
		return pieces;
	}
	// the three edges of every triangle into the edge list
	std::vector<Edge> edges;
	edges.reserve(count * 3);
	for (const auto& triangle : primitive.triangles)
	{
		for (int corner = 0; corner < 3; ++corner)
		{
			edges.push_back(TriangleEdge(primitive, triangle, corner));
		}
	}
	// qsort by length; of a run of equal neighbours the last one kept
	CrtQsort(edges.data(), edges.size(), CompareForSort);
	std::vector<Edge> unique;
	unique.reserve(edges.size());
	for (size_t i = 0; i < edges.size(); ++i)
	{
		while (i + 1 < edges.size() && SameEdge(edges[i + 1], edges[i]))
		{
			++i;
		}
		unique.push_back(edges[i]);
	}
	// per unique edge the triangles that have it
	std::vector<std::vector<uint32_t>> users(unique.size());
	for (uint32_t t = 0; t < count; ++t)
	{
		for (int corner = 0; corner < 3; ++corner)
		{
			const int found = FindEdge(unique, TriangleEdge(primitive, primitive.triangles[t], corner));
			if (found >= 0)
			{
				users[static_cast<size_t>(found)].push_back(t);
			}
		}
	}
	// the pieces. From the first triangle not used yet, a walk: the triangle goes in, then the first
	// unused triangle sharing one of its edges (corners 0, 1, 2, the users in their order) is the next one, while one is
	// found and the piece has no more than MaxTrigsPerFrag (so up to MaxTrigsPerFrag + 1 triangles)
	std::vector<uint8_t> used(count, 0);
	uint32_t start = 0;
	while (start < count)
	{
		auto& piece = pieces.emplace_back();
		uint32_t current = start;
		int made = 0;
		bool found = false;
		do
		{
			piece.push_back(current);
			++made;
			used[current] = 1;
			found = false;
			for (int corner = 0; corner < 3 && !found; ++corner)
			{
				const int edge = FindEdge(unique, TriangleEdge(primitive, primitive.triangles[current], corner));
				if (edge < 0)
				{
					continue;
				}
				for (const auto other : users[static_cast<size_t>(edge)])
				{
					if (used[other] == 0)
					{
						current = other;
						found = true;
						break;
					}
				}
			}
		} while (found && made <= maxTrigsPerFrag);
		// the next start, the first unused triangle from this start on (count when there is none)
		uint32_t next = start;
		while (next < count && used[next] != 0)
		{
			++next;
		}
		start = next;
	}
	return pieces;
}

glm::vec3 explode_object::PieceVelocity(const glm::vec3& centre, const glm::vec3& origin, float speed, float randomFactor,
                                        glm::vec3 random)
{
	glm::vec3 d = centre - origin;
	if (d.x != 0.0f || d.y != 0.0f || d.z != 0.0f)
	{
		// speed / sqrt((x x + z z) + y y)
		const float k = speed / std::sqrt((d.x * d.x + d.z * d.z) + d.y * d.y);
		d *= k;
	}
	// |d| x RandomFactor
	const float length = std::sqrt((d.x * d.x + d.z * d.z) + d.y * d.y) * randomFactor;
	if (random.x != 0.0f || random.y != 0.0f || random.z != 0.0f)
	{
		const float k = length / std::sqrt((random.x * random.x + random.y * random.y) + random.z * random.z);
		random *= k;
	}
	// only the random part's Y is halved, then the three added to d
	return {d.x + random.x, d.y + random.y * k_RandomYFactor, d.z + random.z};
}

// ---- the queues ----

void explode_object::QueueMesh(std::shared_ptr<const SourceMesh> mesh, const glm::mat3& axes, const glm::vec3& position,
                               const glm::vec3& origin, float speed, float spread, bool second)
{
	// no mesh -> nothing; the entry at the end of its queue
	if (!mesh)
	{
		return;
	}
	(second ? ManagerState().explodeQueue2 : ManagerState().explodeQueue)
	    .push_back({std::move(mesh), axes, position, origin, speed, spread});
}

bool explode_object::QueueObject(entt::entity object, const glm::vec3& origin, float speed, float spread, bool second)
{
	// an object with a 3D object: its world matrix and that 3D object's mesh
	if (!Locator::entitiesRegistry::has_value() || object == entt::null)
	{
		return false;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return false;
	}
	const auto* mesh = registry.TryGet<const ecs::components::Mesh>(object);
	const auto* transform = registry.TryGet<const ecs::components::Transform>(object);
	if (mesh == nullptr || transform == nullptr)
	{
		return false;
	}
	// the drawn matrix of the object (RenderingSystem: T(position) R S)
	const int index = PackIndexOf(mesh->id);
	if (index < 0)
	{
		if (magic::TraceEnabled())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "ExplodeObject: object {} has a mesh that is not the pack's, not exploded",
			                   static_cast<uint32_t>(object));
		}
		return false;
	}
	auto source = PackMesh(static_cast<uint32_t>(index));
	if (!source)
	{
		return false;
	}
	glm::mat3 axes = transform->rotation;
	axes[0] *= transform->scale.x;
	axes[1] *= transform->scale.y;
	axes[2] *= transform->scale.z;
	QueueMesh(std::move(source), axes, transform->position, origin, speed, spread, second);
	return true;
}

size_t explode_object::QueuedCount(bool second)
{
	return (second ? ManagerState().explodeQueue2 : ManagerState().explodeQueue).size();
}

void explode_object::GameLoopEnd()
{
	// the effect is made when there is none: particle type 0x17 at (0, 0, 0), direction (0, 0, 0), power 1.0, no owner
	if (ManagerState().explodeEffect != 0 && manager::Find(ManagerState().explodeEffect) == nullptr)
	{
		ManagerState().explodeEffect = 0; // gone with the PSys (a new map)
	}
	if (ManagerState().explodeEffect == 0)
	{
		const auto file = ParticleTypeFile(ParticleType::ExplodeObject);
		if (file.empty())
		{
			return;
		}
		ManagerState().explodeEffect =
		    manager::StartForSpell(std::string(file), glm::vec3(0.0f), glm::vec3(0.0f), 1.0f, nullptr);
		if (ManagerState().explodeEffect == 0)
		{
			return;
		}
	}
	// run each turn with a process info of zeros (power 1.0); a result of 5 (finished: SF_ExplodeObject's MaxSpellAge
	// 25) deletes it and the next turn makes a new one
	const ProcessInfo info {};
	if (!manager::ProcessForSpell(ManagerState().explodeEffect, info, game_clock::k_TurnSeconds))
	{
		ManagerState().explodeEffect = 0;
	}
}

void explode_object::Clear()
{
	// clearing the map empties both queues
	ManagerState().explodeQueue2.clear();
	ManagerState().explodeQueue.clear();
	ManagerState().explodeEffect = 0;
}

// ---- the pack meshes ----

namespace
{
/// The bytes of mesh `index` of AllMeshes.g3d, read from the pack's cached bytes; empty when there is none
std::vector<uint8_t> PackMeshBytes(uint32_t index)
{
	auto stream = OpenPack();
	if (!stream)
	{
		return {};
	}
	PackIndex pack;
	if (!ReadPackIndex(*stream, pack))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "ExplodeObject: no MESHES block in AllMeshes.g3d");
		return {};
	}
	if (index >= pack.offsets.size())
	{
		return {};
	}
	const uint32_t begin = pack.offsets[index];
	const uint32_t end = index + 1 < pack.offsets.size() ? pack.offsets[index + 1] : pack.size;
	std::vector<uint8_t> bytes(end > begin ? end - begin : 0);
	stream->clear();
	stream->seekg(pack.block + static_cast<std::streamoff>(begin));
	stream->read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	return *stream ? bytes : std::vector<uint8_t> {};
}
} // namespace

std::optional<SourceMesh> explode_object::ReadSourceMesh(uint32_t index, const std::vector<uint8_t>& bytes)
{
	l3d::L3DFile file;
	if (bytes.empty() || file.Open(bytes) != l3d::L3DResult::Success)
	{
		return std::nullopt;
	}
	SourceMesh mesh;
	mesh.meshId = resources::HashIdentifier(static_cast<MeshId>(index));
	const auto& headers = file.GetSubmeshHeaders();
	for (uint32_t s = 0; s < headers.size(); ++s)
	{
		auto& subMesh = mesh.subMeshes.emplace_back();
		std::memcpy(&subMesh.flags, &headers[s].flags, sizeof(subMesh.flags));
		const auto& vertices = file.GetVertexSpan(s);
		const auto& indices = file.GetIndexSpan(s);
		size_t firstVertex = 0;
		size_t firstIndex = 0;
		for (const auto& header : file.GetPrimitiveSpan(s))
		{
			auto& primitive = subMesh.primitives.emplace_back();
			for (uint32_t v = 0; v < header.numVertices && firstVertex + v < vertices.size(); ++v)
			{
				const auto& vertex = vertices[firstVertex + v];
				primitive.positions.emplace_back(vertex.position.x, vertex.position.y, vertex.position.z);
				primitive.uvs.emplace_back(vertex.texCoord.x, vertex.texCoord.y);
				primitive.normals.emplace_back(vertex.normal.x, vertex.normal.y, vertex.normal.z);
			}
			for (uint32_t t = 0; t < header.numTriangles && firstIndex + t * 3 + 2 < indices.size(); ++t)
			{
				const std::array<uint16_t, 3> triangle {indices[firstIndex + t * 3], indices[firstIndex + t * 3 + 1],
				                                        indices[firstIndex + t * 3 + 2]};
				if (triangle[0] < primitive.positions.size() && triangle[1] < primitive.positions.size() &&
				    triangle[2] < primitive.positions.size())
				{
					primitive.triangles.push_back(triangle);
				}
			}
			firstVertex += header.numVertices;
			firstIndex += header.numTriangles * 3;
		}
	}
	return mesh;
}

std::shared_ptr<const SourceMesh> explode_object::PackMesh(uint32_t index)
{
	if (!Locator::resources::has_value())
	{
		return nullptr;
	}
	// parsed once into the source mesh cache
	auto& meshes = Locator::resources::value().GetSourceMeshes();
	const auto id = entt::hashed_string(fmt::format("explode/{}", index).c_str()).value();
	if (!meshes.Contains(id))
	{
		try
		{
			meshes.Load(id, resources::SourceMeshLoader::FromBufferTag {}, index, PackMeshBytes(index));
		}
		catch (const std::exception&)
		{
			return nullptr;
		}
	}
	return meshes.Handle(id).handle();
}

// ---- the pieces ----

const Piece* explode_object::PieceOf(const Atom& atom)
{
	if (atom.creator != &g_PieceCreator)
	{
		return nullptr;
	}
	const auto it = atom.modifierData.find(&g_PieceKey);
	return it != atom.modifierData.end() ? static_cast<const Piece*>(it->second.get()) : nullptr;
}

uint32_t explode_object::LandLight(const glm::vec3& position)
{
	const auto& table = land_light::CurrentTable();
	// off the map or with no block, table[255]
	const uint32_t none = table.GetRaw(255);
	if (!Locator::terrainSystem::has_value())
	{
		return none;
	}
	const auto& island = Locator::terrainSystem::value();
	// x and z x 0.1, truncated. The port's cells count from the island's extent (as
	// vs_object's u_cellMap and RendererMists do); 0 for the original's maps
	const auto extent = island.GetExtent();
	const float fx = (position.x - extent.minimum.x) * 0.1f;
	const float fz = (position.z - extent.minimum.y) * 0.1f;
	const auto cx = static_cast<int32_t>(fx);
	const auto cz = static_cast<int32_t>(fz);
	const int32_t last = static_cast<int32_t>(island.GetCellsPerSide()) - 1; // 0x1FF
	if (cx < 0 || cx > last || cz < 0 || cz > last || !island.HasBlockAt(glm::u16vec2(cx, cz)))
	{
		return none;
	}
	// the cell and its neighbours of the 17 x 17 block; table[luminosity] of each. The original's lookup has two
	// outputs: these lights interpolated, and the cells' own colours (| 0xFF000000) interpolated the same way; the
	// pieces' draw keeps only the first (the second is never read), so only the lights are ported here.
	// (approximate) on the map's last row / column the original reads the block's 17th cell; the port clamps to
	// `last`
	const auto lightOf = [&](int32_t x, int32_t z) {
		const auto cell = glm::u16vec2(std::min(x, last), std::min(z, last));
		return table.GetRaw(island.GetCell(cell).luminosity);
	};
	const uint32_t l00 = lightOf(cx, cz);
	const uint32_t l01 = lightOf(cx, cz + 1);
	const uint32_t l10 = lightOf(cx + 1, cz);
	const uint32_t l11 = lightOf(cx + 1, cz + 1);
	// the weights, frac x 256 truncated
	const auto wx = static_cast<int32_t>((fx - static_cast<float>(cx)) * 256.0f);
	const auto wz = static_cast<int32_t>((fz - static_cast<float>(cz)) * 256.0f);
	// per channel a + ((b - a) w >> 8), then the channel's mask: the red, green and blue; the alpha comes out 0xFF
	const auto lerp = [](uint32_t a, uint32_t b, int32_t w) {
		uint32_t out = 0;
		for (const int shift : {16, 8, 0})
		{
			const auto ca = static_cast<int32_t>((a >> shift) & 0xFFu);
			const auto cb = static_cast<int32_t>((b >> shift) & 0xFFu);
			out |= (static_cast<uint32_t>(ca + ((cb - ca) * w >> 8)) & 0xFFu) << shift;
		}
		return out;
	};
	const uint32_t z0 = lerp(l00, l01, wz);
	const uint32_t z1 = lerp(l10, l11, wz);
	return lerp(z0, z1, wx) | 0xFF000000u;
}

uint32_t explode_object::LitColour(uint32_t argb, const glm::vec3& position)
{
	const uint32_t light = LandLight(position);
	uint32_t out = 0;
	for (const int shift : {24, 16, 8, 0})
	{
		out |= ((((argb >> shift) & 0xFFu) * ((light >> shift) & 0xFFu)) >> 8) << shift;
	}
	return out;
}

// ---- the pieces' draw ----

uint32_t mesh_pieces::DrawDataColour(const Effect::DrawAtom& atom)
{
	// the DrawData colour: the atom's colour and its alpha byte
	const auto alphaByte = static_cast<uint32_t>(std::clamp(atom.alpha, 0.0f, 255.0f));
	return (alphaByte << 24) | (static_cast<uint32_t>(atom.colour[0]) << 16) | (static_cast<uint32_t>(atom.colour[1]) << 8) |
	       atom.colour[2];
}

glm::mat4 mesh_pieces::DrawMatrix(const Effect::DrawAtom& atom)
{
	// the DrawData matrix: the PSR matrix (rotation x scale, the Y axis x the stretch): the LHMatrix rows are the columns here
	glm::mat3 axes = atom.rotation * atom.scale;
	axes[1] *= atom.stretch;
	glm::mat4 model(axes);
	model[3] = glm::vec4(atom.position, 1.0f);
	return model;
}

void mesh_pieces::AppendPiece(graphics::world_triangles::Frame& out, const Piece& piece, const glm::mat4& model, uint32_t argb,
                              const void* tag)
{
	namespace wt = graphics::world_triangles;
	const size_t count = piece.positions.size();
	if (count == 0 || !piece.source)
	{
		return;
	}
	// the pieces are land lit (ExplodeMesh sets it): the DrawData colour x the land light under the matrix's
	// translation, byte by byte
	const uint32_t base = LitColour(argb, glm::vec3(model[3]));
	// the model light needs the lighting switch (always on) and a normal per vertex
	const bool lit = piece.normals.size() == count;
	// the light as a point through the inverse of the drawn matrix, normalised unless null. (approximate)
	// model_light::LightInMeshSpace sums the squares x, y, z where the original does (x x + z z) + y y: the last bit of
	// the float may differ
	const glm::vec3 light = lit ? model_light::LightInMeshSpace(model) : glm::vec3(0.0f);
	const int ambient = model_light::Ambient();
	const glm::vec3 r0(model[0]);
	const glm::vec3 r1(model[1]);
	const glm::vec3 r2(model[2]);
	const glm::vec3 t(model[3]);
	std::vector<wt::Vertex> vertices(count);
	for (size_t i = 0; i < count; ++i)
	{
		const auto& v = piece.positions[i];
		// ((z r2 + y r1) + x r0) + t per axis, into the world
		const glm::vec3 world((v.z * r2.x + v.y * r1.x) + v.x * r0.x + t.x, (v.z * r2.y + v.y * r1.y) + v.x * r0.y + t.y,
		                      (v.z * r2.z + v.y * r1.z) + v.x * r0.z + t.z);
		uint32_t colour = base; // no colours of its own, all of the base colour
		if (lit)
		{
			// I = round(255 ((l.z n.z + l.y n.y) + l.x n.x)); then f and RGB x f >> 8,
			// the alpha kept
			const auto& n = piece.normals[i];
			const float dot = (light.z * n.z + light.y * n.y) + light.x * n.x;
			colour = model_light::Apply(base, model_light::Intensity(dot), ambient);
		}
		const glm::vec2 uv = i < piece.uvs.size() ? piece.uvs[i] : glm::vec2(0.0f);
		vertices[i] = {world, uv, wt::ToAbgr(colour)};
	}
	// a DrawData alpha byte not 0xFF -> the global alpha render mode table for this draw
	const auto alphaByte = static_cast<uint8_t>(argb >> 24);
	const auto table = alphaByte != 0xFFu ? graphics::render_modes::Table::GlobalAlpha : graphics::render_modes::Table::Normal;
	// drawn as world triangles with the source primitive's material.
	// (inferred) the pieces have no second colour set, so never the second-colour branch
	out.Append({piece.source->meshId, piece.subMesh, piece.primitive}, table, alphaByte, vertices, tag);
}

void mesh_pieces::Build(graphics::world_triangles::Frame& out, DrawPath path, const manager::FrameInputs& inputs)
{
	for (const auto& drawable : manager::Collect(Creator::Kind::MeshPiece, inputs))
	{
		if (drawable.path != path)
		{
			continue;
		}
		for (const auto& atom : drawable.atoms)
		{
			if (path == DrawPath::Sorted)
			{
				if (const auto* piece = atom.atom != nullptr ? PieceOf(*atom.atom) : nullptr; piece != nullptr)
				{
					// tagged with its atom: the main pass draws it at its place in SortedFrame::atOnce
					AppendPiece(out, *piece, DrawMatrix(atom), DrawDataColour(atom), atom.atom);
				}
			}
			else
			{
				BuildAtom(out, atom);
			}
		}
	}
}

void mesh_pieces::BuildAtom(graphics::world_triangles::Frame& out, const Effect::DrawAtom& atom)
{
	if (const auto* piece = atom.atom != nullptr ? PieceOf(*atom.atom) : nullptr; piece != nullptr)
	{
		AppendPiece(out, *piece, DrawMatrix(atom), DrawDataColour(atom), atom.atom);
	}
}

void explode_object::RegisterRules()
{
	RegisterModifier("UR_ExplodeObject", MakeModifierOf<ExplodeObject>);
	RegisterModifier("UR_ExplodeObject2", MakeModifierOf<ExplodeObject2>);
}
