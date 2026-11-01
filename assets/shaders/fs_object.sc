$input v_position, v_texcoord0, v_normal, v_color0, v_haze, v_snow, v_snowLight

#include <bgfx_shader.sh>

#include "snow_object.sh"

// The creatures' and the hand's moving shadows' data (see ground_shadow.sh)
SAMPLER2D(s_groundShadowData, 14);
#define GROUND_SHADOW_DATA s_groundShadowData
#include "ground_shadow.sh"

SAMPLER2D(s_diffuse, 0);
#ifdef USE_ENVIRONMENT
SAMPLER2D(s_environment, 5);
// x: how the environment map is combined with the colour: 0 not at all, 1 in its place, 2 left as it was, 3 multiplied,
// 4 multiplied and doubled, 5 added
uniform vec4 u_environmentStage;
#endif // USE_ENVIRONMENT
// y: the alpha threshold, of 1. z: 0 for a primitive that is not see-through. A see-through primitive gives 1 for no
// alpha test, 2 for a test against the reference w of 255, and 3 for a test against w lowered by the instance's alpha.
uniform vec4 u_skyAlphaThreshold;
// The temple's controls glow under the cursor: the game adds the colour as the vertices' specular, after the texture
// stages
uniform vec4 u_glow;
// rgb: a colour the object is drawn in, as the game gives it. w: 0 to light it as usual, otherwise to draw
// it unlit, in that colour alone, with its alpha by w
uniform vec4 u_tint;
// x: 1 to show only what stands above the height y: in the sea's reflection, which shows only what stands above the sea,
// and for a mesh cut by a plane. z: 1 to blend by the instance's alpha rather than dissolve the share of it gone. w: 1 to
// multiply the texture's alpha by the instance's before the alpha test, as a material that the game changed does.
uniform vec4 u_seaClip;
// w: the whole object's alpha, which its texture's is multiplied by; 0 for an object drawn as its materials say
uniform vec4 u_objectLook;
// x: 1 to show only what stands below the height y: a building drawn as far as it is built
uniform vec4 u_keepBelow;
// The creature spells' looks: the ice a frozen creature is sheened with by how frozen it is (a positive v_haze.w), and
// the static one fizzing out of sight is drawn through, which slides across its skin by u_creatureSpellLook.xy
SAMPLER2D(s_iceEnvironment, 10);
// The ice's alpha, which weighs how much of it is added. Every stage is taken by one shader or another: it goes at the
// environment map's, which no creature's shader samples, and environment-mapped meshes, which are never frozen, have it at
// the height map's, which their vertex shader doesn't sample
#ifdef USE_ENVIRONMENT
SAMPLER2D(s_iceEnvironmentAlpha, 1);
#else
SAMPLER2D(s_iceEnvironmentAlpha, 5);
#endif // USE_ENVIRONMENT
SAMPLER2D(s_staticAlpha, 15);
// A creature fizzing out of sight is drawn twice. z 1: its depth alone, wherever the static's alpha is at least w; z 2:
// its body, blended at w, over just that depth. z 3: frozen, the ice alone over all of its mesh, with w the ice's alpha.
// z 0 for anything else, with w the ice's alpha on a piece of a creature drawn on its own.
uniform vec4 u_creatureSpellLook;

// The ice over a frozen creature, looked up by the way each face points across the view, so it shows facet by facet. Its
// colour, weighed by its alpha.
vec3 IceSheen(vec3 position)
{
	vec3 face = normalize(cross(dFdx(position), dFdy(position)));
	vec3 eye = u_invView[3].xyz;
	face = dot(face, eye - position) < 0.0f ? -face : face;
	vec3 viewFace = normalize(mul(u_view, vec4(face, 0.0f)).xyz);
	vec2 iceUv = (viewFace.xy + 1.0f) * 0.498046875f;
	return texture2D(s_iceEnvironment, iceUv).rgb * texture2D(s_iceEnvironmentAlpha, iceUv).r;
}

