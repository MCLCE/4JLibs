#pragma once
#include "Renderer.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

Renderer::CommandBuffer::CommandBuffer(bool full)
	: m_vertexBuffer(nullptr)
	, m_vertexData(nullptr)
	, m_vertexDataLength(0)
	, m_commands()
	, m_allocated(0x1000)
	, isActive(full ? 1 : 0)
{
	std::memset(paddingAfterActive, 0, sizeof(paddingAfterActive));
	m_vertexData = std::malloc(m_allocated);
	EnterCriticalSection(&Renderer::totalAllocCS);
	Renderer::totalAlloc += static_cast<int>(m_allocated);
	LeaveCriticalSection(&Renderer::totalAllocCS);
}

Renderer::CommandBuffer::~CommandBuffer()
{
	if (m_vertexBuffer)
	{
		m_vertexBuffer->Release();
	}

	std::free(m_vertexData);

	EnterCriticalSection(&Renderer::totalAllocCS);
	Renderer::totalAlloc -= static_cast<int>(m_allocated);
	LeaveCriticalSection(&Renderer::totalAllocCS);
}

void Renderer::CommandBuffer::StartRecording()
{
}

void Renderer::CommandBuffer::EndRecording(ID3D11Device* device)
{
	if (m_vertexDataLength != 0)
	{
		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = m_vertexDataLength;
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

		D3D11_SUBRESOURCE_DATA data = {};
		data.pSysMem = m_vertexData;
		device->CreateBuffer(&desc, &data, &m_vertexBuffer);
	}

	std::free(m_vertexData);
	m_vertexData = nullptr;
}

std::uint64_t Renderer::CommandBuffer::GetAllocated()
{
	return m_allocated;
}

bool Renderer::CommandBuffer::IsBusy()
{
	return false;
}

void Renderer::CommandBuffer::AddMatrix(const float* matrix)
{
	Command command = {};
	command.m_command_type = COMMAND_ADD_MATRIX;
	std::memcpy(command.add_matrix.m_matrix, matrix, sizeof(command.add_matrix.m_matrix));
	m_commands.push_back(command);
}

void Renderer::CommandBuffer::AddVertices(unsigned int stride, unsigned int count, void* dataIn, Renderer::Context& context)
{
	if (context.matrixDirty[3])
	{
		this->AddMatrix(InternalRenderManager.MatrixGet(3));
		context.matrixDirty[3] = false;
	}

	const std::uint64_t vertexOffset = m_vertexDataLength;
	const std::uint64_t copySize = std::uint64_t(stride) * std::uint64_t(count);

	Command command = {};
	command.m_command_type = COMMAND_ADD_VERTICES;
	command.add_vertices.m_vertex_index_start = (unsigned int)vertexOffset;
	command.add_vertices.m_vertex_count = count;

	m_vertexDataLength = vertexOffset + copySize;
	if (m_vertexDataLength > m_allocated)
	{
		EnterCriticalSection(&Renderer::totalAllocCS);
		Renderer::totalAlloc -= static_cast<int>(m_allocated);
		LeaveCriticalSection(&Renderer::totalAllocCS);

		m_allocated = ((m_vertexDataLength + (0x1000u - 1u)) & ~(0x1000u - 1u));
		m_vertexData = std::realloc(m_vertexData, m_allocated);

		EnterCriticalSection(&Renderer::totalAllocCS);
		Renderer::totalAlloc += static_cast<int>(m_allocated);
		LeaveCriticalSection(&Renderer::totalAllocCS);
	}

	const std::size_t byteCount = std::size_t(stride) * std::size_t(count);
	std::memcpy(static_cast<std::uint8_t*>(m_vertexData) + vertexOffset, dataIn, byteCount);
	m_commands.push_back(command);
}

void Renderer::CommandBuffer::BindTexture(int idx)
{
	Command command = {};
	command.m_command_type = COMMAND_BIND_TEXTURE;
	command.bind_texture.m_texture_index = idx;
	m_commands.push_back(command);
}

void Renderer::CommandBuffer::SetColor(float r, float g, float b, float a)
{
	Command command = {};
	command.m_command_type = COMMAND_SET_COLOR;
	command.set_color.m_color[0] = r;
	command.set_color.m_color[1] = g;
	command.set_color.m_color[2] = b;
	command.set_color.m_color[3] = a;
	m_commands.push_back(command);
}

