#include "renderer.h"
#include "../shared/convar.h"

#include <d3dcompiler.h>

#include <cmath>
#include <cstdint>
#include <vector>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

static ConVar r_wireframe("r_wireframe", "0", FCVAR_CHEAT, "Draw geometry as wireframe", 0.0f, 1.0f);

namespace
{
	
	struct PerFrameCB
	{
		XMFLOAT4X4 viewProj;
		XMFLOAT3 lightDir;
		float time;
		XMFLOAT3 camPos;
		float pad;
	};

	struct PerObjectCB
	{
		XMFLOAT4X4 world;
		XMFLOAT4 color;
		int mode;
		int pad[3];
	};

	static_assert(sizeof(PerFrameCB) % 16 == 0 && sizeof(PerObjectCB) % 16 == 0);

	struct Vertex
	{
		XMFLOAT3 pos;
		XMFLOAT3 normal;
	};

	Renderer* g_renderer = nullptr;

	bool CompileShader(const std::filesystem::path& file, const char* entry, const char* target, ComPtr<ID3DBlob>& out)
	{
		UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
		flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
		ComPtr<ID3DBlob> errors;
		const HRESULT hr = D3DCompileFromFile(file.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, entry, target, flags, 0, &out, &errors);
		if (FAILED(hr))
		{
			if (errors) ConError("%s\n", static_cast<const char*>(errors->GetBufferPointer()));
			else ConError("Could not read shader %s (0x%08X)\n", file.string().c_str(), static_cast<unsigned>(hr));
			return false;
		}
		if (errors && errors->GetBufferSize() > 0) ConWarning("%s\n", static_cast<const char*>(errors->GetBufferPointer()));
		return true;
	}

	bool CreateConstantBuffer(ID3D11Device* dev, UINT size, ComPtr<ID3D11Buffer>& out)
	{
		D3D11_BUFFER_DESC bd{};
		bd.ByteWidth = size;
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		return SUCCEEDED(dev->CreateBuffer(&bd, nullptr, &out));
	}

	bool CreateMesh(ID3D11Device* dev, RenderMesh& mesh, const std::vector<Vertex>& verts, const std::vector<uint16_t>& indices)
	{
		D3D11_BUFFER_DESC bd{};
		bd.Usage = D3D11_USAGE_IMMUTABLE;
		D3D11_SUBRESOURCE_DATA sd{};

		bd.ByteWidth = static_cast<UINT>(verts.size() * sizeof(Vertex));
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		sd.pSysMem = verts.data();
		if (FAILED(dev->CreateBuffer(&bd, &sd, &mesh.vb))) return false;

		bd.ByteWidth = static_cast<UINT>(indices.size() * sizeof(uint16_t));
		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		sd.pSysMem = indices.data();
		if (FAILED(dev->CreateBuffer(&bd, &sd, &mesh.ib))) return false;

		mesh.indexCount = static_cast<UINT>(indices.size());
		return true;
	}

	void AddQuad(std::vector<Vertex>& verts, std::vector<uint16_t>& idx, FXMVECTOR center, FXMVECTOR n, float halfU, float halfV)
	{
		const XMVECTOR u = (std::fabs(XMVectorGetY(n)) > 0.9f) ? XMVectorSet(1, 0, 0, 0) : XMVectorSet(0, 1, 0, 0);
		const XMVECTOR U = XMVectorScale(u, halfU);
		const XMVECTOR V = XMVectorScale(XMVector3Cross(u, n), halfV);

		const XMVECTOR corners[4] =
		{
			XMVectorSubtract(XMVectorSubtract(center, U), V),
			XMVectorAdd(XMVectorSubtract(center, U), V),
			XMVectorAdd(XMVectorAdd(center, U), V),
			XMVectorSubtract(XMVectorAdd(center, U), V),
		};

		XMFLOAT3 nf;
		XMStoreFloat3(&nf, n);
		const uint16_t base = static_cast<uint16_t>(verts.size());
		for (const XMVECTOR& c : corners)
		{
			Vertex v;
			XMStoreFloat3(&v.pos, c);
			v.normal = nf;
			verts.push_back(v);
		}
		const uint16_t tri[6] = { base, uint16_t(base + 1), uint16_t(base + 2), base, uint16_t(base + 2), uint16_t(base + 3) };
		idx.insert(idx.end(), tri, tri + 6);
	}

}

