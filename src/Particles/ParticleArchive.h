/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <algorithm>
#include <memory>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <glm/vec3.hpp>

#include "ECS/Reflection/PlainImage.h"
#include "ParticleEffect.h"

namespace openblack::ecs::reflection
{
class BinaryWriter;
class BinaryReader;
} // namespace openblack::ecs::reflection

/// The running state of the particle effects in a saved game (deviation 83).
///
/// An effect goes in as the tree of its collections and atoms, then the state that each rule keeps for them. The atoms
/// and the collections of one effect get numbers in the order of the tree, so that a rule can refer to them. The things
/// that effects share with the world and with each other (sounds, bolts, shields, sheets of light, carried objects,
/// gesture trails) go in tables, once each, so that every holder finds the same one again after a load.
namespace openblack::particles
{

/// The things of one kind that a save shares between their holders, by number
template <typename Type>
struct SharedTable
{
	std::vector<std::shared_ptr<Type>> items;
	std::unordered_map<const Type*, uint32_t> numbers;

	/// The number of the thing, which it gets when it is new; 0 for none
	uint32_t Add(const std::shared_ptr<Type>& item)
	{
		if (!item)
		{
			return 0;
		}
		const auto [found, added] = numbers.try_emplace(item.get(), static_cast<uint32_t>(items.size() + 1));
		if (added)
		{
			items.push_back(item);
		}
		return found->second;
	}
	/// The thing with a number, none for 0 or a number that is not in the table
	[[nodiscard]] std::shared_ptr<Type> At(uint32_t number) const
	{
		return number > 0 && number <= items.size() ? items[number - 1] : nullptr;
	}
};

/// The tables of the things that effects share
struct SharedTables
{
	SharedTable<ParticleSoundLink> sounds;
	SharedTable<BoltShare> bolts;
	SharedTable<ShieldSphere> shields;
	SharedTable<LightSheet> sheets;
	SharedTable<CarriedObject> carried;
	SharedTable<GestureTrail> trails;

	template <typename Type>
	[[nodiscard]] SharedTable<Type>& Of()
	{
		if constexpr (std::is_same_v<Type, ParticleSoundLink>)
		{
			return sounds;
		}
		else if constexpr (std::is_same_v<Type, BoltShare>)
		{
			return bolts;
		}
		else if constexpr (std::is_same_v<Type, ShieldSphere>)
		{
			return shields;
		}
		else if constexpr (std::is_same_v<Type, LightSheet>)
		{
			return sheets;
		}
		else if constexpr (std::is_same_v<Type, CarriedObject>)
		{
			return carried;
		}
		else
		{
			static_assert(std::is_same_v<Type, GestureTrail>);
			return trails;
		}
	}
};

/// Writes effects into a save
class EffectWriter
{
public:
	EffectWriter(ecs::reflection::BinaryWriter& out, SharedTables& tables)
	    : _out(&out)
	    , _tables(&tables)
	{
	}

	[[nodiscard]] ecs::reflection::BinaryWriter& Out() { return *_out; }
	[[nodiscard]] SharedTables& Tables() { return *_tables; }

	// The values that the rules keep
	void Write(bool value);
	void Write(int32_t value);
	void Write(uint32_t value);
	void Write(uint64_t value);
	void Write(float value);
	void Write(glm::vec3 value);
	void Write(entt::entity value);
	/// A count of 32 bits
	void WriteSize(size_t size);
	void WriteBytes(const void* bytes, size_t size);
	/// Any value that the archive can keep, in the files that include ParticleArchiveValues.h
	template <typename Type>
	void WriteValue(const Type& value);
	/// The bytes of a plain value that holds no pointer
	template <typename Type>
	    requires std::is_trivially_copyable_v<Type>
	void WritePlain(const Type& value)
	{
		// The gaps between the fields are zero, so that the same value always gives the same bytes
		const auto image = ecs::reflection::PlainImage(value);
		WriteBytes(image.bytes.data(), sizeof(Type));
	}
	template <typename Type>
	    requires std::is_trivially_copyable_v<Type>
	void WritePlainList(const std::vector<Type>& values)
	{
		WriteSize(values.size());
		if constexpr (std::is_class_v<Type>)
		{
			for (const auto& value : values)
			{
				const auto image = ecs::reflection::PlainImage(value);
				WriteBytes(image.bytes.data(), sizeof(Type));
			}
		}
		else
		{
			WriteBytes(values.data(), values.size() * sizeof(Type));
		}
	}

