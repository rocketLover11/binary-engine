#pragma once

#include "d3d11_device.h"
#include "camera.h"

#include <filesystem>

struct RenderMesh
{
	Microsoft::WRL::ComPtr<ID3D11Buffer> vb;
	Microsoft::WRL::ComPtr<ID3D11Buffer> ib;
	UINT indexCount = 0;
};

class Renderer
{
public:
	bool Init(D3D11Device& device, const std::filesystem::path& shaderDir);
	void Shutdown();

	bool ReloadShaders();

	void RenderScene(const Camera& camera, float time);

private:
	void DrawMesh(const RenderMesh& mesh, DirectX::FXMMATRIX world, const DirectX::XMFLOAT4& color, int mode);
	void Upload(ID3D11Buffer* buffer, const void* data, size_t size);

	D3D11Device* m_dev = nullptr;
	std::filesystem::path m_shaderDir;

	Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vs;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> m_ps;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> m_layout;
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbFrame;
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbObject;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_rsSolid;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_rsWire;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_depthState;

	RenderMesh m_cube;
	RenderMesh m_floor;
};