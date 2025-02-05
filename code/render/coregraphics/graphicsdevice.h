#pragma once
//------------------------------------------------------------------------------
/**
    @struct CoreGraphics::GraphicsDeviceCreateInfo

    The Graphics Device is the engine which drives the graphics abstraction layer

    @copyright
    (C) 2018-2020 Individual contributors, see AUTHORS file
*/
//------------------------------------------------------------------------------
#include "pass.h"
#include "profiling/profiling.h"
#include "frame/framescript.h"
#include "primitivegroup.h"
#include "buffer.h"
#include "imagefileformat.h"
#include "io/stream.h"
#include "fence.h"
#include "debug/debugcounter.h"
#include "coregraphics/rendereventhandler.h"
#include "coregraphics/resourcetable.h"
#include "memory/rangeallocator.h"
#include "timing/timer.h"
#include "util/stack.h"
#include "memory.h"
#include "util/set.h"

namespace CoreGraphics
{

struct GraphicsDeviceCreateInfo
{
    uint64 globalConstantBufferMemorySize;
    uint64 globalVertexBufferMemorySize;
    uint64 globalIndexBufferMemorySize;
    uint64 globalUploadMemorySize;
    uint64 memoryHeaps[NumMemoryPoolTypes];
    uint64 maxOcclusionQueries, maxTimestampQueries, maxStatisticsQueries;
    byte numBufferedFrames : 3;
    bool enableValidation : 1;      // enables validation layer and writes output to console
};

/// create graphics device
bool CreateGraphicsDevice(const GraphicsDeviceCreateInfo& info);
/// destroy graphics device
void DestroyGraphicsDevice();

/// get number of buffered frames
SizeT GetNumBufferedFrames();
/// get current frame index, which is between 0 and GetNumBufferedFrames
IndexT GetBufferedFrameIndex();

struct DrawThreadResult
{
    CoreGraphics::CmdBufferId buf;
    Threading::Event* event;
};

struct SubmissionWaitEvent
{
    // Constructor
    SubmissionWaitEvent();
    // Comparison operator with nullptr
    const bool operator==(const std::nullptr_t) const;
    // Comparison operator with nullptr
    const bool operator!=(const std::nullptr_t) const;
    // Invalidate
    void operator=(const std::nullptr_t);

#if __VULKAN__
    uint64_t timelineIndex;
#endif
    CoreGraphics::QueueType queue;
};

struct GraphicsDeviceState
{
    Util::Array<CoreGraphics::TextureId> backBuffers;

    CoreGraphics::CmdBufferPoolId setupTransferCommandBufferPool;
    CoreGraphics::CmdBufferId setupTransferCommandBuffer;
    CoreGraphics::CmdBufferPoolId setupGraphicsCommandBufferPool;
    CoreGraphics::CmdBufferId setupGraphicsCommandBuffer;

    Util::FixedArray<CoreGraphics::FenceId> presentFences;
    Util::FixedArray<CoreGraphics::SemaphoreId> renderingFinishedSemaphores;

    uint globalConstantBufferMaxValue;
    Util::FixedArray<CoreGraphics::BufferId> globalConstantStagingBuffer;
    CoreGraphics::BufferId globalGraphicsConstantBuffer;
    CoreGraphics::BufferId globalComputeConstantBuffer;

    CoreGraphics::ResourceTableId tickResourceTableGraphics;
    CoreGraphics::ResourceTableId tickResourceTableCompute;
    CoreGraphics::ResourceTableId frameResourceTableGraphics;
    CoreGraphics::ResourceTableId frameResourceTableCompute;

    Util::Array<Ptr<CoreGraphics::RenderEventHandler> > eventHandlers;

    Memory::RangeAllocator vertexAllocator;
    CoreGraphics::BufferId vertexBuffer;

    Memory::RangeAllocator indexAllocator;
    CoreGraphics::BufferId indexBuffer;

    int globalUploadBufferMaxValue;
    CoreGraphics::BufferId uploadBuffer;

    bool isOpen : 1;
    bool inNotifyEventHandlers : 1;
    bool renderWireframe : 1;
    bool visualizeMipMaps : 1;
    bool enableValidation : 1;
    IndexT currentFrameIndex = 0;

