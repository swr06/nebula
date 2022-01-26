//------------------------------------------------------------------------------
//  api.cc
//  (C) 2020 Individual contributors, see AUTHORS file
//------------------------------------------------------------------------------
#include "application/stdneb.h"
#include "api.h"
#include "gameserver.h"
#include "ids/idallocator.h"
#include "memdb/tablesignature.h"
#include "memdb/database.h"
#include "memory/arenaallocator.h"
#include "basegamefeature/managers/blueprintmanager.h"
#include "profiling/profiling.h"
#include "util/fixedarray.h"

namespace Game
{

//------------------------------------------------------------------------------
using InclusiveTableMask = MemDb::TableSignature;
using ExclusiveTableMask = MemDb::TableSignature;
using PropertyArray = Util::FixedArray<PropertyId>;
using AccessModeArray = Util::FixedArray<AccessMode>;

Ids::IdAllocator<InclusiveTableMask, ExclusiveTableMask, PropertyArray, AccessModeArray>  filterAllocator;
static Memory::ArenaAllocator<sizeof(Dataset::CategoryTableView) * 256> viewAllocator;

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
/**
*/
World*
GetWorld(uint32_t hash)
{
    return GameServer::Instance()->GetWorld(hash);
}

//------------------------------------------------------------------------------
/**
*/
Filter
CreateFilter(FilterCreateInfo const& info)
{
    n_assert(info.numInclusive > 0);
    uint32_t filter = filterAllocator.Alloc();

    PropertyArray inclusiveArray;
    inclusiveArray.Resize(info.numInclusive);
    for (uint8_t i = 0; i < info.numInclusive; i++)
    {
        inclusiveArray[i] = info.inclusive[i];
    }

    PropertyArray exclusiveArray;
    exclusiveArray.Resize(info.numExclusive);
    for (uint8_t i = 0; i < info.numExclusive; i++)
    {
        exclusiveArray[i] = info.exclusive[i];
    }

    AccessModeArray accessArray;
    accessArray.Resize(info.numInclusive);
    for (uint8_t i = 0; i < info.numInclusive; i++)
    {
        accessArray[i] = info.access[i];
    }

    filterAllocator.Set(filter,
        InclusiveTableMask(inclusiveArray),
        ExclusiveTableMask(exclusiveArray),
        inclusiveArray,
        accessArray
    );

    return filter;
}

//------------------------------------------------------------------------------
/**
*/
void
DestroyFilter(Filter filter)
{
    filterAllocator.Dealloc(filter);
}

//------------------------------------------------------------------------------
/**
*/
ProcessorHandle
CreateProcessor(ProcessorCreateInfo const& info)
{
    return Game::GameServer::Instance()->CreateProcessor(info);
}

//------------------------------------------------------------------------------
/**
*/
void
ReleaseDatasets()
{
    viewAllocator.Release();
}

//------------------------------------------------------------------------------
/**
    @returns    Dataset with category table views.

    @note       The category table view buffer can be NULL if the filter contains
                a non-typed/flag property.
*/
Dataset Query(World* world, Filter filter)
{
#if NEBULA_ENABLE_PROFILING
    //N_COUNTER_INCR("Calls to Game::Query", 1);
    N_SCOPE_ACCUM(QueryTime, EntitySystem);
#endif
    Ptr<MemDb::Database> db = Game::GetWorldDatabase(world);

    Util::Array<MemDb::TableId> tids = db->Query(filterAllocator.Get<0>(filter), filterAllocator.Get<1>(filter));

    return Query(world, tids, filter);
}

//------------------------------------------------------------------------------
/**
*/
Dataset
Query(World* world, Util::Array<MemDb::TableId>& tids, Filter filter)
{
    Ptr<MemDb::Database> db = Game::GetWorldDatabase(world);
    return Query(db, tids, filter);
}

//------------------------------------------------------------------------------
/**
*/
Dataset
Query(Ptr<MemDb::Database> const& db, Util::Array<MemDb::TableId>& tids, Filter filter)
{
    Dataset data;
    data.numViews = 0;

    if (tids.Size() == 0)
    {
        data.views = nullptr;
        return data;
    }

    data.views = (Dataset::CategoryTableView*)viewAllocator.Alloc(sizeof(Dataset::CategoryTableView) * tids.Size());

    PropertyArray const& properties = filterAllocator.Get<2>(filter);

    for (IndexT tableIndex = 0; tableIndex < tids.Size(); tableIndex++)
    {
        if (db->IsValid(tids[tableIndex]))
        {
            SizeT const numRows = db->GetNumRows(tids[tableIndex]);
            if (numRows > 0)
            {
                Dataset::CategoryTableView* view = data.views + data.numViews;
                view->cid = tids[tableIndex];

                MemDb::Table const& tbl = db->GetTable(tids[tableIndex]);

                IndexT i = 0;
                for (auto pid : properties)
                {
                    MemDb::ColumnIndex colId = db->GetColumnId(tbl.tid, pid);
                    // Check if the property is a flag, and return a nullptr in that case.
                    if (colId != InvalidIndex)
                        view->buffers[i] = db->GetBuffer(tbl.tid, colId);
                    else
                        view->buffers[i] = nullptr;
                    i++;
                }

                view->numInstances = numRows;
                data.numViews++;
            }
        }
        else
        {
            tids.EraseIndexSwap(tableIndex);
            // re-run the same index
            tableIndex--;
        }
    }

    return data;
}

//------------------------------------------------------------------------------
/**
*/
PropertyId
CreateProperty(PropertyCreateInfo const& info)
{
    PropertyId const pid = MemDb::TypeRegistry::Register(info.name, info.byteSize, info.defaultValue, info.flags);
    return pid;
}

//------------------------------------------------------------------------------
/**
*/
PropertyId
GetPropertyId(Util::StringAtom name)
{
    return MemDb::TypeRegistry::GetPropertyId(name);
}

//------------------------------------------------------------------------------
/**
*/
BlueprintId
GetBlueprintId(Util::StringAtom name)
{
    return BlueprintManager::GetBlueprintId(name);
}

//------------------------------------------------------------------------------
/**
*/
TemplateId
GetTemplateId(Util::StringAtom name)
{
    return BlueprintManager::GetTemplateId(name);
}

//------------------------------------------------------------------------------
/**
*/
InclusiveTableMask const&
GetInclusiveTableMask(Filter filter)
{
    return filterAllocator.Get<0>(filter);
}

//------------------------------------------------------------------------------
/**
*/
ExclusiveTableMask const&
GetExclusiveTableMask(Filter filter)
{
    return filterAllocator.Get<1>(filter);
}

} // namespace Game
