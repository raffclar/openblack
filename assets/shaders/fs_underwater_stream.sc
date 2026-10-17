$input v_texcoord0, v_texcoord1

#include <bgfx_shader.sh>

// The water reads the field of the wakes and the rings (see underwater_wake.sh)
#define UW_WAKE
#include "underwater.sh"
#include "underwater_streams.sh"

// EXPERIMENT (not for merge): the water in the rivers, drawn over the scene after the experiment's sea. The original
// game cuts each river's channel into the land's alpha, and its sea shows through. Here the water shows in the same
// channels, as much as the original shows its sea there. It flows down the river by the flow map: its small waves and
// its foam move with the water, two layers each, so that they never jump back. The water is clear and shallow, and the
// river's bed shows through it, bent by the waves. It is smooth and glassy in the pools, rough over the rocks and white
// where the river is steep. Where a river meets the sea, its water pushes a little foam out over the sea.

SAMPLER2D(s_sceneColour, 0);
SAMPLER2D(s_sceneDepth, 1);
// The land's alpha that the rivers' channels leave, over the whole land (see RenderPass::LandAlpha)
SAMPLER2D(s_landAlpha, 2);
// The land's block textures, a layer for each block: their alpha is the original's coast alpha
SAMPLER2DARRAY(s_blockTextures, 4);
// The layer of each block of the 32 by 32 map, plus 1; 0 where there is no block
SAMPLER2D(s_blockLayers, 5);
// The sky alone, for the reflection
SAMPLERCUBE(s_environmentSky, 7);

// rgb: the sky's colour, as the haze takes it; w: the near distance of the view
uniform vec4 u_uwSky;
// xy: the land's lowest corner across the ground; zw: 1 over the land's size, for the land's alpha
uniform vec4 u_streamLand;
// x: game units in a metre; y: the deepest water in a channel; z: the gravity; w: 1 when the sky's cube is bound
uniform vec4 u_streamParams;
// x: how many mouths there are; y: the least speed of the water; z: the most speed; w: unused
uniform vec4 u_streamInfo;
// For each mouth: xy where it is, zw the velocity of the water that leaves it
uniform vec4 u_streamMouths[8];
// The river's look (see underwater::streams::RiverProfile): [0] rgb how much of each colour the water takes out a
// metre, w how clear the water is; [1] rgb the colour that the water scatters back, w how strongly it reflects the sky
uniform vec4 u_streamProfile[2];
// x: how high the small waves are; y: how much the wind roughens the water
uniform vec4 u_streamProfileWaves;

// Where a point of the screen is in the world, at a depth of the depth buffer
vec3 StreamWorldAt(vec2 uv, float depth)
{
#if BGFX_SHADER_LANGUAGE_GLSL
	vec3 ndc = vec3(uv * 2.0f - 1.0f, depth * 2.0f - 1.0f);
#else
	vec3 ndc = vec3(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, depth);
#endif
	vec4 p = mul(u_invViewProj, vec4(ndc, 1.0f));
	return p.xyz / p.w;
}

// Where a point of the world is on the screen, 0 to 1 as the textures are read
vec2 StreamScreenAt(vec3 p)
{
	vec4 clip = mul(u_viewProj, vec4(p, 1.0f));
	vec2 ndc = clip.xy / clip.w;
#if BGFX_SHADER_LANGUAGE_GLSL
	return ndc * 0.5f + 0.5f;
#else
	return vec2(ndc.x * 0.5f + 0.5f, 0.5f - ndc.y * 0.5f);
#endif
}

// The land's alpha that the channels leave at a point
float LandAlphaAt(vec2 xz)
{
	vec2 uv = (xz - u_streamLand.xy) * u_streamLand.zw;
#if !BGFX_SHADER_LANGUAGE_GLSL
	uv.y = 1.0f - uv.y;
#endif
	return texture2DLod(s_landAlpha, uv, 0.0f).r;
}

