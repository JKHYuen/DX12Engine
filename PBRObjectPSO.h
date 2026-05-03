#pragma once

/*
	PSO for rendering PBR objects.
	This and other "PSO" classes are ad hoc and a more generalized implementation will be needed in the future.
*/

#include <d3d12.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <memory>
#include <vector>

using namespace DirectX;
using namespace Microsoft::WRL;

class Device;
class RootSignature;
class RenderTarget;
class CommandList;
class Texture;

class PBRObjectPSO {
public:
	PBRObjectPSO(Device& device, DXGI_SAMPLE_DESC sampleDesc, D3D12_RT_FORMAT_ARRAY rtvFormat, DXGI_FORMAT depthFormat);

	enum PBRRootParameters {
		VertexCB,         // ConstantBuffer<VertexProps>   VertexCB	  : register(b0);
		MaterialCB,       // ConstantBuffer<MaterialProps> MaterialCB : register(b0, space1);
		LightCB,          // ConstantBuffer<LightProps>    LightCB    : register(b1);

		Textures,         // Texture2D AlbedoTex		 : register(t0);
						  // Texture2D NormalTex		 : register(t1);
						  // Texture2D MaterialTex		 : register(t2); [r: ao, g: metallic, b: roughness, a: height]
						  // Texture2D IrradianceCubemap : register(t3);
						  // Texture2D PrefilterCubemap	 : register(t4);
						  // Texture2D BRDFLut			 : register(t5);

		NumPBRRootParameters
	};

	/// Expected indices for "pbrTextures" param in UpdateResources() function
	// Static textures only
	enum TextureIndex {
        AlbedoTex,
		NormalTex,
		MaterialTex, // r: AO, g: metallic, b: roughness, a: height 
		IrradianceCubemap,
		PrefilterCubemap,
		BRDFLut,
		DirectionalShadowMap,

		NumTextures
	};

	struct alignas(16) VertexProps {
		XMFLOAT4X4 SRT;
		XMFLOAT4X4 MVP;
		XMFLOAT4X4 directionalLightMVP;
		XMFLOAT4   cameraPosition;
		XMFLOAT2   uvScale;
		float      heightMapMagnitude;
		float      pad1;
	};

	struct alignas(16) MaterialProps {
		float useParallaxShadow;
		float minParallaxLayers;
		float maxParallaxLayers;
		float directionalShadowBias;
		
		float parallaxMagnitude;
		float pad1;
		float pad2;
		float pad3;
	};

	struct alignas(16) LightProps {
		XMFLOAT4 Time; // x: time, y: delta time
		XMFLOAT4 dirLight;
		XMFLOAT4 dirLightColor;
	};

	std::shared_ptr<RootSignature> GetRootSignature() const {
		return m_RootSignature;
	}
		
	void SetPipelineState(CommandList& directCommandList) const;

	void UpdateResources(CommandList& directCommandList, const std::vector<std::shared_ptr<Texture>>& pbrTextures, VertexProps vertexProps, MaterialProps materialProps, LightProps lightProps);

private:
	std::shared_ptr<RootSignature> m_RootSignature;
	ComPtr<ID3D12PipelineState> m_D3d12PipelineState;
};

