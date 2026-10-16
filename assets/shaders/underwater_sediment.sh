#ifndef UNDERWATER_SEDIMENT_SH
#define UNDERWATER_SEDIMENT_SH

// EXPERIMENT (not for merge): the silt and the sand that the water holds. The waves stir it up from the sea bed: there
// is more near the bed in shallow water, where the waves break and in a storm, and less in the deep clear water. It
// gives the water its colour and takes the view away sooner. It moves with the currents.

// x: 1 while the sediment shows; y: how strongly the waves stir the sea bed, 0 to about 2; zw: unused
uniform vec4 u_uwSediment;
// The spray's noise volume (see underwater::SprayNoiseVolume), for the clouds of silt
SAMPLER3D(s_sedimentNoise, 9);

// The colour of the silt and the sand
#define UW_SILT vec3(0.58f, 0.52f, 0.38f)
// How much the silt takes out of the light for each unit, at the most silt
#define UW_SILT_EXTINCTION 0.11f

// How much silt the water holds at a point, 0 to about 2. `shore` is the shore map there, and `drifted` the point
// moved back by the current, so that the clouds of silt move with the water.
float UwSedimentAt(vec3 p, vec4 shore, vec2 drifted)
{
	float column = max(u_uwParams.z - shore.w, 0.0f);
	float above = max(p.y - shore.w, 0.0f);
	float storm = u_uwWaves.y;
	float shallow = 1.0f - smoothstep(1.5f, 24.0f, column);
	// The waves lift a layer of silt off the bed: thicker in a storm and where the water is deeper
	float layer = 0.6f + 2.0f * storm + 0.12f * column;
	float bed = exp(-above / layer) * (0.2f + 0.8f * shallow) * (0.5f + 0.9f * storm);
	// Where the waves break, the silt fills the water from the bed to the surface
	float surf = (1.0f - smoothstep(3.0f, 30.0f + 20.0f * storm, shore.x)) * shallow * u_uwSediment.y;
	// A storm clouds all of the water that is not deep
	float clouded = storm * 0.35f * (1.0f - smoothstep(10.0f, 45.0f, column));
	// Clouds of silt, carried by the current, that rise and fall a little
	vec3 q = vec3(drifted.x * 0.022f, p.y * 0.06f + u_uwParams.y * 0.01f, drifted.y * 0.022f);
	float clouds = 0.35f + 1.3f * texture3DLod(s_sedimentNoise, q, 0.0f).r;
	return (bed + surf + clouded) * clouds;
}

// How far the current moved the water near a point, for the clouds of silt
vec2 UwSedimentDrift(vec3 p, float t)
{
	vec2 shift;
	UwCurrent(p, t, shift);
	return shift + u_uwCurrent.xy * t;
}

// The light that the silt adds along a ray, and how much it takes out of what is behind. The ray goes from `start`
// along `direction` for `range`, under the water. rgb: the light of the silt towards the eye; a: the share of what is
// behind that is left. `mean` is how much silt the ray goes through, on average.
vec4 UwSedimentAlong(vec3 start, vec3 direction, float range, float t, float jitter, int steps, out float mean)
{
	mean = 0.0f;
	if (u_uwSediment.x < 0.5f)
	{
		return vec4(0.0f, 0.0f, 0.0f, 1.0f);
	}
	float reach = min(range, 80.0f);
	float stepLength = reach / float(steps);
	vec2 drift = UwSedimentDrift(start, t);
	vec3 light = u_uwLightColour.rgb * u_uwLightColour.w;
	vec3 added = vec3_splat(0.0f);
	float left = 1.0f;
	float count = 0.0f;
	for (int i = 0; i < steps; i++)
	{
		vec3 p = start + direction * ((float(i) + jitter) * stepLength);
		float depth = u_uwParams.z - p.y;
		if (depth > 0.0f)
		{
			vec4 shore = texture2DLod(s_shore, UwShoreUv(p.xz), 0.0f);
			float silt = UwSedimentAt(p, shore, p.xz - drift);
			// The silt is lit by the light that comes down to its depth and by the water round it. It sends only a small
			// part of the light that falls on it towards the eye.
			vec3 seen = UW_SILT * (light * UwTransmittance(depth / max(u_uwLight.y, 0.2f)) * 0.12f + u_uwScatter.rgb * 0.8f);
			float taken = 1.0f - exp(-silt * UW_SILT_EXTINCTION * stepLength);
			added += seen * taken * left;
			left *= 1.0f - taken;
			mean += silt;
			count += 1.0f;
		}
	}
	mean /= max(count, 1.0f);
	return vec4(added, left);
}

// The shafts of light in water that holds silt: the silt gives them its colour, and they are shorter
vec3 UwSedimentShafts(vec3 shafts, float mean, float left)
{
	vec3 hue = UW_SILT / dot(UW_SILT, vec3(0.299f, 0.587f, 0.114f));
	vec3 tint = mix(vec3_splat(1.0f), hue, min(mean, 1.0f) * 0.8f);
	return shafts * tint * mix(1.0f, left, 0.6f);
}

#endif // UNDERWATER_SEDIMENT_SH
