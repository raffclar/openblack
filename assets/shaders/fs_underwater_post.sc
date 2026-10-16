$input v_texcoord0, v_texcoord1

#include <bgfx_shader.sh>

#include "underwater.sh"

// EXPERIMENT (not for merge): the experiment's water. The scene is drawn into its own target, with no sea, and this
// pass draws it to the window with the water over it.
// Above the surface: the sea's surface with its waves, the scene under it bent by them and faded by the water, the
// reflections of the scene on it found in the screen (the sky where they leave it), and foam where the water is
// shallow.
// Under the surface: the fog and the absorption of the light, the shafts of light from the surface, the surface seen
// from below (the sky bent through it, a mirror at low angles), the wobble of the water, its colour, the dark edge of
// the screen, and the line where the surface cuts the view as the camera goes through it.

SAMPLER2D(s_sceneColour, 0);
SAMPLER2D(s_sceneDepth, 1);
// The shore map (see UwShoreUv): the distance to the coast, the direction to it and the height of the ground
SAMPLER2D(s_shore, 2);
// The land's block textures, a layer for each block: their alpha is the original game's coast alpha, which fades the
// land into the sea under it
SAMPLER2DARRAY(s_blockTextures, 3);
// The layer of each block of the 32 by 32 map, plus 1; 0 where there is no block
SAMPLER2D(s_blockLayers, 4);

// rgb: the sky's colour, as the haze takes it; w: the near distance of the view
uniform vec4 u_uwSky;

// Where a point of the screen (0 to 1, as the textures are read) is in the world, at a depth of the depth buffer
vec3 WorldAt(vec2 uv, float depth)
{
#if BGFX_SHADER_LANGUAGE_GLSL
	vec3 ndc = vec3(uv * 2.0f - 1.0f, depth * 2.0f - 1.0f);
#else
	vec3 ndc = vec3(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, depth);
#endif
	vec4 p = mul(u_invViewProj, vec4(ndc, 1.0f));
	return p.xyz / p.w;
}

// Where a point of the world is on the screen (xy, 0 to 1 as the textures are read) and its depth (z) as the depth
// buffer keeps it
vec3 ScreenAt(vec4 clip)
{
	vec3 ndc = clip.xyz / clip.w;
#if BGFX_SHADER_LANGUAGE_GLSL
	return vec3(ndc.xy * 0.5f + 0.5f, ndc.z * 0.5f + 0.5f);
#else
	return vec3(ndc.x * 0.5f + 0.5f, 0.5f - ndc.y * 0.5f, ndc.z);
#endif
}

// x: the things that float, where they are; w: 1 for each that there is
uniform vec4 u_uwFloaters[8];

// 1 while the waves show, 0 while they are turned off to measure them
float WavesOn()
{
	return u_uwWaves.x > 0.0f ? 1.0f : 0.0f;
}

// One texel of a block's texture, at a texel along the texture's axes (z, then x), kept inside the block
float BlockAlphaTexel(vec2 texel, float layer)
{
	vec2 inside = clamp(texel, vec2_splat(0.0f), vec2_splat(255.0f));
	return texture2DArrayLod(s_blockTextures, vec3((inside + 0.5f) / 256.0f, layer), 0.0f).a;
}

// How much the original game shows its land over its sea at a point, 0 to 1: the coast alpha of the block's texture
// there, blended between its four nearest texels. It is 0 where there is no block. The water shows only where the
// original sea shows, and as much as it shows: the land is never under the water where the original game shows land.
float OriginalLandAt(vec2 xz)
{
	vec2 block = floor(xz / 160.0f);
	if (block.x < 0.0f || block.y < 0.0f || block.x > 31.0f || block.y > 31.0f)
	{
		return 0.0f;
	}
	float layer = texture2DLod(s_blockLayers, (block + 0.5f) / 32.0f, 0.0f).r;
	if (layer < 0.5f)
	{
		return 0.0f;
	}
	layer -= 1.0f;
	vec2 texel = (xz - block * 160.0f).yx * (256.0f / 160.0f) - 0.5f;
	vec2 first = floor(texel);
	vec2 f = texel - first;
	float low = mix(BlockAlphaTexel(first, layer), BlockAlphaTexel(first + vec2(1.0f, 0.0f), layer), f.x);
	float high = mix(BlockAlphaTexel(first + vec2(0.0f, 1.0f), layer), BlockAlphaTexel(first + vec2(1.0f, 1.0f), layer), f.x);
	return mix(low, high, f.y);
}

