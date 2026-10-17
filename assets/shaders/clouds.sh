#ifndef CLOUDS_SH
#define CLOUDS_SH

// EXPERIMENT (not for merge): the volumetric clouds. The rays go through a layer of cloud over the land and add the
// light that the cloud scatters towards the eye. The original game has billboards for its clouds. The formulas match
// the CPU twins in src/Graphics/VolumetricClouds.cpp, which the tests check.

// x: the bottom of the layer; y: the radius of the world's curve; z: the farthest cloud; w: the sea's level
uniform vec4 u_cloudLayer;
// x: how much of the sky has cloud; y: the type of cloud (0 stratus, 0.5 cumulus, 1 cumulonimbus); z: the rain; w: the
// number of storms
uniform vec4 u_cloudCover;
// xy: how far the wind has moved the clouds; z: how far their shapes have changed; w: the frame's number
uniform vec4 u_cloudDrift;
// xyz: the direction towards the sun or the moon; w: how much of the light is the moon's
uniform vec4 u_cloudLight;
// rgb: the colour and the strength of the light; w: 1 while there is light
uniform vec4 u_cloudLightColour;
// rgb: the light of the sky; w: the flash of the lightning in the sky, 0 to 1
uniform vec4 u_cloudAmbient;
// xyz: the eye; w: 1 while the eye is above the water
uniform vec4 u_cloudEye;
// xyz: where the lightning struck; w: the light of its bolt, 0 to 1
uniform vec4 u_cloudStrike;
// The storms: [i] xy the middle, z the inner radius, w the outer radius; [8 + i] x the strength, y the rain
uniform vec4 u_cloudStorms[16];

// The world units that one tile of each noise covers (volumetric_clouds::k_BaseNoiseTile and the others)
#define CLOUD_BASE_TILE 1500.0f
#define CLOUD_DETAIL_TILE 210.0f
#define CLOUD_WEATHER_TILE 14000.0f
// The top of the layer for each type of cloud, over its bottom
#define CLOUD_STRATUS_DEPTH 160.0f
#define CLOUD_CUMULUS_DEPTH 520.0f
#define CLOUD_CUMULONIMBUS_DEPTH 1500.0f
// How far under the bottom of the layer the ragged bottom of a rain cloud hangs at most
#define CLOUD_LOWEST_DROP 200.0f
// How much light the cloud takes out for each unit, at a density of 1
#define CLOUD_EXTINCTION 0.06f
// The colour of the flash of the lightning
#define CLOUD_FLASH_COLOUR vec3(0.78f, 0.84f, 1.0f)
#define CLOUD_PI 3.14159265f

// The noise volumes, the weather noise and the blue noise
SAMPLER3D(s_cloudBase, 0);
SAMPLER3D(s_cloudDetail, 1);
SAMPLER2D(s_cloudWeather, 2);

float CloudRemap(float value, float fromLow, float fromHigh, float toLow, float toHigh)
{
	return toLow + (value - fromLow) / (fromHigh - fromLow) * (toHigh - toLow);
}

// The top of the layer, over its bottom, for a type of cloud
float CloudLayerDepth(float type)
{
	return type < 0.5f ? mix(CLOUD_STRATUS_DEPTH, CLOUD_CUMULUS_DEPTH, type * 2.0f)
	                   : mix(CLOUD_CUMULUS_DEPTH, CLOUD_CUMULONIMBUS_DEPTH, (type - 0.5f) * 2.0f);
}

// The density of a type of cloud at a height in the layer (0 at its bottom, 1 at the top of the type)
float CloudHeightProfile(float type, float h)
{
	if (h <= 0.0f || h >= 1.0f)
	{
		return 0.0f;
	}
	float stratus = smoothstep(0.0f, 0.08f, h) * (1.0f - smoothstep(0.3f, 0.9f, h));
	float cumulus = smoothstep(0.0f, 0.12f, h) * (1.0f - smoothstep(0.45f, 1.0f, h));
	float cumulonimbus = smoothstep(0.0f, 0.06f, h) * (1.0f - smoothstep(0.82f, 1.0f, h));
	float t = clamp(type, 0.0f, 1.0f);
	return t < 0.5f ? mix(stratus, cumulus, t * 2.0f) : mix(cumulus, cumulonimbus, (t - 0.5f) * 2.0f);
}

float CloudHenyeyGreenstein(float cosAngle, float g)
{
	float g2 = g * g;
	float d = 1.0f + g2 - 2.0f * g * cosAngle;
	return (1.0f - g2) / (4.0f * CLOUD_PI * d * sqrt(d));
}

