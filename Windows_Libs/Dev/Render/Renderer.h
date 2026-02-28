#pragma once
#include "4J_Render.h"

class Renderer
{
public:

	class CommandBuffer
	{
	public:

	};

	class DeferredCBuff
	{
	public:

	};

	static DWORD tlsIdx;
};

// Singleton
extern Renderer InternalRenderManager;