CON_COMMAND(r_reloadshaders, "r_reloadshaders: recompile and reload shaders")
{
	if (g_renderer) g_renderer->ReloadShaders();
}

bool Renderer::Init(D3D11Device& device, const std::filesystem::path& shaderDir)
{
	m_dev = &device;
	m_shaderDir = shaderDir;
	ID3D11Device* dev = device.Device();

	if (!ReloadShaders()) return false;

	if (!CreateConstantBuffer(dev, sizeof(PerFrameCB), m_cbFrame) || !CreateConstantBuffer(dev, sizeof(PerObjectCB), m_cbObject))
	{
		ConError("Failed to create constant buffers\n");
		return false;
	}

	D3D11_RASTERIZER_DESC rd{};
	rd.FillMode = D3D11_FILL_SOLID;
	rd.CullMode = D3D11_CULL_BACK;
	rd.FrontCounterClockwise = FALSE;
	rd.DepthClipEnable = TRUE;
	dev->CreateRasterizerState(&rd, &m_rsSolid);
	rd.FillMode = D3D11_FILL_WIREFRAME;
	rd.CullMode = D3D11_CULL_NONE;
	dev->CreateRasterizerState(&rd, &m_rsWire);

	D3D11_DEPTH_STENCIL_DESC dd{};
	dd.DepthEnable = TRUE;
	dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	dd.DepthFunc = D3D11_COMPARISON_LESS;
	dev->CreateDepthStencilState(&dd, &m_depthState);

	{
		std::vector<Vertex> v;
		std::vector<uint16_t> i;
		const float h = 32.0f;
		const XMVECTOR normals[6] =
		{
			XMVectorSet(1, 0, 0, 0), XMVectorSet(-1, 0, 0, 0),
			XMVectorSet(0, 1, 0, 0), XMVectorSet(0, -1, 0, 0),
			XMVectorSet(0, 0, 1, 0), XMVectorSet(0, 0, -1, 0),
		};
		for (const XMVECTOR& n : normals) AddQuad(v, i, XMVectorScale(n, h), n, h, h);
		if (!CreateMesh(dev, m_cube, v, i))
		{
			ConError("Failed to create cube mesh\n");
			return false;
		}
	}
	{
		std::vector<Vertex> v;
		std::vector<uint16_t> i;
		AddQuad(v, i, XMVectorSet(0, 0, 0, 0), XMVectorSet(0, 1, 0, 0), 8192.0f, 8192.0f);
		if (!CreateMesh(dev, m_floor, v, i))
		{
			ConError("Failed to create floor mesh\n");
			return false;
		}
	}

	g_renderer = this;
	return true;
}

void Renderer::Shutdown()
{
	if (g_renderer == this) g_renderer = nullptr;
	m_cube = {};
	m_floor = {};
	m_depthState.Reset();
	m_rsWire.Reset();
	m_rsSolid.Reset();
	m_cbObject.Reset();
	m_cbFrame.Reset();
	m_layout.Reset();
	m_ps.Reset();
	m_vs.Reset();
}

