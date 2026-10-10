#include "DepthPSO.h"

#include "DX12EngineCore/CommandList.h"
#include "DX12EngineCore/Device.h"
#include "DX12EngineCore/VertexInput.h"
#include "DX12EngineCore/RootSignature.h"

#include "AssetImporter.h"
#include "d3d12.h"
#include "dxgiformat.h"

#include <memory>
#include <d3dx12_pipeline_state_stream.h>

DepthPSO::DepthPSO(Device& device, std::shared_ptr<RootSignature> objectRootSignature) 
    : m_RootSignature(objectRootSignature)
{
    struct DepthPipelineStateStream {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE        pRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT          InputLayout;
        CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY    PrimitiveTopologyType;
        CD3DX12_PIPELINE_STATE_STREAM_VS                    VS;
        CD3DX12_PIPELINE_STATE_STREAM_HS                    HS;
        CD3DX12_PIPELINE_STATE_STREAM_DS                    DS;
        CD3DX12_PIPELINE_STATE_STREAM_PS                    PS;
        CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER            Rasterizer;
        CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT  DSVFormat;
    } depthPipelineStateStream;

    CD3DX12_RASTERIZER_DESC rasterizerDesc(D3D12_DEFAULT);
    rasterizerDesc.CullMode = D3D12_CULL_MODE_FRONT;

    depthPipelineStateStream.pRootSignature = objectRootSignature->GetD3D12RootSignature().Get();
    depthPipelineStateStream.InputLayout = VertexInput::Get_POS_NORM_TAN_BIT_UV_InputLayout();
    depthPipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
    depthPipelineStateStream.VS = AssetImporter::Get().GetCompiledShaderFromFile(L"PBR_VS.cso");
    depthPipelineStateStream.HS = AssetImporter::Get().GetCompiledShaderFromFile(L"PBR_HS.cso");
    depthPipelineStateStream.DS = AssetImporter::Get().GetCompiledShaderFromFile(L"PBR_DS.cso");
    depthPipelineStateStream.Rasterizer = rasterizerDesc;
    depthPipelineStateStream.DSVFormat = DXGI_FORMAT_D32_FLOAT;

    device.CreatePipelineState(depthPipelineStateStream, m_PSO);

    // Create seperate PSO for point light depth rendering that uses PS to modify depth
    depthPipelineStateStream.PS = AssetImporter::Get().GetCompiledShaderFromFile(L"PointLightShadowDepthWrite_PS.cso");
    device.CreatePipelineState(depthPipelineStateStream, m_PointLightPSO);
}

void DepthPSO::SetPipelineState(CommandList& directCommandList, bool b_PointLightShadowDepth) const {
    directCommandList.SetPipelineState(b_PointLightShadowDepth ? m_PointLightPSO : m_PSO);
    directCommandList.SetGraphicsRootSignature(m_RootSignature);
    directCommandList.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
}

