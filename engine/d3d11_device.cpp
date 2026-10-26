#include "d3d11_device.h"

#include "../shared/mbox.h"

#include <Windows.h>

#include <cstdio>

bool D3D11Device::Init(void* hwnd, int w, int h, bool debug)
{
	const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
	UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
	if (debug) flags |= D3D11_CREATE_DEVICE_DEBUG;

	HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, levels, _countof(levels), D3D11_SDK_VERSION, &m_dev, nullptr, &m_ctx);
	if (FAILED(hr) && debug)
	{
		flags &= ~D3D11_CREATE_DEVICE_DEBUG;
		hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, levels, _countof(levels), D3D11_SDK_VERSION, &m_dev, nullptr, &m_ctx);
	}
	if (FAILED(hr))
	{
		ErrorMessage("D3D11CreateDevice failed (0x{})\n", (unsigned)hr);
		return false;
	}

	ComPtr<IDXGIDevice> dxgiDev;
	m_dev.As(&dxgiDev);

	ComPtr<IDXGIAdapter> adapter;
	dxgiDev->GetAdapter(&adapter);

	ComPtr<IDXGIFactory2> factory;
	adapter->GetParent(IID_PPV_ARGS(&factory));

	DXGI_SWAP_CHAIN_DESC1 sd{};
	sd.Width = static_cast<UINT>(w);
	sd.Height = static_cast<UINT>(h);
	sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	sd.SampleDesc.Count = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = 2;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	hr = factory->CreateSwapChainForHwnd(m_dev.Get(), static_cast<HWND>(hwnd), &sd, nullptr, nullptr, &m_swap);
	if (FAILED(hr))
	{
		ErrorMessage("CreateSwapChainForHwnd failed (0x{})", (unsigned)hr);
		return false;
	}

	CreateBackbufferViews(w, h);
	return true;
}

void D3D11Device::CreateBackbufferViews(int w, int h)
{
	m_w = w;
	m_h = h;

	ComPtr<ID3D11Texture2D> back;
	m_swap->GetBuffer(0, IID_PPV_ARGS(&back));
	m_dev->CreateRenderTargetView(back.Get(), nullptr, &m_rtv);

	D3D11_TEXTURE2D_DESC dd{};
	dd.Width = static_cast<UINT>(w);
	dd.Height = static_cast<UINT>(h);
	dd.MipLevels = 1;
	dd.ArraySize = 1;
	dd.Format = DXGI_FORMAT_D32_FLOAT;
	dd.SampleDesc.Count = 1;
	dd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	m_dev->CreateTexture2D(&dd, nullptr, &m_depth);
	m_dev->CreateDepthStencilView(m_depth.Get(), nullptr, &m_dsv);
}

void D3D11Device::Resize(int w, int h)
{
	if (!m_swap || w <= 0 || h <= 0) return;
	m_ctx->OMSetRenderTargets(0, nullptr, nullptr);
	m_rtv.Reset();
	m_dsv.Reset();
	m_depth.Reset();
	m_swap->ResizeBuffers(0, static_cast<UINT>(w), static_cast<UINT>(h), DXGI_FORMAT_UNKNOWN, 0);
	CreateBackbufferViews(w, h);
}

void D3D11Device::BeginFrame(const float c[4])
{
	m_ctx->OMSetRenderTargets(1, m_rtv.GetAddressOf(), m_dsv.Get());
	m_ctx->ClearRenderTargetView(m_rtv.Get(), c);
	m_ctx->ClearDepthStencilView(m_dsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
	D3D11_VIEWPORT vp{ 0.0f, 0.0f, static_cast<float>(m_w), static_cast<float>(m_h), 0.0f, 1.0f };
	m_ctx->RSSetViewports(1, &vp);
}

void D3D11Device::EndFrame(bool vsync)
{
	m_swap->Present(vsync ? 1 : 0, 0);
}

void D3D11Device::Shutdown()
{
	if (m_ctx) m_ctx->ClearState();
	m_dsv.Reset();
	m_depth.Reset();
	m_rtv.Reset();
	m_swap.Reset();
	m_ctx.Reset();
	m_dev.Reset();
}