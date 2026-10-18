#ifndef SEABED_SAND_SH
#define SEABED_SAND_SH

// EXPERIMENT (not for merge): the ground under the sea, shared by the land's blocks and the sea bed past them, so the
// two are one surface with no line where they meet. The file that includes it includes underwater.sh first and defines
// SEABED_SAND_TEXTURE (the sand texture), SEABED_FLOOR_TEXTURE (the land's block textures, an array) and
// SEABED_SHORE_TEXTURE (the shore map).

#include "jelly_light.sh"

// x: the layer of SEABED_FLOOR_TEXTURE that holds the ground under the sea, painted with the land's own material
uniform vec4 u_uwSeaFloor;

// The sides of a land block in units, and the texels along a side of its texture
#define SEABED_BLOCK_UNITS 160.0f
#define SEABED_BLOCK_TEXELS 256.0f

// The units of the world in one metre: an adult male villager is 1.661 units and 1.75 metres tall
#define SEABED_UNITS_PER_METRE UW_UNITS_PER_METRE

// Gradient (Perlin) noise from about -1 to 1, with a smooth fifth-order fade between its lattice points
float SeabedPerlin(vec2 p)
{
	vec2 i = floor(p);
	vec2 f = fract(p);
	vec2 u = f * f * f * (f * (f * 6.0f - 15.0f) + 10.0f);
	float a = dot(UwHash2(i) * 2.0f - 1.0f, f);
	float b = dot(UwHash2(i + vec2(1.0f, 0.0f)) * 2.0f - 1.0f, f - vec2(1.0f, 0.0f));
	float c = dot(UwHash2(i + vec2(0.0f, 1.0f)) * 2.0f - 1.0f, f - vec2(0.0f, 1.0f));
	float d = dot(UwHash2(i + vec2(1.0f, 1.0f)) * 2.0f - 1.0f, f - vec2(1.0f, 1.0f));
	return 1.4f * mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

// Turns a point by a fixed angle and moves it, so that the layers of a noise do not line up
vec2 SeabedTurn(vec2 p)
{
	return vec2(0.8f * p.x - 0.6f * p.y, 0.6f * p.x + 0.8f * p.y) * 2.03f + vec2(17.1f, 9.3f);
}

// Fractal noise of four layers of gradient noise, each turned and at twice the size of the one before, about -1 to 1
float SeabedFbm(vec2 p)
{
	float sum = 0.0f;
	float amplitude = 0.5f;
	for (int i = 0; i < 4; ++i)
	{
		sum += amplitude * SeabedPerlin(p);
		p = SeabedTurn(p);
		amplitude *= 0.5f;
	}
	return sum;
}

// Fractal noise of two layers, for features that do not need the fine layers
float SeabedFbm2(vec2 p)
{
	return 0.66f * SeabedPerlin(p) + 0.34f * SeabedPerlin(SeabedTurn(p));
}

// Ridged noise from 0 to 1: sharp crests where the gradient noise goes through zero
float SeabedRidged(vec2 p)
{
	float ridge = 1.0f - abs(SeabedPerlin(p));
	return ridge * ridge;
}

// Cellular (Worley) noise: x the distance to the nearest of the random points, y to the second nearest, zw the offset
// from the nearest point. One random point is in each lattice cell.
vec4 SeabedCells(vec2 p)
{
	vec2 i = floor(p);
	vec2 f = fract(p);
	float first = 8.0f;
	float second = 8.0f;
	vec2 offset = vec2_splat(0.0f);
	for (int y = -1; y <= 1; ++y)
	{
		for (int x = -1; x <= 1; ++x)
		{
			vec2 cell = vec2(float(x), float(y));
			vec2 to = cell + 0.1f + 0.8f * UwHash2(i + cell) - f;
			float d = length(to);
			if (d < first)
			{
				second = first;
				first = d;
				offset = -to;
			}
			else if (d < second)
			{
				second = d;
			}
		}
	}
	return vec4(first, second, offset);
}

// The centre of a hexagon in repeats of the texture, from its corner in the skewed grid
vec2 SeabedHexCentre(vec2 vertex, float cells)
{
	return vec2(vertex.x, 0.5f * vertex.x + 0.8660254f * vertex.y) / 3.4641016f * cells;
}

// One hexagon's read of the sand texture: turned about the hexagon's centre and moved, both at random for the hexagon
vec3 SeabedHexRead(vec2 uv, vec2 vertex, float cells)
{
	vec2 random = UwHash2(vertex + vec2(71.0f, 13.0f));
	float angle = random.x * 6.2831853f;
	vec2 centre = SeabedHexCentre(vertex, cells);
	vec2 d = uv - centre;
	vec2 turned = vec2(cos(angle) * d.x - sin(angle) * d.y, sin(angle) * d.x + cos(angle) * d.y);
	return texture2D(SEABED_SAND_TEXTURE, centre + turned + random * 17.0f).rgb;
}

// The sand texture read with no grid that shows: the plane is cut into hexagons, each reads the texture with its own
// random offset and turn, and the three hexagons round a point blend so that the grains keep their contrast (after
// Mikkelsen, "Practical Real-Time Hex-Tiling", 2022). uv is in repeats of the texture, cells the size of a hexagon in
// repeats.
vec3 SeabedHexSand(vec2 uv, float cells)
{
	vec2 hexUv = uv / cells;
	vec2 st = vec2(hexUv.x, -0.57735027f * hexUv.x + 1.15470054f * hexUv.y) * 3.4641016f;
	vec2 base = floor(st);
	vec2 inCell = fract(st);
	float z = 1.0f - inCell.x - inCell.y;
	float s = step(0.0f, -z);
	float s2 = 2.0f * s - 1.0f;
	vec3 w = vec3(-z * s2, s - inCell.y * s2, s - inCell.x * s2);
	vec3 c1 = SeabedHexRead(uv, base + vec2(s, s), cells);
	vec3 c2 = SeabedHexRead(uv, base + vec2(s, 1.0f - s), cells);
	vec3 c3 = SeabedHexRead(uv, base + vec2(1.0f - s, s), cells);
	w = w * w * w * w * w * w * w;
	w /= max(w.x + w.y + w.z, 1e-6f);
	return c1 * w.x + c2 * w.y + c3 * w.z;
}

// The height of the sea bed at a point, from the shore map, filtered between its corners
float SeabedHeightAt(vec2 xz)
{
	return texture2DLod(SEABED_SHORE_TEXTURE, UwShoreUv(xz), 0.0f).w;
}

// The smooth normal of the sea bed at a point, from the heights of the shore map one corner to each side. The land's
// blocks and the sea bed past them get the same normal at the same point.
vec3 SeabedSmoothNormal(vec2 xz)
{
	float dx = SeabedHeightAt(xz + vec2(10.0f, 0.0f)) - SeabedHeightAt(xz - vec2(10.0f, 0.0f));
	float dz = SeabedHeightAt(xz + vec2(0.0f, 10.0f)) - SeabedHeightAt(xz - vec2(0.0f, 10.0f));
	return normalize(vec3(-dx, 20.0f, -dz));
}

// The normal of the surface under a pixel, from how its place changes across the screen
vec3 SeabedSurfaceNormal(vec3 world)
{
	vec3 normal = normalize(cross(dFdx(world), dFdy(world)));
	return normal.y < 0.0f ? -normal : normal;
}

// The normal of the ground under the sea: the surface's own normal near the sea's level, where the land's shape counts,
// and the smooth normal of the sea bed deeper down
vec3 SeabedNormal(vec3 world)
{
	vec3 surface = SeabedSurfaceNormal(world);
	return normalize(mix(surface, SeabedSmoothNormal(world.xz), smoothstep(0.5f, 2.0f, -world.y)));
}

// The block texture's layer of the ground under the sea, at a point, filtered across its texels as the land's blocks
// filter theirs. It repeats from block to block.
vec3 SeabedFloorTexel(vec2 xz)
{
	// The block's texture runs along z, then x
	vec2 position = fract(xz.yx / SEABED_BLOCK_UNITS) * SEABED_BLOCK_TEXELS - 0.5f;
	vec2 first = floor(position);
	vec2 blend = position - first;
	vec2 low = mod(first + SEABED_BLOCK_TEXELS, SEABED_BLOCK_TEXELS);
	vec2 high = mod(first + 1.0f, SEABED_BLOCK_TEXELS);
	float layer = u_uwSeaFloor.x;
	vec3 a = texture2DArrayLod(SEABED_FLOOR_TEXTURE, vec3((low + 0.5f) / SEABED_BLOCK_TEXELS, layer), 0.0f).rgb;
	vec3 b = texture2DArrayLod(SEABED_FLOOR_TEXTURE, vec3((vec2(high.x, low.y) + 0.5f) / SEABED_BLOCK_TEXELS, layer), 0.0f).rgb;
	vec3 c = texture2DArrayLod(SEABED_FLOOR_TEXTURE, vec3((vec2(low.x, high.y) + 0.5f) / SEABED_BLOCK_TEXELS, layer), 0.0f).rgb;
	vec3 d = texture2DArrayLod(SEABED_FLOOR_TEXTURE, vec3((high + 0.5f) / SEABED_BLOCK_TEXELS, layer), 0.0f).rgb;
	return mix(mix(a, b, blend.x), mix(c, d, blend.x), blend.y);
}

// The ground under the sea painted with the land's own material, at a point. Two reads of the repeating layer, one moved
// and turned a quarter, blend by noise, and the tone changes over large areas, so the blocks do not show as a grid. The
// land's blocks and the sea bed past them use the same function, so the two agree where they meet.
vec3 SeabedFloorAlbedo(vec2 xz)
{
	vec2 metres = xz / SEABED_UNITS_PER_METRE;
	vec3 first = SeabedFloorTexel(xz);
	vec3 second = SeabedFloorTexel(vec2(-xz.y, xz.x) + vec2(53.0f, 97.0f));
	float share = smoothstep(-0.3f, 0.3f, SeabedFbm2(metres * 0.012f + 5.0f));
	vec3 albedo = mix(first, second, share);
	return albedo * (1.0f + 0.12f * SeabedFbm2(metres * 0.004f + 61.0f));
}

// How much ground one pixel covers at a point, about: more far away and where the view meets the ground at a low angle
float SeabedFootprint(vec3 world, vec3 normal)
{
	float viewDistance = length(world - u_uwEye.xyz);
	vec3 toEye = (u_uwEye.xyz - world) / max(viewDistance, 1e-3f);
	return 0.0015f * viewDistance / max(abs(dot(normal, toEye)), 0.12f);
}

// How much of the sun's or the moon's light comes past the hills to a point of the sea's surface, 0 to 1. A few steps go
// towards the light over the land's heights from the shore map, as the map of the sun's visibility is made, so the
// caustics go where the hills' shadow lies on the water.
float SeabedSunLight(vec2 surface)
{
	vec3 towards = u_uwAbove.xyz;
	float across = length(towards.xz);
	if (towards.y <= 0.01f || across < 1e-4f)
	{
		return towards.y > 0.01f ? 1.0f : 0.0f;
	}
	vec2 dir = towards.xz / across;
	float rise = towards.y / across;
	float lit = 1.0f;
	for (int i = 1; i <= 8; ++i)
	{
		float d = float(i * i) * 6.0f;
		float over = u_uwParams.z + rise * d - SeabedHeightAt(surface + dir * d);
		lit = min(lit, clamp(over / 6.0f + 0.5f, 0.0f, 1.0f));
	}
	return lit;
}

// The light on the ground under the sea, for a colour of the ground: the land's light (landLight), the light from the
// surface along its bent direction with the caustics where it went through the surface, and the light of the jellyfish
vec3 SeabedLit(vec3 albedo, vec3 world, vec3 landLight, vec3 normal)
{
	vec2 p = world.xz;
	float t = u_uwParams.y;
	float viewDistance = length(world - u_uwEye.xyz);
	float depth = max(u_uwParams.z - world.y, 0.0f);
	float footprint = SeabedFootprint(world, normal);
	vec3 towards = u_uwLight.xyz;
	vec2 surface = p + towards.xz / max(towards.y, 0.2f) * depth;
	float sharpness = 1.0f - smoothstep(4.0f, 40.0f, depth);
	// The caustics come from the waves over the bed (UwCausticsFromWaves); 0.55 is the even light
	float caustics = u_uwAbove.w > 0.5f ? 0.55f * mix(1.0f, UwCausticsFromWaves(surface, t, depth), sharpness) : 0.55f;
	// A pond has no sea waves: its own ripples make its caustics
	float pond = UwShorePond(texture2DLod(SEABED_SHORE_TEXTURE, UwShoreUv(surface), 0.0f));
	if (pond > 0.0f && u_uwAbove.w > 0.5f)
	{
		caustics = mix(caustics, 0.55f * mix(1.0f, UwPondCaustics(surface, t, depth), sharpness), pond);
	}
	// A storm's rough surface spreads the caustics out
	caustics = mix(caustics, 0.55f, 0.4f * u_uwWaves.y);
	// The caustics too become an even light where their lines would be smaller than a pixel
	caustics = mix(caustics, 0.55f, smoothstep(0.08f, 0.3f, footprint));
	// They show from just under the surface, in the shallows and on the beaches, and they fade with the depth as the
	// water takes the light that makes them
	caustics = mix(0.55f, caustics, exp(-0.04f * depth));
	caustics *= smoothstep(0.0f, 0.15f, depth) * (1.0f - smoothstep(30.0f, 60.0f, viewDistance) * 0.6f);
	float facing = max(dot(normal, towards), 0.0f);
	// The lit and shaded sides, and the light the water lets down to this depth
	vec3 light = u_uwLightColour.rgb * UwTransmittance(depth * 0.6f / max(towards.y, 0.2f));
	// No caustics where the hills' shadow lies on the water above
	vec3 sun = light * facing * (0.6f + 1.0f * caustics * SeabedSunLight(surface));
	// The light of the jellyfish near the ground, and the soft pools of their light that the water scatters
	vec3 jellyfish = JellyLightAt(world, normal, 0.0f, -1.0f) + JellyPoolAt(world, normal);
	return albedo * (landLight * (0.6f + 0.4f * facing) + sun + jellyfish);
}

// The sand of the sea bed at a point, lit by the light under the water and its caustics. Layers of noise change all of
// it over the sea bed, so no part repeats: the tone and the hue of the sand, wet and dark patches against pale patches
// of broken shell, silt and pebbles, and the direction, the size and the strength of the ripples, which also follow the
// current and the depth. landLight is the light of the land at the point, and normal the surface's own normal.
vec3 SeabedSand(vec3 world, vec3 landLight, vec3 normal)
{
	vec2 p = world.xz;
	vec2 metres = p / SEABED_UNITS_PER_METRE;
	float depth = max(u_uwParams.z - world.y, 0.0f);
	float steep = 1.0f - clamp(normal.y, 0.0f, 1.0f);
	float footprint = SeabedFootprint(world, normal);

	// A slow warp of the sea bed's plane: the patches and the ripples bend and do not lie in rows
	vec2 warp = vec2(SeabedFbm2(metres * 0.011f + 3.7f), SeabedFbm2(metres * 0.011f + 11.3f)) * 22.0f;
	vec2 q = metres + warp;

	// The ripples. Their crests lie across the current at the point, turned by noise where the current is weak. They are
	// small in the shallows, where the waves make them, and longer deeper down, where the current makes them. Each set
	// of ripples has a fixed direction and a fixed wavelength, and the sets blend by weights that change over the sea
	// bed: a direction or a wavelength that changed from point to point would bunch the crests far from the origin.
	vec2 shift;
	vec2 flow = UwCurrent(vec3(p.x, world.y, p.y), 0.0f, shift);
	float flowSpeed = length(flow);
	vec2 flowDir = flowSpeed > 1e-4f ? flow / flowSpeed : vec2(1.0f, 0.0f);
	float noiseTurn = SeabedFbm2(q * 0.006f) * 3.2f + SeabedPerlin(q * 0.03f + 4.0f) * 0.5f;
	float follow = smoothstep(0.05f, 0.6f, flowSpeed) * 0.75f;
	float turn = mix(noiseTurn, atan2(flowDir.y, flowDir.x) + noiseTurn * 0.25f, follow);
	float deepRipples = smoothstep(3.0f, 25.0f, depth);
	// The size that the ripples have here, as a step from 0 to 2 between the three wavelengths of the sets
	float size = log2(mix(1.0f, 1.8f, SeabedFbm2(q * 0.02f + 21.0f) * 0.5f + 0.5f) * mix(1.0f, 3.6f, deepRipples));
	float bendUnits = SeabedFbm(q * 0.04f) * 4.0f;
	float lee = mix(0.6f, 0.8f, follow);
	float footprint6 = footprint * 6.2831f;
	vec3 ripple = vec3_splat(0.0f);
	for (int d = 0; d < 4; ++d)
	{
		float angle = float(d) * 0.7853982f;
		float weight = max(cos(2.0f * (turn - angle)), 0.0f);
		weight = weight * weight * weight;
		if (weight < 0.02f)
		{
			continue;
		}
		// The steep side of the crests faces away from the current
		vec2 across = vec2(cos(angle), sin(angle));
		across *= dot(across, flowDir) < 0.0f ? -1.0f : 1.0f;
		for (int s = 0; s < 3; ++s)
		{
			float sizeWeight = max(1.0f - abs(size - float(s)), 0.0f) * weight;
			if (sizeWeight < 0.02f)
			{
				continue;
			}
			float wavelength = SEABED_UNITS_PER_METRE * 1.3f * exp2(float(s));
			// Ripples smaller than about a pixel fade out, so they do not flicker or make rings
			float fade = 1.0f - smoothstep(0.2f, 0.6f, footprint6 / wavelength);
			float wave = fract(dot(p, across) / wavelength + bendUnits / wavelength + float(d) * 0.37f);
			float profile = wave < lee ? wave / lee : (1.0f - wave) / (1.0f - lee);
			float height = profile * profile * (3.0f - 2.0f * profile) - 0.5f;
			float rise = (wave < lee ? 1.0f / lee : -1.0f / (1.0f - lee)) * 6.0f * profile * (1.0f - profile) / wavelength;
			ripple += vec3(height, across * rise) * sizeWeight * fade;
		}
	}
	// The strength: no ripples in patches of calm sand, more where the current is strong, less on steep ground
	float rippledShare = smoothstep(-0.25f, 0.2f, SeabedFbm2(q * 0.008f + 57.0f));
	float strength = 0.45f * rippledShare * (0.7f + 0.6f * smoothstep(0.1f, 0.8f, flowSpeed)) * max(1.0f - steep * 2.0f, 0.0f);
	float crest = ripple.x * strength / 0.45f;
	vec2 gradient = ripple.yz * 0.9f * strength;
	// Fine ridges in the troughs, by ridged noise
	float ridges = SeabedRidged(q * 1.9f + bendUnits * 0.3f) * (1.0f - smoothstep(0.03f, 0.12f, footprint));
	vec3 rippled = normalize(normal + vec3(-gradient.x, 0.0f, -gradient.y));

	// The sand texture, read with no grid at two sizes. Where its grains are smaller than a pixel, its mean colour takes
	// their place, so it does not flicker.
	vec3 meanSand = vec3(0.80f, 0.72f, 0.56f);
	vec3 sand = mix(SeabedHexSand(p * 0.085f, 1.3f), meanSand, smoothstep(0.01f, 0.05f, footprint));
	vec3 grains = mix(SeabedHexSand(p * 0.023f + 0.37f, 0.9f), meanSand, smoothstep(0.04f, 0.2f, footprint));

	// The tone and the hue of the sand change over large areas: paler and yellow, or darker and grey-brown
	float macro = SeabedFbm(metres * 0.0025f + 90.0f);
	float hue = SeabedFbm2(metres * 0.006f + 33.0f);
	sand *= 1.0f + 0.16f * macro;
	sand = mix(sand, sand * vec3(1.05f, 1.0f, 0.86f), clamp(hue, 0.0f, 1.0f));
	sand = mix(sand, sand * vec3(0.9f, 0.92f, 0.98f), clamp(-hue, 0.0f, 1.0f));

	// Wet, darker sand in broad bending patches
	float wet = smoothstep(0.05f, 0.35f, SeabedFbm(q * 0.009f + 3.0f));
	// Pale broken shell in the troughs of the ripples and in patches, with flecks in cells
	float shellPatch = smoothstep(0.15f, 0.45f, SeabedFbm(q * 0.018f + 21.0f));
	vec4 shellCells = SeabedCells(q * 2.6f);
	float flecks = (1.0f - smoothstep(0.08f, 0.2f, shellCells.x)) * step(0.55f, UwHash(floor(q * 2.6f) + 5.0f));
	flecks *= 1.0f - smoothstep(0.02f, 0.08f, footprint);
	float shell = clamp(shellPatch * (0.55f + 0.45f * (0.5f - crest)) + flecks * 0.6f, 0.0f, 1.0f);
	// Silt deeper down and on slopes, more in still water
	float silt = clamp(smoothstep(18.0f, 60.0f, depth) * 0.7f + steep * 1.5f, 0.0f, 0.85f);
	silt *= 0.55f + 0.45f * (SeabedFbm2(q * 0.025f + 7.0f) * 0.5f + 0.5f);
	silt *= 1.0f - 0.35f * smoothstep(0.2f, 0.9f, flowSpeed);
	// Pebbles: round stones in cells, in patches that grow on slopes and where the current is strong
	float pebbleCover = smoothstep(0.2f, 0.5f, SeabedFbm2(q * 0.02f + 70.0f) + steep * 1.2f + 0.2f * flowSpeed);
	vec4 stones = SeabedCells(q * 1.4f);
	float stoneSize = mix(0.18f, 0.38f, UwHash(floor(q * 1.4f) + 2.0f));
	float pebble = (1.0f - smoothstep(stoneSize * 0.7f, stoneSize, stones.x)) * pebbleCover;
	pebble *= step(0.35f, UwHash(floor(q * 1.4f) + 9.0f)) * (1.0f - smoothstep(0.05f, 0.15f, footprint));
	// A pebble is round: its normal leans out from its centre
	vec3 stoneNormal = normalize(rippled + vec3(stones.z, 0.0f, stones.w) * (2.5f / max(stoneSize, 0.05f)) * pebble * 0.5f);

	vec3 albedo = sand;
	albedo = mix(albedo, sand * vec3(0.66f, 0.62f, 0.55f), wet * 0.75f);
	albedo = mix(albedo, mix(sand, vec3(0.93f, 0.9f, 0.82f), 0.6f) * (0.9f + 0.2f * grains.r), shell * 0.7f);
	vec3 siltColour = vec3(0.42f, 0.40f, 0.34f) * (0.8f + 0.4f * grains.g);
	albedo = mix(albedo, siltColour, silt);
	vec3 pebbleColour = mix(vec3(0.30f, 0.29f, 0.27f), vec3(0.55f, 0.50f, 0.44f), UwHash(floor(q * 1.4f) + 4.0f));
	albedo = mix(albedo, pebbleColour * (0.85f + 0.3f * grains.b), pebble);
	albedo *= 0.9f + 0.12f * crest + 0.06f * (ridges - 0.5f);
	return SeabedLit(albedo, world, landLight, normalize(mix(rippled, stoneNormal, pebble)));
}

// The ground under the sea at a point, the same on the land's blocks and past them. ground is the land's own colour
// there before its light: on the blocks the block's texture over the sea floor by its coast alpha, past them the sea
// floor alone. Near the shore the land's own texture shows under the water; deeper down it changes gradually to the
// sand, which keeps off steep ground. The ground is wet and darker below the highest run of the waves up the beach,
// and damp where the tide left it. sandShare gives how much of the result is sand.
vec3 SeabedGround(vec3 world, vec3 ground, vec3 landLight, vec3 normal, out float sandShare)
{
	float steepness = 1.0f - normal.y;
	float depth = -world.y;
	sandShare = smoothstep(2.0f, 12.0f, depth) * (1.0f - smoothstep(0.25f, 0.55f, steepness));
	vec3 colour = SeabedLit(ground, world, landLight, normal);
	if (sandShare > 0.0f)
	{
		colour = mix(colour, SeabedSand(world, landLight, normal), sandShare);
	}
	float level = u_uwParams.z;
	vec4 shore = texture2DLod(SEABED_SHORE_TEXTURE, UwShoreUv(world.xz), 0.0f);
	float face;
	float runUp = level + UwShoreWave(shore.x, level - shore.w, world.xz, UwShoreCliff(shore), UwShorePond(shore), face);
	float wetTop = level + u_uwWaves.w * 1.4f + 0.15f;
	float wet = 1.0f - smoothstep(runUp, wetTop, world.y);
	wet = max(wet, 0.45f * (1.0f - smoothstep(-0.3f, 0.05f, world.y)));
	// Deeper under the water the ground is not darker for being wet: the water's own colour takes its place
	wet *= 1.0f - smoothstep(2.0f, 6.0f, level - world.y);
	return mix(colour, colour * 0.6f, wet);
}

#endif // SEABED_SAND_SH
