#pragma once
//------------------------------------------------------------------------------
/**
    @class Oryol::VertexLayout
    @ingroup Gfx
    @brief describes the data layout of a vertex in a vertex buffer
*/
#include "Core/Assertion.h"
#include "Core/Containers/StaticArray.h"
#include "Gfx/Core/Enums.h"

namespace Oryol {
    
class VertexLayout {
public:
    /// a component in the vertex layout
    class Component {
    public:
        /// default constructor
        Component();
        /// construct from vertex attr and format
        Component(VertexAttr::Code attr, VertexFormat::Code format);
        /// return true if valid (attr and format set)
        bool IsValid() const;
        /// clear the component (unset attr and format)
        void Clear();
        /// get byte size of component
        int32 ByteSize() const;

        VertexAttr::Code Attr;              ///< the vertex attribute (position, normal, ...)
        VertexFormat::Code Format;          ///< the vertex format (float, float2, ...)
    };

    /// constructor
    VertexLayout();
    /// clear the object
    void Clear();
    /// return true if layout is empty
    bool Empty() const;
    /// add a component
    VertexLayout& Add(const Component& comp);
    /// add component by name and format
    VertexLayout& Add(VertexAttr::Code attr, VertexFormat::Code format);
    /// get number of components
    int32 NumComponents() const;
    /// get component at index
    const Component& ComponentAt(int32 index) const;
    /// get component index by vertex attribute, return InvalidIndex if layout doesn't include attr
    int32 ComponentIndexByVertexAttr(VertexAttr::Code attr) const;
    /// get byte size of vertex
    int32 ByteSize() const;
    /// get byte offset of a component
    int32 ComponentByteOffset(int32 componentIndex) const;
    /// test if the layout contains a specific vertex attribute
    bool Contains(VertexAttr::Code attr) const;

    /// maximum number of components in layout
    static const int32 MaxNumComponents = 16;

private:
    int32 numComps;
    int32 byteSize;
    StaticArray<Component, MaxNumComponents> comps;
    StaticArray<int32, MaxNumComponents> byteOffsets;
    StaticArray<int32, VertexAttr::NumVertexAttrs> attrCompIndices;  // maps vertex attributes to component indices
};

//------------------------------------------------------------------------------
inline
VertexLayout::Component::Component() :
Attr(VertexAttr::InvalidVertexAttr),
Format(VertexFormat::InvalidVertexFormat) {
    // empty
}

//------------------------------------------------------------------------------
inline
VertexLayout::Component::Component(VertexAttr::Code attr, VertexFormat::Code fmt) :
Attr(attr),
Format(fmt) {
    // empty
}

//------------------------------------------------------------------------------
inline bool
VertexLayout::Component::IsValid() const {
    return (VertexAttr::InvalidVertexAttr != this->Attr);
}

//------------------------------------------------------------------------------
inline void
VertexLayout::Component::Clear() {
    this->Attr = VertexAttr::InvalidVertexAttr;
    this->Format = VertexFormat::InvalidVertexFormat;
}

//------------------------------------------------------------------------------
inline int32
VertexLayout::Component::ByteSize() const {
    return VertexFormat::ByteSize(this->Format);
}

//------------------------------------------------------------------------------
inline int32
VertexLayout::ComponentIndexByVertexAttr(VertexAttr::Code attr) const {
    return this->attrCompIndices[attr];
}

//------------------------------------------------------------------------------
inline bool
VertexLayout::Contains(VertexAttr::Code attr) const {
    return InvalidIndex != this->ComponentIndexByVertexAttr(attr);
}

//------------------------------------------------------------------------------
inline bool
VertexLayout::Empty() const {
    return 0 == this->numComps;
}

//------------------------------------------------------------------------------
inline int32
VertexLayout::NumComponents() const {
    return this->numComps;
}

//------------------------------------------------------------------------------
inline const class VertexLayout::Component&
VertexLayout::ComponentAt(int32 index) const {
    return this->comps[index];
}

//------------------------------------------------------------------------------
inline int32
VertexLayout::ByteSize() const {
    return this->byteSize;
}

//------------------------------------------------------------------------------
inline int32
VertexLayout::ComponentByteOffset(int32 index) const {
    return this->byteOffsets[index];
}

} // namespace Oryol
