/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <memory>

#include "ECS/Systems/RenderingSystemInterface.h"

/// (openblack engine) OPENBLACK_INSTANCE_VERIFY=1: the proof of the incremental PrepareDraw. After a PrepareDraw
/// build the instances it left in the
/// RenderContext are kept, the full build runs again into the same context, and the two are compared byte by byte:
/// instanceUniforms / instanceColours row by row (every row of the lists, the unused ones too), every draw range map,
/// the instance sets, the PSys atoms and the entity -> instance map. The first mismatch of a check is logged ("game",
/// warn; the first 20 checks with one), and every 300 checks the count. Then the first build is put back, so what is
/// drawn is the first build's. While only the full build exists (GPU 1 step 1) both are the full one: a self-test of
/// its determinism, zero mismatches expected; from step 2 the first one is the fast path.
namespace openblack::ecs::systems::instance_verify
{
/// OPENBLACK_INSTANCE_VERIFY=1 (read once)
[[nodiscard]] bool Enabled();

/// Keeps a copy of what the build left in `context`, runs `build` (which fills `context` again the full way), compares
/// the two and logs the first mismatch, then puts the copy back into `context` (its CPU lists and maps only: the caller
/// uploads the instances again, `build` may have uploaded its own)
void Check(RenderContext& context, const std::function<void()>& build);
} // namespace openblack::ecs::systems::instance_verify
