$input v_texcoord0, v_texcoord1

#include <bgfx_shader.sh>

#include "underwater.sh"

// EXPERIMENT (not for merge): one step of the field of small waves round the camera, that the things in the water write
// into. Each texel keeps the water's height now (x), its height a step before (y) and its foam (z). The waves move by
// a kernel that makes the long waves faster than the short waves, as on deep water, so a moving thing leaves a V-shaped
// wake behind it. The waves stop at the coast and die away at the field's edges.

// The field of the last step
SAMPLER2D(s_wakePrev, 0);
// The shore map (see UwShoreUv): its w is the height of the ground
SAMPLER2D(s_shore, 2);

// xy: the low corner of this field across the ground; z: the space between two texels; w: the side in texels
uniform vec4 u_wakeField;
// xy: how many texels the field moved since the last step; z: 1 when the field starts again from flat; w: how much the
// height pulls back towards flat each step
uniform vec4 u_wakeStep;
// x: the damping times the step's time; y: the share of the foam that is left after a step; z: the step's time in
// seconds; w: unused
uniform vec4 u_wakeDamp;
// The kernel's weights: for each distance along z (0 to 6) two vec4, the weights for the distances along x 0 to 3
// and 4 to 6
uniform vec4 u_wakeKernel[14];
// x: how many movers, y: how many rings
uniform vec4 u_wakeCounts;
// For each mover two: (where it is across the ground, its radius, how far it pushes the water down) and (its velocity
// across the ground, the foam it makes a second, unused)
uniform vec4 u_wakeMovers[32];
// For each ring that starts: where across the ground, its radius, the depth of its hole
uniform vec4 u_wakeRings[16];

// The last step's field at a texel of it
vec4 Last(vec2 texel)
{
	return texture2DLod(s_wakePrev, (texel + 0.5f) / u_wakeField.w, 0.0f);
}

float Height(vec2 texel)
{
	return Last(texel).x;
}

// One row of the kernel: the weights for the distances along x from 0 to 6, each used on both sides
float KernelRow(vec2 middle, vec4 a, vec4 b)
{
	float sum = a.x * Height(middle);
	sum += a.y * (Height(middle + vec2(1.0f, 0.0f)) + Height(middle - vec2(1.0f, 0.0f)));
	sum += a.z * (Height(middle + vec2(2.0f, 0.0f)) + Height(middle - vec2(2.0f, 0.0f)));
	sum += a.w * (Height(middle + vec2(3.0f, 0.0f)) + Height(middle - vec2(3.0f, 0.0f)));
	sum += b.x * (Height(middle + vec2(4.0f, 0.0f)) + Height(middle - vec2(4.0f, 0.0f)));
	sum += b.y * (Height(middle + vec2(5.0f, 0.0f)) + Height(middle - vec2(5.0f, 0.0f)));
	sum += b.z * (Height(middle + vec2(6.0f, 0.0f)) + Height(middle - vec2(6.0f, 0.0f)));
	return sum;
}

