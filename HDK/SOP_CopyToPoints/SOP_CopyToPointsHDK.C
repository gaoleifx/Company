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
 * This SOP copies geometry from the first input onto points in the second
 * input, caching information to speed up subsequent cooks where the output
 * is cached and the input geometry hasn't changed topology.
 */

// A .proto.h file is an automatically generated header file based on theDsFile,
// below, to provide SOP_CopyToPointsHDKParms, an easy way to access parameter
// values from SOP_CopyToPointsHDKVerb::cook with the correct type, and
// SOP_CopyToPointsHDKEnums, a namespace containing enum types for any ordinal
// menu parameters.
#include "SOP_CopyToPointsHDK.proto.h"

#include "GU_Copy2.h"
#include "GEO_BuildPrimitives.h"

#include <SOP/SOP_Node.h>
#include <SOP/SOP_NodeVerb.h>
#include <GU/GU_PackedGeometry.h>
#include <GU/GU_PrimPacked.h>
#include <GEO/GEO_ParallelWiringUtil.h>
#include <GA/GA_Primitive.h>
#include <GA/GA_Range.h>
#include <GA/GA_SplittableRange.h>
#include <OP/OP_AutoLockInputs.h>
#include <PRM/PRM_ChoiceList.h>
#include <PRM/PRM_TemplateBuilder.h>
#include <UT/UT_ArrayMap.h>
#include <UT/UT_ArrayStringMap.h>
#include <UT/UT_DSOVersion.h>
#include <UT/UT_PageArrayImpl.h>
#include <UT/UT_ParallelUtil.h>
#include <UT/UT_StringHolder.h>
#include <UT/UT_UndoManager.h>

#include <utility> // For std::pair

namespace HDK_Sample {

using namespace GU_Copy;

class SOP_CopyToPointsHDKCache : public SOP_NodeCache, public GU_CopyToPointsCache
{
    ~SOP_CopyToPointsHDKCache() override {}
    // Contents in GU_CopyToPointsCache
};

class SOP_CopyToPointsHDKVerb : public SOP_NodeVerb
{
public:
    SOP_CopyToPointsHDKVerb() {}
    ~SOP_CopyToPointsHDKVerb() override {}

    SOP_NodeParms *allocParms() const override { return new SOP_CopyToPointsHDKParms(); }
    SOP_NodeCache *allocCache() const override { return new SOP_CopyToPointsHDKCache(); }
    UT_StringHolder name() const override { return theSOPTypeName; }

    CookMode cookMode(const SOP_NodeParms *parms) const override
    { return COOK_GENERIC; }

    void cook(const CookParms &cookparms) const override;

    /// This is the internal name of the SOP type.
    /// It isn't allowed to be the same as any other SOP's type name.
    static const UT_StringHolder theSOPTypeName;

    /// This static data member automatically registers
    /// this verb class at library load time.
    static const SOP_NodeVerb::Register<SOP_CopyToPointsHDKVerb> theVerb;

    /// This is the parameter interface string, below.
    static const char *const theDsFile;
};

// The static member variable definitions have to be outside the class definition.
// The declarations are inside the class.
// Add "::2.0" to the type name to make a version 2.0 of the node without displacing
// the original.
const UT_StringHolder SOP_CopyToPointsHDKVerb::theSOPTypeName("hdk_copytopoints"_sh);
const SOP_NodeVerb::Register<SOP_CopyToPointsHDKVerb> SOP_CopyToPointsHDKVerb::theVerb;

//******************************************************************************
//*                 Parameters                                                 *
//******************************************************************************

/// This is a multi-line raw string specifying the parameter interface for this SOP.
const char *const SOP_CopyToPointsHDKVerb::theDsFile = R"THEDSFILE(
{
    name        parameters
    parm {
        name    "sourcegroup"
        cppname "SourceGroup"
        label   "Source Group"
        type    string
        default { "" }
        parmtag { "script_action" "import soputils\nkwargs['geometrytype'] = kwargs['node'].parmTuple('sourcegrouptype')\nkwargs['inputindex'] = 0\nsoputils.selectGroupParm(kwargs)" }
        parmtag { "script_action_help" "Select geometry from an available viewport.\nShift-click to turn on Select Groups." }
        parmtag { "script_action_icon" "BUTTONS_reselect" }
        parmtag { "sop_input" "0" }
    }
    parm {
        name    "sourcegrouptype"
        cppname "SourceGroupType"
        label   "Source Group Type"
        type    ordinal
        default { "0" }
        menu {
            "guess"     "Guess from Group"
            "prims"     "Primitives"
            "points"    "Points"
        }
    }
    parm {
        name    "targetgroup"
        cppname "TargetGroup"
        label   "Target Points"
        type    string
        default { "" }
        parmtag { "script_action" "import soputils\nkwargs['geometrytype'] = (hou.geometryType.Points,)\nkwargs['inputindex'] = 1\nsoputils.selectGroupParm(kwargs)" }
        parmtag { "script_action_help" "Select geometry from an available viewport.\nShift-click to turn on Select Groups." }
        parmtag { "script_action_icon" "BUTTONS_reselect" }
        parmtag { "sop_input" "1" }
    }
    parm {
        name        "useidattrib"
        cppname     "UseIDAttrib"
        label       "Piece Attribute"
        type        toggle
        joinnext
        nolabel
        default     { "0" }
    }
    parm {
        name        "idattrib"
        cppname     "IDAttrib"
        label       "Piece Attribute"
        type        string
        default     { "variant" }
        parmtag     { "sop_input" "1" }
        disablewhen "{ useidattrib == 0 }"
    }
    parm {
        name    "pack"
        label   "Pack and Instance"
        type    toggle
        default { "off" }
    }
    parm {
        name    "pivot"
        label   "Pivot Location"
        type    ordinal
        default { "centroid" }
        hidewhen "{ pack == 0 }"
        menu {
            "origin"    "Origin"
            "centroid"  "Centroid"
        }
    }
    parm {
        name    "viewportlod"
        cppname "ViewportLOD"
        label   "Display As"
        type    ordinal
        default { "full" }
        hidewhen "{ pack == 0 }"
        menu {
            "full"      "Full Geometry"
            "points"    "Point Cloud"
            "box"       "Bounding Box"
            "centroid"  "Centroid"
            "hidden"    "Hidden"
        }
    }
    parm {
        name    "transform"
        label   "Transform Using Target Point Orientations"
        type    toggle
        default { "on" }
    }
    parm {
        name    "useimplicitn"
        cppname "UseImplicitN"
        label   "Transform Using Implicit Target Point Normals If No Point N Attribute"
        type    toggle
        default { "on" }
        disablewhen "{ transform == 0 }"
    }
    parm {
        name    "resettargetattribs"
        label   "Reset Attributes from Target"
        type    button
        default { "0" }
    }
    multiparm {
        name    "targetattribs"
        cppname "TargetAttribs"
        label   "Attributes from Target"
        default 0

        parm {
            name        "useapply#"
            label       "Apply Attributes"
            type        toggle
            joinnext
            nolabel
            default     { "1" }
        }
        parm {
            name    "applyto#"
            label   "Apply to"
            type    ordinal
            joinnext
            default { "0" }
            menu {
                "points" "Points"
                "verts"  "Vertices"
                "prims"  "Primitives"
            }
        }
        parm {
            name    "applymethod#"
            label   "by"
            type    ordinal
            joinnext
            default { "0" }
            menu {
                "copy"  "Copying"
                "none"  "Nothing"
                "mult"  "Multiplying"
                "add"   "Adding"
                "sub"   "Subtracting"
            }
        }
        parm {
            name    "applyattribs#"
            label   "Attributes"
            type    string
            parmtag { "sop_input" "1" }
            default { "" }
        }
    }
}
)THEDSFILE";

namespace {
/// Keep this enum in sync with the menu in the "applymethod#" parameter.
using sop_AttribCombineMethod = GU_CopyToPointsCache::AttribCombineMethod;

/// Should take the menu out of the DS file so we can have one
/// copy of this table
static const char *const theViewportLODNames[5] =
{
    "full",
    "points",
    "box",
    "centroid",
    "hidden"
};
}

SYS_FORCE_INLINE
static GEO_ViewportLOD sopViewportLODFromParam(
    const SOP_CopyToPointsHDKEnums::ViewportLOD param_lod)
{
    GEO_ViewportLOD lod = GEOviewportLOD( theViewportLODNames[ int(param_lod) ] );
    return lod;
}
SYS_FORCE_INLINE
static GU_CopyToPointsCache::PackedPivot sopCachePivotType(
    const SOP_CopyToPointsHDKEnums::Pivot pivot_param)
{
    using namespace SOP_CopyToPointsHDKEnums;

    switch (pivot_param)
    {
        case Pivot::ORIGIN:
            return GU_CopyToPointsCache::PackedPivot::ORIGIN;
        case Pivot::CENTROID:
            return GU_CopyToPointsCache::PackedPivot::CENTROID;
    }
    UT_ASSERT_MSG_P(0, "Unhandled pivot type!");
    return GU_CopyToPointsCache::PackedPivot::CENTROID;
}

class SOP_CopyToPointsHDK final : public SOP_Node
{
public:
    SOP_CopyToPointsHDK(OP_Network *net, const char *name, OP_Operator *entry);
    ~SOP_CopyToPointsHDK() override;

    OP_ERROR cookInputGroups(OP_Context &context, int alone = 0) override;

    static OP_Node *myConstructor(OP_Network *net, const char *name, OP_Operator *entry);

    static PRM_Template *buildTemplates();
    const SOP_NodeVerb *cookVerb() const override;

    void resetTargetAttribs();
protected:
    OP_ERROR cookMySop(OP_Context &context) override;
    const char *inputLabel(unsigned idx) const override;
    int isRefInput(unsigned int i) const;
};

static int
sopResetTargetAttribsWrapper(
    void *data, int index, fpreal t, const PRM_Template *)
{
    UT_AutoUndoBlock u("Reset Target Point Attributes", ANYLEVEL);
    SOP_CopyToPointsHDK *me = static_cast<SOP_CopyToPointsHDK *>(data);

    if (!me)
        return 0;

    me->resetTargetAttribs();
    return 1;
}
void
SOP_CopyToPointsHDK::resetTargetAttribs()
{
    setInt("targetattribs", 0, fpreal(0.0), 3);
    int multiparm_index = 1;
    setIntInst(1, "useapply#", &multiparm_index, 0, fpreal(0.0));
    setIntInst(0, "applyto#", &multiparm_index, 0, fpreal(0.0));
    setIntInst(int(sop_AttribCombineMethod::COPY), "applymethod#", &multiparm_index, 0, fpreal(0.0));
    setStringInst("*,^v,^Alpha,^N,^up,^pscale,^scale,^orient,^rot,^pivot,^trans,^transform", CH_StringMeaning::CH_STRING_LITERAL, "applyattribs#", &multiparm_index, 0, fpreal(0.0));
    ++multiparm_index;
    setIntInst(1, "useapply#", &multiparm_index, 0, fpreal(0.0));
    setIntInst(0, "applyto#", &multiparm_index, 0, fpreal(0.0));
    setIntInst(int(sop_AttribCombineMethod::MULTIPLY), "applymethod#", &multiparm_index, 0, fpreal(0.0));
    setStringInst("Alpha", CH_StringMeaning::CH_STRING_LITERAL, "applyattribs#", &multiparm_index, 0, fpreal(0.0));
    ++multiparm_index;
    setIntInst(1, "useapply#", &multiparm_index, 0, fpreal(0.0));
    setIntInst(0, "applyto#", &multiparm_index, 0, fpreal(0.0));
    setIntInst(int(sop_AttribCombineMethod::ADD), "applymethod#", &multiparm_index, 0, fpreal(0.0));
    setStringInst("v", CH_StringMeaning::CH_STRING_LITERAL, "applyattribs#", &multiparm_index, 0, fpreal(0.0));
}

