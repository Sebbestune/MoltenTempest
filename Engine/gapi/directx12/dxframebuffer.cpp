#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxframebuffer.h"
#include "dxdevice.h"
#include "dxswapchain.h"
#include "dxtexture.h"
#include "guid.h"

using namespace Tempest;
using namespace Tempest::Detail;

DxFramebuffer::DxFramebuffer(DxDevice& dev, DxFboLayout& lay, DxSwapchain& swapchain, size_t image)
  :viewsCount(1), lay(&lay) {
  setupViews(*dev.device, {swapchain.views[image].get()}, nullptr);
  views[0].isSwImage = true;
  }

DxFramebuffer::DxFramebuffer(DxDevice& dev, DxFboLayout& lay, DxSwapchain& swapchain, size_t image, DxTexture& zbuf)
  :viewsCount(1), lay(&lay) {
  setupViews(*dev.device, {swapchain.views[image].get()}, zbuf.impl.get());
  views[0].isSwImage = true;
  }

DxFramebuffer::DxFramebuffer(DxDevice& dev, DxFboLayout& lay, DxTexture& cl, DxTexture& zbuf)
  :viewsCount(1), lay(&lay) {
  setupViews(*dev.device, {cl.impl.get()}, zbuf.impl.get());
  }

DxFramebuffer::DxFramebuffer(DxDevice& dev, DxFboLayout& lay, DxTexture& cl)
  :viewsCount(1), lay(&lay) {
  setupViews(*dev.device, {cl.impl.get()}, nullptr);
  views[0].isSwImage = false;
  }

DxFramebuffer::~DxFramebuffer() {
  }

void DxFramebuffer::setupViews(ID3D12Device& device,
                               const std::initializer_list<ID3D12Resource*>& res, ID3D12Resource* ds) {
  // descriptor heap
  D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
  rtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  rtvHeapDesc.NumDescriptors = UINT(res.size() + ds==nullptr ? 0 : 1);
  rtvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  dxAssert(device.CreateDescriptorHeap(&rtvHeapDesc, uuid<ID3D12DescriptorHeap>(), reinterpret_cast<void**>(&rtvHeap)));

  // frame resources.
  views.reset(new View[viewsCount]);
  for(size_t i=0;i<viewsCount;++i) {
    views[i].res = *(res.begin()+i);
    auto desc = views[i].res->GetDesc();
    views[i].format = desc.Format;
    }

  if(ds!=nullptr) {
    D3D12_DESCRIPTOR_HEAP_DESC dsHeapDesc = {};
    dsHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsHeapDesc.NumDescriptors = UINT(res.size() + ds==nullptr ? 0 : 1);
    dsHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dxAssert(device.CreateDescriptorHeap(&dsHeapDesc, uuid<ID3D12DescriptorHeap>(), reinterpret_cast<void**>(&dsvHeap)));

    auto desc = ds->GetDesc();
    depth.res = ds;
    depth.format = desc.Format;
    }

  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
  rtvHeapInc = device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  for(size_t i=0;i<viewsCount;++i) {
    device.CreateRenderTargetView(views[i].res, nullptr, rtvHandle);
    rtvHandle.ptr+=rtvHeapInc;
    }

  if(ds!=nullptr) {
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvHeap->GetCPUDescriptorHandleForHeapStart();
    device.CreateDepthStencilView(depth.res, nullptr, dsvHandle);
    }
  }

#endif
