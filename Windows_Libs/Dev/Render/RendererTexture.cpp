#pragma once
#include "Renderer.h"
#include "libpng/png.h"

#include <cstdint>
#include <cstring>
#include <limits>
#include <new>

DXGI_FORMAT Renderer::textureFormats[C4JRender::MAX_TEXTURE_FORMATS] = {
    DXGI_FORMAT_B8G8R8A8_UNORM,
};

int Renderer::TextureCreate() {
  for (int i = 0; i < 512; ++i) {
    Texture &texture = m_textures[i];
    if (!texture.allocated) {
      texture.texture = nullptr;
      texture.allocated = true;
      texture.mipLevels = 1;
      texture.samplerParams = 0;
      return i;
    }
  }

  return -1;
}

void Renderer::TextureSetTextureLevels(int levels) {
  const int boundTextureIndex = this->getContext().boundTextureIndex;
  m_textures[boundTextureIndex].mipLevels = levels;
}

int Renderer::TextureGetTextureLevels() {
  const int boundTextureIndex = this->getContext().boundTextureIndex;
  return m_textures[boundTextureIndex].mipLevels;
}

void Renderer::TextureSetParam(int param, int value) {
  Texture &texture = m_textures[this->getContext().boundTextureIndex];

  switch (param) {
  case GL_TEXTURE_MIN_FILTER:
    texture.samplerParams &= ~4u;
    if (value == 1) {
      texture.samplerParams |= 4u;
    }
    break;
  case GL_TEXTURE_MAG_FILTER:
    texture.samplerParams &= ~8u;
    if (value == 1) {
      texture.samplerParams |= 8u;
    }
    break;
  case GL_TEXTURE_WRAP_S:
    texture.samplerParams &= ~1u;
    if (value == 0) {
      texture.samplerParams |= 1u;
    }
    break;
  case GL_TEXTURE_WRAP_T:
  case 5:
    texture.samplerParams &= ~2u;
    if (value == 0) {
      texture.samplerParams |= 2u;
    }
    break;
  default:
    break;
  }
}

void Renderer::TextureDynamicUpdateStart() {}

void Renderer::TextureDynamicUpdateEnd() {}

void Renderer::TextureData(int width, int height, void *data, int level,
                           C4JRender::eTextureFormat format) {
  Renderer::Context &context = this->getContext();
  Texture &texture = m_textures[context.boundTextureIndex];
  texture.textureFormat = format;

  if (level == 0) {
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = texture.mipLevels;
    desc.ArraySize = 1;
    desc.Format = textureFormats[format];
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    m_pDevice->CreateTexture2D(&desc, nullptr, &texture.texture);
    m_pDevice->CreateShaderResourceView(texture.texture, nullptr,
                                        &texture.view);
  }

  const UINT rowPitch = width * 4u;
  const UINT depthPitch = width * height * 4u;
  context.m_pDeviceContext->UpdateSubresource(texture.texture, level, nullptr,
                                              data, rowPitch, depthPitch);
}

void Renderer::TextureDataUpdate(int xoffset, int yoffset, int width,
                                 int height, void *data, int level) {
  Renderer::Context &context = this->getContext();
  Texture &texture = m_textures[context.boundTextureIndex];

  D3D11_BOX box = {};
  box.left = xoffset;
  box.right = xoffset + width;
  box.top = yoffset;
  box.bottom = yoffset + height;
  box.front = 0;
  box.back = 1;

  D3D11_TEXTURE2D_DESC desc = {};
  texture.texture->GetDesc(&desc);

  const UINT rowPitch = width * 4u;
  const UINT depthPitch = width * height * 4u;
  context.m_pDeviceContext->UpdateSubresource(texture.texture, level, &box,
                                              data, rowPitch, depthPitch);
}

void Renderer::TextureFree(int idx) {
  Texture &texture = m_textures[idx];
  texture.texture->Release();
  texture.view->Release();
  texture.view = nullptr;
  texture.allocated = false;
  texture.texture = nullptr;
}

void Renderer::TextureGetStats() {}

ID3D11ShaderResourceView *Renderer::TextureGetTexture(int idx) {
  if (idx < 0 || idx > 0x1FF) {
    return nullptr;
  }

  const Texture &texture = m_textures[idx];
  if (!texture.allocated) {
    return nullptr;
  }

  return texture.view;
}

void Renderer::TextureBind(int idx) {
  int textureIndex = idx;
  if (textureIndex == -1) {
    textureIndex = defaultTextureIndex;
  }

  Renderer::Context &context = this->getContext();

  if (context.commandBuffer && context.commandBuffer->isActive) {
    context.commandBuffer->BindTexture(textureIndex);
  }

  context.boundTextureIndex = textureIndex;
  ID3D11ShaderResourceView *const view = m_textures[textureIndex].view;
  context.m_pDeviceContext->PSSetShaderResources(0, 1, &view);
  this->UpdateTextureState(false);
}

