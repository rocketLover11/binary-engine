#pragma once

#include <DirectXMath.h>

#include <algorithm>
#include <cmath>

struct Camera
{
	DirectX::XMFLOAT3 position{ 0.0f, 96.0f, -256.0f };
	float yaw = 0.0f;
	float pitch = 0.0f;
	float fovDegrees = 90.0f;

	static constexpr float kNear = 1.0f;
	static constexpr float kFar = 16384.0f;

	DirectX::XMVECTOR Forward() const
	{
		const float cp = std::cos(pitch);
		return DirectX::XMVectorSet(std::sin(yaw) * cp, std::sin(pitch), std::cos(yaw) * cp, 0.0f);
	}

	DirectX::XMVECTOR Right() const
	{
		return DirectX::XMVectorSet(std::cos(yaw), 0.0f, -std::sin(yaw), 0.0f);
	}

	DirectX::XMMATRIX View() const
	{
		return DirectX::XMMatrixLookToLH(DirectX::XMLoadFloat3(&position), Forward(), DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
	}

	DirectX::XMMATRIX Projection(float aspect) const
	{
		const float h = DirectX::XMConvertToRadians(fovDegrees);
		const float v = 2.0f * std::atan(std::tan(h * 0.5f) / aspect);
		return DirectX::XMMatrixPerspectiveFovLH(v, aspect, kNear, kFar);
	}

	void Look(float dx, float dy, float degreesPerCount)
	{
		const float k = DirectX::XMConvertToRadians(degreesPerCount);
		yaw += dx * k;
		pitch -= dy * k;
		const float limit = DirectX::XMConvertToRadians(89.0f);
		pitch = std::clamp(pitch, -limit, limit);
	}

	void Move(float forward, float right, float up)
	{
		using namespace DirectX;
		XMVECTOR p = XMLoadFloat3(&position);
		p = XMVectorAdd(p, XMVectorScale(Forward(), forward));
		p = XMVectorAdd(p, XMVectorScale(Right(), right));
		p = XMVectorAdd(p, XMVectorSet(0.0f, up, 0.0f, 0.0f));
		XMStoreFloat3(&position, p);
	}
};