cbuffer FXAAParameters : register(b0)
{
    float SubpixelBlend; // 권장 0.5 (값이 높을수록 부드러우나 흐릿해짐)
    float EdgeThreshold; // 권장 0.125 (낮을수록 더 많은 에지를 처리)
    float EdgeThresholdMin; // 권장 0.0312 (낮을수록 약한 에지도 처리)
    float Padding; // 16바이트 정렬용
};

Texture2D SceneTexture : register(t0);
SamplerState LinearClamp : register(s0);

struct VSOut
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
};

VSOut mainVS(uint VertexID : SV_VertexID)
{
    VSOut O;
    float2 Pos = float2((VertexID == 2) ? 3.0 : -1.0,
                           (VertexID == 1) ? 3.0 : -1.0);
    O.Position = float4(Pos, 0.0, 1.0);
    O.UV = float2((Pos.x + 1.0) * 0.5, 1.0 - (Pos.y + 1.0) * 0.5);
    return O;
}

float Luma(float3 Rgb)
{
    return dot(Rgb, float3(0.299, 0.587, 0.114));
}

float4 mainPS(VSOut In) : SV_Target
{
    uint W, H;
    SceneTexture.GetDimensions(W, H);
    float2 Rcp = 1.0 / float2(W, H);

    float3 C = SceneTexture.Sample(LinearClamp, In.UV).rgb;

    float L = Luma(C);
    float LN = Luma(SceneTexture.Sample(LinearClamp, In.UV + float2(0, -Rcp.y)).rgb);
    float LS = Luma(SceneTexture.Sample(LinearClamp, In.UV + float2(0, Rcp.y)).rgb);
    float LW = Luma(SceneTexture.Sample(LinearClamp, In.UV + float2(-Rcp.x, 0)).rgb);
    float LE = Luma(SceneTexture.Sample(LinearClamp, In.UV + float2(Rcp.x, 0)).rgb);

    float LMin = min(L, min(min(LN, LS), min(LW, LE)));
    float LMax = max(L, max(max(LN, LS), max(LW, LE)));
    float Contrast = LMax - LMin;

    if (Contrast < max(EdgeThresholdMin, LMax * EdgeThreshold))
    {
        // 아주 약한 엣지는 그대로 통과 (서브픽셀 보정만 살짝 넣고 싶으면 아래에 따로 처리)
        return float4(C, 1.0);
    }

    // 엣지 방향 결정(가로/세로)
    float EdgeH = abs(LW + LE - 2.0 * L);
    float EdgeV = abs(LN + LS - 2.0 * L);
    bool IsHorizontal = (EdgeH >= EdgeV);

    float2 Step = IsHorizontal ? float2(Rcp.x, 0) : float2(0, Rcp.y);

    float L1 = Luma(SceneTexture.Sample(LinearClamp, In.UV - Step * 1.0).rgb);
    float L2 = Luma(SceneTexture.Sample(LinearClamp, In.UV + Step * 1.0).rgb);
    float Gradient1 = abs(L1 - L);
    float Gradient2 = abs(L2 - L);

    float2 Direction = (Gradient1 < Gradient2) ? Step : -Step;

    float2 UVA = In.UV + Direction * 0.5;
    float2 UVB = In.UV + Direction * 2.0;

    float3 CA = SceneTexture.Sample(LinearClamp, UVA).rgb;
    float3 CB = SceneTexture.Sample(LinearClamp, UVB).rgb;

    float3 CFxaa = (CA + CB + C) / 3.0;
    CFxaa = lerp(C, CFxaa, SubpixelBlend);

    return float4(CFxaa, 1.0);
}