void Renderer::TextureBindVertex(int idx) {
  int textureIndex = idx;
  if (textureIndex == -1) {
    textureIndex = defaultTextureIndex;
  }

  Renderer::Context &context = this->getContext();
  context.boundTextureIndex = textureIndex;

  ID3D11ShaderResourceView *const view = m_textures[textureIndex].view;
  context.m_pDeviceContext->VSSetShaderResources(0, 1, &view);
  this->UpdateTextureState(true);
}

void Renderer::UpdateTextureState(bool vertexSampler) {
  Renderer::Context &context = this->getContext();
  ID3D11SamplerState *sampler = this->GetManagedSamplerState();

  if (vertexSampler) {
    context.m_pDeviceContext->VSSetSamplers(0, 1, &sampler);
  } else {
    context.m_pDeviceContext->PSSetSamplers(0, 1, &sampler);
  }
}

HRESULT Renderer::LoadTextureData(const char *szFilename,
                                  D3DXIMAGE_INFO *pSrcInfo, int **ppDataOut) {
  if (!szFilename || !pSrcInfo || !ppDataOut) {
    return -1;
  }

  *ppDataOut = nullptr;

  png_image image;
  std::memset(&image, 0, sizeof(image));
  image.version = PNG_IMAGE_VERSION;
  if (!png_image_begin_read_from_file(&image, szFilename)) {
    return -1;
  }

  if (image.width == 0 || image.height == 0) {
    png_image_free(&image);
    return -1;
  }

  image.format = PNG_FORMAT_RGBA;
  const png_alloc_size_t pixelCount =
      png_alloc_size_t(image.width) * png_alloc_size_t(image.height);
  if (pixelCount == 0 ||
      pixelCount > (std::numeric_limits<size_t>::max)() / sizeof(int)) {
    png_image_free(&image);
    return -1;
  }

  int *output = new (std::nothrow) int[size_t(pixelCount)];

  if (!output) {
    png_image_free(&image);
    return -1;
  }

  if (!png_image_finish_read(&image, nullptr, output, 0, nullptr)) {
    delete[] output;
    png_image_free(&image);
    return -1;
  }

  *ppDataOut = output;
  pSrcInfo->Width = image.width;
  pSrcInfo->Height = image.height;
  png_image_free(&image);
  return 0;
}

HRESULT Renderer::LoadTextureData(BYTE *pbData, DWORD dwBytes,
                                  D3DXIMAGE_INFO *pSrcInfo, int **ppDataOut) {
  if (!pbData || dwBytes == 0 || !pSrcInfo || !ppDataOut) {
    return -1;
  }

  *ppDataOut = nullptr;

  png_image image;
  std::memset(&image, 0, sizeof(image));
  image.version = PNG_IMAGE_VERSION;
  if (!png_image_begin_read_from_memory(&image, pbData, dwBytes)) {
    return -1;
  }

  if (image.width == 0 || image.height == 0) {
    png_image_free(&image);
    return -1;
  }

  image.format = PNG_FORMAT_RGBA;
  const png_alloc_size_t pixelCount =
      png_alloc_size_t(image.width) * png_alloc_size_t(image.height);
  if (pixelCount == 0 ||
      pixelCount > (std::numeric_limits<size_t>::max)() / sizeof(int)) {
    png_image_free(&image);
    return -1;
  }

  int *output = new (std::nothrow) int[size_t(pixelCount)];

  if (!output) {
    png_image_free(&image);
    return -1;
  }

  if (!png_image_finish_read(&image, nullptr, output, 0, nullptr)) {
    delete[] output;
    png_image_free(&image);
    return -1;
  }

  *ppDataOut = output;
  pSrcInfo->Width = image.width;
  pSrcInfo->Height = image.height;
  png_image_free(&image);
  return 0;
}

HRESULT Renderer::SaveTextureData(const char *szFilename,
                                  D3DXIMAGE_INFO *pSrcInfo, int *ppDataOut) {
  png_image image = {};
  image.version = PNG_IMAGE_VERSION;
  image.width = pSrcInfo->Width;
  image.height = pSrcInfo->Height;
  image.format = PNG_FORMAT_RGBA;

  png_image_write_to_file(&image, szFilename, 0, ppDataOut, 0, nullptr);
  return 0;
}

HRESULT Renderer::SaveTextureDataToMemory(void *pOutput, int outputCapacity,
                                          int *outputLength, int width,
                                          int height, int *ppDataIn) {
  if (!pOutput || outputCapacity <= 0 || !outputLength || width <= 0 ||
      height <= 0 || !ppDataIn) {
    return -1;
  }

  png_image image;
  std::memset(&image, 0, sizeof(image));
  image.version = PNG_IMAGE_VERSION;
  image.width = width;
  image.height = height;
  image.format = PNG_FORMAT_RGBA;

  png_alloc_size_t memoryBytes = static_cast<png_alloc_size_t>(outputCapacity);
  if (!png_image_write_to_memory(&image, pOutput, &memoryBytes, 0, ppDataIn, 0,
                                 nullptr)) {
    *outputLength = 0;
    png_image_free(&image);
    return -1;
  }

  *outputLength = static_cast<int>(memoryBytes);
  png_image_free(&image);
  return 0;
}
