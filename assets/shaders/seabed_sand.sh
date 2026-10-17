#ifndef SEABED_SAND_SH
#define SEABED_SAND_SH

// EXPERIMENT (not for merge): the ground under the sea, shared by the land's blocks and the sea bed past them, so the
// two are one surface with no line where they meet. The file that includes it includes underwater.sh first and defines
// SEABED_SAND_TEXTURE (the sand texture), SEABED_FLOOR_TEXTURE (the land's block textures, an array) and
// SEABED_SHORE_TEXTURE (the shore map).

// x: the layer of SEABED_FLOOR_TEXTURE that holds the ground under the sea, painted with the land's own material
uniform vec4 u_uwSeaFloor;

// The sides of a land block in units, and the texels along a side of its texture
#define SEABED_BLOCK_UNITS 160.0f
#define SEABED_BLOCK_TEXELS 256.0f

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

// The ground under the sea painted with the land's own material, at a point: the block texture's layer for it, which
// repeats from block to block, filtered across its texels as the land's blocks filter theirs
vec3 SeabedFloorAlbedo(vec2 xz)
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

// How much ground one pixel covers at a point, about: more far away and where the view meets the ground at a low angle
float SeabedFootprint(vec3 world, vec3 normal)
{
	float viewDistance = length(world - u_uwEye.xyz);
	vec3 toEye = (u_uwEye.xyz - world) / max(viewDistance, 1e-3f);
	return 0.0015f * viewDistance / max(abs(dot(normal, toEye)), 0.12f);
}

// The light on the ground under the sea, for a colour of the ground: the land's light (landLight), and the light from
// the surface along its bent direction, with the caustics where it went through the surface
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
	float caustics = u_uwAbove.w > 0.5f ? UwCaustics(surface * UW_CAUSTIC_FREQUENCY, t * 1.4f, sharpness) : 0.5f;
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
	vec3 sun = light * facing * (0.6f + 1.0f * caustics);
	return albedo * (landLight * (0.6f + 0.4f * facing) + sun);
}

// The sand of the sea bed at a point: patches of sand of other kinds, ripples that change their way and size over the
// sea bed, lit by the light under the water and its caustics. landLight is the light of the land at the point, and
// normal the surface's own normal.
vec3 SeabedSand(vec3 world, vec3 landLight, vec3 normal)
{
	vec2 p = world.xz;
	float t = u_uwParams.y;
	float depth = max(u_uwParams.z - world.y, 0.0f);
	float steep = 1.0f - clamp(normal.y, 0.0f, 1.0f);

	// The ripples: low crests across a direction and at a size that both change slowly over the sea bed, a little
	// sharp on one side. They fade far away, where they would be smaller than a pixel.
	float turn = (UwNoise(p * 0.004f) - 0.5f) * 3.0f + (UwNoise(p * 0.013f + 4.0f) - 0.5f) * 1.2f;
	vec2 across = vec2(cos(turn + 0.6f), sin(turn + 0.6f));
	float wavelength = mix(2.4f, 5.2f, UwNoise(p * 0.008f + 11.0f));
	float bend = UwNoise(p * 0.035f) * 5.0f + UwNoise(p * 0.11f) * 1.2f;
	float phase = dot(p, across) * (6.2831f / wavelength) + bend;
	float slope = 0.5f * cos(phase) + 0.28f * cos(2.0f * phase + 0.6f);
	float crest = 0.5f * sin(phase) + 0.14f * sin(2.0f * phase + 0.6f);
	// Ripples smaller than about a pixel fade out, so they do not flicker or make rings
	float footprint = SeabedFootprint(world, normal);
	float rippleFade = 1.0f - smoothstep(0.2f, 0.6f, footprint * 6.2831f / wavelength);
	// Fewer ripples on steep ground and in silt
	float strength = 0.4f * rippleFade * (1.0f - steep * 2.0f);
	vec2 gradient = across * (6.2831f / wavelength) * 0.3f * slope * max(strength, 0.0f);
	vec2 crossing = vec2(-across.y, across.x);
	float fine = sin(dot(p, crossing) * 5.3f + bend * 2.0f) * (1.0f - smoothstep(0.2f, 0.6f, footprint * 5.3f));
	gradient += crossing * fine * 0.1f;
	vec3 rippled = normalize(normal + vec3(-gradient.x, 0.0f, -gradient.y));

	// The kinds of sand: darker wet sand in broad patches, pale sand of broken shell, and silt and pebbles deeper down
	// and on slopes
	// The sand texture has no smaller copies: where its grains are smaller than a pixel, its mean colour takes its place,
	// so it does not flicker or make a grid
	vec3 meanSand = vec3(0.80f, 0.72f, 0.56f);
	vec3 sand = mix(texture2D(SEABED_SAND_TEXTURE, p * 0.085f).rgb, meanSand, smoothstep(0.01f, 0.05f, footprint));
	vec3 grains = mix(texture2D(SEABED_SAND_TEXTURE, p * 0.023f + 0.37f).rgb, meanSand, smoothstep(0.04f, 0.2f, footprint));
	float wet = smoothstep(0.45f, 0.75f, UwNoise(p * 0.012f + 3.0f) * 0.7f + UwNoise(p * 0.05f) * 0.3f);
	float shell = smoothstep(0.62f, 0.85f, UwNoise(p * 0.02f + 21.0f) * 0.8f + UwNoise(p * 0.21f) * 0.2f);
	float silt = clamp(smoothstep(18.0f, 60.0f, depth) * 0.7f + steep * 1.5f, 0.0f, 0.85f);
	silt *= 0.6f + 0.4f * UwNoise(p * 0.03f + 7.0f);
	vec3 albedo = sand;
	albedo = mix(albedo, sand * vec3(0.68f, 0.64f, 0.56f), wet * 0.8f);
	albedo = mix(albedo, mix(sand, vec3(0.93f, 0.9f, 0.82f), 0.6f) * (0.9f + 0.2f * grains.r), shell * 0.7f);
	vec3 siltColour = vec3(0.42f, 0.40f, 0.34f) * (0.8f + 0.4f * grains.g);
	float pebbles = step(0.78f, UwNoise(p * 1.7f)) * steep;
	siltColour = mix(siltColour, vec3(0.30f, 0.29f, 0.27f), pebbles);
	albedo = mix(albedo, siltColour, silt);
	albedo *= 0.9f + 0.1f * crest + 0.05f * fine;
	return SeabedLit(albedo, world, landLight, rippled);
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