static bool
sopApproveStringIntAttribs(const GA_Attribute *attrib, void*)
{
    if (GA_ATIString::isType(attrib))
        return true;

    const GA_ATINumeric *numeric = GA_ATINumeric::cast(attrib);
    if (!numeric)
        return false;
    return (numeric->getStorageClass() == GA_STORECLASS_INT);
}

static void
sopBuildStringIntPointAttribMenu(
    void *data, PRM_Name *menu_entries, int menu_size,
    const PRM_SpareData *, const PRM_Parm *)
{
    SOP_CopyToPointsHDK *sop = (SOP_CopyToPointsHDK *)data;
    if (!sop || !sop->getInput(1))
        return;

    sop->fillAttribNameMenu(menu_entries, menu_size,
        GA_ATTRIB_POINT, 1, sopApproveStringIntAttribs);
}

static void
sopBuildTargetPointAttribMenu(
    void *data, PRM_Name *menu_entries, int menu_size,
    const PRM_SpareData *, const PRM_Parm *)
{
    SOP_CopyToPointsHDK *sop = (SOP_CopyToPointsHDK *)data;
    if (!sop || !sop->getInput(1))
        return;

    sop->fillAttribNameMenu(menu_entries, menu_size,
        GA_ATTRIB_POINT, 1);
}

static PRM_ChoiceList sopStringIntPointAttribReplaceMenu(
    (PRM_ChoiceListType)(PRM_CHOICELIST_REPLACE),
    sopBuildStringIntPointAttribMenu);

static PRM_ChoiceList sopTargetPointAttribMenu(
    (PRM_ChoiceListType)(PRM_CHOICELIST_TOGGLE),
    sopBuildTargetPointAttribMenu);

PRM_Template*
SOP_CopyToPointsHDK::buildTemplates()
{
    static PRM_TemplateBuilder templ("SOP_CopyToPointsHDK.C"_sh, SOP_CopyToPointsHDKVerb::theDsFile);
    if (templ.justBuilt())
    {
        templ.setChoiceListPtr("sourcegroup", &SOP_Node::primGroupMenu);
        templ.setChoiceListPtr("targetgroup", &SOP_Node::pointGroupMenu);
        templ.setChoiceListPtr("idattrib", &sopStringIntPointAttribReplaceMenu);
        templ.setChoiceListPtr("applyattribs#",&sopTargetPointAttribMenu);
        templ.setCallback("resettargetattribs", &sopResetTargetAttribsWrapper);
    }
    return templ.templates();
}

OP_Node *
SOP_CopyToPointsHDK::myConstructor(OP_Network *net, const char *name, OP_Operator *entry)
{
    return new SOP_CopyToPointsHDK(net, name, entry);
}

SOP_CopyToPointsHDK::SOP_CopyToPointsHDK(OP_Network *net, const char *name, OP_Operator *entry)
    : SOP_Node(net, name, entry)
{
    mySopFlags.setManagesDataIDs(true);
}
SOP_CopyToPointsHDK::~SOP_CopyToPointsHDK() {}

OP_ERROR
SOP_CopyToPointsHDK::cookInputGroups(OP_Context &context, int alone)
{
    UT_ASSERT(alone);
    OP_AutoLockInputs inputs(this);
    if (inputs.lock(context) >= UT_ERROR_ABORT)
        return error();

    const GU_Detail *detail = inputGeo(0);

    SOP_CopyToPointsHDKEnums::SourceGroupType param_group_type =
        SOP_CopyToPointsHDKEnums::SourceGroupType(
            evalInt(1, 0, CHgetEvalTime()));
    GA_GroupType group_type = GA_GROUP_INVALID;
    if (param_group_type == SOP_CopyToPointsHDKEnums::SourceGroupType::POINTS)
        group_type = GA_GROUP_POINT;
    else if (param_group_type == SOP_CopyToPointsHDKEnums::SourceGroupType::PRIMS)
        group_type = GA_GROUP_PRIMITIVE;

    const GA_Group *source_group;
    OP_ERROR ret = cookInputAllGroups(context, source_group, alone, true, 0, 1, group_type, true, true, false, GroupCreator(detail));
    if (ret >= UT_ERROR_ABORT)
        return ret;

    detail = inputGeo(1);
    const GA_PointGroup *ptgroup;
    ret = cookInputPointGroups(context, ptgroup, alone, true, 2, -1, true, true, GroupCreator(detail));
    return ret;
}

OP_ERROR
SOP_CopyToPointsHDK::cookMySop(OP_Context &context)
{
    return cookMyselfAsVerb(context);
}

const SOP_NodeVerb *
SOP_CopyToPointsHDK::cookVerb() const
{
    return SOP_CopyToPointsHDKVerb::theVerb.get();
}

} // End of HDK_Sample namespace

/// newSopOperator is the hook that Houdini grabs from this dll
/// and invokes to register the SOP.  In this case, we add ourselves
/// to the specified operator table.
void newSopOperator(OP_OperatorTable *table)
{
    table->addOperator(new OP_Operator(
        HDK_Sample::SOP_CopyToPointsHDKVerb::theSOPTypeName,    // Internal name
        "HDK Copy to Points",                                   // UI name
        HDK_Sample::SOP_CopyToPointsHDK::myConstructor,         // How to build the SOP
        HDK_Sample::SOP_CopyToPointsHDK::buildTemplates(),      // My parameters
        2,      // Min # of sources
        2,      // Max # of sources
        nullptr,// Custom local variables (none)
        0));    // No flags: not a generator, no merge input, not an output
}

