cbuffer PerFrame : register(b0)
{
    row_major float4x4 viewProj;
    float3 lightDir;
    float time;
    float3 camPos;
    float pad0;
};

cbuffer PerObject : register(b1)
{
    row_major float4x4 world;
    float4 color;
    int mode;
    int3 pad1;
};

struct VSIn
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
};

struct VSOut
{
    float4 sv : SV_Position;
    float3 wpos : TEXCOORD0;
    float3 normal : TEXCOORD1;
};

VSOut VSMain(VSIn i)
{
    VSOut o;
    float4 wp = mul(float4(i.pos, 1.0), world);
    o.wpos = wp.xyz;
    o.sv = mul(wp, viewProj);
    o.normal = mul(i.normal, (float3x3) world);
    return o;
}

float4 PSMain(VSOut i) : SV_Target
{
    float3 n = normalize(i.normal);
    float ndl = saturate(dot(n, -lightDir));
    
    float3 albedo = color.rgb;
    if (mode == 1)
    {
        float2 cell = floor(i.wpos.xz / 64.0);
        float checker = fmod(abs(cell.x + cell.y), 2.0);
        albedo = lerp(float3(0.30, 0.32, 0.36), float3(0.46, 0.48, 0.54), checker);
    }
    
    float3 lit = albedo * (0.25 + 0.75 * ndl);
    
    float dist = length(camPos - i.wpos);
    float fog = saturate((dist - 1024.0) / 5000.0);
    return float4(lerp(lit, float3(0.10, 0.12, 0.18), fog), 1.0);
}