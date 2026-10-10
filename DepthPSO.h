#pragma once

// PSO for shadow map depth rendering. Hardcoded to PBR pipeline for now.

#include "d3d12.h"

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

	void SetPipelineState(CommandList& directCommandList, bool b_ModifiedDepth = false) const;

private:
    std::shared_ptr<RootSignature> m_RootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PSO;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PointLightPSO;
};

