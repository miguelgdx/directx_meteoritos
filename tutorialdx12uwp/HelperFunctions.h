#pragma once
#include "pch.h"
using namespace DirectX;

unsigned  int CalcConstantBufferByteSize(unsigned int bytesize);
void readfile(char const* fn, std::vector<char> &vbytes);

// Número aleatorio
float RandomNumber(float Min, float Max);

// Vector aleatorio normalizado.
DirectX::XMVECTOR randomNormalizedVector();