void Renderer::CommandBuffer::SetDepthFunc(int func)
{
	Command command = {};
	command.m_command_type = COMMAND_SET_DEPTH_FUNC;
	command.set_depth_func.m_depth_func = func;
	m_commands.push_back(command);
}

void Renderer::CommandBuffer::SetDepthMask(bool enable)
{
	Command command = {};
	command.m_command_type = COMMAND_SET_DEPTH_MASK;
	command.set_depth_mask.m_enable = enable;
	m_commands.push_back(command);
}

void Renderer::CommandBuffer::SetDepthTestEnable(bool enable)
{
	Command command = {};
	command.m_command_type = COMMAND_SET_DEPTH_TEST;
	command.set_depth_test.m_enable = enable;
	m_commands.push_back(command);
}

void Renderer::CommandBuffer::SetLightingEnable(bool enable)
{
	Command command = {};
	command.m_command_type = COMMAND_SET_LIGHTING_ENABLE;
	command.set_lighting_enable.m_enable = enable;
	m_commands.push_back(command);
}

void Renderer::CommandBuffer::SetLightEnable(int light, bool enable)
{
	Command command = {};
	command.m_command_type = COMMAND_SET_LIGHT_ENABLE;
	command.set_light_enable.m_light_index = light;
	command.set_light_enable.m_enable = enable;
	m_commands.push_back(command);
}

void Renderer::CommandBuffer::SetLightDirection(int light, float x, float y, float z)
{
	Renderer::Context& context = InternalRenderManager.getContext();
	const std::uint32_t depth = context.matrixStackDepth[3];
	const DirectX::XMMATRIX& matrix = context.matrixStacks[3][depth];

	DirectX::XMVECTOR direction = DirectX::XMVectorSet(x, y, z, 0.0f);
	direction = DirectX::XMVector3TransformNormal(direction, matrix);
	direction = DirectX::XMVector3Normalize(direction);

	Command command = {};
	command.m_command_type = COMMAND_SET_LIGHT_DIRECTION;
	command.set_light_direction.m_light_index = light;
	DirectX::XMFLOAT4 outDirection;
	DirectX::XMStoreFloat4(&outDirection, direction);
	command.set_light_direction.m_direction[0] = outDirection.x;
	command.set_light_direction.m_direction[1] = outDirection.y;
	command.set_light_direction.m_direction[2] = outDirection.z;
	command.set_light_direction.m_direction[3] = outDirection.w;
	m_commands.push_back(command);
}

void Renderer::CommandBuffer::SetLightColour(int light, float r, float g, float b)
{
	Command command = {};
	command.m_command_type = COMMAND_SET_LIGHT_COLOUR;
	command.set_light_colour.m_light_index = light;
	command.set_light_colour.m_color[0] = r;
	command.set_light_colour.m_color[1] = g;
	command.set_light_colour.m_color[2] = b;
	m_commands.push_back(command);
}

void Renderer::CommandBuffer::SetLightAmbientColour(float r, float g, float b)
{
	Command command = {};
	command.m_command_type = COMMAND_SET_LIGHT_AMBIENT_COLOUR;
	command.set_light_ambient_colour.m_color[0] = r;
	command.set_light_ambient_colour.m_color[1] = g;
	command.set_light_ambient_colour.m_color[2] = b;
	m_commands.push_back(command);
}

void Renderer::CommandBuffer::SetBlendEnable(bool enable)
{
	Command command = {};
	command.m_command_type = COMMAND_SET_BLEND_ENABLE;
	command.set_blend_enable.m_enable = enable;
	m_commands.push_back(command);
}

void Renderer::CommandBuffer::SetBlendFunc(int src, int dst)
{
	Command command = {};
	command.m_command_type = COMMAND_SET_BLEND_FUNC;
	command.set_blend_func.m_src = src;
	command.set_blend_func.m_dst = dst;
	m_commands.push_back(command);
}

void Renderer::CommandBuffer::SetBlendFactor(unsigned int factor)
{
	Command command = {};
	command.m_command_type = COMMAND_SET_BLEND_FACTOR;
	command.set_blend_factor.m_blend_factor = factor;
	m_commands.push_back(command);
}

