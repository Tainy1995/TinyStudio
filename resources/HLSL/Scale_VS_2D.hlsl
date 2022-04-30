#include "Basic.hlsli"

// 顶点着色器(2D)
VertexPosHTex VS_2D(VertexPosTex vIn)
{
    VertexPosHTex vOut;
    float2 cropCenter = float2(1/3.0f, 1.0f);
    float2 offsetParam = float2(1/3.0f, 1.0f);

    vOut.PosH = float4(vIn.PosL, 1.0f);
    vOut.Tex = vIn.Tex * cropCenter + offsetParam;

    return vOut;
}