// The light that reaches a point through an optical depth towards the light, with the light that scatters many times:
// octaves of less extinction, less energy and a wider phase of two lobes
float CloudMultipleScattering(float opticalDepth, float cosAngle)
{
	float sum = 0.0f;
	float extinction = 1.0f;
	float energy = 1.0f;
	float asymmetry = 1.0f;
	for (int octave = 0; octave < 3; ++octave)
	{
		float phase = mix(CloudHenyeyGreenstein(cosAngle, 0.8f * asymmetry), CloudHenyeyGreenstein(cosAngle, -0.35f * asymmetry),
		                  0.35f);
		sum += energy * exp(-opticalDepth * extinction) * phase;
		extinction *= 0.5f;
		energy *= 0.5f;
		asymmetry *= 0.5f;
	}
	return sum;
}

// The powder term: the thin edges of a cloud are darker when the eye sees the side that faces the light
float CloudPowder(float localDepth, float cosAngle)
{
	float powder = 1.0f - exp(-2.0f * localDepth);
	return mix(1.0f, powder, 0.8f * (0.5f - 0.5f * cosAngle));
}

// The height of a point over the curved world under the eye: the world falls away with the square of the distance
float CloudAltitude(vec3 p)
{
	vec2 away = p.xz - u_cloudEye.xz;
	return p.y + dot(away, away) / (2.0f * u_cloudLayer.y);
}

// The weather at a point across the land: the cloud's cover there (0 none), its type and its rain
float CloudWeatherAt(vec2 xz, out float type, out float rain)
{
	vec2 q = xz - u_cloudDrift.xy;
	// A finer copy of the noise bends the place that the weather is read at, so that the edges of the cells are ragged
	vec2 bend = texture2DLod(s_cloudWeather, q * (5.3f / CLOUD_WEATHER_TILE), 0.0f).ga - 0.5f;
	vec4 w = texture2DLod(s_cloudWeather, (q + bend * 900.0f) / CLOUD_WEATHER_TILE, 0.0f);
	// The field changes slowly between two patterns, so that the clouds form and break up as they move
	float turn = 0.5f + 0.5f * sin(u_cloudDrift.z * 0.0035f);
	float field = mix(w.r, w.b, turn);
	float cover = u_cloudCover.x;
	type = clamp(u_cloudCover.y + (w.g - 0.5f) * 0.3f, 0.0f, 1.0f);
	rain = u_cloudCover.z;
	// Each storm builds its own towers round its middle
	for (int i = 0; i < 8; ++i)
	{
		if (float(i) >= u_cloudCover.w)
		{
			break;
		}
		vec4 storm = u_cloudStorms[i];
		vec4 strength = u_cloudStorms[i + 8];
		float reach = 1.0f - smoothstep(storm.z, storm.w, length(xz - storm.xy));
		float s = strength.x * reach;
		cover = mix(cover, 1.0f, s * 0.92f);
		type = mix(type, 1.0f, s);
		rain = max(rain, strength.y * reach);
	}
	return smoothstep(1.0f - cover - 0.12f, 1.0f - cover + 0.3f, field) * smoothstep(0.0f, 0.05f, cover);
}