void Renderer::CommandBuffer::SetFaceCull(bool enable)
{
	Command command = {};
	command.m_command_type = COMMAND_SET_FACE_CULL;
	command.set_face_cull.m_enable = enable;
	m_commands.push_back(command);
}

void Renderer::CommandBuffer::Render(C4JRender::eVertexType vertexType, Renderer::Context& context, int primitiveType)
{
	if (!m_vertexBuffer)
	{
		return;
	}

	int drawVertexType = vertexType;
	int shaderVertexType = drawVertexType;
	bool matrixOverride = false;

	for (std::vector<Command>::const_iterator it = m_commands.begin(); it != m_commands.end(); ++it)
	{
		const Command& command = *it;
		switch (command.m_command_type)
		{
		case COMMAND_ADD_MATRIX:
		{
			if (drawVertexType == C4JRender::VERTEX_TYPE_COMPRESSED)
			{
				const float row[4] =
				{
					command.add_matrix.m_matrix[12],
					command.add_matrix.m_matrix[13],
					command.add_matrix.m_matrix[14],
					command.add_matrix.m_matrix[15]
				};
				D3D11_MAPPED_SUBRESOURCE mappedAux0 = {};
				context.m_pDeviceContext->Map(context.cbAux0, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedAux0);
				std::memcpy(mappedAux0.pData, row, sizeof(row));
				context.m_pDeviceContext->Unmap(context.cbAux0, 0);
			}
			else
			{
				D3D11_MAPPED_SUBRESOURCE mappedMatrix1 = {};
				context.m_pDeviceContext->Map(context.cbMatrix1, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedMatrix1);
				std::memcpy(mappedMatrix1.pData, command.add_matrix.m_matrix, sizeof(command.add_matrix.m_matrix));
				context.m_pDeviceContext->Unmap(context.cbMatrix1, 0);
				matrixOverride = true;
			}
			break;
		}
		case COMMAND_ADD_VERTICES:
		{
			if (isActive)
			{
				InternalRenderManager.UpdateLightingState();

				if (drawVertexType == C4JRender::VERTEX_TYPE_PF3_TF2_CB4_NB4_XW1)
				{
					if (context.lightingEnabled)
					{
						drawVertexType = C4JRender::VERTEX_TYPE_PF3_TF2_CB4_NB4_XW1_LIT;
						shaderVertexType = C4JRender::VERTEX_TYPE_PF3_TF2_CB4_NB4_XW1_LIT;
					}
				}
				else if (drawVertexType == C4JRender::VERTEX_TYPE_PF3_TF2_CB4_NB4_XW1_LIT && !context.lightingEnabled)
				{
					drawVertexType = C4JRender::VERTEX_TYPE_PF3_TF2_CB4_NB4_XW1;
					shaderVertexType = C4JRender::VERTEX_TYPE_PF3_TF2_CB4_NB4_XW1;
				}

				if (static_cast<DWORD>(drawVertexType) != InternalRenderManager.activeVertexType)
				{
					context.m_pDeviceContext->VSSetShader(InternalRenderManager.vertexShaderTable[shaderVertexType], nullptr, 0);
					context.m_pDeviceContext->IASetInputLayout(InternalRenderManager.inputLayoutTable[shaderVertexType]);
					InternalRenderManager.activeVertexType = drawVertexType;
				}
			}

			unsigned int drawCount = command.add_vertices.m_vertex_count;
			bool drawIndexed = false;
			if (primitiveType == C4JRender::PRIMITIVE_TYPE_QUAD_LIST)
			{
				drawCount = (drawCount * 6u) / 4u;
				drawIndexed = true;
			}
			else if (primitiveType == C4JRender::PRIMITIVE_TYPE_TRIANGLE_FAN)
			{
				drawCount = (drawCount - 2u) * 3u;
				drawIndexed = true;
			}

			ID3D11Buffer* buffer = m_vertexBuffer;
			const UINT stride = InternalRenderManager.vertexStrideTable[drawVertexType];
			const UINT offset = command.add_vertices.m_vertex_index_start;
			context.m_pDeviceContext->IASetVertexBuffers(0, 1, &buffer, &stride, &offset);

			if (drawIndexed)
			{
				context.m_pDeviceContext->DrawIndexed(drawCount, 0, 0);
			}
			else
			{
				context.m_pDeviceContext->Draw(drawCount, 0);
			}
			break;
		}
		case COMMAND_BIND_TEXTURE:
		{
			context.boundTextureIndex = command.bind_texture.m_texture_index;
			ID3D11ShaderResourceView* view = InternalRenderManager.m_textures[context.boundTextureIndex].view;
			context.m_pDeviceContext->PSSetShaderResources(0, 1, &view);
			InternalRenderManager.UpdateTextureState(false);
			break;
		}
		case COMMAND_SET_COLOR:
		{
			D3D11_MAPPED_SUBRESOURCE mappedColour = {};
			context.m_pDeviceContext->Map(context.cbColour, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedColour);
			std::memcpy(mappedColour.pData, command.set_color.m_color, sizeof(command.set_color.m_color));
			context.m_pDeviceContext->Unmap(context.cbColour, 0);
			break;
		}
		case COMMAND_SET_DEPTH_FUNC:
		{
			context.depthStencilDesc.DepthFunc = static_cast<D3D11_COMPARISON_FUNC>(command.set_depth_func.m_depth_func);
			context.m_pDeviceContext->OMSetDepthStencilState(InternalRenderManager.GetManagedDepthStencilState(), 0);
			break;
		}
		case COMMAND_SET_DEPTH_MASK:
		{
			context.depthWriteEnabled = command.set_depth_mask.m_enable ? 1 : 0;
			context.depthStencilDesc.DepthWriteMask = command.set_depth_mask.m_enable ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
			context.m_pDeviceContext->OMSetDepthStencilState(InternalRenderManager.GetManagedDepthStencilState(), 0);
			break;
		}
		case COMMAND_SET_DEPTH_TEST:
		{
			context.depthTestEnabled = command.set_depth_test.m_enable ? 1 : 0;
			context.depthStencilDesc.DepthEnable = command.set_depth_test.m_enable ? TRUE : FALSE;
			context.m_pDeviceContext->OMSetDepthStencilState(InternalRenderManager.GetManagedDepthStencilState(), 0);
			break;
		}
		case COMMAND_SET_LIGHTING_ENABLE:
		{
			context.lightingEnabled = command.set_lighting_enable.m_enable ? 1 : 0;
			break;
		}
		case COMMAND_SET_LIGHT_ENABLE:
		{
			const int light = command.set_light_enable.m_light_index;
			if (light >= 0 && light < 2)
			{
				context.lightEnabled[light] = command.set_light_enable.m_enable ? 1 : 0;
				context.lightingDirty = 1;
			}
			break;
		}
		case COMMAND_SET_LIGHT_DIRECTION:
		{
			const int light = command.set_light_direction.m_light_index;
			if (light >= 0 && light < 2)
			{
				context.lightDirection[light].x = command.set_light_direction.m_direction[0];
				context.lightDirection[light].y = command.set_light_direction.m_direction[1];
				context.lightDirection[light].z = command.set_light_direction.m_direction[2];
				context.lightDirection[light].w = command.set_light_direction.m_direction[3];
				context.lightingDirty = 1;
			}
			break;
		}
		case COMMAND_SET_LIGHT_COLOUR:
		{
			const int light = command.set_light_colour.m_light_index;
			if (light >= 0 && light < 2)
			{
				context.lightColour[light].x = command.set_light_colour.m_color[0];
				context.lightColour[light].y = command.set_light_colour.m_color[1];
				context.lightColour[light].z = command.set_light_colour.m_color[2];
				context.lightColour[light].w = 1.0f;
				context.lightingDirty = 1;
			}
			break;
		}
		case COMMAND_SET_LIGHT_AMBIENT_COLOUR:
		{
			context.lightAmbientColour.x = command.set_light_ambient_colour.m_color[0];
			context.lightAmbientColour.y = command.set_light_ambient_colour.m_color[1];
			context.lightAmbientColour.z = command.set_light_ambient_colour.m_color[2];
			context.lightAmbientColour.w = 1.0f;
			context.lightingDirty = 1;
			break;
		}
		case COMMAND_SET_BLEND_ENABLE:
		{
			context.blendDesc.RenderTarget[0].BlendEnable = command.set_blend_enable.m_enable ? TRUE : FALSE;
			break;
		}
		case COMMAND_SET_BLEND_FUNC:
		{
			context.blendDesc.RenderTarget[0].SrcBlend = static_cast<D3D11_BLEND>(command.set_blend_func.m_src);
			context.blendDesc.RenderTarget[0].DestBlend = static_cast<D3D11_BLEND>(command.set_blend_func.m_dst);
			context.m_pDeviceContext->OMSetBlendState(InternalRenderManager.GetManagedBlendState(), context.blendFactor, 0xFFFFFFFFu);
			break;
		}
		case COMMAND_SET_BLEND_FACTOR:
		{
			const unsigned int factor = command.set_blend_factor.m_blend_factor;
			context.blendFactor[0] = float((factor >> 0) & 0xFFu) / 255.0f;
			context.blendFactor[1] = float((factor >> 8) & 0xFFu) / 255.0f;
			context.blendFactor[2] = float((factor >> 16) & 0xFFu) / 255.0f;
			context.blendFactor[3] = float((factor >> 24) & 0xFFu) / 255.0f;
			context.m_pDeviceContext->OMSetBlendState(InternalRenderManager.GetManagedBlendState(), context.blendFactor, 0xFFFFFFFFu);
			break;
		}
		case COMMAND_SET_FACE_CULL:
		{
			context.rasterizerDesc.CullMode = command.set_face_cull.m_enable ? D3D11_CULL_BACK : D3D11_CULL_NONE;
			context.m_pDeviceContext->RSSetState(InternalRenderManager.GetManagedRasterizerState());
			context.faceCullEnabled = command.set_face_cull.m_enable ? 1 : 0;
			break;
		}
		default:
			break;
		}
	}

	if (matrixOverride)
	{
		const DirectX::XMMATRIX identity = DirectX::XMMatrixIdentity();
		D3D11_MAPPED_SUBRESOURCE mappedIdentity = {};
		context.m_pDeviceContext->Map(context.cbMatrix1, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedIdentity);
		std::memcpy(mappedIdentity.pData, &identity, sizeof(identity));
		context.m_pDeviceContext->Unmap(context.cbMatrix1, 0);
	}
}

