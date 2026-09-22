#pragma once

/*
	Derived class from GameObject.
	Simple implementation that currently only support objects using a specific PBR shader/pipeline
*/

#include "GameObject.h" // Base Class
#include "PBRObjectPSO.h"
#include "RenderConstants.h"

#include <DirectXMath.h>
#include <memory>
#include <string>
#include <vector>

using namespace DirectX;
using namespace RenderEnums;

class CommandList;
class DirectionalLight;
class Mesh;
class Texture;
class UpdateEventArgs;
class Scene;
class UnlitPSO;
class UnlitPrimitivePSO;
class PBRObjectPSO;

struct PBRVertexProps;
struct PBRLightProps;
struct PBRTessellationProps;

class PBRGameObject : public GameObject {

	friend class EditorGui;

public:
	// Instance of RenderProps will be kept as member 
	// Most of these values are accessed directly by EditorGui (friend class) right now, not ideal see note in EditorGui.h
	// This is to avoid writing tons of getter/setters
	struct RenderProps {
		std::wstring pbrMatName {};

		bool isShadowCaster = true;

		float heightMapMagnitude = 0.0f;

		// using PBRRenderFlags out of convenience, this could be error prone
		RenderFlags tessellationModeFlag = RenderFlags_UniformTessellation;
		float tessellationMagnitude = 1.0f;
		float tessellationEdgeLength = 5.0f;

		float parallaxMagnitude = 0.0f;
		bool useParallaxShadow = false;
		int minParallaxLayers = 8;
		int maxParallaxLayers = 32;

		XMFLOAT2 uvScale { 1.0f, 1.0f };

		// PSOs are owned by DemoGame
		PBRObjectPSO* pbrPSO {};
	};

	// NOTE: copy command list must still be executed after GameObject (outside of constructor), this is to keep flexibility to batch copy commands together.
	// We don't use RenderProps&& so there isn't accidental object invalidation for the caller.
	// Initialize with preconstructed mesh
	PBRGameObject(Scene& scene, CommandList& copyCommandList, const GameObject::EntityParams& params, const RenderProps& renderProps, std::shared_ptr<Mesh> mesh);

	/// TODO: Initialize with mesh loaded from file
	//GameObject(CommandList& copyCommandList, const GameObject::EntityParams& params, const RenderProps& renderProps, const std::wstring& meshFilePath);

	// Consider moving this to base clsass
	void RenderBoundingBox(CommandList& directCommandList, const UpdateEventArgs& e, UnlitPrimitivePSO* unlitPSO, const Scene& scene, XMFLOAT4 color);

	void Render(CommandList& directCommandList, const UpdateEventArgs& e, const Scene& scene, bool b_RenderWireframe = false);
	void RenderSilhouette(CommandList& directCommandList, const UpdateEventArgs& e, UnlitPSO* unlitPSO, XMFLOAT4 color);
	void RenderToDirectionalShadowMap(CommandList& directCommandList, const DirectionalLight& directionalLight);
	// NOTE: commandlist is not executed here
	void UpdatePBRShaderResourcesFromFile(CommandList& copyCommandList, const std::wstring& pbrMatName);
	void UpdateIBLShaderResources(const Scene& scene);

private:
	RenderProps m_RenderProps {};

	std::vector<std::shared_ptr<Texture>> m_TextureResources { PBRObjectPSO::TextureIndex::NumTextures };
	// Note: Stored CB members are shared between different rendering methods e.g. render bounding box, render silhoutte
	PBRVertexProps m_PBRVertexCB {};
	PBRLightProps m_PBRLightCB {};
	PBRTessellationProps m_TessellationCB {};
};

