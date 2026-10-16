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

// The height of the surface at a point, with its small waves
float SurfaceHeight(vec2 p, float t)
{
	return u_uwParams.z + 0.07f * sin(p.x * 0.7f + t * 1.6f) + 0.05f * sin(p.y * 0.9f - t * 1.3f) +
	       0.03f * sin((p.x + p.y) * 2.3f + t * 2.9f);
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
			float bright = UwSurfaceLight(surface, t);
			// The light lost on its way down to the sample, and on its way from the sample to the eye
			vec3 lost = UwTransmittance(depth / towards.y + along);
			colour += lost * bright;
		}
	}
	// Water scatters the light mostly forwards: the shafts are brightest looking towards the light
	float cosine = dot(direction, towards);
	float g = 0.55f;
	float phase = (1.0f - g * g) / pow(max(1.0f + g * g - 2.0f * g * cosine, 1e-4f), 1.5f) * 0.08f;
	return colour * stepLength * phase * u_uwLightColour.rgb;
}

// The foam where the water is shallow: in patches that break up and drift, and in lines that run in to the shore with
// the waves. depth is how deep the water is under the point.
float Foam(vec2 p, float depth, float t)
{
	float shallow = 1.0f - smoothstep(0.0f, 1.8f, depth);
	if (shallow <= 0.0f)
	{
		return 0.0f;
	}
	float patches = UwNoise(p * 0.9f + vec2(t * 0.25f, -t * 0.18f)) * UwNoise(p * 2.3f - vec2(t * 0.4f, t * 0.31f));
	float lines = 0.5f + 0.5f * sin(depth * 5.0f - t * 2.2f + UwNoise(p * 0.4f) * 4.0f);
	float edge = 1.0f - smoothstep(0.0f, 0.25f, depth);
	float foam = smoothstep(0.18f, 0.42f, patches * 1.4f + lines * 0.35f * shallow) * shallow;
	return clamp(max(foam, edge * (0.6f + 0.4f * patches)), 0.0f, 1.0f);
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
		float toSurface = direction.y > 0.0f ? (SurfaceHeight(eye.xz, t) - eye.y) / direction.y : 1e9f;
		if (toSurface < sceneDistance)
		{
			// The surface seen from below. The light bends as it comes through: where the ray can go out, the sky and
			// what is above the water show through, bent by the waves; at low angles the surface is a mirror of the
			// dark water below.
			vec3 surfacePoint = eye + direction * toSurface;
			vec3 normal = UwWaveNormal(surfacePoint.xz, t, toSurface);
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
		// The surface seen from above, where the ray meets it before the scene
		float toSurface = (u_uwParams.z - eye.y) / direction.y;
		if (toSurface > 0.0f && (toSurface < sceneDistance || farFloor))
		{
			vec3 surfacePoint = eye + direction * toSurface;
			vec3 normal = UwWaveNormal(surfacePoint.xz, t, toSurface);
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

			// The foam where the water is shallow, lit by the light and the sky
			if (u_uwFlags.y > 0.5f)
			{
				float foam = Foam(surfacePoint.xz, max(u_uwParams.z - world.y, 0.0f), t);
				vec3 foamColour = u_uwLightColour.rgb * (0.4f + 0.6f * max(u_uwAbove.y, 0.0f)) + u_uwSky.rgb * 0.7f +
				                  scatter * 0.5f;
				colour = mix(colour, min(foamColour, vec3_splat(1.0f)), foam * 0.85f);
			}
		}
	}
	// The surface that cuts the view, as the camera goes through it: a thin dark line with water running down it
	float meniscus = 1.0f - smoothstep(0.0f, lineWidth * 3.0f, abs(underBy));
	colour = mix(colour, scatter * 0.35f, meniscus * 0.8f);
	gl_FragColor = vec4(colour, 1.0f);
}
