#pragma once
//------------------------------------------------------------------------------
/**
    @class Oryol::RefCounted
    @ingroup Core
    @brief Oryol's reference-counted base class

    The RefCounted class is used together with the Ptr smart-pointer class
    to automatically manage the life-time of objects through 
    reference-counting.
 
    @see Ptr
*/
#include "Core/Types.h"
#include "Core/Ptr.h"
#include "Core/Class.h"
#include "Core/Memory/poolAllocator.h"

namespace Oryol {
    
class RefCounted {
    OryolBaseClassDecl(RefCounted);
public:
    /// destructor
    virtual ~RefCounted();
    
    /// get reference count
    int32 GetRefCount() const;
    /// add reference
    void addRef() const;
    /// release reference (calls destructor when ref_count reaches zero)
    void release() const;

private:
    #if ORYOL_HAS_ATOMIC
    mutable std::atomic<int32> refCount{0};
    #else
    mutable int32 refCount{0};
    #endif
};

//------------------------------------------------------------------------------
inline void
RefCounted::addRef() const {
    #if ORYOL_HAS_ATOMIC
    this->refCount.fetch_add(1, std::memory_order_relaxed);
    #else
    this->refCount++;
    #endif
}

//------------------------------------------------------------------------------
inline void
RefCounted::release() const {
    #if ORYOL_HAS_ATOMIC
    if (1 == this->refCount.fetch_sub(1, std::memory_order_relaxed)) {
    #else
    if (1 == this->refCount--) {
    #endif
        // destroy() is virtual and provided by the OryolClassDecl macro
        const_cast<RefCounted*>(this)->destroy();
    }
}

//------------------------------------------------------------------------------
inline int32
RefCounted::GetRefCount() const
{
    return this->refCount;
}

} // namespace Oryol