// The original's coast alpha at a point: 1 where it shows land, less where its sea shows through (as the sea's pass
// reads it)
float CoastAlphaAt(vec2 xz)
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
	vec2 texel = (xz - block * 160.0f).yx * (256.0f / 160.0f);
	vec2 inside = clamp(texel, vec2_splat(0.5f), vec2_splat(255.5f));
	return texture2DArrayLod(s_blockTextures, vec3(inside / 256.0f, layer - 1.0f), 0.0f).a;
}

// Smooth noise with its slope: x the value from 0 to 1, yz its slope along x and z
vec3 NoiseWithSlope(vec2 p)
{
	vec2 i = floor(p);
	vec2 f = fract(p);
	vec2 u = f * f * f * (f * (f * 6.0f - 15.0f) + 10.0f);
	vec2 du = 30.0f * f * f * (f * (f - 2.0f) + 1.0f);
	float a = UwHash(i);
	float b = UwHash(i + vec2(1.0f, 0.0f));
	float c = UwHash(i + vec2(0.0f, 1.0f));
	float d = UwHash(i + vec2(1.0f, 1.0f));
	float k1 = b - a;
	float k2 = c - a;
	float k3 = a - b - c + d;
	return vec3(a + k1 * u.x + k2 * u.y + k3 * u.x * u.y, du * (vec2(k1, k2) + k3 * u.yx));
}

// The small waves of running water at a point: their height's slope. Three sizes, each turned against the last, so no
// square cells show. `size` is the length of the largest wave in units.
vec2 RippleSlope(vec2 p, float size)
{
	vec2 slope = vec2_splat(0.0f);
	float scale = 1.0f / size;
	float height = 1.0f;
	for (int i = 0; i < 3; i++)
	{
		vec2 q = vec2(p.x * 0.8f - p.y * 0.6f, p.x * 0.6f + p.y * 0.8f) * scale + float(i) * 7.31f;
		vec3 n = NoiseWithSlope(q);
		// The slope of the turned noise, turned back
		vec2 s = vec2(n.y * 0.8f + n.z * 0.6f, -n.y * 0.6f + n.z * 0.8f) * scale;
		slope += s * height;
		scale *= 2.03f;
		height *= 0.45f;
		p = vec2(p.y, -p.x);
	}
	return slope;
}

// The foam's pattern at a point, 0 to 1: lumps and threads
float FoamPattern(vec2 p)
{
	float n = UwFoamNoise(p);
	float threads = 1.0f - abs(UwNoise(p * 2.3f + 3.1f) * 2.0f - 1.0f);
	return n * 0.7f + threads * threads * 0.3f;
}

// The foam where a river pushes its water into the sea: streaks that move out from the mouth and fade. `onLevel` is
// where the ray from the eye meets the sea's level.
float MouthFoam(vec2 onLevel, float t)
{
	float foam = 0.0f;
	for (int i = 0; i < 8; i++)
	{
		if (float(i) >= u_streamInfo.x)
		{
			break;
		}
		vec4 mouth = u_streamMouths[i];
		float speed = length(mouth.zw);
		vec2 along = mouth.zw / max(speed, 1e-3f);
		vec2 away = onLevel - mouth.xy;
		float forward = dot(away, along);
		float side = dot(away, vec2(-along.y, along.x));
		// A fan of water that spreads as it goes out, about 12 units long; a slow river pushes out less
		float width = 1.2f + 0.25f * max(forward, 0.0f);
		float strength = clamp(speed / (1.2f * u_streamParams.x), 0.3f, 1.0f);
		float fan = exp(-side * side / (width * width)) * smoothstep(-2.0f, 1.0f, forward) * exp(-max(forward, 0.0f) / 6.0f) *
		            strength;
		if (fan < 0.01f)
		{
			continue;
		}
		// The streaks move out with the water, two layers so that the pattern never jumps back
		float cycle = 3.0f;
		float phase = fract(t / cycle);
		vec2 q = vec2(forward, side * 2.2f);
		float drift = max(speed, 0.3f) * cycle;
		float a = FoamPattern(q * 0.6f - vec2(phase * drift * 0.6f, 0.0f));
		float b = FoamPattern(q * 0.6f - vec2(fract(phase + 0.5f) * drift * 0.6f, 0.0f) + 4.7f);
		float w = 1.0f - abs(2.0f * phase - 1.0f);
		float pattern = mix(b, a, w);
		foam += fan * smoothstep(0.62f, 0.9f, pattern);
	}
	return clamp(foam, 0.0f, 1.0f);
}

