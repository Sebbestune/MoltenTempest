#pragma once

#include <cstdint>

namespace Tempest {

enum class MemUsage : uint8_t {
  TransferSrc  =1<<0,
  TransferDst  =1<<1,
  UniformBuffer=1<<2,
  VertexBuffer =1<<3,
  IndexBuffer  =1<<4,
  };

inline MemUsage operator | (MemUsage a,const MemUsage& b) {
  return MemUsage(uint8_t(a)|uint8_t(b));
  }

inline MemUsage operator & (MemUsage a,const MemUsage& b) {
  return MemUsage(uint8_t(a)&uint8_t(b));
  }


enum class BufferHeap : uint8_t {
  Static   = 0,
  Upload   = 1,
  Readback = 3,
  };


enum class TextureLayout : uint8_t {
  Undefined,
  Sampler,
  ColorAttach,
  DepthAttach,
  Present,
  TransferSrc,
  TransferDest
  };
}
