/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MobileObjectRecord.h"

#include "InfoConstants.h"

using namespace openblack;
using namespace openblack::ecs;

std::optional<components::MobileObject> mobile_object_record::FromRow(const InfoConstants& info, int32_t row)
{
	if (row == k_FootballRow)
	{
		return components::MobileObject {.type = info.ball.mobileType, .footballRecord = true};
	}
	if (row < 0 || static_cast<size_t>(row) >= info.mobileObject.size())
	{
		return std::nullopt;
	}
	return components::MobileObject {.type = static_cast<MobileObjectInfo>(row)};
}

int32_t mobile_object_record::Row(const components::MobileObject& mobile)
{
	return mobile.footballRecord ? k_FootballRow : static_cast<int32_t>(mobile.type);
}

const GMobileObjectInfo* mobile_object_record::Of(const InfoConstants& info, const components::MobileObject& mobile)
{
	if (mobile.footballRecord)
	{
		return &info.ball;
	}
	const auto row = static_cast<size_t>(mobile.type);
	return row < info.mobileObject.size() ? &info.mobileObject.at(row) : nullptr;
}
