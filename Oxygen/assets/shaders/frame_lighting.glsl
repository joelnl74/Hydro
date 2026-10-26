// The view's forward lighting and sun shadow map settings: set 0 binding 2, filled every frame by
// Renderer2D::UploadLighting. Shared by lighting.glsl and the shadow caster shaders (which only read
// shadowMatrix). Included, not compiled on its own.

// Must match FrameLightingUniform in VulkanGPUData.h (std140, 224 bytes).
layout(set = 0, binding = 2) uniform FrameLighting {
    vec4 cameraPosition;
    vec4 sunDirection;  // xyz towards the sun (unit length), w = 1 when there is a sun
    vec4 sunColor;      // rgb, times its intensity
    vec4 skyAmbient;    // what surfaces facing up get
    vec4 groundAmbient; // and facing down
    vec4 fogColor;      // w = 1 when fog is on
    vec4 fogParams;     // x start, y end (metres from fogOrigin), z bands of the point lights' falloff
    ivec4 counts;       // x lights, y = 1 when the view is forward lit, z = 1 when it has a sun shadow map
    vec4 fogOrigin;     // xyz: what the camera looks at (its point on the ground)
    mat4 shadowMatrix;  // world -> the sun shadow map's clip space (xy -1..1, z 0..1)
    vec4 shadowParams;  // x strength, y one texel in UV, z 1 = soft (3 x 3) / 0 = hard, w normal offset (metres)
} lighting;
