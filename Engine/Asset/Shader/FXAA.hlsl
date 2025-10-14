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
        
         // 4방향 평균(가볍고 링잉 적음)
        float3 Avg4 = (
        SceneTexture.Sample(LinearClamp, In.UV + float2(0, -Rcp.y)).rgb +
        SceneTexture.Sample(LinearClamp, In.UV + float2(0, Rcp.y)).rgb +
        SceneTexture.Sample(LinearClamp, In.UV + float2(-Rcp.x, 0)).rgb +
        SceneTexture.Sample(LinearClamp, In.UV + float2(Rcp.x, 0)).rgb
        ) * 0.25;

        // SubpixelBlend는 0~1 권장(예: 0.5)
        float3 Smooth = lerp(C, Avg4, saturate(SubpixelBlend));
        return float4(Smooth, 1.0);
    }
    // --- Diagonal luma for tangent direction (classic FXAA 3.x) ---
    float lNW = Luma(SceneTexture.Sample(LinearClamp, In.UV + float2(-Rcp.x, -Rcp.y)).rgb);
    float lNE = Luma(SceneTexture.Sample(LinearClamp, In.UV + float2(Rcp.x, -Rcp.y)).rgb);
    float lSW = Luma(SceneTexture.Sample(LinearClamp, In.UV + float2(-Rcp.x, Rcp.y)).rgb);
    float lSE = Luma(SceneTexture.Sample(LinearClamp, In.UV + float2(Rcp.x, Rcp.y)).rgb);
    
    // This estimates the **edge tangent** (not the normal).
    float2 Dir;
    Dir.x = -((lNW + lNE) - (lSW + lSE));
    Dir.y = ((lNW + lSW) - (lNE + lSE));

    float DirReduce = max((LN + LS + LW + LE) * (0.25 * 1 / 8), 0.0004);
    float RcpDirMin = 1.0 / (min(abs(Dir.x), abs(Dir.y)) + DirReduce);
    // Dir = saturate(Dir * rcpDirMin) * rcp;
    Dir *= RcpDirMin; // scale by inverse smallest axis + reduce
    float Len = length(Dir);
    if (Len > 0.0)
        Dir *= (min(5.0, Len) / Len); // cap search to ~8 texels total span

    Dir *= Rcp;

    // Two taps along the edge
    float3 RgbA = 0.5 * (
        SceneTexture.Sample(LinearClamp, In.UV + Dir * (1.0 / 3.0 - 0.5)).rgb +
        SceneTexture.Sample(LinearClamp, In.UV + Dir * (2.0 / 3.0 - 0.5)).rgb);

    float3 RgbB = RgbA * 0.5 + 0.25 * (
        SceneTexture.Sample(LinearClamp, In.UV + Dir * -0.5).rgb +
        SceneTexture.Sample(LinearClamp, In.UV + Dir * 0.5).rgb);

    float LumaB = Luma(RgbB);
    // Choose between A and B to avoid over-blurring
    float3 result = (LumaB < LMin || LumaB > LMax) ? RgbA : RgbB;
    // (추가) 엣지에서도 과블러 없이 '아주 약하게' 서브픽셀 보정
    {
        float3 Avg4 = (
        SceneTexture.Sample(LinearClamp, In.UV + float2(0, -Rcp.y)).rgb +
        SceneTexture.Sample(LinearClamp, In.UV + float2(0, Rcp.y)).rgb +
        SceneTexture.Sample(LinearClamp, In.UV + float2(-Rcp.x, 0)).rgb +
        SceneTexture.Sample(LinearClamp, In.UV + float2(Rcp.x, 0)).rgb
        ) * 0.25;
        // 엣지는 선명도 보호를 위해 1/4 배만 적용(원하면 0.15~0.35 사이로 튜닝)
        result = lerp(result, Avg4, saturate(SubpixelBlend) * 0.25);
    }
    
    // float3 result = RgbA;
    return float4(result, 1.0);
}