// Cook-Torrance PBR based on https://learnopengl.com/PBR/Theory

cbuffer MaterialCB : register(b0, space1) {
    float UseParallaxShadow;
    float MinParallaxLayers;
    float MaxParallaxLayers;

    float DirectionalShadowBias;

    float ParallaxMagnitude;
};

cbuffer LightCB : register(b1) {
    float4 Time;
    float4 DirLight; // vector of directional light
    float4 DirLightColor;
    float4 OutlineColor;
};

Texture2D AlbedoTex                   : register(t0);
Texture2D NormalTex                   : register(t1);
Texture2D MaterialTex                 : register(t2); // [r: ao, g: metallic, b: roughness, a: height]
TextureCube<float4> IrradianceCubemap : register(t3);
TextureCube<float4> PrefilterCubemap  : register(t4);
Texture2D BRDFLut                     : register(t5);
Texture2D DirectionalShadowMap        : register(t6);

// TODO: directional shadow map should use border sampler (?)
SamplerState AnisoWrapSampler          : register(s0);
SamplerState TrilinearClampSampler     : register(s1); // for BRDF lut and directional shadow map

struct PixelInputType {
    float4 position                     : SV_POSITION;
    float3 tangent                      : TANGENT;
    float3 bitangent                    : BITANGENT;
    float3 normal                       : NORMAL0;
    float2 uv                           : TEXCOORD0;
    float4 worldPosition                : TEXCOORD1;
    float4 cameraPosition               : TEXCOORD2;
    float4 directionalLightViewPosition : TEXCOORD3;
    float3 tangentViewDirection         : TEXCOORD4;
};

static const float PI = 3.14159265359;

// Must change cubemap gen LOD count if this value is changed
static const float MAX_REFLECTION_LOD = 9.0;

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

