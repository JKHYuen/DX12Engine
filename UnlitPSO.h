#pragma once

// PSO for rendering meshes using the PBR pipeline with one color unlit shading
// This and other "PSO" classes are ad hoc for the PBR pipeline and a more generalized implementation will be needed in the future.

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
struct PBRTessellationProps;

class UnlitPSO {
public:
	UnlitPSO(Device& device, D3D12_RT_FORMAT_ARRAY rtvFormats, std::shared_ptr<RootSignature> objectRootSignature);

	void SetPipelineState(CommandList& directCommandList) const;
	void SetWireframePipelineState(CommandList& directCommandList) const;

	void UpdateResources(CommandList& directCommandList, const PBRVertexProps& vertexProps, const PBRTessellationProps& tessProps) const;

private:
	std::shared_ptr<RootSignature> m_ObjectRootSignature;
	ComPtr<ID3D12PipelineState> m_PipelineState;
	ComPtr<ID3D12PipelineState> m_WireframePipelineState;
};

