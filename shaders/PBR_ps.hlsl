// Cook-Torrance PBR based on https://learnopengl.com/PBR/Theory

#define PI 3.14159265359f

// Must change cubemap gen LOD count if this value is changed
#define MAX_REFLECTION_LOD 9.0

cbuffer MaterialCB : register(b0, space1) {
    float UseParallaxShadow;
    float MinParallaxLayers;
    float MaxParallaxLayers;

    float DirectionalShadowBias; // currently unsued

    float ParallaxMagnitude;
};

cbuffer LightCB : register(b1) {
    float4 Time;
    float4 DirLight; // vector of directional light
    float4 DirLightColor;
};

Texture2D AlbedoTex                   : register(t0);
Texture2D NormalTex                   : register(t1);
Texture2D MaterialTex                 : register(t2); // [r: ao, g: metallic, b: roughness, a: height]
TextureCube<float4> IrradianceCubemap : register(t3);
TextureCube<float4> PrefilterCubemap  : register(t4);
Texture2D BRDFLut                     : register(t5);
Texture2D DirectionalShadowMap        : register(t6);

SamplerState AnisoWrapSampler                 : register(s0);
SamplerState TrilinearClampSampler            : register(s1); // for BRDF lut
SamplerComparisonState TrilinearBorderSampler : register(s2); // for directional shadow map

struct PixelInputType {
    float4 position                     : SV_POSITION;
    float4 color                        : COLOR;
    float3 tangent                      : TANGENT;
    float3 bitangent                    : BITANGENT;
    float3 normal                       : NORMAL0;
    float2 uv                           : TEXCOORD0;
    float4 worldPosition                : TEXCOORD1;
    float4 cameraPosition               : TEXCOORD2;
    float4 directionalLightViewPosition : TEXCOORD3;
    float3 tangentViewDirection         : TEXCOORD4;
};

