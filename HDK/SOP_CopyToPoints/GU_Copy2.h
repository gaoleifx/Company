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
 * Declarations of functions and structures for copying geometry.
 */

#pragma once

#ifndef __HDK_GU_Copy2_h__
#define __HDK_GU_Copy2_h__

#include <GEO/GEO_PackedTypes.h>
#include <GA/GA_AttributeInstanceMatrix.h>
#include <GA/GA_OffsetList.h>
#include <GA/GA_PolyCounts.h>
#include <GA/GA_Types.h>
#include <UT/UT_Array.h>
#include <UT/UT_ArrayStringMap.h>
#include <UT/UT_SmallArray.h>
#include <UT/UT_UniquePtr.h>
#include <UT/UT_VectorTypes.h>
#include <SYS/SYS_StaticAssert.h>
#include <SYS/SYS_Types.h>

#include <utility> // For std::pair

class GA_PointGroup;
class GA_PrimitiveGroup;
class GA_SplittableRange;
class GU_Detail;
class GU_ConstDetailHandle;

namespace HDK_Sample {

struct GU_PointTransformCache
{
    GU_PointTransformCache() :
        myTargetPDataID(GA_INVALID_DATAID),
        myTargetUsingImplicitN(false),
        myTargetPrimListDataID(GA_INVALID_DATAID),
        myTargetTopologyDataID(GA_INVALID_DATAID),
        myTransformCacheSize(0)
    {
        for (exint i = 0; i < GA_AttributeInstanceMatrix::theNumAttribs; ++i)
        {
            myTargetTransformDataIDs[i] = GA_INVALID_DATAID;
        }
    }

    void clearTransformArrays()
    {
        myTransformMatrices3F.reset();
        myTransformMatrices3D.reset();
        myTransformTranslates3F.reset();
        myTransformTranslates3D.reset();
        myTransformInverse3F.reset();
        myTransformInverse3D.reset();
        myTransformQuaternionsF.reset();
        myTransformQuaternionsD.reset();
        myTransformCacheSize = 0;
    }

    /// This is for keeping track of whether transforms have changed since the last cook.
    GA_DataId myTargetTransformDataIDs[GA_AttributeInstanceMatrix::theNumAttribs];
    GA_DataId myTargetPDataID;
    bool myTargetUsingImplicitN;
    GA_DataId myTargetPrimListDataID;
    GA_DataId myTargetTopologyDataID;

    UT_UniquePtr<UT_Matrix3F[]> myTransformMatrices3F;
    UT_UniquePtr<UT_Matrix3D[]> myTransformMatrices3D;
    UT_UniquePtr<UT_Vector3F[]> myTransformTranslates3F;
    UT_UniquePtr<UT_Vector3D[]> myTransformTranslates3D;
    UT_UniquePtr<UT_Matrix3F[]> myTransformInverse3F;
    UT_UniquePtr<UT_Matrix3D[]> myTransformInverse3D;
    UT_UniquePtr<UT_QuaternionF[]> myTransformQuaternionsF;
    UT_UniquePtr<UT_QuaternionD[]> myTransformQuaternionsD;
    exint myTransformCacheSize;
};

struct GU_CopyToPointsCache : public GU_PointTransformCache
{
    GU_CopyToPointsCache() :
        GU_PointTransformCache(),
        myPrevSourcePrimListDataID(GA_INVALID_DATAID),
        myPrevSourceTopologyDataID(GA_INVALID_DATAID),
        myPrevOutputDetailID(-1),
        myPrevTargetPtCount(-1),
        myPrevHadSourceGroup(false),
        myPrevHadTargetGroup(false),
        myPrevPack(false),
        myPrevSourceGroupDataID(GA_INVALID_DATAID),
        myPrevTargetGroupDataID(GA_INVALID_DATAID),
        myPrevSourceUniqueID(-1),
        myPrevSourceMetaCacheCount(-1),
        mySourceVertexCount(-1),
        myTargetIDAttribDataID(GA_INVALID_DATAID),
        mySourceIDAttribOwner(GA_ATTRIB_INVALID),
        mySourceIDAttribDataID(GA_INVALID_DATAID),
        myPrevPivotEnum(PackedPivot::CENTROID),
        myPrevViewportLOD(GEO_ViewportLOD::GEO_VIEWPORT_FULL)
    {}

    exint myPrevOutputDetailID;
    GA_DataId myPrevSourcePrimListDataID;
    GA_DataId myPrevSourceTopologyDataID;
    GA_Size myPrevTargetPtCount;
    bool myPrevHadSourceGroup;
    bool myPrevHadTargetGroup;
    bool myPrevPack;
    GA_DataId myPrevSourceGroupDataID;
    GA_DataId myPrevTargetGroupDataID;