void main()
{
	// Frozen while fizzing, the ice alone is added over all of the creature after its fizzing passes, by the ice's alpha
	if (u_creatureSpellLook.z > 2.5f)
	{
		gl_FragColor = vec4(IceSheen(v_position.xyz), u_creatureSpellLook.w);
		return;
	}
	// Fizzing, its depth goes down only where the static over its skin is strong enough, whatever its own texture
	if (u_creatureSpellLook.z > 0.5f && u_creatureSpellLook.z < 1.5f)
	{
		float staticAlpha = floor(texture2D(s_staticAlpha, v_texcoord0.xy + u_creatureSpellLook.xy).r * 255.0f + 0.5f);
		if (staticAlpha < floor(u_creatureSpellLook.w * 255.0f + 0.5f) || (u_seaClip.x > 0.5f && v_position.y < u_seaClip.y))
		{
			discard;
		}
		gl_FragColor = vec4_splat(0.0f);
		return;
	}

	float alphaThreshold = u_skyAlphaThreshold.y;

	vec4 diffuseTex = texture2D(s_diffuse, v_texcoord0.xy);
	vec3 light = v_color0.rgb;
	bool tinted = u_tint.w > 0.0f;
	diffuseTex.rgb = diffuseTex.rgb * (tinted ? vec3_splat(1.0f) : light) * u_tint.rgb;
	diffuseTex.a = diffuseTex.a * (tinted ? u_tint.w : 1.0f) * (u_objectLook.w > 0.0f ? u_objectLook.w : 1.0f);
	bool alphaBeforeTest = u_seaClip.w > 0.5f;
	if (alphaBeforeTest)
	{
		diffuseTex.a = diffuseTex.a * v_color0.a;
	}
	// A see-through primitive drops the fragments whose alpha is below the reference of its mode, if its mode has an
	// alpha test. The reference of the blended chroma mode becomes lower with the instance's alpha.
	float seeThroughTest = floor(u_skyAlphaThreshold.z + 0.5f);
	bool belowThreshold = diffuseTex.a <= alphaThreshold;
	if (seeThroughTest > 0.5f)
	{
		float reference = seeThroughTest > 1.5f ? u_skyAlphaThreshold.w : 0.0f;
		// At the full alpha the game draws the first table, which keeps the reference
		float instanceAlpha = floor(v_color0.a * 255.0f + 0.5f);
		if (seeThroughTest > 2.5f && instanceAlpha < 255.0f)
		{
			reference = max(floor((reference * instanceAlpha + 0.5f) / 255.0f) - 5.0f, 0.0f);
		}
		belowThreshold = diffuseTex.a * 255.0f < reference;
	}
	if (belowThreshold || (u_seaClip.x > 0.5f && v_position.y < u_seaClip.y) ||
	    (u_keepBelow.x > 0.5f && v_position.y > u_keepBelow.y))
	{
		discard;
	}
	// Fading by its alpha, blended over what is behind it
	if (u_seaClip.z > 0.5f)
	{
		diffuseTex.a = alphaBeforeTest ? diffuseTex.a : diffuseTex.a * v_color0.a;
	}
	// Fading out of sight: the share of it gone, in specks of the world picked at random, isn't drawn
	else if (v_color0.a < 1.0f)
	{
		float pick = fract(sin(dot(floor(v_position.xyz * 4.0f), vec3(12.9898f, 78.233f, 37.719f))) * 43758.5453f);
		if (pick > v_color0.a)
		{
			discard;
		}
	}
	// Fizzing, its body is blended at what is left of its alpha
	if (u_creatureSpellLook.z > 1.5f)
	{
		diffuseTex.a = diffuseTex.a * u_creatureSpellLook.w;
	}
	// Snow covers the object where it shows, in its own light, over its own texture
	float snowLevel = floor(v_snow.z * 255.0f + 0.5f);
	if (snowLevel > 0.0f && SnowShows(snowLevel, texture2D(s_snowAlpha, v_snow.xy).r))
	{
		diffuseTex.rgb = texture2D(s_snow, v_snow.xy).rgb * (tinted ? vec3_splat(1.0f) : v_snowLight) * u_tint.rgb;
	}
#ifdef USE_ENVIRONMENT
	// The second texture stage combines the environment map with the colour as the primitive's material says: added,
	// which saturates, multiplied, doubled after multiplying, in its place, or not at all
	vec3 environment = texture2D(s_environment, v_texcoord0.zw).rgb;
	float stage = floor(u_environmentStage.x + 0.5f);
	if (stage == 1.0f)
	{
		diffuseTex.rgb = environment;
	}
	else if (stage == 3.0f)
	{
		diffuseTex.rgb = diffuseTex.rgb * environment;
	}
	else if (stage == 4.0f)
	{
		diffuseTex.rgb = min(diffuseTex.rgb * environment * 2.0f, vec3_splat(1.0f));
	}
	else if (stage == 5.0f)
	{
		diffuseTex.rgb = min(diffuseTex.rgb + environment, vec3_splat(1.0f));
	}
	// Or the environment map alone, in the light, at an alpha of its own
	if (u_glow.w > 0.0f)
	{
		diffuseTex = vec4(texture2D(s_environment, v_texcoord0.zw).rgb * light, u_glow.w);
	}
#endif // USE_ENVIRONMENT
	// Frozen, the ice is added over it by its alpha
	// A piece of it drawn on its own gives the ice's alpha in place of the instance
	// A z of -1 shows no ice at all, as the game mirrors a frozen body without it. The fizzing passes show none either:
	// the ice comes after them, over all of the mesh.
	float frozen = 0.0f;
	if (u_creatureSpellLook.z > -0.5f && u_creatureSpellLook.z < 0.5f)
	{
		frozen = v_haze.w > 0.0f ? v_haze.w : u_creatureSpellLook.w;
	}
	if (frozen > 0.0f)
	{
		diffuseTex.rgb = min(diffuseTex.rgb + IceSheen(v_position.xyz) * frozen, vec3_splat(1.0f));
	}
	// A glow of its own, 0xRRGGBB, is added as the vertices' specular is, as the heal lights the people it heals
	vec3 glow = u_glow.rgb;
	if (v_snow.w > 0.5f)
	{
		float glowColour = floor(v_snow.w + 0.5f);
		glow += vec3(floor(glowColour / 65536.0f), mod(floor(glowColour / 256.0f), 256.0f), mod(glowColour, 256.0f)) / 255.0f;
	}
	gl_FragColor = vec4(min(diffuseTex.rgb + glow + v_haze.rgb, vec3_splat(1.0f)), diffuseTex.a);
	// The creatures' and the hand's shadows are laid over it in black, straight down
	if (u_groundShadowInfo.x > 0.0f)
	{
		gl_FragColor.rgb = gl_FragColor.rgb * GroundShadowsOnObject(v_position.xyz);
	}
}
