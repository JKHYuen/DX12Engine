#include "Picker.h"
#include "Scene.h"
#include "GameObject.h"
#include <DirectXMath.h>

using namespace DirectX;

Picker::Picker(Scene* scene) : m_Scene(scene) {}

// Source: based on https://www.rastertek.com/dx11win10tut47.html
bool Picker::Raycast(int mouseX, int mouseY, int windowWidth, int windowHeight) {
	// Move the mouse cursor coordinates into the -1 to +1 range.
	float pointX = ((2.0f * (float)mouseX) / (float)windowWidth) - 1.0f;
	float pointY = (((2.0f * (float)mouseY) / (float)windowHeight) - 1.0f) * -1.0f;

	// Adjust the points using the projection matrix to account for the aspect ratio of the viewport.
	XMMATRIX projectionMatrix = m_Scene->m_MainCamera.Get_ProjectionMatrix();
	XMFLOAT4X4 pMatrix {};
	XMStoreFloat4x4(&pMatrix, projectionMatrix);
	pointX = pointX / pMatrix._11;
	pointY = pointY / pMatrix._22;

	// Get the inverse of the view matrix.
	XMMATRIX viewMatrix = m_Scene->m_MainCamera.Get_ViewMatrix();
	XMMATRIX inverseViewMatrix = XMMatrixInverse(nullptr, viewMatrix);
	XMFLOAT4X4 iViewMatrix {};
	XMStoreFloat4x4(&iViewMatrix, inverseViewMatrix);

	// Calculate the direction of the picking ray in view space.
	XMFLOAT3 cameraDirection {};
	cameraDirection.x = (pointX * iViewMatrix._11) + (pointY * iViewMatrix._21) + iViewMatrix._31;
	cameraDirection.y = (pointX * iViewMatrix._12) + (pointY * iViewMatrix._22) + iViewMatrix._32;
	cameraDirection.z = (pointX * iViewMatrix._13) + (pointY * iViewMatrix._23) + iViewMatrix._33;
	XMVECTOR direction = XMLoadFloat3(&cameraDirection);

	// Get the origin of the picking ray which is the position of the camera.
	XMVECTOR origin = m_Scene->m_MainCamera.Get_Translation();

	// Get the inverse world matrix of the object.
	XMMATRIX worldMatrix = XMMatrixTranslation(0.0f, 0.0f, 0.0f);
	XMMATRIX inverseWorldMatrix = XMMatrixInverse(nullptr, worldMatrix);

	// Now transform the ray origin and the ray direction from view space to world space.
	XMVECTOR rayOrigin = XMVector3TransformCoord(origin, inverseWorldMatrix);
	XMVECTOR rayDirection = XMVector3TransformNormal(direction, inverseWorldMatrix);

	// Normalize the ray direction.
	rayDirection = XMVector3Normalize(rayDirection);

	bool b_RayCastHit = false;
	float rayCastHitDistance = 0.0f;
	for(auto& go : m_Scene->m_SceneObjects) {
		b_RayCastHit = b_RayCastHit || go.GetAABB().Intersects(origin, rayDirection, rayCastHitDistance);
		if(b_RayCastHit) {
			m_PickedObject = &go;
			/// TODO: move this somewhere that makes sense
			m_PickedObject->UpdateAABB();
			break;
		}
	}

	return b_RayCastHit;
}
