#pragma once
#include "Renderer.h"

#include <cstddef>
#include <cstring>
#include <limits>

ID3D11BlendState* Renderer::GetManagedBlendState()
{
	Context& context = this->getContext();
	const D3D11_RENDER_TARGET_BLEND_DESC& rtBlend = context.blendDesc.RenderTarget[0];

	const int key =
		(rtBlend.BlendEnable ? 1 : 0) |
		((static_cast<int>(rtBlend.SrcBlend) & 0x1F) << 1) |
		((static_cast<int>(rtBlend.DestBlend) & 0x1F) << 6) |
		((static_cast<int>(rtBlend.RenderTargetWriteMask) & 0x0F) << 11);

	auto it = managedBlendStates.find(key);
	if (it != managedBlendStates.end())
	{
		return it->second;
	}

	ID3D11BlendState* state = nullptr;
	m_pDevice->CreateBlendState(&context.blendDesc, &state);
	managedBlendStates.emplace(key, state);
	return state;
}

ID3D11DepthStencilState* Renderer::GetManagedDepthStencilState()
{
	Context& context = this->getContext();

	const int key =
		(context.depthStencilDesc.DepthEnable ? 2 : 0) |
		((static_cast<int>(context.depthStencilDesc.DepthFunc) & 0x0F) << 2) |
		(context.depthStencilDesc.DepthWriteMask == D3D11_DEPTH_WRITE_MASK_ALL ? 1 : 0);

	auto it = managedDepthStencilStates.find(key);
	if (it != managedDepthStencilStates.end())
	{
		return it->second;
	}

	ID3D11DepthStencilState* state = nullptr;
	m_pDevice->CreateDepthStencilState(&context.depthStencilDesc, &state);
	managedDepthStencilStates.emplace(key, state);
	return state;
}

ID3D11RasterizerState* Renderer::GetManagedRasterizerState()
{
	Context& context = this->getContext();

	const int key =
		(static_cast<std::uint8_t>(context.rasterizerDesc.DepthBias)) |
		(static_cast<std::uint8_t>(static_cast<int>(context.rasterizerDesc.SlopeScaledDepthBias)) << 8) |
		((static_cast<int>(context.rasterizerDesc.CullMode) & 0x03) << 16);

	auto it = managedRasterizerStates.find(key);
	if (it != managedRasterizerStates.end())
	{
		return it->second;
	}

	ID3D11RasterizerState* state = nullptr;
	m_pDevice->CreateRasterizerState(&context.rasterizerDesc, &state);
	managedRasterizerStates.emplace(key, state);
	return state;
}

ID3D11SamplerState* Renderer::GetManagedSamplerState()
{
	Context& context = this->getContext();
	const int key = m_textures[context.boundTextureIndex].samplerParams;

	auto it = managedSamplerStates.find(key);
	if (it != managedSamplerStates.end())
	{
		return it->second;
	}

	const bool clampU = (key & 0x01) != 0;
	const bool clampV = (key & 0x02) != 0;
	const bool linearFilter = (key & 0x04) != 0;
	const bool mipLinear = (key & 0x08) != 0;
	const int filterBits = (mipLinear ? 0x08 : 0x00) | (linearFilter ? 0x22 : 0x02);

	D3D11_SAMPLER_DESC desc = {};
	desc.Filter = static_cast<D3D11_FILTER>(filterBits >> 1);
	desc.AddressU = clampU ? D3D11_TEXTURE_ADDRESS_CLAMP : D3D11_TEXTURE_ADDRESS_WRAP;
	desc.AddressV = clampV ? D3D11_TEXTURE_ADDRESS_CLAMP : D3D11_TEXTURE_ADDRESS_WRAP;
	desc.AddressW = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(3);
	desc.MipLODBias = 0.0f;
	desc.MaxAnisotropy = 16;
	desc.ComparisonFunc = static_cast<D3D11_COMPARISON_FUNC>(1);
	desc.BorderColor[0] = 0.0f;
	desc.BorderColor[1] = 0.0f;
	desc.BorderColor[2] = 0.0f;
	desc.BorderColor[3] = 0.0f;
	desc.MinLOD = -(std::numeric_limits<float>::max)();
	desc.MaxLOD = (std::numeric_limits<float>::max)();

	ID3D11SamplerState* state = nullptr;
	m_pDevice->CreateSamplerState(&desc, &state);
	managedSamplerStates.emplace(key, state);
	return state;
}

