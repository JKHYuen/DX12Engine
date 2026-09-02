#pragma once
#include <DirectXMath.h>
#include <memory>

using namespace DirectX;

class Mesh;
class UnlitPSO;
class CommandList;
class UpdateEventArgs;
class Camera;

class PointLight {
public:
	PointLight(XMFLOAT4 color, XMFLOAT3 translation, std::shared_ptr<Mesh> visualizationMesh, UnlitPSO* unlitPSO);

	void RenderMesh(CommandList& directCommandList, const UpdateEventArgs& e, const Camera& viewCamera);

	XMFLOAT4 GetColor() const { return m_Color; };
	void SetColor(XMFLOAT4 color) { m_Color = color; };

	XMFLOAT3 GetTranslation() const { return m_Translation; };
	void SetTranslation(XMFLOAT3 translation) { m_Translation = translation; };

private:	
	UnlitPSO* m_UnlitPSO;
	std::shared_ptr<Mesh> m_VisualizationMesh;
	XMFLOAT4 m_Color;
	XMFLOAT3 m_Translation;
};