    _declare_counter(NumImageBytesAllocated);
    _declare_counter(NumBufferBytesAllocated);
    _declare_counter(NumBytesAllocated);
    _declare_counter(GraphicsDeviceNumComputes);
    _declare_counter(GraphicsDeviceNumPrimitives);
    _declare_counter(GraphicsDeviceNumDrawCalls);

#ifdef NEBULA_ENABLE_PROFILING
    Util::Array<FrameProfilingMarker> frameProfilingMarkers;
#endif NEBULA_ENABLE_PROFILING
};

struct GraphicsDeviceThreadState
{
    CoreGraphics::CmdBufferId graphicsCommandBuffer, computeCommandBuffer;

    CmdPipelineBuildBits currentPipelineBits;
    CoreGraphics::PrimitiveTopology::Code primitiveTopology;
    CoreGraphics::PrimitiveGroup primitiveGroup;
    CoreGraphics::PassId pass;
    bool isOpen : 1;
    bool inNotifyEventHandlers : 1;
    bool renderWireframe : 1;
    bool visualizeMipMaps : 1;
    bool usePatches : 1;
    bool enableValidation : 1;
};

/// retrieve the current graphics device state
GraphicsDeviceState const* const GetGraphicsDeviceState();

/// attach a render event handler
void AttachEventHandler(const Ptr<CoreGraphics::RenderEventHandler>& h);
/// remove a render event handler
void RemoveEventHandler(const Ptr<CoreGraphics::RenderEventHandler>& h);
/// notify event handlers about an event
bool NotifyEventHandlers(const CoreGraphics::RenderEvent& e);

/// add a render texture used by the back buffer
void AddBackBufferTexture(const CoreGraphics::TextureId tex);
/// remove a render texture
void RemoveBackBufferTexture(const CoreGraphics::TextureId tex);

/// Lock resource command buffer
const CoreGraphics::CmdBufferId LockTransferSetupCommandBuffer();
/// Release lock on resource command buffer
void UnlockTransferSetupCommandBuffer();
/// Lock handover command buffer
const CoreGraphics::CmdBufferId LockGraphicsSetupCommandBuffer();
/// Release lock on handover command buffer
void UnlockGraphicsSetupCommandBuffer();

/// Submit a command buffer, but doesn't necessarily execute it immediately
SubmissionWaitEvent SubmitCommandBuffer(const CoreGraphics::CmdBufferId cmds, CoreGraphics::QueueType type);
/// Wait for a submission
void WaitForSubmission(SubmissionWaitEvent index, CoreGraphics::QueueType type, CoreGraphics::QueueType waitType);
/// Have a queue wait for another queue
void WaitForLastSubmission(CoreGraphics::QueueType type, CoreGraphics::QueueType waitType);

/// Unlock constants
void UnlockConstantUpdates();
/// Allocate range of memory and set data, return offset (thread safe)
template<class TYPE> uint SetConstants(const TYPE& data);
/// Allocate range of memory and set data as an array of elements, return offset  (thread safe)
template<class TYPE> uint SetConstants(const TYPE* data, SizeT elements);
/// Set constants based on pre-allocated memory  (thread safe)
template<class TYPE> void SetConstants(ConstantBufferOffset offset, const TYPE& data);
/// Set constants based on pre-allocated memory  (thread safe)
template<class TYPE> void SetConstants(ConstantBufferOffset offset, const TYPE* data, SizeT numElements);
/// Lock constant updates
void LockConstantUpdates();

/// Use pre-allocated range of memory to update graphics constants
void SetConstantsInternal(ConstantBufferOffset offset, const void* data, SizeT size);
/// Reserve range of constant buffer memory and return offset
ConstantBufferOffset AllocateConstantBufferMemory(uint size);

/// return id to global graphics constant buffer
CoreGraphics::BufferId GetGraphicsConstantBuffer();
/// Return buffer used for compute constants
CoreGraphics::BufferId GetComputeConstantBuffer();
/// Flush constants for queue type, do this before recording any commands doing draw or dispatch
void FlushConstants(const CoreGraphics::CmdBufferId cmds, const CoreGraphics::QueueType queue);

/// Allocate vertices from the global vertex pool
uint AllocateVertices(const SizeT numVertices, const SizeT vertexSize);
/// Deallocate vertices
void DeallocateVertices(uint offset);
/// Get vertex buffer 
const CoreGraphics::BufferId GetVertexBuffer();

/// Allocate indices from the global index pool
uint AllocateIndices(const SizeT numIndices, const IndexType::Code indexType);
/// Deallocate indices
void DeallocateIndices(uint offset);
/// Get index buffer
const CoreGraphics::BufferId GetIndexBuffer();

/// Allocate upload memory
uint AllocateUpload(const SizeT numBytes, const SizeT alignment = 1);
/// Upload single item to GPU source buffer
template<class TYPE> uint Upload(const TYPE& data);
/// Upload array of items to GPU source buffer
template<class TYPE> uint Upload(const TYPE* data, SizeT elements);
/// Upload item to GPU source buffer with preallocated memory
template<class TYPE> void Upload(uint offset, const TYPE& data);
/// Upload array of items GPU source buffer with preallocated memory
template<class TYPE> void Upload(uint offset, const TYPE* data, SizeT elements);
/// Upload memory to upload buffer, return offset
uint Upload(const void* data, SizeT numBytes);
/// Upload memory to upload buffer with alignment, return offset
uint Upload(const void* data, SizeT numBytes, SizeT alignment);
/// Upload memory to upload buffer with given offset, return offset
void UploadInternal(uint offset, const void* data, SizeT size);
/// Flush upload buffer
void FlushUpload();
/// Get upload buffer
const CoreGraphics::BufferId GetUploadBuffer();

/// trigger reloading a shader
void ReloadShaderProgram(const CoreGraphics::ShaderProgramId& pro);

/// wait for an individual queue to finish
void WaitForQueue(CoreGraphics::QueueType queue);
/// wait for all queues to finish
void WaitAndClearPendingCommands();

/// Add buffer to delete queue
void DelayedDeleteBuffer(const CoreGraphics::BufferId id);
/// Add texture to delete queue
void DelayedDeleteTexture(const CoreGraphics::TextureId id);
/// Add texture view to delete queue
void DelayedDeleteTextureView(const CoreGraphics::TextureViewId id);
/// Add command buffer to late deletion
void DelayedDeleteCommandBuffer(const CoreGraphics::CmdBufferId id);
/// Add memory allocation to delete queue
void DelayedFreeMemory(const CoreGraphics::Alloc alloc);
/// Add a descriptor set to delete queue
void DelayedDeleteDescriptorSet(const CoreGraphics::ResourceTableId id);
/// Add a pass to delayed delete
void DelayedDeletePass(const CoreGraphics::PassId id);

/// Allocate a range of queries
uint AllocateQueries(const CoreGraphics::QueryType type, uint numQueries);
/// Copy query results to buffer
void FinishQueries(const CoreGraphics::CmdBufferId cmdBuf, const CoreGraphics::QueryType type, IndexT start, SizeT count);

/// Get queue index
IndexT GetQueueIndex(const QueueType queue);
/// Get queue indices
const Util::Set<uint32_t>& GetQueueIndices();

/// Swap
void Swap(IndexT i);
/// Finish current frame
void FinishFrame(IndexT frameIndex);
/// Progress to next frame
void NewFrame();

/// save a screenshot to the provided stream
CoreGraphics::ImageFileFormat::Code SaveScreenshot(CoreGraphics::ImageFileFormat::Code fmt, const Ptr<IO::Stream>& outStream);
/// save a region of the screen to the provided stream
CoreGraphics::ImageFileFormat::Code SaveScreenshot(CoreGraphics::ImageFileFormat::Code fmt, const Ptr<IO::Stream>& outStream, const Math::rectangle<int>& rect, int x, int y);
/// get visualization of mipmaps flag   
bool GetVisualizeMipMaps();
/// set visualization of mipmaps flag   
void SetVisualizeMipMaps(bool val);
/// get the render as wireframe flag
bool GetRenderWireframe();
/// set the render as wireframe flag
void SetRenderWireframe(bool b);

#if NEBULA_ENABLE_PROFILING
/// insert timestamp, returns handle to timestamp, which can be retreived on the next N'th frame where N is the number of buffered frames
IndexT Timestamp(CoreGraphics::QueueType queue, const CoreGraphics::PipelineStage stage, const char* name);
/// get cpu profiling markers
const Util::Array<FrameProfilingMarker>& GetProfilingMarkers();
/// get number of draw calls this frame
SizeT GetNumDrawCalls();
#endif

#if NEBULA_GRAPHICS_DEBUG
/// set debug name for object
template<typename OBJECT_ID_TYPE> void ObjectSetName(const OBJECT_ID_TYPE id, const char* name);
/// begin debug marker region
void QueueBeginMarker(const CoreGraphics::QueueType queue, const Math::vec4& color, const char* name);
/// end debug marker region
void QueueEndMarker(const CoreGraphics::QueueType queue);
/// insert marker
void QueueInsertMarker(const CoreGraphics::QueueType queue, const Math::vec4& color, const char* name);
#endif

//------------------------------------------------------------------------------
/**
*/
template<class TYPE>
inline ConstantBufferOffset
SetConstants(const TYPE& data)
{
    const uint uploadSize = sizeof(TYPE);
    ConstantBufferOffset ret = AllocateConstantBufferMemory(uploadSize);
    SetConstantsInternal(ret, &data, uploadSize);
    return ret;
}

//------------------------------------------------------------------------------
/**
*/
template<class TYPE>
inline ConstantBufferOffset
SetConstants(const TYPE* data, SizeT numElements)
{
    const uint uploadSize = sizeof(TYPE) * numElements;
    ConstantBufferOffset ret = AllocateConstantBufferMemory(uploadSize);
    SetConstantsInternal(ret, data, uploadSize);
    return ret;
}

//------------------------------------------------------------------------------
/**
*/
template<class TYPE>
inline void
SetConstants(ConstantBufferOffset offset, const TYPE& data)
{
    SetConstantsInternal(offset, &data, sizeof(TYPE));
}

//------------------------------------------------------------------------------
/**
*/
template<class TYPE>
inline void
SetConstants(ConstantBufferOffset offset, const TYPE* data, SizeT numElements)
{
    SetConstantsInternal(offset, data, sizeof(TYPE) * numElements);
}

//------------------------------------------------------------------------------
/**
*/
template<class TYPE>
inline uint 
Upload(const TYPE& data)
{
    const uint uploadSize = sizeof(TYPE);
    uint offset = AllocateUpload(uploadSize);
    UploadInternal(offset, &data, uploadSize);
    return offset;
}

//------------------------------------------------------------------------------
/**
*/
template<class TYPE>
inline uint
Upload(const TYPE* data, SizeT numElements)
{
    const uint uploadSize = sizeof(TYPE) * numElements;
    uint offset = AllocateUpload(uploadSize);
    UploadInternal(offset, data, uploadSize);
    return offset;
}

//------------------------------------------------------------------------------
/**
*/
template<class TYPE>
inline void
Upload(const uint offset, const TYPE& data)
{
    const uint uploadSize = sizeof(TYPE);
    UploadInternal(offset, &data, uploadSize);
}

//------------------------------------------------------------------------------
/**
*/
template<class TYPE>
inline void
Upload(const uint offset, const TYPE* data, SizeT numElements)
{
    const uint uploadSize = sizeof(TYPE) * numElements;
    UploadInternal(offset, data, uploadSize);
}

//------------------------------------------------------------------------------
/**
*/
inline uint
Upload(const void* data, SizeT numBytes)
{
    uint offset = AllocateUpload(numBytes);
    UploadInternal(offset, data, numBytes);
    return offset;
}

//------------------------------------------------------------------------------
/**
*/
inline uint 
Upload(const void* data, SizeT numBytes, SizeT alignment)
{
    uint offset = AllocateUpload(numBytes, alignment);
    UploadInternal(offset, data, numBytes);
    return offset;
}

} // namespace CoreGraphics
