#include "Model.hlsli"

// 像素着色器
float4 PS_2D(VertexOut pIn) : SV_Target
{
    float4 posH = pIn.PosH;
    float3 PosW = pIn.PosW;
    float3 NormalW = pIn.NormalW;
    float4 texColor = g_DiffuseMap.Sample(g_Sam, pIn.Tex);
    clip(texColor.a - 0.1f);

    return texColor;
}