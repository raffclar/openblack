/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <glm/mat4x4.hpp>

#include "Temple/TemplePeopleRules.h"

namespace openblack
{

/// One of the player's worshippers drawn walking about the temple this frame: its model, where and how it is placed in
/// the temple, and its bones as its clip poses it (empty to draw it as its model rests)
struct TemplePerson
{
	entt::id_type mesh;
	glm::mat4 model;
	std::vector<glm::mat4> bones;
};

/// The player's worshippers walking about the temple's floors while the player is inside it (see temple_people)
class TemplePeople
{
public:
	/// The worshippers of the player's worship sites as the player comes in, on the temple's walk mesh. Nobody comes in
	/// without the walk mesh, or without a temple of the player's.
	[[nodiscard]] static std::unique_ptr<TemplePeople> Gather();

	/// Walks them on, then for those the camera sees plays their clips on and poses them. The temple is placed by its
	/// matrix, and the camera sees it by its view and projection.
	void Update(float milliseconds, const glm::mat4& temple, const glm::mat4& viewProjection, float nearClip);

	/// Those drawn this frame
	[[nodiscard]] const std::vector<TemplePerson>& GetDrawn() const { return _drawn; }
	[[nodiscard]] size_t GetCount() const { return _visitors.size(); }

private:
	struct Visitor
	{
		entt::id_type mesh;
		float scale;
		bool man;
		temple_people::Walker walker;
	};

	temple_people::WalkMesh _mesh;
	std::vector<Visitor> _visitors;
	std::vector<TemplePerson> _drawn;
};

} // namespace openblack
