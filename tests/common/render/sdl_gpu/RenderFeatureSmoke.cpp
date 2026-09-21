#include "RenderFeatureSmoke.hpp"
#include "render/ColorTransform.hpp"
#include "render/PrimitiveMesh.hpp"
#include "render/ShaderAbi.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>

namespace {
using namespace Engine::Render;
struct Resources final {
    IRenderDevice& device;
    explicit Resources(IRenderDevice& value) : device(value) {}
    std::vector<TextureHandle> textures;
    std::vector<MeshHandle> meshes;
    std::vector<ShaderHandle> shaders;
    std::vector<PipelineHandle> pipelines;
    ~Resources() {
        for (auto h : pipelines) static_cast<void>(device.ReleasePipeline(h));
        for (auto h : shaders) static_cast<void>(device.ReleaseShader(h));
        for (auto h : meshes) static_cast<void>(device.ReleaseMesh(h));
        for (auto h : textures) static_cast<void>(device.ReleaseTexture(h));
    }
    TextureHandle Image(std::uint32_t width, std::uint32_t height,
        std::span<const std::uint8_t> pixels, TextureColorSpace colorSpace=TextureColorSpace::Linear) {
        auto result=device.CreateTexture(ImageView{width,height,width*4,std::as_bytes(pixels),colorSpace});
        if (!result) return {};
        textures.push_back(result.value()); return result.value();
    }
};
bool Pixel(const SceneCapture& image, unsigned x, unsigned y,
           std::array<int,4> expected, const char* label) {
    if (x>=image.width || y>=image.height) return false;
    const auto offset=(static_cast<std::size_t>(y)*image.width+x)*4;
    for (unsigned c=0;c<4;++c) if (std::abs(int(image.rgba8[offset+c])-expected[c])>2) {
        std::cerr<<label<<" pixel "<<x<<','<<y<<" channel "<<c<<": "
                 <<int(image.rgba8[offset+c])<<" expected "<<expected[c]<<'\n'; return false;
    }
    return true;
}
bool CheckUvBlendAndTextureColor(IRenderDevice& device, Renderer& renderer) {
    Resources resources{device};
    const std::array<std::uint8_t,16> colors{255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,255,255};
    const std::array<std::uint8_t,4> gray{128,128,128,255};
    auto atlas=resources.Image(2,2,colors);
    auto srgb=resources.Image(1,1,gray,TextureColorSpace::SRgb);
    auto linear=resources.Image(1,1,gray);
    if (!atlas || !srgb || !linear) return false;
    RenderQueue queue({{0,0,1,1}});
    auto sprite=[&](TextureHandle texture, Rect rect, Rect uv=Rect{0,0,1,1}, Color tint=Color{}) {
        SpriteSubmission value; value.material.texture=texture; value.material.tint=tint;
        value.destinationPixels=rect; value.sourceUv=uv; value.layer=CompositeLayer::Scene;
        return static_cast<bool>(queue.Submit(value));
    };
    if (!sprite(atlas,{0,0,64,64}) || !sprite(atlas,{80,0,40,40},{0.5F,0,0.5F,0.5F}) ||
        !sprite(atlas,{0,75,40,40},{0,0.5F,0.5F,0.5F}) ||
        !sprite({}, {80,45,40,20}, {0,0,1,1}, {1,0,0,0.5F}) ||
        !sprite(srgb,{80,75,30,30}) || !sprite(linear,{120,75,30,30})) return false;
    renderer.RequestSceneCapture(); auto rendered=renderer.Render(queue);
    if (!rendered) { std::cerr<<rendered.error().message<<'\n'; return false; }
    auto captured=renderer.TakeSceneCapture();
    return captured && Pixel(*captured,1,1,{255,0,0,255},"texture top-left") &&
        Pixel(*captured,62,1,{0,255,0,255},"texture top-right") &&
        Pixel(*captured,1,62,{0,0,255,255},"texture bottom-left") &&
        Pixel(*captured,62,62,{255,255,255,255},"texture bottom-right") &&
        Pixel(*captured,118,1,{0,255,0,255},"atlas top-right crop") &&
        Pixel(*captured,1,112,{0,0,255,255},"atlas bottom-left crop") &&
        Pixel(*captured,100,55,{188,0,188,255},"straight alpha in linear space") &&
        Pixel(*captured,95,90,{128,128,128,255},"sRGB decode and capture roundtrip") &&
        Pixel(*captured,135,90,{188,188,188,255},"linear texture to sRGB capture");
}
Float3 RotateX(Float3 v,float a) { return {v.x,v.y*std::cos(a)-v.z*std::sin(a),v.y*std::sin(a)+v.z*std::cos(a)}; }
Float3 RotateY(Float3 v,float a) { return {v.x*std::cos(a)+v.z*std::sin(a),v.y,-v.x*std::sin(a)+v.z*std::cos(a)}; }
Float3 RotateZ(Float3 v,float a) { return {v.x*std::cos(a)-v.y*std::sin(a),v.x*std::sin(a)+v.y*std::cos(a),v.z}; }
// Independent scalar reference: do not reuse Renderer matrix helpers.
Float2 Project(Float3 vertex,const Transform3D& object,const PerspectiveCamera3D& camera,unsigned width,unsigned height) {
    vertex={vertex.x*object.scale.x,vertex.y*object.scale.y,vertex.z*object.scale.z};
    vertex=RotateZ(RotateY(RotateX(vertex,object.rotationRadians.x),object.rotationRadians.y),object.rotationRadians.z);
    vertex={vertex.x+object.translation.x-camera.position.x,vertex.y+object.translation.y-camera.position.y,vertex.z+object.translation.z-camera.position.z};
    vertex=RotateX(RotateY(RotateZ(vertex,-camera.rotationRadians.z),-camera.rotationRadians.y),-camera.rotationRadians.x);
    const float scale=float(height)/(2*std::tan(camera.verticalFieldOfViewRadians*0.5F));
    return {float(width)*0.5F+vertex.x/vertex.z*scale,float(height)*0.5F-vertex.y/vertex.z*scale};
}
bool CheckNonSymmetricProjection(IRenderDevice& device,Renderer& renderer) {
    Resources resources{device}; auto quad=MakeUnitQuadXY(); auto created=device.CreateMesh(quad.View());
    if (!created) return false;
    resources.meshes.push_back(created.value());
    PerspectiveCamera3D camera{{0.15F,0.8F,-0.4F},{-0.1F,0.22F,0.07F},0.93F,0.03F,15};
    MeshSubmission mesh; mesh.mesh=created.value(); mesh.doubleSided=true;
    mesh.transform={{0.6F,1.1F,2.3F},{0.25F,-0.33F,0.19F},{0.9F,0.5F,0.7F}};
    RenderQueue queue({{0,0,0,1}}); queue.SetCamera(camera); if (!queue.Submit(mesh)) return false;
    renderer.RequestSceneCapture(); if (!renderer.Render(queue)) return false;
    auto image=renderer.TakeSceneCapture(); if (!image) return false;
    float left=std::numeric_limits<float>::max(),top=left,right=-left,bottom=-left;
    for (auto v:quad.vertices) { auto p=Project(v.position,mesh.transform,camera,image->width,image->height);
        left=std::min(left,p.x); right=std::max(right,p.x); top=std::min(top,p.y); bottom=std::max(bottom,p.y); }
    int actualLeft=int(image->width),actualRight=-1,actualTop=int(image->height),actualBottom=-1;
    for (unsigned y=0;y<image->height;++y) for (unsigned x=0;x<image->width;++x) {
        if (image->rgba8[(std::size_t(y)*image->width+x)*4]<250) continue;
        actualLeft=std::min(actualLeft,int(x));actualRight=std::max(actualRight,int(x));
        actualTop=std::min(actualTop,int(y));actualBottom=std::max(actualBottom,int(y));
    }
    if (actualRight<0 || std::abs(actualLeft-left)>2 || std::abs(actualRight-right)>2 ||
        std::abs(actualTop-top)>2 || std::abs(actualBottom-bottom)>2) {
        std::cerr<<"non-symmetric projection bounds actual "<<actualLeft<<','<<actualTop<<','<<actualRight<<','<<actualBottom
                 <<" expected "<<left<<','<<top<<','<<right<<','<<bottom<<'\n';return false;
    }
    const auto center=Project({},mesh.transform,camera,image->width,image->height);
    return Pixel(*image,unsigned(center.x),unsigned(center.y),{255,255,255,255},"projected model center");
}
bool CheckPostColor(IRenderDevice& device,const ShaderLibrary& library) {
    Resources resources{device};
    ShaderFormat format=ShaderFormat::Metallib;
    for (auto f:{ShaderFormat::DXIL,ShaderFormat::SPIRV,ShaderFormat::Metallib}) if (device.GetInfo().shaderFormats&FormatBit(f)) {format=f;break;}
    auto program=library.FindProgram("builtin/scene_post",format); if (!program) return false;
    auto vertex=device.CreateShader(*program.value().vertex);if (!vertex)return false;resources.shaders.push_back(vertex.value());
    auto fragment=device.CreateShader(*program.value().fragment);if (!fragment)return false;resources.shaders.push_back(fragment.value());
    const std::array<std::uint8_t,4> sourcePixel{32,64,128,255}; auto source=resources.Image(1,1,sourcePixel);if(!source)return false;
    auto target=device.CreateTexture(TextureDesc{13,7,TextureFormat::Rgba8SRgb,false,true,false});if(!target)return false;resources.textures.push_back(target.value());
    auto pipeline=device.CreatePipeline({vertex.value(),fragment.value(),TextureFormat::Rgba8SRgb,false,CullMode::None,false,false,false});
    if(!pipeline)return false;
    resources.pipelines.push_back(pipeline.value());
    const ShaderAbi::SceneColorUniforms uniform{1,2,{}};
    PreparedDraw draw;draw.pipeline=pipeline.value();draw.fragmentTextures.push_back({source,SamplerMode::LinearClamp});
    draw.fragmentUniforms.emplace_back(sizeof(uniform));std::memcpy(draw.fragmentUniforms[0].data(),&uniform,sizeof(uniform));
    PreparedPass pass;pass.color=target.value();pass.colorLoad=AttachmentLoad::DontCare;pass.draws.push_back(std::move(draw));
    PreparedFrame commands;commands.passes.push_back(std::move(pass));
    auto frame=device.AcquireFrame();if(!frame||!frame.value())return false;
    auto submitted=device.SubmitFrame(*frame.value(),commands);if(!submitted)return false;
    auto output=device.ReadTexture(target.value());if(!output)return false;
    const auto& image=output.value();
    if(image.format!=TextureFormat::Rgba8SRgb||image.rowPitch<13*4||image.bytes.size()<image.rowPitch*7)return false;
    for(unsigned y=0;y<7;++y)for(unsigned x=0;x<13;++x)for(unsigned channel=0;channel<4;++channel){
        const int expected=channel==3?255:int(std::lround(EncodeSrgbComponent(std::sqrt(std::min(float(sourcePixel[channel])/255.0F*2,1.0F)))*255));
        const int actual=std::to_integer<int>(image.bytes[std::size_t(y)*image.rowPitch+x*4+channel]);
        if(std::abs(expected-actual)>2){std::cerr<<"scene post/sRGB pixel channel "<<channel<<": "<<actual<<" expected "<<expected<<'\n';return false;}
    }
    return true;
}
} // namespace
bool CheckRenderFeaturePixels(Engine::Render::IRenderDevice& device,Engine::Render::Renderer& renderer,
    const Engine::Render::ShaderLibrary& library) {
    if (!CheckUvBlendAndTextureColor(device,renderer)) {
        std::cerr << "UV/blend/texture color smoke failed\n"; return false;
    }
    if (!CheckNonSymmetricProjection(device,renderer)) {
        std::cerr << "non-symmetric projection smoke failed\n"; return false;
    }
    if (!CheckPostColor(device,library)) {
        std::cerr << "post-processing color smoke failed\n"; return false;
    }
    return true;
}
