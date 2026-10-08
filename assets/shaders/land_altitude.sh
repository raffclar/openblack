#ifndef LAND_ALTITUDE_SH
#define LAND_ALTITUDE_SH

// The GPU side of src/3D/LandMorph.h: the original's land altitude (on the CPU, LandIsland::HeightAt) and the
// melting of the morphable objects (they follow the land under them as they are drawn). Wiki:
// rendering-objects.md, "Mallas pegadas al suelo". The CPU and this one must stay the same: split bit, sea flattening.

SAMPLER2D(s_heightmap, 1); // per cell: r = altitude (height units), g = split bit (LandIsland::CreateHeightMap)
uniform vec4 u_islandExtent;

vec2 HeightCell(vec2 cell, vec2 size)
{
	return texture2DLod(s_heightmap, (cell + 0.5f) / size, 0.0f).rg;
}

// The land altitude (LandIsland::HeightAt): the landscape triangle under the point. Each cell is split
// along the diagonal its split bit chooses and the fourth corner is extrapolated from the other three, so the
// bilinear blend is planar on that triangle; next to the sea (base corner <= 4) heights of 3 or less count as 0.
float LandAltitude(vec2 xz)
{
	vec2 size = (u_islandExtent.zw - u_islandExtent.xy) * 0.1f + 1.0f;
	vec2 position = (xz - u_islandExtent.xy) * 0.1f;
	vec2 cell = floor(position);
	vec2 f = position - cell;
	vec2 base = HeightCell(cell, size);
	float v00 = base.r;
	float v01 = HeightCell(cell + vec2(0.0f, 1.0f), size).r;
	float v10 = HeightCell(cell + vec2(1.0f, 0.0f), size).r;
	float v11 = HeightCell(cell + vec2(1.0f, 1.0f), size).r;
	if (v00 <= 4.0f)
	{
		v00 = v00 > 3.0f ? v00 : 0.0f;
		v01 = v01 > 3.0f ? v01 : 0.0f;
		v10 = v10 > 3.0f ? v10 : 0.0f;
		v11 = v11 > 3.0f ? v11 : 0.0f;
	}
	float c00 = v00;
	float c01 = v01;
	float c10 = v10;
	float c11 = v11;
	if (base.g > 0.5f)
	{
		if (f.y > 1.0f - f.x)
		{
			c00 = v10 + v01 - v11;
		}
		else
		{
			c11 = v10 + v01 - v00;
		}
	}
	else if (f.x > f.y)
	{
		c01 = v00 + v11 - v10;
	}
	else
	{
		c10 = v00 + v11 - v01;
	}
	return mix(mix(c00, c01, f.y), mix(c10, c11, f.y), f.x) * 0.67f;
}

// The melting: delta = (H(w) - H(origin)) x (1 / scale),
// added to the model vertex's y before the object's matrix, so in the world it is localY x delta / scale, with localY
// the drawn matrix's column 1 (its up axis) and scale the object's scale. The object's own altitude over the
// land (a pile sinking, a field's food) is kept. `world` is the vertex in the world without the delta. An upright
// object (column 1 = (0, s, 0)) gets y += H(w) - H(origin) exactly; the one tilted user is the ripe field's sway
// (a shear of column 1 along z), which also moves along z, as in the original.
vec3 LandMelting(vec3 world, vec2 origin, vec3 localY, float scale)
{
	float delta = LandAltitude(world.xz) - LandAltitude(origin);
	if (localY.x == 0.0f && localY.z == 0.0f)
	{
		world.y += delta;
		return world;
	}
	return world + localY * (delta / scale); // the original multiplies by 1 / scale
}

#endif // LAND_ALTITUDE_SH
