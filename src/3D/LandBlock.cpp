/* OpenBlack - A reimplementation of Lionhead's Black & White.
 *
 * OpenBlack is the legal property of its developers, whose names
 * can be found in the AUTHORS.md file distributed with this source
 * distribution.
 *
 * OpenBlack is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * OpenBlack is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenBlack. If not, see <http://www.gnu.org/licenses/>.
 */

#include "LandBlock.h"

#include <3D/LandIsland.h>
#include <glm/gtc/type_ptr.hpp>

using namespace OpenBlack;

struct LH3DLandCell // 8 bytes
{
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t l;

	uint8_t altitude;
	uint8_t savecolor;

	uint8_t flags1;
	uint8_t flags2;
};

struct LH3DLandBlock
{
	LH3DLandCell cells[289];   // 17*17
	uint32_t index;            // 2312
	float mapX;                // 2320
	float mapZ;                // 2316
	uint32_t blockX;           // 2328
	uint32_t blockZ;           // 2324
	uint32_t clipped;          // 2332 0
	uint32_t frameVisibility;  // 0
	uint32_t highestAltitude;  // 0
	uint32_t useSmallBump;     // 0
	uint32_t forceLowResTex;   // 0
	uint32_t meshLOD;          // 0
	uint32_t meshBlending;     // 0
	uint32_t textureBlend;     // 0
	uint32_t meshLODType;      // 0
	uint32_t fog;              // 0
	uint32_t texPointer;       // 0 if LH3DIsland::g_b_use_always_low_texture, else new Tex
	uint32_t matPointer;       // 0 if LH3DIsland::g_b_use_always_low_texture, else new Mat(Tex)
	uint32_t drawSomething;    // 0aaa
	uint32_t specMatBeforePtr; // 0
	uint32_t specMatAfterPtr;  // 0
	float transformUVBefore[3][4];
	float transformUVAfter[3][4];
	uint32_t nextSortingPtr;
	float valueSorting;
	float lowResTexture;
	float fu_lrs; // (iu_lrs / 256)
	float fv_lrs; // (iv_lrs / 256)
	float iu_lrs; // lowrestex x
	float iv_lrs; // lowrestex z
	uint32_t smallTextUpdated;
};

void LandBlock::Load(void* block, size_t block_size)
{
	if (block_size != sizeof(LH3DLandBlock))
	{
		throw new std::runtime_error("LandBlock size does not match struct size.");
	}

	auto lhBlock = static_cast<LH3DLandBlock*>(block);

	_index         = lhBlock->index;
	_blockPosition = glm::ivec2(lhBlock->blockX, lhBlock->blockZ);
	_mapPosition   = glm::vec2(lhBlock->mapX, lhBlock->mapZ);

	// this should just work, not graceful lol
	memcpy(&_cells, lhBlock->cells, 17 * 17 * sizeof(LH3DLandCell));
}

void LandBlock::BuildMesh(LandIsland& island)
{
	if (_mesh != nullptr)
		_mesh.reset();

	VertexDecl decl(7);
	decl[0] = VertexAttrib(0, 3, GL_FLOAT, false, false, sizeof(LandVertex), (GLvoid*)offsetof(LandVertex, position));
	decl[1] = VertexAttrib(1, 3, GL_FLOAT, false, false, sizeof(LandVertex), (GLvoid*)offsetof(LandVertex, weight));
	decl[2] = VertexAttrib(2, 3, GL_UNSIGNED_BYTE, true, false, sizeof(LandVertex), (GLvoid*)offsetof(LandVertex, firstMaterialID));
	decl[3] = VertexAttrib(3, 3, GL_UNSIGNED_BYTE, true, false, sizeof(LandVertex), (GLvoid*)offsetof(LandVertex, secondMaterialID));
	decl[4] = VertexAttrib(4, 3, GL_UNSIGNED_BYTE, false, true, sizeof(LandVertex), (GLvoid*)offsetof(LandVertex, materialBlendCoefficient));
	decl[5] = VertexAttrib(5, 1, GL_UNSIGNED_BYTE, false, true, sizeof(LandVertex), (GLvoid*)offsetof(LandVertex, lightLevel));
	decl[6] = VertexAttrib(6, 1, GL_FLOAT, false, true, sizeof(LandVertex), (GLvoid*)offsetof(LandVertex, waterAlpha));

	auto verts = buildVertexList(island);

	VertexBuffer* vertexBuffer = new VertexBuffer(verts.data(), verts.size(), sizeof(LandVertex));
	_mesh                      = std::make_unique<Mesh>(vertexBuffer, decl, GL_TRIANGLES);
}

