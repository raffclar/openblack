/* openblack - A reimplementation of Lionhead's Black & White.
 *
 * openblack is the legal property of its developers, whose names
 * can be found in the AUTHORS.md file distributed with this source
 * distribution.
 *
 * openblack is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * openblack is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with openblack. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <cassert>

#include <vector>

#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>

namespace openblack::dynamics
{

class LandBlockBulletMeshInterface: public btStridingMeshInterface
{
	std::vector<float[3]> _vertices;
	std::vector<uint16_t> _indices;

public:
	explicit LandBlockBulletMeshInterface(const uint8_t* vertex_data, uint32_t vertex_count, size_t stride)
	    : _vertices(vertex_count / stride)
	    , _indices(vertex_count / stride)
	{
		for (uint16_t i = 0; i < _vertices.size(); ++i)
		{
			auto vertex_base = reinterpret_cast<const float*>(&vertex_data[i * stride]);
			_vertices[i][0] = vertex_base[0];
			_vertices[i][1] = vertex_base[1];
			_vertices[i][2] = vertex_base[2];
			_indices[i] = i;
		}
	}

	/// get read and write access to a subpart of a triangle mesh
	/// this subpart has a continuous array of vertices and indices
	/// in this way the mesh can be handled as chunks of memory with striding
	/// very similar to OpenGL vertexarray support
	/// make a call to unLockVertexBase when the read and write access is finished
	void getLockedVertexIndexBase(unsigned char** vertexbase, int& numverts, PHY_ScalarType& type, int& stride,
	                              unsigned char** indexbase, int& indexstride, int& numfaces, PHY_ScalarType& indicestype,
	                              int subpart = 0) override
	{
		return;
	}

	void getLockedReadOnlyVertexIndexBase(const unsigned char** vertexbase, int& numverts, PHY_ScalarType& type, int& stride,
	                                      const unsigned char** indexbase, int& indexstride, int& numfaces,
	                                      PHY_ScalarType& indicestype, int subpart = 0) const override
	{
		assert(subpart == 0);
		*vertexbase = reinterpret_cast<const unsigned char*>(_vertices.data());
		numverts = static_cast<int>(_vertices.size());
		type = PHY_ScalarType::PHY_FLOAT;
		stride = sizeof(_vertices[0]);
		*indexbase = reinterpret_cast<const unsigned char*>(_indices.data());
		indexstride = 3 * sizeof(_indices[0]);
		numfaces = static_cast<int>(_indices.size() / 3);
		indicestype = PHY_ScalarType::PHY_SHORT;
	}

	/// unLockVertexBase finishes the access to a subpart of the triangle mesh
	/// make a call to unLockVertexBase when the read and write access (using getLockedVertexIndexBase) is finished
	void unLockVertexBase(int subpart) override { return; }

	void unLockReadOnlyVertexBase(int subpart) const override { return; }

	/// getNumSubParts returns the number of separate subparts
	/// each subpart has a continuous array of vertices and indices
	int getNumSubParts() const override { return 1; }

	void preallocateVertices(int numverts) override { return; }

	void preallocateIndices(int numindices) override { return; }
};
} // namespace openblack::dynamics