    enum class PackedPivot
    {
        ORIGIN,
        CENTROID
    };

    /// These are only used when myPrevPack is true.
    exint myPrevSourceUniqueID;
    exint myPrevSourceMetaCacheCount;
    PackedPivot myPrevPivotEnum;
    GEO_ViewportLOD myPrevViewportLOD;

    exint mySourceVertexCount;
    GA_OffsetList mySourceOffsetLists[3];
    GA_OffsetList myTargetOffsetList;

    /// This is for keeping track of whether source attributes need to be re-copied.
    /// @{
    UT_ArrayStringMap<GA_DataId> mySourceAttribDataIDs[3];
    UT_ArrayStringMap<GA_DataId> mySourceGroupDataIDs[3];
    UT_ArrayStringMap<GA_DataId> mySourceEdgeGroupDataIDs;
    /// @}

    enum class AttribCombineMethod
    {
        COPY,
        NONE,
        MULTIPLY,
        ADD,
        SUBTRACT
    };
    struct TargetAttribInfo
    {
        GA_DataId myDataID;
        GA_AttributeOwner myCopyTo;
        AttribCombineMethod myCombineMethod;

        TargetAttribInfo()
            : myDataID(GA_INVALID_DATAID)
            , myCopyTo(GA_ATTRIB_POINT)
            , myCombineMethod(AttribCombineMethod::COPY)
        {}
    };
    using TargetAttribInfoMap = UT_ArrayStringMap<TargetAttribInfo>;

    /// This is for keeping track of whether target attributes need to be re-copied.
    /// @{
    TargetAttribInfoMap myTargetAttribInfo;
    TargetAttribInfoMap myTargetGroupInfo;
    /// @}

    struct PieceData
    {
        /// This is the number of target points that reference this piece.
        exint myRefCount;

        GA_OffsetList mySourceOffsetLists[3];
        SYS_STATIC_ASSERT_MSG(int(GA_ATTRIB_VERTEX)<3 && int(GA_ATTRIB_POINT)<3 && int(GA_ATTRIB_PRIMITIVE)<3,
            "Array above depends on owners other than detail being less than 3");

        /// This maps from the index into mySourceVertex to the index into mySourcePoints.
        GA_ListType<exint,exint> myRelVtxToPt;

        bool myHasSharedPoints;
        bool myHasContiguousPoints;

        GA_PolyCounts myVertexListSizeList;

        UT_SmallArray<std::pair<int,exint>, sizeof(std::pair<int,exint>)> myPrimTypeCountPairs;
        UT_SmallArray<exint, 2*sizeof(exint)> myClosedSpanLengths;
    };

    UT_Array<PieceData> myPieceData;
    UT_Array<exint> myTargetToPiece;
    UT_Array<exint> myPieceOffsetStarts[3];
    SYS_STATIC_ASSERT_MSG(int(GA_ATTRIB_VERTEX)<3 && int(GA_ATTRIB_POINT)<3 && int(GA_ATTRIB_PRIMITIVE)<3,
        "Array above depends on owners other than detail being less than 3");