std::vector<LandVertex> LandBlock::buildVertexList(LandIsland& island)
{
	// reserve 16*16 quads of 2 tris with 3 verts = 1536
	std::vector<LandVertex> verts;
	verts.reserve(1536);

	auto countries = island.GetCountries();

	auto neighbourBlockR  = island.GetBlock(_blockPosition.x + 1, _blockPosition.y);
	auto neighbourBlockUp = island.GetBlock(_blockPosition.x, _blockPosition.y + 1);

	// we'll loop through each cell, 16x16
	// (the array is 17x17 but the 17th block is questionable data)

	int bx = _blockPosition.x * 16;
	int bz = _blockPosition.y * 16;

	for (int x = 0; x < 16; x++)
	{
		for (int z = 0; z < 16; z++)
		{
			LandCell tl = island.GetCell(bx + x + 0, bz + z + 0);
			LandCell tr = island.GetCell(bx + x + 1, bz + z + 0);
			LandCell bl = island.GetCell(bx + x + 0, bz + z + 1);
			LandCell br = island.GetCell(bx + x + 1, bz + z + 1);

			// construct positions from cell altitudes
			glm::vec3 pTL((x + 0) * LandIsland::CellSize, tl.Altitude() * LandIsland::HeightUnit, ((z + 0) * LandIsland::CellSize));
			glm::vec3 pTR((x + 1) * LandIsland::CellSize, tr.Altitude() * LandIsland::HeightUnit, ((z + 0) * LandIsland::CellSize));
			glm::vec3 pBL((x + 0) * LandIsland::CellSize, bl.Altitude() * LandIsland::HeightUnit, ((z + 1) * LandIsland::CellSize));
			glm::vec3 pBR((x + 1) * LandIsland::CellSize, br.Altitude() * LandIsland::HeightUnit, ((z + 1) * LandIsland::CellSize));

			auto tlMat = countries[tl.Country()].MapMaterials[tl.Altitude() + island.GetNoise(bx + x + 0, bz + z + 0)];
			auto trMat = countries[tr.Country()].MapMaterials[tr.Altitude() + island.GetNoise(bx + x + 1, bz + z + 0)];
			auto blMat = countries[bl.Country()].MapMaterials[bl.Altitude() + island.GetNoise(bx + x + 0, bz + z + 1)];
			auto brMat = countries[br.Country()].MapMaterials[br.Altitude() + island.GetNoise(bx + x + 1, bz + z + 1)];


			// use a lambda so we're not repeating ourselves
			auto make_vert = [](glm::vec3 height, glm::vec3 weight, MapMaterial m1, MapMaterial m2, MapMaterial m3, LandCell cell) -> LandVertex {
				return LandVertex(height, weight,
				                  m1.FirstMaterialIndex, m2.FirstMaterialIndex, m3.FirstMaterialIndex,
				                  m1.SecondMaterialIndex, m2.SecondMaterialIndex, m3.SecondMaterialIndex,
				                  m1.Coeficient, m2.Coeficient, m3.Coeficient,
				                  cell.Light(), cell.Alpha());
			};

			// cell splitting
			// winding order = clockwise
			if (!tl.Split())
			{
				// TR/BR/TL  # #
				//             #
				verts.push_back(make_vert(pTR, glm::vec3(0, 1, 0), tlMat, trMat, brMat, tr));
				verts.push_back(make_vert(pBR, glm::vec3(0, 0, 1), tlMat, trMat, brMat, br));
				verts.push_back(make_vert(pTL, glm::vec3(1, 0, 0), tlMat, trMat, brMat, tl));

				// BR/BL/TL  #
				//           # #
				verts.push_back(make_vert(pBR, glm::vec3(0, 0, 1), tlMat, blMat, brMat, br));
				verts.push_back(make_vert(pBL, glm::vec3(0, 1, 0), tlMat, blMat, brMat, bl));
				verts.push_back(make_vert(pTL, glm::vec3(1, 0, 0), tlMat, blMat, brMat, tl));
			}
			else
			{
				// BL/TL/TR  # #
				//           #
				verts.push_back(make_vert(pBL, glm::vec3(1, 0, 0), blMat, tlMat, trMat, bl));
				verts.push_back(make_vert(pTL, glm::vec3(0, 1, 0), blMat, tlMat, trMat, tl));
				verts.push_back(make_vert(pTR, glm::vec3(0, 0, 1), blMat, tlMat, trMat, tr));	

				// TR/BR/BL    #
				//           # #
				verts.push_back(make_vert(pTR, glm::vec3(0, 0, 1), blMat, brMat, trMat, tr));
				verts.push_back(make_vert(pBR, glm::vec3(0, 1, 0), blMat, brMat, trMat, br));
				verts.push_back(make_vert(pBL, glm::vec3(1, 0, 0), blMat, brMat, trMat, bl));
			}
		}
	}

	return verts;
}

void LandBlock::Draw(ShaderProgram& program)
{
	program.SetUniformValue("blockPosition", _mapPosition);
	_mesh->Draw();
}
