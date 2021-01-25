/*
 * Copyright (c) 2020
 *      Side Effects Software Inc.  All rights reserved.
 *
 * Redistribution and use of Houdini Development Kit samples in source and
 * binary forms, with or without modification, are permitted provided that the
 * following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. The name of Side Effects Software may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE `AS IS' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *----------------------------------------------------------------------------
 * This SOP copies packed primitives from the first input onto the specified
 * points in the second input.
 */

// A .proto.h file is an automatically generated header file based on theDsFile,
// below, to provide SOP_CopyPackedParms, an easy way to access parameter
// values from SOP_CopyPackedVerb::cook with the correct type, and
// SOP_CopyPackedEnums, a namespace containing enum types for any ordinal
// menu parameters.
#include "SOP_CopyPacked.proto.h"

#include <SOP/SOP_Node.h>
#include <SOP/SOP_NodeVerb.h>
#include <GU/GU_Detail.h>
#include <GU/GU_PrimPacked.h>
#include <GOP/GOP_Manager.h>
#include <GA/GA_ATITopology.h>
#include <GA/GA_AttributeRefMap.h>
#include <GA/GA_ElementGroup.h>
#include <GA/GA_Handle.h>
#include <GA/GA_Names.h>
#include <GA/GA_SplittableRange.h>
#include <GA/GA_Types.h>
#include <OP/OP_Operator.h>
#include <OP/OP_OperatorTable.h>
#include <PRM/PRM_TemplateBuilder.h>
#include <UT/UT_Assert.h>
#include <UT/UT_DSOVersion.h>
#include <UT/UT_Interrupt.h>
#include <UT/UT_Lock.h>
#include <UT/UT_ParallelUtil.h>
#include <UT/UT_StringHolder.h>
#include <UT/UT_StringMap.h>
#include <UT/UT_UniquePtr.h>
#include <SYS/SYS_MemoryOrder.h>

namespace HDK_Sample {
    
//******************************************************************************
//*                 Setup                                                      *
//******************************************************************************

class SOP_CopyPackedCache : public SOP_NodeCache
{
public:
    SOP_CopyPackedCache() : SOP_NodeCache()
    {}
};


class SOP_CopyPackedVerb : public SOP_NodeVerb
{
public:
    virtual SOP_NodeParms *allocParms() const { return new SOP_CopyPackedParms(); }
    virtual SOP_NodeCache *allocCache() const { return new SOP_CopyPackedCache(); }
    virtual UT_StringHolder name() const { return theSOPTypeName; }

    virtual CookMode cookMode(const SOP_NodeParms *parms) const { return COOK_GENERIC; }

    virtual void cook(const CookParms &cookparms) const;

    /// This is the internal name of the SOP type.
    /// It isn't allowed to be the same as any other SOP's type name.
    static const UT_StringHolder theSOPTypeName;

    /// This static data member automatically registers
    /// this verb class at library load time.
    static const SOP_NodeVerb::Register<SOP_CopyPackedVerb> theVerb;

    /// This is the parameter interface string, below.
    static const char *const theDsFile;
};

// The static member variable definitions have to be outside the class definition.
// The declarations are inside the class.
const UT_StringHolder SOP_CopyPackedVerb::theSOPTypeName("hdk_copypacked"_sh);
const SOP_NodeVerb::Register<SOP_CopyPackedVerb> SOP_CopyPackedVerb::theVerb;

/// This is the SOP class definition.
class SOP_CopyPacked : public SOP_Node
{
public:
    static inline PRM_Template *buildTemplates(const UT_StringHolder &filename, const char *ds_file)
    {
        static PRM_TemplateBuilder templ(filename, ds_file);
        if (templ.justBuilt())
        {
            templ.setChoiceListPtr("pointgroup", &SOP_Node::pointGroupMenu);
        }
        return templ.templates();
    }
    static OP_Node *myConstructor(OP_Network *net, const char *name, OP_Operator *op)
    {
        return new SOP_CopyPacked(net, name, op);
    }

protected:
    virtual const SOP_NodeVerb *cookVerb() const override
    {
        return SOP_CopyPackedVerb::theVerb.get();
    }

