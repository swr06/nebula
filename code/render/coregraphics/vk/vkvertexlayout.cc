//------------------------------------------------------------------------------
//  @file vkvertexlayout.cc
//  @copyright (C) 2022 Individual contributors, see AUTHORS file
//------------------------------------------------------------------------------
#include "render/stdneb.h"
#include "vkvertexlayout.h"
#include "vkshader.h"
#include "vktypes.h"

namespace Vulkan
{

VkVertexLayoutAllocator vertexLayoutAllocator;
static Threading::CriticalSection vertexSignatureMutex;

//------------------------------------------------------------------------------
/**
*/
VkPipelineVertexInputStateCreateInfo*
VertexLayoutGetDerivative(const CoreGraphics::VertexLayoutId layout, const CoreGraphics::ShaderProgramId shader)
{
    Threading::CriticalScope scope(&vertexSignatureMutex);
    Util::HashTable<uint64_t, DerivativeLayout>& hashTable = vertexLayoutAllocator.Get<VertexSignature_ProgramLayoutMapping>(layout.resourceId);
    const Ids::Id64 shaderHash = shader.HashCode64();

    IndexT i = hashTable.FindIndex(shaderHash);
    if (i != InvalidIndex)
    {
        return &hashTable.ValueAtIndex(shaderHash, i).info;
    }
    else
    {
        const VkProgramReflectionInfo& program = ShaderGetProgramReflection(shader);
        const BindInfo& bindInfo = vertexLayoutAllocator.Get<VertexSignature_BindInfo>(layout.resourceId);
        const VkPipelineVertexInputStateCreateInfo& baseInfo = vertexLayoutAllocator.Get<VertexSignature_VkPipelineInfo>(layout.resourceId);

        IndexT index = hashTable.Add(shaderHash, {});
        DerivativeLayout& layout = hashTable.ValueAtIndex(shaderHash, index);
        layout.info = baseInfo;

        uint32_t i;
        IndexT j;
        for (i = 0; i < program.vsInputSlots.Size(); i++)
        {
            uint32_t slot = program.vsInputSlots[i];
            for (j = 0; j < bindInfo.attrs.Size(); j++)
            {
                VkVertexInputAttributeDescription attr = bindInfo.attrs[j];
                if (attr.location == slot)
                {
                    layout.attrs.Append(attr);
                    break;
                }
            }
        }

        if (program.vsInputSlots.Size() != (uint32_t)layout.attrs.Size())
            n_warning("Warning: Vertex shader (%s) and vertex layout mismatch!\n", program.name.Value());

        layout.info.vertexAttributeDescriptionCount = layout.attrs.Size();
        layout.info.pVertexAttributeDescriptions = layout.attrs.Begin();
        return &layout.info;
    }
}

} // namespace Vulkan
namespace CoreGraphics
{

using namespace Vulkan;

//------------------------------------------------------------------------------
/**
*/
const VertexLayoutId
CreateVertexLayout(const VertexLayoutCreateInfo& info)
{
    Ids::Id32 id = vertexLayoutAllocator.Alloc();

    VertexLayoutInfo loadInfo;

    Util::String sig;
    IndexT i;
    SizeT size = 0;
    for (i = 0; i < info.comps.Size(); i++)
    {
        sig.Append(info.comps[i].GetSignature());
        info.comps[i].byteOffset += size;
        size += info.comps[i].GetByteSize();
    }
    sig = Util::String::Sprintf("%s", sig.AsCharPtr());
    Util::StringAtom atom(sig);

    loadInfo.signature = Util::StringAtom(sig);
    loadInfo.vertexByteSize = size;
    loadInfo.shader = Ids::InvalidId64;
    loadInfo.comps = info.comps;
    vertexLayoutAllocator.Set<VertexSignature_LayoutInfo>(id, loadInfo);


    Util::HashTable<uint64_t, DerivativeLayout>& hashTable = vertexLayoutAllocator.Get<VertexSignature_ProgramLayoutMapping>(id);
    VkPipelineVertexInputStateCreateInfo& vertexInfo = vertexLayoutAllocator.Get<VertexSignature_VkPipelineInfo>(id);
    BindInfo& bindInfo = vertexLayoutAllocator.Get<VertexSignature_BindInfo>(id);

    // create binds
    bindInfo.binds.Resize(CoreGraphics::MaxNumVertexStreams);
    bindInfo.attrs.Resize(loadInfo.comps.Size());

    SizeT strides[CoreGraphics::MaxNumVertexStreams] = { 0 };

    uint32_t numUsedStreams = 0;
    IndexT curOffset[CoreGraphics::MaxNumVertexStreams];
    bool usedStreams[CoreGraphics::MaxNumVertexStreams];
    Memory::Fill(curOffset, CoreGraphics::MaxNumVertexStreams * sizeof(IndexT), 0);
    Memory::Fill(usedStreams, CoreGraphics::MaxNumVertexStreams * sizeof(bool), 0);

    IndexT compIndex;
    for (compIndex = 0; compIndex < loadInfo.comps.Size(); compIndex++)
    {
        const CoreGraphics::VertexComponent& component = loadInfo.comps[compIndex];
        VkVertexInputAttributeDescription* attr = &bindInfo.attrs[compIndex];

        attr->location = component.GetSemanticName();
        attr->binding = component.GetStreamIndex();
        attr->format = VkTypes::AsVkVertexType(component.GetFormat());
        attr->offset = curOffset[component.GetStreamIndex()];

        if (usedStreams[attr->binding])
            bindInfo.binds[attr->binding].stride += component.GetByteSize();
        else
        {
            bindInfo.binds[attr->binding].stride = component.GetByteSize();
            usedStreams[attr->binding] = true;
            numUsedStreams++;
        }

        bindInfo.binds[attr->binding].binding = component.GetStreamIndex();
        bindInfo.binds[attr->binding].inputRate = component.GetStrideType() == CoreGraphics::VertexComponent::PerVertex ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE;
        curOffset[component.GetStreamIndex()] += component.GetByteSize();
    }

    vertexInfo =
    {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        NULL,
        0,
        numUsedStreams,
        bindInfo.binds.Begin(),
        (uint32_t)bindInfo.attrs.Size(),
        bindInfo.attrs.Begin()
    };

    VertexLayoutId ret;
    ret.resourceId = id;
    ret.resourceType = VertexLayoutIdType;
    return ret;
}

//------------------------------------------------------------------------------
/**
*/
void
DestroyVertexLayout(const VertexLayoutId id)
{
    vertexLayoutAllocator.Dealloc(id.resourceId);
}

//------------------------------------------------------------------------------
/**
*/
const SizeT
VertexLayoutGetSize(const VertexLayoutId id)
{
    return vertexLayoutAllocator.Get<VertexSignature_LayoutInfo>(id.resourceId).vertexByteSize;
}

//------------------------------------------------------------------------------
/**
*/
const Util::Array<VertexComponent>&
VertexLayoutGetComponents(const VertexLayoutId id)
{
    return vertexLayoutAllocator.Get<VertexSignature_LayoutInfo>(id.resourceId).comps;
}

} // namespace Vulkan