bool Renderer::ReloadShaders()
{
	ID3D11Device* dev = m_dev->Device();
	const std::filesystem::path file = m_shaderDir / L"basic.hlsl";

	ComPtr<ID3DBlob> vsBlob, psBlob;
	if (!CompileShader(file, "VSMain", "vs_5_0", vsBlob) || !CompileShader(file, "PSMain", "ps_5_0", psBlob))
	{
		ConError("Shader compile failed%s\n", m_vs ? "; keeping the previous shaders" : "");
		return false;
	}

	ComPtr<ID3D11VertexShader> vs;
	ComPtr<ID3D11PixelShader> ps;
	ComPtr<ID3D11InputLayout> layout;

	if (FAILED(dev->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vs)) || FAILED(dev->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &ps)))
	{
		ConError("Failed to create shader objects\n");
		return false;
	}

	const D3D11_INPUT_ELEMENT_DESC desc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	if (FAILED(dev->CreateInputLayout(desc, _countof(desc), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &layout)))
	{
		ConError("Failed to create input layout\n");
		return false;
	}

	m_vs = vs;
	m_ps = ps;
	m_layout = layout;
	ConMsg("Shaders loaded: %s\n", file.string().c_str());
	return true;
}

void Renderer::Upload(ID3D11Buffer* buffer, const void* data, size_t size)
{
	ID3D11DeviceContext* ctx = m_dev->Context();
	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (SUCCEEDED(ctx->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		std::memcpy(mapped.pData, data, size);
		ctx->Unmap(buffer, 0);
	}
}

void Renderer::DrawMesh(const RenderMesh& mesh, FXMMATRIX world, const XMFLOAT4& color, int mode)
{
	ID3D11DeviceContext* ctx = m_dev->Context();

	PerObjectCB cb{};
	XMStoreFloat4x4(&cb.world, world);
	cb.color = color;
	cb.mode = mode;
	Upload(m_cbObject.Get(), &cb, sizeof cb);

	const UINT stride = sizeof(Vertex), offset = 0;
	ID3D11Buffer* vb = mesh.vb.Get();
	ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
	ctx->IASetIndexBuffer(mesh.ib.Get(), DXGI_FORMAT_R16_UINT, 0);
	ctx->DrawIndexed(mesh.indexCount, 0, 0);
}

void Renderer::RenderScene(const Camera& cam, float time)
{
	if (m_dev->Height() <= 0) return;
	ID3D11DeviceContext* ctx = m_dev->Context();

	const float aspect = static_cast<float>(m_dev->Width()) / static_cast<float>(m_dev->Height());
	const XMMATRIX viewProj = XMMatrixMultiply(cam.View(), cam.Projection(aspect));

	PerFrameCB fcb{};
	XMStoreFloat4x4(&fcb.viewProj, viewProj);
	XMStoreFloat3(&fcb.lightDir, XMVector3Normalize(XMVectorSet(-0.4f, -1.0f, 0.3f, 0.0f)));
	fcb.time = time;
	fcb.camPos = cam.position;
	Upload(m_cbFrame.Get(), &fcb, sizeof fcb);

	ctx->IASetInputLayout(m_layout.Get());
	ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ctx->VSSetShader(m_vs.Get(), nullptr, 0);
	ctx->PSSetShader(m_ps.Get(), nullptr, 0);

	ID3D11Buffer* cbs[2] = { m_cbFrame.Get(), m_cbObject.Get() };
	ctx->VSSetConstantBuffers(0, 2, cbs);
	ctx->PSSetConstantBuffers(0, 2, cbs);

	ctx->RSSetState(r_wireframe.GetBool() ? m_rsWire.Get() : m_rsSolid.Get());
	ctx->OMSetDepthStencilState(m_depthState.Get(), 0);
	ctx->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);

	DrawMesh(m_floor, XMMatrixIdentity(), XMFLOAT4(1, 1, 1, 1), 1);

	const XMMATRIX cubeWorld = XMMatrixMultiply(XMMatrixRotationRollPitchYaw(time * 0.5f, time * 0.8f, 0.0f), XMMatrixTranslation(0.0f, 64.0f, 0.0f));
	DrawMesh(m_cube, cubeWorld, XMFLOAT4(0.95f, 0.55f, 0.20f, 1.0f), 0);
}