void Renderer::StateSetFogEnable(bool enable)
{
	Context& context = this->getContext();
	context.fogEnabled = enable ? 1 : 0;
}

void Renderer::StateSetFogMode(int mode)
{
	Context& context = this->getContext();
	context.fogMode = mode;
}

void Renderer::StateSetFogNearDistance(float dist)
{
	Context& context = this->getContext();
	context.fogNearDistance = dist;
}

void Renderer::StateSetFogFarDistance(float dist)
{
	Context& context = this->getContext();
	context.fogFarDistance = dist;
}

void Renderer::StateSetFogDensity(float density)
{
	Context& context = this->getContext();
	context.fogDensity = density;
}

void Renderer::StateSetFogColour(float red, float green, float blue)
{
	Context& context = this->getContext();
	context.fogColourRed = red;
	context.fogColourBlue = blue;
	context.fogColourGreen = green;
}

void Renderer::UpdateViewportState()
{
}

void Renderer::StateSetLightingEnable(bool enable)
{
	Context& context = this->getContext();
	if (context.commandBuffer != nullptr && context.commandBuffer->isActive != 0)
	{
		context.commandBuffer->SetLightingEnable(enable);
		return;
	}

	context.lightingEnabled = enable ? 1 : 0;
}

void Renderer::StateSetLightColour(int light, float red, float green, float blue)
{
	if (light >= 2)
	{
		return;
	}

	Context& context = this->getContext();
	if (context.commandBuffer != nullptr && context.commandBuffer->isActive != 0)
	{
		context.commandBuffer->SetLightColour(light, red, green, blue);
		return;
	}

	context.lightColour[light].x = red;
	context.lightColour[light].y = green;
	context.lightColour[light].z = blue;
	context.lightColour[light].w = 1.0f;
	context.lightingDirty = 1;
}

void Renderer::StateSetLightAmbientColour(float red, float green, float blue)
{
	Context& context = this->getContext();
	if (context.commandBuffer != nullptr && context.commandBuffer->isActive != 0)
	{
		context.commandBuffer->SetLightAmbientColour(red, green, blue);
		return;
	}

	context.lightAmbientColour.x = red;
	context.lightAmbientColour.y = green;
	context.lightAmbientColour.z = blue;
	context.lightAmbientColour.w = 1.0f;
	context.lightingDirty = 1;
}

void Renderer::StateSetLightEnable(int light, bool enable)
{
	if (light >= 2)
	{
		return;
	}

	Context& context = this->getContext();
	if (context.commandBuffer != nullptr && context.commandBuffer->isActive != 0)
	{
		context.commandBuffer->SetLightEnable(light, enable);
		return;
	}

	context.lightEnabled[light] = enable ? 1 : 0;
	context.lightingDirty = 1;
}