// The shore map at a point
vec4 ShoreAt(vec2 xz)
{
	return texture2DLod(s_shore, UwShoreUv(xz), 0.0f);
}

// The height of the surface at a point near the eye
float SurfaceHeight(vec2 p, float t)
{
	vec2 slope;
	float face;
	return UwSurface(p, t, 0.0f, ShoreAt(p), slope, face);
}

// Where a ray from the eye meets the sea's surface: the surface's height is found again three times at the point the
// ray reaches, from the sea's level. `along` is how far along the ray, `slope` and `face` as UwSurface gives them,
// and `shore` the shore map there.
float RayToSurface(vec3 eye, vec3 direction, float t, out vec2 slope, out float face, out vec4 shore)
{
	float along = (u_uwParams.z - eye.y) / direction.y;
	slope = vec2_splat(0.0f);
	face = 0.0f;
	shore = vec4(1000.0f, 0.0f, 0.0f, -160.0f);
	for (int i = 0; i < 3; i++)
	{
		vec2 at = eye.xz + direction.xz * max(along, 0.0f);
		shore = ShoreAt(at);
		float height = UwSurface(at, t, max(along, 0.0f), shore, slope, face);
		along = (height - eye.y) / direction.y;
	}
	return along;
}

// The rings on the water round the things that float, as they bob: the slope they add to the surface and their foam
vec3 FloaterRings(vec2 p, float t)
{
	vec3 rings = vec3_splat(0.0f);
	for (int i = 0; i < 8; i++)
	{
		vec4 floater = u_uwFloaters[i];
		if (floater.w > 0.5f)
		{
			vec2 away = p - floater.xz;
			float d = length(away);
			float wave = sin(d * 2.2f - t * 3.0f) * exp(-d * 0.22f) * smoothstep(0.3f, 1.2f, d);
			rings.xy += away / max(d, 1e-3f) * wave * 0.25f;
			rings.z += (1.0f - smoothstep(0.8f, 2.5f, d)) * (0.5f + 0.5f * UwNoise(p * 3.0f + t));
		}
	}
	return rings;
}

// The sky seen in a direction, for the reflections that leave the screen: brighter towards the horizon
vec3 SkyIn(vec3 direction)
{
	float up = clamp(direction.y, 0.0f, 1.0f);
	return u_uwSky.rgb * mix(1.05f, 0.8f, up);
}

// The reflection of the scene on the surface at p, along r, found by a march across the screen: 16 steps out to 150
// units, the nearer more closely, then 4 steps to close in on the hit. The scene under the surface is not reflected.
// alpha is how much of the hit to take: it fades at the screen's edges and far along the ray.
vec4 ScreenReflection(vec3 p, vec3 r, float jitter)
{
	vec4 start = mul(u_viewProj, vec4(p, 1.0f));
	vec4 end = mul(u_viewProj, vec4(p + r * 150.0f, 1.0f));
	// Not behind the eye
	if (end.w < 0.5f)
	{
		end = mix(start, end, (start.w - 0.5f) / max(start.w - end.w, 1e-4f));
	}
	float before = 0.0f;
	float found = -1.0f;
	for (int i = 1; i <= 16; i++)
	{
		float s = (float(i) - 1.0f + jitter) / 16.0f;
		s = s * s;
		vec3 at = ScreenAt(mix(start, end, s));
		if (at.x < 0.0f || at.x > 1.0f || at.y < 0.0f || at.y > 1.0f)
		{
			break;
		}
		float scene = texture2DLod(s_sceneDepth, at.xy, 0.0f).r;
		// The depth is reversed: the ray is behind the scene where its depth is smaller
		if (scene > 0.0f && at.z < scene && WorldAt(at.xy, scene).y > u_uwParams.z - 0.2f)
		{
			found = s;
			break;
		}
		before = s;
	}
	if (found < 0.0f)
	{
		return vec4_splat(0.0f);
	}
	float low = before;
	float high = found;
	for (int j = 0; j < 4; j++)
	{
		float mid = (low + high) * 0.5f;
		vec3 at = ScreenAt(mix(start, end, mid));
		float scene = texture2DLod(s_sceneDepth, at.xy, 0.0f).r;
		if (at.z < scene)
		{
			high = mid;
		}
		else
		{
			low = mid;
		}
	}
	vec3 hit = ScreenAt(mix(start, end, high));
	vec2 edge = abs(hit.xy * 2.0f - 1.0f);
	float alpha = (1.0f - smoothstep(0.8f, 1.0f, max(edge.x, edge.y))) * (1.0f - smoothstep(0.6f, 1.0f, high));
	return vec4(texture2DLod(s_sceneColour, hit.xy, 0.0f).rgb, alpha);
}

