#ifndef CELESTIAL_SH
#define CELESTIAL_SH

// EXPERIMENT (not for merge): the frame of the sky's sphere (atmosphere::CelestialFrame). The stars, the Milky Way and
// the nebulae have fixed places in this frame. The frame turns with the hour, so the sky turns with the sun's path.

// The rows of the turn from the frame that the camera works in to the sky's frame: x towards a right ascension of 0, y
// towards a right ascension of 6 hours and z towards the north pole of the sky
uniform vec4 u_celestial[3];

// A direction in the camera's frame as a direction on the sky's sphere
vec3 CelestialDirection(vec3 dir)
{
	return vec3(dot(u_celestial[0].xyz, dir), dot(u_celestial[1].xyz, dir), dot(u_celestial[2].xyz, dir));
}

#endif // CELESTIAL_SH
