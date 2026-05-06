cbuffer MaterialParamBuffer : register(b0, space0) {
    float4 colorMultiply;
    float4 filter;
    float boxSampleDelta;
    float intensity;
    float usePrefilter; // 0.0 or 1.0
    float useFinalPass; // 0.0 or 1.0
};

Texture2D sourceTexture  : register(t0);
Texture2D screenTexture  : register(t1);
Texture2D maskTexture    : register(t2);

SamplerState clampSampler : register(s0);

struct PixelInputType {
    float4 Position : SV_POSITION;
    float2 uv       : TEXCOORD0;
};

float3 Prefilter(float3 c) {
    // brightness is max of color channels
    float brightness = max(c.r, max(c.g, c.b));
    float soft = brightness - filter.y;
    soft = clamp(soft, 0, filter.z);
    soft = soft * soft * filter.w;
    float contribution = max(soft, brightness - filter.x);
    contribution /= max(brightness, 0.00001);
    return c * contribution;
}

float3 SampleBox(float2 uv, float delta) {
    float width, height, numOfLevels;
    sourceTexture.GetDimensions(0, width, height, numOfLevels);
    
    float4 o = float2(1.0 / width, 1.0 / height).xyxy * float2(-delta, delta).xxyy;
    float3 s = sourceTexture.Sample(clampSampler, uv + o.xy).rgb + sourceTexture.Sample(clampSampler, uv + o.zy).rgb +
			   sourceTexture.Sample(clampSampler, uv + o.xw).rgb + sourceTexture.Sample(clampSampler, uv + o.zw).rgb;
    return s * 0.25f;
}

// NOTE: implementation of different shader passes for bloom using conditionals, may not be the most performant
//       should use macros
float4 main(PixelInputType i) : SV_TARGET {
    float3 color;
    
    if (useFinalPass == 0) {
        // Prefilter + first downsample pass
        if (usePrefilter != 0) {
            color = Prefilter(SampleBox(i.uv, 1));
        }
        // Upsample/downsample pass
        else {
            color = SampleBox(i.uv, boxSampleDelta);
        }
    }
    // Use final upsample + bloom addition pass
    else {
        color = screenTexture.Sample(clampSampler, i.uv).rgb;
        // mask texture (e.g. used for object silhouettes in outline effect)
        float mask = step(maskTexture.Sample(clampSampler, i.uv).a, 0.0);
        color.rgb += intensity * SampleBox(i.uv, 0.5) * (mask * colorMultiply.rgb);
    }
    
    return float4(color, 1.0);
}