// How much light the water scatters towards the eye along the ray, out to a distance: the shafts of light. The ray is
// sampled in steps, and each sample takes the light that came down to it through the surface along the light's
// direction, where the surface's bright and dark patches make the shafts.
vec3 Shafts(vec3 eye, vec3 direction, float range, float t, float jitter)
{
	const int k_Steps = 14;
	float reach = min(range, 70.0f);
	float stepLength = reach / float(k_Steps);
	vec3 towards = u_uwLight.xyz;
	vec3 colour = vec3_splat(0.0f);
	for (int i = 0; i < k_Steps; i++)
	{
		float along = (float(i) + jitter) * stepLength;
		vec3 p = eye + direction * along;
		float depth = u_uwParams.z - p.y;
		if (depth > 0.0f)
		{
			vec2 surface = p.xz + towards.xz / towards.y * depth;
			float bright = UwSurfaceLight(surface, t * (1.0f + 2.0f * u_uwWaves.y));
			// The light lost on its way down to the sample, and on its way from the sample to the eye
			vec3 lost = UwTransmittance(depth / towards.y + along);
			colour += lost * bright;
		}
	}
	// Water scatters the light mostly forwards: the shafts are brightest looking towards the light
	float cosine = dot(direction, towards);
	float g = 0.55f;
	float phase = (1.0f - g * g) / pow(max(1.0f + g * g - 2.0f * g * cosine, 1e-4f), 1.5f) * 0.08f;
	// A storm's rough surface and its clouds break the shafts up and darken them
	return colour * stepLength * phase * u_uwLightColour.rgb * (1.0f - 0.55f * u_uwWaves.y);
}

// The foam where the water is shallow: in patches that break up and drift, and in lines that run in to the shore with
// the waves. depth is how deep the water is under the point.
float Foam(vec2 p, float depth, float t)
{
	// The shallows of a gentle beach go down slowly, so the bands are thin in depth: a wide band of foam would cover them
	float shallow = 1.0f - smoothstep(0.0f, 0.45f, depth);
	if (shallow <= 0.0f)
	{
		return 0.0f;
	}
	// Two layers of noise that drift different ways, turned so that their cells do not line up
	vec2 q = vec2(p.x * 0.8f - p.y * 0.6f, p.x * 0.6f + p.y * 0.8f);
	float patches = UwFoamNoise(q * 0.7f + vec2(t * 0.25f, -t * 0.18f)) * 0.6f + UwFoamNoise(p * 1.9f - vec2(t * 0.4f, t * 0.31f)) * 0.4f;
	float lines = 0.5f + 0.5f * sin(depth * 9.0f - t * 2.2f + UwNoise(p * 0.4f) * 4.0f);
	float edge = 1.0f - smoothstep(0.0f, 0.06f, depth);
	float foam = smoothstep(0.55f, 0.8f, patches * 0.8f + lines * 0.3f) * shallow * shallow;
	return clamp(max(foam, edge * (0.5f + 0.5f * patches)), 0.0f, 1.0f);
}

