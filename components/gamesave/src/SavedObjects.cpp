/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SavedObjects.h"

using namespace openblack::gamesave;

namespace
{
/// The most reactions that a read accepts, so that bad bytes cannot ask for a huge list
constexpr uint32_t k_MostReactions = 0x10000;

void Put(StreamWriter& writer, uint32_t type, const ThingPart& thing)
{
	writer.Put(type);
	writer.Put(thing.extra);
	writer.PutChecksum();
	writer.Put(thing.destroyed);
	writer.Put(thing.flags);
}

/// Reads the part after the type and the extra word, which the reader of the record reads first
void Get(StreamReader& reader, ThingPart& thing)
{
	reader.Get(thing.checksum);
	reader.Get(thing.destroyed);
	reader.Get(thing.flags);
}

bool HasPhysics(const ThingPart& thing, const PositionPart& position)
{
	return (position.flags & k_PositionFlagInPhysics) != 0 && (thing.flags & k_ThingFlagCarriedByParticles) == 0;
}

void Put(StreamWriter& writer, const ThingPart& thing, const PositionPart& position, const ObjectPart& object)
{
	writer.Put(position.position);
	writer.Put(position.flags);
	writer.Put(object.info);
	writer.Put(object.coords);
	writer.Put(object.creationIndex);
	writer.Put(object.life);
	writer.Put(object.scale);
	writer.Put(object.heading);
	Write(writer, object.fire);
	if (HasPhysics(thing, position))
	{
		// An object with no body in the physics writes a body that stands still
		const auto physics = object.physics.value_or(PhysicsPart {});
		writer.Put(physics.matrix);
		writer.Put(physics.velocity);
		writer.Put(physics.spin);
	}
}

void Get(StreamReader& reader, const ThingPart& thing, PositionPart& position, ObjectPart& object)
{
	reader.Get(position.position);
	reader.Get(position.flags);
	reader.Get(object.info);
	reader.Get(object.coords);
	reader.Get(object.creationIndex);
	reader.Get(object.life);
	reader.Get(object.scale);
	reader.Get(object.heading);
	Read(reader, object.fire);
	if (HasPhysics(thing, position))
	{
		PhysicsPart physics;
		reader.Get(physics.matrix);
		reader.Get(physics.velocity);
		reader.Get(physics.spin);
		object.physics = physics;
	}
}

bool IsCircleState(uint8_t state)
{
	return state >= k_FirstCircleState && state <= k_LastCircleState;
}

void Put(StreamWriter& writer, const MovePart& move)
{
	writer.Put(move.mobileWord);
	writer.Put(move.turnsToStateChange);
	writer.Put(move.speed);
	writer.Put(move.angle);
	writer.Put(move.moveState);
	if (IsCircleState(move.moveState))
	{
		const auto circle = move.circle.value_or(CircleHug {});
		writer.Put(circle.turnsToObject);
		writer.Put(circle.entryDistance);
		Write(writer, circle.object);
		writer.Put(circle.index);
		writer.Put(circle.coords);
	}
	else if (move.moveState == k_StepThroughState)
	{
		writer.Put(move.turnsToStepRebuild.value_or(int8_t {0}));
	}
	Write(writer, move.footpath);
	writer.Put(move.goal);
	Write(writer, move.target);
	writer.Put(move.step);
}

void Get(StreamReader& reader, MovePart& move)
{
	reader.Get(move.mobileWord);
	reader.Get(move.turnsToStateChange);
	reader.Get(move.speed);
	reader.Get(move.angle);
	reader.Get(move.moveState);
	if (IsCircleState(move.moveState))
	{
		CircleHug circle;
		reader.Get(circle.turnsToObject);
		reader.Get(circle.entryDistance);
		Read(reader, circle.object);
		reader.Get(circle.index);
		reader.Get(circle.coords);
		move.circle = std::move(circle);
	}
	else if (move.moveState == k_StepThroughState)
	{
		int8_t turns = 0;
		reader.Get(turns);
		move.turnsToStepRebuild = turns;
	}
	Read(reader, move.footpath);
	reader.Get(move.goal);
	Read(reader, move.target);
	reader.Get(move.step);
}

void Put(StreamWriter& writer, const LivingPart& living)
{
	Write(writer, living.footpath);
	Write(writer, living.footpathNode);
	writer.Put(living.action);
	Write(writer, living.reaction);
	writer.Put(living.word9C);
	writer.Put(living.birthTurn);
	writer.Put(living.byteA8);
	Write(writer, living.dataPath);
	Write(writer, living.scriptReminder);
	writer.Put(living.status);
	Write(writer, living.flock);
	Write(writer, living.objectBC);
	Write(writer, living.footpathAgain);
	writer.Put(living.wordC4);
	writer.Put(living.specular);
	writer.Put(living.byteD4);
	Write(writer, living.danceGroup);
	writer.Put(living.byteDC);
	writer.Put(static_cast<uint32_t>(living.reactionsDone.size()));
	for (const auto& done : living.reactionsDone)
	{
		writer.Put(done.reaction);
		writer.Put(done.turn);
	}
}

bool Get(StreamReader& reader, LivingPart& living)
{
	Read(reader, living.footpath);
	Read(reader, living.footpathNode);
	reader.Get(living.action);
	Read(reader, living.reaction);
	reader.Get(living.word9C);
	reader.Get(living.birthTurn);
	reader.Get(living.byteA8);
	Read(reader, living.dataPath);
	Read(reader, living.scriptReminder);
	reader.Get(living.status);
	Read(reader, living.flock);
	Read(reader, living.objectBC);
	Read(reader, living.footpathAgain);
	reader.Get(living.wordC4);
	reader.Get(living.specular);
	reader.Get(living.byteD4);
	Read(reader, living.danceGroup);
	reader.Get(living.byteDC);
	uint32_t count = 0;
	if (!reader.Get(count) || count > k_MostReactions)
	{
		reader.Fail();
		return false;
	}
	living.reactionsDone.resize(count);
	for (auto& done : living.reactionsDone)
	{
		reader.Get(done.reaction);
		reader.Get(done.turn);
	}
	return !reader.Failed();
}

std::shared_ptr<SavedObject> ReadCreature(StreamReader& reader, uint32_t extra)
{
	CreatureObject creature;
	creature.thing.extra = extra;
	Get(reader, creature.thing);
	Get(reader, creature.thing, creature.position, creature.object);
	Get(reader, creature.move);
	if (!Get(reader, creature.living))
	{
		return nullptr;
	}
	auto record = ReadCreatureRecord(reader);
	if (!record.has_value())
	{
		reader.Fail();
		return nullptr;
	}
	creature.creature = std::move(*record);
	return std::make_shared<SavedObject>(SavedObject {.value = std::move(creature)});
}

std::shared_ptr<SavedObject> ReadLeashStatus(StreamReader& reader, uint32_t extra)
{
	LeashStatusObject leash;
	leash.thing.extra = extra;
	Get(reader, leash.thing);
	auto record = ReadLeashRecord(reader);
	if (!record.has_value())
	{
		reader.Fail();
		return nullptr;
	}
	leash.leash = std::move(*record);
	return std::make_shared<SavedObject>(SavedObject {.value = std::move(leash)});
}
} // namespace