    SOP_CopyPacked(OP_Network *net, const char *name, OP_Operator *op)
        : SOP_Node(net, name, op)
    {
        // All verb SOPs must manage data IDs, to track what's changed
        // from cook to cook.
        mySopFlags.setManagesDataIDs(true);
    }

    virtual ~SOP_CopyPacked() {}

    /// Since this SOP implements a verb, cookMySop just delegates to the verb.
    virtual OP_ERROR cookMySop(OP_Context &context) override
    {
        return cookMyselfAsVerb(context);
    }

    /// These are the labels that appear when hovering over the inputs.
    virtual const char *inputLabel(unsigned idx) const override
    {
        switch (idx)
        {
            case 0:     return "Packed Primitives";
            case 1:     return "Points";
            default:    return "Invalid Source";
        }
    }

    /// This just indicates whether an input wire gets drawn with a dotted line
    /// in the network editor.  If something is usually copied directly
    /// into the output, a solid line (false) is used, but this SOP very often
    /// doesn't do that for either input.
    virtual int isRefInput(unsigned i) const override
    {
        // First or second input both use dotted lines
        return (i == 0 || i == 1);
    }
};
} // End HDK_Sample namespace

/// newSopOperator is the hook that Houdini grabs from this dll
/// and invokes to register the SOP.  In this case, we add ourselves
/// to the specified operator table.
void
newSopOperator(OP_OperatorTable *table)
{
    using namespace HDK_Sample;
    table->addOperator(new OP_Operator(
        SOP_CopyPackedVerb::theSOPTypeName, // Internal name
        "Copy Packed",                      // UI name
        SOP_CopyPacked::myConstructor,      // How to build the SOP
        SOP_CopyPacked::buildTemplates(     // My parameters
            "SOP_CopyPacked.C"_sh,
            SOP_CopyPackedVerb::theDsFile),
        2, 2,                               // Min, Max # of inputs
        nullptr,                            // Custom local variables (none)
        0));                                // No special flags (OP_FLAG_UNORDERED to have a multi-input like Merge SOP)
}

namespace HDK_Sample {

//******************************************************************************
//*                 Parameters                                                 *
//******************************************************************************

/// This is a multi-line raw string specifying the parameter interface for this SOP.
const char *const SOP_CopyPackedVerb::theDsFile = R"THEDSFILE(
{
    name        parameters
    parm {
        name    "pointgroup"
        cppname "PointGroup"
        label   "Point Group"
        type    string
        default { "" }
        parmtag { "script_action" "import soputils\nkwargs['geometrytype'] = (hou.geometryType.Points,)\nkwargs['inputindex'] = 1\nsoputils.selectGroupParm(kwargs)" }
        parmtag { "script_action_help" "Select geometry from an available viewport.\nShift-click to turn on Select Groups." }
        parmtag { "script_action_icon" "BUTTONS_reselect" }
        parmtag { "sop_input" "1" }
    }
}
)THEDSFILE";

//******************************************************************************
//*                 Cooking                                                    *
//******************************************************************************

