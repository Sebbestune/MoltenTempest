#if defined(TEMPEST_BUILD_DIRECTX12)
#include "dxdescriptorarray.h"

#include <Tempest/UniformsLayout>

#include "dxbuffer.h"
#include "dxdevice.h"
#include "dxtexture.h"
#include "guid.h"

#include <cassert>

using namespace Tempest;
using namespace Tempest::Detail;

static int swizzle(ComponentSwizzle cs, int def){
  switch(cs) {
    case ComponentSwizzle::Identity:
      return def;
    case ComponentSwizzle::R:
      return 0;
    case ComponentSwizzle::G:
      return 1;
    case ComponentSwizzle::B:
      return 2;
    case ComponentSwizzle::A:
      return 3;
    }
  return def;
  }

DxDescriptorArray::DxDescriptorArray(DxDevice& dev, DxUniformsLay& vlay)
  : layPtr(&vlay), dev(dev) {
  val     = layPtr.handler->allocDescriptors();
  heapCnt = UINT(vlay.heaps.size());
  }

DxDescriptorArray::~DxDescriptorArray() {
  layPtr.handler->freeDescriptors(val);
  }

void DxDescriptorArray::set(size_t id, AbstractGraphicsApi::Texture* tex, const Sampler2d& smp) {
  auto&      device = *dev.device;
  DxTexture& t      = *reinterpret_cast<DxTexture*>(tex);

  // Describe and create a SRV for the texture.
  int mapv[4] = { swizzle(smp.mapping.r,0),
                  swizzle(smp.mapping.g,1),
                  swizzle(smp.mapping.b,2),
                  swizzle(smp.mapping.a,3) };

  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
  srvDesc.Shader4ComponentMapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(mapv[0],mapv[1],mapv[2],mapv[3]);
  srvDesc.Format                  = t.format;
  srvDesc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Texture2D.MipLevels     = t.mips;

  UINT filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
  D3D12_SAMPLER_DESC smpDesc = {};
  smpDesc.Filter           = D3D12_FILTER_MIN_MAG_MIP_POINT;
  smpDesc.AddressU         = nativeFormat(smp.uClamp);
  smpDesc.AddressV         = nativeFormat(smp.vClamp);
  smpDesc.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  smpDesc.MipLODBias       = 0;
  smpDesc.MaxAnisotropy    = 1;
  smpDesc.ComparisonFunc   = D3D12_COMPARISON_FUNC_NEVER;
  smpDesc.BorderColor[0]   = 1;
  smpDesc.BorderColor[1]   = 1;
  smpDesc.BorderColor[2]   = 1;
  smpDesc.BorderColor[3]   = 1;
  smpDesc.MinLOD           = 0.0f;
  smpDesc.MaxLOD           = D3D12_FLOAT32_MAX;//FLOAT(t.mips);

  if(smp.minFilter==Filter::Linear)
    filter |= (1u<<D3D12_MIN_FILTER_SHIFT);
  if(smp.magFilter==Filter::Linear)
    filter |= (1u<<D3D12_MAG_FILTER_SHIFT);
  if(smp.mipFilter==Filter::Linear)
    filter |= (1u<<D3D12_MIP_FILTER_SHIFT);

  if(smp.anisotropic) {
    smpDesc.MaxAnisotropy = 16;
    filter |= D3D12_ANISOTROPIC_FILTERING_BIT;
    }

  smpDesc.Filter = D3D12_FILTER(filter);

  auto& prm = layPtr.handler->prm[id];
  auto  gpu = val.cpu[prm.heapId];
  gpu.ptr += prm.heapOffset;
  device.CreateShaderResourceView(t.impl.get(), &srvDesc, gpu);

  gpu = val.cpu[prm.heapIdSmp];
  gpu.ptr += prm.heapOffsetSmp;
  device.CreateSampler(&smpDesc,gpu);
  }

void DxDescriptorArray::set(size_t id, AbstractGraphicsApi::Buffer* b, size_t offset, size_t size, size_t /*align*/) {
  auto&      device = *dev.device;
  DxBuffer&  buf    = *reinterpret_cast<DxBuffer*>(b);

  D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
  cbvDesc.BufferLocation = buf.impl->GetGPUVirtualAddress()+offset;
  cbvDesc.SizeInBytes    = UINT(size); // CB size is required to be 256-byte aligned.

  auto& prm = layPtr.handler->prm[id];
  auto  gpu = val.cpu[prm.heapId];
  gpu.ptr += prm.heapOffset;
  device.CreateConstantBufferView(&cbvDesc, gpu);
  }

#endif
