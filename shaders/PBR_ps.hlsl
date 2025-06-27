cbuffer MaterialCB : register(b0, space1) {
    float4 Time;
    float3 DirLight;
    float4 Pad1;
    float4 Pad2;
    matrix Pad3;
    matrix Pad4;
    matrix Pad5;
};

Texture2D AlbedoTex    : register(t0);
Texture2D NormalTex    : register(t1);
Texture2D MaterialTex  : register(t2);

SamplerState AnisoSampler : register(s0);

struct PixelInputType {
    float4 position       : SV_Position;
    float3 normal         : NORMAL;
    float3 tangent        : TANGENT;
    float3 bitangent      : BITANGENT;
    float2 uv             : TEXCOORD0;
    float4 worldPosition  : TEXCOORD1;
    float3 cameraPosition : TEXCOORD2;
};

static const float PI = 3.14159265359;

// Normal distribution function
float DistributionGGX(float3 N, float3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
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


float4 main(PixelInputType i) : SV_Target {
    i.uv *= 2;
    
    float3 albedo   = AlbedoTex.Sample(AnisoSampler, i.uv).rgb;
    float ao        = MaterialTex.Sample(AnisoSampler, i.uv).r;
    float metallic  = MaterialTex.Sample(AnisoSampler, i.uv).g;
    float roughness = MaterialTex.Sample(AnisoSampler, i.uv).b;
    // Normal preprocess
    float3 normalMap = NormalTex.Sample(AnisoSampler, i.uv).xyz * 2.0 - 1.0;
    float3 normal = normalize((normalMap.x * i.tangent) + (normalMap.y * i.bitangent) + (normalMap.z * i.normal));
   
    //// BLINN PHONG TEST
    //// Diffuse
    //float3 L = -DirLight;
    //float diffuseMag = max(dot(normal, L), 0.0);
    //float3 diffuse = diffuseMag * float3(1, 1, 1);
    
    //// Specular
    //float3 viewDir = normalize(i.cameraPosition - i.worldPosition.xyz);
    //float3 H = normalize(viewDir + L);
    
    //float specMag = pow(max(dot(normal, H), 0.0), 64);
    //float3 specular = specMag * float3(1, 1, 1);
    
    ////float4 albedo = {0.7, 0.4, 0.3, 1};
   
    //float3 ambient = { 0.1, 0.1, 0.1 };
    //return float4(albedo.rgb * (ambient + diffuse + specular), 1.0);
    ////
    
///////////////////////////////////
/// Calculate PBR Radiance (Lo) ///
///////////////////////////////////
    
    float minRoughness = 0.1;
    roughness = max(minRoughness, roughness);
    
    float3 viewDirection = normalize(i.cameraPosition - i.worldPosition.xyz);
    
    // calculate reflectance at normal incidence; if dia-electric (like plastic) use F0 
    // of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)    
    float3 F0 = 0.04;
    F0 = lerp(F0, albedo, metallic);

    // reflectance equation
    float3 Lo = 0.0;

    // calculate per-light radiance (just one directional light for now)
    float3 L = -DirLight;
    float3 H = normalize(viewDirection + L);

    //float3 radiance = directionalLightColor.rgb;
    float3 radiance = {4.5, 4.0, 3.5};

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(normal, H, roughness);
    float G = GeometrySmith(normal, viewDirection, L, roughness);
    float3 F = FresnelSchlick(max(dot(viewDirection, H), 0.0), F0);
           
    float3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(normal, viewDirection), 0.0) * max(dot(normal, L), 0.0) + 0.0001;
    float3 specular = numerator / denominator;
    
    // Prevent artifacts from extremely bright pixels on perfectly smooth materials (bandaid fix)
    // This may cause some specular highilghts to be omitted (e.g. marble sphere from demo scene)
    //specular = clamp(specular, 0, 10000);
        
    // kS is equal to Fresnel
    float3 kS = F;
    // for energy conservation, the diffuse and specular light can't
    // be above 1.0 (unless the surface emits light); to preserve this
    // relationship the diffuse component (kD) should equal 1.0 - kS.
    float3 kD = 1.0 - kS;
    // multiply kD by the inverse metalness such that only non-metals 
    // have diffuse lighting, or a linear blend if partly metal (pure metals
    // have no diffuse light).
    kD *= 1.0 - metallic;

    // scale light by NdotL
    float NdotL = max(dot(normal, L), 0.0);
    // add to outgoing radiance Lo
    // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again
    Lo += (kD * albedo / PI + specular) * radiance * NdotL;

    // TODO: IBL
    float3 ambient = albedo * ao * 0.1;
    
    float4 retCol = float4(ambient + Lo, 1);
    
    return retCol;
}