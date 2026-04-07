#pragma once

// This class manages and creates all PSOs used for image based lighting - currently just static lighting calculated from skybox as cubemaps

#include <d3d12.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <memory>

using namespace DirectX;
using namespace Microsoft::WRL;

class Device;
class Mesh;
class RootSignature;
class CommandList;
class RenderTarget;

class ImageBasedLightingPSO {
public:
	enum IBLRenderType {
		Skybox,
		Convolution,
		Prefilter,
		BRDF_LUT,

		NumIBLRenderType
	};

	ImageBasedLightingPSO(Device& device, const RenderTarget& renderTarget);

	//enum OutlineRootParameters {
	//	VertexCB,         // ConstantBuffer<Mat> VertexCB		 : register(b0);
	//	MaterialCB,       // ConstantBuffer<Material> MaterialCB : register(b0, space1);
	//	NumOutlineRootParameters
	//};

	//struct VertexProps {
	//	XMFLOAT4X4 SRT;
	//	XMFLOAT4X4 MVP;
	//	XMFLOAT4   screenParams;
	//	XMFLOAT4X4 Pad1;
	//	XMFLOAT4   Pad2;
	//	XMFLOAT4   Pad3;
	//	XMFLOAT4   Pad4;
	//};

	//struct MaterialProps {
	//	XMFLOAT4   outlineColor;
	//	XMFLOAT4   Pad1;
	//	XMFLOAT4   Pad2;
	//	XMFLOAT4   Pad3;
	//	XMFLOAT4X4 Pad4;
	//	XMFLOAT4X4 Pad5;
	//	XMFLOAT4X4 Pad6;
	//};

	//void UpdateResources(CommandList& directCommandList, VertexProps vertexProps, MaterialProps materialProps);

	std::shared_ptr<RootSignature> GetRootSignature() const { return m_RootSignature; }

	void SetPipelineState(CommandList& directCommandList) const;

	ComPtr<ID3D12PipelineState> GetPSO(IBLRenderType renderType) const {
		switch(renderType) {
		case Skybox:
			return m_SkyboxPSO;
		case Convolution:
			return m_ConvolutionPSO;
		case Prefilter:
			return m_PrefilterPSO;
		case BRDF_LUT:
			return m_BRDF_LUT_PSO;
		default:
			throw std::exception("Invalid IBLRenderType.");
			break;
		}
	}

	std::shared_ptr<RootSignature> GetRootSignature(IBLRenderType renderType) const {
		switch(renderType) {
		case Skybox:
		case Convolution:
			return m_SkyboxRootSignature;
		case Prefilter:
			return m_PrefilterRootSignature;
		case BRDF_LUT:
			return m_BRDF_LUT_RootSignature;
		default:
			throw std::exception("Invalid IBLRenderType.");
			break;
		}
	}



private:
	std::shared_ptr<RootSignature> m_RootSignature;
	ComPtr<ID3D12PipelineState> m_D3d12PipelineState;

	ComPtr<ID3D12PipelineState> m_SkyboxPSO;
	ComPtr<ID3D12PipelineState> m_ConvolutionPSO;
	ComPtr<ID3D12PipelineState> m_PrefilterPSO;
	ComPtr<ID3D12PipelineState> m_BRDF_LUT_PSO;

	std::shared_ptr<RootSignature> m_SkyboxRootSignature; // used by skybox render and irradiance convolution
	std::shared_ptr<RootSignature> m_BRDF_LUT_RootSignature;
	std::shared_ptr<RootSignature> m_PrefilterRootSignature;
};

