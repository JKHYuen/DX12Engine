#pragma once
#include "GameObject.h" // Base Class

#include <DirectXMath.h>
#include <memory>
#include <string>

// This class should be a component of the GameObject class eventually, 
// to get functionality like AABBs and basic transform functions easily.
// 
// Variables prefixed "visual" refers to visual mesh to represent light in scene editing, not an actual in scene object,
// visual mesh is colored the same as the light.

using namespace DirectX;

class Mesh;
class UnlitPSO;
class CommandList;
class UpdateEventArgs;
class Camera;
class DepthPSO;
class RenderTarget;

class PointLight : public GameObject {

	friend class EditorGui;

public:
	struct PointLightParams {
		XMFLOAT3 translation;
		XMFLOAT3 color;
		float radius;
		std::shared_ptr<Mesh> visualizationMesh;
		UnlitPSO* unlitPSO;
		DepthPSO* depthPSO;
	};

	PointLight(Device& device, const std::string& name, PointLightParams params);
	PointLight(Device& device, PointLightParams params);

	void RenderMesh(CommandList& directCommandList, const UpdateEventArgs& e, const Camera& viewCamera);

	void SetShadowDepthPipelineStateAndRenderTarget(CommandList& directCommandList) const;
	// pass vertexProps by value to copy and edit MVP to render from light's perspective
	void RenderObjectToDepth(CommandList& directCommandList, Mesh& mesh, PBRVertexProps vertexProps, const PBRTessellationProps& tessProps) const;

	XMFLOAT3 GetColor() const { return m_Color; };
	void SetColor(float r, float g, float b) { m_Color = XMFLOAT3(r, g, b); };

	float GetRadius() const { return m_Radius; };
	void SetRadius(float radius) { m_Radius = radius; };

private:	
	std::unique_ptr<RenderTarget> m_ShadowCubemap_RT;

	XMFLOAT3 m_Color;
	float m_Radius;

	std::shared_ptr<Mesh> m_VisualizationMesh;
	float m_VisualIntensity; // multiplier to color of visualization object, intended to make bloom less distracting
	float m_VisualMeshScale;

	UnlitPSO* m_UnlitPSO;
	DepthPSO* m_DepthPSO;
};

