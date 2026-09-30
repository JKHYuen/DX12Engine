#pragma once

// PSO for shadow map depth rendering, hardcoded to PBR pipeline for now.

#include "d3d12.h"
#include "dxgiformat.h"

#include <wrl/client.h>
#include <memory>

class RootSignature;
class Device;
class CommandList;
struct PBRVertexProps;
struct PBRTessellationProps;

class DepthPSO {
public:
	DepthPSO(Device& device, std::shared_ptr<RootSignature> objectRootSignature);

	void SetPipelineState(CommandList& directCommandList) const;

	void UpdateResources(CommandList& directCommandList, const PBRVertexProps& vertexProps, const PBRTessellationProps& tessProps) const;

private:
    std::shared_ptr<RootSignature> m_RootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PSO;
};

