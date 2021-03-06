#pragma once

#include <Tempest/AbstractGraphicsApi>

#include <d3d12.h>
#include "gapi/directx12/comptr.h"
#include "dxfbolayout.h"

namespace Tempest {

namespace Detail {

class DxDevice;
class DxTexture;
class DxSwapchain;

class DxFramebuffer : public AbstractGraphicsApi::Fbo {
  public:
    DxFramebuffer(DxDevice& dev, DxFboLayout& lay, DxSwapchain &swapchain, size_t image);
    DxFramebuffer(DxDevice& dev, DxFboLayout& lay, DxSwapchain &swapchain, size_t image, DxTexture &zbuf);
    DxFramebuffer(DxDevice& dev, DxFboLayout& lay, DxTexture &cl, DxTexture &zbuf);
    DxFramebuffer(DxDevice& dev, DxFboLayout& lay, DxTexture &cl);
    ~DxFramebuffer();

    ComPtr<ID3D12DescriptorHeap>              rtvHeap;
    UINT                                      rtvHeapInc = 0;
    ComPtr<ID3D12DescriptorHeap>              dsvHeap;

    struct View {
      ID3D12Resource* res       = nullptr;
      bool            isSwImage = false;
      DXGI_FORMAT     format    = DXGI_FORMAT_UNKNOWN;
      };
    std::unique_ptr<View[]>  views;
    UINT32                   viewsCount;

    View                     depth;
    DSharedPtr<DxFboLayout*> lay;

  private:
    void setupViews(ID3D12Device& device, const std::initializer_list<ID3D12Resource*>& res, ID3D12Resource* ds);
  };
}
}


