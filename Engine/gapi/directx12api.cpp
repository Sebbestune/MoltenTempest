#include "directx12api.h"

#if defined(TEMPEST_BUILD_DIRECTX12)
#include <dxgi1_6.h>
#include <d3d12.h>

#include "directx12/guid.h"
#include "directx12/comptr.h"
#include "directx12/dxdevice.h"
#include "directx12/dxbuffer.h"
#include "directx12/dxtexture.h"
#include "directx12/dxshader.h"
#include "directx12/dxpipeline.h"
#include "directx12/dxuniformslay.h"
#include "directx12/dxcommandbuffer.h"
#include "directx12/dxswapchain.h"
#include "directx12/dxfence.h"
#include "directx12/dxframebuffer.h"
#include "directx12/dxdescriptorarray.h"
#include "directx12/dxrenderpass.h"
#include "directx12/dxfbolayout.h"

#include <Tempest/Pixmap>

using namespace Tempest;
using namespace Tempest::Detail;

struct DirectX12Api::Impl {
  Impl(bool validation) {
    if(validation) {
      dxAssert(D3D12GetDebugInterface(uuid<ID3D12Debug>(), reinterpret_cast<void**>(&D3D12DebugController)));
      D3D12DebugController->EnableDebugLayer();
      }
    // Note  Don't mix the use of DXGI 1.0 (IDXGIFactory) and DXGI 1.1 (IDXGIFactory1) in an application.
    // Use IDXGIFactory or IDXGIFactory1, but not both in an application.
    // https://docs.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-d3d12createdevice
    dxAssert(CreateDXGIFactory1(uuid<IDXGIFactory4>(), reinterpret_cast<void**>(&DXGIFactory)));
    }

