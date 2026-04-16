#include "BloomPass.h"
#include "Device.h"
#include "Texture.h"
#include "RenderTarget.h"
#include "CommandList.h"
#include "BloomPSO.h"

#include <format>

BloomPass::BloomPass(Device& device, const RenderTarget& screenRenderTarget, BloomPSO* pso, int maxIterations) :
	m_IterationCount{maxIterations},
	m_PSO{pso}
{
	uint32_t textureWidth  = screenRenderTarget.GetTexture(AttachmentPoint::Color0)->GetWidth();
	uint32_t textureHeight = screenRenderTarget.GetTexture(AttachmentPoint::Color0)->GetHeight();
	DXGI_FORMAT screenTextureFormat = screenRenderTarget.GetRenderTargetFormats().RTFormats[AttachmentPoint::Color0];
	DXGI_SAMPLE_DESC screenSampleDesc = screenRenderTarget.GetSampleDesc();

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = screenTextureFormat;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = -1;

	// Create render targets
	{
		auto textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			screenTextureFormat, textureWidth, textureHeight,
			1, 1, screenSampleDesc.Count, screenSampleDesc.Quality, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
		);

		auto bloomOutputTexture = std::make_shared<Texture>(device, textureDesc, nullptr, false);
		bloomOutputTexture->SetName(L"Bloom Output");

		m_BloomOutputRT.AttachTexture(AttachmentPoint::Color0, bloomOutputTexture);
		bloomOutputTexture->CreateShaderResourceView(srvDesc);

		// Intermediate Render Targets
		m_SamplingRenderTargets.reserve(maxIterations);
		for(size_t i = 0; i < maxIterations; i++) {
			textureWidth /= 2;
			textureHeight /= 2;

			m_SamplingRenderTargets.emplace_back();

			auto sampleTextureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
				screenTextureFormat, textureWidth, textureHeight,
				1, 1, screenSampleDesc.Count, screenSampleDesc.Quality, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
			);

			auto samplingTexture = std::make_shared<Texture>(device, sampleTextureDesc, nullptr, false);
			samplingTexture->SetName(std::format(L"Sample Texture {}", i));
			m_SamplingRenderTargets[i].AttachTexture(AttachmentPoint::Color0, samplingTexture);
			samplingTexture->CreateShaderResourceView(srvDesc);

			// Assume height is less than width
			if(textureHeight < 2) {
				m_IterationCount = (int)i + 1;
				break;
			}
		}
	}
}

void BloomPass::Render(CommandList& directCommandList, const RenderTarget& screenRenderTarget) {
	m_PSO->SetPipelineState(directCommandList);

	float clearColor[] = { 1.0f, 0.0f, 1.0f, 1.0f };

	// First downsample + prefilter
	// Note: bloomParams - x: boxSampleDelta, y: intensity, z: usePrefilter [0, 1], w: useFinalPass [0, 1]
	BloomPSO::BloomProps bloomProps {};
	float knee = m_Threshold * m_SoftThreshold;
	bloomProps.filter = { m_Threshold, m_Threshold - knee, 2.0f * knee, 0.25f / (knee + 0.00001f) };
	bloomProps.boxSampleDelta = 1.0f;
	bloomProps.intensity      = m_Intensity;
	bloomProps.usePrefilter   = 1.0f;
	bloomProps.useFinalPass   = 0.0f;

	directCommandList.SetGraphicsDynamicConstantBuffer(BloomPSO::BloomRootParameters::BloomCB, bloomProps);
	directCommandList.SetShaderResourceView(BloomPSO::BloomRootParameters::Textures, 0, screenRenderTarget.GetTexture(AttachmentPoint::Color0));

	directCommandList.ClearTexture(m_SamplingRenderTargets[0].GetTexture(AttachmentPoint::Color0), clearColor);
	directCommandList.SetRenderTarget(m_SamplingRenderTargets[0]);
	directCommandList.SetViewport(m_SamplingRenderTargets[0].GetViewport());
	directCommandList.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	directCommandList.Draw(3);


}

