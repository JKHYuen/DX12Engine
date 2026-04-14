#pragma once

#include "Logger.h"
#include "StringHelpers.h"
#include "Helpers.h"
#include "VertexTypes.h"
#include "CommandList.h"
#include "Mesh.h"
#include "Logger.h"

#include <d3dcompiler.h>
#include <unordered_map>    
#include <string>    

#include <assimp/Importer.hpp>    
#include <assimp/scene.h>           
#include <assimp/postprocess.h>  

namespace AssetImporter {
	// Still accessible globally, this unnamed namespace just indicates data/members that should only be used in this file
	// Everything can be moved to a class if we really care about encapsulation
	namespace {
		std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3DBlob>> g_LoadedCompiledShaders {};

		// Based on: https://github.com/jpvanoosten/LearningDirectX12/blob/v1.1.0/DX12Lib/src/Scene.cpp
		std::shared_ptr<Mesh> ProcessMesh(CommandList& commandList, const aiMesh& aiMesh) {
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

			// Convert min max AABB representation to just extents
			aiVector3D convertedAABB = (aiMesh.mAABB.mMax - aiMesh.mAABB.mMin) * 0.5f;
			mesh->SetExtents({ convertedAABB.x, convertedAABB.y , convertedAABB.z });

			return mesh;
		}
	}

	/// TODO: INCOMPLETE
	/// TODO: figure out some material system for this project
	inline std::shared_ptr<Mesh> ImportModel(CommandList& commandList, const std::wstring& modelFilePath) {
		Assimp::Importer importer;

		const aiScene* pScene =
			importer.ReadFile(StringConvert::WideString_To_String(modelFilePath),
				aiProcessPreset_TargetRealtime_MaxQuality |
				aiProcess_OptimizeGraph |
				aiProcess_ConvertToLeftHanded | 
				aiProcess_GenBoundingBoxes
			);

		if(pScene == nullptr) {
			Logger::Log(importer.GetErrorString());
			return nullptr;
		}

		/// TEST
		if(!pScene->HasMeshes()) {
			return nullptr;
		}

		return ProcessMesh(commandList, *(pScene->mMeshes[0]));
	}

	// Note: compiled_shaders location is hardcoded, will need more robust system if this engine gets more complicated (ability to load different IGames during runtime)
	inline CD3DX12_SHADER_BYTECODE GetCompiledShaderFromFile(const std::wstring& csoFileName) {
		std::wstring filePath = L"compiled_shaders/" + csoFileName;

		if(auto kvp = g_LoadedCompiledShaders.find(filePath); kvp != g_LoadedCompiledShaders.end()) {
			return CD3DX12_SHADER_BYTECODE(kvp->second.Get());
		}
		else {
			Microsoft::WRL::ComPtr<ID3DBlob> blob;
			ThrowIfFailed(D3DReadFileToBlob(filePath.data(), &blob));
			g_LoadedCompiledShaders.emplace(filePath, blob);
			return CD3DX12_SHADER_BYTECODE(blob.Get());
		}
	}

}