float GeometrySmith(float3 N, float3 V, float3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

float3 FresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (1.0 - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness) {
    return F0 + (max(1.0 - roughness, F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
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
    float3 albedo   = AlbedoTex.Sample(AnisoWrapSampler, i.uv).rgb;
    float ao        = MaterialTex.Sample(AnisoWrapSampler, i.uv).r;
    float metallic  = MaterialTex.Sample(AnisoWrapSampler, i.uv).g;
    float roughness = MaterialTex.Sample(AnisoWrapSampler, i.uv).b;
    
    // Normal preprocess
    float3 normalMap = NormalTex.Sample(AnisoWrapSampler, i.uv).xyz * 2.0 - 1.0;
    float3 normal    = normalize((normalMap.x * i.tangent) + (normalMap.y * i.bitangent) + (normalMap.z * i.normal));
    
//
// Calculate PBR Direct Lighting (Lo)
// Note: Currently only one directional light
//
    // TODO: put this in CB
    float minRoughness = 0.00;
    roughness = max(minRoughness, roughness);
    
    float3 viewDirection = normalize(i.cameraPosition.xyz - i.worldPosition.xyz);
    
    // calculate reflectance at normal incidence; if dia-electric (like plastic) use F0 
    // of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)    
    float3 F0 = 0.04;
    F0 = lerp(F0, albedo, metallic);

    // reflectance equation
    float3 Lo = 0.0;

    // calculate per-light radiance (just one directional light for now)
    const float3 L = -DirLight.xyz;
    float3 H = normalize(viewDirection + L);

    float3 radiance = DirLightColor.rgb;

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(normal, H, roughness);
    float G = GeometrySmith(normal, viewDirection, L, roughness);
    float3 F = FresnelSchlick(max(dot(viewDirection, H), 0.0), F0);
           
    float3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(normal, viewDirection), 0.0) * max(dot(normal, L), 0.0) + 0.0001;
    // Currently only from directional light
    float3 specular = numerator / denominator;
    
    // (Bandaid Fix) Prevent artifacts from extremely bright pixels on perfectly smooth materials
    // This may cause some specular highilghts to be omitted (e.g. marble sphere from demo scene)
    //specular = clamp(specular, 0, 10000);
    
    // for energy conservation, the diffuse and specular light can't
    // be above 1.0 (unless the surface emits light); to preserve this
    // relationship the diffuse component (kD) should equal 1.0 - F (specular is Fresnel-Schlick term).
    float3 kD = 1.0 - F;
    kD *= 1.0 - metallic;
    
    // scale light by NdotL
    float NdotL = max(dot(normal, L), 0.0);
    // add to outgoing radiance Lo
    // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again
    Lo += (kD * albedo / PI + specular) * radiance * NdotL;

    
//
//  IBL Ambient Lighting
//
    float3 R = reflect(-viewDirection, normal);

    // Sample precalculated environment maps
    float3 irradianceMap = IrradianceCubemap.Sample(AnisoWrapSampler, normal).xyz;
    float3 prefilterMap = PrefilterCubemap.SampleLevel(AnisoWrapSampler, R, roughness * MAX_REFLECTION_LOD).xyz;
    float2 envBRDF = BRDFLut.Sample(TrilinearClampSampler, float2(max(dot(normal, viewDirection), 0.0), roughness)).rg;
    
    float3 indirect_kS = FresnelSchlickRoughness(max(dot(normal, viewDirection), 0.0), F0, roughness);
    float3 indirect_kD = 1.0 - indirect_kS;
    indirect_kD *= 1.0 - metallic;
    
    float3 diffuse = irradianceMap * albedo;
    float3 indirectSpecular = prefilterMap * (F * envBRDF.x + envBRDF.y);

    float3 ambient = (indirect_kD * diffuse + indirectSpecular) * ao;
    
//
// Calculate Shadow
//
    // Calculate the projected texture coordinates.
    // use screen coord of vertex position with directional light's view/projection, rescaled tp [0,1]
    float3 normalizedDirectionalLightViewPos = (i.directionalLightViewPosition.xyz / i.directionalLightViewPosition.w);
    float2 projectTexCoord = float2(normalizedDirectionalLightViewPos.x, -normalizedDirectionalLightViewPos.y) * 0.5 + 0.5;
    float lightDepthValue = normalizedDirectionalLightViewPos.z;
        
    // Adaptive shadow bias
    //float shadowBias = max(0.05 * (1.0 - dot(normal, -lightDirection)), 0.005);
    
    // apply bias
    //lightDepthValue = lightDepthValue - shadowBias;
    lightDepthValue = lightDepthValue - 0.001;
        
    // Shadowmap with basic PCF multisampling
    float shadowFactor = 0.0; // 0: in shadow, 1: not in shadow
    
    float width, height, numOfLevels;
    DirectionalShadowMap.GetDimensions(0, width, height, numOfLevels);
    float2 texelSize = 1.0 / width;
    
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            // TODO: use border sampler
            float pcfDepth = DirectionalShadowMap.Sample(TrilinearClampSampler, projectTexCoord + float2(x, y) * texelSize).r;
            shadowFactor += lightDepthValue > pcfDepth ? 0.0 : 1.0;
        }
    }
    shadowFactor /= 9.0;
    
    if (lightDepthValue > 1.0)
        shadowFactor = 1.0;
    
    // EXPERIMENTAL - Parallax occlusion self shadowing
    if (ParallaxMagnitude != 0 && UseParallaxShadow != 0) {
        float3x3 TBN = transpose(float3x3(i.tangent, i.bitangent, i.normal));
        /// TODO: make power factor tweakable
        // Power factor added as a hacky way to make shadows more visible
        float parallaxSelfShadowFactor = pow(CalcParallaxSoftShadowMultiplier(mul(L, TBN), i.uv, 1.0 - MaterialTex.Sample(AnisoWrapSampler, i.uv).a), 16.0);
        shadowFactor *= parallaxSelfShadowFactor;
    }
    
    return float4(ambient + Lo * shadowFactor, 1);
}