namespace HDK_Sample {

/// NOTE: sopcache must already have myTargetOffsetList initialized for
///       this cook, since it's used for passing the target point list,
///       and it may be modified in here.
static bool
sopCopyByIDAttrib(
    const SOP_NodeVerb::CookParms &cookparms,
    const SOP_CopyToPointsHDKParms &sopparms,
    SOP_CopyToPointsHDKCache *sopcache,
    GU_Detail *output_geo,
    const GU_Detail *source,
    const GA_PrimitiveGroup *source_primgroup,
    const GA_PointGroup *source_pointgroup,
    const bool source_topology_changed,
    const GU_Detail *target,
    GA_OffsetList &input_target_point_list,
    const GA_PointGroup *target_group,
    SOP_CopyToPointsHDKCache::TargetAttribInfoMap &target_attrib_info,
    SOP_CopyToPointsHDKCache::TargetAttribInfoMap &target_group_info)
{
    // *** ID Attribute Setup ***

    const UT_StringHolder &idattribname = sopparms.getIDAttrib();
    const GA_Attribute *idattrib = target->findPointAttribute(idattribname);
    const GA_ATINumeric *idnumeric = GA_ATINumeric::cast(idattrib);
    const GA_ATIString *idstring = GA_ATIString::cast(idattrib);

    GA_ROHandleID target_int_idattrib;
    GA_ROHandleID source_int_idattrib;
    GA_ROHandleS target_str_idattrib;
    GA_ROHandleS source_str_idattrib;
    GA_AttributeOwner source_id_owner;
    if (idnumeric)
    {
        if (idnumeric->getStorageClass() != GA_STORECLASS_INT)
        {
            UT_WorkBuffer buf;
            buf.appendSprintf("Target Piece Attribute (%s) is a floating-point "
                "attribute; it must be an integer or string attribute, "
                "so ignoring Piece Attribute.",
                idattribname.c_str());
            cookparms.sopAddWarning(SOP_MESSAGE, buf.buffer());
            return false;
        }

        source_int_idattrib.bind(source->findPrimitiveAttribute(idattribname));
        if (!source_int_idattrib.isValid())
            source_int_idattrib.bind(source->findPointAttribute(idattribname));

        // NOTE: We don't require a source id attribute, because it
        //       defaults to using the source primitive number if there
        //       isn't a source attribute and the target attribute is
        //       an integer attribute.

        target_int_idattrib.bind(idnumeric);
        UT_ASSERT(target_int_idattrib.isValid());
        source_id_owner = !source_int_idattrib.isValid() ? GA_ATTRIB_PRIMITIVE : source_int_idattrib->getOwner();
    }
    else if (idstring)
    {
        source_str_idattrib.bind(source->findPrimitiveAttribute(idattribname));
        if (!source_str_idattrib.isValid())
            source_str_idattrib.bind(source->findPointAttribute(idattribname));

        // String case requires a source attribute
        if (!source_str_idattrib.isValid())
        {
            UT_WorkBuffer buf;
            buf.appendSprintf("Target Piece Attribute (%s) is a string "
                "attribute; source has no corresponding string point or "
                "primitive attribute, so ignoring Piece Attribute.",
                idattribname.c_str());
            cookparms.sopAddWarning(SOP_MESSAGE, buf.buffer());
            return false;
        }

        target_str_idattrib.bind(idstring);
        source_id_owner = source_str_idattrib->getOwner();
    }
    else
    {
        UT_WorkBuffer buf;
        buf.appendSprintf("Piece Attribute (%s) is not an integer "
            "or string attribute in the target input, so ignoring Piece Attribute.",
            idattribname.c_str());
        cookparms.sopAddWarning(SOP_MESSAGE, buf.buffer());
        return false;
    }

    bool target_group_changed =
        (target_group != nullptr) != (sopcache->myPrevHadTargetGroup);
    bool target_group_maybe_changed = target_group_changed;
    if (!target_group_changed && target_group)
    {
        // We don't check the contents until after handling
        // unmatched target points below.
        bool different_group = target_group->isDetached() ||
            (target_group->getDataId() != sopcache->myPrevTargetGroupDataID);
        target_group_maybe_changed = different_group;
    }

    // NOTE: If we're using an ID attribute and the target group has changed,
    //       the output topology changes, even if the number of target points
    //       is the same as before, since the selection could be different.
    bool topology_changed =
        (output_geo->getUniqueId() != sopcache->myPrevOutputDetailID) ||
        source_topology_changed ||
        target_group_changed ||
        (sopparms.getPack() != sopcache->myPrevPack);

    GA_DataId target_idattrib_dataid = idattrib->getDataId();
    topology_changed |= (target_idattrib_dataid != sopcache->myTargetIDAttribDataID);
    sopcache->myTargetIDAttribDataID = target_idattrib_dataid;

    topology_changed |= (source_id_owner != sopcache->mySourceIDAttribOwner);
    sopcache->mySourceIDAttribOwner = source_id_owner;

    GA_DataId source_idattrib_dataid;
    if (source_int_idattrib.isValid())
    {
        source_idattrib_dataid = source_int_idattrib->getDataId();
        topology_changed |= (source_idattrib_dataid != sopcache->mySourceIDAttribDataID);
    }
    else if (source_str_idattrib.isValid())
    {
        source_idattrib_dataid = source_str_idattrib->getDataId();
        topology_changed |= (source_idattrib_dataid != sopcache->mySourceIDAttribDataID);
    }
    else
    {
        // Using source primitive number as id attribute,
        // and source topology didn't change here, so
        // the primitive numbers haven't changed.
        source_idattrib_dataid = GA_INVALID_DATAID;
    }
    sopcache->mySourceIDAttribDataID = source_idattrib_dataid;

    using PieceData = SOP_CopyToPointsHDKCache::PieceData;
    UT_Array<PieceData> &piece_data = sopcache->myPieceData;
    UT_Array<exint> &target_to_piecei = sopcache->myTargetToPiece;

    bool recompute_mapping = topology_changed || target_group_maybe_changed;
    if (recompute_mapping)
    {
        // *** Value to Source Mapping ***

        // These arrays map an attribute value to a pair consisting of
        // the number of target points with that value, and
        // the list of source offsets with that value.
        UT_ArrayMap<exint,std::pair<exint,GA_OffsetList>> target_int_to_source;
        UT_ArrayStringMap<std::pair<exint,GA_OffsetList>> target_str_to_source;
        GA_Range target_point_range(target->getPointMap(), input_target_point_list);
        GA_OffsetList limited_target_point_list_nc;
        if (target_int_idattrib.isValid())
        {
            // First, find all (integer) target values.
            GA_Offset start;
            GA_Offset end;
            for (GA_Iterator it(target_point_range); it.blockAdvance(start, end); )
            {
                GA_PageNum pagenum = GAgetPageNum(start);
                if (target_int_idattrib->isPageConstant(pagenum))
                {
                    // Only try to insert once for constant page
                    exint value = target_int_idattrib.get(start);
                    // UT_ArrayMap excludes one key value, which we must skip.
                    if (UT::DefaultClearer<exint>::isClear(value))
                        continue;

                    bool new_value = !target_int_to_source.contains(value);

                    // NOTE: This relies on operator[] to do the insertion.
                    std::pair<exint,GA_OffsetList> &pair = target_int_to_source[value];
                    if (new_value)
                        pair.first = GA_Size(end-start);
                    else
                    {
                        pair.first += GA_Size(end-start);
                        continue;
                    }

                    if (!source_int_idattrib.isValid() &&
                        GAisValid(value) &&
                        value < source->getNumPrimitives())
                    {
                        // Add primitive offset with value as index,
                        // if it's a valid index and in source_primgroup.
                        GA_Offset source_primoff = source->primitiveOffset(GA_Index(value));
                        if (!source_primgroup || source_primgroup->contains(source_primoff))
                            pair.second.append(source_primoff);
                    }
                }
                else
                {
                    for (GA_Offset target_ptoff = start; target_ptoff < end; ++target_ptoff)
                    {
                        exint value = target_int_idattrib.get(target_ptoff);
                        // UT_ArrayMap excludes one key value, which we must skip.
                        if (UT::DefaultClearer<exint>::isClear(value))
                            continue;

                        bool new_value = !target_int_to_source.contains(value);

                        // NOTE: This relies on operator[] to do the insertion.
                        std::pair<exint,GA_OffsetList> &pair = target_int_to_source[value];
                        if (new_value)
                            pair.first = 1;
                        else
                        {
                            pair.first += 1;
                            continue;
                        }

                        if (!source_int_idattrib.isValid() &&
                            GAisValid(value) &&
                            value < source->getNumPrimitives())
                        {
                            // Add primitive offset with value as index,
                            // if it's a valid index and in source_primgroup.
                            GA_Offset source_primoff = source->primitiveOffset(GA_Index(value));
                            if (!source_primgroup || source_primgroup->contains(source_primoff))
                                pair.second.append(source_primoff);
                        }
                    }
                }
            }

            if (source_int_idattrib.isValid())
            {
                // Fill in all source offsets corresponding with each target value.
                GA_Range source_id_range;
                if (source_id_owner == GA_ATTRIB_PRIMITIVE)
                {
                    source_id_range = source->getPrimitiveRange(source_primgroup);
                }
                else
                {
                    UT_ASSERT(source_id_owner == GA_ATTRIB_POINT);
                    source_id_range = source->getPointRange(source_pointgroup);
                }
                GA_Offset start;
                GA_Offset end;
                for (GA_Iterator it(source_id_range); it.blockAdvance(start, end); )
                {
                    GA_PageNum pagenum = GAgetPageNum(start);
                    if (source_int_idattrib->isPageConstant(pagenum))
                    {
                        exint value = source_int_idattrib.get(start);
                        // UT_ArrayMap excludes one key value, which we must skip explicitly.
                        if (UT::DefaultClearer<exint>::isClear(value))
                            continue;
                        if (!target_int_to_source.contains(value))
                            continue;

                        // Append the whole block at once.
                        GA_OffsetList &list = target_int_to_source[value].second;
                        list.setTrivialRange(list.size(), start, GA_Size(end-start));
                    }
                    else
                    {
                        for (GA_Offset source_offset = start; source_offset < end; ++source_offset)
                        {
                            exint value = source_int_idattrib.get(source_offset);
                            // UT_ArrayMap excludes one key value, which we must skip explicitly.
                            if (UT::DefaultClearer<exint>::isClear(value))
                                continue;
                            if (!target_int_to_source.contains(value))
                                continue;

                            GA_OffsetList &list = target_int_to_source[value].second;
                            list.append(source_offset);
                        }
                    }
                }
            }

            // Remove map entries with no source offsets, since we
            // don't do anything with them, even if we're in the packing case.
            exint original_num_integers = target_int_to_source.size();
            for (auto it = target_int_to_source.begin(); !it.atEnd(); )
            {
                if (it->second.second.size() == 0)
                    it = target_int_to_source.erase(it);
                else
                    ++it;
            }
            if (target_int_to_source.size() != original_num_integers)
            {
                for (exint i = 0, n = input_target_point_list.size(); i < n; ++i)
                {
                    GA_Offset ptoff = input_target_point_list[i];
                    exint value = target_int_idattrib.get(ptoff);
                    if (target_int_to_source.contains(value))
                        limited_target_point_list_nc.append(ptoff);
                }
                input_target_point_list = std::move(limited_target_point_list_nc);
            }
        }
        else
        {
            // First, find all (string) target values.
            UT_ASSERT(target_str_idattrib.isValid() && source_str_idattrib.isValid());
            GA_Offset start;
            GA_Offset end;
            for (GA_Iterator it(target_point_range); it.blockAdvance(start, end); )
            {
                GA_PageNum pagenum = GAgetPageNum(start);
                if (target_str_idattrib->isPageConstant(pagenum))
                {
                    // Only try to insert once for constant page
                    const UT_StringHolder &value = target_str_idattrib.get(start);
                    UT_ASSERT_P(!UT::DefaultClearer<UT_StringHolder>::isClear(value));

                    bool new_value = !target_str_to_source.contains(value);

                    // NOTE: This relies on operator[] to do the insertion.
                    std::pair<exint,GA_OffsetList> &pair = target_str_to_source[value];
                    if (new_value)
                        pair.first = GA_Size(end-start);
                    else
                        pair.first += GA_Size(end-start);
                }
                else
                {
                    for (GA_Offset target_ptoff = start; target_ptoff < end; ++target_ptoff)
                    {
                        const UT_StringHolder &value = target_str_idattrib.get(target_ptoff);
                        UT_ASSERT_P(!UT::DefaultClearer<UT_StringHolder>::isClear(value));

                        bool new_value = !target_str_to_source.contains(value);

                        // NOTE: This relies on operator[] to do the insertion.
                        std::pair<exint,GA_OffsetList> &pair = target_str_to_source[value];
                        if (new_value)
                            pair.first = 1;
                        else
                            pair.first += 1;
                    }
                }
            }

            // Fill in all source offsets corresponding with each target value.
            GA_Range source_id_range;
            if (source_id_owner == GA_ATTRIB_PRIMITIVE)
            {
                source_id_range = source->getPrimitiveRange(source_primgroup);
            }
            else
            {
                UT_ASSERT(source_id_owner == GA_ATTRIB_POINT);
                source_id_range = source->getPointRange(source_pointgroup);
            }
            for (GA_Iterator it(source_id_range); it.blockAdvance(start, end); )
            {
                GA_PageNum pagenum = GAgetPageNum(start);
                if (source_str_idattrib->isPageConstant(pagenum))
                {
                    const UT_StringHolder &value = source_str_idattrib.get(start);
                    UT_ASSERT_P(!UT::DefaultClearer<UT_StringHolder>::isClear(value));
                    if (!target_str_to_source.contains(value))
                        continue;

                    // Append the whole block at once.
                    GA_OffsetList &list = target_str_to_source[value].second;
                    list.setTrivialRange(list.size(), start, GA_Size(end-start));
                }
                else
                {
                    for (GA_Offset source_offset = start; source_offset < end; ++source_offset)
                    {
                        const UT_StringHolder &value = source_str_idattrib.get(source_offset);
                        UT_ASSERT_P(!UT::DefaultClearer<UT_StringHolder>::isClear(value));
                        if (!target_str_to_source.contains(value))
                            continue;

                        GA_OffsetList &list = target_str_to_source[value].second;
                        list.append(source_offset);
                    }
                }
            }

            // Remove map entries with no source offsets, since we
            // don't do anything with them, even if we're in the packing case.
            exint original_num_strings = target_str_to_source.size();
            for (auto it = target_str_to_source.begin(); !it.atEnd(); )
            {
                if (it->second.second.size() == 0)
                    it = target_str_to_source.erase(it);
                else
                    ++it;
            }
            if (target_str_to_source.size() != original_num_strings)
            {
                for (exint i = 0, n = input_target_point_list.size(); i < n; ++i)
                {
                    GA_Offset ptoff = input_target_point_list[i];
                    const UT_StringHolder &value = target_str_idattrib.get(ptoff);
                    if (target_str_to_source.contains(value))
                        limited_target_point_list_nc.append(ptoff);
                }
                input_target_point_list = std::move(limited_target_point_list_nc);
            }
        }

        // We've now limited the input_target_point_list, so we can check whether
        // it's different from the previous cook.
        target_group_changed |=
            (!target_group && input_target_point_list.size() != sopcache->myPrevTargetPtCount);
        if (!target_group_changed && target_group)
        {
            // For named groups, we don't need to the contents if the data ID is the same.
            bool different_group = target_group->isDetached() ||
                (target_group->getDataId() != sopcache->myPrevTargetGroupDataID);
            if (different_group)
            {
                UT_ASSERT(sopcache->myTargetOffsetList.size() == input_target_point_list.size());
                bool equal_group = sopcache->myTargetOffsetList.isEqual(input_target_point_list, 0, input_target_point_list.size());
                if (!equal_group)
                {
                    target_group_changed = true;
                }
            }
        }
        sopcache->myTargetOffsetList = input_target_point_list;
        sopcache->myPrevHadTargetGroup = (target_group != nullptr);
        sopcache->myPrevTargetGroupDataID = ((!target_group || target_group->isDetached()) ? GA_INVALID_DATAID : target_group->getDataId());
        topology_changed |= target_group_changed;

        // Don't recompute piece data or target to piece mapping if
        // it turned out that target group didn't change and
        // topology didn't change for some other reason.
        if (topology_changed)
        {
            // *** Sort out information about each piece ***

            piece_data.clear();

            // First, the integer vs. string specific data.
            if (!target_int_to_source.empty())
            {
                piece_data.setSize(target_int_to_source.size());
                exint piecei = 0;
                for (auto it = target_int_to_source.ordered_begin(std::less<exint>()); !it.atEnd(); ++it, ++piecei)
                {
                    PieceData &current_piece = piece_data[piecei];
                    current_piece.myRefCount = it->second.first;
                    // Record the piece index so that it's fast to look it up below.
                    it->second.first = piecei;

                    current_piece.mySourceOffsetLists[source_id_owner] = std::move(it->second.second);
                }
            }
            else if (!target_str_to_source.empty())
            {
                piece_data.setSize(target_str_to_source.size());
                exint piecei = 0;
                for (auto it = target_str_to_source.ordered_begin(std::less<UT_StringHolder>()); !it.atEnd(); ++it, ++piecei)
                {
                    PieceData &current_piece = piece_data[piecei];
                    current_piece.myRefCount = it->second.first;
                    // Record the piece index so that it's fast to look it up below.
                    it->second.first = piecei;

                    current_piece.mySourceOffsetLists[source_id_owner] = std::move(it->second.second);
                }
            }

            exint num_target_points = input_target_point_list.size();

            // Make a target point number to piece number mapping, so that we don't
            // have to keep checking the attributes.
            target_to_piecei.setSizeNoInit(num_target_points);
            if (target_int_idattrib.isValid())
            {
                for (exint target_pointi = 0; target_pointi < num_target_points; ++target_pointi)
                {
                    GA_Offset target_ptoff = input_target_point_list[target_pointi];
                    exint target_value = target_int_idattrib.get(target_ptoff);
                    exint piecei = target_int_to_source[target_value].first;
                    target_to_piecei[target_pointi] = piecei;
                }
            }
            else
            {
                UT_ASSERT(target_str_idattrib.isValid());
                for (exint target_pointi = 0; target_pointi < num_target_points; ++target_pointi)
                {
                    GA_Offset target_ptoff = input_target_point_list[target_pointi];
                    const UT_StringHolder &target_value = target_str_idattrib.get(target_ptoff);
                    exint piecei = target_str_to_source[target_value].first;
                    target_to_piecei[target_pointi] = piecei;
                }
            }
        }
    }

    // *** Transform Setup ***

    bool had_transform_matrices = (sopcache->myTransformMatrices3D.get() != nullptr);

    // Use the target point list from the cache, so that it's always the limited list,
    // even if we didn't recompute the mapping on this cook.
    const GA_OffsetListRef &target_point_list = sopcache->myTargetOffsetList;

    // NOTE: Transforms have changed if the target group has changed,
    //       even if the number of points is the same.
    bool transforms_changed = target_group_changed;
    GUsetupPointTransforms(sopcache, target_point_list, target, sopparms.getTransform(), sopparms.getUseImplicitN(), transforms_changed);

    const bool has_transform_matrices = (sopcache->myTransformMatrices3D.get() != nullptr);

    if (topology_changed)
    {
        // Next, the data that's independent of integer vs. string.
        const exint npieces = piece_data.size();
        for (exint piecei = 0; piecei < npieces; ++piecei)
        {
            PieceData &current_piece = piece_data[piecei];
            GA_OffsetList &source_point_list = current_piece.mySourceOffsetLists[GA_ATTRIB_POINT];
            GA_OffsetList &source_prim_list = current_piece.mySourceOffsetLists[GA_ATTRIB_PRIMITIVE];

            if (source_id_owner == GA_ATTRIB_POINT)
            {
                if (source->getNumPrimitives() > 0)
                {
                    // Compute source prims list from point list
                    bool done = false;
                    if (source_point_list.size() == 1)
                    {
                        // One point, possibly a simple case.
                        GA_Offset ptoff = source_point_list[0];
                        GA_Offset vtxoff = source->pointVertex(ptoff);
                        if (!GAisValid(vtxoff))
                        {
                            // No vertices on the point, so no primitives.
                            done = true;
                        }
                        else if (!GAisValid(source->pointVertex(vtxoff)))
                        {
                            // One vertex, so one primitive... if it only has one vertex.
                            GA_Offset primoff = source->vertexPrimitive(vtxoff);
                            if (source->getPrimitiveVertexCount(primoff) == 1)
                                source_prim_list.append(primoff);
                            done = true;
                        }
                    }
                    if (!done)
                    {
                        GA_DataBitArray points_in_piece(source->getNumPointOffsets(), false);
                        GA_DataBitArray prims_in_piece(source->getNumPrimitiveOffsets(), false);
                        for (exint i = 0, n = source_point_list.size(); i < n; ++i)
                        {
                            GA_Offset ptoff = source_point_list[i];
                            points_in_piece.set<true>(ptoff);
                            for (GA_Offset vtxoff = source->pointVertex(ptoff); GAisValid(vtxoff); vtxoff = source->vertexToNextVertex(vtxoff))
                            {
                                GA_Offset primoff = source->vertexPrimitive(vtxoff);
                                prims_in_piece.set<true>(primoff);
                            }
                        }

                        // Iterate through all of the primitives.
                        // Add only primitives where all vertices have their points present in the piece.
                        GA_Offset start = GA_Offset(0);
                        GA_Offset end = source->getNumPrimitiveOffsets();
                        while (start < end)
                        {
                            GA_Size size; bool value;
                            prims_in_piece.getConstantSpan(start, end, size, value);
                            if (!value)
                            {
                                start += size;
                                continue;
                            }
                            GA_Offset local_end = start + size;
                            for (GA_Offset primoff = start; primoff < local_end; ++primoff)
                            {
                                bool all_points_present = true;
                                const GA_OffsetListRef vertices = source->getPrimitiveVertexList(primoff);
                                for (exint i = 0, n = vertices.size(); i < n; ++i)
                                {
                                    GA_Offset ptoff = source->vertexPoint(vertices[i]);
                                    all_points_present &= points_in_piece.get(ptoff);
                                }
                                if (all_points_present)
                                    source_prim_list.append(primoff);
                            }
                            start = local_end;
                        }
                    }
                }
            }
            else
            {
                // Compute source points list from prim list
                if (source_prim_list.size() == 1 && source->getPrimitiveVertexCount(source_prim_list[0]) == 1)
                {
                    // Common case of a single vertex, e.g. a single packed primitive.
                    source_point_list.append(source->vertexPoint(source->getPrimitiveVertexOffset(source_prim_list[0],0)));
                }
                else
                {
                    GA_DataBitArray points_in_piece(source->getNumPointOffsets(), false);
                    for (exint i = 0, n = source_prim_list.size(); i < n; ++i)
                    {
                        GA_Offset primoff = source_prim_list[i];
                        const GA_OffsetListRef vertices = source->getPrimitiveVertexList(primoff);
                        for (exint i = 0, n = vertices.size(); i < n; ++i)
                        {
                            GA_Offset ptoff = source->vertexPoint(vertices[i]);
                            points_in_piece.set<true>(ptoff);
                        }
                    }
                    // Iterate through all of the points, adding them to the list.
                    GA_Offset start = GA_Offset(0);
                    GA_Offset end = source->getNumPointOffsets();
                    while (start < end)
                    {
                        GA_Size size; bool value;
                        points_in_piece.getConstantSpan(start, end, size, value);
                        if (value)
                        {
                            source_point_list.setTrivialRange(
                                source_point_list.size(), start, size);
                        }
                        start += size;
                    }
                }
            }

            // Fill in the source vertices list
            GA_OffsetList &source_vert_list = current_piece.mySourceOffsetLists[GA_ATTRIB_VERTEX];
            const GA_PrimitiveList &source_primlist = source->getPrimitiveList();
            for (exint i = 0, n = source_prim_list.size(); i < n; ++i)
            {
                GA_Offset primoff = source_prim_list[i];
                const GA_OffsetListRef vertices = source_primlist.getVertexList(primoff);
                source_vert_list.append(vertices);
            }

            // Fill in the vertex to point mapping
            exint nvertices = source_vert_list.size();
            if (nvertices < 16 || source_point_list.isTrivial())
            {
                // Common special case for few vertices, (e.g. 1 packed prim or
                // 1 simple curve), or trivial point list, so we don't need
                // a map to quickly find the point offset.
                for (exint i = 0; i < nvertices; ++i)
                {
                    GA_Offset vtxoff = source_vert_list[i];
                    GA_Offset ptoff = source->vertexPoint(vtxoff);
                    exint rel_pointi = source_point_list.find(ptoff);
                    current_piece.myRelVtxToPt.append(rel_pointi);
                }
            }
            else
            {
                // Many vertices and non-trivial point list,
                // so we use a reverse point map.
                exint npoints = source_point_list.size();

                UT_ArrayMap<GA_Offset,exint> ptoff_to_piecei;
                ptoff_to_piecei.reserve(npoints);

                for (exint pointi = 0; pointi < npoints; ++pointi)
                {
                    GA_Offset ptoff = source_point_list[pointi];
                    ptoff_to_piecei[ptoff] = pointi;
                }

                for (exint i = 0; i < nvertices; ++i)
                {
                    GA_Offset vtxoff = source_vert_list[i];
                    GA_Offset ptoff = source->vertexPoint(vtxoff);
                    exint rel_pointi = ptoff_to_piecei[ptoff];
                    current_piece.myRelVtxToPt.append(rel_pointi);
                }
            }
        }
    }

    const exint npieces = piece_data.size();
    const exint num_target_points = target_to_piecei.size();

    if (sopparms.getPack())
    {
        const GEO_ViewportLOD lod = sopViewportLODFromParam(sopparms.getViewportLOD());
        const bool source_changed =
            source->getUniqueId() != sopcache->myPrevSourceUniqueID ||
            source->getMetaCacheCount() != sopcache->myPrevSourceMetaCacheCount ||
            source_topology_changed;
        const bool lod_changed = (lod != sopcache->myPrevViewportLOD);
        const bool source_intrinsic_changed =
            source_changed ||
            sopCachePivotType(sopparms.getPivot()) != sopcache->myPrevPivotEnum ||
            lod_changed;

        UT_Array<GU_PackedGeometry *> packed_geos;

        // We need to recreate the packed primitive implementations even if
        // just attributes on the source have changed, since the impls need
        // to pick up the source attribute changes.
        if (topology_changed || source_changed)
        {
            // Packed primitive for each target point whose value has at least one source offset.
            output_geo->clearAndDestroy();
            if (num_target_points > 0)
                GUcreateEmptyPackedGeometryPrims(output_geo, num_target_points);

            // Create a GU_PackedGeometry for each piece, (in parallel).
            packed_geos.setSizeNoInit(npieces);

            UTparallelForEachNumber(npieces, [&piece_data,source,&packed_geos](const UT_BlockedRange<exint> &r)
            {
                for (exint piecei = r.begin(), end = r.end(); piecei < end; ++piecei)
                {
                    PieceData &current_piece = piece_data[piecei];

                    GU_DetailHandle detail_handle;
                    GU_Detail *packed_detail = new GU_Detail();
                    detail_handle.allocateAndSet(packed_detail);

                    // FIXME: This should be able to use current_piece.myRelVtxToPt!!! or is that not the right way to do it???
                    GUcreateGeometryFromSource(
                        packed_detail,
                        source,
                        current_piece.mySourceOffsetLists[GA_ATTRIB_POINT],
                        current_piece.mySourceOffsetLists[GA_ATTRIB_VERTEX],
                        current_piece.mySourceOffsetLists[GA_ATTRIB_PRIMITIVE],
                        1);

                    exint num_source_attribs[3] = {0,0,0};
                    SYS_STATIC_ASSERT_MSG(int(GA_ATTRIB_VERTEX)<3 && int(GA_ATTRIB_POINT)<3 && int(GA_ATTRIB_PRIMITIVE)<3,
                        "Array above depends on owners other than detail being less than 3");
                    GUaddAttributesFromSourceOrTarget(
                        packed_detail,
                        source,
                        num_source_attribs);

                    GA_SplittableRange output_splittable_ranges[3] =
                    {
                        GA_SplittableRange(packed_detail->getVertexRange()),
                        GA_SplittableRange(packed_detail->getPointRange()),
                        GA_SplittableRange(packed_detail->getPrimitiveRange())
                    };
                    SYS_STATIC_ASSERT_MSG(int(GA_ATTRIB_VERTEX)==0 && int(GA_ATTRIB_POINT)==1 && int(GA_ATTRIB_PRIMITIVE)==2,
                        "Arrays above and loop below are assuming the order of GA_AttributeOwner enum");

                    // NOTE: We're never using the cache for this, so we pass nullptr for sopcache.
                    GUcopyAttributesFromSource(
                        packed_detail,
                        output_splittable_ranges,
                        source,
                        1,
                        nullptr,
                        current_piece.mySourceOffsetLists,
                        num_source_attribs,
                        true,
                        false,
                        false,
                        true,
                        true);

                    GU_PackedGeometry *packed_geo = new GU_PackedGeometry();
                    packed_geo->setDetailPtr(detail_handle);

                    // Cache the bounds in advance, even if we don't need the box center for the pivot,
                    // to avoid thread contention downstream when anything requests it.
                    UT_BoundingBox bbox;
                    packed_geo->getBoundsCached(bbox);

                    // Add the reference counter refs in bulk
                    intrusive_ptr_add_ref(packed_geo, current_piece.myRefCount);

                    packed_geos[piecei] = packed_geo;
                }
            });
        }

        bool centroid_pivot = (sopparms.getPivot() == SOP_CopyToPointsHDKEnums::Pivot::CENTROID);

        if (topology_changed || source_intrinsic_changed)
        {
            GA_Offset start_primoff = num_target_points ? output_geo->primitiveOffset(GA_Index(0)) : GA_INVALID_OFFSET;

            // Set the implementations of all of the primitives at once.
            auto &&functor = [output_geo,&packed_geos,start_primoff,
                &target_to_piecei,centroid_pivot,lod,
                topology_changed,source_changed](const UT_BlockedRange<GA_Offset> &r)
            {
                exint target_pointi = GA_Size(r.begin()-start_primoff);
                for (GA_Offset primoff = r.begin(), end = r.end(); primoff < end; ++primoff, ++target_pointi)
                {
                    GA_Primitive *prim = output_geo->getPrimitive(primoff);
                    GU_PrimPacked *packed_prim = UTverify_cast<GU_PrimPacked *>(prim);
                    exint piecei = target_to_piecei[target_pointi];

                    if (topology_changed || source_changed)
                    {
                        GU_PackedGeometry *packed_geo_nc = packed_geos[piecei];

                        // References were already added above, so we don't need to add them here.
                        packed_prim->setImplementation(packed_geo_nc, false);
                    }

                    UT_Vector3 pivot;
                    if (centroid_pivot)
                    {
                        UT_BoundingBox bbox;
                        const GU_PackedImpl *packed_geo = packed_prim->implementation();
                        packed_geo->getBoundsCached(bbox);
                        if (bbox.isValid())
                            pivot = bbox.center();
                        else
                            pivot.assign(0,0,0);
                    }
                    else
                        pivot.assign(0,0,0);
                    packed_prim->setPivot(pivot);

                    packed_prim->setViewportLOD(lod);
                }
            };

            const UT_BlockedRange<GA_Offset> prim_range(start_primoff, start_primoff+num_target_points);
            constexpr exint PARALLEL_THRESHOLD = 256;
            if (num_target_points >= PARALLEL_THRESHOLD)
                UTparallelForLightItems(prim_range, functor);
            else
                functor(prim_range);
        }

        const bool pivots_changed =
            sopCachePivotType(sopparms.getPivot()) != sopcache->myPrevPivotEnum ||
            (centroid_pivot && source_changed);

        // Pivots all zero if pivots not at centroid.
        UT_Vector3 pivot(0,0,0);

        GUhandleTargetAttribsForPackedPrims(
            output_geo,
            sopcache,
            topology_changed,
            had_transform_matrices,
            target,
            target_point_list,
            target_attrib_info,
            target_group_info,
            !centroid_pivot ? &pivot : nullptr);

        if (topology_changed)
        {
            output_geo->bumpDataIdsForAddOrRemove(true, true, true);
        }
        if (source_changed)
        {
            output_geo->getPrimitiveList().bumpDataId();
        }
        if (transforms_changed || pivots_changed)
        {
            output_geo->getP()->bumpDataId();
            // If there are no transform matrices, the primitives weren't transformed.
            // If source_changed, we already bumped the primitive list data ID.
            bool has_transform_matrices = (sopcache->myTransformMatrices3D.get() != nullptr);
            if ((has_transform_matrices || had_transform_matrices) && !source_changed)
                output_geo->getPrimitiveList().bumpDataId();
        }
        if (lod_changed)
        {
            output_geo->getPrimitiveList().bumpDataId();
        }

        sopcache->myPrevPack = true;
        sopcache->myPrevTargetPtCount = num_target_points;
        sopcache->myPrevSourceGroupDataID = source->getUniqueId();
        sopcache->myPrevSourceMetaCacheCount = source->getMetaCacheCount();
        sopcache->myPrevPivotEnum = sopCachePivotType(sopparms.getPivot());
        sopcache->myPrevViewportLOD = lod;
        sopcache->myPrevOutputDetailID = output_geo->getUniqueId();
        sopcache->myPrevSourceUniqueID = source->getUniqueId();
        sopcache->myPrevSourceMetaCacheCount = source->getMetaCacheCount();

        // No attributes from source in output_geo, so we can clear the source data ID maps.
        sopcache->mySourceAttribDataIDs[GA_ATTRIB_VERTEX].clear();
        sopcache->mySourceAttribDataIDs[GA_ATTRIB_POINT].clear();
        sopcache->mySourceAttribDataIDs[GA_ATTRIB_PRIMITIVE].clear();
        sopcache->mySourceGroupDataIDs[GA_ATTRIB_VERTEX].clear();
        sopcache->mySourceGroupDataIDs[GA_ATTRIB_POINT].clear();
        sopcache->mySourceGroupDataIDs[GA_ATTRIB_PRIMITIVE].clear();
        sopcache->mySourceEdgeGroupDataIDs.clear();
        sopcache->myPieceOffsetStarts[GA_ATTRIB_VERTEX].setCapacity(0);
        sopcache->myPieceOffsetStarts[GA_ATTRIB_POINT].setCapacity(0);
        sopcache->myPieceOffsetStarts[GA_ATTRIB_PRIMITIVE].setCapacity(0);
    }
    else
    {
        // Full copying (not packing)
        if (topology_changed)
        {
            output_geo->clearAndDestroy();

            UTparallelForEachNumber(npieces, [&piece_data,source](const UT_BlockedRange<exint> &r)
            {
                for (exint piecei = r.begin(), end = r.end(); piecei < end; ++piecei)
                {
                    PieceData &current_piece = piece_data[piecei];

                    // For this piece, compute:
                    // whether there are shared points,
                    // whether the points are contiguous,
                    // the vertex list size list,
                    // the primtype count pairs, and
                    // the closed span lengths.

                    // It's safe for hassharedpoints to be true when there are no shared points,
                    // but not vice versa.
                    bool hassharedpoints = true;
                    // In this case, "contiguous" means "points of the vertices
                    // start from startpt and are contiguous in vertex order."
                    bool hascontiguouspoints = false;
                    const GA_OffsetList &piece_point_list = current_piece.mySourceOffsetLists[GA_ATTRIB_POINT];
                    const GA_OffsetList &piece_vertex_list = current_piece.mySourceOffsetLists[GA_ATTRIB_VERTEX];
                    const GA_OffsetList &piece_prim_list = current_piece.mySourceOffsetLists[GA_ATTRIB_PRIMITIVE];
                    exint piece_point_count = piece_point_list.size();
                    exint piece_vertex_count = piece_vertex_list.size();
                    exint piece_prim_count = piece_prim_list.size();
                    UT_ASSERT_P(piece_vertex_count == current_piece.myRelVtxToPt.size());
                    if (piece_point_count >= piece_vertex_count)
                    {
                        // If there is at least one point per vertex, there's a
                        // decent chance that none of the source points are shared,
                        // which can make building the primitives much faster,
                        // so we check.

                        hascontiguouspoints = current_piece.myRelVtxToPt.isTrivial();
                        hassharedpoints = false;

                        if (!hascontiguouspoints && piece_vertex_count > 0)
                        {
                            // TODO: Parallelize this.
                            // Unlike in GUcreateGeometryFromSource,
                            // we've already computed current_piece.myRelVtxToPt,
                            // so we can just iterate through it here.
                            exint last_point = current_piece.myRelVtxToPt[0];
                            for (exint vtxi = 1; vtxi < piece_vertex_count; ++vtxi)
                            {
                                exint current_point = current_piece.myRelVtxToPt[vtxi];

                                // This isn't a perfect check for sharing, but since
                                // we don't want to do a full sort, we just check whether
                                // the point offsets are strictly increasing.
                                // If points are shared, this check will make hassharedpoints true.
                                // If no points are shared, this can also end up being true sometimes.
                                hassharedpoints |= (current_point <= last_point);
                                if (hassharedpoints)
                                    break;
                                last_point = current_point;
                            }
                        }
                    }
                    current_piece.myHasSharedPoints = hassharedpoints;
                    current_piece.myHasContiguousPoints = hascontiguouspoints;

                    GA_PolyCounts &vertexlistsizelist = current_piece.myVertexListSizeList;
                    vertexlistsizelist.clear();
                    auto &prim_type_count_pairs = current_piece.myPrimTypeCountPairs;
                    prim_type_count_pairs.clear();
                    UT_SmallArray<exint, 2*sizeof(exint)> &closed_span_lengths = current_piece.myClosedSpanLengths;
                    closed_span_lengths.clear();

                    if (piece_prim_count > 0)
                    {
                        GA_Offset primoff = piece_prim_list[0];
                        const GA_OffsetListRef vertices = source->getPrimitiveVertexList(primoff);
                        exint vertex_count = vertices.size();
                        vertexlistsizelist.append(vertex_count);

                        int primtype = source->getPrimitiveTypeId(primoff);
                        prim_type_count_pairs.append(std::make_pair(primtype,1));

                        bool closed = vertices.getExtraFlag();
                        if (closed)
                        {
                            // Index 0 (size 1) always represents open,
                            // so we need an extra zero.
                            closed_span_lengths.append(0);
                        }
                        closed_span_lengths.append(1);

                        for (exint primi = 1; primi < piece_prim_count; ++primi)
                        {
                            GA_Offset primoff = piece_prim_list[primi];
                            const GA_OffsetListRef vertices = source->getPrimitiveVertexList(primoff);
                            exint vertex_count = vertices.size();
                            vertexlistsizelist.append(vertex_count);

                            int primtype = source->getPrimitiveTypeId(primoff);
                            if (prim_type_count_pairs.last().first == primtype)
                                ++(prim_type_count_pairs.last().second);
                            else
                                prim_type_count_pairs.append(std::make_pair(primtype,1));

                            bool closed = vertices.getExtraFlag();
                            // Index 0 (size 1) always represents open, and so does every even index (odd size).
                            // Every odd index (even size) always represents closed.
                            // This condition checks if we're switching between open and closed.
                            if ((closed_span_lengths.size()&1) == exint(closed))
                                closed_span_lengths.append(1);
                            else
                                ++(closed_span_lengths.last());
                        }
                    }
                }
            });

            // Has shared points if any piece has shared points.
            // Has contiguous points only if all pieces have contiguous points.
            bool hassharedpoints = false;
            bool hascontiguouspoints = true;
            exint total_nvertices = 0;
            exint total_nprims = 0;
            for (exint piecei = 0; piecei < npieces; ++piecei)
            {
                PieceData &current_piece = piece_data[piecei];
                hassharedpoints |= current_piece.myHasSharedPoints;
                hascontiguouspoints &= current_piece.myHasContiguousPoints;
                exint piece_nvertices = current_piece.mySourceOffsetLists[GA_ATTRIB_VERTEX].size();
                exint piece_nprims = current_piece.mySourceOffsetLists[GA_ATTRIB_PRIMITIVE].size();
                total_nvertices += current_piece.myRefCount * piece_nvertices;
                total_nprims += current_piece.myRefCount * piece_nprims;
            }

            // Combine everything needed for GEObuildPrimitives
            // from each of the pieces.
            UT_SmallArray<exint> closed_span_lengths;
            if (total_nprims > 0)
                closed_span_lengths.append(0);
            UT_SmallArray<std::pair<int,exint>> prim_type_count_pairs;
            GA_PolyCounts vertexlistsizelist;
            UT_Array<exint> vertexpointnumbers;
            vertexpointnumbers.setSizeNoInit(hascontiguouspoints ? 0 : total_nvertices);
            UT_Array<exint> &piece_point_starts = sopcache->myPieceOffsetStarts[GA_ATTRIB_POINT];
            UT_Array<exint> &piece_vertex_starts = sopcache->myPieceOffsetStarts[GA_ATTRIB_VERTEX];
            UT_Array<exint> &piece_prim_starts = sopcache->myPieceOffsetStarts[GA_ATTRIB_PRIMITIVE];
            piece_point_starts.setSizeNoInit(num_target_points);
            piece_vertex_starts.setSizeNoInit(num_target_points);
            piece_prim_starts.setSizeNoInit(num_target_points);
            exint piece_point_start = 0;
            exint piece_vertex_start = 0;
            exint piece_prim_start = 0;
            for (exint targeti = 0; targeti < num_target_points; ++targeti)
            {
                exint piecei = target_to_piecei[targeti];
                PieceData &current_piece = piece_data[piecei];

                piece_point_starts[targeti] = piece_point_start;
                piece_vertex_starts[targeti] = piece_vertex_start;
                piece_prim_starts[targeti] = piece_prim_start;

                exint local_npoints = current_piece.mySourceOffsetLists[GA_ATTRIB_POINT].size();
                exint local_nvertices = current_piece.mySourceOffsetLists[GA_ATTRIB_VERTEX].size();
                exint local_nprims = current_piece.mySourceOffsetLists[GA_ATTRIB_PRIMITIVE].size();

                exint num_prim_type_count_pairs = current_piece.myPrimTypeCountPairs.size();
                if (num_prim_type_count_pairs > 0)
                {
                    exint pairi = 0;
                    if (prim_type_count_pairs.size() > 0 &&
                        prim_type_count_pairs.last().first == current_piece.myPrimTypeCountPairs[0].first)
                    {
                        prim_type_count_pairs.last().second += current_piece.myPrimTypeCountPairs[0].second;
                        ++pairi;
                    }
                    for (; pairi < num_prim_type_count_pairs; ++pairi)
                    {
                        prim_type_count_pairs.append(current_piece.myPrimTypeCountPairs[pairi]);
                    }

                    vertexlistsizelist.append(current_piece.myVertexListSizeList);

                    if (!vertexpointnumbers.isEmpty())
                    {
                        for (exint i = 0; i < local_nvertices; ++i)
                        {
                            vertexpointnumbers[piece_vertex_start+i] = current_piece.myRelVtxToPt[i] + piece_point_start;
                        }
                    }

                    auto &local_closed_span_lengths = current_piece.myClosedSpanLengths;
                    exint spani = (local_closed_span_lengths[0] == 0);
                    if (((closed_span_lengths.size()-1)&1) == (spani&1))
                    {
                        closed_span_lengths.last() += local_closed_span_lengths[spani];
                        ++spani;
                    }
                    for (exint nspans = local_closed_span_lengths.size(); spani < nspans; ++spani)
                    {
                        closed_span_lengths.append(local_closed_span_lengths[spani]);
                    }
                }

                piece_point_start += local_npoints;
                piece_vertex_start += local_nvertices;
                piece_prim_start += local_nprims;
            }

            exint total_npoints = piece_point_start;
            GA_Offset start_ptoff = output_geo->appendPointBlock(total_npoints);

            GA_Offset start_primoff = GEObuildPrimitives(
                output_geo,
                prim_type_count_pairs.getArray(),
                start_ptoff,
                total_npoints,
                vertexlistsizelist,
                vertexpointnumbers.getArray(),
                hassharedpoints,
                closed_span_lengths.getArray(),
                1);

            // No primitive data to copy if only polygons and tetrahedra,
            // since they have no member data outside of GA_Primitive,
            // and might be stored compressed in GA_PrimitiveList.
            exint num_polys_and_tets =
                output_geo->countPrimitiveType(GA_PRIMPOLY) +
                output_geo->countPrimitiveType(GA_PRIMTETRAHEDRON);
            if (output_geo->getNumPrimitives() != num_polys_and_tets)
            {
                // Copy primitive subclass data for types other than polygons and tetrahedra.
                UT_BlockedRange<GA_Offset> primrange(start_primoff, start_primoff+output_geo->getNumPrimitives());
                auto &&functor = [output_geo,source,&piece_prim_starts,start_primoff,
                    &target_to_piecei,&piece_data,num_target_points](const UT_BlockedRange<GA_Offset> &r)
                {
                    exint output_primi = r.begin() - start_primoff;
                    // Find the first entry in piece_prim_starts where the next entry is greater than output_primi,
                    // (in other words, the last entry whose value is less than or equal to output_primi,
                    // so output_primi is in that piece.)  The -1 is to go back one.
                    exint targeti = (std::upper_bound(
                        piece_prim_starts.getArray(),
                        piece_prim_starts.getArray()+piece_prim_starts.size(),
                        output_primi) - piece_prim_starts.getArray()) - 1;
                    UT_ASSERT_P(targeti >= 0 && targeti < num_target_points);

                    exint piece_prim_start = piece_prim_starts[targeti];
                    exint piece_primi = output_primi - piece_prim_start;
                    exint piecei = target_to_piecei[targeti];
                    PieceData *current_piece = &piece_data[piecei];
                    const GA_OffsetList *piece_prim_list = &(current_piece->mySourceOffsetLists[GA_ATTRIB_PRIMITIVE]);

                    for (GA_Offset dest_off = r.begin(), end = r.end(); dest_off < end; ++dest_off)
                    {
                        GA_Offset source_off = (*piece_prim_list)[piece_primi];
                        const GA_Primitive *source_prim = source->getPrimitive(source_off);
                        GA_Primitive *output_prim = output_geo->getPrimitive(dest_off);
                        output_prim->copySubclassData(source_prim);

                        ++piece_primi;
                        // NOTE: This must be while instead of if, because there can be zero primitives in a piece.
                        while (piece_primi >= piece_prim_list->size())
                        {
                            piece_primi = 0;
                            ++targeti;
                            if (targeti >= num_target_points)
                                break;
                            piecei = target_to_piecei[targeti];
                            current_piece = &piece_data[piecei];
                            piece_prim_list = &(current_piece->mySourceOffsetLists[GA_ATTRIB_PRIMITIVE]);
                        }
                    }
                };
                if (output_geo->getNumPrimitives() >= 1024)
                {
                    UTparallelForLightItems(primrange, functor);
                }
                else
                {
                    functor(primrange);
                }
            }

            sopcache->myPrevOutputDetailID = output_geo->getUniqueId();
            sopcache->myPrevTargetPtCount = num_target_points;
            sopcache->myPrevPack = false;

            sopcache->mySourceAttribDataIDs[GA_ATTRIB_VERTEX].clear();
            sopcache->mySourceAttribDataIDs[GA_ATTRIB_POINT].clear();
            sopcache->mySourceAttribDataIDs[GA_ATTRIB_PRIMITIVE].clear();
            sopcache->mySourceGroupDataIDs[GA_ATTRIB_VERTEX].clear();
            sopcache->mySourceGroupDataIDs[GA_ATTRIB_POINT].clear();
            sopcache->mySourceGroupDataIDs[GA_ATTRIB_PRIMITIVE].clear();
            sopcache->mySourceEdgeGroupDataIDs.clear();
            sopcache->myTargetAttribInfo.clear();
            sopcache->myTargetGroupInfo.clear();
        }

        // *** Attribute Setup ***

        if (!topology_changed)
        {
            GUremoveUnnecessaryAttribs(
                output_geo,
                source,
                target,
                sopcache,
                &target_attrib_info,
                &target_group_info);
        }

        bool needed_transforms[NeededTransforms::num_needed_transforms];
        for (exint i = 0; i < NeededTransforms::num_needed_transforms; ++i)
            needed_transforms[i] = false;

        // Add attributes from source and target that are not in output_geo.
        exint num_source_attribs[3] = {0,0,0};
        exint num_target_attribs[3] = {0,0,0};
        SYS_STATIC_ASSERT_MSG(int(GA_ATTRIB_VERTEX)<3 && int(GA_ATTRIB_POINT)<3 && int(GA_ATTRIB_PRIMITIVE)<3,
            "Arrays above are assuming the order of GA_AttributeOwner enum");

        GUaddAttributesFromSourceOrTarget(
            output_geo,
            source,
            num_source_attribs,
            has_transform_matrices,
            needed_transforms,
            target,
            &target_attrib_info,
            &target_group_info,
            num_target_attribs);

        // *** Specific Transform Caches ***

        GUcomputeTransformTypeCaches(
            sopcache,
            num_target_points,
            transforms_changed,
            needed_transforms);

        // *** Source Attribute Copying ***

        GA_SplittableRange output_splittable_ranges[3] =
        {
            GA_SplittableRange(output_geo->getVertexRange()),
            GA_SplittableRange(output_geo->getPointRange()),
            GA_SplittableRange(output_geo->getPrimitiveRange())
        };
        SYS_STATIC_ASSERT_MSG(int(GA_ATTRIB_VERTEX)==0 && int(GA_ATTRIB_POINT)==1 && int(GA_ATTRIB_PRIMITIVE)==2,
            "Arrays above and loop below are assuming the order of GA_AttributeOwner enum");

        if (!source->edgeGroups().isEmpty() && output_geo->getNumPrimitives() > 0)
        {
            cookparms.sopAddWarning(SOP_MESSAGE, "Edge groups aren't yet supported when Piece Attribute is enabled and Pack and Instance is disabled.");
        }

        GUcopyAttributesFromSource(
            output_geo,
            output_splittable_ranges,
            source,
            num_target_points,
            sopcache,
            sopcache->mySourceOffsetLists,
            num_source_attribs,
            false,
            had_transform_matrices,
            has_transform_matrices,
            topology_changed,
            transforms_changed,
            target,
            &target_attrib_info,
            &target_group_info,
            target_to_piecei.getArray(),
            sopcache->myPieceOffsetStarts,
            piece_data.getArray());

        // *** Target Attribute Copying ***

        GUcopyAttributesFromTarget(
            output_geo,
            output_splittable_ranges,
            num_target_points,
            sopcache,
            0, // source_point_count
            0, // source_vertex_count
            0, // source_prim_count
            num_target_attribs,
            target_point_list,
            target,
            target_attrib_info,
            target_group_info,
            topology_changed,
            target_to_piecei.getArray(),
            sopcache->myPieceOffsetStarts,
            piece_data.getArray());

        if (topology_changed)
        {
            output_geo->bumpDataIdsForAddOrRemove(true, true, true);
        }
    }

    // Returning true means that we used the ID attribute.
    return true;
}

void
SOP_CopyToPointsHDKVerb::cook(const SOP_NodeVerb::CookParms &cookparms) const
{
    using namespace SOP_CopyToPointsHDKEnums;
    auto &&sopparms = cookparms.parms<SOP_CopyToPointsHDKParms>();
    auto sopcache = (SOP_CopyToPointsHDKCache *)cookparms.cache();
    GU_Detail *output_geo = cookparms.gdh().gdpNC();
    const GU_Detail *source = cookparms.inputGeo(0);
    const GU_Detail *target = cookparms.inputGeo(1);

    // *** Parse input groups ***

    GOP_Manager group_parser;

    const UT_StringHolder &source_groupname = sopparms.getSourceGroup();
    const GA_ElementGroup *source_group = nullptr;
    const GA_PrimitiveGroup *source_primgroup = nullptr;
    GA_PrimitiveGroupUPtr source_primgroup_deleter;
    const GA_PointGroup *source_pointgroup = nullptr;
    GA_PointGroupUPtr source_pointgroup_deleter;
    if (source_groupname.isstring())
    {
        SourceGroupType sourcegrouptype = sopparms.getSourceGroupType();

        bool ok = true;
        GA_GroupType gagrouptype = (sourcegrouptype == SourceGroupType::GUESS) ? GA_GROUP_INVALID :
            ((sourcegrouptype == SourceGroupType::PRIMS) ? GA_GROUP_PRIMITIVE : GA_GROUP_POINT);
        const GA_Group *temp_source_group =
            group_parser.parseGroupDetached(source_groupname, gagrouptype, source, true, true, ok);

        if (!ok)
            cookparms.sopAddWarning(SOP_ERR_BADGROUP, source_groupname);
        if (temp_source_group != nullptr)
        {
            gagrouptype = temp_source_group->classType();
            if (gagrouptype == GA_GROUP_PRIMITIVE)
            {
                source_primgroup = UTverify_cast<const GA_PrimitiveGroup*>(temp_source_group);
                source_group = source_primgroup;

                // Make sure the primitive group is unordered
                if (source_primgroup->isOrdered())
                {
                    source_primgroup_deleter = UTmakeUnique<GA_PrimitiveGroup>(*source);
                    source_primgroup_deleter->copyMembership(*source_primgroup, false);
                    source_primgroup = source_primgroup_deleter.get();
                }

                // Get all points that are used by the primitives
                source_pointgroup_deleter = UTmakeUnique<GA_PointGroup>(*source);
                source_pointgroup_deleter->combine(source_primgroup);
                source_pointgroup_deleter->makeUnordered();
                source_pointgroup = source_pointgroup_deleter.get();
            }
            else if (gagrouptype == GA_GROUP_POINT)
            {
                source_pointgroup = UTverify_cast<const GA_PointGroup*>(temp_source_group);
                source_group = source_pointgroup;

                // We don't want to add other points, so we only
                // want primitives for which all points they use
                // are present in source_pointgroup.
                // First, get all primitives that are used by the points
                source_primgroup_deleter = UTmakeUnique<GA_PrimitiveGroup>(*source);
                source_primgroup_deleter->combine(source_group);
                source_primgroup_deleter->makeUnordered();
                source_primgroup = source_primgroup_deleter.get();

                if (!source_primgroup->isEmpty())
                {
                    // Remove primitives for which not all points they use
                    // are in source_group.
                    GA_Offset start;
                    GA_Offset end;
                    for (GA_Iterator it(source->getPrimitiveRange(source_primgroup)); it.blockAdvance(start, end); )
                    {
                        for (GA_Offset primoff = start; primoff < end; ++primoff)
                        {
                            const GA_OffsetListRef vertices = source->getPrimitiveVertexList(primoff);
                            for (exint i = 0, n = vertices.size(); i < n; ++i)
                            {
                                GA_Offset ptoff = source->vertexPoint(vertices[i]);
                                if (!source_pointgroup->contains(ptoff))
                                {
                                    source_primgroup_deleter->removeOffset(primoff);
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    notifyGroupParmListeners(cookparms.getNode(), 0, 1, source, source_group);

    GA_OffsetList source_prim_list;
    GA_OffsetList source_point_list;
    GUcreatePointOrPrimList(source_prim_list, source, source_primgroup, GA_ATTRIB_PRIMITIVE);
    GUcreatePointOrPrimList(source_point_list, source, source_pointgroup, GA_ATTRIB_POINT);

    const UT_StringHolder &target_groupname = sopparms.getTargetGroup();
    const GA_PointGroup *target_group = nullptr;
    if (target_groupname.isstring())
    {
        bool ok = true;
        // target_group can be ordered, (2nd true value).
        target_group = group_parser.parsePointDetached(target_groupname, target, true, true, ok);

        if (!ok)
            cookparms.sopAddWarning(SOP_ERR_BADGROUP, target_groupname);
    }
    notifyGroupParmListeners(cookparms.getNode(), 2, -1, target, target_group);

    GA_OffsetList target_point_list;
    if (!target_group)
        target_point_list = target->getPointMap().getOffsetFromIndexList();
    else
    {
        // Create list of points in target_group
        // TODO: Parallelize this if it's worthwhile.
        GA_Offset start;
        GA_Offset end;
        for (GA_Iterator it(target->getPointRange(target_group)); it.fullBlockAdvance(start, end); )
        {
            target_point_list.setTrivialRange(target_point_list.size(), start, end-start);
        }
    }

    // *** Primitives ***

    GA_DataId source_primlist_data_id = source->getPrimitiveList().getDataId();
    GA_DataId source_topology_data_id = source->getTopology().getDataId();
    bool source_topology_changed =
        source_primlist_data_id != sopcache->myPrevSourcePrimListDataID ||
        source_topology_data_id != sopcache->myPrevSourceTopologyDataID;
    if (!source_topology_changed)
    {
        source_topology_changed |= (source_group != nullptr) != (sopcache->myPrevHadSourceGroup);
        if (!source_topology_changed && (source_group != nullptr))
        {
            bool different_group = source_group->isDetached() ||
                (source_group->getDataId() != sopcache->myPrevSourceGroupDataID);
            if (different_group)
            {
                // Compare source group contents
                bool equal_group =
                    (source_point_list.size() == sopcache->mySourceOffsetLists[GA_ATTRIB_POINT].size()) &&
                    sopcache->mySourceOffsetLists[GA_ATTRIB_POINT].isEqual(source_point_list, 0, source_point_list.size());
                if (!equal_group)
                {
                    source_topology_changed = true;
                }
                else
                {
                    equal_group =
                        (source_prim_list.size() == sopcache->mySourceOffsetLists[GA_ATTRIB_PRIMITIVE].size()) &&
                        sopcache->mySourceOffsetLists[GA_ATTRIB_PRIMITIVE].isEqual(source_prim_list, 0, source_prim_list.size());
                    if (!equal_group)
                    {
                        source_topology_changed = true;
                    }
                }
            }
        }
    }
    sopcache->myPrevHadSourceGroup = (source_group != nullptr);
    sopcache->myPrevSourceGroupDataID = ((!source_group || source_group->isDetached()) ? GA_INVALID_DATAID : source_group->getDataId());
    sopcache->myPrevSourcePrimListDataID = source_primlist_data_id;
    sopcache->myPrevSourceTopologyDataID = source_topology_data_id;
    sopcache->mySourceOffsetLists[GA_ATTRIB_POINT] = std::move(source_point_list);
    sopcache->mySourceOffsetLists[GA_ATTRIB_PRIMITIVE] = std::move(source_prim_list);

    exint source_point_count = source_pointgroup ? source_pointgroup->entries() : source->getNumPoints();
    exint source_prim_count = source_primgroup ? source_primgroup->entries() : source->getNumPrimitives();
    exint source_vertex_count = 0;
    if (!source_primgroup)
    {
        source_vertex_count = source->getNumVertices();
        sopcache->mySourceVertexCount = source_vertex_count;
    }
    else if (source_primgroup->entries() > 0)
    {
        struct CountVerticesFunctor
        {
            CountVerticesFunctor(const GA_Detail &detail)
                : myDetail(detail)
                , myVertexCount(0)
            {}
            CountVerticesFunctor(const CountVerticesFunctor &that, UT_Split)
                : myDetail(that.myDetail)
                , myVertexCount(0)
            {}
            void operator()(const GA_SplittableRange &range)
            {
                GA_Size nvertices = 0;
                auto &primlist = myDetail.getPrimitiveList();
                GA_Offset start;
                GA_Offset end;
                for (GA_Iterator it(range); it.blockAdvance(start, end);)
                {
                    GA_PageNum pagenum = GAgetPageNum(start);
                    bool constant_page = primlist.isVertexListPageConstant(pagenum);
                    if (constant_page)
                    {
                        nvertices += primlist.getConstantVertexListPage(pagenum).size() * (end-start);
                        continue;
                    }
                    for (GA_Offset off = start; off < end; ++off)
                    {
                        nvertices += myDetail.getPrimitiveVertexCount(off);
                    }
                }
                myVertexCount += nvertices;
            }
            void join(const CountVerticesFunctor &that)
            {
                myVertexCount += that.myVertexCount;
            }
            GA_Size getVertexCount() const
            {
                return myVertexCount;
            }
            const GA_Detail &myDetail;
            exint myVertexCount;
        };
        if (source_topology_changed)
        {
            CountVerticesFunctor functor(*source);
            UTparallelReduceLightItems(GA_SplittableRange(source->getPrimitiveRange(source_primgroup)), functor);
            source_vertex_count = functor.getVertexCount();
            sopcache->mySourceVertexCount = source_vertex_count;
        }
        else
        {
            source_vertex_count = sopcache->mySourceVertexCount;
        }
    }

    // *** Target Attribute Pattern Handling ***

    SOP_CopyToPointsHDKCache::TargetAttribInfoMap target_attrib_info;
    SOP_CopyToPointsHDKCache::TargetAttribInfoMap target_group_info;
    const UT_Array<SOP_CopyToPointsHDKParms::TargetAttribs> &target_attribs = sopparms.getTargetAttribs();
    target->pointAttribs().forEachAttribute([&target_attrib_info,&target_group_info,&target_attribs](const GA_Attribute *attrib)
    {
        GA_AttributeScope scope = attrib->getScope();
        // Never copy private attributes or internal groups.
        if (scope == GA_SCOPE_PRIVATE || (scope == GA_SCOPE_GROUP && UTverify_cast<const GA_ElementGroup *>(attrib)->isInternal()))
            return;

        const UT_StringHolder &attrib_name = attrib->getName();
        // We intentionally avoid copying P, since it's always part of the transform.
        if (scope == GA_SCOPE_PUBLIC && attrib_name == GA_Names::P)
            return;

        // This wrapper is just because the multiMatch function isn't available
        // without a UT_String.  The .c_str() is so that it doesn't do a deep copy.
        const UT_String attrib_name_wrap(attrib_name.c_str());

        for (exint target_attribsi = 0, ntarget_attribs = target_attribs.size(); target_attribsi < ntarget_attribs; ++target_attribsi)
        {
            const SOP_CopyToPointsHDKParms::TargetAttribs &target_attrib_pattern = target_attribs[target_attribsi];
            if (!target_attrib_pattern.useapply)
                continue;

            const UT_StringHolder &attrib_pattern = target_attrib_pattern.applyattribs;
            if (!attrib_name_wrap.multiMatch(attrib_pattern))
                continue;

            // Keep whichever pattern matches last.

            GA_AttributeOwner output_owner;
            if (target_attrib_pattern.applyto == 0)
                output_owner = GA_ATTRIB_POINT;
            else if (target_attrib_pattern.applyto == 1)
                output_owner = GA_ATTRIB_VERTEX;
            else // target_attrib_pattern.applyto == 2
                output_owner = GA_ATTRIB_PRIMITIVE;

            sop_AttribCombineMethod method = sop_AttribCombineMethod(target_attrib_pattern.applymethod);

            if (method == sop_AttribCombineMethod::NONE)
            {
                // Remove any existing.
                if (scope == GA_SCOPE_GROUP)
                    target_group_info.erase(attrib_name);
                else
                    target_attrib_info.erase(attrib_name);
                continue;
            }

            SOP_CopyToPointsHDKCache::TargetAttribInfo &info =
                (scope == GA_SCOPE_GROUP) ?
                target_group_info[attrib_name] :
                target_attrib_info[attrib_name];

            info.myDataID = attrib->getDataId();
            info.myCopyTo = output_owner;
            info.myCombineMethod = method;

            // Only numeric or group types can multiply (intersect), add (union), or subtract
            if (!GA_ATINumeric::isType(attrib) && !GA_ElementGroup::isType(attrib) &&
                method != sop_AttribCombineMethod::COPY)
            {
                method = sop_AttribCombineMethod::COPY;
            }
        }
    });

    // Check for ID attribute case
    if (sopparms.getUseIDAttrib() && sopparms.getIDAttrib().isstring())
    {
        bool used_idattrib = sopCopyByIDAttrib(
            cookparms,
            sopparms,
            sopcache,
            output_geo,
            source,
            source_primgroup,
            source_pointgroup,
            source_topology_changed,
            target,
            target_point_list,
            target_group,
            target_attrib_info,
            target_group_info);

        if (used_idattrib)
            return;

        // ID attribute wasn't valid, so fall through.
    }

    // Not using an ID attribute, so if we previously used an ID attribute,
    // the topology must be marked as changed.
    if (GAisValid(sopcache->myTargetIDAttribDataID))
    {
        // These settings should be sufficient to make everything below
        // recook enough from scratch.  Transforms from target points
        // may still be the same.
        source_topology_changed = true;
        sopcache->myPrevOutputDetailID = -1;
        output_geo->clearAndDestroy();

        // Clear the ID attribute related caches.
        sopcache->myPieceData.clear();
        sopcache->myTargetToPiece.clear();
        sopcache->myTargetIDAttribDataID = GA_INVALID_DATAID;
        sopcache->mySourceIDAttribOwner = GA_ATTRIB_INVALID;
        sopcache->mySourceIDAttribDataID = GA_INVALID_DATAID;
        sopcache->myPieceOffsetStarts[GA_ATTRIB_VERTEX].setCapacity(0);
        sopcache->myPieceOffsetStarts[GA_ATTRIB_POINT].setCapacity(0);
        sopcache->myPieceOffsetStarts[GA_ATTRIB_PRIMITIVE].setCapacity(0);
    }

    // We must check for the target group changing *after* handling the ID
    // attribute case, since the ID attribute case needs to take into account
    // target points that don't match anything in the source.
    bool target_group_changed =
        (target_group != nullptr) != (sopcache->myPrevHadTargetGroup) ||
        (target_point_list.size() != sopcache->myPrevTargetPtCount);
    if (!target_group_changed && (target_group!= nullptr))
    {
        // For named groups, we don't need to the contents if the data ID is the same.
        bool different_group = target_group->isDetached() ||
            (target_group->getDataId() != sopcache->myPrevTargetGroupDataID);
        if (different_group)
        {
            UT_ASSERT(sopcache->myTargetOffsetList.size() == target_point_list.size());
            bool equal_group = sopcache->myTargetOffsetList.isEqual(target_point_list, 0, target_point_list.size());
            if (!equal_group)
            {
                target_group_changed = true;
            }
        }
    }
    sopcache->myTargetOffsetList = target_point_list;
    sopcache->myPrevHadTargetGroup = (target_group != nullptr);
    sopcache->myPrevTargetGroupDataID = ((!target_group || target_group->isDetached()) ? GA_INVALID_DATAID : target_group->getDataId());

    // *** Transform Setup ***

    const bool had_transform_matrices = (sopcache->myTransformMatrices3D.get() != nullptr);

    // NOTE: Transforms have changed if the target group has changed,
    //       even if the number of points is the same.
    bool transforms_changed = target_group_changed;
    GUsetupPointTransforms(sopcache, target_point_list, target, sopparms.getTransform(), sopparms.getUseImplicitN(), transforms_changed);

    const bool has_transform_matrices = (sopcache->myTransformMatrices3D.get() != nullptr);

    GA_Size num_target_points = target_point_list.size();

    if (sopparms.getPack())
    {
        const GEO_ViewportLOD lod = sopViewportLODFromParam(sopparms.getViewportLOD());
        const GU_CopyToPointsCache::PackedPivot pivot_type = sopCachePivotType(sopparms.getPivot());

        GUcopyPackAllSame(
            output_geo,
            lod,
            pivot_type,
            sopcache,
            cookparms.inputGeoHandle(0),
            source,
            source_pointgroup,
            source_primgroup,
            source_topology_changed,
            had_transform_matrices,
            transforms_changed,
            num_target_points,
            target,
            &target_point_list,
            &target_attrib_info,
            &target_group_info);

        // No attributes from source in output_geo, so we can clear the source data ID maps.
        sopcache->mySourceAttribDataIDs[GA_ATTRIB_VERTEX].clear();
        sopcache->mySourceAttribDataIDs[GA_ATTRIB_POINT].clear();
        sopcache->mySourceAttribDataIDs[GA_ATTRIB_PRIMITIVE].clear();
        sopcache->mySourceGroupDataIDs[GA_ATTRIB_VERTEX].clear();
        sopcache->mySourceGroupDataIDs[GA_ATTRIB_POINT].clear();
        sopcache->mySourceGroupDataIDs[GA_ATTRIB_PRIMITIVE].clear();
        sopcache->mySourceEdgeGroupDataIDs.clear();

        return;
    }

    // If anything that could result in different topology has changed,
    // clear the output detail, determine what to copy, and copy it.
    exint output_detail_id = output_geo->getUniqueId();
    bool topology_changed =
        (output_detail_id != sopcache->myPrevOutputDetailID) ||
        source_topology_changed ||
        (num_target_points != sopcache->myPrevTargetPtCount) ||
        sopcache->myPrevPack;
    if (topology_changed)
    {
        output_geo->clearAndDestroy();

        GUcreateVertexListAndGeometryFromSource(
            output_geo,
            source,
            source_point_count,
            source_vertex_count,
            source_prim_count,
            sopcache->mySourceOffsetLists[GA_ATTRIB_POINT],
            sopcache->mySourceOffsetLists[GA_ATTRIB_VERTEX],
            sopcache->mySourceOffsetLists[GA_ATTRIB_PRIMITIVE],
            source_pointgroup,
            source_primgroup,
            num_target_points);

        sopcache->myPrevOutputDetailID = output_detail_id;
        sopcache->myPrevTargetPtCount = num_target_points;
        sopcache->myPrevPack = false;

        sopcache->mySourceAttribDataIDs[GA_ATTRIB_VERTEX].clear();
        sopcache->mySourceAttribDataIDs[GA_ATTRIB_POINT].clear();
        sopcache->mySourceAttribDataIDs[GA_ATTRIB_PRIMITIVE].clear();
        sopcache->mySourceGroupDataIDs[GA_ATTRIB_VERTEX].clear();
        sopcache->mySourceGroupDataIDs[GA_ATTRIB_POINT].clear();
        sopcache->mySourceGroupDataIDs[GA_ATTRIB_PRIMITIVE].clear();
        sopcache->mySourceEdgeGroupDataIDs.clear();
        sopcache->myTargetAttribInfo.clear();
        sopcache->myTargetGroupInfo.clear();
    } // topology_changed

    // *** Attribute Setup ***

    // This gets complicated, with attributes from both source and target.
    // For attributes in output,
    //      if not being applied from target,
    //          if not in source or mismatches storage from source,
    //              delete it.
    //      else if being copied from target,
    //          if mismatches storage from target,
    //              delete it.
    //      else // add/sub/mul,
    //          if not in source, (copy/negate from target)
    //              if mismatches storage from target,
    //                  delete it.
    //          else if mismatches storage from source,
    //              delete it.
    // For attributes in source,
    //      if not being applied from target, or add/sub/mul,
    //          if not in output,
    //              clone from source.
    //          else,
    //              copy non-storage metadata from source.
    // For attributes being applied from target,
    //      if not in source, or being copied from target
    //          if not in output,
    //              clone from target.
    //          else,
    //              copy non-storage metadata from target.
    // For attributes in source,
    //      if not being applied from target,
    //          if source data ID changed or transforming changed,
    //              recopy.
    //      if add/sub/mul with target,
    //          if source data ID changed or transforming changed or target data ID changed or target apply method changed or target owner changed,
    //              recopy.
    //              Invalidate any cached target data ID for the attribute, so that we don't need to check source data IDs below.
    // For attributes being applied from target,
    //      if target data ID changed or target apply method changed or target owner changed,
    //          apply operation.

    if (!topology_changed)
    {
        GUremoveUnnecessaryAttribs(
            output_geo,
            source,
            target,
            sopcache,
            &target_attrib_info,
            &target_group_info);
    }

    bool needed_transforms[NeededTransforms::num_needed_transforms];
    for (exint i = 0; i < NeededTransforms::num_needed_transforms; ++i)
        needed_transforms[i] = false;

    // Add attributes from source and target that are not in output_geo.
    exint num_source_attribs[3] = {0,0,0};
    exint num_target_attribs[3] = {0,0,0};
    SYS_STATIC_ASSERT_MSG(int(GA_ATTRIB_VERTEX)<3 && int(GA_ATTRIB_POINT)<3 && int(GA_ATTRIB_PRIMITIVE)<3,
        "Arrays above are assuming the order of GA_AttributeOwner enum");

    GUaddAttributesFromSourceOrTarget(
        output_geo,
        source,
        num_source_attribs,
        has_transform_matrices,
        needed_transforms,
        target,
        &target_attrib_info,
        &target_group_info,
        num_target_attribs);

    // *** Specific Transform Caches ***

    GUcomputeTransformTypeCaches(
        sopcache,
        num_target_points,
        transforms_changed,
        needed_transforms);

    // *** Source Attribute Copying ***

    GA_SplittableRange output_splittable_ranges[3] =
    {
        GA_SplittableRange(output_geo->getVertexRange()),
        GA_SplittableRange(output_geo->getPointRange()),
        GA_SplittableRange(output_geo->getPrimitiveRange())
    };
    SYS_STATIC_ASSERT_MSG(int(GA_ATTRIB_VERTEX)==0 && int(GA_ATTRIB_POINT)==1 && int(GA_ATTRIB_PRIMITIVE)==2,
        "Arrays above and loop below are assuming the order of GA_AttributeOwner enum");

    GUcopyAttributesFromSource(
        output_geo,
        output_splittable_ranges,
        source,
        num_target_points,
        sopcache,
        sopcache->mySourceOffsetLists,
        num_source_attribs,
        false,
        had_transform_matrices,
        has_transform_matrices,
        topology_changed,
        transforms_changed,
        target,
        &target_attrib_info,
        &target_group_info);

    // *** Target Attribute Copying ***

    GUcopyAttributesFromTarget(
        output_geo,
        output_splittable_ranges,
        num_target_points,
        sopcache,
        source_point_count,
        source_vertex_count,
        source_prim_count,
        num_target_attribs,
        target_point_list,
        target,
        target_attrib_info,
        target_group_info,
        topology_changed);

    if (topology_changed)
    {
        output_geo->bumpDataIdsForAddOrRemove(true, true, true);
    }
}

const char *
SOP_CopyToPointsHDK::inputLabel(unsigned idx) const
{
    switch (idx)
    {
        case 0: return "Geometry to Copy";
        case 1: return "Target Points to Copy to";
        default: break;
    }
    return "Invalid Source";
}

int
SOP_CopyToPointsHDK::isRefInput(unsigned i) const
{
    return (i == 1); // second input
}

} // End of HDK_Sample namespace
