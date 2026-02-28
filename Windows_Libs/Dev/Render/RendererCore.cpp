#pragma once
#include "Renderer.h"

Renderer InternalRenderManager;

DWORD Renderer::tlsIdx = TlsAlloc();