void Renderer::StateSetColour(float r, float g, float b, float a)
{
	Context& context = this->getContext();
	if (context.commandBuffer != nullptr && context.commandBuffer->isActive != 0)
	{
		context.commandBuffer->SetColor(r, g, b, a);
		return;
	}

	ID3D11DeviceContext* d3d11 = context.m_pDeviceContext;
	const float colour[4] = { r, g, b, a };

	D3D11_MAPPED_SUBRESOURCE mapped = {};
	d3d11->Map(context.cbColour, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	std::memcpy(mapped.pData, colour, sizeof(colour));
	d3d11->Unmap(context.cbColour, 0);
}

void Renderer::StateSetDepthMask(bool enable)
{
	Context& context = this->getContext();
	if (context.commandBuffer != nullptr && context.commandBuffer->isActive != 0)
	{
		context.commandBuffer->SetDepthMask(enable);
		return;
	}

	context.depthStencilDesc.DepthWriteMask = enable ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
	context.m_pDeviceContext->OMSetDepthStencilState(this->GetManagedDepthStencilState(), 0);
	context.depthWriteEnabled = enable ? 1 : 0;
}

void Renderer::StateSetBlendEnable(bool enable)
{
	Context& context = this->getContext();
	if (context.commandBuffer != nullptr && context.commandBuffer->isActive != 0)
	{
		context.commandBuffer->SetBlendEnable(enable);
		return;
	}

	context.blendDesc.RenderTarget[0].BlendEnable = enable ? TRUE : FALSE;
	context.m_pDeviceContext->OMSetBlendState(this->GetManagedBlendState(), context.blendFactor, 0xFFFFFFFFu);
}

void Renderer::StateSetBlendFunc(int src, int dst)
{
	Context& context = this->getContext();
	if (context.commandBuffer != nullptr && context.commandBuffer->isActive != 0)
	{
		context.commandBuffer->SetBlendFunc(src, dst);
		return;
	}

	context.blendDesc.RenderTarget[0].SrcBlend = static_cast<D3D11_BLEND>(src);
	context.blendDesc.RenderTarget[0].DestBlend = static_cast<D3D11_BLEND>(dst);
	context.m_pDeviceContext->OMSetBlendState(this->GetManagedBlendState(), context.blendFactor, 0xFFFFFFFFu);
}

void Renderer::StateSetBlendFactor(unsigned int colour)
{
	Context& context = this->getContext();
	if (context.commandBuffer != nullptr && context.commandBuffer->isActive != 0)
	{
		context.commandBuffer->SetBlendFactor(colour);
		return;
	}

	const float scale = 255.0f;
	context.blendFactor[0] = static_cast<float>((colour >> 0) & 0xFFu) / scale;
	context.blendFactor[1] = static_cast<float>((colour >> 8) & 0xFFu) / scale;
	context.blendFactor[2] = static_cast<float>((colour >> 16) & 0xFFu) / scale;
	context.blendFactor[3] = static_cast<float>((colour >> 24) & 0xFFu) / scale;
	context.m_pDeviceContext->OMSetBlendState(this->GetManagedBlendState(), context.blendFactor, 0xFFFFFFFFu);
}

void Renderer::StateSetAlphaFunc(int, float param)
{
	Context& context = this->getContext();
	context.alphaReference = param;

	const float alpha[4] = { 0.0f, 0.0f, 0.0f, context.alphaTestEnabled ? context.alphaReference : 0.0f };
	D3D11_MAPPED_SUBRESOURCE mapped = {};
	context.m_pDeviceContext->Map(context.cbAlphaTest, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	std::memcpy(mapped.pData, alpha, sizeof(alpha));
	context.m_pDeviceContext->Unmap(context.cbAlphaTest, 0);
}

void Renderer::StateSetDepthFunc(int func)
{
	Context& context = this->getContext();
	if (context.commandBuffer != nullptr && context.commandBuffer->isActive != 0)
	{
		context.commandBuffer->SetDepthFunc(func);
		return;
	}

	context.depthStencilDesc.DepthFunc = static_cast<D3D11_COMPARISON_FUNC>(func);
	context.m_pDeviceContext->OMSetDepthStencilState(this->GetManagedDepthStencilState(), 0);
}

void Renderer::StateSetFaceCull(bool enable)
{
	Context& context = this->getContext();
	if (context.commandBuffer != nullptr && context.commandBuffer->isActive != 0)
	{
		context.commandBuffer->SetFaceCull(enable);
		return;
	}

	context.rasterizerDesc.CullMode = enable ? D3D11_CULL_BACK : D3D11_CULL_NONE;
	context.m_pDeviceContext->RSSetState(this->GetManagedRasterizerState());
	context.faceCullEnabled = enable ? 1 : 0;
}

void Renderer::StateSetFaceCullCW(bool enable)
{
	Context& context = this->getContext();
	if (context.faceCullEnabled)
	{
		context.rasterizerDesc.CullMode = enable ? D3D11_CULL_BACK : D3D11_CULL_FRONT;
	}
	else
	{
		context.rasterizerDesc.CullMode = D3D11_CULL_NONE;
	}

	context.m_pDeviceContext->RSSetState(this->GetManagedRasterizerState());
}

void Renderer::StateSetLineWidth(float)
{
}

void Renderer::StateSetWriteEnable(bool red, bool green, bool blue, bool alpha)
{
	Context& context = this->getContext();

	std::uint8_t mask = 0;
	mask |= red ? 0x1 : 0;
	mask |= green ? 0x2 : 0;
	mask |= blue ? 0x4 : 0;
	mask |= alpha ? 0x8 : 0;

	context.blendDesc.RenderTarget[0].RenderTargetWriteMask = mask;
	context.m_pDeviceContext->OMSetBlendState(this->GetManagedBlendState(), context.blendFactor, 0xFFFFFFFFu);
}

void Renderer::StateSetDepthTestEnable(bool enable)
{
	Context& context = this->getContext();
	if (context.commandBuffer != nullptr && context.commandBuffer->isActive != 0)
	{
		context.commandBuffer->SetDepthTestEnable(enable);
		return;
	}

	context.depthStencilDesc.DepthEnable = enable ? TRUE : FALSE;
	context.m_pDeviceContext->OMSetDepthStencilState(this->GetManagedDepthStencilState(), 0);
	context.depthTestEnabled = enable ? 1 : 0;
}

void Renderer::StateSetAlphaTestEnable(bool enable)
{
	Context& context = this->getContext();
	context.alphaTestEnabled = enable ? 1 : 0;

	const float alpha[4] = { 0.0f, 0.0f, 0.0f, enable ? context.alphaReference : 0.0f };
	D3D11_MAPPED_SUBRESOURCE mapped = {};
	context.m_pDeviceContext->Map(context.cbAlphaTest, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	std::memcpy(mapped.pData, alpha, sizeof(alpha));
	context.m_pDeviceContext->Unmap(context.cbAlphaTest, 0);
}

void Renderer::StateSetDepthSlopeAndBias(float slope, float bias)
{
	Context& context = this->getContext();

	const float scale = 65536.0f;
	context.rasterizerDesc.DepthBias = static_cast<int>(bias * scale);
	context.rasterizerDesc.SlopeScaledDepthBias = slope * scale;
	context.m_pDeviceContext->RSSetState(this->GetManagedRasterizerState());
}

void Renderer::UpdateFogState()
{
	Context& context = this->getContext();
	ID3D11DeviceContext* d3d11 = context.m_pDeviceContext;

	float fogParams[4] = {};
	if (context.fogEnabled)
	{
		if (context.fogMode == 1)
		{
			fogParams[0] = context.fogFarDistance;
			fogParams[1] = 1.0f / (context.fogFarDistance - context.fogNearDistance);
			fogParams[2] = 1.0f;
		}
		else
		{
			fogParams[0] = context.fogDensity;
			fogParams[2] = 2.0f;
		}
	}

	const float fogColour[4] =
	{
		context.fogColourRed,
		context.fogColourGreen,
		context.fogColourBlue,
		1.0f
	};

	D3D11_MAPPED_SUBRESOURCE mapped = {};
	d3d11->Map(context.cbFogParams, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	std::memcpy(mapped.pData, fogParams, sizeof(fogParams));
	d3d11->Unmap(context.cbFogParams, 0);

	d3d11->Map(context.cbFogColour, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	std::memcpy(mapped.pData, fogColour, sizeof(fogColour));
	d3d11->Unmap(context.cbFogColour, 0);
}

void Renderer::StateSetVertexTextureUV(float u, float v)
{
	Context& context = this->getContext();
	const float texgen[4] = { u - 1.0f, v - 1.0f, 0.0f, 0.0f };

	D3D11_MAPPED_SUBRESOURCE mapped = {};
	context.m_pDeviceContext->Map(context.cbVertexTexcoord, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	std::memcpy(mapped.pData, texgen, sizeof(texgen));
	context.m_pDeviceContext->Unmap(context.cbVertexTexcoord, 0);
}

void Renderer::UpdateTexGenState()
{
	Context& context = this->getContext();

	D3D11_MAPPED_SUBRESOURCE mapped = {};
	context.m_pDeviceContext->Map(context.cbTexGen, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	std::memcpy(mapped.pData, context.texGenMatrices, sizeof(context.texGenMatrices));
	context.m_pDeviceContext->Unmap(context.cbTexGen, 0);
}

void Renderer::UpdateLightingState()
{
	Context& context = this->getContext();
	if (!context.lightingDirty || !context.lightingEnabled)
	{
		return;
	}

	if (!context.lightEnabled[0])
	{
		std::memset(&context.lightDirection[0], 0, sizeof(context.lightDirection[0]));
		std::memset(&context.lightColour[0], 0, sizeof(context.lightColour[0]));
	}

	if (!context.lightEnabled[1])
	{
		std::memset(&context.lightDirection[1], 0, sizeof(context.lightDirection[1]));
		std::memset(&context.lightColour[1], 0, sizeof(context.lightColour[1]));
	}

	const std::size_t lightingBytes = sizeof(context.lightDirection) + sizeof(context.lightColour) + sizeof(context.lightAmbientColour);
	D3D11_MAPPED_SUBRESOURCE mapped = {};
	context.m_pDeviceContext->Map(context.cbLighting, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	std::memcpy(mapped.pData, context.lightDirection, lightingBytes);
	context.m_pDeviceContext->Unmap(context.cbLighting, 0);

	context.lightingDirty = 0;
}

void Renderer::StateSetLightDirection(int light, float x, float y, float z)
{
	if (light >= 2)
	{
		return;
	}

	Context& context = this->getContext();
	if (context.commandBuffer != nullptr && context.commandBuffer->isActive != 0)
	{
		context.commandBuffer->SetLightDirection(light, x, y, z);
		return;
	}

	const std::uint32_t stackIndex = context.matrixStackDepth[0];
	const DirectX::XMMATRIX& modelView = context.matrixStacks[0][stackIndex];
	const DirectX::XMVECTOR direction = DirectX::XMVectorSet(x, y, z, 0.0f);
	const DirectX::XMVECTOR transformed = DirectX::XMVector3TransformNormal(direction, modelView);
	const DirectX::XMVECTOR normalized = DirectX::XMVector3Normalize(transformed);

	DirectX::XMStoreFloat4(&context.lightDirection[light], normalized);
	context.lightingDirty = 1;
}

void Renderer::StateSetViewport(C4JRender::eViewportType viewportType)
{
	this->getContext();
	m_ViewportType = viewportType;

	const float fullWidth = static_cast<float>(backBufferWidth);
	const float fullHeight = static_cast<float>(backBufferHeight);

	float x = 0.0f;
	float y = 0.0f;
	float width = fullWidth;
	float height = fullHeight;

	switch (viewportType)
	{
	case C4JRender::VIEWPORT_TYPE_FULLSCREEN:
		break;
	case C4JRender::VIEWPORT_TYPE_SPLIT_TOP:
		y = fullHeight * 0.5f;
		height = fullHeight * 0.5f;
		break;
	case C4JRender::VIEWPORT_TYPE_SPLIT_BOTTOM:
		height = fullHeight * 0.5f;
		break;
	case C4JRender::VIEWPORT_TYPE_SPLIT_LEFT:
		width = fullWidth * 0.5f;
		break;
	case C4JRender::VIEWPORT_TYPE_SPLIT_RIGHT:
		x = fullWidth * 0.5f;
		width = fullWidth * 0.5f;
		break;
	case C4JRender::VIEWPORT_TYPE_QUADRANT_TOP_LEFT:
		width = fullWidth * 0.5f;
		height = fullHeight * 0.5f;
		break;
	case C4JRender::VIEWPORT_TYPE_QUADRANT_TOP_RIGHT:
		x = fullWidth * 0.5f;
		width = fullWidth * 0.5f;
		height = fullHeight * 0.5f;
		break;
	case C4JRender::VIEWPORT_TYPE_QUADRANT_BOTTOM_LEFT:
		y = fullHeight * 0.5f;
		width = fullWidth * 0.5f;
		height = fullHeight * 0.5f;
		break;
	case C4JRender::VIEWPORT_TYPE_QUADRANT_BOTTOM_RIGHT:
		x = fullWidth * 0.5f;
		y = fullHeight * 0.5f;
		width = fullWidth * 0.5f;
		height = fullHeight * 0.5f;
		break;
	default:
		break;
	}

	D3D11_VIEWPORT viewport = {};
	viewport.TopLeftX = x;
	viewport.TopLeftY = y;
	viewport.Width = width;
	viewport.Height = height;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	m_pDeviceContext->RSSetViewports(1, &viewport);
	m_pDeviceContext->OMSetRenderTargets(1, &renderTargetView, depthStencilView);
}

void Renderer::StateSetEnableViewportClipPlanes(bool)
{
}

void Renderer::StateSetTexGenCol(int col, float x, float y, float z, float w, bool eyeSpace)
{
	Context& context = this->getContext();

	DirectX::XMVECTOR plane = DirectX::XMVectorSet(x, y, z, w);
	if (eyeSpace)
	{
		DirectX::XMFLOAT4X4 modelView;
		std::memset(&modelView, 0, sizeof(modelView));
		std::memcpy(&modelView, this->MatrixGet(0), sizeof(modelView));

		DirectX::XMVECTOR determinant = DirectX::XMVectorZero();
		const DirectX::XMMATRIX inverse = DirectX::XMMatrixInverse(&determinant, DirectX::XMLoadFloat4x4(&modelView));
		plane = DirectX::XMVector4Transform(plane, inverse);
	}

	DirectX::XMFLOAT4 transformed;
	DirectX::XMStoreFloat4(&transformed, plane);

	const int activeSet = eyeSpace ? 0 : 1;
	const int inactiveSet = eyeSpace ? 1 : 0;

	float* active = reinterpret_cast<float*>(&context.texGenMatrices[activeSet]);
	active[col + 0] = transformed.x;
	active[col + 4] = transformed.y;
	active[col + 8] = transformed.z;
	active[col + 12] = transformed.w;

	float* inactive = reinterpret_cast<float*>(&context.texGenMatrices[inactiveSet]);
	inactive[col + 0] = 0.0f;
	inactive[col + 4] = 0.0f;
	inactive[col + 8] = 0.0f;
	inactive[col + 12] = 0.0f;
}

void Renderer::StateSetStencil(D3D11_COMPARISON_FUNC function, uint8_t stencil_ref, uint8_t stencil_func_mask, uint8_t stencil_write_mask)
{
	Context& context = this->getContext();

	D3D11_DEPTH_STENCIL_DESC desc = context.depthStencilDesc;
	desc.StencilEnable = TRUE;
	desc.StencilReadMask = stencil_func_mask;
	desc.StencilWriteMask = stencil_write_mask;
	desc.FrontFace.StencilFunc = function;
	desc.BackFace.StencilFunc = function;

	ID3D11DepthStencilState* state = nullptr;
	m_pDevice->CreateDepthStencilState(&desc, &state);
	m_pDeviceContext->OMSetDepthStencilState(state, stencil_ref);
	if (state != nullptr)
	{
		state->Release();
	}
}

void Renderer::StateSetForceLOD(int LOD)
{
	Context& context = this->getContext();
	context.forcedLOD = LOD;
}

void Renderer::StateUpdate()
{
	Context& context = this->getContext();
	this->StateSetFaceCull(context.faceCullEnabled != 0);
	this->StateSetDepthMask(context.depthWriteEnabled != 0);
	this->StateSetDepthTestEnable(context.depthTestEnabled != 0);
	this->StateSetAlphaTestEnable(context.alphaTestEnabled != 0);
}
