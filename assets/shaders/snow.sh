#ifndef SNOW_SH
#define SNOW_SH

// The snow lying on the island: how deep it is in each 40 unit cell of a 128 by 128 grid, point sampled
SAMPLER2D(s_snowDepth, 11);
// x: 1 when snow is drawn on what is drawn. y: 1 when an object's submesh has bones. z: 1 for the faces of a broken
// shape, which read the snow by where they are in the world. w: 1 for a fence, which reads the snow by each face.
uniform vec4 u_snow;
// The distance from the front face of a broken shape to its back face
#define BROKEN_FACE_DEPTH 0.45f

// How deep the snow lies at a grid point; none off the grid
float SnowDepthCell(vec2 cell)
{
	bool onGrid = all(greaterThanEqual(cell, vec2_splat(0.0f))) && all(lessThan(cell, vec2_splat(128.0f)));
	return onGrid ? texture2DLod(s_snowDepth, (cell + 0.5f) / 128.0f, 0.0f).r : 0.0f;
}

// How deep the snow lies at a point in the world, blended between the four grid points around it
float SnowDepthAt(vec2 xz)
{
	vec2 position = xz * 0.025f;
	if (any(lessThan(position, vec2_splat(0.0f))) || any(greaterThanEqual(position, vec2_splat(127.0f))))
	{
		return 0.0f;
	}
	vec2 cell = floor(position);
	vec2 w = position - cell;
	float near = mix(SnowDepthCell(cell), SnowDepthCell(cell + vec2(1.0f, 0.0f)), w.x);
	float far = mix(SnowDepthCell(cell + vec2(0.0f, 1.0f)), SnowDepthCell(cell + 1.0f), w.x);
	return mix(near, far, w.y);
}

// The snow that an object shows at its level of snow, 0 to 255: an object shows no snow below the level 5
float SnowShownLevel(float level)
{
	return level < 5.0f ? 0.0f : level;
}

// The whole number part of a value, towards zero
float SnowWhole(float value)
{
	return sign(value) * floor(abs(value));
}

// Where an object's vertex reads the snow texture: across by how much the surface faces up, down by how much it faces
// along x, each less a little by where the vertex is in the mesh. The snow's alpha rises across the texture, so the
// snow lies thickest on what faces up. The normal is in the world, or for a submesh with bones it is the normal in the
// file.
vec2 SnowUv(vec3 meshPosition, vec3 worldNormal)
{
	float a = meshPosition.y * 0.3f + meshPosition.z + 20.0f;
	float b = meshPosition.z * 0.1f + meshPosition.x;
	float u = b - SnowWhole(b) - 2.5f;
	float v = a - SnowWhole(a) - 2.5f;
	return vec2(worldNormal.y + 0.2f * (u - SnowWhole(u)), worldNormal.x + 0.2f * (v - SnowWhole(v)));
}

// The up part of the normal of the face of a corner, which a tree or a fence keeps in the third and fourth indices of
// the corner: a low part of 14 bits and a high part, in steps of one 2^28th (snow_faces::UnpackUp)
float SnowFaceUp(ivec4 indices)
{
	return (float(indices.w) * 16384.0f + float(indices.z)) / 268435456.0f;
}

// Where a corner of a tree or a fence reads the snow texture (snow_faces::SnowUv): across, 0.4 for each unit of the up
// part of the face's normal above -1, and 0.2 of the part after the whole of the size of its first texture coordinate;
// down, 0.2 of that part of its second texture coordinate
vec2 SnowFaceUv(float faceUp, vec2 uv)
{
	vec2 size = abs(uv);
	vec2 part = size - floor(size);
	return vec2((faceUp + 1.0f) * 0.4f + 0.2f * part.x, 0.2f * part.y);
}

// The colour the snow is lit in, 0 to 255, from the object's: its red and blue a quarter of the way to its green
vec3 SnowColour(vec3 colour)
{
	return vec3(floor((3.0f * colour.r + colour.g) / 4.0f), colour.g, floor((3.0f * colour.b + colour.g) / 4.0f));
}

#endif // SNOW_SH
