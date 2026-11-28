/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <DanceFile.h>
#include <VanillaWorld.h>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Animals/AnimalMove.h"
#include "Animals/BirdRules.h"
#include "Animals/GrazerRules.h"
#include "Camera/CameraZoomer.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Systems/Implementations/AnimalSystemDetail.h"
#include "ECS/VanillaCameraImport.h"
#include "ECS/VanillaFootpathImport.h"
#include "ECS/VanillaParticleImport.h"
#include "ECS/VanillaPlayerImport.h"
#include "ECS/VanillaReactionImport.h"
#include "ECS/VanillaSceneryImport.h"
#include "ECS/VanillaScriptImport.h"
#include "ECS/VanillaWorldImport.h"
#include "ECS/WallHugRules.h"

namespace camera_import = openblack::ecs::vanilla_camera_import;
namespace particle_import = openblack::ecs::vanilla_particle_import;
namespace world_import = openblack::ecs::vanilla_world_import;

namespace vanilla_import_test
{

using namespace openblack;
using namespace openblack::gamesave;

/// Makes a small world of records with named values, as the reader keeps them
class FakeWorld
{
public:
	/// Starts a new record. The first record is the game object.
	FakeWorld& Record(VanillaType type)
	{
		_records.push_back({.number = static_cast<uint32_t>(_records.size() + 1), .type = static_cast<uint32_t>(type)});
		return *this;
	}

	template <typename T>
	FakeWorld& Value(std::string_view name, T value)
	{
		const auto at = _bytes.size();
		_bytes.resize(at + sizeof(T));
		std::memcpy(_bytes.data() + at, &value, sizeof(T));
		_records.back().fields.push_back({.name = name, .kind = VanillaValue::Bytes, .at = at, .size = sizeof(T)});
		return *this;
	}

	FakeWorld& Position(int32_t x, int32_t z) { return Value("position", MapCoords {.x = x, .z = z, .altitude = 0.0f}); }

	FakeWorld& Ref(std::string_view name, uint32_t number)
	{
		_records.back().fields.push_back({.name = name, .kind = VanillaValue::Reference, .number = number});
		return *this;
	}

	FakeWorld& Refs(std::string_view name, std::vector<uint32_t> numbers)
	{
		_records.back().fields.push_back({.name = name,
		                                  .kind = VanillaValue::References,
		                                  .count = static_cast<uint32_t>(numbers.size()),
		                                  .number = static_cast<uint32_t>(_numbers.size())});
		_numbers.insert(_numbers.end(), numbers.begin(), numbers.end());
		return *this;
	}

	FakeWorld& Words(std::string_view name, std::vector<uint32_t> words)
	{
		const auto at = _bytes.size();
		for (const auto word : words)
		{
			Value(name, word);
		}
		// The reader keeps an array as one value, with the place of its first value
		auto& fields = _records.back().fields;
		fields.resize(fields.size() - words.size());
		fields.push_back({.name = name,
		                  .kind = VanillaValue::Array,
		                  .at = at,
		                  .size = sizeof(uint32_t),
		                  .count = static_cast<uint32_t>(words.size())});
		return *this;
	}

	/// Gives the record an owner that refers to the player record of the player. Make() uses the player record with
	/// that number, or adds one at the end.
	FakeWorld& OwnedBy(int player)
	{
		Ref("owner", 0);
		_owners.push_back({.record = _records.size() - 1,
		                   .field = _records.back().fields.size() - 1,
		                   .player = static_cast<uint8_t>(player)});
		return *this;
	}

	[[nodiscard]] VanillaWorld Make() const
	{
		auto copy = *this;
		copy.ResolveOwners();
		return {copy._bytes, VanillaFraming {}, copy._records, copy._numbers};
	}

private:
	struct Owner
	{
		size_t record;
		size_t field;
		uint8_t player;
	};

	[[nodiscard]] std::optional<uint32_t> PlayerRecord(uint8_t player) const
	{
		const auto found = std::ranges::find_if(_records, [&](const VanillaRecord& record) {
			return record.type == static_cast<uint32_t>(VanillaType::Player) &&
			       std::ranges::any_of(record.fields, [&](const VanillaField& field) {
				       return field.name == "number" && field.kind == VanillaValue::Bytes && field.size == 1 &&
				              _bytes.at(field.at) == player;
			       });
		});
		return found != _records.end() ? std::optional(found->number) : std::nullopt;
	}