template<typename FUNCTOR>
static void
copyPrimitiveData(
    GEO_Detail *output_geo,
    const GEO_Detail *packed_prims_input,
    GA_PrimitiveGroupUPtr &bad_prim_group_deleter,
    const GA_AttributeRefMap &primitive_attribs,
    const GA_AttributeRefMap &vertex_attribs,
    FUNCTOR &&pt_to_prim_functor)
{
    GA_PrimitiveGroup *volatile bad_prim_group = nullptr;
    UT_Lock bad_prim_group_lock;

    // NOTE: We're using a GA_SplittableRange over the destination primitives so that we can
    //       safely write to unordered primitive groups in parallel without locking around the writes.
    UTparallelForLightItems(GA_SplittableRange(output_geo->getPrimitiveRange()),
        [output_geo, packed_prims_input,
        &bad_prim_group, &bad_prim_group_deleter, &bad_prim_group_lock,
        &primitive_attribs, &vertex_attribs, &pt_to_prim_functor](const GA_SplittableRange &r) {
        GA_Offset start;
        GA_Offset end;
        for (GA_Iterator it(r); it.blockAdvance(start, end); ) {
            GA_Index i = output_geo->primitiveIndex(start);
            for (GA_Offset dest_primoff = start; dest_primoff < end; ++dest_primoff, ++i) {
                GA_Primitive *dest_prim = output_geo->getPrimitive(dest_primoff);
                // There can only be packed primitives here, so this should be safe.
                GEO_PrimPacked *dest_packed_prim = UTverify_cast<GEO_PrimPacked *>(dest_prim);
                GA_Offset dest_ptoff = output_geo->pointOffset(i);
                GA_Offset source_primoff = pt_to_prim_functor(dest_ptoff);
                if (GAisValid(source_primoff)) {
                    const GA_Primitive *source_prim = packed_prims_input->getPrimitive(source_primoff);
                    const GEO_PrimPacked *source_packed_prim = UTverify_cast<const GEO_PrimPacked *>(source_prim);
                    dest_packed_prim->copyMemberDataFrom(*source_packed_prim);

                    if (primitive_attribs.entries()) {
                        primitive_attribs.copyValue(GA_ATTRIB_PRIMITIVE, dest_primoff, GA_ATTRIB_PRIMITIVE, source_primoff);
                    }
                    if (vertex_attribs.entries()) {
                        vertex_attribs.copyValue(GA_ATTRIB_VERTEX, dest_prim->getVertexOffset(0), GA_ATTRIB_VERTEX, source_prim->getVertexOffset(0));
                    }
                }
                else {
                    if (!bad_prim_group) {
                        UT_Lock::Scope lock(bad_prim_group_lock);
                        if (!bad_prim_group) {
                            // Making a detached primitive group (owned by us, not output_geo).
                            bad_prim_group_deleter = UTmakeUnique<GA_PrimitiveGroup>(*output_geo);
                            SYSstoreFence();
                            bad_prim_group = bad_prim_group_deleter.get();
                        }
                    }
                    bad_prim_group->addOffset(dest_primoff);
                }
            }
        }
    });
}

