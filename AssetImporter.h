#pragma once

#include "Logger.h"
#include "StringHelpers.h"
#include "VertexTypes.h"


#include <assimp/Importer.hpp>    
#include <assimp/scene.h>           
#include <assimp/postprocess.h>  

namespace {
    //void ImportMesh(CommandList& commandList, const aiMesh& aiMesh) {
    //    auto mesh = std::make_shared<Mesh>();

    //    std::vector<VertexInput> vertexData(aiMesh.mNumVertices);

    //    assert(aiMesh.mMaterialIndex < m_Materials.size());
    //    mesh->SetMaterial(m_Materials[aiMesh.mMaterialIndex]);

    //    unsigned int i;
    //    if(aiMesh.HasPositions())
    //    {
    //        for(i = 0; i < aiMesh.mNumVertices; ++i)
    //        {
    //            vertexData[i].Position = { aiMesh.mVertices[i].x, aiMesh.mVertices[i].y, aiMesh.mVertices[i].z };
    //        }
    //    }

    //    if(aiMesh.HasNormals())
    //    {
    //        for(i = 0; i < aiMesh.mNumVertices; ++i)
    //        {
    //            vertexData[i].Normal = { aiMesh.mNormals[i].x, aiMesh.mNormals[i].y, aiMesh.mNormals[i].z };
    //        }
    //    }

    //    if(aiMesh.HasTangentsAndBitangents())
    //    {
    //        for(i = 0; i < aiMesh.mNumVertices; ++i)
    //        {
    //            vertexData[i].Tangent = { aiMesh.mTangents[i].x, aiMesh.mTangents[i].y, aiMesh.mTangents[i].z };
    //            vertexData[i].Bitangent = { aiMesh.mBitangents[i].x, aiMesh.mBitangents[i].y, aiMesh.mBitangents[i].z };
    //        }
    //    }

    //    if(aiMesh.HasTextureCoords(0))
    //    {
    //        for(i = 0; i < aiMesh.mNumVertices; ++i)
    //        {
    //            vertexData[i].TexCoord = { aiMesh.mTextureCoords[0][i].x, aiMesh.mTextureCoords[0][i].y,
    //                                       aiMesh.mTextureCoords[0][i].z };
    //        }
    //    }

    //    auto vertexBuffer = commandList.CopyVertexBuffer(vertexData);
    //    mesh->SetVertexBuffer(0, vertexBuffer);

    //    // Extract the index buffer.
    //    if(aiMesh.HasFaces())
    //    {
    //        std::vector<unsigned int> indices;
    //        for(i = 0; i < aiMesh.mNumFaces; ++i)
    //        {
    //            const aiFace& face = aiMesh.mFaces[i];

    //            // Only extract triangular faces
    //            if(face.mNumIndices == 3)
    //            {
    //                indices.push_back(face.mIndices[0]);
    //                indices.push_back(face.mIndices[1]);
    //                indices.push_back(face.mIndices[2]);
    //            }
    //        }

    //        if(indices.size() > 0)
    //        {
    //            auto indexBuffer = commandList.CopyIndexBuffer(indices);
    //            mesh->SetIndexBuffer(indexBuffer);
    //        }
    //    }

    //    // Set the AABB from the AI Mesh's AABB.
    //    mesh->SetAABB(CreateBoundingBox(aiMesh.mAABB));

    //    m_Meshes.push_back(mesh);
    //}
}

namespace AssetImporter {
	inline const aiScene* ImportModel(const std::wstring& modelFilePath) {
		Assimp::Importer importer;

		const aiScene* pScene =
			importer.ReadFile(StringConvert::WideString_to_String(modelFilePath),
				aiProcessPreset_TargetRealtime_MaxQuality |
				aiProcess_OptimizeGraph |
				//aiProcess_ConvertToLeftHanded | 
				aiProcess_GenBoundingBoxes
			);

		if(pScene == nullptr) {
			Logger::Log(importer.GetErrorString());
		}

		return pScene;
	}
}



