#pragma once
#include "CompiledShaders.h"
#include "Renderer.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>

static const unsigned int kVertexBufferSize = 0x100000u;
static const float kAspectRatio = 1.7777778f;
static const unsigned int kScreenGrabWidth = 1920u;
static const unsigned int kScreenGrabHeight = 1080u;
static const unsigned int kThumbnailSize = 64u;

static const unsigned int g_vertexStrides[C4JRender::VERTEX_TYPE_COUNT] = {
    32u, 16u, 32u, 32u};

Renderer InternalRenderManager;

DWORD Renderer::tlsIdx = TlsAlloc();
unsigned int Renderer::s_auiWidths[MAX_MIP_LEVELS + 1] = {1920u, 512u, 256u,
                                                          128u,  64u,  0u};
unsigned int Renderer::s_auiHeights[MAX_MIP_LEVELS + 1] = {
    1080u, 512u, 256u, 128u, 64u, 0xFFFFFFFFu};
D3D11_INPUT_ELEMENT_DESC Renderer::g_vertex_PTN_Elements_PF3_TF2_CB4_NB4_XW1[5] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
     D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12,
     D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 20, D3D11_INPUT_PER_VERTEX_DATA,
     0},
    {"NORMAL", 0, DXGI_FORMAT_R8G8B8A8_SNORM, 0, 24,
     D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 1, DXGI_FORMAT_R16G16_SINT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA,
     0},
};
D3D11_INPUT_ELEMENT_DESC Renderer::g_vertex_PTN_Elements_Compressed[2] = {
    {"POSITION", 0, DXGI_FORMAT_R16G16B16A16_SINT, 0, 0,
     D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R16G16B16A16_SINT, 0, 8,
     D3D11_INPUT_PER_VERTEX_DATA, 0},
};
D3D11_PRIMITIVE_TOPOLOGY Renderer::g_topologies[C4JRender::PRIMITIVE_TYPE_COUNT] = {
    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
    D3D11_PRIMITIVE_TOPOLOGY_LINELIST,
    D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP,
};
int Renderer::totalAlloc = 0;
_RTL_CRITICAL_SECTION Renderer::totalAllocCS = {0};