// Normal distribution function
float DistributionGGX(float3 N, float3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    //return nom / denom;
    return nom / max(denom, 0.00000001);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(float NdotV, float NdotL, float roughness) {
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    return ggx1 * ggx2;
}

float3 FresnelSchlick(float cosTheta, float3 f0) {
    return f0 + (1.0 - f0) * pow(saturate(1.0 - cosTheta), 5.0);
}

float3 FresnelSchlickRoughness(float cosTheta, float3 f0, float roughness) {
    return f0 + (max(1.0 - roughness, f0) - f0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// lightDir and lightHalfVector given to avoid recalculation (half vector is calculated before this funciton for specular)
float3 CalcReflectanceFromLight(const float3 lightDir, const float3 radiance, const float3 albedo, const float3 metallic, const float3 f0, const float roughness, const float3 normal, const float3 viewDirection, const float NdotV) {
    const float3 H = normalize(viewDirection + lightDir);
    const float NdotL = max(dot(normal, lightDir), 0.0);

    // Cook-Torrance BRDF
    const float NDF = DistributionGGX(normal, H, roughness);
    const float G = GeometrySmith(NdotV, NdotL, roughness);
    const float3 F = FresnelSchlick(max(dot(viewDirection, H), 0.0), f0);
           
    const float3 numerator = NDF * G * F;
    const float denominator = 4.0 * NdotV * NdotL + 0.0001;
    // Currently only from directional light
    const float3 specular = numerator / denominator;
    
    // (Bandaid Fix) Prevent artifacts from extremely bright pixels on perfectly smooth materials
    // This may cause some specular highilghts to be omitted (e.g. marble sphere from demo scene)
    //specular = clamp(specular, 0, 10000);
    
    // for energy conservation, the diffuse and specular light can't
    // be above 1.0 (unless the surface emits light); to preserve this
    // relationship the diffuse component (kD) should equal 1.0 - F (specular is Fresnel-Schlick term).
    const float3 kD = (1.0 - F) * (1.0 - metallic);
    
    // reflectance equation
    // outgoing radiance Lo
    // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again here
    return (kD * albedo / PI + specular) * radiance * NdotL;
}

// Parallax mapping adapted from: https://learnopengl.com/Advanced-Lighting/Parallax-Mapping
float2 ParallaxMapping(float2 texCoords, float3 viewDir) {
    float numLayers = lerp(MaxParallaxLayers, MinParallaxLayers, abs(dot(float3(0.0, 0.0, 1.0), viewDir)));
    // calculate the size of each layer
    float layerDepth = 1.0 / numLayers;
    // depth of current layer
    float currentLayerDepth = 0.0;
    // the amount to shift the texture coordinates per layer (from vector P)
    float2 P = viewDir.xy / viewDir.z * ParallaxMagnitude;
    float2 deltaTexCoords = P / numLayers;
  
    // get initial values
    float2 currentTexCoords = texCoords;
    float currentDepthMapValue = 1.0 - MaterialTex.Sample(AnisoWrapSampler, currentTexCoords).r;

    [loop]
    for (int i = 0; i < MaxParallaxLayers && currentLayerDepth < currentDepthMapValue; i++) {
        // shift texture coordinates along direction of P
        currentTexCoords -= deltaTexCoords;
        // get depthmap value at current texture coordinates
        currentDepthMapValue = 1.0 - MaterialTex.Sample(AnisoWrapSampler, currentTexCoords).r;
        // get depth of next layer
        currentLayerDepth += layerDepth;
    }
    
    // get texture coordinates before collision (reverse operations)
    float2 prevTexCoords = currentTexCoords + deltaTexCoords;

    // get depth after and before collision for linear interpolation
    float afterDepth = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = 1.0 - MaterialTex.Sample(AnisoWrapSampler, prevTexCoords).r - currentLayerDepth + layerDepth;
 
    // interpolation of texture coordinates
    float weight = afterDepth / (afterDepth - beforeDepth);
    float2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);
    
    // Note: could be used for shadow correction
    // currentParallaxLayer = currentLayerDepth + beforeDepth * weight + afterDepth * (1.0 - weight);

    return finalTexCoords;
}

// EXPERIMENTAL
// Parallax map self shadowing adpated from: https://chanhaeng.blogspot.com/2019/01/normalparllax-mapping-with-self.html
float CalcParallaxSoftShadowMultiplier(float3 lightDir, float2 initialTexCoords, float initialHeight) {
    float shadowMultiplier = 0.0;
    
    float dotDir = max(dot(float3(0, 0, 1), lightDir), 0.0);

    // calculate lighting only for surface oriented to the light source
    if (dotDir > 0) {
        // calculate initial parameters
        float numSamplesUnderSurface = 0;
        shadowMultiplier = 0;
        float numLayers = lerp(MaxParallaxLayers, MinParallaxLayers, dotDir);
        float layerHeight = initialHeight / numLayers;
        float2 texStep = ParallaxMagnitude * lightDir.xy / lightDir.z / numLayers;

        // current parameters
        float currentLayerHeight = initialHeight - layerHeight;
        float2 currentTexCoords = initialTexCoords + texStep;
        float depthFromTexture = 1.0 - MaterialTex.Sample(AnisoWrapSampler, currentTexCoords).a;
        
        // while point is below depth 0.0
        [loop]
        for (int i = 1; i < MaxParallaxLayers && currentLayerHeight > 0.0; i++) {
            // if point is under the surface
            if (depthFromTexture < currentLayerHeight) {
                // calculate partial shadowing factor
                numSamplesUnderSurface += 1;
                float newShadowMultiplier = (currentLayerHeight - depthFromTexture) * (1.0 - i / numLayers);
                shadowMultiplier = max(shadowMultiplier, newShadowMultiplier);
            }

            // offset to the next layer
            currentLayerHeight -= layerHeight;
            currentTexCoords += texStep;
            depthFromTexture = 1.0 - MaterialTex.Sample(AnisoWrapSampler, currentTexCoords).a;
        }
        
        // Shadowing factor should be 1 if there were no points under the surface
        if (numSamplesUnderSurface < 1) {
            shadowMultiplier = 1;
        }
        else {
            shadowMultiplier = 1.0 - shadowMultiplier;
        }
    }

    return shadowMultiplier;
}

float4 main(PixelInputType i) : SV_TARGET {
    // POM
    if (ParallaxMagnitude != 0) {
        // offset texture coordinates with Parallax Mapping
        i.uv = ParallaxMapping(i.uv, normalize(i.tangentViewDirection));
    }
    
    /// TODO: try just trilinear filtering for non albedo channels (suggested by Valve)
    const float3 albedo = AlbedoTex.Sample(AnisoWrapSampler, i.uv).rgb;
    const float ao = MaterialTex.Sample(AnisoWrapSampler, i.uv).r;
    const float metallic = MaterialTex.Sample(AnisoWrapSampler, i.uv).g;
    /// TODO: put this in CB
    float minRoughness = 0.00;
    const float roughness = max(minRoughness, MaterialTex.Sample(AnisoWrapSampler, i.uv).b);

    // Normal preprocess
    const float3 normalMap = NormalTex.Sample(AnisoWrapSampler, i.uv).xyz * 2.0 - 1.0;
    float3x3 TBN = transpose(float3x3(i.tangent, i.bitangent, i.normal));
    const float3 normal = normalize(mul(TBN, normalMap));
    
    // Values used throughout shader
    const float3 viewDirection = normalize(i.cameraPosition.xyz - i.worldPosition.xyz);
    const float NdotV = max(dot(normal, viewDirection), 0.0);
    // calculate reflectance at normal incidence; if dia-electric (like plastic) use F0 
    // of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)    
    const float3 F0 = lerp(0.04, albedo, metallic);

/// CALCULATE PBR DIRECT LIGHTING (LO)
    const float3 dirLightLo = CalcReflectanceFromLight(-DirLight.xyz, DirLightColor.rgb, albedo, metallic, F0, roughness, normal, viewDirection, NdotV);
     
    /// TODO: TEMP
    float3 spotLightPos = float3(0, 0, 0);
    float3 spotDir = spotLightPos - i.worldPosition.xyz;
    float spotLightDist = length(spotDir);
    float attenuation = 1.0 / (1.0 + 0.09 * spotLightDist + 0.032 * (spotLightDist * spotLightDist));
    float3 radiance = float3(10, 0, 0);
    /// END TEMP
    const float3 spotLightLo = CalcReflectanceFromLight(spotDir, radiance * attenuation, albedo, metallic, F0, roughness, normal, viewDirection, NdotV);
    //const float3 spotLightLo = 0;
/// END CALCULATE PBR DIRECT LIGHTING (LO)
    
/// IBL AMBIENT LIGHTING
    const float3 R = reflect(-viewDirection, normal);

    // Sample precalculated environment maps
    const float3 irradianceMap = IrradianceCubemap.Sample(AnisoWrapSampler, normal).xyz;
    const float3 prefilterMap = PrefilterCubemap.SampleLevel(AnisoWrapSampler, R, roughness * MAX_REFLECTION_LOD).xyz;
    const float2 envBRDF = BRDFLut.Sample(TrilinearClampSampler, float2(NdotV, roughness)).rg;
    
    const float3 indirect_kS = FresnelSchlickRoughness(NdotV, F0, roughness);
    const float3 indirect_kD = (1.0 - indirect_kS) * (1.0 - metallic);
    
    const float3 ambientDiffuse = indirect_kD * irradianceMap * albedo;
    const float3 ambientIndirectSpecular = prefilterMap * (indirect_kS * envBRDF.x + envBRDF.y);

    const float3 ambient = (ambientDiffuse + ambientIndirectSpecular) * ao;
/// END IBL AMBIENT LIGHTING
    
/// CALCULATE SHADOW (only directional Light for now)
    // Calculate the projected texture coordinates.
    // use screen coord of vertex position with directional light's view/projection, rescaled to [0,1]
    const float3 normalizedDirectionalLightViewPos = (i.directionalLightViewPosition.xyz / i.directionalLightViewPosition.w);
    const float2 projectTexCoord = float2(normalizedDirectionalLightViewPos.x, -normalizedDirectionalLightViewPos.y) * 0.5 + 0.5;
    float lightDepthValue = normalizedDirectionalLightViewPos.z;
        
    /// TODO: apply bias
    // Adaptive shadow bias
    //float shadowBias = max(0.05 * (1.0 - dot(normal, -DirLight.xyz)), 0.005);
    //lightDepthValue = lightDepthValue - shadowBias;
    
    // Directional light shadowmap with basic PCF multisampling
    float dirLightShadowFactor = 0.0; // 0: in shadow, 1: not in shadow
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            dirLightShadowFactor += DirectionalShadowMap.SampleCmpLevelZero(TrilinearBorderSampler, projectTexCoord, lightDepthValue, float2(x, y));
        }
    }
    dirLightShadowFactor /= 9.0;
    
    if (lightDepthValue > 1.0)
        dirLightShadowFactor = 1.0;
    
    // EXPERIMENTAL - Parallax occlusion self shadowing
    if (ParallaxMagnitude != 0 && UseParallaxShadow != 0) {
        /// TODO: make power factor tweakable
        // Power factor added as a hacky way to make shadows more visible
        const float parallaxSelfShadowFactor = pow(CalcParallaxSoftShadowMultiplier(normalize(mul(-DirLight.xyz, TBN)), i.uv, 1.0 - MaterialTex.Sample(AnisoWrapSampler, i.uv).a), 16.0);
        // Note: pow above causes invalid values sometimes (blows up bloom effect), this seems to only happen on specific materials
        // saturate() ensures valid values
        dirLightShadowFactor *= saturate(parallaxSelfShadowFactor);
    }
/// END CALCULATE SHADOW 
    
    return float4(ambient + spotLightLo + dirLightLo * dirLightShadowFactor, 1);
}
