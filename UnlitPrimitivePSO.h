#pragma once

// PSO for rendering primitive meshes with one color unlit shading
// Using the PBR pipeline root sig for now since Unlit_PS uses existing cb to change color

#include <d3d12.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <memory>

using namespace DirectX;
using namespace Microsoft::WRL;

class Device;
class RootSignature;
class CommandList;
struct PBRVertexProps;

class UnlitPrimitivePSO {
public:
	UnlitPrimitivePSO(Device& device, D3D12_RT_FORMAT_ARRAY rtvFormats, std::shared_ptr<RootSignature> rootSignature);

	void SetPipelineState(CommandList& directCommandList) const;
	void SetWireframePipelineState(CommandList& directCommandList) const;

	void UpdateResources(CommandList& directCommandList, const PBRVertexProps& vertexProps) const;

private:
	std::shared_ptr<RootSignature> m_ObjectRootSignature;
	ComPtr<ID3D12PipelineState> m_PipelineState;
	ComPtr<ID3D12PipelineState> m_WireframePipelineState;
};

