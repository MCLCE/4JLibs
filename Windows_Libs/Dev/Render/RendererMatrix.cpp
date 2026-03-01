#pragma once
#include "Renderer.h"

#include <cstring>

const float* Renderer::MatrixGet(int type)
{
	Context& context = this->getContext();
	const int depth = context.matrixStackDepth[type];
	return reinterpret_cast<const float*>(&context.matrixStacks[type][depth]);
}

void Renderer::MatrixMode(int type)
{
	Context& context = this->getContext();
	context.matrixModeType = type;
}

void Renderer::MatrixMult(float* mat)
{
	DirectX::XMMATRIX matrix;
	std::memcpy(&matrix, mat, sizeof(matrix));
	this->MultWithStack(matrix);
}

void Renderer::MatrixOrthogonal(float left, float right, float bottom, float top, float zNear, float zFar)
{
	const DirectX::XMMATRIX matrix = DirectX::XMMatrixOrthographicOffCenterRH(left, right, bottom, top, zNear, zFar);
	this->MultWithStack(matrix);
}

void Renderer::MatrixPerspective(float fovy, float aspect, float zNear, float zFar)
{
	const float fovRadians = fovy * (3.14159274f / 180.0f);
	const DirectX::XMMATRIX matrix = DirectX::XMMatrixPerspectiveFovRH(fovRadians, aspect, zNear, zFar);
	this->MultWithStack(matrix);
}

void Renderer::MatrixPop()
{
	Context& context = this->getContext();
	const int mode = context.matrixModeType;
	--context.matrixStackDepth[mode];
	context.matrixDirty[mode] = true;
}

void Renderer::MatrixPush()
{
	Context& context = this->getContext();
	const int mode = context.matrixModeType;
	const int depth = context.matrixStackDepth[mode];
	context.matrixStacks[mode][depth + 1] = context.matrixStacks[mode][depth];
	++context.matrixStackDepth[mode];
}

void Renderer::MatrixRotate(float angle, float x, float y, float z)
{
	const DirectX::XMVECTOR axis = DirectX::XMVectorSet(x, y, z, 0.0f);
	const DirectX::XMMATRIX matrix = DirectX::XMMatrixRotationAxis(axis, angle);
	this->MultWithStack(matrix);
}

void Renderer::MatrixScale(float x, float y, float z)
{
	const DirectX::XMMATRIX matrix = DirectX::XMMatrixScaling(x, y, z);
	this->MultWithStack(matrix);
}

void Renderer::MatrixSetIdentity()
{
	Context& context = this->getContext();
	const int mode = context.matrixModeType;
	const int depth = context.matrixStackDepth[mode];
	context.matrixStacks[mode][depth] = DirectX::XMMatrixIdentity();
	context.matrixDirty[mode] = true;
}

void Renderer::MatrixTranslate(float x, float y, float z)
{
	const DirectX::XMMATRIX matrix = DirectX::XMMatrixTranslation(x, y, z);
	this->MultWithStack(matrix);
}

void Renderer::MultWithStack(DirectX::XMMATRIX matrix)
{
	Context& context = this->getContext();
	const int mode = context.matrixModeType;
	const int depth = context.matrixStackDepth[mode];
	DirectX::XMMATRIX& current = context.matrixStacks[mode][depth];
	current = DirectX::XMMatrixMultiply(matrix, current);
	context.matrixDirty[mode] = true;
}

void Renderer::Set_matrixDirty()
{
	Context& context = this->getContext();
	const DirectX::XMMATRIX identity = DirectX::XMMatrixIdentity();

	context.matrixStacks[0][0] = identity;
	context.matrixStacks[1][0] = identity;
	context.matrixStacks[2][0] = identity;
	context.matrixStacks[3][0] = identity;

	context.matrixDirty[0] = true;
	context.matrixDirty[1] = true;
	context.matrixDirty[2] = true;
	context.matrixDirty[3] = true;

	activeVertexType = 0xFFFFFFFFu;
	activePixelType = 0xFFFFFFFFu;
}