//bool Bloom::RenderEffect(D3DInstance* d3dInstance, int indexCount, XMMATRIX worldMatrix, XMMATRIX viewMatrix, XMMATRIX projectionMatrix, ID3D11ShaderResourceView* screenTextureSource) {
//	bool result {};
//
//	static ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
//
//	// First downsample + prefilter
//	d3dInstance->DisableAlphaBlending();
//	mb_UsePrefilter = true;
//	m_BoxSampleDelta = 1.0f;
//
//	m_RenderTexures[0]->SetRenderTargetAndViewPort();
//	m_RenderTexures[0]->ClearRenderTarget(1.0f, 0.0f, 0.0f, 1.0f);
//	result = Render(d3dInstance->GetDeviceContext(), indexCount, worldMatrix, viewMatrix, projectionMatrix, screenTextureSource, false);
//
//	if(!result) return false;
//
//	// Progressive Downsampling
//	mb_UsePrefilter = false;
//	int i = 1;
//	for(; i < m_IterationCount; i++) {
//		m_RenderTexures[i]->SetRenderTargetAndViewPort();
//		m_RenderTexures[i]->ClearRenderTarget(1.0f, 0.0f, 0.0f, 1.0f);
//		result = Render(d3dInstance->GetDeviceContext(), indexCount, worldMatrix, viewMatrix, projectionMatrix, m_RenderTexures[i - 1]->GetTextureSRV(), false);
//		d3dInstance->GetDeviceContext()->PSSetShaderResources(0, 1, nullSRV);
//
//		if(!result) return false;
//	}
//
//	// Progressive Upsampling
//	d3dInstance->EnableAdditiveBlending();
//	m_BoxSampleDelta = 0.5f;
//	for(i -= 2; i >= 0; i--) {
//		m_RenderTexures[i]->SetRenderTargetAndViewPort();
//		result = Render(d3dInstance->GetDeviceContext(), indexCount, worldMatrix, viewMatrix, projectionMatrix, m_RenderTexures[i + 1]->GetTextureSRV(), false);
//		d3dInstance->GetDeviceContext()->PSSetShaderResources(0, 1, nullSRV);
//
//		if(!result) return false;
//	}
//
//	d3dInstance->EnableAlphaBlending();
//	m_BloomOutputTexture->SetRenderTargetAndViewPort();
//	m_BloomOutputTexture->ClearRenderTarget(1.0f, 0.0f, 0.0f, 1.0f);
//	// Bind original screen render image
//	d3dInstance->GetDeviceContext()->PSSetShaderResources(1, 1, &screenTextureSource);
//	result = Render(d3dInstance->GetDeviceContext(), indexCount, worldMatrix, viewMatrix, projectionMatrix, m_RenderTexures[0]->GetTextureSRV(), true);
//	if(!result) return false;
//
//	return true;
//}
//
//bool Bloom::Render(ID3D11DeviceContext* deviceContext, int indexCount, XMMATRIX worldMatrix, XMMATRIX viewMatrix, XMMATRIX projectionMatrix, ID3D11ShaderResourceView* textureSRV, bool b_IsFinalPass) const {
//	// Transpose the matrices to prepare them for the shader.
//	worldMatrix = XMMatrixTranspose(worldMatrix);
//	viewMatrix = XMMatrixTranspose(viewMatrix);
//	projectionMatrix = XMMatrixTranspose(projectionMatrix);
//
//	/// Write to matrix buffer
//	D3D11_MAPPED_SUBRESOURCE mappedResource {};
//	HRESULT result = deviceContext->Map(m_MatrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
//	if(FAILED(result)) {
//		return false;
//	}
//
//	MatrixBufferType* matrixDataPtr = (MatrixBufferType*)mappedResource.pData;
//
//	matrixDataPtr->world = worldMatrix;
//	matrixDataPtr->view = viewMatrix;
//	matrixDataPtr->projection = projectionMatrix;
//
//	deviceContext->Unmap(m_MatrixBuffer, 0);
//
//	deviceContext->VSSetConstantBuffers(0, 1, &m_MatrixBuffer);
//
//	/// Write to shader parameters buffer
//	result = deviceContext->Map(m_BloomParamBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
//	if(FAILED(result)) return false;
//
//	BloomParamBufferType* bloomDataPtr = (BloomParamBufferType*)mappedResource.pData;
//
//	float knee = m_Threshold * m_SoftThreshold;
//	XMFLOAT4 filter { m_Threshold, m_Threshold - knee, 2.0f * knee, 0.25f / (knee + 0.00001f) };
//	bloomDataPtr->filter = filter;
//	bloomDataPtr->boxSampleDelta = m_BoxSampleDelta;
//	bloomDataPtr->intensity = m_Intensity;
//	bloomDataPtr->b_UsePrefilter = mb_UsePrefilter ? 1.0f : 0.0f;
//	bloomDataPtr->b_UseFinalPass = b_IsFinalPass ? 1.0f : 0.0f;
//
//	deviceContext->Unmap(m_BloomParamBuffer, 0);
//	deviceContext->PSSetConstantBuffers(0, 1, &m_BloomParamBuffer);
//
//	/// Bind Textures
//	deviceContext->PSSetShaderResources(0, 1, &textureSRV);
//
//	/// Render the prepared buffers with the shader
//	deviceContext->IASetInputLayout(m_screenShaderLayoutInstance);
//
//	deviceContext->VSSetShader(m_ScreenVertexShaderInstance, NULL, 0);
//	deviceContext->PSSetShader(m_PixelShader, NULL, 0);
//
//	deviceContext->HSSetShader(NULL, NULL, 0);
//	deviceContext->DSSetShader(NULL, NULL, 0);
//
//	deviceContext->PSSetSamplers(0, 1, &m_SampleState);
//
//	deviceContext->DrawIndexed(indexCount, 0, 0);
//
//	return true;
//}
