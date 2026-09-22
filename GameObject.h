#pragma once

/*
	Game object with mesh and transform values, currently inherited by other classes like PBRGameObject and point lights. Eventually a Unity-esque component system will replace this class hierarchy
	
	NOTE:
		- AABB does not support rotation updates currently (it's non-trivial)
		- A dynamic component system should be used for render features
		- Can use dirty flag system for SRT/root sig updates
*/

#include "DirectXCollision.h"

#include <DirectXMath.h>
#include <memory>
#include <string>
#include <string_view>

using namespace DirectX;

class Mesh;
class Camera;
class Scene;

class GameObject {

	friend class EditorGui;

public:
	struct EntityParams {
		// string passed by value for convenience e.g. when modifying instances of this struct
		std::string name;

		// Only support construction with radians for now
		// This should ideally be a union with quaternion and radian representations, but will need some validation
		XMFLOAT3 scale, radianEulerRotation, translation;
	};

	// NOTE: copy command list must still be executed after GameObject (outside of constructor), this is to keep flexibility to batch copy commands together.
	// Initialize with preconstructed mesh
	GameObject(const EntityParams& params, std::shared_ptr<Mesh> mesh);
	// Simplified constructor for uniform scaled, unrotated objects
	GameObject(XMFLOAT3 translation, float scale, std::shared_ptr<Mesh> mesh, const std::string& name);
	virtual ~GameObject() = default;

	XMFLOAT3 GetTranslation()   const { return m_Translation; };
	// Radians!
	XMFLOAT3 GetEulerRotation() const { return m_RadianEulerRotation; };
	XMFLOAT3 GetScale()         const { return m_Scale; };

	void SetTranslation(float x, float y, float z);
	void SetEulerRotation(float x, float y, float z); // Radians!
	void SetScale(float x, float y, float z);

	void Translate(float x, float y, float z);   // Adds to world position values
	void XM_CALLCONV QuatRotate(FXMVECTOR quaternion);
	void Scale(float x, float y, float z);       // Multiplies current scale (*not add)

	std::string_view GetName() const      { return m_Name; }
	void SetName(const std::string& name) { m_Name = name; }

	const BoundingBox& GetAABB() const { return m_AABB; }
	std::shared_ptr<Mesh> GetMesh() const { return m_Mesh; }
	
protected:
	BoundingBox m_AABB {};

	// Mesh local origin rotated by model's current rotation matrix
	// This is needed for AABB calc because rotating an object that's local origin is not world 0,0,0 moves AABB center
	XMFLOAT3 m_AABBOffset {};

	void RecalcAABB();

	std::string m_Name;

	XMFLOAT4X4 m_TranslationMat {};
	XMFLOAT4X4 m_RotationMat {};
	XMFLOAT4X4 m_ScaleMat {};

	/// TODO: test if operations saved is worth the space
	// Cached and updated only when when rotation or scale matrix is updated (i.e. in translation and scale setters)
	XMFLOAT4X4 m_SRMat;
	
	// Keep track of these separate from matrices for convenience (e.g. UI display)
	XMFLOAT3 m_Translation, m_RadianEulerRotation /*Radians*/, m_Scale;

	bool mb_RenderThisFrame;

	std::shared_ptr<Mesh> m_Mesh {};

private:
	void Initialize(const EntityParams& params, std::shared_ptr<Mesh> mesh);
};

