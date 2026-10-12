/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ChallengeRecords.h"

#include <algorithm>

#include "ECS/Components/ChallengeRecord.h"
#include "ECS/DiscipleRules.h"
#include "ECS/Registry.h"

using namespace openblack::ecs;
using openblack::ecs::components::ChallengeRecord;
namespace snapshots = openblack::script::challenge_snapshots;

namespace
{
/// Challenges are told apart by the low 16 bits of their number
bool SameChallenge(uint32_t a, uint32_t b)
{
	return static_cast<uint16_t>(a) == static_cast<uint16_t>(b);
}

void ForEachThing(const std::vector<snapshots::Value>& arguments, const std::function<void(entt::entity)>& act)
{
	if (!act)
	{
		return;
	}
	for (const auto& argument : arguments)
	{
		if (argument.type == openblack::lhvm::DataType::Object)
		{
			act(static_cast<entt::entity>(argument.value.uintVal));
		}
	}
}

/// What every snapshot writes into the record
void Write(ChallengeRecord& record, const snapshots::Snapshot& snapshot, bool takesCamera, uint32_t turn,
           const challenge_records::References& references)
{
	record.reminderScript = snapshot.reminderScript;
	record.title = snapshot.title;
	record.success = snapshot.success;
	record.alignment = snapshot.alignment;
	if (takesCamera)
	{
		record.position = snapshot.position;
		record.focus = snapshot.focus;
	}
	record.arguments = snapshot.arguments;
	ForEachThing(record.arguments, references.add);
	if (record.success == 1.0f)
	{
		record.doneTurn = turn;
	}
}
} // namespace

std::optional<entt::entity> challenge_records::Find(const Registry& registry, uint32_t challenge)
{
	std::optional<entt::entity> found;
	registry.Each<const ChallengeRecord>([&found, challenge](entt::entity entity, const ChallengeRecord& record) {
		if (!found.has_value() && SameChallenge(record.challenge, challenge))
		{
			found = entity;
		}
	});
	return found;
}

challenge_records::Outcome challenge_records::Record(Registry& registry, const snapshots::Snapshot& snapshot,
                                                     snapshots::Call call, uint32_t turn, const References& references)
{
	const bool takesCamera = call == snapshots::Call::Start || (call == snapshots::Call::Picture && snapshot.takingPicture);
	if (const auto existing = Find(registry, snapshot.challenge); existing.has_value())
	{
		auto& record = registry.Get<ChallengeRecord>(*existing);
		ForEachThing(record.arguments, references.remove);
		auto update = snapshot;
		if (record.success >= 1.0f)
		{
			update.success = 1.0f;
		}
		Write(record, update, takesCamera, turn, references);
		return {.record = *existing, .alignment = snapshot.alignment, .takesPicture = takesCamera};
	}

	uint32_t order = 0;
	registry.Each<const ChallengeRecord>(
	    [&order](entt::entity, const ChallengeRecord& record) { order = std::max(order, record.order + 1); });
	const auto entity = registry.Create();
	auto& record = registry.Assign<ChallengeRecord>(entity);
	record.challenge = snapshot.challenge;
	record.order = order;
	record.startedTurn = turn;
	record.quest = call == snapshots::Call::Start && snapshot.quest;
	Write(record, snapshot, call != snapshots::Call::Update, turn, references);
	return {.record = entity, .alignment = std::nullopt, .takesPicture = true};
}

std::vector<snapshots::Value> challenge_records::ReminderArguments(const ChallengeRecord& record)
{
	return {record.arguments.rbegin(), record.arguments.rend()};
}

float challenge_records::AlignmentChange(float playerAlignment, float snapshotAlignment, float scriptScale)
{
	return disciple::DampedAlignment(playerAlignment, snapshotAlignment * scriptScale);
}
