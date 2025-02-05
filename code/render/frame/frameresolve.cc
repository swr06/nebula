//------------------------------------------------------------------------------
//  @file frameresolve.cc
//  @copyright (C) 2022 Individual contributors, see AUTHORS file
//------------------------------------------------------------------------------
#include "foundation/stdneb.h"
#include "frameresolve.h"
#include "coregraphics/graphicsdevice.h"
#include "coregraphics/texture.h"


using namespace CoreGraphics;
namespace Frame
{

//------------------------------------------------------------------------------
/**
*/
FrameResolve::FrameResolve()
    : fromBits(CoreGraphics::ImageBits::ColorBits)
    , toBits(CoreGraphics::ImageBits::ColorBits)
{
}

//------------------------------------------------------------------------------
/**
*/
FrameResolve::~FrameResolve()
{
}

//------------------------------------------------------------------------------
/**
*/
FrameOp::Compiled*
FrameResolve::AllocCompiled(Memory::ArenaAllocator<BIG_CHUNK>& allocator)
{
    CompiledImpl* ret = allocator.Alloc<CompiledImpl>();

    PixelFormat::Code fmtSource = TextureGetPixelFormat(this->from);
    PixelFormat::Code fmtDest = TextureGetPixelFormat(this->to);
    //n_assert(fmtSource == fmtDest);
    bool isDepth = PixelFormat::IsDepthFormat(fmtSource);

    if (isDepth)
    {
        TextureDimensions dimsSource = TextureGetDimensions(this->from);
        TextureDimensions dimsDest = TextureGetDimensions(this->to);
        n_assert(dimsSource == dimsDest);

        uint samples = TextureGetNumSamples(this->from);

        auto shaderName = Util::String::Sprintf("shd:msaaresolvedepth%d.fxb", samples);
        ShaderId shader = ShaderGet(shaderName);
        ShaderProgramId program = ShaderGetProgram(shader, ShaderFeatureFromString("Resolve"));

        ret->dispatchDims.x = Math::divandroundup(dimsSource.width, 64);
        ret->dispatchDims.y = dimsSource.height;
        ret->program = program;
        ret->shaderResolve = true;

        ret->constants.resolveSource = TextureGetBindlessHandle(this->from);
        ret->constants.width = dimsSource.width;

        ret->resourceTables.Resize(GetNumBufferedFrames());
        for (IndexT i = 0; i < ret->resourceTables.Size(); i++)
        {
            ret->resourceTables[i] = ShaderCreateResourceTable(shader, NEBULA_BATCH_GROUP);
            ResourceTableSetRWTexture(ret->resourceTables[i], { this->to, Msaaresolvedepth4::Table_Batch::resolve_SLOT, true, true });
            ResourceTableCommitChanges(ret->resourceTables[i]);
        }
    }
    else
    {
        ret->shaderResolve = false;
    }
#if NEBULA_GRAPHICS_DEBUG
    ret->name = this->name;
#endif

    ret->toBits = this->toBits;
    ret->fromBits = this->fromBits;
    ret->from = this->from;
    ret->to = this->to;
    return ret;
}

//------------------------------------------------------------------------------
/**
*/
void
FrameResolve::CompiledImpl::Run(const CoreGraphics::CmdBufferId cmdBuf, const IndexT frameIndex, const IndexT bufferIndex)
{

    if (this->shaderResolve)
    {
        N_CMD_SCOPE(cmdBuf, NEBULA_MARKER_COMPUTE, this->name.Value());

        CmdSetShaderProgram(cmdBuf, this->program);
        CmdSetResourceTable(cmdBuf, this->resourceTables[bufferIndex], NEBULA_BATCH_GROUP, CoreGraphics::ComputePipeline, nullptr);
        CmdDispatch(cmdBuf, this->dispatchDims.x, this->dispatchDims.y, 1);
    }
    else
    {
        N_CMD_SCOPE(cmdBuf, NEBULA_MARKER_TRANSFER, this->name.Value());

        TextureDimensions fromDims = TextureGetDimensions(this->from);
        TextureDimensions toDims = TextureGetDimensions(this->to);
        CoreGraphics::TextureCopy from, to;
        from.region.set(0, 0, fromDims.width, fromDims.height);
        from.mip = 0;
        from.layer = 0;
        from.bits = this->fromBits;
        to.region.set(0, 0, toDims.width, toDims.height);
        to.mip = 0;
        to.layer = 0;
        to.bits = this->toBits;
        CmdResolve(cmdBuf, this->from, from, this->to, to);
    }
}

//------------------------------------------------------------------------------
/**
*/
void
FrameResolve::CompiledImpl::SetupConstants(const IndexT bufferIndex)
{
    uint offset = SetConstants(this->constants);
    ResourceTableSetConstantBuffer(this->resourceTables[bufferIndex],
                                   {
                                       CoreGraphics::GetGraphicsConstantBuffer()
                                       , Msaaresolvedepth4::Table_Batch::ResolveBlock::SLOT
                                       , Msaaresolvedepth4::Table_Batch::ResolveBlock::SIZE
                                       , (SizeT)offset
                                   });
    ResourceTableCommitChanges(this->resourceTables[bufferIndex]);
}

} // namespace Frame