bool Renderer::CBuffCall(int index, bool full)
{
	EnterCriticalSection(&rtl_critical_section100);

	bool result = false;
	std::int16_t* externalToInternal = static_cast<std::int16_t*>(reservedRendererPtr2);
	const int internalIndex = externalToInternal[index];
	if (internalIndex >= 0)
	{
		Renderer::Context& context = this->getContext();
		const std::uint8_t vertexType = static_cast<std::uint8_t*>(reservedRendererPtr6)[internalIndex];
		const std::uint8_t primitiveType = reinterpret_cast<std::uint8_t*>(reservedRendererPtr1)[internalIndex];

		if (full)
		{
			if (context.matrixDirty[0])
			{
				D3D11_MAPPED_SUBRESOURCE mappedMatrix0 = {};
				context.m_pDeviceContext->Map(context.cbMatrix0, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedMatrix0);
				std::memcpy(mappedMatrix0.pData, this->MatrixGet(0), sizeof(DirectX::XMMATRIX));
				context.m_pDeviceContext->Unmap(context.cbMatrix0, 0);
				context.matrixDirty[0] = false;
			}

			if (context.matrixDirty[1])
			{
				D3D11_MAPPED_SUBRESOURCE mappedMatrix2 = {};
				context.m_pDeviceContext->Map(context.cbMatrix2, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedMatrix2);
				std::memcpy(mappedMatrix2.pData, this->MatrixGet(1), sizeof(DirectX::XMMATRIX));
				context.m_pDeviceContext->Unmap(context.cbMatrix2, 0);
				context.matrixDirty[1] = false;
			}

			if (context.matrixDirty[2])
			{
				D3D11_MAPPED_SUBRESOURCE mappedMatrix3 = {};
				context.m_pDeviceContext->Map(context.cbMatrix3, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedMatrix3);
				std::memcpy(mappedMatrix3.pData, this->MatrixGet(2), sizeof(DirectX::XMMATRIX));
				context.m_pDeviceContext->Unmap(context.cbMatrix3, 0);
				context.matrixDirty[2] = false;
			}

			this->UpdateFogState();
			this->UpdateLightingState();
			this->UpdateViewportState();
			this->UpdateTexGenState();

			if (vertexType != activeVertexType)
			{
				context.m_pDeviceContext->VSSetShader(vertexShaderTable[vertexType], nullptr, 0);
				context.m_pDeviceContext->IASetInputLayout(inputLayoutTable[vertexType]);
				activeVertexType = vertexType;
			}

			int pixelType = 0;
			if (static_cast<int>(context.forcedLOD) > -1)
			{
				const float forcedLod[4] = { static_cast<float>(static_cast<int>(context.forcedLOD)), 0.0f, 0.0f, 0.0f };
				D3D11_MAPPED_SUBRESOURCE mappedAux4 = {};
				context.m_pDeviceContext->Map(context.cbAux4, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedAux4);
				std::memcpy(mappedAux4.pData, forcedLod, sizeof(forcedLod));
				context.m_pDeviceContext->Unmap(context.cbAux4, 0);
				pixelType = C4JRender::PIXEL_SHADER_TYPE_FORCELOD;
			}

			if (static_cast<DWORD>(pixelType) != activePixelType)
			{
				context.m_pDeviceContext->PSSetShader(pixelShaderTable[pixelType], nullptr, 0);
				activePixelType = pixelType;
			}

			context.m_pDeviceContext->IASetPrimitiveTopology(m_Topologies[primitiveType]);

			ID3D11Buffer* indexBuffer = nullptr;
			if (primitiveType == C4JRender::PRIMITIVE_TYPE_QUAD_LIST)
			{
				indexBuffer = quadIndexBuffer;
			}
			else if (primitiveType == C4JRender::PRIMITIVE_TYPE_TRIANGLE_FAN)
			{
				indexBuffer = fanIndexBuffer;
			}

			context.m_pDeviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R16_UINT, 0);
		}

		Renderer::CommandBuffer* commandBuffer = static_cast<Renderer::CommandBuffer**>(reservedRendererPtr3)[internalIndex];
		commandBuffer->Render(static_cast<C4JRender::eVertexType>(vertexType), context, primitiveType);

		if (full)
		{
			this->MultWithStack(static_cast<DirectX::XMMATRIX*>(reservedRendererPtr4)[internalIndex]);
			context.matrixStacks[3][0] = DirectX::XMMatrixIdentity();
			context.matrixDirty[3] = true;
		}

		result = true;
	}

	LeaveCriticalSection(&rtl_critical_section100);
	return result;
}