// The density of the cloud at a point at a height over the curved world. `detail` adds the small shapes that wear away
// the edges; the light's rays leave them out.
float CloudDensity(vec3 p, float altitude, bool detail)
{
	float type;
	float rain;
	float cover = CloudWeatherAt(p.xz, type, rain);
	if (cover <= 0.001f)
	{
		return 0.0f;
	}
	// The bottom of the cloud rises and falls a little; under rain it hangs lower, in ragged lumps
	float lumps = texture2DLod(s_cloudWeather, (p.xz - u_cloudDrift.xy) * (9.1f / CLOUD_WEATHER_TILE), 0.0f).a - 0.5f;
	float bottom = u_cloudLayer.x - rain * 70.0f + lumps * (50.0f + rain * 220.0f);
	// Each cloud is tallest where its cover is densest, at its middle, and low at its edges: round domes
	float h = (altitude - bottom) / (CloudLayerDepth(type) * mix(0.3f, 1.0f, cover));
	float profile = CloudHeightProfile(type, h);
	if (profile <= 0.0f)
	{
		return 0.0f;
	}
	// The clouds move with the wind, their tops a little ahead, and change shape slowly
	vec3 q = p;
	q.xz -= u_cloudDrift.xy * (1.0f + h * 0.15f);
	q.y -= u_cloudDrift.z;
	vec4 n = texture3DLod(s_cloudBase, q / CLOUD_BASE_TILE, 0.0f);
	float low = n.g * 0.625f + n.b * 0.25f + n.a * 0.125f;
	float shape = clamp(CloudRemap(n.r, low - 1.0f, 1.0f, 0.0f, 1.0f), 0.0f, 1.0f);
	// The shape spreads over the whole range, so that the tops rise and fall with it
	shape = clamp((shape - 0.42f) / 0.45f, 0.0f, 1.0f);
	// The anvil: the top of a storm's tower spreads out
	float anvil = smoothstep(0.75f, 1.0f, type) * smoothstep(0.65f, 0.85f, h);
	float coverHere = clamp(cover * (1.0f + anvil * 0.6f), 0.0f, 1.0f);
	// The more cover, the lower the threshold that the shape must pass. Inside a full cell the noise still makes
	// gaps, and the tops follow the noise as the profile falls.
	// The threshold also rises with the height, so that the sides lean in to round tops and no cell has walls
	float threshold = mix(0.85f, 0.25f, coverHere) + h * 0.15f;
	shape = clamp(CloudRemap(shape * profile, threshold, 1.0f, 0.0f, 1.0f), 0.0f, 1.0f) * smoothstep(0.0f, 0.3f, coverHere);
	if (shape <= 0.0f)
	{
		return 0.0f;
	}
	if (detail)
	{
		vec3 d = texture3DLod(s_cloudDetail, (q - vec3(0.0f, u_cloudDrift.z * 2.0f, 0.0f)) / CLOUD_DETAIL_TILE, 0.0f).rgb;
		float fine = d.r * 0.625f + d.g * 0.25f + d.b * 0.125f;
		// Wisps at the bottom, billows at the top
		float wear = mix(fine, 1.0f - fine, clamp(h * 5.0f, 0.0f, 1.0f));
		shape = clamp(CloudRemap(shape, wear * mix(0.4f, 0.6f, h), 1.0f, 0.0f, 1.0f), 0.0f, 1.0f);
	}
	// Rain clouds are denser and darker at the bottom
	return shape * (1.0f + rain * 0.8f * (1.0f - h));
}

// The two roots of a t^2 + b t + c = 0, the smaller first; none (x > y) when there are none
vec2 CloudRoots(float a, float b, float c)
{
	if (a < 1e-9f)
	{
		if (abs(b) < 1e-9f)
		{
			return vec2(1.0f, -1.0f);
		}
		float t = -c / b;
		return b > 0.0f ? vec2(-1e9f, t) : vec2(t, 1e9f);
	}
	float disc = b * b - 4.0f * a * c;
	if (disc < 0.0f)
	{
		return vec2(1.0f, -1.0f);
	}
	float s = sqrt(disc);
	return vec2((-b - s) / (2.0f * a), (-b + s) / (2.0f * a));
}

// The part of a ray inside the layer, from the eye, up to a limit: x where it goes in, y where it comes out. None when
// x >= y. The world's curve brings every ray up into the layer far away.
vec2 CloudSpan(vec3 eye, vec3 dir, float limit)
{
	float a = dot(dir.xz, dir.xz) / (2.0f * u_cloudLayer.y);
	float b = dir.y;
	// The lowest that a rain cloud's ragged bottom hangs
	float bottom = u_cloudLayer.x - CLOUD_LOWEST_DROP;
	float top = u_cloudLayer.x + CLOUD_CUMULONIMBUS_DEPTH;
	// Under the top between these, over the bottom outside these
	vec2 underTop = CloudRoots(a, b, eye.y - top);
	vec2 underBottom = CloudRoots(a, b, eye.y - bottom);
	if (underTop.x >= underTop.y)
	{
		return vec2(1.0f, 0.0f);
	}
	float far = min(limit, u_cloudLayer.z);
	if (underBottom.x >= underBottom.y)
	{
		return vec2(max(underTop.x, 0.0f), min(underTop.y, far));
	}
	vec2 first = vec2(max(underTop.x, 0.0f), min(min(underTop.y, underBottom.x), far));
	if (first.x < first.y)
	{
		return first;
	}
	return vec2(max(max(underTop.x, underBottom.y), 0.0f), min(underTop.y, far));
}

