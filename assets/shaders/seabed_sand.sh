#ifndef SEABED_SAND_SH
#define SEABED_SAND_SH

// EXPERIMENT (not for merge): the sand of the sea bed, shared by the land and the sea bed past the land's blocks. The
// file that includes it defines SEABED_SAND_TEXTURE, the sampler that holds the sand texture, and includes
// underwater.sh first.

// The normal of the surface under a pixel, from how its place changes across the screen
vec3 SeabedSurfaceNormal(vec3 world)
{
	vec3 normal = normalize(cross(dFdx(world), dFdy(world)));
	return normal.y < 0.0f ? -normal : normal;
}

// The sand of the sea bed at a point: patches of sand of other kinds, ripples that change their way and size over the
// sea bed, lit by the light under the water and its caustics. landLight is the light of the land at the point, and
// normal the surface's own normal.
vec3 SeabedSand(vec3 world, vec3 landLight, vec3 normal)
{
	vec2 p = world.xz;
	float t = u_uwParams.y;
	float viewDistance = length(world - u_uwEye.xyz);
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
	// How much ground one pixel covers here, about: more far away and where the view meets the ground at a low angle.
	// Ripples smaller than about a pixel fade out, so they do not flicker or make rings.
	vec3 toEye = (u_uwEye.xyz - world) / max(viewDistance, 1e-3f);
	float footprint = 0.0015f * viewDistance / max(abs(dot(normal, toEye)), 0.12f);
	float rippleFade = 1.0f - smoothstep(0.5f, 1.4f, footprint * 6.2831f / wavelength);
	// Fewer ripples on steep ground and in silt
	float strength = 0.4f * rippleFade * (1.0f - steep * 2.0f);
	vec2 gradient = across * (6.2831f / wavelength) * 0.3f * slope * max(strength, 0.0f);
	vec2 crossing = vec2(-across.y, across.x);
	float fine = sin(dot(p, crossing) * 5.3f + bend * 2.0f) * (1.0f - smoothstep(0.5f, 1.4f, footprint * 5.3f));
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

	// The light comes from the surface along its bent direction: the caustics are where it went through the surface
	vec3 towards = u_uwLight.xyz;
	vec2 surface = p + towards.xz / max(towards.y, 0.2f) * depth;
	float sharpness = 1.0f - smoothstep(4.0f, 40.0f, depth);
	float caustics = u_uwAbove.w > 0.5f ? UwCaustics(surface * 0.21f, t * 1.1f, sharpness) : 0.5f;
	// The caustics too become an even light where their lines would be smaller than a pixel
	caustics = mix(caustics, 0.55f, smoothstep(0.08f, 0.3f, footprint));
	caustics *= smoothstep(0.0f, 1.2f, depth) * (1.0f - smoothstep(30.0f, 60.0f, viewDistance) * 0.6f);
	float facing = max(dot(rippled, towards), 0.0f);
	// The ripples' lit and shaded sides, and the light the water lets down to this depth
	vec3 light = u_uwLightColour.rgb * UwTransmittance(depth * 0.6f / max(towards.y, 0.2f));
	vec3 sun = light * facing * (0.6f + 1.0f * caustics);
	return albedo * (landLight * (0.6f + 0.4f * facing) + sun);
}

#endif // SEABED_SAND_SH
