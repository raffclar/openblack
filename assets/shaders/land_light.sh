// The land's light per cell on the GPU: the GPU side of src/3D/LandLight.h (land_light). The cells of this frame, with
// the light and shadow stamps in them: rgb = the cell colour read as a D3DCOLOR (bytes 2, 1, 0), a = the
// luminosity (land_light::Texels). Values here are bytes, 0..255.
#ifndef LAND_LIGHT_SH
#define LAND_LIGHT_SH

// Before including: SAMPLER2D(s_landCells, n) and SAMPLER2D(s_landLightTable, n) under these names
uniform vec4 u_cellMap; // xy: world position of the map's first cell, zw: map size in cells

// The cell's dword, rgb colour and a luminosity, as bytes; off the cell map colour 0 and luminosity 255, like the
// cells of no block
vec4 LandCell(vec2 cell)
{
	if (any(lessThan(cell, vec2_splat(0.0f))) || any(greaterThanEqual(cell, u_cellMap.zw)))
	{
		return vec4(0.0f, 0.0f, 0.0f, 255.0f);
	}
	return floor(texture2DLod(s_landCells, (cell + 0.5f) / u_cellMap.zw, 0.0f) * 255.0f + 0.5f);
}

// table[luminosity] as bytes
vec3 LandTable(float luminosity)
{
	return floor(texture2DLod(s_landLightTable, vec2((luminosity + 0.5f) / 256.0f, 0.5f), 0.0f).rgb * 255.0f + 0.5f);
}

// a + ((b - a) w >> 8) per byte (land_light::LerpBytes)
vec3 LandLerp(vec3 a, vec3 b, float w)
{
	return a + floor((b - a) * w / 256.0f);
}

// The 4 cells from `cell` (map index), lerped along z by w.y and then along x by w.x: diffuse (the lights) in xyz
// and specular (the colours) through `specular`
vec3 LandLightFour(vec2 cell, vec2 w, out vec3 specular)
{
	vec4 c00 = LandCell(cell);
	vec4 c01 = LandCell(cell + vec2(0.0f, 1.0f));
	vec4 c10 = LandCell(cell + vec2(1.0f, 0.0f));
	vec4 c11 = LandCell(cell + vec2(1.0f, 1.0f));
	specular = LandLerp(LandLerp(c00.rgb, c01.rgb, w.y), LandLerp(c10.rgb, c11.rgb, w.y), w.x);
	return LandLerp(LandLerp(LandTable(c00.a), LandTable(c01.a), w.y), LandLerp(LandTable(c10.a), LandTable(c11.a), w.y),
	                w.x);
}

// The bilinear land light at a world x, z: the cell trunc(x 0.1), the weights trunc(frac 256). (aproximado)
// off the map or with c00 in a missing block, each cell counts as no block (luminosity 255, colour 0) instead of the
// whole sample being table[255] and 0xFF000000
vec3 LandLightBilinear(vec2 xz, out vec3 specular)
{
	vec2 position = (xz - u_cellMap.xy) * 0.1f;
	vec2 cell = floor(position);
	return LandLightFour(cell, floor((position - cell) * 256.0f), specular);
}

// The land light from the map coordinates: the cell and the weights CellX >> 8, CellZ >> 8 of the
// global cell (world / 10)
vec3 LandLightCellShift(vec2 xz, out vec3 specular)
{
	vec2 global = floor(xz * 0.1f);
	vec2 cell = floor((xz - u_cellMap.xy) * 0.1f);
	return LandLightFour(cell, floor(global / 256.0f), specular);
}

// The colour of a single cell, as the original's altitude-and-colour lookup takes it
vec3 LandLightCell(vec2 xz, out vec3 specular)
{
	vec4 c = LandCell(floor((xz - u_cellMap.xy) * 0.1f));
	specular = c.rgb;
	return LandTable(c.a);
}

// The full light: table[255]
vec3 LandLightFull()
{
	return LandTable(255.0f);
}

#endif // LAND_LIGHT_SH
