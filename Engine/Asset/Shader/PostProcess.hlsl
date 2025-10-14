cbuffer PostProcessParameters : register(b0)
{
    // FXAA Parameters
    float SubpixelBlend; // 권장 0.5 (값이 높을수록 부드러우나 흐릿해짐)
    float EdgeThreshold; // 권장 0.125 (낮을수록 더 많은 에지를 처리)
    float EdgeThresholdMin; // 권장 0.0312 (낮을수록 약한 에지도 처리)
    float EnableFXAA; // FXAA 활성화 플래그 (0.0 = OFF, 1.0 = ON)

    float2 ViewportTopLeft;
    float2 ViewportSize;
    float2 SceneRTSize;
    float2 Padding2;

    // Fog Parameters
    float FogDensity;
    float FogHeightFalloff;
    float StartDistance;
    float FogCutoffDistance;

    float FogMaxOpacity;
    float3 FogInscatteringColor;

    float3 CameraPosition;
    float FogHeight;

    row_major float4x4 InvViewProj;
};

Texture2D SceneTexture : register(t0);
Texture2D SceneDepthTexture : register(t1);
SamplerState LinearClamp : register(s0);

struct VS_INPUT
{
    float3 Position : POSITION;
    float2 TexCoord : TEXCOORD0;
};

struct VSOut
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
};

struct PS_OUTPUT
{
    float4 Color : SV_TARGET;
    float Depth : SV_DEPTH;
};

// Fullscreen Quad Vertex Shader
VSOut mainVS(VS_INPUT input)
{
    VSOut O;
    O.Position = float4(input.Position, 1.0);
    O.UV = input.TexCoord;
    return O;
}
// RGB -> 밝기(루마)로 변환. 색보다 밝기가 엣지 판단에 유리하다.
float Luma(float3 Rgb)
{
    return dot(Rgb, float3(0.299, 0.587, 0.114));
}

// Apply Fog (same logic as HeightFogShader.hlsl)
float3 ApplyFog(float3 SceneColor, float Depth, float2 viewportUV)
{
    // If FogDensity is 0, fog is disabled
    if (FogDensity <= 0.0)
    {
        return SceneColor;
    }

    // Reconstruct World Position from Depth
    // IMPORTANT: Use viewportUV (0~1 within viewport) instead of sceneUV
    float2 screenPos = viewportUV * 2.0 - 1.0;
    screenPos.y = -screenPos.y;
    float4 clipPos = float4(screenPos, Depth, 1.0);
    float4 worldPos = mul(clipPos, InvViewProj);
    worldPos /= worldPos.w;

    float3 worldPosition = worldPos.xyz;
    float DistanceFromCamera = length(worldPosition - CameraPosition);

    // Apply Start Distance
    float fogDistance = max(0.0, DistanceFromCamera - StartDistance);

    // Exponential Height Fog - Line Integral
    float CameraHeight = CameraPosition.z;
    float PixelHeight = worldPosition.z;
    float Falloff = FogHeightFalloff * (PixelHeight - CameraHeight);
    float LineIntegral;

    if (abs(Falloff) > 0.01)
    {
        float ExpStart = exp(-FogHeightFalloff * (CameraHeight - FogHeight));
        float ExpEnd = exp(-FogHeightFalloff * (PixelHeight - FogHeight));
        LineIntegral = (ExpStart - ExpEnd) / Falloff;
    }
    else
    {
        float AvgHeight = (CameraHeight + PixelHeight) * 0.5;
        LineIntegral = exp(-FogHeightFalloff * (AvgHeight - FogHeight));
    }

    float FogAmount = FogDensity * LineIntegral * fogDistance;
    float FogFactor = 1.0 - exp(-FogAmount);

    if (DistanceFromCamera > FogCutoffDistance)
    {
        FogFactor = 1.0;
    }

    FogFactor = saturate(FogFactor);
    FogFactor = min(FogFactor, FogMaxOpacity);

    return lerp(SceneColor, FogInscatteringColor, FogFactor);
}