// The optical depth from a point towards the light through the cloud, from a few samples that go farther apart
float CloudLightDepth(vec3 p, int samples)
{
	vec3 towards = u_cloudLight.xyz;
	float depth = 0.0f;
	float along = 0.0f;
	float step = 18.0f;
	for (int i = 0; i < 6; ++i)
	{
		if (i >= samples)
		{
			break;
		}
		along += step;
		vec3 at = p + towards * (along - step * 0.5f);
		depth += CloudDensity(at, CloudAltitude(at), false) * step;
		step *= 2.0f;
	}
	return depth * CLOUD_EXTINCTION;
}

// The light of the lightning inside a storm's cloud at a point: the flash of the sky lights all of the storm's cloud,
// and the bolt lights the cloud round the place that it struck
vec3 CloudLightning(vec3 p, float rain)
{
	vec3 core = vec3(u_cloudStrike.x, u_cloudLayer.x + 350.0f, u_cloudStrike.z);
	float near = exp(-length(p - core) / 450.0f);
	float glow = u_cloudAmbient.w * (0.08f + 0.3f * rain) + u_cloudStrike.w * near * 1.6f;
	return CLOUD_FLASH_COLOUR * glow;
}

// The clouds along a ray from the eye up to a limit: rgb the light that they scatter towards the eye, a how much of the
// light behind them goes through. `depth` is where the cloud is along the ray, weighted by how much each part shows,
// or the end of the ray when there is no cloud. `steps` is the most samples, `jitter` (0 to 1) moves the first one.
// `sky` is the light of the sky from above, `behind` the colour of the sky behind the ray, which the far clouds fade
// into.
vec4 MarchClouds(vec3 eye, vec3 dir, float limit, float jitter, int steps, bool detail, vec3 sky, vec3 behind,
                 out float depth)
{
	vec2 span = CloudSpan(eye, dir, limit);
	depth = min(limit, u_cloudLayer.z);
	if (span.x >= span.y)
	{
		return vec4(0.0f, 0.0f, 0.0f, 1.0f);
	}
	// The steps grow with the distance: small near the eye, where the shapes show, and large far away. A short span
	// keeps at least a few dozen steps.
	float spanLength = span.y - span.x;
	float growth = 6.4f / float(steps);
	float shortest = min(6.0f, spanLength / 24.0f);
	float longest = max(spanLength / 24.0f, 6.0f);
	float dt = clamp(span.x * growth, shortest, longest);
	float t = span.x + dt * jitter;
	float cosAngle = dot(dir, u_cloudLight.xyz);
	vec3 sun = u_cloudLightColour.rgb * u_cloudLightColour.w;
	// The light from below: the land and the sea give back a little of the sky's and the sun's light
	vec3 ground = sky * 0.5f + u_cloudAmbient.rgb * 0.3f + sun * 0.12f * max(u_cloudLight.y, 0.0f);
	vec3 colour = vec3_splat(0.0f);
	float transmittance = 1.0f;
	float weightedDepth = 0.0f;
	float weights = 0.0f;
	int lightSamples = detail ? 6 : 4;
	for (int i = 0; i < 160; ++i)
	{
		if (i >= steps || t > span.y || transmittance < 0.01f)
		{
			break;
		}
		vec3 p = eye + dir * t;
		float altitude = CloudAltitude(p);
		// The clouds thin out towards the farthest distance, so that the layer has no hard edge there
		float density = CloudDensity(p, altitude, detail) * (1.0f - smoothstep(u_cloudLayer.z * 0.35f, u_cloudLayer.z, t));
		if (density > 0.0f)
		{
			float sigma = density * CLOUD_EXTINCTION;
			float type;
			float rain;
			CloudWeatherAt(p.xz, type, rain);
			float h = clamp((altitude - u_cloudLayer.x) / CloudLayerDepth(type), 0.0f, 1.0f);
			// The sun's or the moon's light that comes through the cloud, scattered once and many times
			float lightDepth = CloudLightDepth(p, lightSamples);
			vec3 direct = sun * CloudMultipleScattering(lightDepth, cosAngle) * CloudPowder(sigma * 40.0f, cosAngle) * 32.0f;
			// The sky lights the tops more than the bottoms; the bottoms of rain clouds are dark
			// The game darkens its sky and its haze in a storm, so the storm's clouds take more of the dim light they
			// have, to stay grey and not black
			vec3 ambient = mix(ground, sky, h * 0.7f + 0.3f) * (1.0f + rain * (1.3f - 0.4f * (1.0f - h)));
			if (detail)
			{
				// The sky's light comes down through the cloud over the point: where the cloud is thinner, its
				// bottom is brighter
				float above = 0.0f;
				for (int k = 1; k <= 3; ++k)
				{
					vec3 up = p + vec3(0.0f, float(k * k) * 45.0f, 0.0f);
					above += CloudDensity(up, CloudAltitude(up), false) * float(k * 2 - 1) * 45.0f;
				}
				ambient *= 0.55f + 0.45f * exp(-above * CLOUD_EXTINCTION * 0.35f);
			}
			// The sun's light that a thick deck lets through after it scatters many times: grey and soft
			vec3 diffused = sun * 0.35f * exp(-lightDepth * 0.015f) * (0.5f + 0.5f * h);
			vec3 scattered = direct + ambient + diffused + CloudLightning(p, rain);
			float through = exp(-sigma * dt);
			// The light scattered over the step, as the step's own cloud dims it (energy-conserving)
			colour += transmittance * scattered * (1.0f - through);
			weightedDepth += t * transmittance * (1.0f - through);
			weights += transmittance * (1.0f - through);
			transmittance *= through;
		}
		t += dt;
		dt = clamp(t * growth, shortest, longest);
	}
	if (weights > 0.0f)
	{
		depth = weightedDepth / weights;
	}
	// The air between the eye and far clouds fades them into the colour of the sky
	float haze = 1.0f - exp(-depth / 9000.0f);
	colour = mix(colour, behind * (1.0f - transmittance), haze * 0.9f);
	return vec4(colour, transmittance);
}

