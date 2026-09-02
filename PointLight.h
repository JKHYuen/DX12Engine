#pragma once
#include <DirectXMath.h>
#include <memory>

using namespace DirectX;

class Mesh;
class UnlitPSO;

class PointLight {
public:
	PointLight(XMFLOAT3 color, XMFLOAT3 translation, std::shared_ptr<Mesh> visualizationMesh, UnlitPSO* unlitPSO);

	void Render();

	XMFLOAT3 GetColor() const { return m_Color; };
	void SetColor(XMFLOAT3 color) { m_Color = color; };

	XMFLOAT3 GetTranslation() const { return m_Translation; };
	void SetTranslation(XMFLOAT3 translation) { m_Translation = translation; };

private:	
	UnlitPSO* m_UnlitPSO;
	std::shared_ptr<Mesh> m_VisualizationMesh;
	XMFLOAT3 m_Color;
	XMFLOAT3 m_Translation;
};

