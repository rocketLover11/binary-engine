#pragma once

#include "render_device.h"

#include <d3d11.h>
#include <dxgi1_3.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

class D3D11Device final : public IRenderDevice
{
public:
	bool Init(void* hwnd, int width, int height, bool debug) override;
	void Resize(int width, int height) override;
	void BeginFrame(const float clearColor[4]) override;
	void EndFrame(bool vsync) override;
	void Shutdown() override;

	ID3D11Device* Device() const { return m_dev.Get(); }
	ID3D11DeviceContext* Context() const { return m_ctx.Get(); }
	int Width()  const { return m_w; }
	int Height() const { return m_h; }

private:
	void CreateBackbufferViews(int w, int h);

	ComPtr<ID3D11Device> m_dev;
	ComPtr<ID3D11DeviceContext> m_ctx;
	ComPtr<IDXGISwapChain1> m_swap;
	ComPtr<ID3D11RenderTargetView> m_rtv;
	ComPtr<ID3D11Texture2D> m_depth;
	ComPtr<ID3D11DepthStencilView> m_dsv;
	int m_w = 0, m_h = 0;
};