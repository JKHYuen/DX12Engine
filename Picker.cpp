#include "Picker.h"
#include "Scene.h"
#include "GameObject.h"
#include "Logger.h"

#include <queue>
#include <algorithm>
#include <DirectXMath.h>

using namespace DirectX;

namespace {
	// Source: based on https://www.rastertek.com/dx11win10tut47.html
	void GetPickerRayVectors(int mouseX, int mouseY, int windowWidth, int windowHeight, const Camera& camera, XMVECTOR& outOrigin, XMVECTOR& outDirection) {
		// Move the mouse cursor coordinates into the -1 to +1 range.
		float pointX = ((2.0f * (float)mouseX) / (float)windowWidth) - 1.0f;
		float pointY = (((2.0f * (float)mouseY) / (float)windowHeight) - 1.0f) * -1.0f;

		// Adjust the points using the projection matrix to account for the aspect ratio of the viewport.
		XMMATRIX projectionMatrix = camera.Get_ProjectionMatrix();
		XMFLOAT4X4 pMatrix {};
		XMStoreFloat4x4(&pMatrix, projectionMatrix);
		pointX = pointX / pMatrix._11;
		pointY = pointY / pMatrix._22;

		// Get the inverse of the view matrix.
		XMMATRIX viewMatrix = camera.Get_ViewMatrix();
		XMMATRIX inverseViewMatrix = XMMatrixInverse(nullptr, viewMatrix);
		XMFLOAT4X4 iViewMatrix {};
		XMStoreFloat4x4(&iViewMatrix, inverseViewMatrix);

		// Calculate the direction of the picking ray in view space.
		XMFLOAT3 cameraDirection {};
		cameraDirection.x = (pointX * iViewMatrix._11) + (pointY * iViewMatrix._21) + iViewMatrix._31;
		cameraDirection.y = (pointX * iViewMatrix._12) + (pointY * iViewMatrix._22) + iViewMatrix._32;
		cameraDirection.z = (pointX * iViewMatrix._13) + (pointY * iViewMatrix._23) + iViewMatrix._33;

		outDirection = XMLoadFloat3(&cameraDirection);
		outDirection = XMVector3Normalize(outDirection);

		// Get the origin of the picking ray which is the position of the camera.
		outOrigin = camera.Get_Translation();
	}
}

Picker::Picker(Scene* scene) : m_Scene(scene) {}

// Cache all intersection hits and sort by raycast hit distance.
// If mouse has not moved, loop through all cached raycast hits.
bool Picker::MouseRaycast(int mouseX, int mouseY, int windowWidth, int windowHeight) {
	static int currentCacheIndex = 0;
	bool b_RayCastHit = false;

	if(mouseX == m_LastMousePos.first && mouseY == m_LastMousePos.second) {
		if(m_RaycastCache.size() == 0) return false;

		currentCacheIndex++;
		m_PickedObject = m_RaycastCache[currentCacheIndex % m_RaycastCache.size()].second;
		b_RayCastHit = true;
	}
	else {
		currentCacheIndex = 0;

		m_LastMousePos.first = mouseX;
		m_LastMousePos.second = mouseY;

		m_RaycastCache.clear();

		XMVECTOR origin {};
		XMVECTOR direction {};
		GetPickerRayVectors(mouseX, mouseY, windowWidth, windowHeight, m_Scene->m_MainCamera, origin, direction);

		for(auto& go : m_Scene->m_SceneObjects) {
			float hitDistance = 0.0f;
			if(go.GetAABB().Intersects(origin, direction, hitDistance)) {
				b_RayCastHit = true;
				m_RaycastCache.emplace_back(hitDistance, &go);
			}
		}

		if(b_RayCastHit) {
			std::sort(m_RaycastCache.begin(), m_RaycastCache.end(),
				[](std::pair<float, GameObject*> a, std::pair<float, GameObject*> b) { return a.first < b.first; }
			);

			m_PickedObject = m_RaycastCache[0].second;
		}
		else {
			m_PickedObject = nullptr;
		}
	}

	/// TODO: TEMP
	if(m_PickedObject != nullptr) Logger::Log(m_PickedObject->GetName());

	return b_RayCastHit;
}