    GA_DataId myTargetIDAttribDataID;
    GA_DataId mySourceIDAttribOwner;
    GA_DataId mySourceIDAttribDataID;
};

namespace GU_Copy
{

void
GUremoveUnnecessaryAttribs(
    GU_Detail *output_geo,
    const GU_Detail *source,
    const GU_Detail *target,
    GU_CopyToPointsCache *cache,
    const GU_CopyToPointsCache::TargetAttribInfoMap *target_attrib_info,
    const GU_CopyToPointsCache::TargetAttribInfoMap *target_group_info);

/// NOTE: transforms_changed must be initialized in advance to the condition
///       of whether there are external factors that lead the transforms
///       to need to be recomputed, e.g. the target point list changing from
///       the previous cook.
void
GUsetupPointTransforms(
    GU_PointTransformCache *cache,
    const GA_OffsetListRef &target_point_list,
    const GU_Detail *target,
    const bool transform_using_more_than_P,
    const bool allow_implicit_N,
    bool &transforms_changed);

namespace NeededTransforms {
enum
{
    translate3f, matrix3f, inverse3f, inverse3d, quaternionf, quaterniond, num_needed_transforms
};
}

/// Adds to output_geo any missing attributes from source or target as needed.
/// Also optionally computes what transform caches are needed for source
/// attributes.
///
/// If an attribute is in both source and target_attrib_info/target_group_info,
/// it will be taken from target if myCombineMethod is COPY, and will be taken
/// from source if MULTIPLY, ADD, or SUBTRACT.
///
/// If there is only a source, e.g. if output_geo is the internal detail of a
/// packed geometry primitive, omit target, target_attrib_info,
/// target_group_info, and num_target_attribs.
///
/// If there is only a target, pass nullptr for source, and then
/// num_source_attribs, has_transform_matrices, and needed_transforms will
/// be ignored.
///
/// NOTE: num_source_attribs or num_target_attribs, if non-null, should be an
///       array long enough to support GA_ATTRIB_VERTEX, GA_ATTRIB_POINT, and
///       GA_ATTRIB_PRIMITIVE as indices, (3 at the moment, since they come
///       first in GA_AttributeOwner).
/// NOTE: needed_transforms, if non-null, should be an array of length
///       NeededTransforms::num_needed_transforms.
void
GUaddAttributesFromSourceOrTarget(
    GU_Detail *output_geo,
    const GU_Detail *source,
    exint *num_source_attribs = nullptr,
    bool has_transform_matrices = false,
    bool *needed_transforms = nullptr,
    const GU_Detail *target = nullptr,
    GU_CopyToPointsCache::TargetAttribInfoMap *target_attrib_info = nullptr,
    GU_CopyToPointsCache::TargetAttribInfoMap *target_group_info = nullptr,
    exint *num_target_attribs = nullptr);

void
GUcomputeTransformTypeCaches(
    GU_PointTransformCache *cache,
    exint num_target_points,
    bool transforms_changed,
    const bool needed_transforms[NeededTransforms::num_needed_transforms]);

/// NOTE: This does not clear output_geo.
void
GUcreateGeometryFromSource(
    GU_Detail *output_geo,
    const GU_Detail *const source,
    const GA_OffsetList &source_point_list_cache,
    const GA_OffsetList &source_vertex_list_cache,
    const GA_OffsetList &source_prim_list_cache,
    const exint ncopies);

void
GUcreatePointOrPrimList(
    GA_OffsetList &offset_list,
    const GU_Detail *const detail,
    const GA_ElementGroup *const group,
    const GA_AttributeOwner owner);

/// NOTE: This does not clear output_geo.
/// NOTE: The point and primitive lists must be created with GUcreatePointOrPrimList.
void
GUcreateVertexListAndGeometryFromSource(
    GU_Detail *output_geo,
    const GU_Detail *const source,
    const exint source_point_count,
    const exint source_vertex_count,
    const exint source_prim_count,
    const GA_OffsetList &source_point_list_cache,
    GA_OffsetList &source_vertex_list_cache,
    const GA_OffsetList &source_prim_list_cache,
    const GA_PointGroup *const source_pointgroup,
    const GA_PrimitiveGroup *const source_primgroup,
    const exint ncopies);

void
GUcreateEmptyPackedGeometryPrims(
    GU_Detail *const output_geo,
    const exint num_packed_prims);

void
GUcopyAttributesFromSource(
    GU_Detail *const output_geo,
    const GA_SplittableRange *const output_splittable_ranges,
    const GU_Detail *const source,
    const exint num_target_points,
    GU_CopyToPointsCache *const cache,
    const GA_OffsetList *const source_offset_lists,
    const exint *const num_source_attribs,
    const bool no_transforms,
    const bool had_transform_matrices,
    const bool has_transform_matrices,
    const bool topology_changed,
    const bool transforms_changed,
    const GU_Detail *const target = nullptr,
    const GU_CopyToPointsCache::TargetAttribInfoMap *const target_attrib_info = nullptr,
    const GU_CopyToPointsCache::TargetAttribInfoMap *const target_group_info = nullptr,
    const exint *const target_to_piecei = nullptr,
    const UT_Array<exint> *const owner_piece_offset_starts = nullptr,
    const GU_CopyToPointsCache::PieceData *const piece_data = nullptr);

void
GUcopyAttributesFromTarget(
    GU_Detail *const output_geo,
    const GA_SplittableRange *const output_splittable_ranges,
    const exint ncopies,
    GU_CopyToPointsCache *const cache,
    const exint source_point_count,
    const exint source_vertex_count,
    const exint source_prim_count,
    const exint *const num_target_attribs,
    const GA_OffsetListRef &target_point_list,
    const GU_Detail *const target,
    GU_CopyToPointsCache::TargetAttribInfoMap &target_attrib_info,
    GU_CopyToPointsCache::TargetAttribInfoMap &target_group_info,
    const bool topology_changed,
    const exint *const target_to_piecei = nullptr,
    const UT_Array<exint> *const owner_piece_offset_starts = nullptr,
    const GU_CopyToPointsCache::PieceData *const piece_data = nullptr);

/// This sets the packed primitive local transforms,
/// and sets P based on the translations and pivots.
void
GUupdatePackedPrimTransforms(
    GU_Detail *output_geo,
    GU_CopyToPointsCache *cache,
    const bool had_transform_matrices,
    const exint num_packed_prims,
    const UT_Vector3 *const constant_pivot);

/// This sets the packed primitive local transforms,
/// sets P based on the translations and pivots,
/// (i.e. it first calls GUupdatePackedPrimTransforms),
/// then removes unnecessary attributes, adds needed attributes from target,
/// and copies target attribute values.
///
/// NOTE: constant_pivot being null means to get pivots from the primitives.
void
GUhandleTargetAttribsForPackedPrims(
    GU_Detail *output_geo,
    GU_CopyToPointsCache *cache,
    const bool topology_changed,
    const bool had_transform_matrices,
    const GU_Detail *const target,
    const GA_OffsetListRef &target_point_list,
    GU_CopyToPointsCache::TargetAttribInfoMap &target_attrib_info,
    GU_CopyToPointsCache::TargetAttribInfoMap &target_group_info,
    const UT_Vector3 *const constant_pivot);

/// Uses the transforms in cache and geometry from source
/// to create num_packed_prims packed geometry primitives referencing
/// the same geometry.  If the source groups are null, source_handle
/// will be used to create the packed geometry primitives.
///
/// If target and the later parameters are non-null, target attributes
/// in target_attrib_info and target_group_info will be copied from
/// the points in target_point_list.
///
/// NOTE: This will clear output_geo if the topology has changed since
///       the previous cook.
void
GUcopyPackAllSame(
    GU_Detail *output_geo,
    const GEO_ViewportLOD lod,
    const GU_CopyToPointsCache::PackedPivot pivot_type,
    GU_CopyToPointsCache *cache,
    const GU_ConstDetailHandle source_handle,
    const GU_Detail *source,
    const GA_PointGroup *source_pointgroup,
    const GA_PrimitiveGroup *source_primgroup,
    bool source_topology_changed,
    bool had_transform_matrices,
    bool transforms_changed,
    const exint num_packed_prims,
    const GU_Detail *target = nullptr,
    const GA_OffsetListRef *target_point_list = nullptr,
    GU_CopyToPointsCache::TargetAttribInfoMap *target_attrib_info = nullptr,
    GU_CopyToPointsCache::TargetAttribInfoMap *target_group_info = nullptr);


} // namespace GU_Copy

} // End of HDK_Sample namespace

