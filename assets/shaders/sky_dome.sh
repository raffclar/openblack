#ifndef OPENBLACK_SKY_DOME_SH
#define OPENBLACK_SKY_DOME_SH

// EXPERIMENT (not for merge): the sky's dome writes its depth, so the passes over the scene must know a point on it
// from a point of the land. The dome is near a half ball round the land's middle: xy its middle across the land, z its
// half width and w its height. A point near its skin is on the dome. A test of the width and the height alone is not
// correct: high over an eye away from the middle the dome is lower than its top and nearer than its width, and the
// passes then put the haze of a far scene on the sky.
uniform vec4 u_hazeDome;

bool OnSkyDome(vec3 p)
{
	if (u_hazeDome.z <= 0.0f)
	{
		return false;
	}
	vec3 scaled = vec3((p.x - u_hazeDome.x) / u_hazeDome.z, p.y / u_hazeDome.w, (p.z - u_hazeDome.y) / u_hazeDome.z);
	return dot(scaled, scaled) > 0.75f * 0.75f;
}

#endif // OPENBLACK_SKY_DOME_SH