void Renderer::CBuffClear(int index)
{
	EnterCriticalSection(&rtl_critical_section100);

	std::int16_t* externalToInternal = static_cast<std::int16_t*>(reservedRendererPtr2);
	const int internalIndex = externalToInternal[index];
	if (internalIndex >= 0)
	{
		this->DeleteInternalBuffer(internalIndex);
		externalToInternal[index] = static_cast<std::int16_t>(-2);
	}

	LeaveCriticalSection(&rtl_critical_section100);
}

int Renderer::CBuffCreate(int count)
{
	const int kMaxExternalCBuffers = 0x800000;

	EnterCriticalSection(&rtl_critical_section100);

	int first = reservedRendererDword1;
	std::int16_t* externalToInternal = static_cast<std::int16_t*>(reservedRendererPtr2);

	if (first < kMaxExternalCBuffers)
	{
		int probe = first;
		int end = first + count;
		while (true)
		{
			int cursor = probe;
			while (cursor < end && cursor < kMaxExternalCBuffers && externalToInternal[cursor] == static_cast<std::int16_t>(-1))
			{
				++cursor;
			}

			if (cursor >= end)
			{
				break;
			}

			++first;
			++probe;
			++end;
			if (first >= kMaxExternalCBuffers || end > kMaxExternalCBuffers)
			{
				first = -1;
				break;
			}
		}

		if (first >= 0)
		{
			const int allocationEnd = first + count;
			for (int i = first; i < allocationEnd; ++i)
			{
				externalToInternal[i] = static_cast<std::int16_t>(-2);
			}

			if (reservedRendererByte1)
			{
				reservedRendererDword1 = allocationEnd;
			}
		}
	}
	else
	{
		first = -1;
	}

	LeaveCriticalSection(&rtl_critical_section100);
	return first;
}