	void ResolveOwners()
	{
		for (const auto& owner : _owners)
		{
			auto number = PlayerRecord(owner.player);
			if (!number.has_value())
			{
				Record(VanillaType::Player).Value<uint8_t>("number", owner.player);
				number = _records.back().number;
			}
			_records.at(owner.record).fields.at(owner.field).number = *number;
		}
		_owners.clear();
	}

	std::vector<uint8_t> _bytes;
	std::vector<VanillaRecord> _records;
	std::vector<uint32_t> _numbers;
	std::vector<Owner> _owners;
};

/// A game object with the turn in its game data
inline FakeWorld GameAtTurn(uint32_t turn)
{
	FakeWorld world;
	world.Record(VanillaType::Game);
	for (const uint32_t word : std::array<uint32_t, 8> {11, 22, turn, 0, 40, 0, 0, 3})
	{
		world.Value("data", word);
	}
	return world;
}

constexpr uint32_t AbodeRowNumber(AbodeInfo row)
{
	return world_import::k_FirstAbodeRowNumber - static_cast<uint32_t>(row);
}

constexpr uint32_t VillagerRowNumber(VillagerInfo row)
{
	return world_import::k_FirstVillagerRowNumber - static_cast<uint32_t>(row);
}

/// Calls the function with each world file of any land in the copy of a Profiles folder that OPENBLACK_VANILLA_SAVES
/// gives. Gives the number of files, or nothing when there is no such folder.
template <typename Check>
std::optional<uint32_t> ForEachSave(Check&& check)
{
	// Read only, and no other thread changes the environment
	const char* saves = std::getenv("OPENBLACK_VANILLA_SAVES"); // NOLINT(concurrency-mt-unsafe)
	std::error_code error;
	if (saves == nullptr || !std::filesystem::is_directory(saves, error))
	{
		return std::nullopt;
	}
	uint32_t files = 0;
	for (const auto& entry : std::filesystem::recursive_directory_iterator(saves))
	{
		const auto name = entry.path().filename().string();
		if (!entry.is_regular_file() || entry.path().extension() != ".sav" || name.starts_with("Script"))
		{
			continue;
		}
		std::ifstream stream(entry.path(), std::ios::binary);
		const std::vector<uint8_t> bytes {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
		const auto read = ReadVanillaWorld(bytes);
		EXPECT_TRUE(read.world.has_value()) << entry.path();
		if (read.world.has_value())
		{
			++files;
			check(entry.path(), *read.world);
		}
	}
	return files;
}

/// Calls the function with each world file of Land 1 in the copy of a Profiles folder that OPENBLACK_VANILLA_SAVES
/// gives. Gives the number of files, or nothing when there is no such folder.
template <typename Check>
std::optional<uint32_t> ForEachLandOneSave(Check&& check)
{
	// Read only, and no other thread changes the environment
	const char* saves = std::getenv("OPENBLACK_VANILLA_SAVES"); // NOLINT(concurrency-mt-unsafe)
	std::error_code error;
	if (saves == nullptr || !std::filesystem::is_directory(saves, error))
	{
		return std::nullopt;
	}
	uint32_t files = 0;
	for (const auto& entry : std::filesystem::recursive_directory_iterator(saves))
	{
		const auto name = entry.path().filename().string();
		if (!entry.is_regular_file() || entry.path().extension() != ".sav" || name.starts_with("Script"))
		{
			continue;
		}
		std::ifstream stream(entry.path(), std::ios::binary);
		const std::vector<uint8_t> bytes {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
		const auto read = ReadVanillaWorld(bytes);
		EXPECT_TRUE(read.world.has_value()) << entry.path();
		if (!read.world.has_value() || read.world->Framing().landscape.find("Land1.lnd") == std::string::npos)
		{
			continue;
		}
		++files;
		check(entry.path(), *read.world);
	}
	return files;
}

/// The record types of the animals of Land 1 and the row of the table of animals that each has
constexpr std::array<std::pair<uint32_t, AnimalInfo>, 9> k_LandOneAnimals = {{
    {0x82, AnimalInfo::Cow},
    {0x83, AnimalInfo::Sheep},
    {0x85, AnimalInfo::Horse},
    {0x87, AnimalInfo::Pig},
    {0x88, AnimalInfo::Tortoise},
    {0x8E, AnimalInfo::Dove},
    {0x91, AnimalInfo::Swallow},
    {0x93, AnimalInfo::Seagull},
    {0x94, AnimalInfo::Bat},
}};

/// Every object of a saved game becomes an entity of the same number, so that a test sees which object a value holds
inline entt::entity SameNumber(uint32_t number)
{
	return static_cast<entt::entity>(number);
}

/// A value of a record as the file keeps it, from its bytes
template <typename T>
T FileValue(const VanillaWorld& world, const VanillaRecord& record, std::string_view name)
{
	T value {};
	const auto bytes = world.Raw(record, name);
	EXPECT_GE(bytes.size(), sizeof(T)) << record.number << " " << name;
	if (bytes.size() >= sizeof(T))
	{
		std::memcpy(&value, bytes.data(), sizeof(T));
	}
	return value;
}

inline glm::vec3 FilePoint(const VanillaWorld& world, const VanillaRecord& record, std::string_view name)
{
	const auto point = FileValue<std::array<float, 3>>(world, record, name);
	return {point[0], point[1], point[2]};
}

/// The values of the file that the import took, with how many of each group it checked
struct Checked
{
	uint32_t containers {0};
	uint32_t effects {0};
	uint32_t collections {0};
	uint32_t atoms {0};
	uint32_t emitters {0};
};

inline void CheckAtom(const VanillaWorld& world, const VanillaRecord& record, const particles::RestoredAtom& atom,
                      Checked& checked);

/// The collection of the import agrees with each value of its record
inline void CheckCollection(const VanillaWorld& world, uint32_t number, const particles::RestoredCollection& collection,
                            Checked& checked)
{
	const auto* record = world.Record(number);
	ASSERT_NE(record, nullptr);
	ASSERT_TRUE(record->Is(VanillaType::AtomCollection));
	++checked.collections;
	EXPECT_EQ(collection.group, static_cast<int>(FileValue<uint32_t>(world, *record, "collectionGroup")));
	EXPECT_EQ(collection.birth, FileValue<float>(world, *record, "collectionBirth"));
	EXPECT_EQ(collection.alpha, static_cast<float>(FileValue<uint8_t>(world, *record, "collectionAlpha")));
	const auto flags = FileValue<uint8_t>(world, *record, "collectionFlags");
	EXPECT_EQ(collection.hierarchy, (flags & 1U) == 0);
	EXPECT_EQ(collection.interpolated, (flags & 2U) != 0);
	EXPECT_EQ(collection.emitting, (flags & 4U) != 0);
	// A collection is in a hierarchy when the group of an atom over it is a hierarchy, as the file's hierarchy value says
	if (const auto* parent = world.Record(world.Ref(*record, "collectionParent").value_or(0)); parent != nullptr)
	{
		const auto* above = world.Record(world.Ref(*parent, "atomCollection").value_or(0));
		ASSERT_NE(above, nullptr);
		const bool aboveHierarchy = FileValue<uint32_t>(world, *above, "collectionHierarchy") != 0 ||
		                            (FileValue<uint8_t>(world, *above, "collectionFlags") & 1U) == 0;
		EXPECT_EQ(collection.hierarchy, aboveHierarchy) << number;
	}
	const auto rules = world.Fields(*record, "rule");
	ASSERT_EQ(collection.rules.size(), rules.size());
	for (size_t i = 0; i < rules.size(); ++i)
	{
		uint32_t rule = 0;
		std::memcpy(&rule, world.Bytes().data() + rules[i]->at, sizeof(rule));
		EXPECT_EQ(collection.rules[i], static_cast<int>(rule));
	}
	const auto data = world.Refs(*record, "ruleData");
	ASSERT_EQ(collection.emitters.size(), data.size());
	for (size_t i = 0; i < data.size(); ++i)
	{
		const auto* emitter = world.Record(data[i]);
		ASSERT_NE(emitter, nullptr);
		++checked.emitters;
		const auto& clock = collection.emitters[i];
		EXPECT_EQ(clock.rule, static_cast<int>(FileValue<uint32_t>(world, *emitter, "dataRule")));
		EXPECT_EQ(clock.clock.next, FileValue<float>(world, *emitter, "emitterNext"));
		EXPECT_EQ(clock.clock.emitted, static_cast<int>(FileValue<uint32_t>(world, *emitter, "emitterEmitted")));
		EXPECT_EQ(clock.clock.started, FileValue<uint8_t>(world, *emitter, "emitterNotStarted") == 0);
		// An emitter of the collection is one of its rules
		EXPECT_NE(std::ranges::find(collection.rules, clock.rule), collection.rules.end());
	}
	// The file keeps the newest atom first, the import the oldest first
	const auto atoms = world.Refs(*record, "collectionAtoms");
	ASSERT_EQ(collection.atoms.size(), atoms.size());
	for (size_t i = 0; i < atoms.size(); ++i)
	{
		const auto* atom = world.Record(atoms[atoms.size() - 1 - i]);
		ASSERT_NE(atom, nullptr);
		CheckAtom(world, *atom, collection.atoms[i], checked);
		if (i > 0)
		{
			EXPECT_LE(collection.atoms[i - 1].birth, collection.atoms[i].birth) << "the oldest atom first";
		}
	}
}

inline void CheckDrawState(std::span<const uint8_t> bytes, const particles::DrawState& state, float frame)
{
	ASSERT_EQ(bytes.size(), 56U);
	std::array<float, 13> floats {};
	std::memcpy(floats.data(), bytes.data(), sizeof(floats));
	EXPECT_EQ(state.position, glm::vec3(floats[9], floats[10], floats[11]));
	EXPECT_EQ(state.scale, floats[12]);
	EXPECT_EQ(state.alpha, static_cast<float>(bytes[52]));
	EXPECT_EQ(state.frame, frame);
	// The file keeps the rotation times the scale
	for (int row = 0; row < 3; ++row)
	{
		for (int column = 0; column < 3; ++column)
		{
			EXPECT_FLOAT_EQ(state.rotation[row][column] * state.scale, floats[static_cast<size_t>(row * 3 + column)]);
		}
	}
}

inline void CheckAtom(const VanillaWorld& world, const VanillaRecord& record, const particles::RestoredAtom& atom,
                      Checked& checked)
{
	ASSERT_TRUE(record.Is(VanillaType::AtomCore));
	++checked.atoms;
	EXPECT_EQ(atom.position, FilePoint(world, record, "atomPosition"));
	EXPECT_EQ(atom.velocity, FilePoint(world, record, "atomVelocity"));
	EXPECT_EQ(atom.birth, FileValue<float>(world, record, "atomBirth"));
	const auto rotation = FileValue<std::array<float, 12>>(world, record, "atomRotation");
	for (int row = 0; row < 3; ++row)
	{
		for (int column = 0; column < 3; ++column)
		{
			EXPECT_EQ(atom.rotation[row][column], rotation[static_cast<size_t>(row * 3 + column)]);
		}
	}
	EXPECT_EQ(atom.baseScale, FileValue<float>(world, record, "atomBaseScale"));
	EXPECT_EQ(atom.ruleScale, FileValue<float>(world, record, "atomRuleScale"));
	EXPECT_EQ(atom.stretch, FileValue<float>(world, record, "atomStretch"));
	const auto colour = FileValue<std::array<uint8_t, 4>>(world, record, "atomColour");
	EXPECT_EQ(atom.rgba, (std::array<uint8_t, 4> {colour[2], colour[1], colour[0], colour[3]}));
	const auto specular = FileValue<uint32_t>(world, record, "atomSpecular");
	EXPECT_EQ(atom.specular, (std::array<uint8_t, 3> {static_cast<uint8_t>(specular >> 16U),
	                                                  static_cast<uint8_t>(specular >> 8U), static_cast<uint8_t>(specular)}));
	const auto flags = FileValue<uint8_t>(world, record, "atomFlags");
	EXPECT_EQ(atom.drawOnFirstUpdate, (flags & 1U) != 0);
	EXPECT_EQ(atom.steps, (flags >> 1U) & 3U);
	EXPECT_EQ(atom.deflected, (flags & 8U) != 0);
	EXPECT_EQ(atom.visible, (flags & 16U) != 0);
	CheckDrawState(world.Raw(record, "atomPrevious"), atom.previous, FileValue<float>(world, record, "atomPreviousFrame"));
	CheckDrawState(world.Raw(record, "atomCurrent"), atom.current, FileValue<float>(world, record, "atomFrame"));
	const auto animation = world.Raw(record, "atomAnimation");
	ASSERT_EQ(animation.size(), 12U);
	EXPECT_EQ(atom.frameRate, FileValue<float>(world, record, "atomAnimation"));
	EXPECT_EQ(atom.playAnim, animation[8] != 0);
	EXPECT_EQ(atom.gravity, FileValue<float>(world, record, "atomGravity"));
	EXPECT_EQ(atom.random, FileValue<uint8_t>(world, record, "atomRandom"));
	const auto* render = world.Record(world.Ref(record, "atomRender").value_or(0));
	ASSERT_NE(render, nullptr);
	switch (atom.look)
	{
	case particles::RestoredLook::Sprite:
		ASSERT_TRUE(render->Is(VanillaType::SpriteParticle));
		EXPECT_EQ(atom.creator, static_cast<int>(FileValue<uint32_t>(world, *render, "spriteCreator")));
		// The sprite keeps the number of the particle file of its effect with its creator
		EXPECT_NE(FileValue<uint32_t>(world, *render, "spriteCreatorFile"), 0U);
		break;
	case particles::RestoredLook::GoldenShower:
	case particles::RestoredLook::ObjectRef:
		EXPECT_TRUE(render->Is(VanillaType::GoldenShowerParticle) || render->Is(VanillaType::ObjectRefParticle));
		EXPECT_EQ(atom.object, SameNumber(world.Ref(*render, "objectRef").value_or(0)));
		break;
	case particles::RestoredLook::Point:
		EXPECT_TRUE(render->Is(VanillaType::PointParticle));
		break;
	}
	const auto collections = world.Refs(record, "atomCollections");
	ASSERT_EQ(atom.collections.size(), collections.size());
	for (size_t i = 0; i < collections.size(); ++i)
	{
		CheckCollection(world, collections[collections.size() - 1 - i], atom.collections[i], checked);
	}
}

inline size_t CountAtoms(const particles::RestoredCollection& collection)
{
	size_t count = collection.atoms.size();
	for (const auto& atom : collection.atoms)
	{
		for (const auto& sub : atom.collections)
		{
			count += CountAtoms(sub);
		}
	}
	return count;
}

/// The effect of the import agrees with each value of its particle system
inline void CheckEffect(const VanillaWorld& world, uint32_t system, const particle_import::ImportedEffect& effect,
                        Checked& checked)
{
	const auto* interface = world.Record(system);
	ASSERT_NE(interface, nullptr);
	const auto* manager = world.Record(world.Ref(*interface, "systemManager").value_or(0));
	ASSERT_NE(manager, nullptr);
	++checked.effects;
	const auto& state = effect.state;
	EXPECT_EQ(static_cast<uint32_t>(effect.type), FileValue<uint32_t>(world, *manager, "managerType"));
	EXPECT_EQ(effect.synced, FileValue<uint8_t>(world, *manager, "managerSynced") != 0);
	EXPECT_EQ(state.atomCount, FileValue<uint32_t>(world, *manager, "managerAtomCount"));
	EXPECT_EQ(state.origin, FilePoint(world, *manager, "managerOrigin"));
	EXPECT_EQ(state.globalAlpha, static_cast<float>(FileValue<uint8_t>(world, *manager, "managerAlpha")));
	EXPECT_EQ(state.age, FileValue<float>(world, *manager, "managerAge"));
	EXPECT_EQ(state.closing, (FileValue<uint32_t>(world, *manager, "managerFlags") & 1U) != 0);
	EXPECT_EQ(state.direction, FilePoint(world, *manager, "managerDirection"));
	EXPECT_EQ(state.magnitude, FileValue<float>(world, *manager, "managerMagnitude"));
	EXPECT_EQ(state.closeAge, FileValue<float>(world, *manager, "managerCloseAge"));
	EXPECT_EQ(state.firstStepTwice, FileValue<uint8_t>(world, *manager, "managerStepTwice") != 0);
	// The process info: three words that are 0 in every save, the hand, the camera, the direction, the power, the spin
	// and the enabled byte
	const auto info = FileValue<std::array<float, 15>>(world, *manager, "managerInfo");
	const auto infoWords = FileValue<std::array<uint32_t, 15>>(world, *manager, "managerInfo");
	EXPECT_EQ(infoWords[0], 0U);
	EXPECT_EQ(infoWords[1], 0U);
	EXPECT_EQ(infoWords[2], 0U);
	EXPECT_EQ(state.info.handPosition, glm::vec3(info[3], info[4], info[5]));
	EXPECT_EQ(state.info.cameraForward, glm::vec3(info[6], info[7], info[8]));
	EXPECT_EQ(state.info.direction, glm::vec3(info[9], info[10], info[11]));
	EXPECT_EQ(state.info.power, info[12]);
	EXPECT_EQ(state.info.spin, info[13]);
	EXPECT_EQ(state.info.enabled, (infoWords[14] & 0xFFU) != 0);
	// The flags keep a copy of whether the miracle is still cast
	EXPECT_EQ(state.info.enabled, (FileValue<uint32_t>(world, *manager, "managerFlags") & 2U) != 0);
	EXPECT_EQ(state.targets.size(), FileValue<uint32_t>(world, *manager, "targetObjectCount"));
	EXPECT_EQ(state.targetPositions.size(), FileValue<uint32_t>(world, *manager, "targetPointCount"));
	if (const auto player = world.Ref(*manager, "managerPlayer").value_or(0); player != 0)
	{
		EXPECT_EQ(state.player, static_cast<int>(world.Byte(*world.Record(player), "number").value_or(0xFF)));
	}
	else
	{
		EXPECT_EQ(state.player, -1);
	}
	const auto roots = world.Refs(*manager, "managerRoots");
	ASSERT_EQ(state.roots.size(), roots.size());
	size_t atoms = 0;
	for (size_t i = 0; i < roots.size(); ++i)
	{
		CheckCollection(world, roots[roots.size() - 1 - i], state.roots[i], checked);
		atoms += CountAtoms(state.roots[i]);
	}
	// The effect counts every atom of its tree
	EXPECT_EQ(state.atomCount, atoms);
}

inline world_import::ImportedStateValue BytesValue(std::vector<uint8_t> bytes)
{
	return {.object = std::nullopt, .bytes = std::move(bytes)};
}

inline world_import::ImportedStateValue ObjectValue(uint32_t number)
{
	return {.object = number, .bytes = {}};
}

template <typename T>
T BytesAs(const world_import::ImportedStateValue& value)
{
	T result {};
	std::memcpy(&result, value.bytes.data(), sizeof(T));
	return result;
}

/// The 12 numbers that the file keeps for a glide of the game, from a glide of the camera that is part of its way
inline std::array<float, 12> GlideNumbers(const camera::Zoomer& zoomer)
{
	std::array<float, 12> numbers {};
	camera::Zoomer::VisitSaved(zoomer, [&numbers](float value, float speed, float destination, float destinationSpeed,
	                                              float time, float duration, float startValue, float startSpeed, float c1,
	                                              float c2, float c3) {
		numbers = {value, destination, destinationSpeed, speed, 0.0f, time, duration, startValue, startSpeed, c1, c2, c3};
	});
	return numbers;
}

inline camera_import::Glide GlideOfNumbers(const std::array<float, 12>& numbers)
{
	std::array<uint8_t, camera_import::k_GlideBytes> bytes {};
	std::memcpy(bytes.data(), numbers.data(), bytes.size());
	return camera_import::GlideOf(bytes);
}

/// A glide from a value to a destination over some seconds, which has gone on for some of them
inline camera::Zoomer GlideFrom(float from, float to, float seconds, float elapsed)
{
	camera::Zoomer zoomer;
	zoomer.SetPosition(from);
	zoomer.SetDestination(to, 0.0f, seconds);
	zoomer.Update(elapsed);
	return zoomer;
}

/// A world whose camera has a mode, with still glides
inline FakeWorld WorldWithCameraMode(uint32_t mode)
{
	std::array<float, 36> still {};
	for (size_t axis = 0; axis < 3; ++axis)
	{
		std::ranges::copy(GlideNumbers(GlideFrom(1.0f, 1.0f, 0.0f, 0.0f)),
		                  still.begin() + static_cast<std::ptrdiff_t>(axis * 12));
	}
	auto world = GameAtTurn(1);
	world.Record(VanillaType::Camera)
	    .Value("cameraFocusGlide", still)
	    .Value("cameraEyeGlide", still)
	    .Value("cameraLensGlide", GlideNumbers(GlideFrom(1.0f, 1.0f, 0.0f, 0.0f)))
	    .Value<uint8_t>("cameraBelowLand", 0)
	    .Value<uint32_t>("mode", mode);
	return world;
}

} // namespace vanilla_import_test