  std::vector<Props> devices() {
    auto& dxgi = *DXGIFactory;
    std::vector<Props> d;

    ComPtr<IDXGIAdapter1> adapter;
    for(UINT i = 0; DXGI_ERROR_NOT_FOUND != dxgi.EnumAdapters1(i, &adapter.get()); ++i) {
      DXGI_ADAPTER_DESC1 desc={};
      adapter->GetDesc1(&desc);
      if(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        continue;

      AbstractGraphicsApi::Props props={};
      DxDevice::getProp(desc,props);
      auto hr = D3D12CreateDevice(adapter.get(), D3D_FEATURE_LEVEL_11_0, uuid<ID3D12Device>(), nullptr);
      if(SUCCEEDED(hr))
        d.push_back(props);
      }

    return d;
    }

  AbstractGraphicsApi::Device* createDevice(const char* gpuName) {
    auto& dxgi = *DXGIFactory;

    ComPtr<IDXGIAdapter1> adapter;
    for(UINT i = 0; DXGI_ERROR_NOT_FOUND != dxgi.EnumAdapters1(i, &adapter.get()); ++i) {
      DXGI_ADAPTER_DESC1 desc={};
      adapter->GetDesc1(&desc);
      if(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        continue;

      AbstractGraphicsApi::Props props={};
      DxDevice::getProp(desc,props);
      if(gpuName!=nullptr && std::strcmp(props.name,gpuName)!=0)
        continue;

      // Check to see if the adapter supports Direct3D 12, but don't create the
      // actual device yet.
      auto hr = D3D12CreateDevice(adapter.get(), D3D_FEATURE_LEVEL_11_0, uuid<ID3D12Device>(), nullptr);
      if(SUCCEEDED(hr))
        break;
      }

    if(adapter.get()==nullptr)
      throw std::system_error(Tempest::GraphicsErrc::NoDevice);
    return new DxDevice(*adapter);
    }

  void submit(AbstractGraphicsApi::Device* d,
              ID3D12CommandList** cmd, size_t count,
              AbstractGraphicsApi::Semaphore** wait, size_t waitCnt,
              AbstractGraphicsApi::Semaphore** done, size_t doneCnt,
              AbstractGraphicsApi::Fence* doneCpu){
    Detail::DxDevice& dx   = *reinterpret_cast<Detail::DxDevice*>(d);
    Detail::DxFence&  fcpu = *reinterpret_cast<Detail::DxFence*>(doneCpu);

    std::lock_guard<SpinLock> guard(dx.syncCmdQueue);
    fcpu.reset();

    for(size_t i=0;i<waitCnt;++i) {
      Detail::DxSemaphore& swait = *reinterpret_cast<Detail::DxSemaphore*>(wait[i]);
      dx.cmdQueue->Wait(swait.impl.get(),DxFence::Ready);
      }

    dx.cmdQueue->ExecuteCommandLists(UINT(count), cmd);

    for(size_t i=0;i<doneCnt;++i) {
      Detail::DxSemaphore& sgpu = *reinterpret_cast<Detail::DxSemaphore*>(done[i]);
      dx.cmdQueue->Signal(sgpu.impl.get(),DxFence::Ready);
      }
    fcpu.signal(*dx.cmdQueue);
    }

  ComPtr<ID3D12Debug>    D3D12DebugController;
  ComPtr<IDXGIFactory4>  DXGIFactory;
  };

DirectX12Api::DirectX12Api(ApiFlags f) {
  impl.reset(new Impl(bool(f&ApiFlags::Validation)));
  }

DirectX12Api::~DirectX12Api(){
  }

std::vector<AbstractGraphicsApi::Props> DirectX12Api::devices() const {
  return impl->devices();
  }

AbstractGraphicsApi::Device* DirectX12Api::createDevice(const char* gpuName) {
  return impl->createDevice(gpuName);
  }

void DirectX12Api::destroy(AbstractGraphicsApi::Device* d) {
  delete d;
  }

AbstractGraphicsApi::Swapchain* DirectX12Api::createSwapchain(SystemApi::Window* w, AbstractGraphicsApi::Device* d) {
  auto* dx = reinterpret_cast<Detail::DxDevice*>(d);
  return new Detail::DxSwapchain(*dx,*impl->DXGIFactory,w);
  }

AbstractGraphicsApi::PPass DirectX12Api::createPass(AbstractGraphicsApi::Device*, const FboMode** att, size_t acount) {
  return PPass(new DxRenderPass(att,acount));
  }

AbstractGraphicsApi::PFbo DirectX12Api::createFbo(AbstractGraphicsApi::Device* d, AbstractGraphicsApi::FboLayout* l,
                                                  AbstractGraphicsApi::Swapchain* s, uint32_t imageId) {
  auto& dx  = *reinterpret_cast<Detail::DxDevice*>   (d);
  auto& lay = *reinterpret_cast<Detail::DxFboLayout*>(l);
  auto& sx  = *reinterpret_cast<Detail::DxSwapchain*>(s);
  return PFbo(new DxFramebuffer(dx,lay,sx,imageId));
  }

AbstractGraphicsApi::PFbo DirectX12Api::createFbo(AbstractGraphicsApi::Device* d, AbstractGraphicsApi::FboLayout* l,
                                                  AbstractGraphicsApi::Swapchain* s, uint32_t imageId, AbstractGraphicsApi::Texture* zbuf) {
  auto& dx  = *reinterpret_cast<Detail::DxDevice*>   (d);
  auto& lay = *reinterpret_cast<Detail::DxFboLayout*>(l);
  auto& sx  = *reinterpret_cast<Detail::DxSwapchain*>(s);
  auto& z   = *reinterpret_cast<Detail::DxTexture*>(zbuf);
  return PFbo(new DxFramebuffer(dx,lay,sx,imageId,z));
  }

AbstractGraphicsApi::PFbo DirectX12Api::createFbo(AbstractGraphicsApi::Device* d, AbstractGraphicsApi::FboLayout* l,
                                                  uint32_t /*w*/, uint32_t /*h*/,
                                                  AbstractGraphicsApi::Texture* cl, AbstractGraphicsApi::Texture* zbuf) {
  auto& dx  = *reinterpret_cast<Detail::DxDevice*> (d);
  auto& lay = *reinterpret_cast<Detail::DxFboLayout*>(l);
  auto& t0  = *reinterpret_cast<Detail::DxTexture*>(cl);
  auto& z   = *reinterpret_cast<Detail::DxTexture*>(zbuf);
  return PFbo(new DxFramebuffer(dx,lay,t0,z));
  }

AbstractGraphicsApi::PFbo DirectX12Api::createFbo(AbstractGraphicsApi::Device* d, AbstractGraphicsApi::FboLayout* l,
                                                  uint32_t /*w*/, uint32_t /*h*/, AbstractGraphicsApi::Texture* cl) {
  auto& dx  = *reinterpret_cast<Detail::DxDevice*> (d);
  auto& lay = *reinterpret_cast<Detail::DxFboLayout*>(l);
  auto& t0  = *reinterpret_cast<Detail::DxTexture*>(cl);
  return PFbo(new DxFramebuffer(dx,lay,t0));
  }

AbstractGraphicsApi::PFboLayout DirectX12Api::createFboLayout(AbstractGraphicsApi::Device*, AbstractGraphicsApi::Swapchain* s,
                                                              TextureFormat* att, size_t attCount) {
  auto& sx = *reinterpret_cast<Detail::DxSwapchain*>(s);
  return PFboLayout(new DxFboLayout(sx,att,attCount));
  }

AbstractGraphicsApi::PPipeline DirectX12Api::createPipeline(AbstractGraphicsApi::Device* d, const RenderState& st,
                                                            const Decl::ComponentType* decl, size_t declSize, size_t stride,
                                                            Topology tp,
                                                            const UniformsLay& ulayImpl,
                                                            const std::initializer_list<AbstractGraphicsApi::Shader*>& shaders) {
  Shader*const*        arr=shaders.begin();
  auto* dx = reinterpret_cast<Detail::DxDevice*>(d);
  auto* vs = reinterpret_cast<Detail::DxShader*>(arr[0]);
  auto* fs = reinterpret_cast<Detail::DxShader*>(arr[1]);
  auto& ul = reinterpret_cast<const Detail::DxUniformsLay&>(ulayImpl);

  return PPipeline(new Detail::DxPipeline(*dx,st,decl,declSize,stride,tp,ul,*vs,*fs));
  }

AbstractGraphicsApi::PShader DirectX12Api::createShader(AbstractGraphicsApi::Device*,
                                                        const void* source, size_t src_size) {
  return PShader(new Detail::DxShader(source,src_size));
  }

AbstractGraphicsApi::Fence* DirectX12Api::createFence(AbstractGraphicsApi::Device* d) {
  Detail::DxDevice* dx = reinterpret_cast<Detail::DxDevice*>(d);
  return new DxFence(*dx);
  }

AbstractGraphicsApi::Semaphore* DirectX12Api::createSemaphore(AbstractGraphicsApi::Device* d) {
  Detail::DxDevice* dx = reinterpret_cast<Detail::DxDevice*>(d);
  return new DxSemaphore(*dx);
  }

AbstractGraphicsApi::PBuffer DirectX12Api::createBuffer(AbstractGraphicsApi::Device* d, const void* mem,
                                                        size_t count, size_t size, size_t alignedSz,
                                                        MemUsage usage, BufferHeap flg) {
  Detail::DxDevice& dx = *reinterpret_cast<Detail::DxDevice*>(d);

  if(flg==BufferHeap::Upload) {
    Detail::DxBuffer stage=dx.allocator.alloc(mem,count,size,alignedSz,usage,BufferHeap::Upload);
    return PBuffer(new Detail::DxBuffer(std::move(stage)));
    }
  else {
    Detail::DxBuffer  stage=dx.allocator.alloc(mem,    count,size,alignedSz,MemUsage::TransferSrc,      BufferHeap::Upload);
    Detail::DxBuffer  buf  =dx.allocator.alloc(nullptr,count,size,alignedSz,usage|MemUsage::TransferDst,BufferHeap::Static);

    Detail::DSharedPtr<Detail::DxBuffer*> pstage(new Detail::DxBuffer(std::move(stage)));
    Detail::DSharedPtr<Detail::DxBuffer*> pbuf  (new Detail::DxBuffer(std::move(buf)));

    DxDevice::Data dat(dx);
    dat.flush(*pstage.handler,count*alignedSz);
    dat.hold(pbuf);
    dat.hold(pstage); // preserve stage buffer, until gpu side copy is finished
    dat.copy(*pbuf.handler,*pstage.handler,count*alignedSz);
    dat.commit();

    return PBuffer(pbuf.handler);
    }
  return PBuffer();
  }

AbstractGraphicsApi::Desc* DirectX12Api::createDescriptors(AbstractGraphicsApi::Device* d, UniformsLay& layP) {
  Detail::DxDevice&      dx = *reinterpret_cast<Detail::DxDevice*>(d);
  Detail::DxUniformsLay& u  = reinterpret_cast<Detail::DxUniformsLay&>(layP);
  return new DxDescriptorArray(dx,u);
  }

AbstractGraphicsApi::PUniformsLay DirectX12Api::createUboLayout(Device* d, const std::initializer_list<Shader*>& shaders) {
  Shader*const*        arr=shaders.begin();
  auto* dx = reinterpret_cast<Detail::DxDevice*>(d);
  auto* vs = reinterpret_cast<Detail::DxShader*>(arr[0]);
  auto* fs = reinterpret_cast<Detail::DxShader*>(arr[1]);

  return PUniformsLay(new DxUniformsLay(*dx,vs->lay,fs->lay));
  }

AbstractGraphicsApi::PTexture DirectX12Api::createTexture(Device* d, const Pixmap& p, TextureFormat frm, uint32_t mipCnt) {
  if(isCompressedFormat(frm))
    return createCompressedTexture(d,p,frm,mipCnt);

  Detail::DxDevice& dx     = *reinterpret_cast<Detail::DxDevice*>(d);

  mipCnt = 1; //TODO

  DXGI_FORMAT       format = Detail::nativeFormat(frm);
  uint32_t          row    = p.w()*p.bpp();
  const uint32_t    pith   = ((row+D3D12_TEXTURE_DATA_PITCH_ALIGNMENT-1)/D3D12_TEXTURE_DATA_PITCH_ALIGNMENT)*D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
  Detail::DxBuffer  stage  = dx.allocator.alloc(p.data(),p.h(),row,pith,MemUsage::TransferSrc,BufferHeap::Upload);
  Detail::DxTexture buf    = dx.allocator.alloc(p,mipCnt,format);

  Detail::DSharedPtr<Detail::DxBuffer*>  pstage(new Detail::DxBuffer (std::move(stage)));
  Detail::DSharedPtr<Detail::DxTexture*> pbuf  (new Detail::DxTexture(std::move(buf)));

  Detail::DxDevice::Data dat(dx);
  dat.hold(pstage);
  dat.hold(pbuf);

  // dat.changeLayout(*pbuf.handler, frm, TextureLayout::Undefined, TextureLayout::TransferDest, mipCnt);
  dat.copy(*pbuf.handler,p.w(),p.h(),0,*pstage.handler,0);
  if(mipCnt>1)
    dat.generateMipmap(*pbuf.handler,frm,p.w(),p.h(),mipCnt); else
    dat.changeLayout(*pbuf.handler, frm, TextureLayout::TransferDest, TextureLayout::Sampler,mipCnt);
  dat.commit();
  return PTexture(pbuf.handler);
  }

AbstractGraphicsApi::PTexture DirectX12Api::createCompressedTexture(Device* d, const Pixmap& p, TextureFormat frm, uint32_t mipCnt) {
  Detail::DxDevice& dx     = *reinterpret_cast<Detail::DxDevice*>(d);

  DXGI_FORMAT format    = Detail::nativeFormat(frm);
  UINT        blockSize = (frm==TextureFormat::DXT1) ? 8 : 16;

  size_t bufferSize      = 0;
  UINT   stageBufferSize = 0;
  uint32_t w = p.w(), h = p.h();

  for(uint32_t i=0; i<mipCnt; i++){
    UINT wBlk = (w+3)/4;
    UINT hBlk = (h+3)/4;
    UINT pitch = wBlk*blockSize;
    pitch = alignTo(pitch,D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
    bufferSize += wBlk*hBlk*blockSize;

    stageBufferSize += pitch*hBlk;
    stageBufferSize = alignTo(stageBufferSize,D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

    w = std::max<uint32_t>(1,w/2);
    h = std::max<uint32_t>(1,h/2);
    }

  Detail::DxBuffer  stage  = dx.allocator.alloc(nullptr,stageBufferSize,1,1,MemUsage::TransferSrc,BufferHeap::Upload);
  Detail::DxTexture buf    = dx.allocator.alloc(p,mipCnt,format);
  Detail::DSharedPtr<Detail::DxBuffer*>  pstage(new Detail::DxBuffer (std::move(stage)));
  Detail::DSharedPtr<Detail::DxTexture*> pbuf  (new Detail::DxTexture(std::move(buf)));

  pstage.handler->uploadS3TC(reinterpret_cast<const uint8_t*>(p.data()),p.w(),p.h(),mipCnt,blockSize);

  Detail::DxDevice::Data dat(dx);
  dat.hold(pstage);
  dat.hold(pbuf);

  stageBufferSize = 0;
  w = p.w();
  h = p.h();
  for(uint32_t i=0; i<mipCnt; i++) {
    UINT wBlk = (w+3)/4;
    UINT hBlk = (h+3)/4;

    dat.copy(*pbuf.handler,w,h,i,*pstage.handler,stageBufferSize);

    UINT pitch = wBlk*blockSize;
    pitch = alignTo(pitch,D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

    stageBufferSize += pitch*hBlk;
    stageBufferSize = alignTo(stageBufferSize,D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

    w = std::max<uint32_t>(4,w/2);
    h = std::max<uint32_t>(4,h/2);
    }

  dat.changeLayout(*pbuf.handler, frm, TextureLayout::TransferDest, TextureLayout::Sampler, mipCnt);
  dat.commit();
  return PTexture(pbuf.handler);
  }

AbstractGraphicsApi::PTexture DirectX12Api::createTexture(Device* d, const uint32_t w, const uint32_t h, uint32_t mipCnt, TextureFormat frm) {
  Detail::DxDevice& dx = *reinterpret_cast<Detail::DxDevice*>(d);

  Detail::DxTexture buf=dx.allocator.alloc(w,h,mipCnt,frm);
  Detail::DSharedPtr<Detail::DxTexture*> pbuf(new Detail::DxTexture(std::move(buf)));

  return PTexture(pbuf.handler);
  }

void DirectX12Api::readPixels(Device* d, Pixmap& out, const PTexture t, TextureLayout lay,
                              TextureFormat frm, const uint32_t w, const uint32_t h, uint32_t mip) {
  Detail::DxDevice&  dx = *reinterpret_cast<Detail::DxDevice*>(d);
  Detail::DxTexture& tx = *reinterpret_cast<Detail::DxTexture*>(t.handler);

  size_t         bpp  = 0;
  Pixmap::Format pfrm = Pixmap::Format::RGBA;
  switch(frm) {
    case TextureFormat::Undefined: bpp=0; break;
    case TextureFormat::Last:      bpp=0; break;
    //---
    case TextureFormat::R8:        bpp=1; pfrm = Pixmap::Format::R;      break;
    case TextureFormat::RG8:       bpp=2; pfrm = Pixmap::Format::RG;     break;
    case TextureFormat::RGB8:      bpp=3; pfrm = Pixmap::Format::RGB;    break;
    case TextureFormat::RGBA8:     bpp=4; pfrm = Pixmap::Format::RGBA;   break;
    //---
    case TextureFormat::R16:       bpp=2; pfrm = Pixmap::Format::R16;    break;
    case TextureFormat::RG16:      bpp=4; pfrm = Pixmap::Format::RG16;   break;
    case TextureFormat::RGB16:     bpp=6; pfrm = Pixmap::Format::RGB16;  break;
    case TextureFormat::RGBA16:    bpp=8; pfrm = Pixmap::Format::RGBA16; break;
    //---
    case TextureFormat::Depth16:   bpp=2; pfrm = Pixmap::Format::R16; break;
    case TextureFormat::Depth24S8: bpp=4; break;
    case TextureFormat::Depth24x8: bpp=4; break;
    // TODO: dxt
    case TextureFormat::DXT1:      bpp=0; break;
    case TextureFormat::DXT3:      bpp=0; break;
    case TextureFormat::DXT5:      bpp=0; break;
    }

  const size_t     size  = w*h*bpp;
  Detail::DxBuffer stage = dx.allocator.alloc(nullptr,size,1,1,MemUsage::TransferDst,BufferHeap::Readback);

  //TODO: D3D12_TEXTURE_DATA_PITCH_ALIGNMENT
  Detail::DxDevice::Data dat(dx);
  dat.changeLayout(tx, frm, lay, TextureLayout::TransferSrc, 1);
  dat.copy(stage,w,h,mip,tx,0);
  dat.changeLayout(tx, frm, TextureLayout::TransferSrc, lay, 1);
  dat.commit();

  dx.waitData();

  out = Pixmap(w,h,pfrm);
  stage.read(out.data(),0,size);
  }

AbstractGraphicsApi::CommandBuffer* DirectX12Api::createCommandBuffer(Device* d) {
  Detail::DxDevice* dx = reinterpret_cast<Detail::DxDevice*>(d);
  return new DxCommandBuffer(*dx);
  }

void DirectX12Api::present(AbstractGraphicsApi::Device* d, AbstractGraphicsApi::Swapchain* sw,
                           uint32_t imageId, const AbstractGraphicsApi::Semaphore* wait) {
  // TODO: handle imageId
  (void)imageId;
  Detail::DxDevice&    dx    = *reinterpret_cast<Detail::DxDevice*>(d);
  auto&                swait = *reinterpret_cast<const Detail::DxSemaphore*>(wait);
  Detail::DxSwapchain* sx    = reinterpret_cast<Detail::DxSwapchain*>(sw);

  std::lock_guard<SpinLock> guard(dx.syncCmdQueue);
  dx.cmdQueue->Wait(swait.impl.get(),DxFence::Ready);
  sx->queuePresent();
  }

void DirectX12Api::submit(AbstractGraphicsApi::Device* d, AbstractGraphicsApi::CommandBuffer* cmd,
                          AbstractGraphicsApi::Semaphore* wait,
                          AbstractGraphicsApi::Semaphore* done, AbstractGraphicsApi::Fence* doneCpu) {
  Detail::DxDevice&        dx = *reinterpret_cast<Detail::DxDevice*>(d);
  Detail::DxCommandBuffer& bx = *reinterpret_cast<Detail::DxCommandBuffer*>(cmd);
  ID3D12CommandList* cmdList[] = { bx.impl.get() };

  dx.waitData();
  impl->submit(d,cmdList,1,&wait,1,&done,1,doneCpu);
  }

void DirectX12Api::submit(AbstractGraphicsApi::Device* d, AbstractGraphicsApi::CommandBuffer** cmd, size_t count,
                          AbstractGraphicsApi::Semaphore** wait, size_t waitCnt, AbstractGraphicsApi::Semaphore** done,
                          size_t doneCnt, AbstractGraphicsApi::Fence* doneCpu) {
  Detail::DxDevice&                     dx = *reinterpret_cast<Detail::DxDevice*>(d);
  ID3D12CommandList*                    cmdListStk[16]={};
  std::unique_ptr<ID3D12CommandList*[]> cmdListHeap;
  ID3D12CommandList**                   cmdList = cmdListStk;
  if(count>16) {
    cmdListHeap.reset(new ID3D12CommandList*[16]);
    cmdList = cmdListHeap.get();
    }
  for(size_t i=0;i<count;++i) {
    Detail::DxCommandBuffer& bx = *reinterpret_cast<Detail::DxCommandBuffer*>(cmd[i]);
    cmdList[i] = bx.impl.get();
    }
  dx.waitData();
  impl->submit(d,cmdList,count,wait,waitCnt,done,doneCnt,doneCpu);
  }

void DirectX12Api::getCaps(AbstractGraphicsApi::Device* d, AbstractGraphicsApi::Props& caps) {
  Detail::DxDevice& dx = *reinterpret_cast<Detail::DxDevice*>(d);
  caps = dx.props;
  }


#endif
