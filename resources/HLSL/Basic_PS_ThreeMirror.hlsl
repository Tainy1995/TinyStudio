#include "Basic.hlsli"

// 像素着色器(2D)
float4 PS_2D(VertexPosHTex pIn) : SV_Target
{
    float left_arrow = 1 / 3.0;
    float right_arrow = 2 / 3.0;
    if (pIn.Tex.x < left_arrow)
        pIn.Tex.x += left_arrow;
    else if (pIn.Tex.x > right_arrow)
        pIn.Tex.x = left_arrow + (pIn.Tex.x - right_arrow);
    return g_Tex.Sample(g_SamLinear, pIn.Tex);
}