PS_OUTPUT mainPS(VSOut In)
{
    PS_OUTPUT Output;

    // Convert viewport TexCoord to Scene RT UV
    // In.UV는 현재 viewport 내에서 (0,0)~(1,1)
    // Scene RT UV로 변환: (ViewportTopLeft + UV * ViewportSize) / SceneRTSize
    float2 sceneUV = (ViewportTopLeft + In.UV * ViewportSize) / SceneRTSize;

    // Sample Scene Depth and output to BackBuffer DSV
    float Depth = SceneDepthTexture.Sample(LinearClamp, sceneUV).r;
    Output.Depth = Depth;

    // FXAA가 비활성화되었으면 단순히 Scene Color + Fog만 적용
    if (EnableFXAA < 0.5)
    {
        float3 SceneColor = SceneTexture.Sample(LinearClamp, sceneUV).rgb;
        SceneColor = ApplyFog(SceneColor, Depth, In.UV);
        Output.Color = float4(SceneColor, 1.0);
        return Output;
    }

    // Use Scene RT dimensions for FXAA sampling
    // 역해상도를 텍셀 오프셋에 쓴다.
    float2 Rcp = 1.0 / SceneRTSize;

    float3 C = SceneTexture.Sample(LinearClamp, sceneUV).rgb;
    // 현재 픽셀과 상하좌우의 루마를 얻기
    float L = Luma(C);
    float LN = Luma(SceneTexture.Sample(LinearClamp, sceneUV + float2(0, -Rcp.y)).rgb);
    float LS = Luma(SceneTexture.Sample(LinearClamp, sceneUV + float2(0, Rcp.y)).rgb);
    float LW = Luma(SceneTexture.Sample(LinearClamp, sceneUV + float2(-Rcp.x, 0)).rgb);
    float LE = Luma(SceneTexture.Sample(LinearClamp, sceneUV + float2(Rcp.x, 0)).rgb);
    // 루마끼리의 대비 = 엣지 강도! 대비가 작으면 엣지가 약하다는 뜻!
    float LMin = min(L, min(min(LN, LS), min(LW, LE)));
    float LMax = max(L, max(max(LN, LS), max(LW, LE)));
    float Contrast = LMax - LMin;
    // 4방향 평균(가볍고 링잉 적음), 링잉 = 선명한 경계 주변에 밝고 어두운 고리나 잔물결처럼 생기는 거짓 물결
    float3 Avg4 = (
        SceneTexture.Sample(LinearClamp, sceneUV + float2(0, -Rcp.y)).rgb +
        SceneTexture.Sample(LinearClamp, sceneUV + float2(0, Rcp.y)).rgb +
        SceneTexture.Sample(LinearClamp, sceneUV + float2(-Rcp.x, 0)).rgb +
        SceneTexture.Sample(LinearClamp, sceneUV + float2(Rcp.x, 0)).rgb
        ) * 0.25;
    // 엣지가 약하면 원본과 4방향 평균을 SubpixelBlend로 섞고 끝(약한 계단만 살짝 누름)
    if (Contrast < max(EdgeThresholdMin, LMax * EdgeThreshold))
    {
        // SubpixelBlend는 0~1 권장(예: 0.5)
        float3 Smooth = lerp(C, Avg4, saturate(SubpixelBlend));

        // Apply Fog after AA (use In.UV for correct viewport-relative position)
        Smooth = ApplyFog(Smooth, Depth, In.UV);

        Output.Color = float4(Smooth, 1.0);
        return Output;
    }
    // --- Diagonal luma for tangent direction (classic FXAA 3.x) ---
    float LNW = Luma(SceneTexture.Sample(LinearClamp, sceneUV + float2(-Rcp.x, -Rcp.y)).rgb);
    float LNE = Luma(SceneTexture.Sample(LinearClamp, sceneUV + float2(Rcp.x, -Rcp.y)).rgb);
    float LSW = Luma(SceneTexture.Sample(LinearClamp, sceneUV + float2(-Rcp.x, Rcp.y)).rgb);
    float LSE = Luma(SceneTexture.Sample(LinearClamp, sceneUV + float2(Rcp.x, Rcp.y)).rgb);
    
    // Edge tangent(엣지의 법선(노말)이 아님, 엣지 방향)
    float2 Dir;
    // 대각선 애들을 써서 엣지의 진행 방향을 얻음
    // x가 크면 수평 엣지, y가 크면 수직 엣지
    Dir.x = -((LNW + LNE) - (LSW + LSE));
    Dir.y = ((LNW + LSW) - (LNE + LSE));
    // 상하좌우 루마 평균 기반의 작은 완충치 (min(|x|,|y|)가 너무 작아 방향 비율이 폭주하는 걸 방지
    float DirReduce = max((LN + LS + LW + LE) * (0.25 * 1 / 8), 1 / 128);
     // Dir의 두 축 중 작은쪽 성분 (작은 쪽이 엣지 방향인 것임)
    float RcpDirMin = 1.0 / (min(abs(Dir.x), abs(Dir.y)) + DirReduce);
    // 작은 축 기준으로 적당히 키움
    Dir *= RcpDirMin; // scale by inverse smallest axis + reduce
    // 너무 멀리까지 탐색하지 않도록 길이를 제한
    float Len = length(Dir);
    if (Len > 0.0)
        Dir *= (min(5.0, Len) / Len); // 탐색 폭 캡 (~8 texels 근처)
    // Rcp 곱으로 픽셀좌표 -> 텍스쳐 좌표 보정
    Dir *= Rcp;

    // 엣지 방향을 따라 자기 점에서 양쪽인 가까운 두 점 평균(부드럽고 안정적).
    float3 RgbA = 0.5 * (
        SceneTexture.Sample(LinearClamp, sceneUV + Dir * (1.0 / 3.0 - 0.5)).rgb +
        SceneTexture.Sample(LinearClamp, sceneUV + Dir * (2.0 / 3.0 - 0.5)).rgb);
    // 더 멀리 두 점을 추가로 섞은 버전(더 강한 블러 가능).
    float3 RgbB = RgbA * 0.5 + 0.25 * (
        SceneTexture.Sample(LinearClamp, sceneUV + Dir * -0.5).rgb +
        SceneTexture.Sample(LinearClamp, sceneUV + Dir * 0.5).rgb);

    float LumaB = Luma(RgbB);
    // B 후보가 주변 루마 범위([LMin, LMax])를 벗어나면 "너무 많이 건드린 것"으로 간주 → RgbA(덜 흐림)를 채택.
    float3 result = (LumaB < LMin || LumaB > LMax) ? RgbA : RgbB;
    
    // 엣지에서도 과블러 없이 '아주 약하게' 서브픽셀 보정 (Avg4)
    // 엣지는 선명도 보호를 위해 1/4 배만 적용(원하면 0.15~0.35 사이로 튜닝)
    result = lerp(result, Avg4, saturate(SubpixelBlend) * 0.25);

    // Apply Fog after AA (use In.UV for correct viewport-relative position)
    result = ApplyFog(result, Depth, In.UV);

    // float3 result = RgbA;
    Output.Color = float4(result, 1.0);
    return Output;
}