void main()
{
	vec2 uv = gl_FragCoord.xy * u_viewTexel.xy;
	float t = u_uwParams.y;
	vec3 eye = u_uwEye.xyz;
	vec3 direction = normalize(WorldAt(uv, 0.5f) - eye);
	float jitter = fract(52.9829189f * fract(dot(gl_FragCoord.xy, vec2(0.06711056f, 0.00583715f))));
	// Where the view's near plane is at this pixel, and whether it is under the surface: the line between the two
	// sides is the surface cutting the view
	vec3 nearPoint = eye + direction * max(u_uwSky.w, 0.1f) * 1.5f;
	float underBy = SurfaceHeight(nearPoint.xz, t) - nearPoint.y;
	float lineWidth = max(fwidth(underBy), 1e-4f);
	bool under = underBy > 0.0f;

	// Under the water, the water bends what is seen through it a little, moving in time
	vec2 wobble = vec2(sin(uv.y * 38.0f + t * 2.1f) + sin(uv.y * 71.0f - t * 1.3f) * 0.5f,
	                   cos(uv.x * 33.0f + t * 1.7f) + cos(uv.x * 63.0f + t * 2.6f) * 0.5f) * 0.0016f;
	vec2 sampleUv = under ? clamp(uv + wobble, vec2_splat(0.001f), vec2_splat(0.999f)) : uv;
	float depth = texture2DLod(s_sceneDepth, sampleUv, 0.0f).r;
	vec3 scene = texture2DLod(s_sceneColour, sampleUv, 0.0f).rgb;
	// Nothing drawn (the clear depth) is far away
	bool sky = depth <= 0.0f;
	vec3 world = sky ? eye + direction * 10000.0f : WorldAt(sampleUv, depth);
	float sceneDistance = length(world - eye);

	vec3 scatter = u_uwScatter.rgb;
	// Beyond the land's blocks nothing is drawn under the water: there the far floor of sand lies at the sea bed's
	// deepest, lit by what light comes down to it
	bool farFloor = sky && direction.y < 0.0f;
	vec3 floorColour = vec3(0.62f, 0.56f, 0.44f) *
	                   (u_uwLightColour.rgb * UwTransmittance(160.0f / max(u_uwLight.y, 0.2f)) * 0.6f + scatter * 0.6f);
	if (farFloor)
	{
		world = eye + direction * ((u_uwParams.z - 160.0f - eye.y) / direction.y);
		sceneDistance = length(world - eye);
	}
	vec3 colour = scene;
	if (under)
	{
		// A little blur, more far away, as the particles in the water soften what is seen
		vec2 texel = u_viewTexel.xy * 1.5f;
		vec3 around = texture2DLod(s_sceneColour, sampleUv + vec2(texel.x, texel.y), 0.0f).rgb +
		              texture2DLod(s_sceneColour, sampleUv + vec2(-texel.x, texel.y), 0.0f).rgb +
		              texture2DLod(s_sceneColour, sampleUv + vec2(texel.x, -texel.y), 0.0f).rgb +
		              texture2DLod(s_sceneColour, sampleUv + vec2(-texel.x, -texel.y), 0.0f).rgb;
		float far = smoothstep(10.0f, 50.0f, sceneDistance);
		scene = mix(scene, around * 0.25f, 0.35f + 0.45f * far);

		// The distance along the ray to the surface, when the ray goes up to it before it meets the scene
		float toSurface = 1e9f;
		vec2 surfaceSlope = vec2_splat(0.0f);
		if (direction.y > 0.0f)
		{
			float face;
			vec4 shore;
			toSurface = max(RayToSurface(eye, direction, t, surfaceSlope, face, shore), 0.0f);
		}
		if (toSurface < sceneDistance)
		{
			// The surface seen from below. The light bends as it comes through: where the ray can go out, the sky and
			// what is above the water show through, bent by the waves; at low angles the surface is a mirror of the
			// dark water below.
			vec3 surfacePoint = eye + direction * toSurface;
			vec3 normal = UwSurfaceNormalOf(surfaceSlope, toSurface);
			vec3 leaving = refract(direction, -normal, 1.333f);
			float ripple = UwSurfaceLight(surfacePoint.xz * 1.7f, t * 1.8f);
			vec3 mirror = scatter * (0.55f + 0.35f * ripple);
			vec3 surface = mirror;
			if (dot(leaving, leaving) > 0.0f)
			{
				vec3 through = SkyIn(leaving);
				if (u_uwFlags.z > 0.5f)
				{
					// What is above the water, from where the bent ray points
					vec3 at = ScreenAt(mul(u_viewProj, vec4(surfacePoint + leaving * 60.0f, 1.0f)));
					if (at.x > 0.0f && at.x < 1.0f && at.y > 0.0f && at.y < 1.0f)
					{
						through = texture2DLod(s_sceneColour, at.xy, 0.0f).rgb;
					}
				}
				// Most of the light goes through near the middle of the window, and less towards its rim
				float window = smoothstep(0.0f, 0.25f, leaving.y);
				surface = mix(mirror, through * (1.1f + 0.4f * ripple) + u_uwLightColour.rgb * ripple * 0.2f, window);
			}
			colour = mix(scatter, surface, UwTransmittance(toSurface));
		}
		else
		{
			// What is under the water: lit by the light that came down to its depth, then faded into the water's colour
			// by its distance, the red first
			float targetDepth = max(u_uwParams.z - world.y, 0.0f);
			vec3 litScene = farFloor ? floorColour : (sky ? vec3_splat(0.0f) : scene * UwTransmittance(targetDepth * 0.4f));
			vec3 left = UwTransmittance(sceneDistance);
			colour = litScene * left + scatter * (1.0f - left);
		}
		// The shafts of light, broken by the moving surface; the jitter of each pixel hides the steps
		if (u_uwFlags.w > 0.5f)
		{
			colour += Shafts(eye, direction, min(sceneDistance, toSurface), t, jitter) * u_uwLightColour.w;
		}

		// The colour of the water: a little less red and more contrast, and the edge of the screen darker
		colour = mix(vec3_splat(dot(colour, vec3(0.299f, 0.587f, 0.114f))), colour, 1.08f);
		colour = colour * vec3(0.94f, 1.0f, 1.03f);
		vec2 centre = uv - 0.5f;
		float vignette = 1.0f - smoothstep(0.35f, 0.85f, length(centre * vec2(1.25f, 1.0f)));
		colour *= mix(0.62f, 1.0f, vignette);
	}
	else if (direction.y < 0.0f)
	{
		// The surface seen from above, where the ray meets it before the scene. The water's height there is the sea's
		// level with the waves, higher where the water gets shallow, and the run of the waves up the beach. The
		// depth of the water is the depth under its level of what the scene shows at the pixel.
		vec2 surfaceSlope;
		float face;
		vec4 shore;
		float toSurface = RayToSurface(eye, direction, t, surfaceSlope, face, shore);
		float waterTop = eye.y + direction.y * toSurface;
		float waterDepth = waterTop - shore.w;
		float lift = waterTop - u_uwParams.z;
		if (toSurface > 0.0f && (toSurface < sceneDistance || farFloor))
		{
			vec3 surfacePoint = eye + direction * toSurface;
			vec3 ring = FloaterRings(surfacePoint.xz, t);
			vec3 normal = UwSurfaceNormalOf(surfaceSlope + ring.xy, toSurface);
			// How deep the water is over what the pixel shows, and over the ground under the surface
			float thickness = farFloor ? 160.0f : waterTop - world.y;
			float cliff = UwShoreCliff(shore);
			// The scene under the surface, bent by the waves: the bend is less where the water is thin, and none
			// where it would show what is above the water
			vec3 below = world;
			vec3 through = farFloor ? floorColour : scene;
			if (u_uwFlags.z > 0.5f)
			{
				float thin = clamp((sceneDistance - toSurface) / 6.0f, 0.0f, 1.0f);
				vec2 bentUv = clamp(uv + normal.xz * 0.05f * thin, vec2_splat(0.001f), vec2_splat(0.999f));
				float bentDepth = texture2DLod(s_sceneDepth, bentUv, 0.0f).r;
				vec3 bentWorld = bentDepth <= 0.0f ? eye + normalize(WorldAt(bentUv, 0.5f) - eye) * 10000.0f
				                                   : WorldAt(bentUv, bentDepth);
				if (bentWorld.y < u_uwParams.z && bentDepth > 0.0f && !sky)
				{
					below = bentWorld;
					through = texture2DLod(s_sceneColour, bentUv, 0.0f).rgb;
				}
			}
			// The water takes the light out on its way down to the scene and back up to the surface, the red first
			float down = max(u_uwParams.z - below.y, 0.0f);
			float path = min(length(below - surfacePoint), 400.0f);
			vec3 left = UwTransmittance(path + down * 0.6f);
			vec3 water = through * left + scatter * (1.0f - left);

			// The reflection: of the scene where the screen has it, else of the sky
			vec3 reflected = reflect(direction, normal);
			reflected.y = abs(reflected.y);
			vec3 mirror = SkyIn(reflected);
			if (u_uwFlags.x > 0.5f)
			{
				vec4 found = ScreenReflection(surfacePoint, reflected, jitter);
				mirror = mix(mirror, found.rgb, found.a);
			}
			float facing = clamp(dot(-direction, normal), 0.0f, 1.0f);
			float fresnel = 0.02f + 0.98f * pow(max(1.0f - facing, 0.0f), 5.0f);
			colour = mix(water, mirror, fresnel);
			// The glint of the sun or the moon on the waves
			float glint = pow(max(dot(reflected, u_uwAbove.xyz), 0.0f), 300.0f);
			colour += u_uwLightColour.rgb * glint * 3.0f * u_uwLightColour.w;

			// The foam where the water is shallow and at the edge of the run up the beach, on the crests that break
			// near the coast, round the things that float, and in a storm on the crests and in streaks along the wind
			if (u_uwFlags.y > 0.5f)
			{
				float foam = Foam(surfacePoint.xz, max(min(thickness, waterDepth), 0.0f), t);
				float crest = lift / max(UwWaveTop(), 1e-3f);
				// The shore wave breaks on its face as it comes in, from 20 units out to the edge of the water
				float breaking = face * (1.0f - smoothstep(2.0f, 20.0f, shore.x)) * smoothstep(0.0f, 0.3f, waterDepth) *
				                 (1.0f - cliff);
				foam = max(foam, breaking * (0.6f + 0.4f * UwNoise(surfacePoint.xz * 1.3f + t)));
				// At the foot of a cliff: a thick band of boiling white water against the rock, and broad sheets of foam
				// that each crash pushes out and that break up into patches and streaks as the current carries them
				// away. The foam builds up between the waves, so it never clears at a cliff.
				if (cliff > 0.01f)
				{
					vec2 shift;
					vec2 flow = UwCurrent(vec3(surfacePoint.x, u_uwParams.z, surfacePoint.z), t, shift);
					vec2 carried = surfacePoint.xz - shift - u_uwCurrent.xy * t;
					float boil = UwFoamNoise(carried * 0.7f - t * 0.5f);
					float band = (1.0f - smoothstep(4.0f, 12.0f, shore.x)) * (0.75f + 0.25f * boil);
					float omega = mix(1.1f, 1.5f, u_uwWaves.y);
					float since = fract((omega * t + UwNoise(surfacePoint.xz * 0.008f) * 1.2f) / 6.2831853f);
					float front = 6.0f + since * (40.0f + 40.0f * u_uwWaves.y);
					float push = smoothstep(front - 25.0f, front, shore.x) * (1.0f - smoothstep(front, front + 4.0f, shore.x));
					float spread = 1.0f - smoothstep(10.0f, 70.0f + 50.0f * u_uwWaves.y, shore.x);
					float pieces = smoothstep(0.38f, 0.62f, UwFoamNoise(carried * 0.18f) * 0.7f + UwFoamNoise(carried * 0.6f) * 0.3f);
					float sheets = spread * mix(pieces, 1.0f, push * 0.6f);
					float strength = 0.7f + 0.3f * u_uwWaves.y + 0.15f * min(u_uwWaves.x, 2.0f);
					foam = max(foam, clamp((band + sheets * 0.85f) * cliff * 1.4f, 0.0f, 1.0f) * strength + length(flow) * 0.0f);
				}
				foam = max(foam, ring.z * 0.6f);
				if (u_uwFlags2.x > 0.5f && u_uwWaves.y > 0.05f)
				{
					vec2 wind = vec2(cos(u_uwWaves.z), sin(u_uwWaves.z));
					vec2 across = vec2(-wind.y, wind.x);
					vec2 q = surfacePoint.xz;
					float caps = smoothstep(0.5f, 0.9f, crest) * smoothstep(0.45f, 0.75f, UwNoise(q * 0.35f + t * 0.4f));
					// Two layers, the second turned against the first, so the noise's square cells do not show
					vec2 streakUv = vec2(dot(q, wind) * 0.04f - t * 0.2f, dot(q, across) * 0.5f);
					vec2 turned = vec2(streakUv.x * 0.8f - streakUv.y * 0.6f, streakUv.x * 0.6f + streakUv.y * 0.8f);
					float streaks = smoothstep(0.6f, 0.85f, UwNoise(streakUv) * 0.6f + UwNoise(turned * 1.7f + 3.1f) * 0.4f);
					foam = max(foam, (caps + streaks * 0.5f) * u_uwWaves.y);
				}
				vec3 foamColour = u_uwLightColour.rgb * (0.4f + 0.6f * max(u_uwAbove.y, 0.0f)) + u_uwSky.rgb * 0.7f +
				                  scatter * 0.5f;
				colour = mix(colour, min(foamColour, vec3_splat(1.0f)), foam * 0.85f);
			}
			// Where the original game shows its land over its sea, the land shows here too, as much as there
			float originalLand = OriginalLandAt(surfacePoint.xz);
			colour = u_uwFlags2.w > 0.5f ? vec3(1.0f, 0.0f, 1.0f) : colour;
			colour = mix(colour, scene, originalLand);
		}
	}
	// The surface that cuts the view, as the camera goes through it: a thin dark line with water running down it
	float meniscus = 1.0f - smoothstep(0.0f, lineWidth * 3.0f, abs(underBy));
	colour = mix(colour, scatter * 0.35f, meniscus * 0.8f);
	gl_FragColor = vec4(colour, 1.0f);
}
