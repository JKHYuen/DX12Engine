#include "PointLight.h"
#include <memory>
#include <DirectXMath.h>

using namespace DirectX;

PointLight::PointLight(XMFLOAT3 color, XMFLOAT3 translation, std::shared_ptr<Mesh> visualizationMesh, UnlitPSO* unlitPSO)
	: m_Color { color }
	, m_Translation { translation }
	, m_VisualizationMesh { visualizationMesh }
	, m_UnlitPSO { unlitPSO }
{

}
