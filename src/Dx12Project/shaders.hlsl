struct Vs2Ps
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

// ■追加：定数バッファ
cbuffer ObjectConstantBuffer : register(b0)
{
    float4x4 matWVP;
};


// テクスチャとサンプラーステート
Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

Vs2Ps VSMain(float4 position : POSITION, float2 texcoord : TEXCOORD)
{
    Vs2Ps output;

    output.position = mul(position, matWVP); // ■変更
    output.texcoord = texcoord;

    return output;
}

float4 PSMain(Vs2Ps input) : SV_TARGET
{
    return g_texture.Sample(g_sampler, input.texcoord);
}