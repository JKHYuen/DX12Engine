#include "GameObject.h"

#include "DX12EngineCore/Mesh.h"

#include <array>
#include <cmath>
#include <DirectXMath.h>
#include <memory>
#include <Logger.h>
#include <string>

using namespace DirectX;

void GameObject::Initialize(const EntityParams& params, std::shared_ptr<Mesh> mesh) {
	m_Mesh = mesh;
	m_Name = params.name;
	mb_RenderThisFrame = true;

	// Don't use setters (e.g. SetTranslation()) to initialize, this ensures AABB is initialized properly / avoids unneccesary calcs
	m_Scale = params.scale;
	XMStoreFloat4x4(&m_ScaleMat, XMMatrixScaling(params.scale.x, params.scale.y, params.scale.z));
	m_RadianEulerRotation = params.radianEulerRotation;
	XMStoreFloat4x4(&m_RotationMat, XMMatrixRotationRollPitchYaw(params.radianEulerRotation.x, params.radianEulerRotation.y, params.radianEulerRotation.z));
	m_Translation = params.translation;
	XMStoreFloat4x4(&m_TranslationMat, XMMatrixTranslation(params.translation.x, params.translation.y, params.translation.z));

	XMStoreFloat4x4(&m_SRMat, XMMatrixMultiply(XMLoadFloat4x4(&m_ScaleMat), XMLoadFloat4x4(&m_RotationMat)));

	// Initialize AABB
	{
		XMStoreFloat3(&m_AABBOffset, XMVector3Transform(XMVECTORF32 { m_Mesh->GetCenter().x, m_Mesh->GetCenter().y, m_Mesh->GetCenter().z }, XMLoadFloat4x4(&m_RotationMat)));

		// AABB center is always object geometric center, loaded mesh origin may be offsetted, account for it here
		m_AABB.Center.x = m_Translation.x + m_AABBOffset.x;
		m_AABB.Center.y = m_Translation.y + m_AABBOffset.y;
		m_AABB.Center.z = m_Translation.z + m_AABBOffset.z;

		RecalcAABB();
	}
}

GameObject::GameObject(const EntityParams& params, std::shared_ptr<Mesh> mesh) {
	Initialize(params, mesh);
}

GameObject::GameObject(XMFLOAT3 translation, float scale, std::shared_ptr<Mesh> mesh, const std::string& name = "") {
	EntityParams p {name, XMFLOAT3(scale, scale, scale), XMFLOAT3(0, 0, 0), translation};
	Initialize(p, mesh);
}

void GameObject::Translate(float x, float y, float z) {
	SetTranslation(m_Translation.x + x, m_Translation.y + y, m_Translation.z + z);
}

void XM_CALLCONV GameObject::QuatRotate(FXMVECTOR quaternion) {
	XMFLOAT4X4 tempMat {};
	XMStoreFloat4x4(&tempMat, XMMatrixMultiply(XMLoadFloat4x4(&m_RotationMat), XMMatrixRotationQuaternion(quaternion)));

	// Source: https://stackoverflow.com/a/67421550
	DirectX::XMVECTOR from { XMVectorSet(tempMat._12, tempMat._31, 0.0f, 0.0f) };
	DirectX::XMVECTOR to   { XMVectorSet(tempMat._22, tempMat._33, 0.0f, 0.0f) };
	DirectX::XMVECTOR res  { XMVectorATan2(from, to) };
	float roll  = XMVectorGetX(res);
	float pitch = XMScalarASin(-tempMat._32);
	float yaw   = XMVectorGetY(res);

	SetEulerRotation(pitch, yaw, roll);
}

void GameObject::Scale(float x, float y, float z) {
	SetScale(m_Scale.x * x, m_Scale.y * y, m_Scale.z * z);
}

void GameObject::SetTranslation(float x, float y, float z) {
	m_Translation = { x, y, z };
	XMStoreFloat4x4(&m_TranslationMat, XMMatrixTranslation(x, y, z));

	// AABB center is always object geometric center, loaded mesh origin may be offsetted, account for it here
	m_AABB.Center.x = m_Translation.x + m_AABBOffset.x;
	m_AABB.Center.y = m_Translation.y + m_AABBOffset.y;
	m_AABB.Center.z = m_Translation.z + m_AABBOffset.z;
}

void GameObject::SetEulerRotation(float x, float y, float z) {
	m_RadianEulerRotation = { x, y, z };
	XMStoreFloat4x4(&m_RotationMat, XMMatrixRotationRollPitchYaw(x, y, z));

	// Update cached values
	{
		XMStoreFloat4x4(&m_SRMat, XMMatrixMultiply(XMLoadFloat4x4(&m_ScaleMat), XMLoadFloat4x4(&m_RotationMat)));
		XMStoreFloat3(&m_AABBOffset, XMVector3Transform(XMVECTORF32 { m_Mesh->GetCenter().x, m_Mesh->GetCenter().y, m_Mesh->GetCenter().z }, XMLoadFloat4x4(&m_RotationMat)));
	}

	RecalcAABB();
}

void GameObject::SetScale(float x, float y, float z) {
	m_Scale = {x, y, z};
	XMStoreFloat4x4(&m_ScaleMat, XMMatrixScaling(x, y, z));

	// Update cached values
	XMStoreFloat4x4(&m_SRMat, XMMatrixMultiply(XMLoadFloat4x4(&m_ScaleMat), XMLoadFloat4x4(&m_RotationMat)));

	RecalcAABB();
}

// Note: currently does not support height map, might be a lot more expensive/inaccurate if we do
// Scale and rotate all 8 vertices (for non-uniform scaling support) with object transformation matrices
// then find new extents based on this new transformed AABB.
// Kind of slow, only called when scaling or rotating object. 
// Can be simplified if there is uniform scaling.
void GameObject::RecalcAABB() {
	// Start with original mesh extents at world origin
	const XMFLOAT3& e = m_Mesh->GetExtents();
	static std::array<XMFLOAT3, 8> aabbVerts {};
	aabbVerts[0] = {+ e.x, + e.y, + e.z}; aabbVerts[1] = {- e.x, - e.y, - e.z};
	aabbVerts[2] = {+ e.x, + e.y, - e.z}; aabbVerts[3] = {- e.x, - e.y, + e.z};
	aabbVerts[4] = {+ e.x, - e.y, + e.z}; aabbVerts[5] = {- e.x, + e.y, - e.z};
	aabbVerts[6] = {+ e.x, - e.y, - e.z}; aabbVerts[7] = {- e.x, + e.y, + e.z};

	float maxX = 0.0f, maxY = 0.0f, maxZ = 0.0f;
	for(XMFLOAT3& v : aabbVerts) {
		// Rotate/Scale AABB vertex
		XMStoreFloat3(&v, XMVector3Transform(XMLoadFloat3(&v), XMLoadFloat4x4(&m_SRMat)));

		maxX = std::fmax(maxX, std::fabs(v.x));
		maxY = std::fmax(maxY, std::fabs(v.y));
		maxZ = std::fmax(maxZ, std::fabs(v.z));
	}

	m_AABB.Extents.x = maxX;
	m_AABB.Extents.y = maxY;
	m_AABB.Extents.z = maxZ;

	// Update AABB center location because rotating with object's pivot could change where AABB center is 
	// (AABB center is always object's geometric center)
	m_AABB.Center.x = m_Translation.x + m_AABBOffset.x;
	m_AABB.Center.y = m_Translation.y + m_AABBOffset.y;
	m_AABB.Center.z = m_Translation.z + m_AABBOffset.z;
}


