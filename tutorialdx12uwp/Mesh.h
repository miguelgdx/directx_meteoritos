#pragma once
#include "pch.h"

using namespace DirectX;

struct Vertex {

	XMFLOAT3 pos;
	XMFLOAT4 col;
	XMFLOAT3 normal;
	XMFLOAT2 uvcoords;

};

class Mesh
{
public:
	Mesh();
	Mesh(std::string const fileName);
	~Mesh();

	UINT64 GetVSize() const;
	UINT64 GetISize() const;
	void readFile(std::string const fileName);
	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices;
	struct Texture {
		std::string Name;
		std::wstring Filename;
		Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeap = nullptr;
	};

	std::unique_ptr<Texture> meshTexture;
private:
	UINT64 vsize;
	UINT64 isize;
	XMFLOAT4 defaultColor;
};

