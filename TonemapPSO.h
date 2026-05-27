#pragma once

#include <memory>
#include <wrl/client.h>

using namespace Microsoft::WRL;

class Device;
class CommandList;
struct ID3D12PipelineState;
class RenderTarget;
class RootSignature;

class TonemapPSO {
public:
	TonemapPSO(Device& device, const RenderTarget& renderTarget);

	void SetPipelineState(CommandList& directCommandList) const;

private:
	ComPtr<ID3D12PipelineState> m_PipelineState;
	std::shared_ptr<RootSignature> m_RootSignature;
};

