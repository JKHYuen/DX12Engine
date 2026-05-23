#include "AssetImporter.h"

#include "Logger.h"
#include "StringHelpers.h"
#include "Helpers.h"
#include "d3dx12_core.h"

#include "DX12EngineCore/VertexInput.h"
#include "DX12EngineCore/CommandList.h"
#include "DX12EngineCore/Mesh.h"

#include <d3dcompiler.h>
#include <unordered_map>    
#include <string>    
#include <filesystem>

#include <assimp/Importer.hpp>    
#include <assimp/scene.h>           
#include <assimp/postprocess.h>  

using namespace DirectX;
namespace fs = std::filesystem;

namespace {
	AssetImporter* sp_Singleton;

	std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3DBlob>> s_LoadedCompiledShaders {};
	std::mutex s_LoadedCompiledShadersMutex;

	fs::path s_AssetPath;
}

AssetImporter& AssetImporter::Initialize() {
	if(!sp_Singleton) {
		sp_Singleton = new AssetImporter();

		/// Find assets folder
		s_AssetPath = L"";

		// Check if assets folder is in same folder as exe
		if(fs::exists(L"assets/")) {
			s_AssetPath = L"assets/";
		}
		// Look for assets in a shared location, currently hardcoded for visual studio project
		// assets location: solutionDir/assets
		//    exe location: solutionDir/Bin/(Release/Debug)/
		else {
			const auto& sharedAssetPath = fs::current_path().parent_path().parent_path();
			if(sharedAssetPath != "") {
				s_AssetPath = sharedAssetPath / L"assets/";
			}
		}

		if(s_AssetPath == L"") {
			MessageBox(NULL, L"Asset folder not found.", NULL, MB_OK);
			throw std::exception("Asset folder not found.");
		}
		///
	}
	return *sp_Singleton;
}

AssetImporter& AssetImporter::Get() {
	assert(sp_Singleton != nullptr);
	return *sp_Singleton;
}

void AssetImporter::Destroy() {
	if(sp_Singleton) {
		delete sp_Singleton;
		sp_Singleton = nullptr;
	}
}

const std::filesystem::path& AssetImporter::GetAssetPath() {
	return s_AssetPath;
}

// Based on: https://github.com/jpvanoosten/LearningDirectX12/blob/v1.1.0/DX12Lib/src/Scene.cpp
std::shared_ptr<Mesh> AssetImporter::ProcessMesh(CommandList& commandList, const aiMesh& aiMesh) {
	auto mesh = std::make_shared<Mesh>();

	std::vector<VertexInput> vertexData(aiMesh.mNumVertices);

	unsigned int i;
	if(aiMesh.HasPositions()) {
		for(i = 0; i < aiMesh.mNumVertices; ++i) {
			vertexData[i].Position = { aiMesh.mVertices[i].x, aiMesh.mVertices[i].y, aiMesh.mVertices[i].z };
		}
	}

	if(aiMesh.HasNormals()) {
		for(i = 0; i < aiMesh.mNumVertices; ++i) {
			vertexData[i].Normal = { aiMesh.mNormals[i].x, aiMesh.mNormals[i].y, aiMesh.mNormals[i].z };
		}
	}

	if(aiMesh.HasTangentsAndBitangents()) {
		for(i = 0; i < aiMesh.mNumVertices; ++i) {
			vertexData[i].Tangent = { aiMesh.mTangents[i].x, aiMesh.mTangents[i].y, aiMesh.mTangents[i].z };
			vertexData[i].Bitangent = { aiMesh.mBitangents[i].x, aiMesh.mBitangents[i].y, aiMesh.mBitangents[i].z };
		}
	}

	if(aiMesh.HasTextureCoords(0)) {
		for(i = 0; i < aiMesh.mNumVertices; ++i) {
			vertexData[i].TexCoord = { aiMesh.mTextureCoords[0][i].x, aiMesh.mTextureCoords[0][i].y, aiMesh.mTextureCoords[0][i].z };
		}
	}

	auto vertexBuffer = commandList.CopyVertexBuffer(vertexData);
	mesh->SetVertexBuffer(0, vertexBuffer);

	// Extract the index buffer.
	if(aiMesh.HasFaces()) {
		std::vector<unsigned int> indices;
		for(i = 0; i < aiMesh.mNumFaces; ++i) {
			const aiFace& face = aiMesh.mFaces[i];

			// Only extract triangular faces
			if(face.mNumIndices == 3) {
				indices.push_back(face.mIndices[0]);
				indices.push_back(face.mIndices[1]);
				indices.push_back(face.mIndices[2]);
			}
		}

		if(indices.size() > 0) {
			auto indexBuffer = commandList.CopyIndexBuffer(indices);
			mesh->SetIndexBuffer(indexBuffer);
		}
	}

	// Convert min max AABB representation
	aiVector3D convertedAABBExtents = (aiMesh.mAABB.mMax - aiMesh.mAABB.mMin) * 0.5f;
	mesh->SetExtents({ convertedAABBExtents.x, convertedAABBExtents.y , convertedAABBExtents.z });

	aiVector3D convertedAABBCenter = (aiMesh.mAABB.mMax + aiMesh.mAABB.mMin) * 0.5f;
	mesh->SetCenter({ convertedAABBCenter.x, convertedAABBCenter.y, convertedAABBCenter.z });

	return mesh;
}

/// TODO: INCOMPLETE - function only checks for first mesh in file
/// TODO: figure out a way to make Assimp materials compatible with this renderer
std::shared_ptr<Mesh> AssetImporter::ImportModel(CommandList& commandList, const std::wstring& modelPath) {
	Assimp::Importer importer;
	std::string filePathStr {};
	StringConvert::WideString_To_String(modelPath, filePathStr);
	const aiScene* pScene =
		importer.ReadFile(filePathStr,
			aiProcessPreset_TargetRealtime_MaxQuality |
			aiProcess_OptimizeGraph |
			aiProcess_ConvertToLeftHanded |
			aiProcess_GenBoundingBoxes
		);

	if(pScene == nullptr) {
		Logger::Log(importer.GetErrorString());
		return nullptr;
	}

	if(!pScene->HasMeshes()) {
		Logger::Log("ASSET IMPORTER: No meshes found.");
		return nullptr;
	}

	return ProcessMesh(commandList, *(pScene->mMeshes[0]));
}

// compiled_shaders location is hardcoded, will need more robust system if this engine gets more complicated (ability to load different IGames during runtime)
CD3DX12_SHADER_BYTECODE AssetImporter::GetCompiledShaderFromFile(const std::wstring& csoFileName) {
	std::lock_guard<std::mutex> lock(s_LoadedCompiledShadersMutex);

	std::wstring filePath = L"compiled_shaders/" + csoFileName;

	if(auto kvp = s_LoadedCompiledShaders.find(filePath); kvp != s_LoadedCompiledShaders.end()) {
		return CD3DX12_SHADER_BYTECODE(kvp->second.Get());
	}
	else {
		Microsoft::WRL::ComPtr<ID3DBlob> blobptr;
		ThrowIfFailed(D3DReadFileToBlob(filePath.data(), &blobptr));
		s_LoadedCompiledShaders.emplace(filePath, blobptr); // string construction
		return CD3DX12_SHADER_BYTECODE(blobptr.Get());
	}
}