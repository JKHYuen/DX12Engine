#pragma once
#include <DirectXMath.h>
#include <memory>

// This class should be a component of the GameObject class eventually, 
// to get functionality like AABBs and basic transform functions easily
// 
// Variable "m_Translation" refers to light world position, it is named this way to match GameObject class for now

using namespace DirectX;

class Mesh;
class UnlitPSO;
class CommandList;
class UpdateEventArgs;
class Camera;

class PointLight {
	friend class EditorGUI;

public:
	PointLight(XMFLOAT3 color, XMFLOAT3 translation, float radius, std::shared_ptr<Mesh> visualizationMesh, UnlitPSO* unlitPSO);

	void RenderMesh(CommandList& directCommandList, const UpdateEventArgs& e, const Camera& viewCamera);

	XMFLOAT3 GetColor() const { return m_Color; };
	void SetColor(XMFLOAT3 color) { m_Color = color; };

	XMFLOAT3 GetTranslation() const { return m_Translation; };
	void SetTranslation(XMFLOAT3 translation) { m_Translation = translation; };

	float GetRadius() const { return m_Radius; };
	void SetRadius(float radius) { m_Radius = radius; };

private:	
	UnlitPSO* m_UnlitPSO;
	std::shared_ptr<Mesh> m_VisualizationMesh;
	XMFLOAT3 m_Color;
	XMFLOAT3 m_Translation;
	float m_Radius;
	float m_VisualIntensity; // multiplier to color of visualization object, intended to make bloom less distracting
};