// Where a point of the screen (0 to 1, as the textures are read) is in the world, at a depth of the depth buffer
vec3 CloudWorldAt(vec2 uv, float depth)
{
#if BGFX_SHADER_LANGUAGE_GLSL
	vec3 ndc = vec3(uv * 2.0f - 1.0f, depth * 2.0f - 1.0f);
#else
	vec3 ndc = vec3(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, depth);
#endif
	vec4 p = mul(u_invViewProj, vec4(ndc, 1.0f));
	return p.xyz / p.w;
}

// Where a point of the world is on the screen of a view-projection (xy, 0 to 1 as the textures are read); z is less
// than 0 behind the eye
vec3 CloudScreenAt(mat4 viewProj, vec3 world)
{
	vec4 clip = mul(viewProj, vec4(world, 1.0f));
	vec2 ndc = clip.xy / max(abs(clip.w), 1e-6f);
#if BGFX_SHADER_LANGUAGE_GLSL
	return vec3(ndc * 0.5f + 0.5f, clip.w);
#else
	return vec3(ndc.x * 0.5f + 0.5f, 0.5f - ndc.y * 0.5f, clip.w);
#endif
}

// The direction of the ray from the eye through a point of the screen
vec3 CloudRayAt(vec2 uv)
{
	return normalize(CloudWorldAt(uv, 0.5f) - u_cloudEye.xyz);
}

// How far the scene is from the eye at a depth of the depth buffer; very far where the screen shows the sky
float CloudSceneDistance(vec2 uv, float depth)
{
	if (depth <= 1e-7f)
	{
		return 1e9f;
	}
	return length(CloudWorldAt(uv, depth) - u_cloudEye.xyz);
}

// The sky's dome: xy its middle across the land; z how far across and w how high a point must be to be on it. The dome
// writes its depth, so a scene point on it is the sky.
uniform vec4 u_hazeDome;

bool CloudOnDome(vec3 dir, float reach)
{
	vec3 p = u_cloudEye.xyz + dir * reach;
	return length(p.xz - u_hazeDome.xy) > u_hazeDome.z || p.y > u_hazeDome.w;
}

// How far along a ray from the eye the lowest cloud can be: the ray meets the lowest bottom of the layer there, on the
// curved world. A scene nearer than this has no cloud in front of it. 0 when the eye is in or over the layer.
float CloudNearestAlong(vec3 dir)
{
	float lowest = u_cloudLayer.x - CLOUD_LOWEST_DROP - 60.0f;
	float below = lowest - u_cloudEye.y;
	if (below <= 0.0f)
	{
		return 0.0f;
	}
	// eye.y + t dir.y + t^2 (1 - dir.y^2) / (2 R) = lowest
	float a = max(1.0f - dir.y * dir.y, 1e-6f) / (2.0f * u_cloudLayer.y);
	return 2.0f * below / (dir.y + sqrt(max(dir.y * dir.y + 4.0f * a * below, 0.0f)));
}

#endif // CLOUDS_SH