	/// The effect's atoms and collections start their numbers again
	void StartEffect(const Effect& effect)
	{
		_effect = &effect;
		_atoms.clear();
		_collections.clear();
	}
	[[nodiscard]] const Effect& CurrentEffect() const { return *_effect; }
	void NumberAtom(const Atom* atom) { _atoms.try_emplace(atom, static_cast<uint32_t>(_atoms.size() + 1)); }
	void NumberCollection(const Collection* collection)
	{
		_collections.try_emplace(collection, static_cast<uint32_t>(_collections.size() + 1));
	}
	/// An atom or a collection of the effect by its number; 0 for none or one that has no number
	void WriteAtom(const Atom* atom) { Write(NumberOf(_atoms, atom)); }
	[[nodiscard]] uint32_t AtomNumber(const Atom* atom) const { return NumberOf(_atoms, atom); }
	void WriteCollection(const Collection* collection) { Write(NumberOf(_collections, collection)); }
	/// A shared thing by its number in its table
	template <typename Type>
	void WriteShared(const std::shared_ptr<Type>& item)
	{
		Write(_tables->Of<Type>().Add(item));
	}

private:
	template <typename Map, typename Key>
	[[nodiscard]] static uint32_t NumberOf(const Map& numbers, const Key* key)
	{
		const auto found = key != nullptr ? numbers.find(key) : numbers.end();
		return found != numbers.end() ? found->second : 0;
	}

	ecs::reflection::BinaryWriter* _out;
	SharedTables* _tables;
	const Effect* _effect {nullptr};
	std::unordered_map<const Atom*, uint32_t> _atoms;
	std::unordered_map<const Collection*, uint32_t> _collections;
};

/// Reads effects from a save
class EffectReader
{
public:
	EffectReader(ecs::reflection::BinaryReader& in, SharedTables& tables)
	    : _in(&in)
	    , _tables(&tables)
	{
	}

	[[nodiscard]] ecs::reflection::BinaryReader& In() { return *_in; }
	[[nodiscard]] SharedTables& Tables() { return *_tables; }
	[[nodiscard]] bool Failed() const;

	// The values that the rules keep
	void Read(bool& value);
	void Read(int32_t& value);
	void Read(uint32_t& value);
	void Read(uint64_t& value);
	void Read(float& value);
	void Read(glm::vec3& value);
	void Read(entt::entity& value);
	/// A count of 32 bits, none when the reader fails
	[[nodiscard]] std::optional<size_t> ReadSize();
	bool ReadBytes(void* bytes, size_t size);
	/// Any value that the archive can keep, in the files that include ParticleArchiveValues.h
	template <typename Type>
	void ReadValue(Type& value);
	template <typename Type>
	    requires std::is_trivially_copyable_v<Type>
	void ReadPlain(Type& value)
	{
		ReadBytes(&value, sizeof(Type));
	}
	template <typename Type>
	    requires std::is_trivially_copyable_v<Type>
	void ReadPlainList(std::vector<Type>& values)
	{
		const auto size = ReadSize();
		if (!size.has_value())
		{
			return;
		}
		values.resize(*size);
		if (!ReadBytes(values.data(), values.size() * sizeof(Type)))
		{
			values.clear();
		}
	}

	void StartEffect(Effect& effect)
	{
		_effect = &effect;
		_atoms.clear();
		_collections.clear();
	}
	[[nodiscard]] Effect& CurrentEffect() { return *_effect; }
	void NumberAtom(Atom* atom) { _atoms.push_back(atom); }
	void NumberCollection(Collection* collection) { _collections.push_back(collection); }
	[[nodiscard]] Atom* ReadAtom() { return Take(_atoms); }
	[[nodiscard]] Collection* ReadCollection() { return Take(_collections); }
	template <typename Type>
	[[nodiscard]] std::shared_ptr<Type> ReadShared()
	{
		uint32_t number = 0;
		Read(number);
		return _tables->Of<Type>().At(number);
	}

private:
	template <typename Pointer>
	[[nodiscard]] Pointer* Take(const std::vector<Pointer*>& numbered)
	{
		uint32_t number = 0;
		Read(number);
		return number > 0 && number <= numbered.size() ? numbered[number - 1] : nullptr;
	}

	ecs::reflection::BinaryReader* _in;
	SharedTables* _tables;
	Effect* _effect {nullptr};
	std::vector<Atom*> _atoms;
	std::vector<Collection*> _collections;
};

/// A map of what a rule keeps for each of its atoms, in the order of the atoms
template <typename Value>
void WriteAtomMap(EffectWriter& out, const std::unordered_map<const Atom*, Value>& map, auto&& write)
{
	std::vector<std::pair<uint32_t, const Value*>> entries;
	for (const auto& [atom, value] : map)
	{
		if (const auto number = out.AtomNumber(atom); number != 0)
		{
			entries.emplace_back(number, &value);
		}
	}
	std::ranges::sort(entries, {}, &std::pair<uint32_t, const Value*>::first);
	out.WriteSize(entries.size());
	for (const auto& [number, value] : entries)
	{
		out.Write(number);
		write(*value);
	}
}

template <typename Value>
void ReadAtomMap(EffectReader& in, std::unordered_map<const Atom*, Value>& map, auto&& read)
{
	map.clear();
	const auto size = in.ReadSize();
	for (size_t i = 0; size.has_value() && i < *size && !in.Failed(); ++i)
	{
		const auto* atom = in.ReadAtom();
		Value value {};
		read(value);
		if (atom != nullptr)
		{
			map.insert_or_assign(atom, std::move(value));
		}
	}
}

/// The parts of the shared things that the tables keep. The holders put back the links to their effects and atoms.
void SaveSharedTables(EffectWriter& out);
void LoadSharedTables(EffectReader& in);

} // namespace openblack::particles