void main()
{
	vec2 uv = gl_FragCoord.xy * u_viewTexel.xy;
	float depth = texture2DLod(s_sceneDepth, uv, 0.0f).r;
	vec3 p = StreamWorldAt(uv, depth);
	vec4 flow = UwStreamFlowAt(p.xz);
	float t = u_uwParams.y;
	vec3 eye = u_uwEye.xyz;
	vec3 toPoint = p - eye;
	vec3 view = normalize(toPoint);
	vec3 toLight = normalize(u_uwAbove.xyz);
	vec3 light = u_uwLightColour.rgb * max(toLight.y, 0.0f) + u_uwSky.rgb * 0.55f;
	vec4 scene = texture2DLod(s_sceneColour, uv, 0.0f);
	bool land = abs(scene.a - UW_LAND_MARK) < 0.5f / 255.0f;

	// Where a river meets the sea: foam on the sea's level, over the sea bed that shows under it
	if (u_streamInfo.x > 0.5f && p.y < u_uwParams.z - 0.05f && view.y < -0.01f)
	{
		float along = (u_uwParams.z - eye.y) / view.y;
		vec3 onLevel = eye + view * along;
		float foam = UwStreamFlowAt(onLevel.xz).w < 16.0f ? MouthFoam(onLevel.xz, t) * 0.35f : 0.0f;
		if (foam > 0.004f)
		{
			vec3 colour = vec3(0.92f, 0.95f, 0.95f) * light;
			gl_FragColor = vec4(colour * foam, foam);
			return;
		}
	}
	if (!land || flow.w > 4.0f)
	{
		discard;
	}
	// The water shows in the original's channels, where they leave less of the land's alpha. Where the original shows
	// its sea through its coast alpha, the sea's pass draws the water.
	// A little noise on the alpha breaks up the steps of its texels along the bank
	float channelAlpha = LandAlphaAt(p.xz) + (UwNoise(p.xz * 2.7f) - 0.5f) * 0.07f;
	float share = (1.0f - smoothstep(0.80f, 0.97f, channelAlpha)) * CoastAlphaAt(p.xz);
	if (share < 0.004f)
	{
		discard;
	}
	float metre = u_streamParams.x;
	// The water is deepest in the middle of the channel, and faster there than at the banks
	float middle = clamp((1.0f - channelAlpha) * 3.0f, 0.0f, 1.0f);
	float waterDepth = u_streamParams.y * sqrt(middle);
	vec2 velocity = flow.xy * (0.35f + 0.65f * middle);
	float speed = length(velocity);
	float fast = smoothstep(0.25f * metre, 1.6f * metre, speed);
	vec2 along = speed > 1e-4f ? velocity / speed : vec2(1.0f, 0.0f);
	vec2 across = vec2(-along.y, along.x);

	// The small waves move with the water. Two layers each start again in turn, half a cycle apart, and each fades out
	// before it starts again; a noise offsets the cycle from place to place, so that no pulse shows.
	float cycle = 1.1f;
	float offset = UwNoise(p.xz * 0.21f);
	float phase0 = fract(t / cycle + offset);
	float phase1 = fract(phase0 + 0.5f);
	float weight0 = 1.0f - abs(2.0f * phase0 - 1.0f);
	vec2 shift0 = velocity * (phase0 - 0.5f) * cycle;
	vec2 shift1 = velocity * (phase1 - 0.5f) * cycle;
	float rippleSize = 0.55f * metre;
	vec2 ripple = mix(RippleSlope(p.xz - shift1 + vec2(3.7f, 1.9f), rippleSize), RippleSlope(p.xz - shift0, rippleSize),
	                  weight0);
	// The waves stretch across the flow in fast water, and the pools are almost glassy
	float roughness = mix(0.06f, 0.6f, fast);
	vec2 slope = ripple * roughness * 0.45f * u_streamProfileWaves.x;
	// The wind roughens the water with small waves that run with it, most on the slow pools, where the river's own
	// waves are small. The wind comes from the sea's waves: its direction, and more in a storm.
	vec2 wind = vec2(cos(u_uwWaves.z), sin(u_uwWaves.z));
	float windy = u_streamProfileWaves.y * (0.25f + 0.75f * u_uwWaves.y) * (1.0f - 0.7f * fast);
	vec2 gust = RippleSlope(p.xz - wind * t * 0.9f * metre + vec2(11.3f, 5.1f), 0.25f * metre);
	slope += gust * windy * 0.12f;

	// Over the rocks in the bed the fast water stands in waves that do not move: a hump over each rock, and a few
	// waves behind it. Their foam is white.
	vec2 rockAt = vec2(dot(p.xz, along) * 0.55f, dot(p.xz, across) * 1.1f) / metre;
	vec3 rock = NoiseWithSlope(p.xz * 0.9f / metre + 17.3f);
	float rocks = smoothstep(0.62f, 0.86f, rock.x) * fast;
	// The foam is on the side of a rock that the water pushes into, not on the side that it leaves
	vec2 rockSlope = rock.yz / max(length(rock.yz), 1e-4f);
	float rockPush = rocks * clamp(dot(along, rockSlope) * 0.7f + 0.3f, 0.0f, 1.0f);
	float standing = sin(rockAt.x * 6.2831853f * 1.4f + rock.x * 5.0f);
	slope += rock.yz * 0.9f / metre * rocks * 0.25f + along * standing * rocks * 0.08f;

	// The rings and wakes in the water's field of small waves
	vec4 wake = UwWakeAt(p.xz);
	slope += wake.zw;

	// Waves steeper than this are broken water, which the foam covers
	slope *= min(1.0f, 0.6f / max(length(slope), 1e-4f));
	vec3 normal = normalize(vec3(-slope.x, 1.0f, -slope.y));

	// The river's bed through the water, bent by the waves: the land's own colour where the bent ray meets the bed
	vec3 refracted = refract(view, normal, 1.0f / 1.33f);
	float path = waterDepth / max(-refracted.y, 0.25f);
	// The bed is seen through the bent surface: the waves move it about by more than the shallow depth alone gives
	vec3 bedPoint = p + refracted * path + vec3(slope.x, 0.0f, slope.y) * waterDepth * 1.5f;
	vec2 bedUv = StreamScreenAt(bedPoint);
	vec4 bedSample = texture2DLod(s_sceneColour, clamp(bedUv, vec2_splat(0.0f), vec2_splat(1.0f)), 0.0f);
	vec3 bed = abs(bedSample.a - UW_LAND_MARK) < 0.5f / 255.0f ? bedSample.rgb : scene.rgb;
	// The light that the waves focus on the bed
	float caustic = UwCaustics(bedPoint.xz * 1.3f / metre - velocity * t * 0.3f, t * 1.6f, 0.8f);
	bed *= 0.85f + 0.45f * caustic * u_uwAbove.w * (1.0f - fast * 0.5f) * smoothstep(0.0f, 0.3f, toLight.y);
	// Fresh water takes out the red first, and a little light scatters back from the water itself
	vec3 absorb = u_streamProfile[0].rgb / metre;
	vec3 transmit = exp(-absorb * path * 2.0f) * u_streamProfile[0].w;
	vec3 scatter = u_streamProfile[1].rgb * light * (0.6f + 0.8f * middle);
	vec3 below = bed * transmit + scatter * (1.0f - transmit);

	// The sky in the water, more at low angles
	vec3 reflected = reflect(view, normal);
	reflected.y = abs(reflected.y);
	vec3 sky = u_streamParams.w > 0.5f ? textureCubeLod(s_environmentSky, reflected, 0.0f).rgb : u_uwSky.rgb * 1.1f;
	// The banks, the grass and the rock beside the water hide the sky low down: a ray reflected near the level shows
	// the dark bank, so the waves show as bright and dark lines
	vec3 banks = scene.rgb * 0.55f;
	sky = mix(banks, sky, smoothstep(0.08f, 0.55f, reflected.y));
	float cosine = clamp(dot(-view, normal), 0.0f, 1.0f);
	float fresnel = 0.02f + 0.98f * pow(1.0f - cosine, 5.0f);
	// Small waves too fine for a pixel to show still reflect the sky, more on rough water
	float far = smoothstep(40.0f, 260.0f, length(toPoint));
	float reflection = max(fresnel, 0.06f + 0.12f * fast + 0.25f * far) * u_streamProfile[1].w;
	vec3 colour = mix(below, sky, reflection);
	// The sun's glint: sharp on the glassy pools, wide and broken on the rough water
	float shine = mix(600.0f, 60.0f, fast);
	float glint = pow(max(dot(reflected, toLight), 0.0f), shine) * mix(6.0f, 2.0f, fast);
	colour += u_uwLightColour.rgb * glint * u_uwLightColour.w;

	// The white water: where the river is steep, over the rocks, at the banks of fast water, and in the field's foam.
	// Its pattern moves with the water, in two layers as the waves do.
	float foamCycle = 1.6f;
	float foamPhase0 = fract(t / foamCycle + offset);
	float foamPhase1 = fract(foamPhase0 + 0.5f);
	float foamWeight = 1.0f - abs(2.0f * foamPhase0 - 1.0f);
	vec2 foamUv = vec2(dot(p.xz, along), dot(p.xz, across) * 1.8f) * 1.6f / metre;
	float foamSpeed = speed * 1.6f / metre * foamCycle;
	float pattern = mix(FoamPattern(foamUv - vec2((foamPhase1 - 0.5f) * foamSpeed, 0.0f) + 2.3f),
	                    FoamPattern(foamUv - vec2((foamPhase0 - 0.5f) * foamSpeed, 0.0f)), foamWeight);
	// Where the water pushes into the bank (at the outside of a bend) it makes foam; where it runs along the bank or
	// away from it, none. The distance to the river's line grows towards the bank.
	vec2 step_ = vec2(1.0f, 0.0f);
	vec2 bankward = vec2(UwStreamFlowAt(p.xz + step_.xy).w - UwStreamFlowAt(p.xz - step_.xy).w,
	                     UwStreamFlowAt(p.xz + step_.yx).w - UwStreamFlowAt(p.xz - step_.yx).w);
	bankward /= max(length(bankward), 1e-4f);
	float push = clamp(dot(velocity, bankward) / max(speed, 1e-3f), 0.0f, 1.0f);
	float bank = (1.0f - middle) * fast * (0.15f + 0.85f * push) * 0.7f;
	// A steep river falls in steps: short white cascades with smoother runs between them, which do not move
	float steps = smoothstep(0.3f, 0.7f, UwNoise(p.xz * 0.17f / metre + 3.3f));
	float white = clamp(flow.z * (0.6f + 0.4f * fast) * (0.25f + 0.75f * steps) + rockPush * 0.8f + bank + wake.y, 0.0f,
	                    1.0f);
	// Even the whitest water breaks into lumps and threads, with darker water between them
	float foam = smoothstep(0.9f - 0.7f * white, 1.15f - 0.7f * white, pattern) * smoothstep(0.02f, 0.15f, white);
	vec3 foamLight = u_uwSky.rgb * 1.5f + u_uwLightColour.rgb * (0.3f + 0.7f * max(toLight.y, 0.0f));
	vec3 foamColour = min(vec3(0.93f, 0.96f, 0.96f) * foamLight, vec3_splat(1.3f));
	// Thick foam is brighter than thin foam, and the sun lights the tops of its lumps
	foamColour *= 0.7f + 0.4f * smoothstep(0.3f, 0.9f, pattern);
	colour = mix(colour, foamColour, foam * 0.9f);

	// The effect that draws the water as a flat colour shows the river's flow instead: red for the white water, green
	// for the speed, blue for the depth
	if (u_uwFlags2.w > 0.5f)
	{
		colour = vec3(white, speed / u_streamInfo.z, waterDepth / u_streamParams.y);
	}

	// The edge of the water is soft, and a thin bright line shows where the water meets the bank
	float edge = smoothstep(0.0f, 0.7f, share);
	gl_FragColor = vec4(colour * edge, edge);
}
