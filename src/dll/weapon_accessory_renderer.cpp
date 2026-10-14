#include "weapon_accessory_renderer.h"
#include "../common/weapon_magazines.generated.h"
#include "../common/weapon_generic_magazine.h"
#include "../common/weapon_surfaces.generated.h"
#include <d3dcompiler.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>

namespace
{
constexpr char shader[]=R"(
cbuffer Transform : register(b0) { float4 objectQ,objectP,eyeQ,eyeP,tangents,color; };
Texture2D surface : register(t0);
Texture2DArray scene : register(t1);
SamplerState surfaceSampler : register(s0);
float3 rotate(float4 q,float3 p) { return p+2*cross(q.xyz,cross(q.xyz,p)+q.w*p); }
struct V { float3 p:POSITION; float3 n:NORMAL; float2 uv:TEXCOORD; };
struct O { float4 p:SV_Position; float3 n:NORMAL; float2 uv:TEXCOORD; };
O vs_main(V v) {
    O o;
    float3 p=rotate(eyeQ,rotate(objectQ,v.p)+objectP.xyz-eyeP.xyz);
    float z=-p.z;
    o.p=float4((2*p.x-(tangents.y+tangents.x)*z)/(tangents.y-tangents.x),
        (2*p.y-(tangents.w+tangents.z)*z)/(tangents.w-tangents.z),
        (20.0*z-0.02*20.0)/(20.0-0.02),z);
    o.n=rotate(objectQ,v.n);o.uv=v.uv;return o;
}
float4 ps_main(O i):SV_Target {
    float diffuse=abs(dot(normalize(i.n),normalize(float3(-0.3,0.8,0.5))));
    float3 albedo=surface.Sample(surfaceSampler,i.uv).rgb*color.xyz;
    // color.w: 0 = fixed lighting; else scene slice+1, +0.5 when the target
    // view is linear (sRGB or float: samples and writes are linear there).
    if(color.w<0.75) return float4(albedo*(0.35+0.65*diffuse),1);
    float slot=floor(color.w)-1;
    bool linearView=frac(color.w)>0.25;
    float3 average=max(scene.SampleLevel(surfaceSampler,float3(0.5,0.5,slot),8).rgb,0);
    float3 display=linearView?pow(average,1.0/2.2):average;
    float luminance=dot(display,float3(0.2126,0.7152,0.0722));
    // Mid-grey surroundings keep the authored look; darker or brighter
    // surroundings scale it, with a mild tint from the local colour.
    float exposure=clamp(luminance/0.40,0.06,1.3);
    float3 tint=lerp(float3(1,1,1),clamp(display/max(luminance,0.001),0,2),0.35);
    float3 scale=exposure*tint;
    if(linearView) scale=pow(scale,2.2);
    return float4(albedo*(0.5+0.5*diffuse)*scale,1);
}
)";
}
void WeaponAccessoryRenderer::Reset() noexcept
{
    if(context_) context_->ClearState();
    depth_.Reset();depthState_.Reset();raster_.Reset();constants_.Reset();vertices_.Reset();genericVertices_.Reset();
    surface_.Reset();sampler_.Reset();sceneView_.Reset();scene_.Reset();
    sceneResourceFormat_=sceneViewFormat_=DXGI_FORMAT_UNKNOWN;sceneUnsupported_=false;
    for(auto& captured:sceneCaptured_) captured=false;
    layout_.Reset();ps_.Reset();vs_.Reset();context_.Reset();device_.Reset();
    width_=height_=0;prepared_=S_FALSE;
}
HRESULT WeaponAccessoryRenderer::Prepare(ID3D11Device* device,unsigned width,unsigned height)
{
    if(!device||!width||!height||width>16384||height>16384) return E_INVALIDARG;
    if(device_.Get()!=device) { Reset();device_=device; }
    if(FAILED(prepared_)) return prepared_; // no per-frame shader compilation retries
    if(prepared_==S_FALSE)
    {
        HRESULT hr=device->CreateDeferredContext(0,&context_);
        Ptr<ID3DBlob> vs,ps,error;
        if(SUCCEEDED(hr)) hr=D3DCompile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,
            "vs_main","vs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&vs,&error);
        error.Reset();
        if(SUCCEEDED(hr)) hr=D3DCompile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,
            "ps_main","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&ps,&error);
        if(SUCCEEDED(hr)) hr=device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&vs_);
        if(SUCCEEDED(hr)) hr=device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,&ps_);
        const D3D11_INPUT_ELEMENT_DESC elements[]{
            {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
            {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0},
            {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0}};
        if(SUCCEEDED(hr)) hr=device->CreateInputLayout(elements,3,vs->GetBufferPointer(),vs->GetBufferSize(),&layout_);
        D3D11_TEXTURE2D_DESC surface{};
        surface.Width=weapon_model::kSurfaceWidth;surface.Height=weapon_model::kSurfaceHeight;
        surface.MipLevels=surface.ArraySize=1;surface.SampleDesc.Count=1;
        surface.Format=DXGI_FORMAT_BC1_UNORM;surface.Usage=D3D11_USAGE_IMMUTABLE;
        surface.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA pixels{weapon_model::kSurfaceBlocks,weapon_model::kSurfaceWidth*2,0};
        Ptr<ID3D11Texture2D> surfaceTexture;
        if(SUCCEEDED(hr)) hr=device->CreateTexture2D(&surface,&pixels,&surfaceTexture);
        if(SUCCEEDED(hr)) hr=device->CreateShaderResourceView(surfaceTexture.Get(),nullptr,&surface_);
        D3D11_SAMPLER_DESC sampler{};sampler.Filter=D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler.MaxLOD=D3D11_FLOAT32_MAX;sampler.ComparisonFunc=D3D11_COMPARISON_NEVER;
        if(SUCCEEDED(hr)) hr=device->CreateSamplerState(&sampler,&sampler_);
        D3D11_BUFFER_DESC buffer{};buffer.ByteWidth=sizeof(weapon_model::kVertices);
        buffer.Usage=D3D11_USAGE_IMMUTABLE;buffer.BindFlags=D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA data{weapon_model::kVertices};
        if(SUCCEEDED(hr)) hr=device->CreateBuffer(&buffer,&data,&vertices_);
        buffer.ByteWidth=sizeof(weapon_model::kGenericVertices);data.pSysMem=weapon_model::kGenericVertices;
        if(SUCCEEDED(hr)) hr=device->CreateBuffer(&buffer,&data,&genericVertices_);
        buffer.ByteWidth=sizeof(weapon_accessory::Constants);buffer.Usage=D3D11_USAGE_DYNAMIC;
        buffer.BindFlags=D3D11_BIND_CONSTANT_BUFFER;buffer.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
        if(SUCCEEDED(hr)) hr=device->CreateBuffer(&buffer,nullptr,&constants_);
        D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;
        raster.CullMode=D3D11_CULL_NONE;raster.DepthClipEnable=TRUE;
        if(SUCCEEDED(hr)) hr=device->CreateRasterizerState(&raster,&raster_);
        D3D11_DEPTH_STENCIL_DESC depth{};depth.DepthEnable=TRUE;
        depth.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;depth.DepthFunc=D3D11_COMPARISON_LESS;
        if(SUCCEEDED(hr)) hr=device->CreateDepthStencilState(&depth,&depthState_);
        prepared_=hr;
        if(FAILED(hr)) return hr;
    }
    if(width_!=width||height_!=height||!depth_)
    {
        depth_.Reset();width_=height_=0;
        D3D11_TEXTURE2D_DESC desc{};desc.Width=width;desc.Height=height;
        desc.ArraySize=desc.MipLevels=1;desc.SampleDesc.Count=1;
        desc.Format=DXGI_FORMAT_D32_FLOAT;desc.BindFlags=D3D11_BIND_DEPTH_STENCIL;
        Ptr<ID3D11Texture2D> texture;
        HRESULT hr=device->CreateTexture2D(&desc,nullptr,&texture);
        if(SUCCEEDED(hr)) hr=device->CreateDepthStencilView(texture.Get(),nullptr,&depth_);
        if(FAILED(hr)) { prepared_=hr;return hr; }
        width_=width;height_=height;
    }
    return S_OK;
}
bool WeaponAccessoryRenderer::PrepareScene(ID3D11RenderTargetView* target,ID3D11Texture2D*& source,
    UINT& sourceSubresource,bool& linearView)
{
    source=nullptr;sourceSubresource=0;linearView=false;
    if(sceneUnsupported_||!target||!device_) return false;
    D3D11_RENDER_TARGET_VIEW_DESC view{};target->GetDesc(&view);
    Ptr<ID3D11Resource> resource;target->GetResource(&resource);
    Ptr<ID3D11Texture2D> texture;
    if(!resource||FAILED(resource.As(&texture))) return false;
    D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
    if(desc.SampleDesc.Count!=1) return false;
    if(view.ViewDimension==D3D11_RTV_DIMENSION_TEXTURE2D)
        sourceSubresource=D3D11CalcSubresource(view.Texture2D.MipSlice,0,desc.MipLevels);
    else if(view.ViewDimension==D3D11_RTV_DIMENSION_TEXTURE2DARRAY)
        sourceSubresource=D3D11CalcSubresource(view.Texture2DArray.MipSlice,view.Texture2DArray.FirstArraySlice,desc.MipLevels);
    else return false;
    const DXGI_FORMAT viewFormat=view.Format;
    if(!scene_||sceneResourceFormat_!=desc.Format||sceneViewFormat_!=viewFormat)
    {
        sceneView_.Reset();scene_.Reset();
        for(auto& captured:sceneCaptured_) captured=false;
        UINT support=0;
        constexpr UINT needed=D3D11_FORMAT_SUPPORT_TEXTURE2D|D3D11_FORMAT_SUPPORT_RENDER_TARGET|
            D3D11_FORMAT_SUPPORT_SHADER_SAMPLE|D3D11_FORMAT_SUPPORT_MIP_AUTOGEN;
        if(FAILED(device_->CheckFormatSupport(viewFormat,&support))||(support&needed)!=needed)
        { sceneUnsupported_=true;return false; }
        D3D11_TEXTURE2D_DESC sceneDesc{};
        sceneDesc.Width=sceneDesc.Height=kSceneRegion;sceneDesc.MipLevels=kSceneMips;sceneDesc.ArraySize=kMaxItems;
        sceneDesc.Format=desc.Format;sceneDesc.SampleDesc.Count=1;sceneDesc.Usage=D3D11_USAGE_DEFAULT;
        sceneDesc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
        sceneDesc.MiscFlags=D3D11_RESOURCE_MISC_GENERATE_MIPS;
        D3D11_SHADER_RESOURCE_VIEW_DESC sceneView{};sceneView.Format=viewFormat;
        sceneView.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        sceneView.Texture2DArray.MipLevels=kSceneMips;sceneView.Texture2DArray.ArraySize=kMaxItems;
        HRESULT hr=device_->CreateTexture2D(&sceneDesc,nullptr,&scene_);
        if(SUCCEEDED(hr)) hr=device_->CreateShaderResourceView(scene_.Get(),&sceneView,&sceneView_);
        if(FAILED(hr)) { sceneView_.Reset();scene_.Reset();sceneUnsupported_=true;return false; }
        sceneResourceFormat_=desc.Format;sceneViewFormat_=viewFormat;
    }
    source=texture.Get();
    linearView=viewFormat==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB||viewFormat==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB||
        viewFormat==DXGI_FORMAT_B8G8R8X8_UNORM_SRGB||viewFormat==DXGI_FORMAT_R16G16B16A16_FLOAT||
        viewFormat==DXGI_FORMAT_R11G11B10_FLOAT||viewFormat==DXGI_FORMAT_R32G32B32A32_FLOAT;
    return true;
}
namespace
{
// Pixel centre of an item in the eye picture, from its draw constants.
bool ScreenCentre(const weapon_accessory::Constants& c,const D3D11_VIEWPORT& viewport,float& x,float& y) noexcept
{
    const float q[4]{c.eyeQ[0],c.eyeQ[1],c.eyeQ[2],c.eyeQ[3]};
    const float v[3]{c.objectP[0]-c.eyeP[0],c.objectP[1]-c.eyeP[1],c.objectP[2]-c.eyeP[2]};
    // p + 2*cross(q, cross(q,p) + w*p), as the vertex shader rotates.
    const float t[3]{q[1]*v[2]-q[2]*v[1]+q[3]*v[0],q[2]*v[0]-q[0]*v[2]+q[3]*v[1],q[0]*v[1]-q[1]*v[0]+q[3]*v[2]};
    const float p[3]{v[0]+2*(q[1]*t[2]-q[2]*t[1]),v[1]+2*(q[2]*t[0]-q[0]*t[2]),v[2]+2*(q[0]*t[1]-q[1]*t[0])};
    const float z=-p[2];
    const float l=c.tangents[0],r=c.tangents[1],d=c.tangents[2],u=c.tangents[3];
    if(!(z>0.02f)||!(r-l>0.01f)||!(u-d>0.01f)) return false;
    const float ndcX=(2*p[0]/z-(r+l))/(r-l),ndcY=(2*p[1]/z-(u+d))/(u-d);
    if(!(std::fabs(ndcX)<4)||!(std::fabs(ndcY)<4)) return false;
    x=viewport.TopLeftX+(ndcX*0.5f+0.5f)*viewport.Width;
    y=viewport.TopLeftY+(0.5f-ndcY*0.5f)*viewport.Height;
    return true;
}
bool Drawable(const weapon_model::Model* model) noexcept
{
    const bool generic=weapon_model::UsesTokenGeometry(model);
    const size_t available=generic?std::size(weapon_model::kGenericVertices):std::size(weapon_model::kVertices);
    return model&&model->vertexCount&&model->firstVertex<=available&&
        model->vertexCount<=available-model->firstVertex&&model->vertexCount%3==0;
}
}
HRESULT WeaponAccessoryRenderer::Draw(ID3D11Device* device,ID3D11DeviceContext* immediate,
    ID3D11RenderTargetView* target,unsigned width,unsigned height,const D3D11_VIEWPORT& viewport,
    const weapon_accessory::Presentation& item,const weapon_accessory::Constants& constants,ID3D11CommandList** staged)
{
    const weapon_accessory::Presentation* items[]{&item};
    return DrawMany(device,immediate,target,width,height,viewport,items,&constants,1,staged,SceneLighting{false,false});
}
HRESULT WeaponAccessoryRenderer::DrawMany(ID3D11Device* device,ID3D11DeviceContext* immediate,
    ID3D11RenderTargetView* target,unsigned width,unsigned height,const D3D11_VIEWPORT& viewport,
    const weapon_accessory::Presentation* const* items,const weapon_accessory::Constants* constants,
    unsigned count,ID3D11CommandList** staged,SceneLighting sceneLighting)
{
    if(staged) *staged=nullptr;
    if(!immediate||!target||!items||!constants||!count||count>kMaxItems) return E_INVALIDARG;
    unsigned drawable=0;
    for(unsigned i=0;i<count;++i) drawable+=items[i]&&Drawable(items[i]->model)?1u:0u;
    if(!drawable) return E_INVALIDARG;
    HRESULT hr=Prepare(device,width,height);
    if(FAILED(hr)) return hr;
    // Optional scene lighting: copy the picture around each item before any
    // item is drawn, then average it down to one colour per item.
    ID3D11Texture2D* source=nullptr;UINT sourceSubresource=0;bool linearView=false;
    const bool scene=sceneLighting.enabled&&PrepareScene(target,source,sourceSubresource,linearView);
    bool sceneUsable[kMaxItems]{};
    if(scene)
    {
        D3D11_TEXTURE2D_DESC sourceDesc{};source->GetDesc(&sourceDesc);
        const UINT mip=sourceSubresource%sourceDesc.MipLevels;
        const UINT sourceWidth=(std::max)(1u,sourceDesc.Width>>mip),sourceHeight=(std::max)(1u,sourceDesc.Height>>mip);
        bool copied=false;
        for(unsigned i=0;i<count&&sceneLighting.capture;++i)
        {
            float x=0,y=0;
            if(!items[i]||!Drawable(items[i]->model)||!ScreenCentre(constants[i],viewport,x,y)) continue;
            // Keep the region inside both the eye's rectangle and the texture.
            const float left=(std::max)(0.0f,viewport.TopLeftX),top=(std::max)(0.0f,viewport.TopLeftY);
            const float right=(std::min)(float(sourceWidth),viewport.TopLeftX+viewport.Width);
            const float bottom=(std::min)(float(sourceHeight),viewport.TopLeftY+viewport.Height);
            if(right-left<float(kSceneRegion)||bottom-top<float(kSceneRegion)) continue;
            const float half=float(kSceneRegion)*0.5f;
            const UINT boxLeft=UINT(std::clamp(x-half,left,right-float(kSceneRegion)));
            const UINT boxTop=UINT(std::clamp(y-half,top,bottom-float(kSceneRegion)));
            const D3D11_BOX box{boxLeft,boxTop,0,boxLeft+kSceneRegion,boxTop+kSceneRegion,1};
            context_->CopySubresourceRegion(scene_.Get(),D3D11CalcSubresource(0,i,kSceneMips),0,0,0,
                source,sourceSubresource,&box);
            sceneCaptured_[i]=copied=true;
        }
        if(copied) context_->GenerateMips(sceneView_.Get());
        for(unsigned i=0;i<count;++i) sceneUsable[i]=sceneCaptured_[i];
    }
    context_->OMSetRenderTargets(1,&target,depth_.Get());
    context_->ClearDepthStencilView(depth_.Get(),D3D11_CLEAR_DEPTH,1,0);
    context_->OMSetDepthStencilState(depthState_.Get(),0);
    context_->OMSetBlendState(nullptr,nullptr,0xFFFFFFFF);
    context_->RSSetState(raster_.Get());context_->RSSetViewports(1,&viewport);
    context_->IASetInputLayout(layout_.Get());
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    const UINT stride=sizeof(weapon_model::Vertex),offset=0;
    ID3D11Buffer* cb=constants_.Get();
    context_->VSSetConstantBuffers(0,1,&cb);
    context_->PSSetConstantBuffers(0,1,&cb);
    ID3D11ShaderResourceView* surface=surface_.Get();ID3D11SamplerState* sampler=sampler_.Get();
    context_->PSSetShaderResources(0,1,&surface);context_->PSSetSamplers(0,1,&sampler);
    ID3D11ShaderResourceView* sceneView=scene?sceneView_.Get():nullptr;
    context_->PSSetShaderResources(1,1,&sceneView);
    context_->VSSetShader(vs_.Get(),nullptr,0);context_->PSSetShader(ps_.Get(),nullptr,0);
    for(unsigned i=0;i<count&&SUCCEEDED(hr);++i)
    {
        const auto* model=items[i]?items[i]->model:nullptr;
        if(!Drawable(model)) continue;
        // A deferred context records each discarded map as its own version.
        D3D11_MAPPED_SUBRESOURCE mapped{};
        hr=context_->Map(constants_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped);
        if(FAILED(hr)) break;
        weapon_accessory::Constants itemConstants=constants[i];
        itemConstants.color[3]=scene&&sceneUsable[i]?float(i+1)+(linearView?0.5f:0.0f):0.0f;
        std::memcpy(mapped.pData,&itemConstants,sizeof(itemConstants));context_->Unmap(constants_.Get(),0);
        ID3D11Buffer* vertex=weapon_model::UsesTokenGeometry(model)?genericVertices_.Get():vertices_.Get();
        context_->IASetVertexBuffers(0,1,&vertex,&stride,&offset);
        context_->Draw(model->vertexCount,model->firstVertex);
    }
    if(FAILED(hr)) { context_->ClearState();return hr; }
    Ptr<ID3D11CommandList> commands;
    hr=context_->FinishCommandList(FALSE,&commands);
    if(SUCCEEDED(hr))
    {
        if(staged) *staged=commands.Detach();
        else immediate->ExecuteCommandList(commands.Get(),TRUE);
    }
    else context_->ClearState();
    return hr;
}
