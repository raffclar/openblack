$input v_texcoord0, v_texcoord1

#include <bgfx_shader.sh>

#include "underwater.sh"
#include "underwater_foam.sh"

// EXPERIMENT (not for merge): one step of the foam field. The current carries the foam that was there, a little of it
// spreads to the texels round it, and it decays. New foam builds up where the shore wave breaks, at the edge of the
// run up the beach, at the foot of the cliffs (a band that never clears and a push of foam at each crash) and on the
// crests of a storm. Fresh foam holds much air, which leaves it soon.

SAMPLER2D(s_foamPrevious, 0);
SAMPLER2D(s_shore, 2);

// x: the time that the step covers, in seconds; y: 1 to start the field again with no foam; zw: the corner of the
// field before the step
uniform vec4 u_uwFoamStep;

// The foam of the field before the step at a point, or none outside the field then
vec4 FoamBefore(vec2 xz)
{
	vec2 uv = (xz - u_uwFoamStep.zw) / u_uwFoam.w;
	if (uv.x < 0.0f || uv.y < 0.0f || uv.x > 1.0f || uv.y > 1.0f)
	{
		return vec4_splat(0.0f);
	}
	return texture2DLod(s_foamPrevious, uv, 0.0f);
}

void main()
{
	float texel = u_uwFoam.w * u_viewTexel.x;
	vec2 p = u_uwFoam.xy + gl_FragCoord.xy * texel;
	float dt = u_uwFoamStep.x;
	float t = u_uwParams.y;
	float level = u_uwParams.z;
	float storm = u_uwWaves.y;

	// The foam that the current carries here: it was upstream of this point before the step
	vec2 shift;
	vec2 flow = UwCurrent(vec3(p.x, level, p.y), t, shift);
	vec2 from = p - flow * dt;
	vec4 before = FoamBefore(from) * 0.84f + (FoamBefore(from + vec2(texel, 0.0f)) + FoamBefore(from - vec2(texel, 0.0f)) +
	                                          FoamBefore(from + vec2(0.0f, texel)) + FoamBefore(from - vec2(0.0f, texel))) *
	                                             0.04f;
	if (u_uwFoamStep.y > 0.5f)
	{
		before = vec4_splat(0.0f);
	}

	// Where the waves make new foam
	vec4 shore = UwOpenSea(texture2DLod(s_shore, UwShoreUv(p), 0.0f), p);
	vec2 slope;
	float face;
	// The waves as a texel of the field shows them: a round pixel of 1.5 units
	float height = UwSurface(p, t, 1000.0f, shore, slope, face);
	float waterDepth = height - shore.w;
	float wet = smoothstep(-0.05f, 0.05f, waterDepth);
	float cliff = UwShoreCliff(shore);
	// The face of the shore wave as it breaks, and the edge of the water as it runs up the beach
	float breaking = face * (1.0f - smoothstep(1.0f, 12.0f, shore.x)) * smoothstep(0.0f, 0.3f, waterDepth);
	float edge = (1.0f - smoothstep(0.0f, 0.06f, waterDepth)) * wet * (1.0f - cliff) * (1.0f - smoothstep(2.0f, 8.0f, abs(shore.x)));
	// At the foot of a cliff: a band of foam that never clears, and a push of foam from each crash, at the time and the
	// place where the crash drives its cloud of air under the water
	float since;
	float crash;
	float period;
	float strength = UwCliffCrash(p, t, since, crash, period);
	float band = cliff * (1.0f - smoothstep(1.5f, 6.0f, shore.x));
	float reach = 4.0f + since * (3.0f + 3.0f * storm);
	float push = cliff * (strength / 0.55f) * exp(-since * 1.2f) * (1.0f - smoothstep(reach - 6.0f, reach, shore.x)) *
	             (0.7f + 0.3f * u_uwWaves.w / 0.25f);
	// The crests of a storm break into whitecaps
	float crest = (height - level) / max(UwWaveTop(), 1e-3f);
	float caps = storm * smoothstep(0.72f, 0.98f, crest) * smoothstep(0.6f, 0.85f, UwNoise(p * 0.7f + t * 0.4f)) *
	             smoothstep(20.0f, 40.0f, shore.x);
	// A little more than half of the foam that the field made at first; none on a pond
	float made = (5.0f * breaking + 1.0f * edge + 3.0f * band + 6.0f * push + 3.0f * caps) * wet * 0.55f *
	             (1.0f - UwShorePond(shore));
	float air = 3.0f * breaking + 1.0f * band + 5.0f * push + 2.0f * caps;

	// The foam decays: slowly at the cliffs and in the shallows, sooner on the open sea, and soon on dry sand
	float lasts = mix(mix(4.0f, 6.0f, 1.0f - smoothstep(0.5f, 3.0f, waterDepth)), 10.0f, cliff);
	lasts = mix(0.8f, lasts, wet);
	float kept = exp(-dt / lasts);
	float thick = before.x * kept;
	float added = made * dt;
	float total = min(thick + added, 2.0f);
	// The age of the foam is the age of the foam that was there, made younger by the new foam
	float age = min((before.y + dt) * thick / max(thick + added, 1e-4f), 60.0f);
	float held = min(before.z * exp(-dt / 1.2f) + air * dt, min(total, 2.0f));
	gl_FragColor = vec4(total, age, held, 1.0f);
}