bool ObjectRef::operator==(const ObjectRef& other) const
{
	if (key != other.key || (record == nullptr) != (other.record == nullptr))
	{
		return false;
	}
	return record == nullptr || *record == *other.record;
}

void openblack::gamesave::Write(StreamWriter& writer, const ObjectRef& ref)
{
	if (!writer.PutReference(ref.key))
	{
		return;
	}
	if (ref.record != nullptr)
	{
		Write(writer, *ref.record);
	}
	else
	{
		writer.MarkForeign(ref.key);
	}
}

bool openblack::gamesave::Read(StreamReader& reader, ObjectRef& ref)
{
	ref.record = nullptr;
	switch (reader.GetReference(ref.key))
	{
	case ReferenceKind::None:
	case ReferenceKind::Earlier:
	case ReferenceKind::Foreign:
		return true;
	case ReferenceKind::New:
		ref.record = ReadObject(reader);
		return ref.record != nullptr;
	case ReferenceKind::Bad:
		break;
	}
	return false;
}

void openblack::gamesave::Write(StreamWriter& writer, const CreatureObject& creature)
{
	Put(writer, k_CreatureType, creature.thing);
	Put(writer, creature.thing, creature.position, creature.object);
	Put(writer, creature.move);
	Put(writer, creature.living);
	Write(writer, creature.creature);
}

void openblack::gamesave::Write(StreamWriter& writer, const LeashStatusObject& leash)
{
	Put(writer, k_LeashStatusType, leash.thing);
	Write(writer, leash.leash);
}

void openblack::gamesave::Write(StreamWriter& writer, const SavedObject& object)
{
	std::visit([&writer](const auto& value) { Write(writer, value); }, object.value);
}

std::shared_ptr<SavedObject> openblack::gamesave::ReadObject(StreamReader& reader)
{
	uint32_t type = 0;
	uint32_t extra = 0;
	reader.Get(type);
	reader.Get(extra);
	if (reader.Failed())
	{
		return nullptr;
	}
	switch (type)
	{
	case k_CreatureType:
		return ReadCreature(reader, extra);
	case k_LeashStatusType:
		return ReadLeashStatus(reader, extra);
	default:
		reader.SetUnknownType(type);
		return nullptr;
	}
}

void openblack::gamesave::ForEachReference(CreatureObject& creature, const std::function<void(ObjectRef&)>& function)
{
	function(creature.object.fire);
	if (creature.move.circle.has_value())
	{
		function(creature.move.circle->object);
	}
	function(creature.move.footpath);
	function(creature.move.target);
	auto& living = creature.living;
	for (auto* ref : {&living.footpath, &living.footpathNode, &living.reaction, &living.dataPath, &living.scriptReminder,
	                  &living.flock, &living.objectBC, &living.footpathAgain, &living.danceGroup})
	{
		function(*ref);
	}
	auto& record = creature.creature;
	for (auto& slot : record.spells)
	{
		function(slot.miracle);
	}
	for (auto& queued : record.queued)
	{
		function(queued.miracle);
	}
	for (auto& visual : record.spellVisuals)
	{
		function(visual);
	}
	for (auto& particles : record.handParticles)
	{
		function(particles);
	}
	function(record.targetParticles);
	function(record.targetObject);
	function(record.arena);
	for (auto& partner : record.dancePartners)
	{
		function(partner);
	}
	function(record.dance);
	if (record.plan.has_value())
	{
		for (auto& object : record.plan->objects)
		{
			function(object);
		}
		for (auto& step : record.plan->agenda.steps)
		{
			function(step.object);
			function(step.other);
		}
		function(record.plan->agenda.creature);
		function(record.plan->agenda.other);
	}
}
