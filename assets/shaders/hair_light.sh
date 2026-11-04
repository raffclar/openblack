#ifndef HAIR_LIGHT_SH
#define HAIR_LIGHT_SH

// The light and the added colour of a creature's hair, 0 to 255. Include haze.sh and land_light.sh before this file.

// xyz: where the creature stands, which its hair takes the land's light and haze at
uniform vec4 u_hairOrigin;
// The colours of the creature's own look, 0 to 255, in place of the land's light and the haze: xyz the light (w 1 when
// the look is used, 2 to use the colours that s_hairKept keeps), and xyz the colour added after it
uniform vec4 u_hairLight;
uniform vec4 u_hairAdded;

// The light and the added colour of the hair at the creature's place. The depth is the place's depth in front of the
// camera that draws the hair. The light is the land's light under the creature, faded by the haze. The added colour is
// the haze's colour and the land's colour, at most white.
void HairLightAt(vec3 origin, float depth, out vec3 light, out vec3 added)
{
	light = u_hairLight.xyz;
	added = u_hairAdded.xyz;
	if (u_hairLight.w < 0.5f)
	{
		light = vec3_splat(255.0f);
		if (u_landLight.x > 0.0f)
		{
			light = min(floor(LandLightAt(origin.xz) * u_landLight.y), vec3_splat(255.0f));
		}
		float hazeT = HazeT(depth);
		light = HazeDiffuse(light, HazeFactor(hazeT));
		added = HazeColour(hazeT);
		if (u_landLight.x > 0.0f)
		{
			added = min(added + LandColourAt(origin.xz), vec3_splat(255.0f));
		}
	}
}

#endif // HAIR_LIGHT_SH
