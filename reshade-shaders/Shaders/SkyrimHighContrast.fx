// Scenery/detail study only. Category colours require an engine-supplied mask.
// This pass still affects game UI; do not distribute as a complete accessibility mode.
#include "ReShade.fxh"

uniform float3 WorldColour < ui_type = "color"; > = float3(0.19, 0.24, 0.28);
uniform float DepthScale < ui_type = "slider"; ui_min = 1.0; ui_max = 100.0; > = 20.0;
uniform float EdgeStrength < ui_type = "slider"; ui_min = 0.0; ui_max = 1.0; > = 0.6;
uniform float ShapeStrength < ui_label = "Geometry shading"; ui_type = "slider"; ui_min = 0.0; ui_max = 1.0; > = 0.75;
uniform float VerticalFOV < ui_label = "Vertical field of view (approximate)"; ui_type = "slider"; ui_min = 30.0; ui_max = 110.0; > = 60.0;
uniform float LuminanceDetail < ui_label = "Original lighting / texture detail"; ui_tooltip = "Optional grayscale detail for eyes and lips. Also brings back some surface texture; applies to the whole frame."; ui_type = "slider"; ui_min = 0.0; ui_max = 1.0; > = 0.0;

float3 ViewPosition(float2 uv, float depth)
{
    float halfHeight = tan(VerticalFOV * 0.00872664626);
    float2 ray = (uv * 2.0 - 1.0) * float2(BUFFER_WIDTH * BUFFER_RCP_HEIGHT, -1.0);
    return float3(ray * halfHeight, 1.0) * depth;
}

float3 SurfaceNormal(float2 uv, float depth, out float edge)
{
    float2 stepX = float2(BUFFER_RCP_WIDTH, 0.0);
    float2 stepY = float2(0.0, BUFFER_RCP_HEIGHT);
    float left = ReShade::GetLinearizedDepth(saturate(uv - stepX));
    float right = ReShade::GetLinearizedDepth(saturate(uv + stepX));
    float above = ReShade::GetLinearizedDepth(saturate(uv - stepY));
    float below = ReShade::GetLinearizedDepth(saturate(uv + stepY));
    float3 center = ViewPosition(uv, depth);
    // Prefer the neighbour on the same surface at silhouettes.
    float3 dx = abs(right - depth) <= abs(depth - left)
        ? ViewPosition(saturate(uv + stepX), right) - center
        : center - ViewPosition(saturate(uv - stepX), left);
    float3 dy = abs(below - depth) <= abs(depth - above)
        ? ViewPosition(saturate(uv + stepY), below) - center
        : center - ViewPosition(saturate(uv - stepY), above);
    // Scale first to avoid tiny cross products near the camera.
    dx /= max(length(dx), 1e-12);
    dy /= max(length(dy), 1e-12);
    float3 normal = cross(dx, dy);
    float magnitude = length(normal);
    normal = magnitude > 1e-6 ? normal / max(magnitude, 1e-6) : float3(0.0, 0.0, -1.0);
    if (normal.z > 0.0)
        normal = -normal;
    float jump = max(max(abs(left - depth), abs(right - depth)),
                     max(abs(above - depth), abs(below - depth)));
    edge = saturate(jump / max(depth, 0.0001) * 12.0);
    return normal;
}

float4 HC_PS(float4 position : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    float4 original = tex2D(ReShade::BackBuffer, uv);
    float depth = ReShade::GetLinearizedDepth(uv);
    // Depth normals retain geometric features without sampling texture colour.
    float shade = lerp(1.15, 0.55, saturate(depth * DepthScale));
    float edge;
    float3 normal = SurfaceNormal(uv, depth, edge);
    float lighting = 0.45 + 0.95 * saturate(dot(normal, normalize(float3(-0.35, 0.45, -1.0))));
    float detail = lerp(1.0, lighting, ShapeStrength);
    // Optional luminance affects brightness only, never assigns a category.
    float luminance = dot(original.rgb, float3(0.2126, 0.7152, 0.0722));
    detail *= lerp(1.0, 0.4 + 1.2 * sqrt(saturate(luminance)), LuminanceDetail);
    float3 result = WorldColour * shade * detail * (1.0 - edge * EdgeStrength);
    if (depth >= 0.9999)
        result = WorldColour * 1.25;

    return float4(saturate(result), original.a);
}

technique SkyrimHighContrast < ui_label = "Skyrim High Contrast (scenery study - affects UI)"; >
{
    pass
    {
        VertexShader = PostProcessVS;
        PixelShader = HC_PS;
    }
}