void Renderer::CBuffDeferredModeEnd()
{
	Renderer::Context& context = this->getContext();
	if (!context.deferredModeEnabled)
	{
		return;
	}

	EnterCriticalSection(&rtl_critical_section100);
	context.deferredModeEnabled = 0;

	std::int16_t* externalToInternal = static_cast<std::int16_t*>(reservedRendererPtr2);
	int* internalToExternal = static_cast<int*>(reservedRendererPtr5);
	std::uint8_t* internalVertexTypes = static_cast<std::uint8_t*>(reservedRendererPtr6);
	std::uint8_t* internalPrimitiveTypes = reinterpret_cast<std::uint8_t*>(reservedRendererPtr1);
	Renderer::CommandBuffer** internalBuffers = static_cast<Renderer::CommandBuffer**>(reservedRendererPtr3);
	DirectX::XMMATRIX* internalMatrices = static_cast<DirectX::XMMATRIX*>(reservedRendererPtr4);

	for (std::vector<Renderer::DeferredCBuff>::const_iterator it = context.deferredBuffers.begin(); it != context.deferredBuffers.end(); ++it)
	{
		const Renderer::DeferredCBuff& deferred = *it;
		const int existingIndex = externalToInternal[deferred.m_vertex_index];
		if (existingIndex >= 0)
		{
			this->DeleteInternalBuffer(existingIndex);
		}

		if (static_cast<int>(reservedRendererDword2 + reservedRendererDword3 + 10u) > 16000)
		{
			DebugBreak();
		}

		const int internalSlot = reservedRendererDword2;
		++reservedRendererDword2;

		externalToInternal[deferred.m_vertex_index] = static_cast<std::int16_t>(internalSlot);
		internalToExternal[internalSlot] = deferred.m_vertex_index;
		internalVertexTypes[internalSlot] = static_cast<std::uint8_t>(deferred.m_vertex_type);
		internalPrimitiveTypes[internalSlot] = static_cast<std::uint8_t>(deferred.m_primitive_type);
		internalBuffers[internalSlot] = deferred.m_command_buf;
		internalMatrices[internalSlot] = deferred.m_matrix;
	}

	context.deferredBuffers.clear();
	LeaveCriticalSection(&rtl_critical_section100);
}

