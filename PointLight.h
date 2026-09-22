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

class PointLight : public GameObject {
	friend class EditorGUI;

public:
	PointLight(const std::string& name, XMFLOAT3 translation, XMFLOAT3 color, float radius, std::shared_ptr<Mesh> visualizationMesh, UnlitPSO* unlitPSO);
	PointLight(XMFLOAT3 translation, XMFLOAT3 color, float radius, std::shared_ptr<Mesh> visualizationMesh, UnlitPSO* unlitPSO);

	void RenderMesh(CommandList& directCommandList, const UpdateEventArgs& e, const Camera& viewCamera);

	XMFLOAT3 GetColor() const { return m_Color; };
	void SetColor(XMFLOAT3 color) { m_Color = color; };

	float GetRadius() const { return m_Radius; };
	void SetRadius(float radius) { m_Radius = radius; };

private:	
	UnlitPSO* m_UnlitPSO;
	XMFLOAT3 m_Color;
	float m_Radius;

	std::shared_ptr<Mesh> m_VisualizationMesh;
	float m_VisualIntensity; // multiplier to color of visualization object, intended to make bloom less distracting
	float m_VisualMeshScale;
};