/// This is the function that does the actual work.
void SOP_CopyPackedVerb::cook(const CookParms &cookparms) const
{
    // This gives easy access to all of the current parameter values
    auto &&sopparms = cookparms.parms<SOP_CopyPackedParms>();
    auto sopcache = (SOP_CopyPackedCache *)cookparms.cache();

    // The output detail
    GEO_Detail *output_geo = cookparms.gdh().gdpNC();

    const GEO_Detail *const packed_prims_input = cookparms.inputGeo(0);
    const GEO_Detail *const points_input = cookparms.inputGeo(1);

    // GOP_Manager will own any temporary groups it creates
    // and automatically destroy them when it goes out of scope.
    GOP_Manager group_manager;
    const GA_PointGroup *input_point_group = nullptr;
    if (sopparms.getPointGroup().isstring()) {
        // Parse point group on points_input detail.
        bool success;
        input_point_group = group_manager.parsePointDetached(
            sopparms.getPointGroup().c_str(),
            points_input, true, success);
    }
    if (input_point_group) {
        output_geo->mergePoints(*points_input, input_point_group, true, false);
    }
    else {
        output_geo->replaceWithPoints(*points_input);
    }

    if (packed_prims_input->getNumPrimitives() == 0) {
        return;
    }

    GA_PrimitiveTypeId primitive_type_id(packed_prims_input->getPrimitiveTypeId(packed_prims_input->primitiveOffset(GA_Index(0))));
    if (!GU_PrimPacked::isPackedPrimitive(primitive_type_id)) {
        cookparms.sopAddError(SOP_MESSAGE, "All input primitives for copying must be packed primitives, at the moment.");
        return;
    }
    if (packed_prims_input->countPrimitiveType(primitive_type_id) != packed_prims_input->getNumPrimitives()) {
        cookparms.sopAddError(SOP_MESSAGE, "All input packed primitives must be the same primitive type, at the moment.");
        return;
    }

    GA_Size npoints = output_geo->getNumPoints();
    GA_Offset start_primoff = output_geo->appendPrimitiveBlock(primitive_type_id, npoints);
    GA_Offset start_vtxoff = output_geo->appendVertexBlock(npoints);

    output_geo->bumpDataIdsForAddOrRemove(false, true, true);

    // Initialize the vertex lists in parallel
    UTparallelForLightItems(UT_BlockedRange<exint>(0, npoints), [output_geo, start_vtxoff, start_primoff](const UT_BlockedRange<exint> &r) {
        for (exint i = r.begin(), n = r.end(); i < n; ++i) {
            GA_Primitive *prim = output_geo->getPrimitive(start_primoff+i);
            // There can only be packed primitives here, so this should be safe.
            GEO_PrimPacked *packed_prim = UTverify_cast<GEO_PrimPacked *>(prim);
            packed_prim->assignVertex(start_vtxoff+i, false);
        }
    });

    // Initialize the vertex-to-primitive topology attribute in parallel
    GA_ATITopology *vtx_to_prim = output_geo->getTopology().getPrimitiveRef();
    vtx_to_prim->hardenAllPages();
    UTparallelForLightItems(UT_BlockedRange<exint>(0, npoints), [vtx_to_prim, start_vtxoff, start_primoff](const UT_BlockedRange<exint> &r) {
        for (exint i = r.begin(), n = r.end(); i < n; ++i) {
            vtx_to_prim->setLink(start_vtxoff+i, start_primoff+i);
        }
    });

    // Initialize the vertex-to-point and point-to-vertex topology attributes in parallel.
    // NOTE: vertex-to-next-vertex and vertex-to-prev-vertex topology attributes
    //       don't need to be updated in this case, since their default GA_INVALID_OFFSET (-1)
    //       is what we want.
    GA_ATITopology *vtx_to_pt = output_geo->getTopology().getPointRef();
    vtx_to_pt->hardenAllPages();
    GA_ATITopology *pt_to_vtx = output_geo->getTopology().getVertexRef();
    const GA_IndexMap &pt_map = output_geo->getPointMap();
    if (pt_map.isTrivialMap()) {
        UTparallelForLightItems(UT_BlockedRange<exint>(0, npoints), [vtx_to_pt, start_vtxoff](const UT_BlockedRange<exint> &r) {
            for (exint i = r.begin(), n = r.end(); i < n; ++i) {
                vtx_to_pt->setLink(start_vtxoff+i, GA_Offset(i));
            }
        });
        pt_to_vtx->hardenAllPages();
        UTparallelForLightItems(UT_BlockedRange<exint>(0, npoints), [pt_to_vtx, start_vtxoff](const UT_BlockedRange<exint> &r) {
            for (exint i = r.begin(), n = r.end(); i < n; ++i) {
                pt_to_vtx->setLink(GA_Offset(i), start_vtxoff+i);
            }
        });
    }
    else {
        UTparallelForLightItems(UT_BlockedRange<exint>(0, npoints), [vtx_to_pt, start_vtxoff, &pt_map](const UT_BlockedRange<exint> &r) {
            for (exint i = r.begin(), n = r.end(); i < n; ++i) {
                vtx_to_pt->setLink(start_vtxoff+i, pt_map.offsetFromIndex(GA_Index(i)));
            }
        });
        pt_to_vtx->hardenAllPages();
        UTparallelForLightItems(GA_SplittableRange(output_geo->getPointRange()), [output_geo, pt_to_vtx, start_vtxoff](const GA_SplittableRange &r) {
            GA_Offset start;
            GA_Offset end;
            for (GA_Iterator it(r); it.blockAdvance(start, end); ) {
                exint i = (exint)output_geo->pointIndex(start);
                for (GA_Offset ptoff = start; ptoff < end; ++ptoff, ++i) {
                    pt_to_vtx->setLink(ptoff, start_vtxoff+i);
                }
            }
        });
    }

    GA_AttributeRefMap primitive_attribs(*output_geo, packed_prims_input);
    primitive_attribs.appendAndCreateAllSource(GA_ATTRIB_PRIMITIVE, GA_ATTRIB_PRIMITIVE, "*");

    GA_AttributeRefMap vertex_attribs(*output_geo, packed_prims_input);
    vertex_attribs.appendAndCreateAllSource(GA_ATTRIB_VERTEX, GA_ATTRIB_VERTEX, "*");

    // Below, we're relying on that the vertex attribute pages and primitive attribute pages
    // line up, in case there are vertex groups we're copying, since it's not safe
    // for multiple threads to write to the same page of the same group at the same time,
    // even if we were hardening the pages.  Luckily, the vertex and primitive attribute pages
    // should line up in this case, since there's exactly one vertex per primitive here and
    // they should both start at GA_Offset(0) and the offsets of each are in a contiguous block.
    UT_ASSERT_MSG(start_vtxoff == GA_Offset(0) && start_primoff == GA_Offset(0),
        "start_vtxoff and start_primoff should both be zero, since we appended a block to an empty detail.");

    // This will automatically delete the detached primitive group
    // (if one is created) when this goes out of scope.
    GA_PrimitiveGroupUPtr bad_prim_group(nullptr);

    // It might seem like overkill to read the id attribute as int64, since
    // primitive numbers usually don't exceed 2 billion, but if the IDs are
    // incremented over the course of a simulation, or anything like that,
    // they could conceivably need integers greater than 2 billion.
    // Most of the time, the attribute will be int32, and its values will be
    // automatically converted to int64 upon reading, which is fine.
    GA_ROHandleID pt_id_attrib(output_geo->findPointAttribute(GA_Names::id));
    if (pt_id_attrib.isValid()) {
        GA_ROHandleID prim_id_attrib(packed_prims_input->findPrimitiveAttribute(GA_Names::id));
        GA_AttributeOwner prim_id_owner = GA_ATTRIB_PRIMITIVE;
        if (!prim_id_attrib.isValid()) {
            prim_id_attrib.bind(packed_prims_input->findVertexAttribute(GA_Names::id));
            prim_id_owner = GA_ATTRIB_VERTEX;
            if (!prim_id_attrib.isValid()) {
                prim_id_attrib.bind(packed_prims_input->findPointAttribute(GA_Names::id));
                prim_id_owner = GA_ATTRIB_POINT;
            }
        }

        if (prim_id_attrib.isValid()) {
            // Record the mapping from ID to packed primitive offset
            UT::ArrayMap<exint,GA_Offset> id_to_primoff;
            GA_Offset start;
            GA_Offset end;
            for (GA_Iterator it(packed_prims_input->getPrimitiveRange()); it.blockAdvance(start, end); ) {
                for (GA_Offset primoff = start; primoff < end; ++primoff) {
                    GA_Offset attriboff = primoff;
                    if (prim_id_owner != GA_ATTRIB_PRIMITIVE) {
                        // NOTE: Packed primitives in the input should always have one vertex,
                        //       so this should be safe.
                        attriboff = packed_prims_input->getPrimitiveVertexOffset(primoff, 0);
                        if (prim_id_owner == GA_ATTRIB_POINT) {
                            attriboff = packed_prims_input->vertexPoint(attriboff);
                        }
                    }
                    exint id = prim_id_attrib.get(attriboff);
                    if (UT::ArrayMap<exint,GA_Offset>::clearer_type::isClear(id)) {
                        // UT::ArrayMap<exint,...> doesn't support
                        // 0x8000000000000000LL as a key.  Hopefully that's okay.
                        continue;
                    }

                    // NOTE: This will not overwrite if another primitive with the same ID has already been inserted,
                    //       so it only keeps the first match.
                    id_to_primoff.insert(id, primoff);
                }
            }

            copyPrimitiveData(output_geo, packed_prims_input, bad_prim_group, primitive_attribs, vertex_attribs,
                [&pt_id_attrib, &id_to_primoff](GA_Offset dest_ptoff) -> GA_Offset {
                const exint id = pt_id_attrib.get(dest_ptoff);
                if (UT::ArrayMap<exint,GA_Offset>::clearer_type::isClear(id)) {
                    // UT::ArrayMap<exint,...> doesn't support
                    // 0x8000000000000000LL as a key.  Hopefully that's okay.
                    return GA_INVALID_OFFSET;
                }
                auto &&it = id_to_primoff.find(id);
                if (it != id_to_primoff.end()) {
                    return it->second;
                }
                return GA_INVALID_OFFSET;
            });
        }
        else {
            // There's no primitive id attribute, so id represents the primitive number (index).
            copyPrimitiveData(output_geo, packed_prims_input, bad_prim_group, primitive_attribs, vertex_attribs,
                [&pt_id_attrib, packed_prims_input](GA_Offset dest_ptoff) -> GA_Offset {
                const GA_Index source_primind(pt_id_attrib.get(dest_ptoff));
                if (GAisValid(source_primind) && source_primind < packed_prims_input->getNumPrimitives()) {
                    // Primitive number is in range
                    return packed_prims_input->primitiveOffset(source_primind);
                }
                return GA_INVALID_OFFSET;
            });
        }
    }
    else {
        GA_ROHandleS pt_name_attrib(output_geo->findPointAttribute(GA_Names::name));
        GA_ROHandleS prim_name_attrib(output_geo->findPrimitiveAttribute(GA_Names::name));
        GA_AttributeOwner prim_name_owner = GA_ATTRIB_PRIMITIVE;
        if (!prim_name_attrib.isValid()) {
            prim_name_attrib.bind(packed_prims_input->findVertexAttribute(GA_Names::name));
            prim_name_owner = GA_ATTRIB_VERTEX;
            if (!prim_name_attrib.isValid()) {
                prim_name_attrib.bind(packed_prims_input->findPointAttribute(GA_Names::name));
                prim_name_owner = GA_ATTRIB_POINT;
            }
        }
        if (pt_name_attrib.isValid() && prim_name_attrib.isValid()) {
            // Record the mapping from name to packed primitive offset
            UT_ArrayStringMap<GA_Offset> name_to_primoff;
            GA_Offset start;
            GA_Offset end;
            for (GA_Iterator it(packed_prims_input->getPrimitiveRange()); it.blockAdvance(start, end); ) {
                for (GA_Offset primoff = start; primoff < end; ++primoff) {
                    GA_Offset attriboff = primoff;
                    if (prim_name_owner != GA_ATTRIB_PRIMITIVE) {
                        // NOTE: Packed primitives in the input should always have one vertex,
                        //       so this should be safe.
                        attriboff = packed_prims_input->getPrimitiveVertexOffset(primoff, 0);
                        if (prim_name_owner == GA_ATTRIB_POINT) {
                            attriboff = packed_prims_input->vertexPoint(attriboff);
                        }
                    }
                    const UT_StringHolder &name = prim_name_attrib.get(attriboff);
                    if (!name.isstring()) {
                        // UT_ArrayStringMap doesn't support "" as a key in
                        // Houdini 16.5 and earlier, and if
                        // "" appears in the matching attribute, the user is
                        // probably indicating that they don't want to copy
                        // anything there, so it should be okay.
                        continue;
                    }

                    // NOTE: This will not overwrite if another primitive with the same name has already been inserted,
                    //       so it only keeps the first match.
                    name_to_primoff.insert(name, primoff);
                }
            }

            copyPrimitiveData(output_geo, packed_prims_input, bad_prim_group, primitive_attribs, vertex_attribs,
                [&pt_name_attrib, &name_to_primoff](GA_Offset dest_ptoff) -> GA_Offset {
                const UT_StringHolder &name = pt_name_attrib.get(dest_ptoff);
                if (!name.isstring()) {
                    // UT_ArrayStringMap doesn't support "" in 16.5 and earlier,
                    // and it probably means that we're not trying to match
                    // anything, anyway.
                    return GA_INVALID_OFFSET;
                }
                auto &&it = name_to_primoff.find(name);
                if (it != name_to_primoff.end()) {
                    return it->second;
                }
                return GA_INVALID_OFFSET;
            });

        }
        else {
            output_geo->clearAndDestroy();
            cookparms.sopAddError(SOP_MESSAGE, "Either the points must have an id attribute, or both the points and the packed primitives must have a name attribute.");
            return;
        }
    }

    if (bad_prim_group) {
        // If there were any points that didn't match any source primitives,
        // delete the incompletely initialized primitives we've made.
        output_geo->deletePrimitives(*bad_prim_group);
    }
}

} // End HDK_Sample namespace