void Renderer::CBuffDeferredModeStart()
{
	this->getContext().deferredModeEnabled = 1;
}

void Renderer::CBuffDelete(int first, int count)
{
	EnterCriticalSection(&rtl_critical_section100);

	std::int16_t* externalToInternal = static_cast<std::int16_t*>(reservedRendererPtr2);
	const int end = first + count;
	for (int i = first; i < end; ++i)
	{
		const int internalIndex = externalToInternal[i];
		if (internalIndex >= 0)
		{
			this->DeleteInternalBuffer(internalIndex);
		}

		externalToInternal[i] = static_cast<std::int16_t>(-1);
	}

	LeaveCriticalSection(&rtl_critical_section100);
}

void Renderer::CBuffEnd()
{
	Renderer::Context& context = this->getContext();
	EnterCriticalSection(&rtl_critical_section100);

	if (context.deferredModeEnabled)
	{
		Renderer::DeferredCBuff deferred;
		deferred.m_command_buf = context.commandBuffer;
		deferred.m_vertex_index = context.recordingBufferIndex;
		deferred.m_vertex_type = context.recordingVertexType;
		deferred.m_primitive_type = context.recordingPrimitiveType;
		deferred.m_matrix = context.matrixStacks[3][0];
		context.deferredBuffers.push_back(deferred);
	}
	else
	{
		std::int16_t* externalToInternal = static_cast<std::int16_t*>(reservedRendererPtr2);
		int* internalToExternal = static_cast<int*>(reservedRendererPtr5);
		std::uint8_t* internalVertexTypes = static_cast<std::uint8_t*>(reservedRendererPtr6);
		std::uint8_t* internalPrimitiveTypes = reinterpret_cast<std::uint8_t*>(reservedRendererPtr1);
		Renderer::CommandBuffer** internalBuffers = static_cast<Renderer::CommandBuffer**>(reservedRendererPtr3);
		DirectX::XMMATRIX* internalMatrices = static_cast<DirectX::XMMATRIX*>(reservedRendererPtr4);

		const int existingIndex = externalToInternal[context.recordingBufferIndex];
		if (existingIndex >= 0)
		{
			this->DeleteInternalBuffer(existingIndex);
		}

		if (static_cast<int>(reservedRendererDword2 + reservedRendererDword3 + 10u) > 16000)
		{
			DebugBreak();
		}

		const int internalSlot = reservedRendererDword2;
		++reservedRendererDword2;

		externalToInternal[context.recordingBufferIndex] = static_cast<std::int16_t>(internalSlot);
		internalToExternal[internalSlot] = context.recordingBufferIndex;
		internalVertexTypes[internalSlot] = static_cast<std::uint8_t>(context.recordingVertexType);
		internalPrimitiveTypes[internalSlot] = static_cast<std::uint8_t>(context.recordingPrimitiveType);
		internalBuffers[internalSlot] = context.commandBuffer;
		internalMatrices[internalSlot] = context.matrixStacks[3][0];
	}

	context.matrixModeType = 0;
	context.commandBuffer->EndRecording(m_pDevice);
	context.commandBuffer = nullptr;

	LeaveCriticalSection(&rtl_critical_section100);
}

