/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "StateHash.h"

#include <cstdlib>

#include <algorithm>
#include <bit>
#include <fstream>
#include <iterator>

#include <spdlog/spdlog.h>

#include "Common/GameRandom.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "GameClock.h"
#include "Locator.h"

namespace openblack::state_hash
{
namespace
{
struct State
{
	bool read {false};
	bool enabled {false};
	bool builtIns {false};
	std::string file;
	std::vector<std::pair<std::string, Part>> parts;
	std::vector<TurnHash> taken;
};

/// The state hash's state (Locator::debugHooks)
State& S()
{
	return openblack::Locator::debugHooks::value().Get<State>();
}

/// One file per run: emptied when the hash starts
void Truncate(const std::string& file)
{
	const std::ofstream truncate(file, std::ios::trunc);
}

void ReadEnvironment()
{
	auto& s = S();
	if (s.read)
	{
		return;
	}
	s.read = true;
	if (const char* file = std::getenv("OPENBLACK_STATE_HASH"); file != nullptr && *file != '\0')
	{
		s.enabled = true;
		s.file = file;
		Truncate(s.file);
	}
}

/// The parts every game has, registered before the owners' ones
void RegisterBuiltIns()
{
	auto& s = S();
	if (s.builtIns)
	{
		return;
	}
	s.builtIns = true;
	std::vector<std::pair<std::string, Part>> builtIns;
	// the synced and local seeds and the particle stream (Common/GameRandom.h): the first to go when two games part
	builtIns.emplace_back("random", [](Hasher& h) {
		const auto& seeds = game_random::Current();
		h.U32(seeds.synced);
		h.U32(seeds.local);
		h.U32(static_cast<uint32_t>(game_random::psys::Active()));
	});
	// the CRT seed apart: most of its draws are the draw's (clouds, night lights, smoke, camera shake), so a difference
	// only here may be a picture's, not the game's
	builtIns.emplace_back("crt", [](Hasher& h) { h.U32(game_random::crt::Seed()); });
	// the game's clock (GameClock.h): the turn, pause and speed (the frame's values depend on the frame, not hashed)
	builtIns.emplace_back("clock", [](Hasher& h) {
		h.U32(game_clock::Turn());
		h.U32(game_clock::IsPaused() ? 1 : 0);
		h.Float(game_clock::Speed());
		h.U32(game_clock::MsPerTurn());
	});
	// every storage of the registry (the entities' own first): its size and its entities in their order (creations,
	// deletions, versions, the order the systems walk), folded by storage id so that the order the storages were first
	// used in (which the draw can change) does not count
	builtIns.emplace_back("pools", [](Hasher& h) {
		if (!Locator::entitiesRegistry::has_value())
		{
			return;
		}
		std::vector<std::pair<entt::id_type, uint64_t>> storages;
		Locator::entitiesRegistry::value().EachStorage([&storages](entt::id_type id, const entt::sparse_set& storage) {
			Hasher one;
			one.U64(storage.size());
			for (const auto entity : storage)
			{
				one.U32(static_cast<uint32_t>(entity));
			}
			storages.emplace_back(id, one.Value());
		});
		std::sort(storages.begin(), storages.end());
		for (const auto& [id, value] : storages)
		{
			h.U32(static_cast<uint32_t>(id));
			h.U64(value);
		}
	});
	// the bits of every Transform, in the storage's order (read through a const registry: no storage is created)
	builtIns.emplace_back("transform", [](Hasher& h) {
		if (!Locator::entitiesRegistry::has_value())
		{
			return;
		}
		const auto& registry = Locator::entitiesRegistry::value();
		registry.Each<const ecs::components::Transform>([&h](entt::entity entity, const ecs::components::Transform& transform) {
			h.U32(static_cast<uint32_t>(entity));
			h.Vec3(transform.position);
			h.Mat3(transform.rotation);
			h.Vec3(transform.scale);
		});
	});
	s.parts.insert(s.parts.begin(), std::make_move_iterator(builtIns.begin()), std::make_move_iterator(builtIns.end()));
}

} // namespace

void Hasher::Bytes(std::span<const std::byte> bytes) noexcept
{
	for (const auto byte : bytes)
	{
		_value ^= static_cast<uint64_t>(byte);
		_value *= 0x100000001B3ull;
	}
}

void Hasher::U32(uint32_t value) noexcept
{
	Bytes(std::as_bytes(std::span(&value, 1)));
}

void Hasher::U64(uint64_t value) noexcept
{
	Bytes(std::as_bytes(std::span(&value, 1)));
}

void Hasher::Float(float value) noexcept
{
	U32(std::bit_cast<uint32_t>(value));
}

void Hasher::Vec3(const glm::vec3& value) noexcept
{
	Float(value.x);
	Float(value.y);
	Float(value.z);
}

void Hasher::Mat3(const glm::mat3& value) noexcept
{
	for (int column = 0; column < 3; ++column)
	{
		Vec3(value[column]);
	}
}

void Register(std::string_view name, Part part)
{
	// a part of the same name is replaced (a second Game in the same process registers again)
	auto& parts = S().parts;
	const auto it = std::find_if(parts.begin(), parts.end(), [name](const auto& entry) { return entry.first == name; });
	if (it != parts.end())
	{
		it->second = std::move(part);
		return;
	}
	parts.emplace_back(std::string(name), std::move(part));
}

bool Enabled()
{
	ReadEnvironment();
	return S().enabled;
}

void Enable(std::string file)
{
	ReadEnvironment();
	auto& s = S();
	s.enabled = true;
	if (!file.empty())
	{
		s.file = std::move(file);
		Truncate(s.file);
	}
}

void OnTurnEnd(uint32_t turn)
{
	if (!Enabled())
	{
		return;
	}
	RegisterBuiltIns();
	auto& s = S();
	TurnHash result {turn, 0, {}};
	Hasher total;
	for (const auto& [name, part] : s.parts)
	{
		Hasher h;
		part(h);
		result.parts.emplace_back(name, h.Value());
		total.U64(h.Value());
	}
	result.total = total.Value();
	if (!s.file.empty())
	{
		std::ofstream out(s.file, std::ios::app);
		out << "turn " << turn << ' ' << std::hex << result.total;
		for (const auto& [name, value] : result.parts)
		{
			out << ' ' << name << '=' << value;
		}
		out << std::dec << '\n';
	}
	s.taken.push_back(std::move(result));
}

const std::vector<TurnHash>& Taken()
{
	return S().taken;
}

void Clear()
{
	S().taken.clear();
}

} // namespace openblack::state_hash
