// What a mesh's fragment stage gets (mesh.vert's outputs, the material's set 1) and writes: shared by the Standard
// material (mesh_standard.glsl) and the material shaders games write (MaterialData::Shader; docs/Architecture.md,
// "Material shaders"). Included, not compiled on its own. A material shader gets exactly this set 1, filled from its
// MaterialData: the Material block (tint, emission, tiling, alpha cut-off, normal strength) and the albedo, emissive
// and normal textures, all with the material's filter, repeating. Set 0 (the view: camera, lights, shadows) comes
// with lighting.glsl. It may declare the object's push constants (mesh_fade.frag's block) to read the opacity.
// Safe to include twice.
#ifndef MESH_MATERIAL_GLSL
#define MESH_MATERIAL_GLSL

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragColor;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) flat in int entityID;
layout(location = 4) in vec3 fragWorldPos;

// Must match MaterialUniform in Renderer3D.cpp (std140, 48 bytes).
layout(set = 1, binding = 0) uniform Material {
    vec4 tint;
    vec4 emission;   // rgb = colour * intensity
    vec2 tiling;     // repeats per metre
    float alphaCutoff;
    float normalStrength; // 0 = no normal map (binding 3 is the flat one)
} material;
layout(set = 1, binding = 1) uniform sampler2D albedoTexture;
layout(set = 1, binding = 2) uniform sampler2D emissiveTexture;
layout(set = 1, binding = 3) uniform sampler2D normalTexture; // tangent space, RGBA (linear), green up

layout(location = 0) out vec4 outColor;
// The Editor view's entity-ID attachment; ignored in views without it.
layout(location = 1) out int outEntityID;

#endif
