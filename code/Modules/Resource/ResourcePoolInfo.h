#pragma once
//------------------------------------------------------------------------------
/**
    @class Oryol::ResourcePoolInfo
    @ingroup Resource
    @brief detailed resource pool information

    Note the querying resource pool information is relatively slow
    since the function must iterate over all slots.
*/
#include "Core/Containers/StaticArray.h"
#include "Resource/ResourceState.h"

namespace Oryol {

class ResourcePoolInfo {
public:
    /// constructor
    ResourcePoolInfo() {
        this->NumSlotsByState.Fill(0);
    }
    
    /// number of resource slots by their state
    StaticArray<int32, ResourceState::NumStates> NumSlotsByState;
    /// resource type of the pool
    uint16 ResourceType = Id::InvalidType;
    /// overall number of slots
    int32 NumSlots = 0;
    /// number of used slots
    int32 NumUsedSlots = 0;
    /// number of free slots
    int32 NumFreeSlots = 0;
};

} // namespace Oryol