Renderer::Context::Context(ID3D11Device *device,
                           ID3D11DeviceContext *deviceContext)
    : m_pDeviceContext(deviceContext), userAnnotation(nullptr),
      contextStateFlags(0), matrixModeType(0), boundTextureIndex(0),
      faceCullEnabled(1), depthTestEnabled(1), alphaTestEnabled(0),
      alphaReference(1.0f), depthWriteEnabled(1), fogEnabled(0),
      fogNearDistance(0.0f), fogFarDistance(0.0f), fogDensity(0.0f),
      fogColourRed(0.0f), fogColourBlue(0.0f), fogColourGreen(0.0f), fogMode(0),
      lightingEnabled(0), lightingDirty(0), forcedLOD(0xFFFFFFFFu),
      cbMatrix0(nullptr), cbMatrix1(nullptr), cbMatrix2(nullptr),
      cbMatrix3(nullptr), cbVertexTexcoord(nullptr), cbFogParams(nullptr),
      cbLighting(nullptr), cbTexGen(nullptr), cbAux0(nullptr), cbAux1(nullptr),
      cbColour(nullptr), cbFogColour(nullptr), cbAux2(nullptr),
      cbAlphaTest(nullptr), cbAux3(nullptr), cbAux4(nullptr),
      dynamicVertexBase(0), dynamicVertexOffset(0),
      dynamicVertexBuffer(nullptr), commandBuffer(nullptr),
      recordingBufferIndex(0), recordingVertexType(0),
      recordingPrimitiveType(0), deferredModeEnabled(0), deferredBuffers(),
      reservedContext0(0), reservedContext1(0) {
  deviceContext->QueryInterface(IID_PPV_ARGS(&userAnnotation));
  std::memset(matrixStacks, 0, sizeof(matrixStacks));
  std::memset(matrixDirty, 0, sizeof(matrixDirty));
  std::memset(matrixStackDepth, 0, sizeof(matrixStackDepth));
  std::memset(lightEnabled, 0, sizeof(lightEnabled));
  std::memset(lightDirection, 0, sizeof(lightDirection));
  std::memset(lightColour, 0, sizeof(lightColour));
  std::memset(&lightAmbientColour, 0, sizeof(lightAmbientColour));
  std::memset(texGenMatrices, 0, sizeof(texGenMatrices));
  std::memset(&blendDesc, 0, sizeof(blendDesc));
  std::memset(&depthStencilDesc, 0, sizeof(depthStencilDesc));
  std::memset(&rasterizerDesc, 0, sizeof(rasterizerDesc));
  blendFactor[0] = 0.0f;
  blendFactor[1] = 0.0f;
  blendFactor[2] = 0.0f;
  blendFactor[3] = 0.0f;

  const DirectX::XMMATRIX identity = DirectX::XMMatrixIdentity();
  for (unsigned int i = 0; i < 4; ++i) {
    matrixStacks[i][0] = identity;
    matrixStackDepth[i] = 0;
  }

  blendDesc.AlphaToCoverageEnable = FALSE;
  blendDesc.IndependentBlendEnable = FALSE;
  blendDesc.RenderTarget[0].BlendEnable = FALSE;
  blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
  blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
  blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
  blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
  blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
  blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
  blendDesc.RenderTarget[0].RenderTargetWriteMask =
      D3D11_COLOR_WRITE_ENABLE_ALL;

  depthStencilDesc.DepthEnable = TRUE;
  depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
  depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
  depthStencilDesc.StencilEnable = FALSE;
  depthStencilDesc.StencilReadMask = 0xFFu;
  depthStencilDesc.StencilWriteMask = 0xFFu;
  depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
  depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
  depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
  depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
  depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
  depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
  depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
  depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

  rasterizerDesc.FillMode = D3D11_FILL_SOLID;
  rasterizerDesc.CullMode = D3D11_CULL_BACK;
  rasterizerDesc.FrontCounterClockwise = TRUE;
  rasterizerDesc.DepthBias = 0;
  rasterizerDesc.DepthBiasClamp = 0.0f;
  rasterizerDesc.SlopeScaledDepthBias = 0.0f;
  rasterizerDesc.DepthClipEnable = TRUE;
  rasterizerDesc.ScissorEnable = FALSE;
  rasterizerDesc.MultisampleEnable = TRUE;
  rasterizerDesc.AntialiasedLineEnable = FALSE;

  std::memset(lightDirection, 0, sizeof(lightDirection));
  std::memset(lightColour, 0, sizeof(lightColour));
  std::memset(&lightAmbientColour, 0, sizeof(lightAmbientColour));
  std::memset(texGenMatrices, 0, sizeof(texGenMatrices));

  const float zero4[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  const float one4[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  const float alpha4[4] = {0.0f, 0.0f, 0.0f, 1.0f};

  D3D11_BUFFER_DESC cbDesc = {};
  cbDesc.Usage = D3D11_USAGE_DYNAMIC;
  cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

  D3D11_SUBRESOURCE_DATA cbData = {};
  cbDesc.ByteWidth = sizeof(DirectX::XMMATRIX);
  cbData.pSysMem = &identity;
  device->CreateBuffer(&cbDesc, &cbData, &cbMatrix0);
  device->CreateBuffer(&cbDesc, &cbData, &cbMatrix1);
  device->CreateBuffer(&cbDesc, &cbData, &cbMatrix2);
  device->CreateBuffer(&cbDesc, &cbData, &cbMatrix3);

  cbDesc.ByteWidth = sizeof(zero4);
  cbData.pSysMem = zero4;
  device->CreateBuffer(&cbDesc, &cbData, &cbVertexTexcoord);
  device->CreateBuffer(&cbDesc, &cbData, &cbFogParams);

  const UINT lightingBytes =
      sizeof(lightDirection) + sizeof(lightColour) + sizeof(lightAmbientColour);
  cbDesc.ByteWidth = lightingBytes;
  cbData.pSysMem = lightDirection;
  device->CreateBuffer(&cbDesc, &cbData, &cbLighting);

  cbDesc.ByteWidth = sizeof(texGenMatrices);
  cbData.pSysMem = texGenMatrices;
  device->CreateBuffer(&cbDesc, &cbData, &cbTexGen);

  cbDesc.ByteWidth = sizeof(zero4);
  cbData.pSysMem = zero4;
  device->CreateBuffer(&cbDesc, &cbData, &cbAux0);
  device->CreateBuffer(&cbDesc, &cbData, &cbAux1);

  cbDesc.ByteWidth = sizeof(one4);
  cbData.pSysMem = one4;
  device->CreateBuffer(&cbDesc, &cbData, &cbColour);
  device->CreateBuffer(&cbDesc, &cbData, &cbFogColour);
  device->CreateBuffer(&cbDesc, &cbData, &cbAux2);

  cbDesc.ByteWidth = sizeof(alpha4);
  cbData.pSysMem = alpha4;
  device->CreateBuffer(&cbDesc, &cbData, &cbAlphaTest);

  cbDesc.ByteWidth = sizeof(zero4);
  cbData.pSysMem = zero4;
  device->CreateBuffer(&cbDesc, &cbData, &cbAux3);
  device->CreateBuffer(&cbDesc, &cbData, &cbAux4);

  deviceContext->VSSetConstantBuffers(0, 10, &cbMatrix0);
  deviceContext->PSSetConstantBuffers(0, 6, &cbColour);

  {
    void *dynamicVertexPtr = operator new[](kVertexBufferSize);
    dynamicVertexBase = reinterpret_cast<std::uint64_t>(dynamicVertexPtr);
  }
  dynamicVertexOffset = 0;

  D3D11_BUFFER_DESC vbDesc = {};
  vbDesc.ByteWidth = kVertexBufferSize;
  vbDesc.Usage = D3D11_USAGE_DYNAMIC;
  vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  device->CreateBuffer(&vbDesc, nullptr, &dynamicVertexBuffer);
}

void Renderer::BeginConditionalRendering(int) {}

void Renderer::BeginConditionalSurvey(int) {}

void Renderer::CaptureScreen(ImageFileBuffer *, XSOCIAL_PREVIEWIMAGE *) {}

void Renderer::Clear(int flags, D3D11_RECT *) {
  Renderer::Context &context = this->getContext();

  ID3D11BlendState *blendState = nullptr;
  ID3D11DepthStencilState *depthState = nullptr;
  ID3D11RasterizerState *rasterizerState = nullptr;

  D3D11_BLEND_DESC blendDesc = {};
  blendDesc.AlphaToCoverageEnable = FALSE;
  blendDesc.IndependentBlendEnable = FALSE;
  blendDesc.RenderTarget[0].BlendEnable = FALSE;
  blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
  blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
  blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
  blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
  blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
  blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
  blendDesc.RenderTarget[0].RenderTargetWriteMask =
      (flags & CLEAR_COLOUR_FLAG) ? D3D11_COLOR_WRITE_ENABLE_ALL : 0;
  m_pDevice->CreateBlendState(&blendDesc, &blendState);

  D3D11_DEPTH_STENCIL_DESC depthDesc = {};
  depthDesc.DepthEnable = (flags & CLEAR_DEPTH_FLAG) ? TRUE : FALSE;
  depthDesc.DepthWriteMask = depthDesc.DepthEnable
                                 ? D3D11_DEPTH_WRITE_MASK_ALL
                                 : D3D11_DEPTH_WRITE_MASK_ZERO;
  depthDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
  depthDesc.StencilEnable = FALSE;
  depthDesc.StencilReadMask = 0xFFu;
  depthDesc.StencilWriteMask = 0xFFu;
  depthDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
  depthDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
  depthDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
  depthDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
  depthDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
  depthDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
  depthDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
  depthDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
  m_pDevice->CreateDepthStencilState(&depthDesc, &depthState);

  D3D11_RASTERIZER_DESC rasterDesc = {};
  rasterDesc.FillMode = D3D11_FILL_SOLID;
  rasterDesc.CullMode = D3D11_CULL_NONE;
  rasterDesc.DepthClipEnable = TRUE;
  rasterDesc.MultisampleEnable = TRUE;
  m_pDevice->CreateRasterizerState(&rasterDesc, &rasterizerState);

  context.m_pDeviceContext->VSSetShader(screenClearVertexShader, nullptr, 0);
  context.m_pDeviceContext->IASetInputLayout(nullptr);
  context.m_pDeviceContext->PSSetShader(screenClearPixelShader, nullptr, 0);
  context.m_pDeviceContext->OMSetRenderTargets(1, &renderTargetView,
                                               depthStencilView);
  context.m_pDeviceContext->OMSetBlendState(blendState, nullptr, 0xFFFFFFFFu);
  context.m_pDeviceContext->OMSetDepthStencilState(depthState, 0);
  context.m_pDeviceContext->RSSetState(rasterizerState);
  context.m_pDeviceContext->PSSetShaderResources(0, 0, nullptr);
  context.m_pDeviceContext->IASetPrimitiveTopology(
      D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  context.m_pDeviceContext->Draw(4, 0);

  if (blendState) {
    blendState->Release();
    blendState = nullptr;
  }
  if (depthState) {
    depthState->Release();
    depthState = nullptr;
  }
  if (rasterizerState) {
    rasterizerState->Release();
    rasterizerState = nullptr;
  }

  context.m_pDeviceContext->OMSetBlendState(this->GetManagedBlendState(),
                                            context.blendFactor, 0xFFFFFFFFu);
  context.m_pDeviceContext->OMSetDepthStencilState(
      this->GetManagedDepthStencilState(), 0);
  context.m_pDeviceContext->RSSetState(this->GetManagedRasterizerState());
  context.m_pDeviceContext->OMSetRenderTargets(1, &renderTargetView,
                                               depthStencilView);
  activeVertexType = 0xFFFFFFFFu;
  activePixelType = 0xFFFFFFFFu;
}

void Renderer::ConvertLinearToPng(ImageFileBuffer *pngOut,
                                  unsigned char *linearData, unsigned int width,
                                  unsigned int height) {
  const size_t dataSize = size_t(width) * size_t(height) * 4u;
  const size_t outputCapacity = ((dataSize * 24u) / 20u) + 256u;
  const int outputCapacityInt = int(outputCapacity);
  const int widthInt = int(width);
  const int heightInt = int(height);
  void *outputBuffer = std::malloc(outputCapacity);
  int outputLength = 0;
  this->SaveTextureDataToMemory(outputBuffer, outputCapacityInt, &outputLength,
                          widthInt, heightInt,
                          reinterpret_cast<int *>(linearData));
  pngOut->m_type = ImageFileBuffer::e_typePNG;
  pngOut->m_pBuffer = outputBuffer;
  pngOut->m_bufferSize = outputLength;
}

void Renderer::DoScreenGrabOnNextPresent() { shouldScreenGrabNextFrame = 1; }

void Renderer::EndConditionalRendering() {}

void Renderer::EndConditionalSurvey() {}

void Renderer::BeginEvent(LPCWSTR eventName) {
  Renderer::Context *context =
      static_cast<Renderer::Context *>(TlsGetValue(Renderer::tlsIdx));
  if (context &&
      context->m_pDeviceContext->GetType() != D3D11_DEVICE_CONTEXT_DEFERRED &&
      context->userAnnotation) {
    context->userAnnotation->BeginEvent(eventName);
    ++context->contextStateFlags;
  }
}

void Renderer::EndEvent() {
  Renderer::Context *context =
      static_cast<Renderer::Context *>(TlsGetValue(Renderer::tlsIdx));
  if (context &&
      context->m_pDeviceContext->GetType() != D3D11_DEVICE_CONTEXT_DEFERRED &&
      context->userAnnotation) {
    context->userAnnotation->EndEvent();
    if (context->contextStateFlags > 0) {
      --context->contextStateFlags;
    }
  }
}

void Renderer::Initialise(ID3D11Device *pDevice, IDXGISwapChain *pSwapChain) {
  m_pDevice = pDevice;
  m_pDeviceContext = this->InitialiseContext(true);
  m_pSwapChain = pSwapChain;

  reservedRendererPtr2 = operator new[](0x1000000u);
  reservedRendererPtr3 = operator new[](0x1F400u);
  reservedRendererPtr4 = operator new[](0xFA000u);
  reservedRendererPtr5 = operator new[](0xFA00u);
  reservedRendererPtr6 = operator new[](0x3E80u);
  {
    void *reservedPtr = operator new[](0x3E80u);
    reservedRendererPtr1 = reinterpret_cast<std::uint64_t>(reservedPtr);
  }

  std::memset(reservedRendererPtr2, 0xFF, 0x1000000u);
  std::memset(reservedRendererPtr3, 0, 0xFA00u);
  std::memset(reservedRendererPtr5, 0, 0xFA00u);
  std::memset(reservedRendererPtr6, 0, 0x3E80u);
  {
    void *reservedPtr = reinterpret_cast<void *>(reservedRendererPtr1);
    std::memset(reservedPtr, 0, 0x3E80u);
  }
  reservedRendererDword3 = 0;

  shouldScreenGrabNextFrame = 0;
  suspended = 0;

  this->SetupShaders();
  const float clearColour[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  this->SetClearColour(clearColour);

  UINT backBufferSampleCount = 1;
  UINT backBufferSampleQuality = 0;
  ID3D11Texture2D *backBuffer = nullptr;
  pSwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
  if (backBuffer) {
    D3D11_TEXTURE2D_DESC backDesc = {};
    backBuffer->GetDesc(&backDesc);
    backBufferWidth = backDesc.Width;
    backBufferHeight = backDesc.Height;
    backBufferSampleCount = backDesc.SampleDesc.Count;
    backBufferSampleQuality = backDesc.SampleDesc.Quality;
    m_pDevice->CreateRenderTargetView(backBuffer, nullptr, &renderTargetView);
    m_pDevice->CreateShaderResourceView(backBuffer, nullptr,
                                        &renderTargetShaderResourceView);
    backBuffer->Release();
  }

  ID3D11RenderTargetView *boundRTV = nullptr;
  m_pDeviceContext->OMGetRenderTargets(1, &boundRTV, &depthStencilView);
  if (boundRTV) {
    boundRTV->Release();
  }

  if (!depthStencilView && backBufferWidth != 0 && backBufferHeight != 0) {
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = backBufferWidth;
    depthDesc.Height = backBufferHeight;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = backBufferSampleCount;
    depthDesc.SampleDesc.Quality = backBufferSampleQuality;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    ID3D11Texture2D *depthTexture = nullptr;
    if (SUCCEEDED(
            m_pDevice->CreateTexture2D(&depthDesc, nullptr, &depthTexture))) {
      m_pDevice->CreateDepthStencilView(depthTexture, nullptr,
                                        &depthStencilView);
      depthTexture->Release();
    }
  }

  for (unsigned int i = 0; i < MAX_MIP_LEVELS - 1; ++i) {
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = s_auiWidths[i + 1];
    desc.Height = s_auiHeights[i + 1];
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    m_pDevice->CreateTexture2D(&desc, nullptr, &renderTargetTextures[i]);
    m_pDevice->CreateRenderTargetView(renderTargetTextures[i], nullptr,
                                      &renderTargetViews[i]);
    m_pDevice->CreateShaderResourceView(renderTargetTextures[i], nullptr,
                                        &renderTargetShaderResourceViews[i]);
  }

  std::memset(m_textures, 0, sizeof(m_textures));
  defaultTextureIndex = this->TextureCreate();
  this->TextureBind(defaultTextureIndex);

  unsigned char *defaultTextureData = new unsigned char[0x400u];
  std::memset(defaultTextureData, 0xFF, 0x400u);
  this->TextureData(16, 16, defaultTextureData, 0,
              C4JRender::TEXTURE_FORMAT_RxGyBzAw);
  delete[] defaultTextureData;

  presentCount = 0;
  rendererFlag0 = 0;
  reservedRendererWord0 = 10922;
  this->StateSetViewport(C4JRender::VIEWPORT_TYPE_FULLSCREEN);
  this->StateSetVertexTextureUV(0.0f, 0.0f);
  this->TextureBindVertex(-1);
  InitializeCriticalSection(&rtl_critical_section100);
  reservedRendererDword1 = 0;
  activeVertexType = 0xFFFFFFFFu;
  activePixelType = 0xFFFFFFFFu;
  reservedRendererByte1 = 1;
  reservedRendererByte0 = 0;

  unsigned short *quadIndices = new unsigned short[0x18000u];
  for (unsigned int i = 0; i < 0x4000u; ++i) {
    const unsigned short base = (unsigned short)(i * 4u);
    const unsigned int offset = i * 6u;
    quadIndices[offset + 0] = base;
    quadIndices[offset + 1] = (unsigned short)(base + 1u);
    quadIndices[offset + 2] = (unsigned short)(base + 3u);
    quadIndices[offset + 3] = (unsigned short)(base + 1u);
    quadIndices[offset + 4] = (unsigned short)(base + 2u);
    quadIndices[offset + 5] = (unsigned short)(base + 3u);
  }
  D3D11_BUFFER_DESC quadIndexDesc = {};
  quadIndexDesc.ByteWidth = 0x30000u;
  quadIndexDesc.Usage = D3D11_USAGE_IMMUTABLE;
  quadIndexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
  D3D11_SUBRESOURCE_DATA quadIndexData = {};
  quadIndexData.pSysMem = quadIndices;
  m_pDevice->CreateBuffer(&quadIndexDesc, &quadIndexData, &quadIndexBuffer);
  delete[] quadIndices;

  unsigned short *fanIndices = new unsigned short[0x2FFFAu];
  for (unsigned int i = 0; i < 65534u; ++i) {
    const unsigned int offset = i * 3u;
    fanIndices[offset + 0] = 0u;
    fanIndices[offset + 1] = (unsigned short)(i + 1u);
    fanIndices[offset + 2] = (unsigned short)(i + 2u);
  }
  D3D11_BUFFER_DESC fanIndexDesc = {};
  fanIndexDesc.ByteWidth = 0x5FFF4u;
  fanIndexDesc.Usage = D3D11_USAGE_IMMUTABLE;
  fanIndexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
  D3D11_SUBRESOURCE_DATA fanIndexData = {};
  fanIndexData.pSysMem = fanIndices;
  m_pDevice->CreateBuffer(&fanIndexDesc, &fanIndexData, &fanIndexBuffer);
  delete[] fanIndices;

  InitializeCriticalSection(&Renderer::totalAllocCS);
  m_Topologies = g_topologies;
}

ID3D11DeviceContext *Renderer::InitialiseContext(bool fromPresent) {
  ID3D11DeviceContext *deviceContext = nullptr;
  if (fromPresent) {
    m_pDevice->GetImmediateContext(&deviceContext);
  } else {
    m_pDevice->CreateDeferredContext(0, &deviceContext);
  }

  Renderer::Context *context =
      new (std::nothrow) Renderer::Context(m_pDevice, deviceContext);
  TlsSetValue(Renderer::tlsIdx, context);
  return deviceContext;
}

bool Renderer::IsHiDef() { return true; }

bool Renderer::IsWidescreen() { return true; }

void Renderer::Present() {
  if (shouldScreenGrabNextFrame) {
    int *linearData = new int[kScreenGrabWidth * kScreenGrabHeight];
    ID3D11Texture2D *backBuffer = nullptr;
    ID3D11Texture2D *stagingTexture = nullptr;

    m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (backBuffer) {
      D3D11_TEXTURE2D_DESC desc = {};
      backBuffer->GetDesc(&desc);
      desc.Usage = D3D11_USAGE_STAGING;
      desc.BindFlags = 0;
      desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
      desc.MiscFlags = 0;
      m_pDevice->CreateTexture2D(&desc, nullptr, &stagingTexture);
    }

    if (stagingTexture && backBuffer) {
      m_pDeviceContext->CopyResource(stagingTexture, backBuffer);
      D3D11_MAPPED_SUBRESOURCE mapped = {};
      if (SUCCEEDED(m_pDeviceContext->Map(stagingTexture, 0, D3D11_MAP_READ, 0,
                                          &mapped))) {
        unsigned char *dst = reinterpret_cast<unsigned char *>(linearData);
        const unsigned char *src =
            reinterpret_cast<const unsigned char *>(mapped.pData);
        for (unsigned int y = 0; y < kScreenGrabHeight; ++y) {
          unsigned char *dstRow = dst + y * (kScreenGrabWidth * 4u);
          const unsigned char *srcRow = src + y * mapped.RowPitch;
          std::memcpy(dstRow, srcRow, kScreenGrabWidth * 4u);
          for (unsigned int x = 0; x < kScreenGrabWidth; ++x) {
            dstRow[x * 4u + 3u] = 0xFF;
          }
        }
        m_pDeviceContext->Unmap(stagingTexture, 0);
      }
    }

    static int count = 0;
    char fileName[304];
    sprintf_s(fileName, "d:\\screen%d.png", count++);
    D3DXIMAGE_INFO info;
    info.Width = kScreenGrabWidth;
    info.Height = kScreenGrabHeight;
    this->SaveTextureData(fileName, &info, linearData);
    delete[] linearData;

    if (stagingTexture) {
      stagingTexture->Release();
      stagingTexture = nullptr;
    }
    if (backBuffer) {
      backBuffer->Release();
      backBuffer = nullptr;
    }
    shouldScreenGrabNextFrame = 0;
  }

  m_pSwapChain->Present(1, 0);
  ++presentCount;
}

void Renderer::Resume() { suspended = 0; }

void Renderer::SetClearColour(const float colourRGBA[4]) {
  std::memcpy(m_fClearColor, colourRGBA, sizeof(m_fClearColor));

  Renderer::Context *context =
      static_cast<Renderer::Context *>(TlsGetValue(Renderer::tlsIdx));
  if (context) {
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    context->m_pDeviceContext->Map(context->cbAux3, 0, D3D11_MAP_WRITE_DISCARD,
                                   0, &mapped);
    std::memcpy(mapped.pData, colourRGBA, sizeof(float) * 4);
    context->m_pDeviceContext->Unmap(context->cbAux3, 0);
  }
}

void Renderer::SetupShaders() {
  vertexShaderTable = new ID3D11VertexShader *[C4JRender::VERTEX_TYPE_COUNT];
  pixelShaderTable = new ID3D11PixelShader *[C4JRender::PIXEL_SHADER_COUNT];
  vertexStrideTable = new unsigned int[C4JRender::VERTEX_TYPE_COUNT];
  inputLayoutTable = new ID3D11InputLayout *[C4JRender::VERTEX_TYPE_COUNT];

  for (unsigned int i = 0; i < C4JRender::VERTEX_TYPE_COUNT; ++i) {
    vertexShaderTable[i] = nullptr;
    inputLayoutTable[i] = nullptr;
    vertexStrideTable[i] = g_vertexStrides[i];
  }

  for (unsigned int i = 0; i < C4JRender::PIXEL_SHADER_COUNT; ++i) {
    pixelShaderTable[i] = nullptr;
  }

  screenSpaceVertexShader = nullptr;
  screenClearVertexShader = nullptr;
  screenSpacePixelShader = nullptr;
  screenClearPixelShader = nullptr;

  m_pDevice->CreateVertexShader(
      VS_PF3_TF2_CB4_NB4_XW1_Data, sizeof(VS_PF3_TF2_CB4_NB4_XW1_Data), nullptr,
      &vertexShaderTable[C4JRender::VERTEX_TYPE_PF3_TF2_CB4_NB4_XW1]);
  m_pDevice->CreateVertexShader(
      VS_Compressed_Data, sizeof(VS_Compressed_Data), nullptr,
      &vertexShaderTable[C4JRender::VERTEX_TYPE_COMPRESSED]);
  m_pDevice->CreateVertexShader(
      VS_PF3_TF2_CB4_NB4_XW1_Lighting_Data,
      sizeof(VS_PF3_TF2_CB4_NB4_XW1_Lighting_Data), nullptr,
      &vertexShaderTable[C4JRender::VERTEX_TYPE_PF3_TF2_CB4_NB4_XW1_LIT]);
  m_pDevice->CreateVertexShader(
      VS_PF3_TF2_CB4_NB4_XW1_Texgen_Data,
      sizeof(VS_PF3_TF2_CB4_NB4_XW1_Texgen_Data), nullptr,
      &vertexShaderTable[C4JRender::VERTEX_TYPE_PF3_TF2_CB4_NB4_XW1_TEXGEN]);
  m_pDevice->CreateVertexShader(VS_ScreenSpace_Data,
                                sizeof(VS_ScreenSpace_Data), nullptr,
                                &screenSpaceVertexShader);
  m_pDevice->CreateVertexShader(VS_ScreenClear_Data,
                                sizeof(VS_ScreenClear_Data), nullptr,
                                &screenClearVertexShader);

  m_pDevice->CreatePixelShader(
      PS_Standard_Data, sizeof(PS_Standard_Data), nullptr,
      &pixelShaderTable[C4JRender::PIXEL_SHADER_TYPE_STANDARD]);
  m_pDevice->CreatePixelShader(
      PS_TextureProjection_Data, sizeof(PS_TextureProjection_Data), nullptr,
      &pixelShaderTable[C4JRender::PIXEL_SHADER_TYPE_PROJECTION]);
  m_pDevice->CreatePixelShader(
      PS_ForceLOD_Data, sizeof(PS_ForceLOD_Data), nullptr,
      &pixelShaderTable[C4JRender::PIXEL_SHADER_TYPE_FORCELOD]);
  m_pDevice->CreatePixelShader(PS_ScreenSpace_Data, sizeof(PS_ScreenSpace_Data),
                               nullptr, &screenSpacePixelShader);
  m_pDevice->CreatePixelShader(PS_ScreenClear_Data, sizeof(PS_ScreenClear_Data),
                               nullptr, &screenClearPixelShader);

  m_pDevice->CreateInputLayout(
      g_vertex_PTN_Elements_PF3_TF2_CB4_NB4_XW1, 5, VS_PF3_TF2_CB4_NB4_XW1_Data,
      sizeof(VS_PF3_TF2_CB4_NB4_XW1_Data),
      &inputLayoutTable[C4JRender::VERTEX_TYPE_PF3_TF2_CB4_NB4_XW1]);
  m_pDevice->CreateInputLayout(
      g_vertex_PTN_Elements_Compressed, 2, VS_Compressed_Data,
      sizeof(VS_Compressed_Data),
      &inputLayoutTable[C4JRender::VERTEX_TYPE_COMPRESSED]);
  m_pDevice->CreateInputLayout(
      g_vertex_PTN_Elements_PF3_TF2_CB4_NB4_XW1, 5,
      VS_PF3_TF2_CB4_NB4_XW1_Lighting_Data,
      sizeof(VS_PF3_TF2_CB4_NB4_XW1_Lighting_Data),
      &inputLayoutTable[C4JRender::VERTEX_TYPE_PF3_TF2_CB4_NB4_XW1_LIT]);
  m_pDevice->CreateInputLayout(
      g_vertex_PTN_Elements_PF3_TF2_CB4_NB4_XW1, 5,
      VS_PF3_TF2_CB4_NB4_XW1_Texgen_Data,
      sizeof(VS_PF3_TF2_CB4_NB4_XW1_Texgen_Data),
      &inputLayoutTable[C4JRender::VERTEX_TYPE_PF3_TF2_CB4_NB4_XW1_TEXGEN]);
}

void Renderer::StartFrame() {
  Renderer::Context &context = this->getContext();

  activeVertexType = 0xFFFFFFFFu;
  activePixelType = 0xFFFFFFFFu;

  this->TextureBindVertex(-1);
  this->TextureBind(-1);
  this->StateSetColour(1.0f, 1.0f, 1.0f, 1.0f);
  this->StateSetDepthMask(true);
  this->StateSetBlendEnable(true);
  this->StateSetBlendFunc(D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA);
  this->StateSetBlendFactor(0xFFFFFFFFu);
  this->StateSetAlphaFunc(D3D11_COMPARISON_GREATER, 0.1f);
  this->StateSetDepthFunc(D3D11_COMPARISON_LESS_EQUAL);
  this->StateSetFaceCull(true);
  this->StateSetLineWidth(1.0f);
  this->StateSetWriteEnable(true, true, true, true);
  this->StateSetDepthTestEnable(false);
  this->StateSetAlphaTestEnable(true);

  context.m_pDeviceContext->VSSetConstantBuffers(0, 10, &context.cbMatrix0);
  context.m_pDeviceContext->PSSetConstantBuffers(0, 6, &context.cbColour);

  D3D11_VIEWPORT viewport = {};
  viewport.TopLeftX = 0.0f;
  viewport.TopLeftY = 0.0f;
  viewport.Width = (float)backBufferWidth;
  viewport.Height = (float)backBufferHeight;
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;
  context.m_pDeviceContext->RSSetViewports(1, &viewport);
  context.m_pDeviceContext->OMSetRenderTargets(1, &renderTargetView,
                                               depthStencilView);
}

void Renderer::Suspend() { suspended = 1; }

bool Renderer::Suspended() { return suspended != 0; }

void Renderer::UpdateGamma(unsigned short) {}

Renderer::Context &Renderer::getContext() {
  return *static_cast<Renderer::Context *>(TlsGetValue(Renderer::tlsIdx));
}

void Renderer::CaptureThumbnail(ImageFileBuffer *pngOut) {
  Renderer::Context &context = this->getContext();

  float left;
  float bottom;
  float right;
  float top;
  left = 0.0f;
  bottom = 0.0f;
  right = 1.0f;
  top = 1.0f;
  switch (m_ViewportType) {
  case C4JRender::VIEWPORT_TYPE_SPLIT_TOP:
    bottom = 0.5f;
    break;
  case C4JRender::VIEWPORT_TYPE_SPLIT_BOTTOM:
    top = 0.5f;
    break;
  case C4JRender::VIEWPORT_TYPE_SPLIT_LEFT:
    right = 0.5f;
    break;
  case C4JRender::VIEWPORT_TYPE_SPLIT_RIGHT:
    left = 0.5f;
    break;
  case C4JRender::VIEWPORT_TYPE_QUADRANT_TOP_LEFT:
    right = 0.5f;
    bottom = 0.5f;
    break;
  case C4JRender::VIEWPORT_TYPE_QUADRANT_TOP_RIGHT:
    left = 0.5f;
    bottom = 0.5f;
    break;
  case C4JRender::VIEWPORT_TYPE_QUADRANT_BOTTOM_LEFT:
    right = 0.5f;
    top = 0.5f;
    break;
  case C4JRender::VIEWPORT_TYPE_QUADRANT_BOTTOM_RIGHT:
    left = 0.5f;
    top = 0.5f;
    break;
  default:
    break;
  }

  float leftScaled = left * kAspectRatio;
  float rightScaled = right * kAspectRatio;
  const float viewportHeight = top - bottom;
  const float viewportScaledWidth = rightScaled - leftScaled;

  if (viewportHeight <= viewportScaledWidth) {
    const float pad = (viewportScaledWidth - viewportHeight) * 0.5f;
    leftScaled += pad;
    rightScaled -= pad;
  } else {
    const float pad = (viewportHeight - viewportScaledWidth) * 0.5f;
    bottom += pad;
    top -= pad;
  }

  const float sampleLeft = leftScaled / kAspectRatio;
  const float sampleWidth = (rightScaled - leftScaled) / kAspectRatio;
  const float sampleHeight = top - bottom;

  ID3D11BlendState *blendState = nullptr;
  ID3D11DepthStencilState *depthState = nullptr;
  ID3D11RasterizerState *rasterizerState = nullptr;
  ID3D11SamplerState *samplerState = nullptr;
  ID3D11Texture2D *stagingTexture = nullptr;

  D3D11_BLEND_DESC blendDesc = {};
  blendDesc.RenderTarget[0].BlendEnable = FALSE;
  blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
  blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
  blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
  blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
  blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
  blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
  blendDesc.RenderTarget[0].RenderTargetWriteMask =
      D3D11_COLOR_WRITE_ENABLE_ALL;
  m_pDevice->CreateBlendState(&blendDesc, &blendState);

  D3D11_DEPTH_STENCIL_DESC depthDesc = {};
  depthDesc.DepthEnable = FALSE;
  depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
  depthDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
  depthDesc.StencilEnable = FALSE;
  depthDesc.StencilReadMask = 0xFFu;
  depthDesc.StencilWriteMask = 0xFFu;
  depthDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
  depthDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
  depthDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
  depthDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
  depthDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
  depthDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
  depthDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
  depthDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
  m_pDevice->CreateDepthStencilState(&depthDesc, &depthState);

  D3D11_RASTERIZER_DESC rasterDesc = {};
  rasterDesc.FillMode = D3D11_FILL_SOLID;
  rasterDesc.CullMode = D3D11_CULL_NONE;
  rasterDesc.DepthClipEnable = TRUE;
  rasterDesc.MultisampleEnable = TRUE;
  m_pDevice->CreateRasterizerState(&rasterDesc, &rasterizerState);

  D3D11_SAMPLER_DESC samplerDesc = {};
  samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
  samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
  samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
  samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
  samplerDesc.MaxAnisotropy = 1;
  samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
  samplerDesc.MinLOD = 0.0f;
  samplerDesc.MaxLOD = (std::numeric_limits<float>::max)();
  m_pDevice->CreateSamplerState(&samplerDesc, &samplerState);

  context.m_pDeviceContext->VSSetShader(screenSpaceVertexShader, nullptr, 0);
  context.m_pDeviceContext->IASetInputLayout(nullptr);
  context.m_pDeviceContext->PSSetShader(screenSpacePixelShader, nullptr, 0);
  context.m_pDeviceContext->OMSetBlendState(blendState, nullptr, 0xFFFFFFFFu);
  context.m_pDeviceContext->OMSetDepthStencilState(depthState, 0);
  context.m_pDeviceContext->RSSetState(rasterizerState);

  for (unsigned int i = 0; i < MAX_MIP_LEVELS - 1; ++i) {
    D3D11_VIEWPORT viewport = {};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = (float)s_auiWidths[i + 1];
    viewport.Height = (float)s_auiHeights[i + 1];
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    context.m_pDeviceContext->OMSetRenderTargets(1, &renderTargetViews[i],
                                                 nullptr);
    context.m_pDeviceContext->RSSetViewports(1, &viewport);

    ID3D11ShaderResourceView *inputTexture =
        (i == 0) ? renderTargetShaderResourceView
                 : renderTargetShaderResourceViews[i - 1];
    context.m_pDeviceContext->PSSetShaderResources(0, 1, &inputTexture);
    context.m_pDeviceContext->PSSetSamplers(0, 1, &samplerState);
    context.m_pDeviceContext->IASetPrimitiveTopology(
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    context.m_pDeviceContext->Map(context.cbAux1, 0, D3D11_MAP_WRITE_DISCARD, 0,
                                  &mapped);
    float *constants = static_cast<float *>(mapped.pData);
    if (i == 0) {
      constants[0] = sampleLeft;
      constants[1] = bottom;
      constants[2] = sampleWidth;
      constants[3] = sampleHeight;
    } else {
      constants[0] = 0.0f;
      constants[1] = 0.0f;
      constants[2] = 1.0f;
      constants[3] = 1.0f;
    }
    context.m_pDeviceContext->Unmap(context.cbAux1, 0);
    context.m_pDeviceContext->Draw(4, 0);
  }

  D3D11_TEXTURE2D_DESC desc = {};
  renderTargetTextures[MAX_MIP_LEVELS - 2]->GetDesc(&desc);
  desc.Usage = D3D11_USAGE_STAGING;
  desc.BindFlags = 0;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  desc.MiscFlags = 0;
  m_pDevice->CreateTexture2D(&desc, nullptr, &stagingTexture);

  unsigned char *linearData =
      new unsigned char[kThumbnailSize * kThumbnailSize * 4u];
  if (stagingTexture) {
    context.m_pDeviceContext->CopyResource(
        stagingTexture, renderTargetTextures[MAX_MIP_LEVELS - 2]);
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(context.m_pDeviceContext->Map(stagingTexture, 0,
                                                D3D11_MAP_READ, 0, &mapped))) {
      const unsigned char *src =
          static_cast<const unsigned char *>(mapped.pData);
      unsigned char *dst = linearData;
      for (unsigned int y = 0; y < kThumbnailSize; ++y) {
        std::memcpy(dst, src, kThumbnailSize * 4u);
        for (unsigned int x = 0; x < kThumbnailSize; ++x) {
          dst[x * 4u + 3u] = 0xFF;
        }
        src += mapped.RowPitch;
        dst += kThumbnailSize * 4u;
      }
      context.m_pDeviceContext->Unmap(stagingTexture, 0);
    }
  }

  this->ConvertLinearToPng(pngOut, linearData, kThumbnailSize, kThumbnailSize);
  delete[] linearData;

  if (stagingTexture) {
    stagingTexture->Release();
    stagingTexture = nullptr;
  }
  if (samplerState) {
    samplerState->Release();
    samplerState = nullptr;
  }
  if (rasterizerState) {
    rasterizerState->Release();
    rasterizerState = nullptr;
  }
  if (depthState) {
    depthState->Release();
    depthState = nullptr;
  }
  if (blendState) {
    blendState->Release();
    blendState = nullptr;
  }

  context.m_pDeviceContext->OMSetBlendState(this->GetManagedBlendState(),
                                            context.blendFactor, 0xFFFFFFFFu);
  context.m_pDeviceContext->OMSetDepthStencilState(
      this->GetManagedDepthStencilState(), 0);
  context.m_pDeviceContext->RSSetState(this->GetManagedRasterizerState());

  D3D11_VIEWPORT viewport = {};
  viewport.TopLeftX = 0.0f;
  viewport.TopLeftY = 0.0f;
  viewport.Width = (float)backBufferWidth;
  viewport.Height = (float)backBufferHeight;
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;
  context.m_pDeviceContext->RSSetViewports(1, &viewport);
  context.m_pDeviceContext->OMSetRenderTargets(1, &renderTargetView,
                                               depthStencilView);
  activeVertexType = 0xFFFFFFFFu;
  activePixelType = 0xFFFFFFFFu;
}
