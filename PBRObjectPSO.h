#pragma once
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
class ShaderResourceView;

class PBRObjectPSO {
public:
	enum PBRRootParameters {
		VertexCB,         // ConstantBuffer<Mat> VertexCB : register(b0);
		MaterialCB,       // ConstantBuffer<Material> MaterialCB : register( b0, space1 );
		Textures,         // Texture2D AlbedoTex            : register(t0);
						  // Texture2D NormalTex            : register(t1);
						  // Texture2D MaterialTex          : register(t2);
						  // Texture2D IrradianceCubemap    : register(t3);
						  // Texture2D PrefilterCubemap     : register(t4);
						  // Texture2D BRDFLut              : register(t5);
		VolatileTextures, // Texture2D DirectionalShadowMap : register(t6);		

		NumPBRRootParameters
	};

	enum PBRTextures {
        AlbedoTex,
		NormalTex,
		MaterialTex, // r: AO, g: metallic, b: roughness, a: height 
		IrradianceCubemap,
		PrefilterCubemap,
		BRDFLut,
		DirectionalShadowMap,		

		NumPBRTextures
	};

	struct VertexProps {
		XMFLOAT4X4 SRT;
		XMFLOAT4X4 MVP;
		XMFLOAT4X4 directionalLightMVP;
		XMFLOAT4   CameraPosition;
		XMFLOAT4   Pad1;
		XMFLOAT4   Pad2;
		XMFLOAT4   Pad3;
	};

	struct MaterialProps {
		XMFLOAT4   Time;
		XMFLOAT4   DirLight;
		XMFLOAT4   DirLightColor;
		XMFLOAT4   Pad2;
		XMFLOAT4X4 Pad3;
		XMFLOAT4X4 Pad4;
		XMFLOAT4X4 Pad5;
	};

	PBRObjectPSO(Device& device, DXGI_SAMPLE_DESC sampleDesc, D3D12_RT_FORMAT_ARRAY rtvFormat, DXGI_FORMAT depthFormat);

	std::shared_ptr<RootSignature> GetRootSignature() const {
		return m_RootSignature;
	}

	ComPtr<ID3D12PipelineState> GetD3D12PipelineState() const {
		return m_D3d12PipelineState;
	}
		
	void SetPipelineState(CommandList& directCommandList, const RenderTarget& renderTarget, D3D12_VIEWPORT viewPort, D3D12_RECT scissorRect, float clearColor[]) const;

	void UpdateResources(CommandList& directCommandList, const std::vector<std::shared_ptr<ShaderResourceView>>& pbrTextures, VertexProps vertexProps, MaterialProps materialProps);

private:
	std::shared_ptr<RootSignature> m_RootSignature;
	ComPtr<ID3D12PipelineState> m_D3d12PipelineState;
};