void Renderer::CBuffLockStaticCreations()
{
	reservedRendererByte1 = 0;
}

int Renderer::CBuffSize(int index)
{
	if (index == -1)
	{
		return totalAlloc < 0 ? 0 : totalAlloc;
	}

	unsigned int size = 0;
	EnterCriticalSection(&rtl_critical_section100);
	const int internalIndex = static_cast<std::int16_t*>(reservedRendererPtr2)[index];
	if (internalIndex >= 0)
	{
		size = static_cast<Renderer::CommandBuffer**>(reservedRendererPtr3)[internalIndex]->GetAllocated();
	}
	LeaveCriticalSection(&rtl_critical_section100);
	return size;
}

void Renderer::CBuffStart(int index, bool full)
{
	Renderer::Context& context = this->getContext();
	context.commandBuffer = new (std::nothrow) Renderer::CommandBuffer(full);
	context.recordingBufferIndex = index;
	context.matrixModeType = 3;
	context.matrixStackDepth[3] = 0;
	context.matrixStacks[3][0] = DirectX::XMMatrixIdentity();
	context.matrixDirty[3] = true;
}

void Renderer::CBuffTick()
{
	const int kMaxInternalCBuffers = 16000;

	EnterCriticalSection(&rtl_critical_section100);

	Renderer::CommandBuffer** buffers = static_cast<Renderer::CommandBuffer**>(reservedRendererPtr3);
	const int firstPending = kMaxInternalCBuffers - static_cast<int>(reservedRendererDword3);
	for (int i = firstPending; i < kMaxInternalCBuffers; ++i)
	{
		Renderer::CommandBuffer* buffer = buffers[i];
		if (buffer)
		{
			delete buffer;
		}
		buffers[i] = nullptr;
	}

	reservedRendererDword3 = 0;
	LeaveCriticalSection(&rtl_critical_section100);
}

void Renderer::DeleteInternalBuffer(int index)
{
	const int kMaxInternalCBuffers = 16000;

	EnterCriticalSection(&rtl_critical_section100);

	Renderer::CommandBuffer** internalBuffers = static_cast<Renderer::CommandBuffer**>(reservedRendererPtr3);
	DirectX::XMMATRIX* internalMatrices = static_cast<DirectX::XMMATRIX*>(reservedRendererPtr4);
	std::uint8_t* internalVertexTypes = static_cast<std::uint8_t*>(reservedRendererPtr6);
	std::uint8_t* internalPrimitiveTypes = reinterpret_cast<std::uint8_t*>(reservedRendererPtr1);
	std::int16_t* externalToInternal = static_cast<std::int16_t*>(reservedRendererPtr2);
	int* internalToExternal = static_cast<int*>(reservedRendererPtr5);

	++reservedRendererDword3;
	const int recycledSlot = kMaxInternalCBuffers - static_cast<int>(reservedRendererDword3);

	internalBuffers[recycledSlot] = internalBuffers[index];
	internalMatrices[recycledSlot] = internalMatrices[index];

	if (reservedRendererDword2-- != 1)
	{
		const int lastActive = reservedRendererDword2;

		internalBuffers[index] = internalBuffers[lastActive];
		internalMatrices[index] = internalMatrices[lastActive];
		internalVertexTypes[index] = internalVertexTypes[lastActive];
		internalPrimitiveTypes[index] = internalPrimitiveTypes[lastActive];

		const int externalIndex = internalToExternal[lastActive];
		externalToInternal[externalIndex] = static_cast<std::int16_t>(index);
		internalToExternal[index] = externalIndex;
	}

	LeaveCriticalSection(&rtl_critical_section100);
}
