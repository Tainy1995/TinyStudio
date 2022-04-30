#include "Model.hlsli"

// 顶点着色器
VertexOut VS_2D(VertexIn vIn)
{
    VertexOut vOut;
    matrix viewProj = mul(g_View, g_Proj);
    float4 posW = mul(float4(vIn.PosL, 1.0f), g_World);

    vOut.PosH = mul(posW, viewProj);
    vOut.PosW = posW.xyz;
    vOut.NormalW = vIn.NormalL;
    vOut.Tex = vIn.Tex;
    return vOut;
}