void main()
{
	vec2 texel = floor(gl_FragCoord.xy);
	vec2 world = u_wakeField.xy + (texel + 0.5f) * u_wakeField.z;
	// Where this texel was in the last step's field
	vec2 source = texel + u_wakeStep.xy;
	float fresh = u_wakeStep.z > 0.5f ? 0.0f : 1.0f;
	vec2 inside = step(vec2_splat(0.0f), source) * step(source, vec2_splat(u_wakeField.w - 1.0f));
	fresh *= inside.x * inside.y;

	vec4 last = Last(source) * fresh;
	float height = last.x;
	float before = last.y;

	// The pull back towards flat, by the kernel over the texels round this one
	float pull = KernelRow(source, u_wakeKernel[0], u_wakeKernel[1]);
	for (int row = 1; row <= 6; row++)
	{
		vec2 along = vec2(0.0f, float(row));
		pull += KernelRow(source + along, u_wakeKernel[row * 2], u_wakeKernel[row * 2 + 1]);
		pull += KernelRow(source - along, u_wakeKernel[row * 2], u_wakeKernel[row * 2 + 1]);
	}
	pull *= fresh;
	// In shallow water the waves are slower, as tanh(k h) of the depth h for a wave of the reference length
	vec4 shore = texture2DLod(s_shore, UwShoreUv(world), 0.0f);
	float water = max(u_uwParams.z - shore.w, 0.0f);
	float kh = 6.2831853f / (16.0f * u_wakeField.z) * water;
	float shallow = clamp((exp(2.0f * kh) - 1.0f) / (exp(2.0f * kh) + 1.0f), 0.1f, 1.0f);
	float damping = u_wakeDamp.x;
	float next = (height * (2.0f - damping) - before - u_wakeStep.w * shallow * pull) / (1.0f + damping);

	// The foam: it spreads a little to the texels next to it, and it fades
	vec4 east = Last(source + vec2(1.0f, 0.0f)) * fresh;
	vec4 west = Last(source - vec2(1.0f, 0.0f)) * fresh;
	vec4 north = Last(source + vec2(0.0f, 1.0f)) * fresh;
	vec4 south = Last(source - vec2(0.0f, 1.0f)) * fresh;
	// A little smoothing of the height takes out the waves as short as two texels, which the kernel does not move
	next += 0.05f * (east.x + west.x + north.x + south.x - 4.0f * height);
	float foam = mix(last.z, (east.z + west.z + north.z + south.z) * 0.25f, 0.2f) * u_wakeDamp.y;
	// Steep water and sharp crests make foam: the arms of a wake and the first ring of a splash
	float spacing = u_wakeField.z;
	vec2 slope = vec2(east.x - west.x, north.x - south.x) / (2.0f * spacing);
	float crest = -(east.x + west.x + north.x + south.x - 4.0f * height) / (spacing * spacing);
	foam += (max(length(slope) - 0.08f, 0.0f) * 3.0f + max(crest - 0.4f, 0.0f) * 0.08f) * u_wakeDamp.z * 6.0f;

	// The things that move in the water hold the water down under them, so the water flows round them as they move
	for (int i = 0; i < 16; i++)
	{
		if (float(i) >= u_wakeCounts.x)
		{
			break;
		}
		vec4 a = u_wakeMovers[i * 2];
		vec4 b = u_wakeMovers[i * 2 + 1];
		vec2 away = world - a.xy;
		float d = length(away);
		float r = max(a.z, 0.3f);
		float core = exp(-2.0f * (d / r) * (d / r));
		next = mix(next, -a.w * core, core * 0.35f);
		// The foam round the thing, more behind it than in front of it
		float speed = length(b.xy);
		float behind = speed > 1e-3f ? clamp(-dot(away, b.xy) / max(d * speed, 1e-3f), 0.0f, 1.0f) : 0.5f;
		float fromRim = (d - r * 0.9f) / (0.15f * r + 0.3f);
		float rim = exp(-fromRim * fromRim);
		foam += b.z * (0.2f + 0.8f * behind) * (rim + core * 0.3f) * u_wakeDamp.z * 0.8f;
	}
	// The rings that start this step: a hole in the water where a thing fell in, with foam for a splash
	for (int j = 0; j < 16; j++)
	{
		if (float(j) >= u_wakeCounts.y)
		{
			break;
		}
		vec4 ring = u_wakeRings[j];
		float d = length(world - ring.xy);
		float r = max(ring.z, 0.3f);
		float hole = exp(-2.0f * (d / r) * (d / r));
		next -= ring.w * hole;
		float spread = d / (r * 1.6f);
		foam += clamp(ring.w, 0.0f, 0.6f) * exp(-spread * spread);
	}

	// The waves stop at the coast, and die away near the edges of the field so that they do not come back from them
	float land = smoothstep(u_uwParams.z - 0.25f, u_uwParams.z + 0.05f, shore.w);
	vec2 fromEdge = min(texel, u_wakeField.w - 1.0f - texel);
	float edge = smoothstep(0.0f, 40.0f, min(fromEdge.x, fromEdge.y));
	next *= (1.0f - land) * mix(0.9f, 1.0f, edge);
	foam *= mix(0.6f, 1.0f, edge);
	gl_FragColor = vec4(clamp(next, -4.0f, 4.0f), height * (1.0f - land), clamp(foam, 0.0f, 1.0f), 0.0f);
}
