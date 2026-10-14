#pragma once
#include "../common/weapon_accessory_logic.h"
#include <d3d11.h>
#include <wrl/client.h>

// Runs only in the VR compositor, after game-eye resolve. Never called by a
// native render/palette hook. Private deferred state preserves ALL game state.
class WeaponAccessoryRenderer
{
public:
    HRESULT Draw(ID3D11Device* device,ID3D11DeviceContext* immediate,
        ID3D11RenderTargetView* target,unsigned width,unsigned height,
        const D3D11_VIEWPORT& viewport,const weapon_accessory::Presentation& item,
        const weapon_accessory::Constants& constants,ID3D11CommandList** staged=nullptr);
    // Several items (reserve/held part plus parts lying on the floor) share one
    // self-depth clear and one command list. Invalid items are skipped; the
    // call fails only when none can be drawn.
    static constexpr unsigned kMaxItems=6;
    // Scene lighting: each item takes its brightness and a little colour from
    // the game picture around where it is drawn, instead of fixed lighting.
    // `captureScene` records the copy of those regions (the first eye of a
    // frame, executed first); other eyes reuse it so both eyes match.
    struct SceneLighting { bool enabled,capture; };
    HRESULT DrawMany(ID3D11Device* device,ID3D11DeviceContext* immediate,
        ID3D11RenderTargetView* target,unsigned width,unsigned height,
        const D3D11_VIEWPORT& viewport,const weapon_accessory::Presentation* const* items,
        const weapon_accessory::Constants* constants,unsigned count,ID3D11CommandList** staged=nullptr,
        SceneLighting scene=SceneLighting{false,false});
    void Reset() noexcept;
    // Region of the eye picture sampled around one item (pixels, square).
    static constexpr unsigned kSceneRegion=256,kSceneMips=9;
private:
    HRESULT Prepare(ID3D11Device* device,unsigned width,unsigned height);
    // False leaves the fixed lighting; never fails the draw.
    bool PrepareScene(ID3D11RenderTargetView* target,ID3D11Texture2D*& source,UINT& sourceSubresource,
        bool& linearView);
    template<class T> using Ptr=Microsoft::WRL::ComPtr<T>;
    Ptr<ID3D11Device> device_;
    Ptr<ID3D11DeviceContext> context_;
    Ptr<ID3D11VertexShader> vs_;
    Ptr<ID3D11PixelShader> ps_;
    Ptr<ID3D11InputLayout> layout_;
    Ptr<ID3D11Buffer> vertices_,genericVertices_,grenadeVertices_,constants_;
    Ptr<ID3D11RasterizerState> raster_;
    Ptr<ID3D11DepthStencilState> depthState_;
    Ptr<ID3D11DepthStencilView> depth_;
    Ptr<ID3D11ShaderResourceView> surface_;
    Ptr<ID3D11SamplerState> sampler_,wrapSampler_;
    Ptr<ID3D11ShaderResourceView> grenadeSurface_;
    Ptr<ID3D11Texture2D> scene_;
    Ptr<ID3D11ShaderResourceView> sceneView_;
    DXGI_FORMAT sceneResourceFormat_{},sceneViewFormat_{};
    bool sceneUnsupported_{},sceneCaptured_[kMaxItems]{};
    unsigned width_{},height_{};
    HRESULT prepared_{S_FALSE};
};
