/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "RenderingSystemVerify.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <array>
#include <bit>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <fmt/format.h>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>

#include "ECS/Systems/DebugHooksInterface.h"
#include "Locator.h"

namespace openblack::ecs::systems::instance_verify
{
namespace
{
/// The checks with a mismatch that are logged (then only counted), and how often the count is logged
constexpr uint64_t k_LoggedMismatches = 20;
constexpr uint64_t k_SummaryEvery = 300;

/// What a build leaves in a RenderContext for the draw (RenderingSystemInterface.h), copied
struct Snapshot
{
	decltype(RenderContext::instanceUniforms) instanceUniforms;
	decltype(RenderContext::instanceColours) instanceColours;
	decltype(RenderContext::instancedDrawDescs) instancedDrawDescs;
	decltype(RenderContext::footprintOnlyDrawDescs) footprintOnlyDrawDescs;
	decltype(RenderContext::translucentDrawDescs) translucentDrawDescs;
	decltype(RenderContext::additiveInstances) additiveInstances;
	decltype(RenderContext::cutAtomDrawDescs) cutAtomDrawDescs;
	decltype(RenderContext::cutAtomInstances) cutAtomInstances;
	decltype(RenderContext::psysAtoms) psysAtoms;
	decltype(RenderContext::psysAtomIndex) psysAtomIndex;
	decltype(RenderContext::psysAtomDrawDescs) psysAtomDrawDescs;
	decltype(RenderContext::sortPoints) sortPoints;
	decltype(RenderContext::meshLandLight) meshLandLight;
	decltype(RenderContext::entityInstances) entityInstances;
	decltype(RenderContext::instancePoses) instancePoses;
	decltype(RenderContext::shadowCasterDrawDescs) shadowCasterDrawDescs;
	decltype(RenderContext::sortedOpaqueDrawDescs) sortedOpaqueDrawDescs;
	decltype(RenderContext::sortedInstances) sortedInstances;
	decltype(RenderContext::entityRows) entityRows;
};

Snapshot Take(const RenderContext& context)
{
	return {
	    context.instanceUniforms,
	    context.instanceColours,
	    context.instancedDrawDescs,
	    context.footprintOnlyDrawDescs,
	    context.translucentDrawDescs,
	    context.additiveInstances,
	    context.cutAtomDrawDescs,
	    context.cutAtomInstances,
	    context.psysAtoms,
	    context.psysAtomIndex,
	    context.psysAtomDrawDescs,
	    context.sortPoints,
	    context.meshLandLight,
	    context.entityInstances,
	    context.instancePoses,
	    context.shadowCasterDrawDescs,
	    context.sortedOpaqueDrawDescs,
	    context.sortedInstances,
	    context.entityRows,
	};
}

/// Swapped, not copied: the snapshot is dropped afterwards
void Restore(Snapshot& snapshot, RenderContext& context)
{
	context.instanceUniforms.swap(snapshot.instanceUniforms);
	context.instanceColours.swap(snapshot.instanceColours);
	context.instancedDrawDescs.swap(snapshot.instancedDrawDescs);
	context.footprintOnlyDrawDescs.swap(snapshot.footprintOnlyDrawDescs);
	context.translucentDrawDescs.swap(snapshot.translucentDrawDescs);
	context.additiveInstances.swap(snapshot.additiveInstances);
	context.cutAtomDrawDescs.swap(snapshot.cutAtomDrawDescs);
	context.cutAtomInstances.swap(snapshot.cutAtomInstances);
	context.psysAtoms.swap(snapshot.psysAtoms);
	context.psysAtomIndex.swap(snapshot.psysAtomIndex);
	context.psysAtomDrawDescs.swap(snapshot.psysAtomDrawDescs);
	context.sortPoints.swap(snapshot.sortPoints);
	context.meshLandLight.swap(snapshot.meshLandLight);
	context.entityInstances.swap(snapshot.entityInstances);
	context.instancePoses.swap(snapshot.instancePoses);
	context.shadowCasterDrawDescs.swap(snapshot.shadowCasterDrawDescs);
	context.sortedOpaqueDrawDescs.swap(snapshot.sortedOpaqueDrawDescs);
	context.sortedInstances.swap(snapshot.sortedInstances);
	context.entityRows.swap(snapshot.entityRows);
}

/// Who writes an instance row, for the log: an entity's drawn row, a PSys atom's, or another (its static shadow, its
/// footprint, a bounding box)
std::string OwnerOf(const RenderContext& context, size_t row)
{
	for (const auto& [entity, instance] : context.entityInstances)
	{
		if (instance.index == row)
		{
			return fmt::format("entity {} mesh {}", entt::to_integral(entity), instance.meshId);
		}
	}
	for (size_t i = 0; i < context.psysAtoms.size(); ++i)
	{
		if (context.psysAtoms[i].index == row)
		{
			return fmt::format("PSys atom {} mesh {}", i, context.psysAtoms[i].meshId);
		}
	}
	return "no drawn entity: a shadow, footprint or box row, or unused";
}

/// The first float that differs, bit for bit, in two lists of rows of floats (glm::mat4 / glm::vec4)
template <typename Row>
std::string RowsMismatch(std::string_view name, const std::vector<Row>& expected, const std::vector<Row>& actual,
                         const RenderContext& context)
{
	if (expected.size() != actual.size())
	{
		return fmt::format("{}: {} rows, then {}", name, expected.size(), actual.size());
	}
	constexpr size_t k_Floats = sizeof(Row) / sizeof(float);
	for (size_t row = 0; row < expected.size(); ++row)
	{
		if (std::memcmp(&expected[row], &actual[row], sizeof(Row)) == 0)
		{
			continue;
		}
		const float* before = glm::value_ptr(expected[row]);
		const float* after = glm::value_ptr(actual[row]);
		for (size_t i = 0; i < k_Floats; ++i)
		{
			const auto bitsBefore = std::bit_cast<uint32_t>(before[i]);
			const auto bitsAfter = std::bit_cast<uint32_t>(after[i]);
			if (bitsBefore != bitsAfter)
			{
				return fmt::format("{} row {} ({}) float {}: {:#010x} ({}), then {:#010x} ({})", name, row,
				                   OwnerOf(context, row), i, bitsBefore, before[i], bitsAfter, after[i]);
			}
		}
	}
	return {};
}

using Descs = std::map<entt::id_type, const RenderContext::InstancedDrawDesc>;

/// The first range that differs (std::map: both in the same order)
std::string DescsMismatch(std::string_view name, const Descs& expected, const Descs& actual)
{
	if (expected.size() != actual.size())
	{
		return fmt::format("{}: {} meshes, then {}", name, expected.size(), actual.size());
	}
	for (auto before = expected.begin(), after = actual.begin(); before != expected.end(); ++before, ++after)
	{
		const auto& b = before->second;
		const auto& a = after->second;
		if (before->first != after->first || b.offset != a.offset || b.count != a.count ||
		    b.morphWithTerrain != a.morphWithTerrain)
		{
			return fmt::format("{}: mesh {} offset {} count {} morph {}, then mesh {} offset {} count {} morph {}", name,
			                   before->first, b.offset, b.count, b.morphWithTerrain, after->first, a.offset, a.count,
			                   a.morphWithTerrain);
		}
	}
	return {};
}

/// The first key missing or with another value in an unordered map (its order is not compared: the copy may have
/// other buckets; the ranges and rows above carry the order the build took from it)
template <typename Map, typename Equal, typename KeyText>
std::string MapMismatch(std::string_view name, const Map& expected, const Map& actual, Equal equal, KeyText keyText)
{
	if (expected.size() != actual.size())
	{
		return fmt::format("{}: {} entries, then {}", name, expected.size(), actual.size());
	}
	for (const auto& [key, value] : expected)
	{
		const auto found = actual.find(key);
		if (found == actual.end())
		{
			return fmt::format("{}: {} only in the first build", name, keyText(key));
		}
		if (!equal(value, found->second))
		{
			return fmt::format("{}: {} differs", name, keyText(key));
		}
	}
	return {};
}

std::string SetMismatch(std::string_view name, const std::unordered_set<uint32_t>& expected,
                        const std::unordered_set<uint32_t>& actual)
{
	if (expected.size() != actual.size())
	{
		return fmt::format("{}: {} instances, then {}", name, expected.size(), actual.size());
	}
	for (const auto index : expected)
	{
		if (!actual.contains(index))
		{
			return fmt::format("{}: instance {} only in the first build", name, index);
		}
	}
	return {};
}

using Atoms = decltype(RenderContext::psysAtoms);

std::string AtomsMismatch(const Atoms& expected, const Atoms& actual)
{
	if (expected.size() != actual.size())
	{
		return fmt::format("psysAtoms: {} atoms, then {}", expected.size(), actual.size());
	}
	for (size_t i = 0; i < expected.size(); ++i)
	{
		const auto& b = expected[i];
		const auto& a = actual[i];
		if (b.index != a.index || b.meshId != a.meshId || b.path != a.path || b.effect != a.effect || b.atom != a.atom ||
		    std::memcmp(&b.key, &a.key, sizeof(b.key)) != 0 || b.translucent != a.translucent || b.additive != a.additive ||
		    b.cut != a.cut)
		{
			return fmt::format("psysAtoms: atom {} (instance {} mesh {}, then instance {} mesh {})", i, b.index, b.meshId,
			                   a.index, a.meshId);
		}
	}
	return {};
}

/// The first mismatch between the kept build and the one in `context`, empty when they are the same
std::string FirstMismatch(const Snapshot& expected, const RenderContext& context)
{
	const auto instance = [](uint32_t index) { return fmt::format("instance {}", index); };
	const auto mesh = [](entt::id_type id) { return fmt::format("mesh {}", id); };
	const auto bitwise = [](const auto& b, const auto& a) { return std::memcmp(&b, &a, sizeof(b)) == 0; };
	std::string mismatch = RowsMismatch("instanceUniforms", expected.instanceUniforms, context.instanceUniforms, context);
	if (mismatch.empty())
	{
		mismatch = RowsMismatch("instanceColours", expected.instanceColours, context.instanceColours, context);
	}
	using Pair = std::pair<std::string_view, std::pair<const Descs*, const Descs*>>;
	const std::array<Pair, 6> descs = {{
	    {"instancedDrawDescs", {&expected.instancedDrawDescs, &context.instancedDrawDescs}},
	    {"footprintOnlyDrawDescs", {&expected.footprintOnlyDrawDescs, &context.footprintOnlyDrawDescs}},
	    {"translucentDrawDescs", {&expected.translucentDrawDescs, &context.translucentDrawDescs}},
	    {"cutAtomDrawDescs", {&expected.cutAtomDrawDescs, &context.cutAtomDrawDescs}},
	    {"psysAtomDrawDescs", {&expected.psysAtomDrawDescs, &context.psysAtomDrawDescs}},
	    {"shadowCasterDrawDescs", {&expected.shadowCasterDrawDescs, &context.shadowCasterDrawDescs}},
	}};
	for (const auto& [name, maps] : descs)
	{
		if (mismatch.empty())
		{
			mismatch = DescsMismatch(name, *maps.first, *maps.second);
		}
	}
	if (mismatch.empty())
	{
		mismatch = SetMismatch("additiveInstances", expected.additiveInstances, context.additiveInstances);
	}
	if (mismatch.empty())
	{
		mismatch = SetMismatch("cutAtomInstances", expected.cutAtomInstances, context.cutAtomInstances);
	}
	if (mismatch.empty())
	{
		mismatch = AtomsMismatch(expected.psysAtoms, context.psysAtoms);
	}
	if (mismatch.empty())
	{
		mismatch = MapMismatch(
		    "psysAtomIndex", expected.psysAtomIndex, context.psysAtomIndex, [](uint32_t b, uint32_t a) { return b == a; },
		    [](const openblack::psys::Atom* atom) { return fmt::format("atom {}", static_cast<const void*>(atom)); });
	}
	if (mismatch.empty())
	{
		mismatch = MapMismatch("sortPoints", expected.sortPoints, context.sortPoints, bitwise, instance);
	}
	if (mismatch.empty())
	{
		mismatch = MapMismatch(
		    "meshLandLight", expected.meshLandLight, context.meshLandLight,
		    [](const openblack::land_light::ObjectLight& b, const openblack::land_light::ObjectLight& a) {
			    return b.mode == a.mode && b.haze == a.haze;
		    },
		    mesh);
	}
	if (mismatch.empty())
	{
		mismatch = MapMismatch(
		    "entityInstances", expected.entityInstances, context.entityInstances,
		    [](const RenderContext::EntityInstance& b, const RenderContext::EntityInstance& a) {
			    return b.meshId == a.meshId && b.index == a.index && b.morphWithTerrain == a.morphWithTerrain &&
			           b.receivesDynamicShadow == a.receivesDynamicShadow;
		    },
		    [](entt::entity entity) { return fmt::format("entity {}", entt::to_integral(entity)); });
	}
	if (mismatch.empty())
	{
		mismatch = MapMismatch(
		    "instancePoses", expected.instancePoses, context.instancePoses,
		    [](const std::vector<glm::mat4>& b, const std::vector<glm::mat4>& a) {
			    return b.size() == a.size() &&
			           (b.empty() || std::memcmp(b.data(), a.data(), b.size() * sizeof(glm::mat4)) == 0);
		    },
		    instance);
	}
	return mismatch;
}

struct Totals
{
	uint64_t checked = 0;
	uint64_t mismatched = 0;
};

/// OPENBLACK_INSTANCE_VERIFY's totals, in the debug hooks' store (Locator::debugHooks)
struct InstanceVerifyDebugHooksState
{
	Totals totals;
};

InstanceVerifyDebugHooksState& InstanceVerifyDebugHooksData()
{
	if (!Locator::debugHooks::has_value())
	{
		std::fputs("instance_verify: no debug hooks in the locator (Locator::debugHooks)\n", stderr);
		std::abort();
	}
	return Locator::debugHooks::value().Get<InstanceVerifyDebugHooksState>();
}
} // namespace

bool Enabled()
{
	static const bool enabled = []() {
		const char* value = std::getenv("OPENBLACK_INSTANCE_VERIFY");
		return value != nullptr && std::string_view(value) == "1";
	}();
	return enabled;
}

void Check(RenderContext& context, const std::function<void()>& build)
{
	auto& totals = InstanceVerifyDebugHooksData().totals;
	auto logger = spdlog::get("game");
	if (totals.checked == 0 && logger)
	{
		SPDLOG_LOGGER_INFO(logger, "Instance verify (OPENBLACK_INSTANCE_VERIFY=1): every PrepareDraw build is built "
		                           "again and compared byte by byte");
	}
	auto kept = Take(context);
	build();
	const std::string mismatch = FirstMismatch(kept, context);
	++totals.checked;
	if (!mismatch.empty())
	{
		++totals.mismatched;
		if (totals.mismatched <= k_LoggedMismatches && logger)
		{
			SPDLOG_LOGGER_WARN(logger, "Instance verify: check {}: {}", totals.checked, mismatch);
		}
	}
	if (totals.checked % k_SummaryEvery == 0 && logger)
	{
		SPDLOG_LOGGER_INFO(logger, "Instance verify: {} checks, {} with a mismatch", totals.checked, totals.mismatched);
	}
	Restore(kept, context);
}
} // namespace openblack::ecs::systems::instance_verify