namespace UT {
template <typename T>
struct DefaultClearer;

template<>
struct DefaultClearer<HDK_Sample::GU_CopyToPointsCache::TargetAttribInfo>
{
    SYS_FORCE_INLINE
        static void clear(HDK_Sample::GU_CopyToPointsCache::TargetAttribInfo &v)
    {
        v.myDataID = GA_INVALID_DATAID;
        v.myCopyTo = GA_ATTRIB_POINT;
        v.myCombineMethod = HDK_Sample::GU_CopyToPointsCache::AttribCombineMethod::COPY;
    }
    SYS_FORCE_INLINE
        static bool isClear(const HDK_Sample::GU_CopyToPointsCache::TargetAttribInfo &v)
    {
        return
            v.myDataID == GA_INVALID_DATAID &&
            v.myCopyTo == GA_ATTRIB_POINT &&
            v.myCombineMethod == HDK_Sample::GU_CopyToPointsCache::AttribCombineMethod::COPY;
    }
    SYS_FORCE_INLINE
        static void clearConstruct(HDK_Sample::GU_CopyToPointsCache::TargetAttribInfo *p)
    {
        new ((void *)p) HDK_Sample::GU_CopyToPointsCache::TargetAttribInfo();
    }
    static const bool clearNeedsDestruction = false;
};
}

namespace UT {
template <typename T>
struct DefaultClearer;

// FIXME: Move this specialization to GA_OffsetList.h and template it for all GA_ListType and GA_ListTypeRef!
template<>
struct DefaultClearer<GA_OffsetList>
{
    SYS_FORCE_INLINE
        static void clear(GA_OffsetList &v)
    {
        v.clear();
    }
    SYS_FORCE_INLINE
        static bool isClear(const GA_OffsetList &v)
    {
        return v.size() == 0 && v.isTrivial() && v.trivialStart() == GA_Offset(0) && !v.getExtraFlag();
    }
    SYS_FORCE_INLINE
        static void clearConstruct(GA_OffsetList *p)
    {
        new ((void *)p) GA_OffsetList();
    }
    static const bool clearNeedsDestruction = false;
};
}

#endif
