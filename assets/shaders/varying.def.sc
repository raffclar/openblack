vec4 a_position          : POSITION;
vec3 a_normal            : NORMAL;
vec4 a_indices           : BLENDINDICES;
vec4 a_color0            : COLOR0;     // time of day
vec4 a_color1            : COLOR1;     // firstMaterialID, w: once-per-block bits; a creature's weak or strong position
vec4 a_color2            : COLOR2;     // secondMaterialID, w: once-per-block bits
float a_color3           : COLOR3;     // water alpha
vec2 a_texcoord0         : TEXCOORD0;
vec3 a_texcoord1         : TEXCOORD1;  // weight; a creature's thin or fat position
vec3 a_texcoord2         : TEXCOORD2;  // material blend coefficient; a creature's thin or fat normal
vec3 a_tangent           : TANGENT;    // a creature's evil or good position
vec3 a_bitangent         : BITANGENT;  // a creature's evil or good normal
vec4 a_weight            : BLENDWEIGHT; // a creature's weak or strong normal
vec4 i_data0             : TEXCOORD7;
vec4 i_data1             : TEXCOORD6;
vec4 i_data2             : TEXCOORD5;
vec4 i_data3             : TEXCOORD4;
vec4 i_data4             : TEXCOORD3;

vec4 v_position          : TEXCOORD1 = vec4(0.0, 0.0, 0.0, 0.0);
vec4 v_color0            : COLOR0    = vec4(1.0, 0.0, 0.0, 1.0);
vec4 v_texcoord0         : TEXCOORD0 = vec4(0.0, 0.0, 0.0, 1.0);
vec4 v_texcoord1         : TEXCOORD1 = vec4(0.0, 0.0, 0.0, 1.0);
vec3 v_normal            : NORMAL;
vec3 v_weight            : COLOR5;
flat ivec4 v_materialID0 : COLOR0;
flat ivec4 v_materialID1 : COLOR1;
vec3 v_materialBlend     : COLOR2;
float v_lightLevel       : COLOR3;
float v_shoreFade        : COLOR4;
float v_distToCamera     : DEPTH0;
vec2 v_smallBumpFade     : TEXCOORD3; // small bump pass vertex diffuse: x colour (white/black), y alpha
vec3 v_landLight         : TEXCOORD4;
vec3 v_landSpecular      : TEXCOORD6;
vec2 v_worldXZ           : TEXCOORD7;
