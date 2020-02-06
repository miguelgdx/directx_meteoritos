#include "pch.h"
#include "Mesh.h"


Mesh::Mesh() : defaultColor (XMFLOAT4(Colors::White))
{
	vertices = {
		{XMFLOAT3(-1.0f,-1.0f,-1.0f),XMFLOAT4(Colors::Red)},
		{XMFLOAT3(-1.0f,+1.0f,-1.0f),XMFLOAT4(Colors::Red)},
		{XMFLOAT3(+1.0f,+1.0f,-1.0f),XMFLOAT4(Colors::Red)},
		{XMFLOAT3(+1.0f,-1.0f,-1.0f),XMFLOAT4(Colors::Red)},
		{XMFLOAT3(-1.0f,-1.0f,+1.0f),XMFLOAT4(Colors::Red)},
		{XMFLOAT3(-1.0f,+1.0f,+1.0f),XMFLOAT4(Colors::Red)},
		{XMFLOAT3(+1.0f,+1.0f,+1.0f),XMFLOAT4(Colors::Red)},
		{XMFLOAT3(+1.0f,-1.0f,+1.0f),XMFLOAT4(Colors::Red)}
	};

	indices = {
		0,1,2,
		0,2,3,
		4,6,5,
		5,7,6,
		4,5,1,
		4,1,0,
		3,2,6,
		3,6,7,
		1,5,6,
		1,6,2,
		4,0,3,
		4,3,7


	};

	unsigned int sv = sizeof(Vertex);
	unsigned int nvertices = vertices.size();
	vsize = nvertices * sv;
	unsigned int sind = sizeof(unsigned int);
	unsigned int nindices = indices.size();
	isize = nindices * sind;
}

Mesh::Mesh(std::string const fileName) : defaultColor(XMFLOAT4(Colors::White)) {
	readFile(fileName);
}


Mesh::~Mesh()
{
}

UINT64 Mesh::GetVSize() const {
	return vsize;
}

UINT64 Mesh::GetISize() const {
	return isize;
}

void Mesh::readFile(std::string const fileName) {
	std::ifstream file(fileName, std::fstream::in);
	if (!file.good())
		return;
	unsigned int nvertices;
	vertices.clear();
	indices.clear();
	file >> nvertices;
	float vec[3]{};
	for (auto i = 0; i < nvertices; i++) {
		file >> vec[0] >> vec[1] >> vec[2];
		Vertex v;
		v.pos = XMFLOAT3(vec[0], vec[1], vec[2]);
		v.col = defaultColor;
		vertices.push_back(v);
	}

	auto cont = 0;
	for (auto i = 0; i < nvertices; i++) {
		file >> vec[0] >> vec[1] >> vec[2];
		vertices[cont++].normal = XMFLOAT3(vec[0], vec[1], vec[2]);
	}
	
	unsigned int nindices;
	file >> nindices;
	for (auto i = 0; i < nindices; i++) {
		unsigned int ind;
		file >> ind;
		indices.push_back(ind);
	}
	isize = indices.size()*sizeof(unsigned int);
	for (auto i = 0; i < nvertices; i++) {
		float u, v;
		file >> u >> v;
		XMFLOAT2 uv = XMFLOAT2(u, v);
		vertices[i].uvcoords = uv;
	}
	vsize = vertices.size() * sizeof(Vertex);
	file.close();

}