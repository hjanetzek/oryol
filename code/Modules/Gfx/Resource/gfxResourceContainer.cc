//------------------------------------------------------------------------------
//  gfxResourceContainer.cc
//------------------------------------------------------------------------------
#include "Pre.h"
#include "Core/Core.h"
#include "IO/IO.h"
#include "gfxResourceContainer.h"
#include "Gfx/Core/displayMgr.h"

namespace Oryol {
namespace _priv {

//------------------------------------------------------------------------------
gfxResourceContainer::gfxResourceContainer() :
renderer(nullptr),
displayMgr(nullptr),
runLoopId(RunLoop::InvalidId) {
    // empty
}

//------------------------------------------------------------------------------
void
gfxResourceContainer::setup(const GfxSetup& setup, class renderer* rendr, class displayMgr* dspMgr) {
    o_assert(!this->isValid());
    o_assert(nullptr != rendr);
    o_assert(nullptr != dspMgr);
    
    this->renderer = rendr;
    this->displayMgr = dspMgr;
    
    this->meshFactory.Setup(this->renderer, &this->meshPool);
    this->meshPool.Setup(GfxResourceType::Mesh, setup.PoolSize(GfxResourceType::Mesh));
    this->shaderFactory.Setup(this->renderer);
    this->shaderPool.Setup(GfxResourceType::Shader, setup.PoolSize(GfxResourceType::Shader));
    this->programBundleFactory.Setup(this->renderer, &this->shaderPool, &this->shaderFactory);
    this->programBundlePool.Setup(GfxResourceType::ProgramBundle, setup.PoolSize(GfxResourceType::ProgramBundle));
    this->textureFactory.Setup(this->renderer, this->displayMgr, &this->texturePool);
    this->texturePool.Setup(GfxResourceType::Texture, setup.PoolSize(GfxResourceType::Texture));
    this->drawStateFactory.Setup(this->renderer, &this->meshPool, &this->programBundlePool);
    this->drawStatePool.Setup(GfxResourceType::DrawState, setup.PoolSize(GfxResourceType::DrawState));
    
    this->runLoopId = Core::PostRunLoop()->Add([this]() {
        this->update();
    });
    
    resourceContainerBase::setup(setup.ResourceLabelStackCapacity, setup.ResourceRegistryCapacity);
}

//------------------------------------------------------------------------------
void
gfxResourceContainer::discard() {
    o_assert_dbg(this->isValid());
    
    Core::PostRunLoop()->Remove(this->runLoopId);
    for (const auto& loader : this->pendingLoaders) {
        loader->Cancel();
    }
    this->pendingLoaders.Clear();
    
    resourceContainerBase::discard();

    this->drawStatePool.Discard();
    this->drawStateFactory.Discard();
    this->texturePool.Discard();
    this->textureFactory.Discard();
    this->programBundlePool.Discard();
    this->programBundleFactory.Discard();
    this->shaderPool.Discard();
    this->shaderFactory.Discard();
    this->meshPool.Discard();
    this->meshFactory.Discard();
    this->renderer = nullptr;
    this->displayMgr = nullptr;
}

//------------------------------------------------------------------------------
template<> Id
gfxResourceContainer::Create(const MeshSetup& setup) {
    o_assert_dbg(this->isValid());
    o_assert_dbg(!setup.ShouldSetupFromFile());

    Id resId = this->registry.Lookup(setup.Locator);
    if (resId.IsValid()) {
        return resId;
    }
    else {
        resId = this->meshPool.AllocId();
        this->registry.Add(setup.Locator, resId, this->peekLabel());
        mesh& res = this->meshPool.Assign(resId, setup, ResourceState::Setup);
        const ResourceState::Code newState = this->meshFactory.SetupResource(res);
        o_assert((newState == ResourceState::Valid) || (newState == ResourceState::Failed));
        this->meshPool.UpdateState(resId, newState);
    }
    return resId;
}
    
//------------------------------------------------------------------------------
template<> Id
gfxResourceContainer::Create(const MeshSetup& setup, const void* data, int32 size) {
    o_assert_dbg(this->isValid());
    o_assert_dbg(nullptr != data);
    o_assert_dbg(size > 0);
    o_assert_dbg(!setup.ShouldSetupFromFile());

    Id resId = this->registry.Lookup(setup.Locator);
    if (resId.IsValid()) {
        return resId;
    }
    else {
        resId = this->meshPool.AllocId();
        this->registry.Add(setup.Locator, resId, this->peekLabel());
        mesh& res = this->meshPool.Assign(resId, setup, ResourceState::Setup);
        const ResourceState::Code newState = this->meshFactory.SetupResource(res, data, size);
        o_assert((newState == ResourceState::Valid) || (newState == ResourceState::Failed));
        this->meshPool.UpdateState(resId, newState);
    }
    return resId;
}

//------------------------------------------------------------------------------
template<> Id
gfxResourceContainer::Create(const TextureSetup& setup) {
    o_assert_dbg(this->isValid());
    o_assert_dbg(!setup.ShouldSetupFromFile());

    Id resId = this->registry.Lookup(setup.Locator);
    if (resId.IsValid()) {
        return resId;
    }
    else {
        resId = this->texturePool.AllocId();
        this->registry.Add(setup.Locator, resId, this->peekLabel());
        texture& res = this->texturePool.Assign(resId, setup, ResourceState::Setup);
        const ResourceState::Code newState = this->textureFactory.SetupResource(res);
        o_assert((newState == ResourceState::Valid) || (newState == ResourceState::Failed));
        this->texturePool.UpdateState(resId, newState);
    }
    return resId;
}
    
//------------------------------------------------------------------------------
template<> Id
gfxResourceContainer::Create(const TextureSetup& setup, const void* data, int32 size) {
    o_assert_dbg(this->isValid());
    o_assert_dbg(nullptr != data);
    o_assert_dbg(size > 0);
    o_assert_dbg(!setup.ShouldSetupFromFile());

    Id resId = this->registry.Lookup(setup.Locator);
    if (resId.IsValid()) {
        return resId;
    }
    else {
        resId = this->texturePool.AllocId();
        this->registry.Add(setup.Locator, resId, this->peekLabel());
        texture& res = this->texturePool.Assign(resId, setup, ResourceState::Setup);
        const ResourceState::Code newState = this->textureFactory.SetupResource(res, data, size);
        o_assert((newState == ResourceState::Valid) || (newState == ResourceState::Failed));
        this->texturePool.UpdateState(resId, newState);
    }
    return resId;
}

//------------------------------------------------------------------------------
template<> Id
gfxResourceContainer::prepareAsync(const MeshSetup& setup) {
    o_assert_dbg(this->isValid());
    
    Id resId = this->meshPool.AllocId();
    this->registry.Add(setup.Locator, resId, this->peekLabel());
    this->meshPool.Assign(resId, setup, ResourceState::Pending);
    return resId;
}

//------------------------------------------------------------------------------
template<> ResourceState::Code 
gfxResourceContainer::initAsync(const Id& resId, const MeshSetup& setup, const void* data, int32 size) {
    o_assert_dbg(this->isValid());
    
    // the prepared resource may have been destroyed while it was loading
    if (this->meshPool.Contains(resId)) {
        mesh& res = this->meshPool.Assign(resId, setup, ResourceState::Pending);
        const ResourceState::Code newState = this->meshFactory.SetupResource(res, data, size);
        o_assert((newState == ResourceState::Valid) || (newState == ResourceState::Failed));
        this->meshPool.UpdateState(resId, newState);
        return newState;
    }
    else {
        // the prepared mesh object was destroyed before it was loaded
        o_warn("gfxResourceContainer::initAsync(): resource destroyed before initAsync (type: %d, slot: %d!)\n",
            resId.Type, resId.SlotIndex);
        return ResourceState::InvalidState;
    }
}

//------------------------------------------------------------------------------
template<> Id
gfxResourceContainer::prepareAsync(const TextureSetup& setup) {
    o_assert_dbg(this->isValid());
    
    Id resId = this->texturePool.AllocId();
    this->registry.Add(setup.Locator, resId, this->peekLabel());
    this->texturePool.Assign(resId, setup, ResourceState::Pending);
    return resId;
}

//------------------------------------------------------------------------------
template<> ResourceState::Code 
gfxResourceContainer::initAsync(const Id& resId, const TextureSetup& setup, const void* data, int32 size) {
    o_assert_dbg(this->isValid());
    
    // the prepared resource may have been destroyed while it was loading
    if (this->texturePool.Contains(resId)) {
        texture& res = this->texturePool.Assign(resId, setup, ResourceState::Pending);
        const ResourceState::Code newState = this->textureFactory.SetupResource(res, data, size);
        o_assert((newState == ResourceState::Valid) || (newState == ResourceState::Failed));
        this->texturePool.UpdateState(resId, newState);
        return newState;
    }
    else {
        // the prepared texture object was destroyed before it was loaded
        o_warn("gfxResourceContainer::initAsync(): resource destroyed before initAsync (type: %d, slot: %d!)\n",
            resId.Type, resId.SlotIndex);
        return ResourceState::InvalidState;
    }
}

//------------------------------------------------------------------------------
ResourceState::Code
gfxResourceContainer::failedAsync(const Id& resId) {
    o_assert_dbg(this->isValid());
    
    switch (resId.Type) {
        case GfxResourceType::Mesh:
            // the prepared resource may have been destroyed while it was loading
            if (this->meshPool.Contains(resId)) {
                this->meshPool.UpdateState(resId, ResourceState::Failed);
                return ResourceState::Failed;
            }
            break;
            
        case GfxResourceType::Texture:
            // the prepared resource may have been destroyed while it was loading
            if (this->texturePool.Contains(resId)) {
                this->texturePool.UpdateState(resId, ResourceState::Failed);
                return ResourceState::Failed;
            }
            break;
            
        default:
            o_error("Invalid resource type for async creation!");
            break;
    }
    // fallthrough: resource was already destroyed while still loading
    return ResourceState::InvalidState;
}


//------------------------------------------------------------------------------
template<> Id
gfxResourceContainer::Create(const ShaderSetup& setup) {
    o_assert_dbg(this->isValid());

    Id resId = this->registry.Lookup(setup.Locator);
    if (resId.IsValid()) {
        return resId;
    }
    else {
        resId = this->shaderPool.AllocId();
        this->registry.Add(setup.Locator, resId, this->peekLabel());
        shader& res = this->shaderPool.Assign(resId, setup, ResourceState::Setup);
        const ResourceState::Code newState = this->shaderFactory.SetupResource(res);
        o_assert((newState == ResourceState::Valid) || (newState == ResourceState::Failed));
        this->shaderPool.UpdateState(resId, newState);
    }
    return resId;
}
    
//------------------------------------------------------------------------------
template<> Id
gfxResourceContainer::Create(const ProgramBundleSetup& setup) {
    o_assert_dbg(this->isValid());
    
    Id resId = this->registry.Lookup(setup.Locator);
    if (resId.IsValid()) {
        return resId;
    }
    else {
        resId = this->programBundlePool.AllocId();
        this->registry.Add(setup.Locator, resId, this->peekLabel());
        programBundle& res = this->programBundlePool.Assign(resId, setup, ResourceState::Setup);
        const ResourceState::Code newState = this->programBundleFactory.SetupResource(res);
        o_assert((newState == ResourceState::Valid) || (newState == ResourceState::Failed));
        this->programBundlePool.UpdateState(resId, newState);
    }
    return resId;
}
    
//------------------------------------------------------------------------------
template<> Id
gfxResourceContainer::Create(const DrawStateSetup& setup) {
    o_assert_dbg(this->isValid());
    
    Id resId = this->registry.Lookup(setup.Locator);
    if (resId.IsValid()) {
        return resId;
    }
    else {
        resId = this->drawStatePool.AllocId();
        this->registry.Add(setup.Locator, resId, this->peekLabel());
        drawState& res = this->drawStatePool.Assign(resId, setup, ResourceState::Setup);

        // check if all referenced resources are loaded, if not defer draw state creation
        const ResourceState::Code resState = this->queryDrawStateDependenciesState(&res);
        if (resState == ResourceState::Pending)
        {
            this->pendingDrawStates.Add(resId);
            this->drawStatePool.UpdateState(resId, ResourceState::Pending);
        }
        else {
            const ResourceState::Code newState = this->drawStateFactory.SetupResource(res);
            o_assert((newState == ResourceState::Valid) || (newState == ResourceState::Failed));        
            this->drawStatePool.UpdateState(resId, newState);
        }
    }
    return resId;
}

//------------------------------------------------------------------------------
ResourceState::Code
gfxResourceContainer::queryDrawStateDependenciesState(const drawState* ds) {
    o_assert_dbg(ds);

    // this returns an overall state of the meshes attached to
    // a draw state (failed, pending, valid)
    for (const Id& meshId : ds->Setup.Meshes) {
        if (meshId.IsValid()) {
            const mesh* msh = this->meshPool.Get(meshId);
            if (msh) {
                switch (msh->State) {
                    case ResourceState::Failed:
                        // at least one mesh has failed loading
                        return ResourceState::Failed;
                    case ResourceState::Pending:
                        // at least one mesh has not finished loading
                        return ResourceState::Pending;
                    case ResourceState::Initial:
                    case ResourceState::Setup:
                        // this cannot happen
                        o_error("can't happen!\n");
                        return ResourceState::InvalidState;
                    default:
                        break;
                }
            }
            else {
                // the required mesh no longer exists
                return ResourceState::Failed;
            }
        }
    }
    // fallthough means: all mesh dependencies are valid
    return ResourceState::Valid;
}

//------------------------------------------------------------------------------
void
gfxResourceContainer::handlePendingDrawStates() {

    // this goes through all pending draw states (where meshes are still
    // loading), checks whether meshes have finished loading (or failed to load)
    // and finishes the draw state creation
    for (int i = this->pendingDrawStates.Size() - 1; i >= 0; i--) {
        const Id& resId = this->pendingDrawStates[i];
        o_assert_dbg(resId.IsValid());
        drawState* ds = this->drawStatePool.Get(resId);
        if (ds) {
            const ResourceState::Code state = this->queryDrawStateDependenciesState(ds);
            if (state != ResourceState::Pending) {
                this->pendingDrawStates.Erase(i);
                if (state == ResourceState::Valid) {
                    // all ok, can setup draw state
                    const ResourceState::Code newState = this->drawStateFactory.SetupResource(*ds);
                    o_assert((newState == ResourceState::Valid) || (newState == ResourceState::Failed));        
                    this->drawStatePool.UpdateState(resId, newState);
                }
                else {
                    // oops, a mesh has failed loading, also set draw state to failed
                    o_assert_dbg(state == ResourceState::Failed);
                    this->drawStatePool.UpdateState(resId, state);
                }
            }
        }
        else {
            // the drawState object was destroyed before everything was loaded,
            // still need to remove the entry from the pending array
            o_warn("gfxResourceContainer::handlePendingDrawStates(): drawState destroyed before dependencies loaded!\n");
            this->pendingDrawStates.Erase(i);
        }
    }
}

//------------------------------------------------------------------------------
Id
gfxResourceContainer::Load(const Ptr<ResourceLoader>& loader) {
    o_assert_dbg(this->isValid());

    Id resId = this->registry.Lookup(loader->Locator());
    if (resId.IsValid()) {
        return resId;
    }
    else {
        this->pendingLoaders.Add(loader);
        resId = loader->Start();
        return resId;
    }
}

//------------------------------------------------------------------------------
void
gfxResourceContainer::Destroy(ResourceLabel label) {
    o_assert_dbg(this->isValid());
    
    Array<Id> ids = this->registry.Remove(label);
    for (const Id& id : ids) {
        switch (id.Type) {
            case GfxResourceType::Texture:
            {
                if (ResourceState::Valid == this->texturePool.QueryState(id)) {
                    texture* tex = this->texturePool.Lookup(id);
                    if (tex) {
                        this->textureFactory.DestroyResource(*tex);
                    }
                }
                this->texturePool.Unassign(id);
            }
            break;
                
            case GfxResourceType::Mesh:
            {
                if (ResourceState::Valid == this->meshPool.QueryState(id)) {
                    mesh* msh = this->meshPool.Lookup(id);
                    if (msh) {
                        this->meshFactory.DestroyResource(*msh);
                    }
                }
                this->meshPool.Unassign(id);
            }
            break;
                
            case GfxResourceType::Shader:
            {
                if (ResourceState::Valid == this->shaderPool.QueryState(id)) {
                    shader* shd = this->shaderPool.Lookup(id);
                    if (shd) {
                        this->shaderFactory.DestroyResource(*shd);
                    }
                }
                this->shaderPool.Unassign(id);
            }
            break;
                
            case GfxResourceType::ProgramBundle:
            {
                if (ResourceState::Valid == this->programBundlePool.QueryState(id)) {
                    programBundle* prog = this->programBundlePool.Lookup(id);
                    if (prog) {
                        this->programBundleFactory.DestroyResource(*prog);
                    }
                }
                this->programBundlePool.Unassign(id);
            }
            break;
                
            case GfxResourceType::ConstantBlock:
                o_assert2(false, "FIXME!!!\n");
                break;
                
            case GfxResourceType::DrawState:
            {
                if (ResourceState::Valid == this->drawStatePool.QueryState(id)) {
                    drawState* ds = this->drawStatePool.Lookup(id);
                    if (ds) {
                        this->drawStateFactory.DestroyResource(*ds);
                    }
                }
                this->drawStatePool.Unassign(id);
            }
            break;
                
            default:
                o_assert(false);
                break;
        }
    }
}
    
//------------------------------------------------------------------------------
void
gfxResourceContainer::update() {
    o_assert_dbg(this->isValid());
    
    /// call update method on resource pools (this is cheap)
    this->meshPool.Update();
    this->shaderPool.Update();
    this->programBundlePool.Update();
    this->texturePool.Update();
    this->drawStatePool.Update();

    // trigger loaders, and remove from pending array if finished
    for (int32 i = this->pendingLoaders.Size() - 1; i >= 0; i--) {
        const auto& loader = this->pendingLoaders[i];
        ResourceState::Code state = loader->Continue();
        if (ResourceState::Pending != state) {
            this->pendingLoaders.Erase(i);
        }
    }

    // handle drawstates with pending dependendies
    this->handlePendingDrawStates();
}

//------------------------------------------------------------------------------
ResourceInfo
gfxResourceContainer::QueryResourceInfo(const Id& resId) const {
    o_assert_dbg(this->isValid());
    
    switch (resId.Type) {
        case GfxResourceType::Texture:
            return this->texturePool.QueryResourceInfo(resId);
        case GfxResourceType::Mesh:
            return this->meshPool.QueryResourceInfo(resId);
        case GfxResourceType::Shader:
            return this->shaderPool.QueryResourceInfo(resId);
        case GfxResourceType::ProgramBundle:
            return this->programBundlePool.QueryResourceInfo(resId);
        case GfxResourceType::ConstantBlock:
            o_assert2(false, "FIXME!!!\n");
            return ResourceInfo();
        case GfxResourceType::DrawState:
            return this->drawStatePool.QueryResourceInfo(resId);
        default:
            o_assert(false);
            return ResourceInfo();
    }
}

//------------------------------------------------------------------------------
ResourcePoolInfo
gfxResourceContainer::QueryPoolInfo(GfxResourceType::Code resType) const {
    o_assert_dbg(this->isValid());
    
    switch (resType) {
        case GfxResourceType::Texture:
            return this->texturePool.QueryPoolInfo();
        case GfxResourceType::Mesh:
            return this->meshPool.QueryPoolInfo();
        case GfxResourceType::Shader:
            return this->shaderPool.QueryPoolInfo();
        case GfxResourceType::ProgramBundle:
            return this->programBundlePool.QueryPoolInfo();
        case GfxResourceType::ConstantBlock:
            o_assert2(false, "FIXME!!!\n");
            return ResourcePoolInfo();
        case GfxResourceType::DrawState:
            return this->drawStatePool.QueryPoolInfo();
        default:
            o_assert(false);
            return ResourcePoolInfo();
    }
}

//------------------------------------------------------------------------------
int32
gfxResourceContainer::QueryFreeSlots(GfxResourceType::Code resourceType) const {
    o_assert_dbg(this->isValid());

    switch (resourceType) {
        case GfxResourceType::Texture:
            return this->texturePool.GetNumFreeSlots();
        case GfxResourceType::Mesh:
            return this->meshPool.GetNumFreeSlots();
        case GfxResourceType::Shader:
            return this->shaderPool.GetNumFreeSlots();
        case GfxResourceType::ProgramBundle:
            return this->programBundlePool.GetNumFreeSlots();
        case GfxResourceType::ConstantBlock:
            o_assert2(false, "FIXME!!!\n");
            return 0;
        case GfxResourceType::DrawState:
            return this->drawStatePool.GetNumFreeSlots();
        default:
            o_assert(false);
            return 0;
    }
}

} // namespace _priv
} // namespace Oryol
