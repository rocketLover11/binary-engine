#pragma once

struct IRenderDevice
{
	virtual bool Init(void* hwnd, int w, int h, bool debug) = 0;
	virtual void Resize(int w, int h) = 0;
	virtual void BeginFrame(const float c[4]) = 0;
	virtual void EndFrame(bool vsync) = 0;
	virtual void Shutdown() = 0;
};