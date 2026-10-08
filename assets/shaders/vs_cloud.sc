$input a_position, a_texcoord0, a_normal
$output v_texcoord0, v_color0

#include <bgfx_shader.sh>

// The same model light as vs_object: ModelLightI / ModelLightFactor / ModelLightDiffuse of src/Graphics/ModelLight.h
// The clouds and the mists carry their own ambient and light in u_cloud.z / u_cloudLight, which is what
// the mist draw puts in the model light's ambient and light while it draws them (the ambient set, the light
// saved and moved, both put back afterwards); the CPU fills them from model_light::Ambient
// and model_light::LightInMeshSpace
#include "model_light.sh"

uniform vec4 u_cloud;       // xy: texture atlas offset of the animation frame, z: ambient (210 / 256 for the clouds and
                            // the shrinking mists, 90 / 256 for the other ones), w: unused
uniform vec4 u_cloudLight;  // xyz: the light's direction in the mesh's own space
uniform vec4 u_cloudColour; // rgb: cloud colour, a: alpha

// A sky cloud or a map mist (mist.l3d) drawn like a model with a temporary light straight above (0, 500000, 0)
// and ambient 210 / 256 (the effect branch of the mist draw); mists without the effect flag keep the models' light
// (ambient 90 / 256, the light at (-500000, 500000, -500000))
void main()
{
	v_texcoord0 = vec4(a_texcoord0 + u_cloud.xy, 0.0f, 0.0f);
	// The model light dots the untransformed vertex normal with the light's position brought into the mesh's own space by
	// the inverse of the object matrix (so with a non-uniform scale it is not the light of a rotated normal); f ends up
	// 210..254 for the clouds, never the full 256
	float ambient = floor(u_cloud.z * 256.0f + 0.5f);
	float factor = ModelLightFactor(ModelLightI(a_normal.xyz, u_cloudLight.xyz, false), ambient);
	vec3 colour = floor(u_cloudColour.rgb * 255.0f + 0.5f);
	v_color0 = vec4(ModelLightDiffuse(colour, factor) / 255.0f, u_cloudColour.a);
	gl_Position = mul(u_viewProj, mul(u_model[0], vec4(a_position.xyz, 1.0f)));
}
