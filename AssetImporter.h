#pragma once

// Only needed for CD3DX12_SHADER_BYTECODE return value,
// unsure how to avoid this while keeping the convenience of the return type
#include "d3dx12_core.h"

#include <memory>
#include <string>
#include <filesystem>

class CommandList;
class Mesh;
struct aiMesh;

/// Singleton
class AssetImporter {
public:
	static AssetImporter& Initialize();
	static AssetImporter& Get();
	static void Destroy();

	AssetImporter(const AssetImporter&) = delete;
	AssetImporter& operator=(const AssetImporter&) = delete;
	AssetImporter(AssetImporter&&) = delete;
	AssetImporter& operator=(AssetImporter&&) = delete;

	const std::filesystem::path& GetAssetPath();

	std::shared_ptr<Mesh> ImportModel(CommandList& commandList, const std::wstring& modelPath);
	CD3DX12_SHADER_BYTECODE GetCompiledShaderFromFile(const std::wstring& csoFileName);

private:
	AssetImporter() = default;

	std::shared_ptr<Mesh> ProcessMesh(CommandList& commandList, const aiMesh& aiMesh);
};


