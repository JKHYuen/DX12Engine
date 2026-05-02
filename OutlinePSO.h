#pragma once

/*
	PSO for outline effect (front cull hull shader).
	This and other "PSO" classes are ad hoc and a more generalized implementation will be needed in the future.
*/

#include <d3d12.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <memory>

#include "PBRObjectPSO.h"

using namespace DirectX;
using namespace Microsoft::WRL;

class Device;
class RootSignature;
class CommandList;

class OutlinePSO {
public:
	OutlinePSO(Device& device, D3D12_RT_FORMAT_ARRAY rtvFormats, DXGI_FORMAT depthFormat, std::shared_ptr<RootSignature> objectRootSignature);

	std::shared_ptr<RootSignature> GetRootSignature() const { return m_ObjectRootSignature; }

	void SetPipelineState(CommandList& directCommandList) const;

	void UpdateResources(CommandList& directCommandList, PBRObjectPSO::VertexProps vertexProps, PBRObjectPSO::LightProps lightProps);

private:
	std::shared_ptr<RootSignature> m_ObjectRootSignature;
	ComPtr<ID3D12PipelineState> m_D3d12PipelineState;
};

