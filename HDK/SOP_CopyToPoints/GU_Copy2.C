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
 * Definitions of functions and structures for copying geometry.
 */

#include "GU_Copy2.h"

#include "GEO_BuildPrimitives.h"

#include <GU/GU_Detail.h>
#include <GU/GU_DetailHandle.h>
#include <GU/GU_PackedGeometry.h>
#include <GU/GU_PrimPacked.h>
#include <GEO/GEO_Normal.h>
#include <GEO/GEO_PackedTypes.h>
#include <GEO/GEO_ParallelWiringUtil.h>
#include <GA/GA_ATINumeric.h>
#include <GA/GA_ATITopology.h>
#include <GA/GA_Attribute.h>
#include <GA/GA_AttributeDict.h>
#include <GA/GA_AttributeSet.h>
#include <GA/GA_AttributeInstanceMatrix.h>
#include <GA/GA_Edge.h>
#include <GA/GA_EdgeGroup.h>
#include <GA/GA_ElementGroup.h>
#include <GA/GA_ElementGroupTable.h>
#include <GA/GA_Handle.h>
#include <GA/GA_Iterator.h>
#include <GA/GA_OffsetList.h>
#include <GA/GA_PageArray.h>
#include <GA/GA_PolyCounts.h>
#include <GA/GA_Primitive.h>
#include <GA/GA_PrimitiveTypes.h>
#include <GA/GA_Range.h>
#include <GA/GA_RTIOffsetList.h>
#include <GA/GA_SplittableRange.h>
#include <GA/GA_Types.h>
#include <UT/UT_Array.h>
#include <UT/UT_ArrayStringMap.h>
#include <UT/UT_Assert.h>
#include <UT/UT_Matrix3.h>
#include <UT/UT_Matrix4.h>
#include <UT/UT_PageArray.h>
#include <UT/UT_PageArrayImpl.h>
#include <UT/UT_Quaternion.h>
#include <UT/UT_Vector3.h>
#include <UT/UT_ParallelUtil.h>
#include <UT/UT_SmallArray.h>
#include <UT/UT_StringHolder.h>
#include <UT/UT_UniquePtr.h>
#include <UT/UT_VectorTypes.h>
#include <SYS/SYS_StaticAssert.h>
#include <SYS/SYS_Types.h>

#include <algorithm>    // For std::upper_bound
#include <utility>      // For std::pair

namespace HDK_Sample {

namespace GU_Copy {

SYS_FORCE_INLINE
static GA_AttributeOwner
guConflictAttribOwner(GA_AttributeOwner owner)
{
    if (owner == GA_ATTRIB_POINT)
        return GA_ATTRIB_VERTEX;
    if (owner == GA_ATTRIB_VERTEX)
        return GA_ATTRIB_POINT;
    return GA_ATTRIB_INVALID;
}

static GA_TypeInfo
guGetTransformTypeInfo(const GA_ATINumeric *attrib, const bool has_transform_matrices)
{
    int tuple_size = attrib->getTupleSize();
    if (tuple_size < 3 || !attrib->needsTransform())
        return GA_TYPE_VOID;

    GA_TypeInfo attrib_type_info = attrib->getTypeInfo();
    if (tuple_size == 3)
    {
        // Vectors and normals don't react to translations.
        if (attrib_type_info == GA_TYPE_POINT ||
            (has_transform_matrices && (attrib_type_info == GA_TYPE_VECTOR || attrib_type_info == GA_TYPE_NORMAL)))
        {
            return attrib_type_info;
        }
    }
    else if (tuple_size == 4)
    {
        // Quaternions don't react to translations.
        if ((has_transform_matrices && attrib_type_info == GA_TYPE_QUATERNION) || attrib_type_info == GA_TYPE_HPOINT)
            return attrib_type_info;
    }
    else if (tuple_size == 9)
    {
        // 3x3 matrices don't react to translations.
        if (has_transform_matrices && attrib_type_info == GA_TYPE_TRANSFORM)
            return attrib_type_info;
    }
    else if (tuple_size == 16)
    {
        if (attrib_type_info == GA_TYPE_TRANSFORM)
            return attrib_type_info;
    }
    return GA_TYPE_VOID;
}

void
GUremoveUnnecessaryAttribs(
    GU_Detail *output_geo,
    const GU_Detail *source,
    const GU_Detail *target,
    GU_CopyToPointsCache *cache,
    const GU_CopyToPointsCache::TargetAttribInfoMap *target_attrib_info,
    const GU_CopyToPointsCache::TargetAttribInfoMap *target_group_info)
{
    // Remove attributes from previous cook that are not present in source
    // or that mismatch the type in source.
    UT_SmallArray<GA_Attribute*> attribs_to_delete;
    for (int owneri = 0; owneri < GA_ATTRIB_OWNER_N; ++owneri)
    {
        attribs_to_delete.clear();
        bool pos_storage_mismatch = false;
        GA_AttributeOwner owner = GA_AttributeOwner(owneri);
        output_geo->getAttributeDict(owner).forEachAttribute([source,owner,
            &attribs_to_delete,output_geo,&pos_storage_mismatch,
            target_group_info,target_attrib_info,target,cache](GA_Attribute *attrib)
        {
            const GA_AttributeScope scope = attrib->getScope();
            const bool is_group = (scope == GA_SCOPE_GROUP);
            if (scope == GA_SCOPE_PRIVATE ||
                (is_group && UTverify_cast<GA_ElementGroup*>(attrib)->isInternal()))
            {
                // Don't delete topology attributes.
                if (!GA_ATITopology::isType(attrib))
                    attribs_to_delete.append(attrib);
                return;
            }
            const UT_StringHolder &name = attrib->getName();
            const GA_Attribute *source_attrib = source ? source->findAttribute(owner, scope, name) : nullptr;
            const GU_CopyToPointsCache::TargetAttribInfoMap *target_info = is_group ? target_group_info : target_attrib_info;
            GU_CopyToPointsCache::TargetAttribInfoMap::const_iterator it;
            if (target_info)
                it = target_info->find(name);
            if (!target_info || it.atEnd() || it->second.myCopyTo != owner)
            {
                if (!source_attrib || !source_attrib->matchesStorage(attrib))
                {
                    // Be careful with P, since we can't delete it.
                    if (owner == GA_ATTRIB_POINT && attrib == output_geo->getP())
                        pos_storage_mismatch = (source_attrib != nullptr);
                    else
                        attribs_to_delete.append(attrib);
                }
                else // if (source) This is a redundant check, since if !source, source_attrib is null.
                {
                    // If there was previously not a source attribute, this was
                    // a target attribute, so to reduce the risk of data ID
                    // havoc, since this is an uncommon case, we just delete.
                    // NOTE: The check for non-null source is because the source
                    //       data IDs are irrelevant if they were just copied
                    //       into a packed primitive.
                    // NOTE: Don't delete P.  (It won't be in the cache on the first cook.)
                    auto *source_dataids = is_group ? cache->mySourceGroupDataIDs : cache->mySourceAttribDataIDs;
                    if (!source_dataids[owner].contains(name) && attrib != output_geo->getP())
                    {
                        attribs_to_delete.append(attrib);
                    }
                }
            }
            else if (it->second.myCombineMethod == GU_CopyToPointsCache::AttribCombineMethod::COPY || !source_attrib)
            {
                // NOTE: P is never applied from target, so we don't need the
                //       special case here or in the next case.
                const GA_Attribute *target_attrib = target->findAttribute(GA_ATTRIB_POINT, scope, name);
                UT_ASSERT(target_attrib);
                if (!target_attrib->matchesStorage(attrib))
                    attribs_to_delete.append(attrib);
                else if (source)
                {
                    // If we previously cloned from a source attribute,
                    // (we're now going to be cloning from a target attribute),
                    // to avoid data ID havoc, we delete.
                    // Without this case, there were problems where a target
                    // attribute being combined with a source attribute
                    // wasn't getting its data ID bumped or updating
                    // properly when the source attribute was no longer
                    // in the source on the next cook.
                    // NOTE: The check for non-null source is because the source
                    //       data IDs are irrelevant if they were just copied
                    //       into a packed primitive.
                    auto *source_dataids = is_group ? cache->mySourceGroupDataIDs : cache->mySourceAttribDataIDs;
                    if (source_dataids[owner].contains(name))
                    {
                        attribs_to_delete.append(attrib);
                    }
                }
            }
            else if (!source_attrib->matchesStorage(attrib))
            {
                attribs_to_delete.append(attrib);
            }
            else // if (source) This is a redundant check, since if !source, source_attrib is null.
            {
                // If there was previously not a source attribute, this was
                // a target attribute, so to reduce the risk of data ID
                // havoc, since this is an uncommon case, we just delete.
                // NOTE: The check for non-null source is because the source
                //       data IDs are irrelevant if they were just copied
                //       into a packed primitive.
                auto *source_dataids = is_group ? cache->mySourceGroupDataIDs : cache->mySourceAttribDataIDs;
                if (!source_dataids[owner].contains(name))
                {
                    attribs_to_delete.append(attrib);
                }
            }
        });

        for (exint i = 0, n = attribs_to_delete.size(); i < n; ++i)
        {
            const UT_StringHolder &name = attribs_to_delete[i]->getName();

            // Remove it from any data ID caches before deleting it,
            // else we'll need to get a full UT_StringHolder to keep
            // the name in scope.
            if (attribs_to_delete[i]->getScope() == GA_SCOPE_GROUP)
            {
                cache->mySourceGroupDataIDs[owneri].erase(name);
                cache->myTargetGroupInfo.erase(name);
                output_geo->destroyElementGroup(owner, name);
            }
            else
            {
                cache->mySourceAttribDataIDs[owneri].erase(name);
                cache->myTargetAttribInfo.erase(name);
                output_geo->destroyAttribute(owner, name);
            }

        }

        if (pos_storage_mismatch)
        {
            // Separate handling for P, since we can't delete it.
            UTverify_cast<GA_ATINumeric*>(output_geo->getP())->setStorage(
                UTverify_cast<const GA_ATINumeric*>(source->getP())->getStorage());

            cache->mySourceAttribDataIDs[GA_ATTRIB_POINT].erase(source->getP()->getName());
        }
    }

    // Remove edge groups from previous cook that are not present in source.
    UT_SmallArray<GA_EdgeGroup*> edgegroups_to_delete;
    for (auto it = output_geo->edgeGroups().beginTraverse(); !it.atEnd(); ++it)
    {
        GA_EdgeGroup *edgegroup = it.group();
        if (edgegroup->isInternal())
        {
            edgegroups_to_delete.append(edgegroup);
            continue;
        }
        const GA_EdgeGroup *source_edgegroup = source ? source->findEdgeGroup(edgegroup->getName()) : nullptr;
        if (!source_edgegroup || source_edgegroup->isInternal())
        {
            edgegroups_to_delete.append(edgegroup);
        }
    }
    for (exint i = 0, n = edgegroups_to_delete.size(); i < n; ++i)
    {
        const UT_StringHolder &name = edgegroups_to_delete[i]->getName();
        output_geo->destroyEdgeGroup(name);
        cache->mySourceEdgeGroupDataIDs.erase(name);
    }
}

void
GUsetupPointTransforms(
    GU_PointTransformCache *cache,
    const GA_OffsetListRef &target_point_list,
    const GU_Detail *target,
    const bool transform_using_more_than_P,
    const bool allow_implicit_N,
    bool &transforms_changed)
{
    const exint num_target_points = target_point_list.size();
    if (cache->myTransformCacheSize > 0 && num_target_points != cache->myTransformCacheSize)
    {
        UT_ASSERT(transforms_changed);
        transforms_changed = true;
        cache->clearTransformArrays();
    }
    if (num_target_points == 0)
    {
        UT_ASSERT(cache->myTransformCacheSize == 0);
        return;
    }

    GA_AttributeInstanceMatrix target_transform_attribs;
    bool using_implicit_N = false;
    if (transform_using_more_than_P)
    {
        target_transform_attribs.initialize(target->pointAttribs());
        if (!target_transform_attribs.getN().isValid() && allow_implicit_N && target->getNumPrimitives() != 0)
        {
            using_implicit_N = true;
        }
    }
    transforms_changed |= (using_implicit_N != cache->myTargetUsingImplicitN);
    cache->myTargetUsingImplicitN = using_implicit_N;

    if (using_implicit_N)
    {
        // Implicit normals depend on the primitive list and the topology,
        // (also depend on P, checked below), so check data IDs.
        GA_DataId primlist_dataid = target->getPrimitiveList().getDataId();
        GA_DataId topology_dataid = target->getTopology().getDataId();
        transforms_changed |=
            primlist_dataid != cache->myTargetPrimListDataID ||
            topology_dataid != cache->myTargetTopologyDataID;
        cache->myTargetPrimListDataID = primlist_dataid;
        cache->myTargetTopologyDataID = topology_dataid;
    }
    else
    {
        cache->myTargetPrimListDataID = GA_INVALID_DATAID;
        cache->myTargetTopologyDataID = GA_INVALID_DATAID;
    }

    GA_DataId new_transform_data_ids[GA_AttributeInstanceMatrix::theNumAttribs];
    target_transform_attribs.getDataIds(new_transform_data_ids);
    transforms_changed |=
        target->getP()->getDataId() == GA_INVALID_DATAID ||
        cache->myTargetPDataID == GA_INVALID_DATAID ||
        target->getP()->getDataId() != cache->myTargetPDataID;
    if (!transforms_changed)
    {
        for (exint i = 0; i < GA_AttributeInstanceMatrix::theNumAttribs; ++i)
        {
            // NOTE: GA_AttributeInstanceMatrix uses a different value to
            //       indicate that an attribute is not present, so this
            //       check still supports missing attributes.
            if (new_transform_data_ids[i] == GA_INVALID_DATAID ||
                cache->myTargetTransformDataIDs[i] == GA_INVALID_DATAID ||
                new_transform_data_ids[i] != cache->myTargetTransformDataIDs[i])
            {
                transforms_changed = true;
                break;
            }
        }
    }
    if (transforms_changed)
    {
        //cache->myTransforming = true;
        memcpy(cache->myTargetTransformDataIDs, new_transform_data_ids, GA_AttributeInstanceMatrix::theNumAttribs*sizeof(GA_DataId));
        cache->myTargetPDataID = target->getP()->getDataId();

        // We always cache the full transform in double-precision,
        // in case on future cooks, it's needed for new source attributes,
        // when !transforms_changed.
        bool onlyP = !target_transform_attribs.hasAnyAttribs() && !using_implicit_N;
        if (!onlyP && !cache->myTransformMatrices3D)
            cache->myTransformMatrices3D.reset(new UT_Matrix3D[num_target_points]);
        if (!cache->myTransformTranslates3D)
            cache->myTransformTranslates3D.reset(new UT_Vector3D[num_target_points]);
        cache->myTransformCacheSize = num_target_points;

        // Recompute and cache needed transforms
        const GA_ROHandleV3D targetP(target->getP());
        if (onlyP)
        {
            cache->myTransformMatrices3D.reset();
            UT_Vector3D *translates = cache->myTransformTranslates3D.get();
            UT_ASSERT(targetP.isValid());
            auto &&functor = [&target_point_list,&targetP,translates](const UT_BlockedRange<exint> &r)
            {
                for (exint i = r.begin(), end = r.end(); i < end; ++i)
                {
                    GA_Offset target_ptoff = target_point_list[i];
                    translates[i] = targetP.get(target_ptoff);
                }
            };
            if (num_target_points > 1024)
                UTparallelFor(UT_BlockedRange<exint>(0, num_target_points), functor, 2, 512);
            else
                functor(UT_BlockedRange<exint>(0, num_target_points));
        }
        else
        {
            // Implicit N doesn't need to be in cache, since we cache
            // the transforms themselves.
            // If things like pscale are changing and not P or the primitive
            // list or topology, it might be worth caching the implicit N,
            // but that's probably an uncommon edge case to optimize for.
            GA_AttributeUPtr implicitN;
            if (using_implicit_N)
            {
                implicitN = target->createDetachedTupleAttribute(GA_ATTRIB_POINT, GA_STORE_REAL32, 3);
                GA_RWHandleV3 implicitN_h(implicitN.get());

                // Compute implicit normals based on P and the primitives in target.
                GEOcomputeNormals(*target, implicitN_h);
                target_transform_attribs.setN(implicitN.get());
            }

            UT_Matrix3D *matrices = cache->myTransformMatrices3D.get();
            UT_Vector3D *translates = cache->myTransformTranslates3D.get();
            UT_ASSERT(targetP.isValid());
            auto &&functor = [&target_transform_attribs,&target_point_list,&targetP,matrices,translates](const UT_BlockedRange<exint> &r)
            {
                for (exint i = r.begin(), end = r.end(); i < end; ++i)
                {
                    GA_Offset target_ptoff = target_point_list[i];
                    UT_Matrix4D transform;
                    target_transform_attribs.getMatrix(transform, targetP.get(target_ptoff), target_ptoff);

                    // Save transform in matrices and translates
                    matrices[i] = UT_Matrix3D(transform);
                    transform.getTranslates(translates[i]);
                }
            };
            if (num_target_points > 512)
                UTparallelFor(UT_BlockedRange<exint>(0, num_target_points), functor, 2, 256);
            else
                functor(UT_BlockedRange<exint>(0, num_target_points));
        }
    }
}

void
GUaddAttributesFromSourceOrTarget(
    GU_Detail *output_geo,
    const GU_Detail *source,
    exint *num_source_attribs,
    bool has_transform_matrices,
    bool *needed_transforms,
    const GU_Detail *target,
    GU_CopyToPointsCache::TargetAttribInfoMap *target_attrib_info,
    GU_CopyToPointsCache::TargetAttribInfoMap *target_group_info,
    exint *num_target_attribs)
{
    using AttribCombineMethod = GU_CopyToPointsCache::AttribCombineMethod;

    for (int owneri = source ? 0 : GA_ATTRIB_OWNER_N; owneri < GA_ATTRIB_OWNER_N; ++owneri)
    {
        GA_AttributeOwner owner = GA_AttributeOwner(owneri);
        source->getAttributeDict(owner).forEachAttribute(GA_SCOPE_PUBLIC,
            [owner,output_geo,num_source_attribs,
            needed_transforms,has_transform_matrices,
            target_attrib_info](const GA_Attribute *source_attrib)
        {
            const UT_StringHolder &name = source_attrib->getName();
            if (target_attrib_info)
            {
                // If copying from target, skip, since it'll be added from target.
                // Target attributes take precedence over source attributes,
                // because users can always remove the target attributes from
                // the pattern parameters, but they don't have control over the
                // source attributes in this node.
                // NOTE: Point and vertex attributes of the same name are not allowed,
                //       so we check for both.
                auto it = target_attrib_info->find(name);
                if (!it.atEnd() && (it->second.myCopyTo == owner || it->second.myCopyTo == guConflictAttribOwner(owner)) &&
                    it->second.myCombineMethod == AttribCombineMethod::COPY)
                    return;
            }

            GA_Attribute *dest_attrib = output_geo->findAttribute(owner, name);
            UT_ASSERT(!dest_attrib || dest_attrib->matchesStorage(source_attrib));
            if (!dest_attrib)
            {
                dest_attrib = output_geo->getAttributes().cloneAttribute(owner, name, GA_AttributeSet::namevalidcertificate(), *source_attrib,
                    true, (owner == GA_ATTRIB_DETAIL) ? GA_DATA_ID_CLONE : GA_DATA_ID_BUMP);
            }

            // Copy detail attributes immediately
            if (owner == GA_ATTRIB_DETAIL)
            {
                dest_attrib->replace(*source_attrib);
            }
            else
            {
                if (num_source_attribs)
                    ++num_source_attribs[owner];

                // Just copy non-storage metadata for the rest; (storage type already matches).
                dest_attrib->copyNonStorageMetadata(source_attrib);

                if (!needed_transforms)
                    return;

                GA_ATINumeric *dest_numeric = GA_ATINumeric::cast(dest_attrib);
                if (dest_numeric)
                {
                    GA_TypeInfo transform_type = guGetTransformTypeInfo(dest_numeric, has_transform_matrices);
                    if (transform_type != GA_TYPE_VOID)
                    {
                        using namespace NeededTransforms;
                        bool double_precision = (dest_numeric->getStorage() == GA_STORE_REAL64);
                        if (transform_type == GA_TYPE_POINT || transform_type == GA_TYPE_HPOINT)
                        {
                            needed_transforms[matrix3f] |= !double_precision;
                            needed_transforms[translate3f] |= !double_precision;
                        }
                        else if (transform_type == GA_TYPE_VECTOR)
                        {
                            needed_transforms[matrix3f] |= !double_precision;
                        }
                        else if (transform_type == GA_TYPE_NORMAL)
                        {
                            needed_transforms[inverse3d] |= double_precision;
                            needed_transforms[inverse3f] |= !double_precision;
                        }
                        else if (transform_type == GA_TYPE_QUATERNION)
                        {
                            needed_transforms[quaterniond] |= double_precision;
                            needed_transforms[quaternionf] |= !double_precision;
                        }
                        else if (transform_type == GA_TYPE_TRANSFORM)
                        {
                            needed_transforms[matrix3f] |= !double_precision;
                            needed_transforms[translate3f] |= !double_precision && (dest_numeric->getTupleSize() == 16);
                        }
                    }
                }
            }
        });

        if (owner != GA_ATTRIB_DETAIL)
        {
            // Now for the element groups
            for (auto it = source->getElementGroupTable(owner).beginTraverse(); !it.atEnd(); ++it)
            {
                const GA_ElementGroup *source_group = it.group();
                if (source_group->isInternal())
                    continue;

                if (target_group_info)
                {
                    // If copying from target, skip, since it'll be added from target.
                    auto it = target_group_info->find(source_group->getName());
                    if (!it.atEnd() && it->second.myCopyTo == owner)
                        return;
                }

                GA_ElementGroup *dest_group = output_geo->findElementGroup(owner, source_group->getName());
                if (!dest_group)
                {
                    dest_group = UTverify_cast<GA_ElementGroup *>(output_geo->getElementGroupTable(owner).newGroup(source_group->getName()));
                    UT_ASSERT_MSG(!dest_group->isOrdered(), "Writing to groups in parallel requires unordered groups, and ordering isn't as useful for copied geometry");
                }
                else if (dest_group->isOrdered())
                {
                    dest_group->clearOrdered();
                }
                if (num_source_attribs)
                    ++num_source_attribs[owner];
            }
        }
    }

    // Add edge groups from source that are not in output_geo.
    if (source)
    {
        for (auto it = source->edgeGroups().beginTraverse(); !it.atEnd(); ++it)
        {
            const GA_EdgeGroup *source_group = it.group();
            if (source_group->isInternal())
                continue;
            GA_EdgeGroup *dest_group = output_geo->findEdgeGroup(source_group->getName());
            if (!dest_group)
            {
                dest_group = UTverify_cast<GA_EdgeGroup *>(output_geo->edgeGroups().newGroup(source_group->getName()));
            }
        }
    }

    if (!target || !target_attrib_info || !target_group_info)
        return;

    for (auto it = target_attrib_info->begin(); !it.atEnd(); ++it)
    {
        const UT_StringHolder &name = it->first;
        GA_AttributeOwner output_owner = it->second.myCopyTo;
        AttribCombineMethod method = it->second.myCombineMethod;
        if (source && method != AttribCombineMethod::COPY)
        {
            // If source has the attribute and the method isn't copying from target,
            // we've already cloned the attribute above, so skip.
            const GA_Attribute *source_attrib = source->findAttribute(output_owner, name);
            if (source_attrib)
            {
                if (num_target_attribs)
                    ++num_target_attribs[output_owner];
                continue;
            }
            // NOTE: Point and vertex attributes of the same name are not allowed,
            //       so we check for both.
            GA_AttributeOwner conflict_owner = guConflictAttribOwner(output_owner);
            if (conflict_owner != GA_ATTRIB_INVALID)
            {
                source_attrib = source->findAttribute(conflict_owner, name);
                if (source_attrib)
                {
                    // For simplicity, instead of trying to promote to the specified target type,
                    // just stick with the type in source.
                    // TODO: Maybe for completeness in the future we should promote,
                    //       but the partial cooking case checking gets very complicated,
                    //       so I'm not adding that right now.
                    it->second.myCopyTo = conflict_owner;
                    if (num_target_attribs)
                        ++num_target_attribs[conflict_owner];
                    continue;
                }
            }
        }

        const GA_Attribute *target_attrib = target->findAttribute(GA_ATTRIB_POINT, name);
        GA_Attribute *dest_attrib = output_geo->findAttribute(output_owner, name);
        UT_ASSERT(!dest_attrib || dest_attrib->matchesStorage(target_attrib));
        if (!dest_attrib)
        {
            dest_attrib = output_geo->getAttributes().cloneAttribute(output_owner, name, GA_AttributeSet::namevalidcertificate(), *target_attrib,
                true, GA_DATA_ID_BUMP);

            // We want multiplying with no attribute to be equivalent to copying,
            // and adding to no attribute is automatically equivalent to copying,
            // so it's easiest to just change it to copy here.
            // Subtracting is different, but equivalent to subtracting from zero,
            // so we can leave it as is.
            if (method == AttribCombineMethod::MULTIPLY ||
                method == AttribCombineMethod::ADD)
            {
                it->second.myCombineMethod = AttribCombineMethod::COPY;
            }
        }
        UT_ASSERT(dest_attrib != nullptr);

        // Just copy non-storage metadata for the rest; (storage type already matches).
        dest_attrib->copyNonStorageMetadata(target_attrib);

        if (num_target_attribs)
            ++num_target_attribs[output_owner];
    }

    for (auto it = target_group_info->begin(); !it.atEnd(); ++it)
    {
        const UT_StringHolder &name = it->first;
        GA_AttributeOwner output_owner = it->second.myCopyTo;
        AttribCombineMethod method = it->second.myCombineMethod;
        if (source && method != AttribCombineMethod::COPY)
        {
            // If source has the group and the method isn't copying from target,
            // we've already cloned the group above, so skip.
            const GA_ElementGroup *source_group = source->findElementGroup(output_owner, name);
            if (source_group)
            {
                if (num_target_attribs)
                    ++num_target_attribs[output_owner];
                continue;
            }
            // NOTE: Point and vertex attributes of the same name are not allowed,
            //       so we check for both.
            GA_AttributeOwner conflict_owner = guConflictAttribOwner(output_owner);
            if (conflict_owner != GA_ATTRIB_INVALID)
            {
                source_group = source->findElementGroup(conflict_owner, name);
                if (source_group)
                {
                    // For simplicity, instead of trying to promote to the specified target type,
                    // just stick with the type in source.
                    // TODO: Maybe for completeness in the future we should promote,
                    //       but the partial cooking case checking gets very complicated,
                    //       so I'm not adding that right now.
                    it->second.myCopyTo = conflict_owner;
                    if (num_target_attribs)
                        ++num_target_attribs[conflict_owner];
                    continue;
                }
            }
        }

        UT_ASSERT(target->findPointGroup(name) != nullptr);
        GA_ElementGroup *dest_group = output_geo->findElementGroup(output_owner, name);
        if (!dest_group)
        {
            dest_group = UTverify_cast<GA_ElementGroup *>(output_geo->getElementGroupTable(output_owner).newGroup(name));

            // We want intersecting with no group to be equivalent to copying,
            // and unioning with no group is automatically equivalent to copying,
            // so it's easiest to just change it to copy here.
            if (method == AttribCombineMethod::MULTIPLY ||
                method == AttribCombineMethod::ADD)
            {
                it->second.myCombineMethod = AttribCombineMethod::COPY;
            }
            // Subtracting from no group is equivalent to doing nothing,
            // (apart from ensuring that the group exists).
            if (method == AttribCombineMethod::SUBTRACT)
            {
                it->second.myCombineMethod = AttribCombineMethod::NONE;
            }
        }
        UT_ASSERT_MSG(!dest_group->isOrdered(), "Writing to groups in parallel requires unordered groups, and ordering isn't as useful for copied geometry");

        if (num_target_attribs)
            ++num_target_attribs[output_owner];
    }
}

void
GUcomputeTransformTypeCaches(
    GU_PointTransformCache *cache,
    exint num_target_points,
    bool transforms_changed,
    const bool needed_transforms[NeededTransforms::num_needed_transforms])
{
    using namespace NeededTransforms;

    bool has_transform_matrices = (cache->myTransformMatrices3D.get() != nullptr);
    if (needed_transforms[translate3f] && num_target_points > 0)
    {
        bool compute = transforms_changed;
        if (!cache->myTransformTranslates3F)
        {
            cache->myTransformTranslates3F.reset(new UT_Vector3F[num_target_points]);
            compute = true;
        }
        if (compute)
        {
            const UT_Vector3D *vector3d = cache->myTransformTranslates3D.get();
            UT_Vector3F *vector3f = cache->myTransformTranslates3F.get();
            auto &&functor = [vector3d,vector3f](const UT_BlockedRange<exint> &r)
            {
                for (exint i = r.begin(), end = r.end(); i < end; ++i)
                {
                    vector3f[i] = UT_Vector3F(vector3d[i]);
                }
            };
            if (num_target_points > 1024)
                UTparallelFor(UT_BlockedRange<exint>(0, num_target_points), functor, 2, 512);
            else
                functor(UT_BlockedRange<exint>(0, num_target_points));
        }
    }

    if (!has_transform_matrices)
    {
        // Only translates, so no matrices, inverses, or quaternions
        cache->myTransformMatrices3F.reset();
        cache->myTransformInverse3F.reset();
        cache->myTransformInverse3D.reset();
        cache->myTransformQuaternionsF.reset();
        cache->myTransformQuaternionsD.reset();
        return;
    }
    if (num_target_points <= 0)
        return;

    if (needed_transforms[matrix3f])
    {
        bool compute = transforms_changed;
        if (!cache->myTransformMatrices3F)
        {
            cache->myTransformMatrices3F.reset(new UT_Matrix3F[num_target_points]);
            compute = true;
        }
        if (compute)
        {
            const UT_Matrix3D *matrices3d = cache->myTransformMatrices3D.get();
            UT_Matrix3F *matrices3f = cache->myTransformMatrices3F.get();
            auto &&functor = [matrices3d,matrices3f](const UT_BlockedRange<exint> &r)
            {
                for (exint i = r.begin(), end = r.end(); i < end; ++i)
                {
                    matrices3f[i] = UT_Matrix3F(matrices3d[i]);
                }
            };
            if (num_target_points > 1024)
                UTparallelFor(UT_BlockedRange<exint>(0, num_target_points), functor, 2, 512);
            else
                functor(UT_BlockedRange<exint>(0, num_target_points));
        }
    }
    if (needed_transforms[inverse3f] || needed_transforms[inverse3d])
    {
        bool compute = transforms_changed;
        if (needed_transforms[inverse3f] && !cache->myTransformInverse3F)
        {
            cache->myTransformInverse3F.reset(new UT_Matrix3F[num_target_points]);
            compute = true;
        }
        if (needed_transforms[inverse3d] && !cache->myTransformInverse3D)
        {
            cache->myTransformInverse3D.reset(new UT_Matrix3D[num_target_points]);
            compute = true;
        }
        if (compute)
        {
            const UT_Matrix3D *matrices3d = cache->myTransformMatrices3D.get();
            UT_Matrix3D *inverses3d = cache->myTransformInverse3D.get();
            UT_Matrix3F *inverses3f = cache->myTransformInverse3F.get();
            auto &&functor = [matrices3d,inverses3d,inverses3f](const UT_BlockedRange<exint> &r)
            {
                for (exint i = r.begin(), end = r.end(); i < end; ++i)
                {
                    UT_Matrix3D inverse;
                    auto singular = matrices3d[i].invert(inverse);
                    if (singular)
                    {
                        // FIXME: Check if 1, 2, or 3 zero dimensions!!!
                        inverse.identity();
                    }

                    // This determinant check and scale are from GA_AttributeTransformer.
                    // They're presumably so that normals get flipped when applying
                    // a negative scale.  I'm not sure whether that's ideal behaviour,
                    // but it's consistent with previous behaviour, so I'm sticking with it.
                    if (matrices3d[i].determinant() < 0)
                        inverse.scale(-1, -1, -1);

                    if (inverses3d)
                        inverses3d[i] = inverse;
                    if (inverses3f)
                        inverses3f[i] = UT_Matrix3F(inverse);
                }
            };
            if (num_target_points > 512)
                UTparallelFor(UT_BlockedRange<exint>(0, num_target_points), functor, 2, 256);
            else
                functor(UT_BlockedRange<exint>(0, num_target_points));
        }
    }
    if (needed_transforms[quaternionf] || needed_transforms[quaterniond])
    {
        bool compute = transforms_changed;
        if (needed_transforms[quaternionf] && !cache->myTransformQuaternionsF)
        {
            cache->myTransformQuaternionsF.reset(new UT_QuaternionF[num_target_points]);
            compute = true;
        }
        if (needed_transforms[quaterniond] && !cache->myTransformQuaternionsD)
        {
            cache->myTransformQuaternionsD.reset(new UT_QuaternionD[num_target_points]);
            compute = true;
        }
        if (compute)
        {
            const UT_Matrix3D *matrices3d = cache->myTransformMatrices3D.get();
            UT_QuaternionD *quaternionsd = cache->myTransformQuaternionsD.get();
            UT_QuaternionF *quaternionsf = cache->myTransformQuaternionsF.get();
            auto &&functor = [matrices3d,quaternionsd,quaternionsf](const UT_BlockedRange<exint> &r)
            {
                for (exint i = r.begin(), end = r.end(); i < end; ++i)
                {
                    UT_QuaternionD quaternion;
                    quaternion.updateFromArbitraryMatrix(matrices3d[i]);

                    if (quaternionsd)
                        quaternionsd[i] = quaternion;
                    if (quaternionsf)
                        quaternionsf[i] = UT_QuaternionF(quaternion);
                }
            };
            if (num_target_points > 512)
                UTparallelFor(UT_BlockedRange<exint>(0, num_target_points), functor, 2, 256);
            else
                functor(UT_BlockedRange<exint>(0, num_target_points));
        }
    }
}

static void
guFindStartInTarget(
    const GA_Offset start,
    exint &targeti,
    exint &piece_elementi,
    exint &piece_element_count,
    const GA_OffsetList **piece_offset_list,
    const GA_Offset start_offset,
    const exint *const piece_offset_starts,
    const exint *const piece_offset_starts_end,
    const exint num_target_points,
    const exint *const target_to_piecei,
    const GU_CopyToPointsCache::PieceData *const piece_data,
    const int owneri,
    const GA_OffsetList *const source_offset_list,
    const exint source_offset_list_size)
{
    const exint output_primi = start - start_offset;
    if (piece_offset_starts)
    {
        // Find the first entry in piece_prim_starts where the next entry is greater than output_primi,
        // (in other words, the last entry whose value is less than or equal to output_primi,
        // so output_primi is in that piece.)  The -1 is to go back one.
        targeti = (std::upper_bound(
            piece_offset_starts,
            piece_offset_starts_end,
            output_primi) - piece_offset_starts) - 1;
        UT_ASSERT_P(targeti >= 0 && targeti < num_target_points);
        piece_elementi = output_primi - piece_offset_starts[targeti];
        const exint piecei = target_to_piecei[targeti];
        const GU_CopyToPointsCache::PieceData &current_piece = piece_data[piecei];
        const GA_OffsetList &local_piece_offset_list = current_piece.mySourceOffsetLists[owneri];
        piece_element_count = local_piece_offset_list.size();
        if (piece_offset_list != nullptr)
            *piece_offset_list = &local_piece_offset_list;
    }
    else
    {
        piece_element_count = source_offset_list_size;
        if (piece_offset_list != nullptr)
        {
            UT_ASSERT_P(source_offset_list_size == source_offset_list->size());
            *piece_offset_list = source_offset_list;
        }
        targeti = output_primi / piece_element_count;
        piece_elementi = output_primi % piece_element_count;
    }
};

static inline void
guIteratePieceElement(
    exint &piece_elementi,
    exint &piece_element_count,
    exint &targeti,
    const exint *const piece_offset_starts,
    const exint num_target_points,
    const exint *const target_to_piecei,
    const GU_CopyToPointsCache::PieceData *const piece_data,
    const int owneri,
    const GA_OffsetList *&piece_offset_list)
{
    ++piece_elementi;
    // NOTE: This must be while instead of if, because there can be zero primitives in a piece.
    while (piece_elementi >= piece_element_count)
    {
        piece_elementi = 0;
        ++targeti;

        if (targeti >= num_target_points)
            break;

        if (piece_offset_starts != nullptr)
        {
            exint piecei = target_to_piecei[targeti];
            const GU_CopyToPointsCache::PieceData &current_piece = piece_data[piecei];
            piece_offset_list = &current_piece.mySourceOffsetLists[owneri];
            piece_element_count = piece_offset_list->size();
        }
    }
}

/// This is the same as guIteratePieceElement, but also looking up the target offset,
/// and not returning the piece_offset_list.
static inline void
guIteratePieceElementOff(
    exint &piece_elementi,
    exint &piece_element_count,
    exint &targeti,
    const exint *const piece_offset_starts,
    const exint num_target_points,
    const exint *const target_to_piecei,
    const GU_CopyToPointsCache::PieceData *const piece_data,
    const int owneri,
    GA_Offset &target_off,
    const GA_OffsetListRef &target_point_list)
{
    ++piece_elementi;
    // NOTE: This must be while instead of if, because there can be zero primitives in a piece.
    while (piece_elementi >= piece_element_count)
    {
        piece_elementi = 0;
        ++targeti;

        if (targeti >= num_target_points)
            break;

        target_off = target_point_list[targeti];

        if (piece_offset_starts != nullptr)
        {
            exint piecei = target_to_piecei[targeti];
            const GU_CopyToPointsCache::PieceData &current_piece = piece_data[piecei];
            const GA_OffsetList &piece_offset_list = current_piece.mySourceOffsetLists[owneri];
            piece_element_count = piece_offset_list.size();
        }
    }
}

/// This is the same as guIteratePieceElement, but also looking up whether
/// the target offset is in the given group, and not returning the piece_offset_list.
static inline void
guIteratePieceElementGroup(
    exint &piece_elementi,
    exint &piece_element_count,
    exint &targeti,
    const exint *const piece_offset_starts,
    const exint num_target_points,
    const exint *const target_to_piecei,
    const GU_CopyToPointsCache::PieceData *const piece_data,
    const int owneri,
    bool &target_in_group,
    const GA_OffsetListRef &target_point_list,
    const GA_PointGroup *const target_group)
{
    ++piece_elementi;
    // NOTE: This must be while instead of if, because there can be zero primitives in a piece.
    while (piece_elementi >= piece_element_count)
    {
        piece_elementi = 0;
        ++targeti;

        if (targeti >= num_target_points)
            break;

        const GA_Offset target_off = target_point_list[targeti];
        target_in_group = target_group->contains(target_off);

        if (piece_offset_starts != nullptr)
        {
            exint piecei = target_to_piecei[targeti];
            const GU_CopyToPointsCache::PieceData &current_piece = piece_data[piecei];
            const GA_OffsetList &piece_offset_list = current_piece.mySourceOffsetLists[owneri];
            piece_element_count = piece_offset_list.size();
        }
    }
}

static void
guApplyTransformToAttribute(
    const GU_CopyToPointsCache *const cache,
    GA_TypeInfo transform_type,
    GA_ATINumeric *output_numeric,
    const GA_ATINumeric *source_numeric,
    const GA_OffsetList &source_offset_list,
    const bool copy_source_attribs_in_parallel,
    UT_TaskList &task_list,
    const GA_SplittableRange &output_splittable_range,
    const GA_Offset start_offset,
    const exint *target_to_piecei,
    const exint num_target_points,
    const exint *piece_offset_starts,
    const exint *piece_offset_starts_end,
    const GU_CopyToPointsCache::PieceData *piece_data)
{
    const GA_IndexMap &index_map = output_numeric->getIndexMap();
    if (index_map.indexSize() == 0)
        return;

    int owneri = output_numeric->getOwner();

    const UT_Matrix3F *const transform_matrices_3f = cache->myTransformMatrices3F.get();
    const UT_Matrix3D *const transform_matrices_3d = cache->myTransformMatrices3D.get();
    const UT_Vector3F *const transform_translates_3f = cache->myTransformTranslates3F.get();
    const UT_Vector3D *const transform_translates_3d = cache->myTransformTranslates3D.get();
    if (transform_type == GA_TYPE_POINT)
    {
        auto &&functor = [output_numeric,source_numeric,&source_offset_list,
            transform_matrices_3f,transform_matrices_3d,transform_translates_3f,transform_translates_3d,
            start_offset,piece_offset_starts,piece_offset_starts_end,
            num_target_points,target_to_piecei,piece_data,owneri](const GA_SplittableRange &r)
        {
            GA_Offset start; GA_Offset end;
            for (GA_Iterator it(r); it.fullBlockAdvance(start, end); )
            {
                exint targeti;
                exint piece_elementi;
                exint piece_element_count;
                const GA_OffsetList *piece_offset_list;
                guFindStartInTarget(start, targeti, piece_elementi, piece_element_count,
                    &piece_offset_list, start_offset, piece_offset_starts, piece_offset_starts_end,
                    num_target_points, target_to_piecei, piece_data, owneri,
                    &source_offset_list, source_offset_list.size());

                if (output_numeric->getStorage() == GA_STORE_REAL32)
                {
                    GA_PageArray<fpreal32,3> &output_data = output_numeric->getData().castType<fpreal32>().castTupleSize<3>();
                    const GA_PageArray<fpreal32,3> &source_data = source_numeric->getData().castType<fpreal32>().castTupleSize<3>();
                    // FIXME: Find longer contiguous spans to transform, for better performance.
                    for (GA_Offset dest_off = start; dest_off < end; ++dest_off)
                    {
                        GA_Offset source_off = (*piece_offset_list)[piece_elementi];
                        UT_Vector3F pos = source_data.getVector(source_off);
                        if (transform_matrices_3f)
                            pos = (pos * transform_matrices_3f[targeti]) + transform_translates_3f[targeti];
                        else
                            pos += transform_translates_3f[targeti];
                        output_data.setVector(dest_off, pos);

                        guIteratePieceElement(piece_elementi, piece_element_count, targeti, piece_offset_starts,
                            num_target_points, target_to_piecei, piece_data, owneri, piece_offset_list);
                    }
                }
                else if (output_numeric->getStorage() == GA_STORE_REAL64)
                {
                    GA_PageArray<fpreal64,3> &output_data = output_numeric->getData().castType<fpreal64>().castTupleSize<3>();
                    const GA_PageArray<fpreal64,3> &source_data = source_numeric->getData().castType<fpreal64>().castTupleSize<3>();
                    // FIXME: Find longer contiguous spans to transform, for better performance.
                    for (GA_Offset dest_off = start; dest_off < end; ++dest_off)
                    {
                        GA_Offset source_off = (*piece_offset_list)[piece_elementi];
                        UT_Vector3D pos = source_data.getVector(source_off);
                        if (transform_matrices_3d)
                            pos = (pos * transform_matrices_3d[targeti]) + transform_translates_3d[targeti];
                        else
                            pos += transform_translates_3d[targeti];
                        output_data.setVector(dest_off, pos);

                        guIteratePieceElement(piece_elementi, piece_element_count, targeti, piece_offset_starts,
                            num_target_points, target_to_piecei, piece_data, owneri, piece_offset_list);
                    }
                }
                else
                {
                    GA_RWHandleV3 output_data(output_numeric);
                    GA_ROHandleV3 source_data(source_numeric);
                    UT_ASSERT_P(output_data.isValid() && source_data.isValid());
                    // FIXME: Find longer contiguous spans to transform, for better performance.
                    for (GA_Offset dest_off = start; dest_off < end; ++dest_off)
                    {
                        GA_Offset source_off = (*piece_offset_list)[piece_elementi];
                        UT_Vector3F pos = source_data.get(source_off);
                        if (transform_matrices_3f)
                            pos = (pos * transform_matrices_3f[targeti]) + transform_translates_3f[targeti];
                        else
                            pos += transform_translates_3f[targeti];
                        output_data.set(dest_off, pos);

                        guIteratePieceElement(piece_elementi, piece_element_count, targeti, piece_offset_starts,
                            num_target_points, target_to_piecei, piece_data, owneri, piece_offset_list);
                    }
                }
            }
        };
        if (copy_source_attribs_in_parallel)
            UTparallelForAppendToTaskList(task_list, output_splittable_range, functor);
        else
            functor(output_splittable_range);
    }
    else if (transform_type == GA_TYPE_VECTOR)
    {
        auto &&functor = [output_numeric,source_numeric,&source_offset_list,
            transform_matrices_3f,transform_matrices_3d,
            start_offset,piece_offset_starts,piece_offset_starts_end,
            num_target_points,target_to_piecei,piece_data,owneri](const GA_SplittableRange &r)
        {
            GA_Offset start; GA_Offset end;
            for (GA_Iterator it(r); it.fullBlockAdvance(start, end); )
            {
                exint targeti;
                exint piece_elementi;
                exint piece_element_count;
                const GA_OffsetList *piece_offset_list;
                guFindStartInTarget(start, targeti, piece_elementi, piece_element_count,
                    &piece_offset_list, start_offset, piece_offset_starts, piece_offset_starts_end,
                    num_target_points, target_to_piecei, piece_data, owneri,
                    &source_offset_list, source_offset_list.size());

                if (output_numeric->getStorage() == GA_STORE_REAL32)
                {
                    UT_ASSERT_P(transform_matrices_3f);
                    GA_PageArray<fpreal32,3> &output_data = output_numeric->getData().castType<fpreal32>().castTupleSize<3>();
                    const GA_PageArray<fpreal32,3> &source_data = source_numeric->getData().castType<fpreal32>().castTupleSize<3>();
                    // FIXME: Find longer contiguous spans to transform, for better performance.
                    for (GA_Offset dest_off = start; dest_off < end; ++dest_off)
                    {
                        GA_Offset source_off = (*piece_offset_list)[piece_elementi];
                        UT_Vector3F vec = source_data.getVector(source_off);
                        vec.rowVecMult(transform_matrices_3f[targeti]);
                        output_data.setVector(dest_off, vec);

                        guIteratePieceElement(piece_elementi, piece_element_count, targeti, piece_offset_starts,
                            num_target_points, target_to_piecei, piece_data, owneri, piece_offset_list);
                    }
                }
                else if (output_numeric->getStorage() == GA_STORE_REAL64)
                {
                    UT_ASSERT_P(transform_matrices_3d);
                    GA_PageArray<fpreal64,3> &output_data = output_numeric->getData().castType<fpreal64>().castTupleSize<3>();
                    const GA_PageArray<fpreal64,3> &source_data = source_numeric->getData().castType<fpreal64>().castTupleSize<3>();
                    // FIXME: Find longer contiguous spans to transform, for better performance.
                    for (GA_Offset dest_off = start; dest_off < end; ++dest_off)
                    {
                        GA_Offset source_off = (*piece_offset_list)[piece_elementi];
                        UT_Vector3D vec = source_data.getVector(source_off);
                        vec.rowVecMult(transform_matrices_3d[targeti]);
                        output_data.setVector(dest_off, vec);

                        guIteratePieceElement(piece_elementi, piece_element_count, targeti, piece_offset_starts,
                            num_target_points, target_to_piecei, piece_data, owneri, piece_offset_list);
                    }
                }
                else
                {
                    UT_ASSERT_P(transform_matrices_3f);
                    GA_RWHandleV3 output_data(output_numeric);
                    GA_ROHandleV3 source_data(source_numeric);
                    UT_ASSERT_P(output_data.isValid() && source_data.isValid());
                    // FIXME: Find longer contiguous spans to transform, for better performance.
                    for (GA_Offset dest_off = start; dest_off < end; ++dest_off)
                    {
                        GA_Offset source_off = (*piece_offset_list)[piece_elementi];
                        UT_Vector3F vec = source_data.get(source_off);
                        vec.rowVecMult(transform_matrices_3f[targeti]);
                        output_data.set(dest_off, vec);

                        guIteratePieceElement(piece_elementi, piece_element_count, targeti, piece_offset_starts,
                            num_target_points, target_to_piecei, piece_data, owneri, piece_offset_list);
                    }
                }
            }
        };
        if (copy_source_attribs_in_parallel)
            UTparallelForAppendToTaskList(task_list, output_splittable_range, functor);
        else
            functor(output_splittable_range);
    }
    else if (transform_type == GA_TYPE_NORMAL)
    {
        const UT_Matrix3F *transform_inverse_3f = cache->myTransformInverse3F.get();
        const UT_Matrix3D *transform_inverse_3d = cache->myTransformInverse3D.get();
        auto &&functor = [output_numeric,source_numeric,&source_offset_list,
            transform_inverse_3f,transform_inverse_3d,
            start_offset,piece_offset_starts,piece_offset_starts_end,
            num_target_points,target_to_piecei,piece_data,owneri](const GA_SplittableRange &r)
        {
            GA_Offset start; GA_Offset end;
            for (GA_Iterator it(r); it.fullBlockAdvance(start, end); )
            {
                exint targeti;
                exint piece_elementi;
                exint piece_element_count;
                const GA_OffsetList *piece_offset_list;
                guFindStartInTarget(start, targeti, piece_elementi, piece_element_count,
                    &piece_offset_list, start_offset, piece_offset_starts, piece_offset_starts_end,
                    num_target_points, target_to_piecei, piece_data, owneri,
                    &source_offset_list, source_offset_list.size());

                if (output_numeric->getStorage() == GA_STORE_REAL32)
                {
                    UT_ASSERT_P(transform_inverse_3f);
                    GA_PageArray<fpreal32,3> &output_data = output_numeric->getData().castType<fpreal32>().castTupleSize<3>();
                    const GA_PageArray<fpreal32,3> &source_data = source_numeric->getData().castType<fpreal32>().castTupleSize<3>();
                    // FIXME: Find longer contiguous spans to transform, for better performance.
                    for (GA_Offset dest_off = start; dest_off < end; ++dest_off)
                    {
                        GA_Offset source_off = (*piece_offset_list)[piece_elementi];
                        UT_Vector3F nml = source_data.getVector(source_off);
                        float orig_length2 = nml.length2();
                        nml.colVecMult(transform_inverse_3f[targeti]);
                        float new_length2 = nml.length2();
                        // Preserve normal length
                        if (new_length2 != 0)
                            nml *= SYSsqrt(orig_length2/new_length2);
                        output_data.setVector(dest_off, nml);

                        guIteratePieceElement(piece_elementi, piece_element_count, targeti, piece_offset_starts,
                            num_target_points, target_to_piecei, piece_data, owneri, piece_offset_list);
                    }
                }
                else if (output_numeric->getStorage() == GA_STORE_REAL64)
                {
                    UT_ASSERT_P(transform_inverse_3d);
                    GA_PageArray<fpreal64,3> &output_data = output_numeric->getData().castType<fpreal64>().castTupleSize<3>();
                    const GA_PageArray<fpreal64,3> &source_data = source_numeric->getData().castType<fpreal64>().castTupleSize<3>();
                    // FIXME: Find longer contiguous spans to transform, for better performance.
                    for (GA_Offset dest_off = start; dest_off < end; ++dest_off)
                    {
                        GA_Offset source_off = (*piece_offset_list)[piece_elementi];
                        UT_Vector3D nml = source_data.getVector(source_off);
                        float orig_length2 = nml.length2();
                        nml.colVecMult(transform_inverse_3d[targeti]);
                        float new_length2 = nml.length2();
                        // Preserve normal length
                        if (new_length2 != 0)
                            nml *= SYSsqrt(orig_length2/new_length2);
                        output_data.setVector(dest_off, nml);

                        guIteratePieceElement(piece_elementi, piece_element_count, targeti, piece_offset_starts,
                            num_target_points, target_to_piecei, piece_data, owneri, piece_offset_list);
                    }
                }
                else
                {
                    UT_ASSERT_P(transform_inverse_3f);
                    GA_RWHandleV3 output_data(output_numeric);
                    GA_ROHandleV3 source_data(source_numeric);
                    UT_ASSERT_P(output_data.isValid() && source_data.isValid());
                    // FIXME: Find longer contiguous spans to transform, for better performance.
                    for (GA_Offset dest_off = start; dest_off < end; ++dest_off)
                    {
                        GA_Offset source_off = (*piece_offset_list)[piece_elementi];
                        UT_Vector3F nml = source_data.get(source_off);
                        float orig_length2 = nml.length2();
                        nml.colVecMult(transform_inverse_3f[targeti]);
                        float new_length2 = nml.length2();
                        // Preserve normal length
                        if (new_length2 != 0)
                            nml *= SYSsqrt(orig_length2/new_length2);
                        output_data.set(dest_off, nml);

                        guIteratePieceElement(piece_elementi, piece_element_count, targeti, piece_offset_starts,
                            num_target_points, target_to_piecei, piece_data, owneri, piece_offset_list);
                    }
                }
            }
        };
        if (copy_source_attribs_in_parallel)
            UTparallelForAppendToTaskList(task_list, output_splittable_range, functor);
        else
            functor(output_splittable_range);
    }
    else if (transform_type == GA_TYPE_QUATERNION)
    {
        const UT_QuaternionF *transform_quaternions_3f = cache->myTransformQuaternionsF.get();
        const UT_QuaternionD *transform_quaternions_3d = cache->myTransformQuaternionsD.get();
        auto &&functor = [output_numeric,source_numeric,&source_offset_list,
            transform_quaternions_3f,transform_quaternions_3d,
            start_offset,piece_offset_starts,piece_offset_starts_end,
            num_target_points,target_to_piecei,piece_data,owneri](const GA_SplittableRange &r)
        {
            GA_Offset start; GA_Offset end;
            for (GA_Iterator it(r); it.fullBlockAdvance(start, end); )
            {
                exint targeti;
                exint piece_elementi;
                exint piece_element_count;
                const GA_OffsetList *piece_offset_list;
                guFindStartInTarget(start, targeti, piece_elementi, piece_element_count,
                    &piece_offset_list, start_offset, piece_offset_starts, piece_offset_starts_end,
                    num_target_points, target_to_piecei, piece_data, owneri,
                    &source_offset_list, source_offset_list.size());

                if (output_numeric->getStorage() == GA_STORE_REAL32)
                {
                    UT_ASSERT_P(transform_quaternions_3f);
                    GA_PageArray<fpreal32,4> &output_data = output_numeric->getData().castType<fpreal32>().castTupleSize<4>();
                    const GA_PageArray<fpreal32,4> &source_data = source_numeric->getData().castType<fpreal32>().castTupleSize<4>();
                    // FIXME: Find longer contiguous spans to transform, for better performance.
                    for (GA_Offset dest_off = start; dest_off < end; ++dest_off)
                    {
                        GA_Offset source_off = (*piece_offset_list)[piece_elementi];
                        UT_QuaternionF q(source_data.getVector(source_off));
                        q = transform_quaternions_3f[targeti] * q;
                        output_data.setVector(dest_off, *(const UT_FixedVector<fpreal32,4>*)&q);

                        guIteratePieceElement(piece_elementi, piece_element_count, targeti, piece_offset_starts,
                            num_target_points, target_to_piecei, piece_data, owneri, piece_offset_list);
                    }
                }
                else if (output_numeric->getStorage() == GA_STORE_REAL64)
                {
                    UT_ASSERT_P(transform_quaternions_3d);
                    GA_PageArray<fpreal64,4> &output_data = output_numeric->getData().castType<fpreal64>().castTupleSize<4>();
                    const GA_PageArray<fpreal64,4> &source_data = source_numeric->getData().castType<fpreal64>().castTupleSize<4>();
                    // FIXME: Find longer contiguous spans to transform, for better performance.
                    for (GA_Offset dest_off = start; dest_off < end; ++dest_off)
                    {
                        GA_Offset source_off = (*piece_offset_list)[piece_elementi];
                        UT_QuaternionD q(source_data.getVector(source_off));
                        q = transform_quaternions_3d[targeti] * q;
                        output_data.setVector(dest_off, *(const UT_FixedVector<fpreal64,4>*)&q);

                        guIteratePieceElement(piece_elementi, piece_element_count, targeti, piece_offset_starts,
                            num_target_points, target_to_piecei, piece_data, owneri, piece_offset_list);
                    }
                }
                else
                {
                    UT_ASSERT_P(transform_quaternions_3f);
                    GA_RWHandleQ output_data(output_numeric);
                    GA_ROHandleQ source_data(source_numeric);
                    UT_ASSERT_P(output_data.isValid() && source_data.isValid());
                    // FIXME: Find longer contiguous spans to transform, for better performance.
                    for (GA_Offset dest_off = start; dest_off < end; ++dest_off)
                    {
                        GA_Offset source_off = (*piece_offset_list)[piece_elementi];
                        UT_QuaternionF q = source_data.get(source_off);
                        q = transform_quaternions_3f[targeti] * q;
                        output_data.set(dest_off, q);

                        guIteratePieceElement(piece_elementi, piece_element_count, targeti, piece_offset_starts,
                            num_target_points, target_to_piecei, piece_data, owneri, piece_offset_list);
                    }
                }
            }
        };
        if (copy_source_attribs_in_parallel)
            UTparallelForAppendToTaskList(task_list, output_splittable_range, functor);
        else
            functor(output_splittable_range);
    }
    else if (transform_type == GA_TYPE_HPOINT)
    {
        auto &&functor = [output_numeric,source_numeric,&source_offset_list,
            transform_matrices_3f,transform_matrices_3d,transform_translates_3f,transform_translates_3d,
            start_offset,piece_offset_starts,piece_offset_starts_end,
            num_target_points,target_to_piecei,piece_data,owneri](const GA_SplittableRange &r)
        {
            UT_Matrix3F identity3f(1);
            UT_Matrix3D identity3d(1);
            GA_Offset start; GA_Offset end;
            for (GA_Iterator it(r); it.fullBlockAdvance(start, end); )
            {
                exint targeti;
                exint piece_elementi;
                exint piece_element_count;
                const GA_OffsetList *piece_offset_list;
                guFindStartInTarget(start, targeti, piece_elementi, piece_element_count,
                    &piece_offset_list, start_offset, piece_offset_starts, piece_offset_starts_end,
                    num_target_points, target_to_piecei, piece_data, owneri,
                    &source_offset_list, source_offset_list.size());

                if (output_numeric->getStorage() == GA_STORE_REAL32)
                {
                    GA_PageArray<fpreal32,4> &output_data = output_numeric->getData().castType<fpreal32>().castTupleSize<4>();
                    const GA_PageArray<fpreal32,4> &source_data = source_numeric->getData().castType<fpreal32>().castTupleSize<4>();
                    // FIXME: Find longer contiguous spans to transform, for better performance.
                    for (GA_Offset dest_off = start; dest_off < end; ++dest_off)
                    {
                        GA_Offset source_off = (*piece_offset_list)[piece_elementi];
                        UT_Vector4F hp(source_data.getVector(source_off));
                        hp.homogenize();
                        UT_Matrix4F transform(transform_matrices_3f ? transform_matrices_3f[targeti] : identity3f);
                        transform.setTranslates(transform_translates_3f[targeti]);
                        hp *= transform;
                        hp.dehomogenize();
                        output_data.setVector(dest_off, hp);

                        guIteratePieceElement(piece_elementi, piece_element_count, targeti, piece_offset_starts,
                            num_target_points, target_to_piecei, piece_data, owneri, piece_offset_list);
                    }
                }
                else if (output_numeric->getStorage() == GA_STORE_REAL64)
                {
                    GA_PageArray<fpreal64,4> &output_data = output_numeric->getData().castType<fpreal64>().castTupleSize<4>();
                    const GA_PageArray<fpreal64,4> &source_data = source_numeric->getData().castType<fpreal64>().castTupleSize<4>();
                    // FIXME: Find longer contiguous spans to transform, for better performance.
                    for (GA_Offset dest_off = start; dest_off < end; ++dest_off)
                    {
                        GA_Offset source_off = (*piece_offset_list)[piece_elementi];
                        UT_Vector4D hp(source_data.getVector(source_off));
                        hp.homogenize();
                        UT_Matrix4D transform(transform_matrices_3d ? transform_matrices_3d[targeti] : identity3d);
                        transform.setTranslates(transform_translates_3d[targeti]);
                        hp *= transform;
                        hp.dehomogenize();
                        output_data.setVector(dest_off, hp);

                        guIteratePieceElement(piece_elementi, piece_element_count, targeti, piece_offset_starts,
                            num_target_points, target_to_piecei, piece_data, owneri, piece_offset_list);
                    }
                }
                else
                {
                    GA_RWHandleV4 output_data(output_numeric);
                    GA_ROHandleV4 source_data(source_numeric);
                    UT_ASSERT_P(output_data.isValid() && source_data.isValid());
                    // FIXME: Find longer contiguous spans to transform, for better performance.
                    for (GA_Offset dest_off = start; dest_off < end; ++dest_off)
                    {
                        GA_Offset source_off = (*piece_offset_list)[piece_elementi];
                        UT_Vector4F hp = source_data.get(source_off);
                        hp.homogenize();
                        UT_Matrix4F transform(transform_matrices_3f ? transform_matrices_3f[targeti] : identity3f);
                        transform.setTranslates(transform_translates_3f[targeti]);
                        hp *= transform;
                        hp.dehomogenize();
                        output_data.set(dest_off, hp);

                        guIteratePieceElement(piece_elementi, piece_element_count, targeti, piece_offset_starts,
                            num_target_points, target_to_piecei, piece_data, owneri, piece_offset_list);
                    }
                }
            }
        };
        if (copy_source_attribs_in_parallel)
            UTparallelForAppendToTaskList(task_list, output_splittable_range, functor);
        else
            functor(output_splittable_range);
    }
    else if (output_numeric->getTupleSize() == 9) // transform_type == GA_TYPE_TRANSFORM
    {
        UT_ASSERT(transform_type == GA_TYPE_TRANSFORM);
        auto &&functor = [output_numeric,source_numeric,&source_offset_list,
            transform_matrices_3f,transform_matrices_3d,
            start_offset,piece_offset_starts,piece_offset_starts_end,
            num_target_points,target_to_piecei,piece_data,owneri](const GA_SplittableRange &r)
        {
            GA_Offset start; GA_Offset end;
            for (GA_Iterator it(r); it.fullBlockAdvance(start, end); )
            {
                exint targeti;
                exint piece_elementi;
                exint piece_element_count;
                const GA_OffsetList *piece_offset_list;
                guFindStartInTarget(start, targeti, piece_elementi, piece_element_count,
                    &piece_offset_list, start_offset, piece_offset_starts, piece_offset_starts_end,
                    num_target_points, target_to_piecei, piece_data, owneri,
                    &source_offset_list, source_offset_list.size());

                if (output_numeric->getStorage() == GA_STORE_REAL32)
                {
                    UT_ASSERT_P(transform_matrices_3f);
                    GA_PageArray<fpreal32,9> &output_data = output_numeric->getData().castType<fpreal32>().castTupleSize<9>();
                    const GA_PageArray<fpreal32,9> &source_data = source_numeric->getData().castType<fpreal32>().castTupleSize<9>();
                    // FIXME: Find longer contiguous spans to transform, for better performance.
                    for (GA_Offset dest_off = start; dest_off < end; ++dest_off)
                    {
                        GA_Offset source_off = (*piece_offset_list)[piece_elementi];
                        UT_Matrix3F mat(source_data.getVector(source_off));
                        mat *= transform_matrices_3f[targeti];
                        output_data.setVector(dest_off, *(const UT_FixedVector<fpreal32,9>*)&mat);

                        guIteratePieceElement(piece_elementi, piece_element_count, targeti, piece_offset_starts,
                            num_target_points, target_to_piecei, piece_data, owneri, piece_offset_list);
                    }
                }
                else if (output_numeric->getStorage() == GA_STORE_REAL64)
                {
                    UT_ASSERT_P(transform_matrices_3d);
                    GA_PageArray<fpreal64,9> &output_data = output_numeric->getData().castType<fpreal64>().castTupleSize<9>();
                    const GA_PageArray<fpreal64,9> &source_data = source_numeric->getData().castType<fpreal64>().castTupleSize<9>();
                    // FIXME: Find longer contiguous spans to transform, for better performance.
                    for (GA_Offset dest_off = start; dest_off < end; ++dest_off)
                    {
                        GA_Offset source_off = (*piece_offset_list)[piece_elementi];
                        UT_Matrix3D mat(source_data.getVector(source_off));
                        mat *= transform_matrices_3d[targeti];
                        output_data.setVector(dest_off, *(const UT_FixedVector<fpreal64,9>*)&mat);

                        guIteratePieceElement(piece_elementi, piece_element_count, targeti, piece_offset_starts,
                            num_target_points, target_to_piecei, piece_data, owneri, piece_offset_list);
                    }
                }
                else
                {
                    UT_ASSERT_P(transform_matrices_3f);
                    GA_RWHandleM3 output_data(output_numeric);
                    GA_ROHandleM3 source_data(source_numeric);
                    UT_ASSERT_P(output_data.isValid() && source_data.isValid());
                    // FIXME: Find longer contiguous spans to transform, for better performance.
                    for (GA_Offset dest_off = start; dest_off < end; ++dest_off)
                    {
                        GA_Offset source_off = (*piece_offset_list)[piece_elementi];
                        UT_Matrix3F mat = source_data.get(source_off);
                        mat *= transform_matrices_3f[targeti];
                        output_data.set(dest_off, mat);

                        guIteratePieceElement(piece_elementi, piece_element_count, targeti, piece_offset_starts,
                            num_target_points, target_to_piecei, piece_data, owneri, piece_offset_list);
                    }
                }
            }
        };
        if (copy_source_attribs_in_parallel)
            UTparallelForAppendToTaskList(task_list, output_splittable_range, functor);
        else
            functor(output_splittable_range);
    }
    else
    {
        UT_ASSERT(transform_type == GA_TYPE_TRANSFORM);
        UT_ASSERT(output_numeric->getTupleSize() == 16);
        auto &&functor = [output_numeric,source_numeric,&source_offset_list,
            transform_matrices_3f,transform_matrices_3d,transform_translates_3f,transform_translates_3d,
            start_offset,piece_offset_starts,piece_offset_starts_end,
            num_target_points,target_to_piecei,piece_data,owneri](const GA_SplittableRange &r)
        {
            GA_Offset start; GA_Offset end;
            for (GA_Iterator it(r); it.fullBlockAdvance(start, end); )
            {
                exint targeti;
                exint piece_elementi;
                exint piece_element_count;
                const GA_OffsetList *piece_offset_list;
                guFindStartInTarget(start, targeti, piece_elementi, piece_element_count,
                    &piece_offset_list, start_offset, piece_offset_starts, piece_offset_starts_end,
                    num_target_points, target_to_piecei, piece_data, owneri,
                    &source_offset_list, source_offset_list.size());

                if (output_numeric->getStorage() == GA_STORE_REAL32)
                {
                    GA_PageArray<fpreal32,16> &output_data = output_numeric->getData().castType<fpreal32>().castTupleSize<16>();
                    const GA_PageArray<fpreal32,16> &source_data = source_numeric->getData().castType<fpreal32>().castTupleSize<16>();
                    // FIXME: Find longer contiguous spans to transform, for better performance.
                    for (GA_Offset dest_off = start; dest_off < end; ++dest_off)
                    {
                        GA_Offset source_off = (*piece_offset_list)[piece_elementi];
                        UT_Matrix4F mat(source_data.getVector(source_off));
                        if (transform_matrices_3f)
                        {
                            UT_Matrix4F transform(transform_matrices_3f[targeti]);
                            transform.setTranslates(transform_translates_3f[targeti]);
                            mat *= transform;
                        }
                        else
                            mat.translate(transform_translates_3f[targeti]);
                        output_data.setVector(dest_off, *(const UT_FixedVector<fpreal32,16>*)&mat);

                        guIteratePieceElement(piece_elementi, piece_element_count, targeti, piece_offset_starts,
                            num_target_points, target_to_piecei, piece_data, owneri, piece_offset_list);
                    }
                }
                else if (output_numeric->getStorage() == GA_STORE_REAL64)
                {
                    GA_PageArray<fpreal64,16> &output_data = output_numeric->getData().castType<fpreal64>().castTupleSize<16>();
                    const GA_PageArray<fpreal64,16> &source_data = source_numeric->getData().castType<fpreal64>().castTupleSize<16>();
                    // FIXME: Find longer contiguous spans to transform, for better performance.
                    for (GA_Offset dest_off = start; dest_off < end; ++dest_off)
                    {
                        GA_Offset source_off = (*piece_offset_list)[piece_elementi];
                        UT_Matrix4D mat(source_data.getVector(source_off));
                        if (transform_matrices_3d)
                        {
                            UT_Matrix4D transform(transform_matrices_3d[targeti]);
                            transform.setTranslates(transform_translates_3d[targeti]);
                            mat *= transform;
                        }
                        else
                            mat.translate(transform_translates_3d[targeti]);
                        output_data.setVector(dest_off, *(const UT_FixedVector<fpreal64,16>*)&mat);

                        guIteratePieceElement(piece_elementi, piece_element_count, targeti, piece_offset_starts,
                            num_target_points, target_to_piecei, piece_data, owneri, piece_offset_list);
                    }
                }
                else
                {
                    GA_RWHandleM4 output_data(output_numeric);
                    GA_ROHandleM4 source_data(source_numeric);
                    UT_ASSERT_P(output_data.isValid() && source_data.isValid());
                    // FIXME: Find longer contiguous spans to transform, for better performance.
                    for (GA_Offset dest_off = start; dest_off < end; ++dest_off)
                    {
                        GA_Offset source_off = (*piece_offset_list)[piece_elementi];
                        UT_Matrix4F mat = source_data.get(source_off);
                        if (transform_matrices_3f)
                        {
                            UT_Matrix4F transform(transform_matrices_3f[targeti]);
                            transform.setTranslates(transform_translates_3f[targeti]);
                            mat *= transform;
                        }
                        else
                            mat.translate(transform_translates_3f[targeti]);
                        output_data.set(dest_off, mat);

                        guIteratePieceElement(piece_elementi, piece_element_count, targeti, piece_offset_starts,
                            num_target_points, target_to_piecei, piece_data, owneri, piece_offset_list);
                    }
                }
            }
        };
        if (copy_source_attribs_in_parallel)
            UTparallelForAppendToTaskList(task_list, output_splittable_range, functor);
        else
            functor(output_splittable_range);
    }
}

void
GUcreateGeometryFromSource(
    GU_Detail *output_geo,
    const GU_Detail *const source,
    const GA_OffsetList &source_point_list_cache,
    const GA_OffsetList &source_vertex_list_cache,
    const GA_OffsetList &source_prim_list_cache,
    const exint ncopies)
{
    exint source_point_count = source_point_list_cache.size();
    exint source_vertex_count = source_vertex_list_cache.size();
    exint source_prim_count = source_prim_list_cache.size();

    GA_Size totalnpoints = source_point_count * ncopies;
    GA_Offset startpt = output_geo->appendPointBlock(totalnpoints);

    if (source_prim_count <= 0)
        return;

    UT_SmallArray<std::pair<int,exint>, 2*sizeof(std::pair<int,exint>)> prim_type_count_pairs;
    bool all_source_prims = source_prim_list_cache.isSame(source->getPrimitiveMap().getOffsetFromIndexList());
    GA_Range source_primrange;
    if (!all_source_prims)
        source_primrange = GA_RTIOffsetList(source->getPrimitiveMap(), source_prim_list_cache);
    source->getPrimitiveList().getPrimitiveTypeCounts(prim_type_count_pairs, !all_source_prims ? &source_primrange : nullptr);
    UT_ASSERT(!prim_type_count_pairs.isEmpty());

    // Compute a ptoff to point-within-a-copy index structure if we're not copying all points.
    GA_PageArray<exint,1> source_ptoff_to_pointi;
    bool all_source_points = source_point_list_cache.isSame(source->getPointMap().getOffsetFromIndexList());
    bool have_reverse_point_map = !all_source_points && !source_point_list_cache.isTrivial();
    if (have_reverse_point_map)
    {
        source_ptoff_to_pointi.setSize(source->getNumPointOffsets(), exint(-1));
        // TODO: Parallelize this if it's worthwhile.
        for (exint pointi = 0; pointi < source_point_count; ++pointi)
        {
            source_ptoff_to_pointi.set(source_point_list_cache[pointi], pointi);
        }
    }

    // It's safe for hassharedpoints to be true when there are no shared points,
    // but not vice versa.
    bool hassharedpoints = true;
    // In this case, "contiguous" means "starting from startpt and contiguous"
    bool hascontiguouspoints = false;
    if (source_point_count >= source_vertex_count)
    {
        // If there is at least one point per vertex, there's a
        // decent chance that none of the source points are shared,
        // which can make building the primitives much faster,
        // so we check.

        hascontiguouspoints = true;
        hassharedpoints = false;

        // TODO: Parallelize this.
        GA_Offset last_point = GA_INVALID_OFFSET;
        for (exint primi = 0; primi < source_prim_count; ++primi)
        {
            GA_Offset primoff = source_prim_list_cache[primi];
            const GA_OffsetListRef vertices = source->getPrimitiveVertexList(primoff);
            if (vertices.size() == 0)
                continue;
            if (!GAisValid(last_point))
            {
                last_point = source->vertexPoint(vertices[0]);
                if (last_point != source_point_list_cache[0])
                {
                    hascontiguouspoints = false;
                }
            }
            else
            {
                GA_Offset current_point = source->vertexPoint(vertices[0]);
                hascontiguouspoints &= (current_point == last_point+1);

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
            for (exint i = 1, n = vertices.size(); i < n; ++i)
            {
                GA_Offset current_point = source->vertexPoint(vertices[i]);
                hascontiguouspoints &= (current_point == last_point+1);

                // See comment above about how this isn't a perfect check.
                hassharedpoints |= (current_point <= last_point);
                if (hassharedpoints)
                    break;
                last_point = current_point;
            }
            if (hassharedpoints)
                break;
        }
    }

    UT_UniquePtr<exint[]> vertexpointnumbers_deleter(!hascontiguouspoints ? new exint[source_vertex_count] : nullptr);
    GA_PolyCounts vertexlistsizelist;
    UT_SmallArray<exint, 2*sizeof(exint)> closed_span_lengths;
    closed_span_lengths.append(0);
    exint *vertexpointnumbers = vertexpointnumbers_deleter.get();
    // TODO: Parallelize this if it's worthwhile.
    exint vertexi = 0;
    for (exint primi = 0; primi < source_prim_count; ++primi)
    {
        GA_Offset primoff = source_prim_list_cache[primi];
        const GA_OffsetListRef vertices = source->getPrimitiveVertexList(primoff);
        GA_Size n = vertices.size();

        vertexlistsizelist.append(n);

        bool closed = vertices.getExtraFlag();
        // Index 0 (size 1) always represents open, and so does every even index (odd size).
        // Every odd index (even size) always represents closed.
        // This condition checks if we're switching between open and closed.
        if ((closed_span_lengths.size()&1) == exint(closed))
            closed_span_lengths.append(1);
        else
            ++(closed_span_lengths.last());

        if (!hascontiguouspoints)
        {
            if (have_reverse_point_map)
            {
                for (exint i = 0; i < n; ++i)
                {
                    GA_Offset source_ptoff = source->vertexPoint(vertices(i));
                    exint pointi = source_ptoff_to_pointi.get(source_ptoff);
                    vertexpointnumbers[vertexi] = pointi;
                    ++vertexi;
                }
            }
            else if (source_point_list_cache.isTrivial())
            {
                for (exint i = 0; i < n; ++i)
                {
                    GA_Offset source_ptoff = source->vertexPoint(vertices(i));
                    exint pointi = source_point_list_cache.find(source_ptoff);
                    UT_ASSERT_P(pointi >= 0);
                    vertexpointnumbers[vertexi] = pointi;
                    ++vertexi;
                }
            }
            else
            {
                for (exint i = 0; i < n; ++i)
                {
                    GA_Offset source_ptoff = source->vertexPoint(vertices(i));
                    exint pointi = exint(source->pointIndex(source_ptoff));
                    vertexpointnumbers[vertexi] = pointi;
                    ++vertexi;
                }
            }
        }
    }

    GA_Offset start_primoff = GEObuildPrimitives(
        output_geo,
        prim_type_count_pairs.getArray(),
        startpt,
        source_point_count,
        vertexlistsizelist,
        vertexpointnumbers,
        hassharedpoints,
        closed_span_lengths.getArray(),
        ncopies);


    // Early exit if only polygons and tetrahedra,
    // since they have no member data outside of GA_Primitive,
    // and might be stored compressed in GA_PrimitiveList.
    exint num_polys_and_tets =
        output_geo->countPrimitiveType(GA_PRIMPOLY) +
        output_geo->countPrimitiveType(GA_PRIMTETRAHEDRON);
    if (output_geo->getNumPrimitives() == num_polys_and_tets)
        return;

    // Copy primitive subclass data for types other than polygons and tetrahedra.
    UT_BlockedRange<GA_Offset> primrange(start_primoff, start_primoff+output_geo->getNumPrimitives());
    auto &&functor = [output_geo,source,&source_prim_list_cache,source_prim_count](const UT_BlockedRange<GA_Offset> &r)
    {
        // FIXME: Find longer contiguous spans to copy, for better performance.
        for (GA_Offset dest_off = r.begin(), end = r.end(); dest_off < end; ++dest_off)
        {
            exint sourcei = exint(dest_off) % source_prim_count;
            GA_Offset source_off = source_prim_list_cache[sourcei];
            const GA_Primitive *source_prim = source->getPrimitive(source_off);
            GA_Primitive *output_prim = output_geo->getPrimitive(dest_off);
            output_prim->copySubclassData(source_prim);
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

void
GUcreatePointOrPrimList(
    GA_OffsetList &offset_list,
    const GU_Detail *const detail,
    const GA_ElementGroup *const group,
    const GA_AttributeOwner owner)
{
    if (group == nullptr)
    {
        offset_list = detail->getIndexMap(owner).getOffsetFromIndexList();
    }
    else
    {
        offset_list.clear();
        GA_Offset start;
        GA_Offset end;
        GA_Range range(*group);
        for (GA_Iterator it(range); it.fullBlockAdvance(start, end); )
        {
            offset_list.setTrivialRange(offset_list.size(), start, end-start);
        }
    }
}

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
    const exint ncopies)
{
    if (ncopies <= 0)
    {
        source_vertex_list_cache.clear();
        return;
    }

    if (source_prim_count <= 0)
    {
        source_vertex_list_cache.clear();

        GA_Size totalnpoints = source_point_count * ncopies;
        output_geo->appendPointBlock(totalnpoints);
        return;
    }

    // We need to build and cache a structure for quickly looking up
    // vertex offsets in order to easily copy vertex attributes
    // in parallel.
    {
        // TODO: Parallelize this if it's worthwhile.
        source_vertex_list_cache.clear();
        GA_Offset start;
        GA_Offset end;
        for (GA_Iterator it(source->getPrimitiveRange(source_primgroup)); it.fullBlockAdvance(start, end); )
        {
            for (GA_Offset primoff = start; primoff < end; ++primoff)
            {
                const GA_OffsetListRef vertices = source->getPrimitiveVertexList(primoff);
                source_vertex_list_cache.append(vertices);
            }
        }
    }
    UT_ASSERT(source_vertex_list_cache.size() == source_vertex_count);

    GUcreateGeometryFromSource(
        output_geo,
        source,
        source_point_list_cache,
        source_vertex_list_cache,
        source_prim_list_cache,
        ncopies);
}

void
GUcreateEmptyPackedGeometryPrims(
    GU_Detail *const output_geo,
    const exint num_packed_prims)
{
    if (num_packed_prims <= 0)
        return;

    // Create points
    GA_Offset start_ptoff = output_geo->appendPointBlock(num_packed_prims);
    GA_Offset end_ptoff = start_ptoff+num_packed_prims;

    // Create primitives and vertices
    GA_Offset start_vtxoff;
    output_geo->appendPrimitivesAndVertices(
        GU_PackedGeometry::typeId(), num_packed_prims, 1, start_vtxoff);
    GA_Offset end_vtxoff = start_vtxoff+num_packed_prims;

    // Wire the vertices to the points.
    // There are no shared points, so it's relatively easy.
    GA_ATITopology *const vertexToPoint = output_geo->getTopology().getPointRef();
    GA_ATITopology *const pointToVertex = output_geo->getTopology().getVertexRef();
    if (vertexToPoint)
    {
        UTparallelForLightItems(GA_SplittableRange(GA_Range(output_geo->getVertexMap(), start_vtxoff, end_vtxoff)),
            geo_SetTopoMappedParallel<int>(vertexToPoint, start_ptoff, start_vtxoff, nullptr));
    }
    if (pointToVertex)
    {
        UTparallelForLightItems(GA_SplittableRange(GA_Range(output_geo->getPointMap(), start_ptoff, end_ptoff)),
            geo_SetTopoMappedParallel<int>(pointToVertex, start_vtxoff, start_ptoff, nullptr));
    }
}

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
    const GU_Detail *const target,
    const GU_CopyToPointsCache::TargetAttribInfoMap *const target_attrib_info,
    const GU_CopyToPointsCache::TargetAttribInfoMap *const target_group_info,
    const exint *const target_to_piecei,
    const UT_Array<exint> *const owner_piece_offset_starts,
    const GU_CopyToPointsCache::PieceData *const piece_data)
{
    if (num_target_points <= 0)
        return;

    using AttribCombineMethod = GU_CopyToPointsCache::AttribCombineMethod;

    UT_ASSERT(source_offset_lists != nullptr);
    // If target is present or transforms are applied, cache is required.
    UT_ASSERT((target == nullptr && no_transforms) || cache != nullptr);
    const exint num_output_elements =
        output_geo->getNumVertices()   * num_source_attribs[GA_ATTRIB_VERTEX] +
        output_geo->getNumPoints()     * num_source_attribs[GA_ATTRIB_POINT] +
        output_geo->getNumPrimitives() * num_source_attribs[GA_ATTRIB_PRIMITIVE];
    const bool copy_source_attribs_in_parallel = num_output_elements >= 4096;
    UT_TaskList task_list;
    for (int owneri = 0; owneri < 3; ++owneri)
    {
        const exint *piece_offset_starts = nullptr;
        const exint *piece_offset_starts_end = nullptr;
        if (owner_piece_offset_starts)
        {
            auto &array = owner_piece_offset_starts[owneri];
            piece_offset_starts = array.getArray();
            piece_offset_starts_end = array.getArray() + array.size();
        }

        const GA_OffsetList &source_offset_list = source_offset_lists[owneri];
        GA_AttributeOwner owner = GA_AttributeOwner(owneri);
        const GA_IndexMap &index_map = output_geo->getIndexMap(owner);
        if (index_map.indexSize() == 0)
            continue;

        // To get the start offset of the copied geometry without the overhead of using a GA_Iterator
        // (which copyies the GA_Range), we just use iterateCreate and iterateRewind directly.
        GA_IteratorState iterator_state;
        output_splittable_ranges[owneri].iterateCreate(iterator_state);
        GA_Offset start_offset;
        GA_Offset first_block_end;
        output_splittable_ranges[owneri].iterateRewind(iterator_state, start_offset, first_block_end);

        for (auto it = output_geo->getAttributeDict(owner).begin(GA_SCOPE_PUBLIC); !it.atEnd(); ++it)
        {
            GA_Attribute *output_attrib = it.attrib();
            const UT_StringHolder &name = output_attrib->getName();
            const GA_Attribute *source_attrib = source->findAttribute(owner, name);
            if (!source_attrib)
                continue;

            // Check for interactions with target attributes.
            AttribCombineMethod target_method = AttribCombineMethod::NONE;
            if (target)
            {
                auto target_it = target_attrib_info->find(name);
                if (!target_it.atEnd())
                {
                    if (target_it->second.myCopyTo == owner)
                    {
                        target_method = target_it->second.myCombineMethod;

                        // Target attributes take precedence if copying.
                        if (target_method == AttribCombineMethod::COPY)
                            continue;
                    }
                    else if (target_it->second.myCombineMethod == AttribCombineMethod::COPY &&
                        target_it->second.myCopyTo == guConflictAttribOwner(owner))
                    {
                        // Target attributes take precedence if copying,
                        // including if destination type differs between point and vertex.
                        // NOTE: We don't have to check point vs. vertex for
                        //       mult/add/sub, because we already forced target type
                        //       to match source type in those situations.
                        continue;
                    }
                }
            }

            AttribCombineMethod prev_target_method = AttribCombineMethod::NONE;
            GU_CopyToPointsCache::TargetAttribInfoMap::iterator prev_target_it;
            bool target_changed = false;
            if (target)
            {
                prev_target_it = cache->myTargetAttribInfo.find(name);
                if (!prev_target_it.atEnd())
                {
                    if (prev_target_it->second.myCopyTo == owner)
                    {
                        prev_target_method = prev_target_it->second.myCombineMethod;

                        if (prev_target_method != AttribCombineMethod::NONE)
                        {
                            const GA_Attribute *target_attrib = target->findPointAttribute(name);
                            GA_DataId prev_target_dataid = prev_target_it->second.myDataID;
                            target_changed = !target_attrib ||
                                !GAisValid(prev_target_dataid) ||
                                !GAisValid(target_attrib->getDataId()) ||
                                target_attrib->getDataId() != prev_target_dataid;
                        }
                    }
                }
                if (target_method != prev_target_method)
                {
                    target_changed = true;
                }
            }

            GA_ATINumeric *output_numeric = GA_ATINumeric::cast(output_attrib);
            if (output_numeric)
            {
                const GA_ATINumeric *source_numeric = UTverify_cast<const GA_ATINumeric*>(source_attrib);

                GA_TypeInfo transform_type = no_transforms ? GA_TYPE_VOID : guGetTransformTypeInfo(output_numeric, has_transform_matrices);

                if (transform_type != GA_TYPE_VOID)
                {
                    UT_ASSERT(cache != nullptr);
                    // If !topology_changed, and !transforms_changed, and the
                    // source_attrib data ID hasn't changed, don't re-transform.
                    if (!topology_changed && !transforms_changed && !target_changed)
                    {
                        auto it = cache->mySourceAttribDataIDs[owneri].find(name);
                        if (!it.atEnd() && it->second != GA_INVALID_DATAID && it->second == source_attrib->getDataId())
                        {
                            continue;
                        }
                    }
                    cache->mySourceAttribDataIDs[owneri][name] = source_attrib->getDataId();
                    output_attrib->bumpDataId();

                    // Remove any entry from the target cache, so that the code for
                    // applying target attributes is forced to re-apply the operation.
                    if (target && !prev_target_it.atEnd())
                        cache->myTargetAttribInfo.erase(prev_target_it);

                    guApplyTransformToAttribute(
                        cache,
                        transform_type,
                        output_numeric,
                        source_numeric,
                        source_offset_list,
                        copy_source_attribs_in_parallel,
                        task_list,
                        output_splittable_ranges[owneri],
                        start_offset,
                        target_to_piecei,
                        num_target_points,
                        piece_offset_starts,
                        piece_offset_starts_end,
                        piece_data);
                }
                else
                {
                    if (cache != nullptr)
                    {
                        // If !topology_changed and the source_attrib data ID hasn't changed since last time, don't re-copy.
                        if (!topology_changed && !target_changed)
                        {
                            auto it = cache->mySourceAttribDataIDs[owneri].find(name);
                            if (!it.atEnd() && it->second != GA_INVALID_DATAID && it->second == source_attrib->getDataId())
                            {
                                // NOTE: no_transforms should only be false when copying for a packed primitive,
                                //       in which case, there are never transforms, so we don't have to worry
                                //       about it changing whether an attribute is transformed or copied.
                                if (no_transforms || has_transform_matrices || !had_transform_matrices)
                                    continue;

                                // If this attribute was transforming last time and is no longer transforming,
                                // it needs to be copied, even though it hasn't changed.
                                GA_TypeInfo old_transform_type = guGetTransformTypeInfo(output_numeric, has_transform_matrices);
                                if (old_transform_type == GA_TYPE_VOID)
                                {
                                    // Wasn't transforming last time either,
                                    // and the attribute hasn't changed, so we can skip it.
                                    continue;
                                }
                            }
                        }
                        cache->mySourceAttribDataIDs[owneri][name] = source_attrib->getDataId();

                        // Remove any entry from the target cache, so that the code for
                        // applying target attributes is forced to re-apply the operation.
                        if (target && !prev_target_it.atEnd())
                            cache->myTargetAttribInfo.erase(prev_target_it);
                    }

                    output_attrib->bumpDataId();

                    auto &&functor = [output_numeric,source_numeric,&source_offset_list,
                        piece_offset_starts,piece_offset_starts_end,num_target_points,
                        target_to_piecei,piece_data,owneri,start_offset](const GA_SplittableRange &r)
                    {
                        auto &output_data = output_numeric->getData();
                        const auto &source_data = source_numeric->getData();

                        GA_Offset start;
                        GA_Offset end;
                        for (GA_Iterator it(r); it.fullBlockAdvance(start, end); )
                        {
                            exint targeti;
                            exint piece_elementi;
                            exint piece_element_count;
                            const GA_OffsetList *piece_offset_list;
                            guFindStartInTarget(start, targeti, piece_elementi, piece_element_count,
                                &piece_offset_list, start_offset, piece_offset_starts, piece_offset_starts_end,
                                num_target_points, target_to_piecei, piece_data, owneri,
                                &source_offset_list, source_offset_list.size());

                            // FIXME: Consider switching on type and tuple size outside the loop.
                            for (GA_Offset dest_off = start; dest_off < end; ++dest_off)
                            {
                                GA_Offset source_off = (*piece_offset_list)[piece_elementi];
                                output_data.moveRange(source_data, source_off, dest_off, GA_Offset(1));

                                guIteratePieceElement(piece_elementi, piece_element_count, targeti, piece_offset_starts,
                                    num_target_points, target_to_piecei, piece_data, owneri, piece_offset_list);
                            }
                        }
                    };
                    if (copy_source_attribs_in_parallel)
                        UTparallelForAppendToTaskList(task_list, output_splittable_ranges[owneri], functor);
                    else
                        functor(output_splittable_ranges[owneri]);
                }

            }
            else
            {
                if (cache != nullptr)
                {
                    // If !topology_changed and the source_attrib data ID hasn't changed since last time, don't re-copy.
                    if (!topology_changed && !target_changed)
                    {
                        auto it = cache->mySourceAttribDataIDs[owneri].find(name);
                        if (!it.atEnd() && it->second != GA_INVALID_DATAID && it->second == source_attrib->getDataId())
                            continue;
                    }
                    cache->mySourceAttribDataIDs[owneri][name] = source_attrib->getDataId();

                    // Remove any entry from the target cache, so that the code for
                    // applying target attributes is forced to re-apply the operation.
                    if (target && !prev_target_it.atEnd())
                        cache->myTargetAttribInfo.erase(prev_target_it);
                }

                output_attrib->bumpDataId();

                GA_ATIString *output_string = GA_ATIString::cast(output_attrib);
                if (output_string)
                {
                    // Special case for string attributes for batch adding of string references.
                    const GA_ATIString *source_string = UTverify_cast<const GA_ATIString *>(source_attrib);
                    auto &&functor = [output_string,source_string,&source_offset_list,
                        piece_offset_starts,piece_offset_starts_end,num_target_points,
                        target_to_piecei,piece_data,start_offset,owneri](const GA_SplittableRange &r)
                    {
                        GA_RWBatchHandleS output_string_handle(output_string);
                        UT_ASSERT_P(output_string_handle.isValid());
                        GA_ROHandleS source_string_handle(source_string);
                        GA_Offset start;
                        GA_Offset end;
                        for (GA_Iterator it(r); it.fullBlockAdvance(start, end); )
                        {
                            exint targeti;
                            exint piece_elementi;
                            exint piece_element_count;
                            const GA_OffsetList *piece_offset_list;
                            guFindStartInTarget(start, targeti, piece_elementi, piece_element_count,
                                &piece_offset_list, start_offset, piece_offset_starts, piece_offset_starts_end,
                                num_target_points, target_to_piecei, piece_data, owneri,
                                &source_offset_list, source_offset_list.size());

                            for (GA_Offset dest_off = start; dest_off < end; ++dest_off)
                            {
                                GA_Offset source_off = (*piece_offset_list)[piece_elementi];
                                output_string_handle.set(dest_off, source_string_handle.get(source_off));

                                guIteratePieceElement(piece_elementi, piece_element_count, targeti, piece_offset_starts,
                                    num_target_points, target_to_piecei, piece_data, owneri, piece_offset_list);
                            }
                        }
                    };
                    if (copy_source_attribs_in_parallel)
                        UTparallelForAppendToTaskList(task_list, output_splittable_ranges[owneri], functor);
                    else
                        functor(output_splittable_ranges[owneri]);
                }
                else
                {
                    auto &&functor = [output_attrib,source_attrib,&source_offset_list,
                        piece_offset_starts,piece_offset_starts_end,num_target_points,
                        target_to_piecei,piece_data,start_offset,owneri](const GA_SplittableRange &r)
                    {
                        GA_Offset start;
                        GA_Offset end;
                        for (GA_Iterator it(r); it.fullBlockAdvance(start, end); )
                        {
                            exint targeti;
                            exint piece_elementi;
                            exint piece_element_count;
                            const GA_OffsetList *piece_offset_list;
                            guFindStartInTarget(start, targeti, piece_elementi, piece_element_count,
                                &piece_offset_list, start_offset, piece_offset_starts, piece_offset_starts_end,
                                num_target_points, target_to_piecei, piece_data, owneri,
                                &source_offset_list, source_offset_list.size());

                            for (GA_Offset dest_off = start; dest_off < end; ++dest_off)
                            {
                                GA_Offset source_off = (*piece_offset_list)[piece_elementi];
                                output_attrib->copy(dest_off, *source_attrib, source_off);

                                guIteratePieceElement(piece_elementi, piece_element_count, targeti, piece_offset_starts,
                                    num_target_points, target_to_piecei, piece_data, owneri, piece_offset_list);
                            }
                        }
                    };
                    if (copy_source_attribs_in_parallel)
                        UTparallelForAppendToTaskList(task_list, output_splittable_ranges[owneri], functor);
                    else
                        functor(output_splittable_ranges[owneri]);
                }
            }
        }
    }

    // Don't forget the element groups
    for (int owneri = 0; owneri < 3; ++owneri)
    {
        const exint *piece_offset_starts = nullptr;
        const exint *piece_offset_starts_end = nullptr;
        if (owner_piece_offset_starts)
        {
            auto &array = owner_piece_offset_starts[owneri];
            piece_offset_starts = array.getArray();
            piece_offset_starts_end = array.getArray() + array.size();
        }

        const GA_OffsetList &source_offset_list = source_offset_lists[owneri];
        GA_AttributeOwner owner = GA_AttributeOwner(owneri);
        const GA_IndexMap &index_map = output_geo->getIndexMap(owner);
        if (index_map.indexSize() == 0)
            continue;

        // To get the start offset of the copied geometry without the overhead of using a GA_Iterator
        // (which copyies the GA_Range), we just use iterateCreate and iterateRewind directly.
        GA_IteratorState iterator_state;
        output_splittable_ranges[owner].iterateCreate(iterator_state);
        GA_Offset start_offset;
        GA_Offset first_block_end;
        output_splittable_ranges[owner].iterateRewind(iterator_state, start_offset, first_block_end);

        for (auto it = output_geo->getElementGroupTable(owner).beginTraverse(); !it.atEnd(); ++it)
        {
            GA_ElementGroup *output_group = it.group();
            UT_ASSERT_MSG(!output_group->isInternal(),
                "GUremoveUnnecessaryAttribs removes internal groups and "
                "GUaddAttributesFromSourceOrTarget doesn't add them");

            const UT_StringHolder &name = output_group->getName();

            // Check for interactions with target groups.
            AttribCombineMethod target_method = AttribCombineMethod::NONE;
            if (target)
            {
                auto target_it = target_group_info->find(name);
                if (!target_it.atEnd())
                {
                    if (target_it->second.myCopyTo == owner)
                    {
                        target_method = target_it->second.myCombineMethod;

                        // Target groups take precedence if copying.
                        if (target_method == AttribCombineMethod::COPY)
                            continue;
                    }
                    else if (target_it->second.myCombineMethod == AttribCombineMethod::COPY &&
                        target_it->second.myCopyTo == guConflictAttribOwner(owner))
                    {
                        // Target groups take precedence if copying,
                        // including if destination type differs between point and vertex.
                        // NOTE: We don't have to check point vs. vertex for
                        //       mult/add/sub, because we already forced target type
                        //       to match source type in those situations.
                        continue;
                    }
                }
            }

            AttribCombineMethod prev_target_method = AttribCombineMethod::NONE;
            GU_CopyToPointsCache::TargetAttribInfoMap::iterator prev_target_it;
            bool target_changed = false;
            if (target)
            {
                prev_target_it = cache->myTargetGroupInfo.find(name);
                if (!prev_target_it.atEnd())
                {
                    if (prev_target_it->second.myCopyTo == owner)
                    {
                        prev_target_method = prev_target_it->second.myCombineMethod;

                        if (prev_target_method != AttribCombineMethod::NONE)
                        {
                            const GA_Attribute *target_group = target->findPointGroup(name);
                            GA_DataId prev_target_dataid = prev_target_it->second.myDataID;
                            target_changed = !target_group ||
                                !GAisValid(prev_target_dataid) ||
                                !GAisValid(target_group->getDataId()) ||
                                target_group->getDataId() != prev_target_dataid;
                        }
                    }
                }
                if (target_method != prev_target_method)
                {
                    target_changed = true;
                }
            }

            const GA_ElementGroup *source_group = source->getElementGroupTable(owner).find(name);
            UT_ASSERT(source_group);

            if (cache != nullptr)
            {
                // If !topology_changed and the source_group data ID hasn't changed since last time, don't re-copy.
                if (!topology_changed && !target_changed)
                {
                    auto it = cache->mySourceGroupDataIDs[owneri].find(name);
                    if (!it.atEnd() && it->second != GA_INVALID_DATAID && it->second == source_group->getDataId())
                        continue;
                }
                cache->mySourceGroupDataIDs[owneri][name] = source_group->getDataId();

                // Remove any entry from the target cache, so that the code for
                // applying target groups is forced to re-apply the operation.
                if (target && !prev_target_it.atEnd())
                    cache->myTargetAttribInfo.erase(prev_target_it);
            }

            output_group->bumpDataId();
            output_group->invalidateGroupEntries();

            auto &&functor = [output_group,source_group,&source_offset_list,
                piece_offset_starts,piece_offset_starts_end,owneri,target_to_piecei,piece_data,
                num_target_points,start_offset](const GA_SplittableRange &r)
            {
                GA_Offset start;
                GA_Offset end;
                for (GA_Iterator it(r); it.fullBlockAdvance(start, end); )
                {
                    exint targeti;
                    exint piece_elementi;
                    exint piece_element_count;
                    const GA_OffsetList *piece_offset_list;
                    guFindStartInTarget(start, targeti, piece_elementi, piece_element_count,
                        &piece_offset_list, start_offset, piece_offset_starts, piece_offset_starts_end,
                        num_target_points, target_to_piecei, piece_data, owneri,
                        &source_offset_list, source_offset_list.size());

                    for (GA_Offset dest_off = start; dest_off < end; ++dest_off)
                    {
                        GA_Offset source_off = (*piece_offset_list)[piece_elementi];
                        output_group->setElement(dest_off, source_group->contains(source_off));

                        guIteratePieceElement(piece_elementi, piece_element_count, targeti, piece_offset_starts,
                            num_target_points, target_to_piecei, piece_data, owneri, piece_offset_list);
                    }
                }
            };
            if (copy_source_attribs_in_parallel)
                UTparallelForAppendToTaskList(task_list, output_splittable_ranges[owneri], functor);
            else
                functor(output_splittable_ranges[owneri]);
        }
    }

    // Don't forget the edge groups, either.
    // FIXME: This doesn't work for the piece attribute case!!!
    GA_PageArray<exint,1> source_to_sourcei;
    bool all_source_points;
    if (output_geo->edgeGroups().entries() > 0 && piece_data == nullptr)
    {
        // We need a source-point-offset-to-source-point-in-group mapping.
        const GA_OffsetList &source_point_list = source_offset_lists[GA_ATTRIB_POINT];
        all_source_points = source_point_list.isSame(source->getPointMap().getOffsetFromIndexList());
        if (!all_source_points)
        {
            source_to_sourcei.setSize(source->getNumPointOffsets(), exint(-1));
            for (exint i = 0, n = source_point_list.size(); i < n; ++i)
            {
                source_to_sourcei.set(source_point_list[i], i);
            }
        }
    }
    for (auto it = output_geo->edgeGroups().beginTraverse(); piece_data == nullptr && !it.atEnd(); ++it)
    {
        GA_EdgeGroup *output_group = it.group();
        UT_ASSERT_MSG(!output_group->isInternal(),
            "GUremoveUnnecessaryAttribs removes internal groups and "
            "GUaddAttributesFromSourceOrTarget doesn't add them");
        const GA_EdgeGroup *source_group = source->findEdgeGroup(output_group->getName());
        UT_ASSERT(source_group);

        if (cache != nullptr)
        {
            // If !topology_changed and the source_group data ID hasn't changed since last time, don't re-copy.
            if (!topology_changed)
            {
                auto it = cache->mySourceEdgeGroupDataIDs.find(source_group->getName());
                if (!it.atEnd() && it->second != GA_INVALID_DATAID && it->second == source_group->getDataId())
                    continue;
            }
            cache->mySourceEdgeGroupDataIDs[source_group->getName()] = source_group->getDataId();
        }
        output_group->bumpDataId();

        // There are two main possible approaches:
        // A) Iterate through source_group, checking whether both points are in source_pointgroup (if source_pointgroup is non-null)
        // B) Iterate through all edges of one copy in output_geo, checking whether corresponding edges are in source_group
        // After either, the results from the first copy could be easily duplicated for other copies,
        // but which one is more efficient depends on the portion of source_group that's in the output
        // and the portion of output_geo that is not in source_group.
        // I've semi-arbitrarily chosen A, under the assumption that most edge groups are a small
        // portion of the total edges in the geometry and that the most common case is to copy
        // all of the input geometry.

        const exint source_point_count = source_offset_lists[GA_ATTRIB_POINT].size();

        for (auto it = source_group->begin(); !it.atEnd(); ++it)
        {
            const GA_Edge source_edge = it.getEdge();
            GA_Index index0;
            GA_Index index1;
            if (all_source_points)
            {
                index0 = source->pointIndex(source_edge.p0());
                index1 = source->pointIndex(source_edge.p1());
            }
            else
            {
                index0 = GA_Index(source_to_sourcei.get(source_edge.p0()));
                index1 = GA_Index(source_to_sourcei.get(source_edge.p1()));

                // Both of the points must be in the source point list.
                if (index0 == -1 || index1 == -1)
                    continue;
            }
            GA_Edge output_edge(
                output_geo->pointOffset(index0),
                output_geo->pointOffset(index1));

            UT_ASSERT(num_target_points > 0);
            output_group->add(output_edge);

            for (exint copy = 1; copy < num_target_points; ++copy)
            {
                output_edge.p0() += source_point_count;
                output_edge.p1() += source_point_count;
                output_group->add(output_edge);
            }
        }
    }

    // If we're transforming, apply the transform to transforming primitives like in GEO_Detail::transform
    if (!no_transforms && output_geo->hasTransformingPrimitives() && (topology_changed || transforms_changed))
    {
        output_geo->getPrimitiveList().bumpDataId();

        UT_ASSERT(cache != nullptr);

        const exint *piece_offset_starts = nullptr;
        const exint *piece_offset_starts_end = nullptr;
        if (owner_piece_offset_starts)
        {
            auto &array = owner_piece_offset_starts[GA_ATTRIB_PRIMITIVE];
            piece_offset_starts = array.getArray();
            piece_offset_starts_end = array.getArray() + array.size();
        }

        const GA_OffsetList &source_prim_list = source_offset_lists[GA_ATTRIB_PRIMITIVE];
        const UT_Matrix3D *transform_matrices_3d = cache->myTransformMatrices3D.get();
        const UT_Vector3D *transform_translates_3d = cache->myTransformTranslates3D.get();
        auto &&functor = [transform_matrices_3d,transform_translates_3d,output_geo,
            piece_offset_starts,piece_offset_starts_end,target_to_piecei,piece_data,
            num_target_points,topology_changed,source,&source_prim_list](const GA_SplittableRange &r)
        {
            const exint source_prim_count = source_prim_list.size();
            GA_Offset start_primoff = output_geo->primitiveOffset(GA_Index(0));
            GA_Offset start;
            GA_Offset end;
            for (GA_Iterator it(r); it.fullBlockAdvance(start, end); )
            {
                exint targeti;
                exint piece_elementi;
                exint piece_element_count;
                const GA_OffsetList *piece_offset_list;
                guFindStartInTarget(start, targeti, piece_elementi, piece_element_count,
                    &piece_offset_list, start_primoff, piece_offset_starts, piece_offset_starts_end,
                    num_target_points, target_to_piecei, piece_data, GA_ATTRIB_PRIMITIVE,
                    &source_prim_list, source_prim_count);

                UT_Matrix4D transform;
                if (transform_matrices_3d)
                    transform = UT_Matrix4D(transform_matrices_3d[targeti]);
                else
                    transform.identity();
                transform.setTranslates(transform_translates_3d[targeti]);

                // TODO: Remove the cast to UT_Matrix4 once
                //       GEO_Primitive::transform accepts UT_Matrix4D.
                UT_Matrix4 transformf(transform);

                for (GA_Offset dest_off = start; dest_off < end; ++dest_off)
                {
                    GEO_Primitive *prim = output_geo->getGEOPrimitive(dest_off);

                    if (!topology_changed)
                    {
                        // We haven't re-copied the untransformed primitive,
                        // so we need to copy it now.  Note that VDB primitives
                        // sometimes transform actual voxel data when transformed,
                        // so we can't just call setLocalTransform.
                        GA_Offset source_off = (*piece_offset_list)[piece_elementi];
                        const GA_Primitive *source_prim = source->getPrimitive(source_off);
                        prim->copySubclassData(source_prim);
                    }

                    prim->transform(transformf);

                    // The iteration below is similar to guIteratePieceElement above,
                    // but also assembling target transforms.
                    ++piece_elementi;
                    // NOTE: This must be while instead of if, because there can be zero primitives in a piece.
                    while (piece_elementi >= piece_element_count)
                    {
                        piece_elementi = 0;
                        ++targeti;

                        if (targeti >= num_target_points)
                            break;

                        if (transform_matrices_3d)
                            transform = UT_Matrix4D(transform_matrices_3d[targeti]);
                        else
                            transform.identity();
                        transform.setTranslates(transform_translates_3d[targeti]);
                        // TODO: Remove the cast to UT_Matrix4 once
                        //       GEO_Primitive::transform accepts UT_Matrix4D.
                        transformf = UT_Matrix4(transform);

                        if (piece_offset_starts != nullptr)
                        {
                            exint piecei = target_to_piecei[targeti];
                            const GU_CopyToPointsCache::PieceData &current_piece = piece_data[piecei];
                            piece_offset_list = &current_piece.mySourceOffsetLists[GA_ATTRIB_PRIMITIVE];
                            piece_element_count = piece_offset_list->size();
                        }
                    }
                }
            }
        };
        if (copy_source_attribs_in_parallel)
            UTparallelForAppendToTaskList(task_list, output_splittable_ranges[GA_ATTRIB_PRIMITIVE], functor);
        else
            functor(output_splittable_ranges[GA_ATTRIB_PRIMITIVE]);
    }

    if (copy_source_attribs_in_parallel)
        task_list.spawnRootAndWait();
}

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
    const exint *const target_to_piecei,
    const UT_Array<exint> *const owner_piece_offset_starts,
    const GU_CopyToPointsCache::PieceData *const piece_data)
{
    if (ncopies <= 0)
        return;

    using AttribCombineMethod = GU_CopyToPointsCache::AttribCombineMethod;

    const exint num_target_output_elements =
        output_geo->getNumVertices()   * num_target_attribs[GA_ATTRIB_VERTEX] +
        output_geo->getNumPoints()     * num_target_attribs[GA_ATTRIB_POINT] +
        output_geo->getNumPrimitives() * num_target_attribs[GA_ATTRIB_PRIMITIVE];
    const bool copy_target_attribs_in_parallel = num_target_output_elements >= 4096;

    if (num_target_output_elements <= 0)
        return;

    exint source_element_counts[3] =
    {
        source_vertex_count,
        source_point_count,
        source_prim_count
    };
    SYS_STATIC_ASSERT_MSG(int(GA_ATTRIB_VERTEX)==0 && int(GA_ATTRIB_POINT)==1 && int(GA_ATTRIB_PRIMITIVE)==2,
        "Arrays above and loop below are assuming the order of GA_AttributeOwner enum");

    const exint num_target_points = target_point_list.size();

    UT_TaskList task_list;
    for (auto it = target_attrib_info.begin(); !it.atEnd(); ++it)
    {
        const UT_StringHolder &name = it->first;
        const GA_Attribute *target_attrib = target->findPointAttribute(name);
        UT_ASSERT(target_attrib != nullptr);

        const GA_AttributeOwner owner = it->second.myCopyTo;
        GA_Attribute *output_attrib = output_geo->findAttribute(owner, name);
        UT_ASSERT(output_attrib != nullptr);

        const GA_IndexMap &index_map = output_geo->getIndexMap(owner);
        if (index_map.indexSize() == 0)
            continue;

        // To get the start offset of the copied geometry without the overhead of using a GA_Iterator
        // (which copyies the GA_Range), we just use iterateCreate and iterateRewind directly.
        GA_IteratorState iterator_state;
        output_splittable_ranges[owner].iterateCreate(iterator_state);
        GA_Offset start_offset;
        GA_Offset first_block_end;
        output_splittable_ranges[owner].iterateRewind(iterator_state, start_offset, first_block_end);

        exint num_elements_per_copy = source_element_counts[owner];
        const AttribCombineMethod method = it->second.myCombineMethod;
        if (method == AttribCombineMethod::NONE)
            continue;

        if (!topology_changed)
        {
            // If unchanged since last cook, no need to re-apply operation.
            // NOTE: GUcopyAttributesFromSource removed cache entries for
            //       attributes that were re-copied from source.
            auto prev_it = cache->myTargetAttribInfo.find(name);
            if (!prev_it.atEnd() &&
                prev_it->second.myDataID == it->second.myDataID &&
                prev_it->second.myCopyTo == it->second.myCopyTo &&
                prev_it->second.myCombineMethod == it->second.myCombineMethod)
            {
                continue;
            }
        }

        const exint *piece_offset_starts = nullptr;
        const exint *piece_offset_starts_end = nullptr;
        if (owner_piece_offset_starts)
        {
            auto &array = owner_piece_offset_starts[owner];
            piece_offset_starts = array.getArray();
            piece_offset_starts_end = array.getArray() + array.size();
        }

        output_attrib->bumpDataId();

        if (method == AttribCombineMethod::COPY)
        {
            // Copy attribute values from target to output_geo
            // FIXME: Specialize for numeric and string attributes to reduce
            //        performance impact of the GA_Attribute::copy() virtual function calls.
            auto &&functor = [output_attrib,target_attrib,&target_point_list,num_elements_per_copy,
                start_offset,piece_offset_starts,piece_offset_starts_end,
                num_target_points,target_to_piecei,piece_data,owner](const GA_SplittableRange &r)
            {
                GA_Offset start;
                GA_Offset end;
                for (GA_Iterator it(r); it.fullBlockAdvance(start, end); )
                {
                    exint targeti;
                    exint piece_elementi;
                    exint piece_element_count;
                    guFindStartInTarget(start, targeti, piece_elementi, piece_element_count,
                        nullptr, start_offset, piece_offset_starts, piece_offset_starts_end,
                        num_target_points, target_to_piecei, piece_data, owner,
                        nullptr, num_elements_per_copy);

                    GA_Offset target_off = target_point_list[targeti];

                    // FIXME: Find longer contiguous spans to copy, for better performance.
                    for (GA_Offset dest_off = start; dest_off < end; ++dest_off)
                    {
                        output_attrib->copy(dest_off, *target_attrib, target_off);

                        guIteratePieceElementOff(piece_elementi, piece_element_count, targeti, piece_offset_starts,
                            num_target_points, target_to_piecei, piece_data, owner, target_off, target_point_list);
                    }
                }
            };
            if (copy_target_attribs_in_parallel)
                UTparallelForAppendToTaskList(task_list, output_splittable_ranges[owner], functor);
            else
                functor(output_splittable_ranges[owner]);
        }
        else
        {
            GA_ATINumeric *output_numeric = UTverify_cast<GA_ATINumeric *>(output_attrib);
            const GA_ATINumeric *target_numeric = UTverify_cast<const GA_ATINumeric *>(target_attrib);
            const exint min_tuple_size = SYSmin(output_numeric->getTupleSize(), target_numeric->getTupleSize());
            GA_PageArray<void, -1> &output_data = output_numeric->getData();
            const GA_PageArray<void, -1> &target_data = target_numeric->getData();
            if (method == AttribCombineMethod::MULTIPLY)
            {
                // Multiply by target attribute values: output_geo (i.e. source) * target.
                auto &&functor = [&output_data,&target_data,&target_point_list,num_elements_per_copy,min_tuple_size,
                    start_offset,piece_offset_starts,piece_offset_starts_end,
                    num_target_points,target_to_piecei,piece_data,owner](const GA_SplittableRange &r)
                {
                    GA_Offset start;
                    GA_Offset end;
                    for (GA_Iterator it(r); it.fullBlockAdvance(start, end); )
                    {
                        exint targeti;
                        exint piece_elementi;
                        exint piece_element_count;
                        guFindStartInTarget(start, targeti, piece_elementi, piece_element_count,
                            nullptr, start_offset, piece_offset_starts, piece_offset_starts_end,
                            num_target_points, target_to_piecei, piece_data, owner,
                            nullptr, num_elements_per_copy);

                        GA_Offset target_off = target_point_list[targeti];

                        // FIXME: Find longer contiguous spans to copy, for better performance.
                        for (GA_Offset dest_off = start; dest_off < end; ++dest_off)
                        {
                            for (exint component = 0; component < min_tuple_size; ++component)
                            {
                                // FIXME: This is just mimicking what the old code did, but
                                //        it doesn't maintain full precision for 64-bit integers,
                                //        so we can do better.
                                //        The performance probably isn't great, either.
                                // TODO: Maybe we should handle transform matrices and quaternions
                                //       in a special way, too.
                                fpreal64 existing_value = output_data.get<fpreal64>(dest_off, component);
                                fpreal64 target_value = target_data.get<fpreal64>(target_off, component);
                                fpreal64 product = existing_value*target_value;
                                output_data.set(dest_off, component, product);
                            }

                            guIteratePieceElementOff(piece_elementi, piece_element_count, targeti, piece_offset_starts,
                                num_target_points, target_to_piecei, piece_data, owner, target_off, target_point_list);
                        }
                    }
                };
                if (copy_target_attribs_in_parallel)
                    UTparallelForAppendToTaskList(task_list, output_splittable_ranges[owner], functor);
                else
                    functor(output_splittable_ranges[owner]);
            }
            else if (method == AttribCombineMethod::ADD)
            {
                // Add target attribute values: output_geo (i.e. source) + target.
                auto &&functor = [&output_data,&target_data,&target_point_list,num_elements_per_copy,min_tuple_size,
                    start_offset,piece_offset_starts,piece_offset_starts_end,
                    num_target_points,target_to_piecei,piece_data,owner](const GA_SplittableRange &r)
                {
                    GA_Offset start;
                    GA_Offset end;
                    for (GA_Iterator it(r); it.fullBlockAdvance(start, end); )
                    {
                        exint targeti;
                        exint piece_elementi;
                        exint piece_element_count;
                        guFindStartInTarget(start, targeti, piece_elementi, piece_element_count,
                            nullptr, start_offset, piece_offset_starts, piece_offset_starts_end,
                            num_target_points, target_to_piecei, piece_data, owner,
                            nullptr, num_elements_per_copy);

                        GA_Offset target_off = target_point_list[targeti];

                        // FIXME: Find longer contiguous spans to copy, for better performance.
                        for (GA_Offset dest_off = start; dest_off < end; ++dest_off)
                        {
                            for (exint component = 0; component < min_tuple_size; ++component)
                            {
                                // FIXME: This is just mimicking what the old code did, but
                                //        it doesn't maintain full precision for 64-bit integers,
                                //        so we can do better.
                                //        The performance probably isn't great, either.
                                fpreal64 existing_value = output_data.get<fpreal64>(dest_off, component);
                                fpreal64 target_value = target_data.get<fpreal64>(target_off, component);
                                fpreal64 sum = existing_value + target_value;
                                output_data.set(dest_off, component, sum);
                            }

                            guIteratePieceElementOff(piece_elementi, piece_element_count, targeti, piece_offset_starts,
                                num_target_points, target_to_piecei, piece_data, owner, target_off, target_point_list);
                        }
                    }
                };
                if (copy_target_attribs_in_parallel)
                    UTparallelForAppendToTaskList(task_list, output_splittable_ranges[owner], functor);
                else
                    functor(output_splittable_ranges[owner]);
            }
            else // (method == AttribCombineMethod::SUBTRACT)
            {
                // Subtract target attribute values: output_geo (i.e. source or zero) - target.
                auto &&functor = [&output_data,&target_data,&target_point_list,num_elements_per_copy,min_tuple_size,
                    start_offset,piece_offset_starts,piece_offset_starts_end,
                    num_target_points,target_to_piecei,piece_data,owner](const GA_SplittableRange &r)
                {
                    GA_Offset start;
                    GA_Offset end;
                    for (GA_Iterator it(r); it.fullBlockAdvance(start, end); )
                    {
                        exint targeti;
                        exint piece_elementi;
                        exint piece_element_count;
                        guFindStartInTarget(start, targeti, piece_elementi, piece_element_count,
                            nullptr, start_offset, piece_offset_starts, piece_offset_starts_end,
                            num_target_points, target_to_piecei, piece_data, owner,
                            nullptr, num_elements_per_copy);

                        GA_Offset target_off = target_point_list[targeti];

                        // FIXME: Find longer contiguous spans to copy, for better performance.
                        for (GA_Offset dest_off = start; dest_off < end; ++dest_off)
                        {
                            for (exint component = 0; component < min_tuple_size; ++component)
                            {
                                // FIXME: This is just mimicking what the old code did, but
                                //        it doesn't maintain full precision for 64-bit integers,
                                //        so we can do better.
                                //        The performance probably isn't great, either.
                                fpreal64 existing_value = output_data.get<fpreal64>(dest_off, component);
                                fpreal64 target_value = target_data.get<fpreal64>(target_off, component);
                                fpreal64 difference = existing_value - target_value;
                                output_data.set(dest_off, component, difference);
                            }

                            guIteratePieceElementOff(piece_elementi, piece_element_count, targeti, piece_offset_starts,
                                num_target_points, target_to_piecei, piece_data, owner, target_off, target_point_list);
                        }
                    }
                };
                if (copy_target_attribs_in_parallel)
                    UTparallelForAppendToTaskList(task_list, output_splittable_ranges[owner], functor);
                else
                    functor(output_splittable_ranges[owner]);
            }
        }
    }
    for (auto it = target_group_info.begin(); !it.atEnd(); ++it)
    {
        const UT_StringHolder &name = it->first;
        const GA_PointGroup *target_group = target->findPointGroup(name);
        UT_ASSERT(target_group != nullptr);

        const GA_AttributeOwner owner = it->second.myCopyTo;
        GA_ElementGroup *output_group = output_geo->findElementGroup(owner, name);
        UT_ASSERT(output_group != nullptr);

        const GA_IndexMap &index_map = output_geo->getIndexMap(owner);
        if (index_map.indexSize() == 0)
            continue;

        // To get the start offset of the copied geometry without the overhead of using a GA_Iterator
        // (which copyies the GA_Range), we just use iterateCreate and iterateRewind directly.
        GA_IteratorState iterator_state;
        output_splittable_ranges[owner].iterateCreate(iterator_state);
        GA_Offset start_offset;
        GA_Offset first_block_end;
        output_splittable_ranges[owner].iterateRewind(iterator_state, start_offset, first_block_end);

        const AttribCombineMethod method = it->second.myCombineMethod;
        if (method == AttribCombineMethod::NONE)
            continue;

        if (!topology_changed)
        {
            // If unchanged since last cook, no need to re-apply operation.
            // NOTE: GUcopyAttributesFromSource removed cache entries for
            //       attributes that were re-copied from source.
            auto prev_it = cache->myTargetAttribInfo.find(name);
            if (!prev_it.atEnd() &&
                prev_it->second.myDataID == it->second.myDataID &&
                prev_it->second.myCopyTo == it->second.myCopyTo &&
                prev_it->second.myCombineMethod == it->second.myCombineMethod)
            {
                continue;
            }
        }

        const exint *piece_offset_starts = nullptr;
        const exint *piece_offset_starts_end = nullptr;
        if (owner_piece_offset_starts)
        {
            auto &array = owner_piece_offset_starts[owner];
            piece_offset_starts = array.getArray();
            piece_offset_starts_end = array.getArray() + array.size();
        }

        output_group->bumpDataId();
        output_group->invalidateGroupEntries();

        if (method == AttribCombineMethod::COPY)
        {
            // Copy group membership from target to output_geo
            exint num_elements_per_copy = source_element_counts[owner];
            auto &&functor = [output_group,target_group,&target_point_list,num_elements_per_copy,
                start_offset,piece_offset_starts,piece_offset_starts_end,
                num_target_points,target_to_piecei,piece_data,owner](const GA_SplittableRange &r)
            {
                GA_Offset start;
                GA_Offset end;
                for (GA_Iterator it(r); it.fullBlockAdvance(start, end); )
                {
                    exint targeti;
                    exint piece_elementi;
                    exint piece_element_count;
                    guFindStartInTarget(start, targeti, piece_elementi, piece_element_count,
                        nullptr, start_offset, piece_offset_starts, piece_offset_starts_end,
                        num_target_points, target_to_piecei, piece_data, owner,
                        nullptr, num_elements_per_copy);

                    GA_Offset target_off = target_point_list[targeti];
                    bool target_in_group = target_group->contains(target_off);

                    // FIXME: Find longer contiguous spans to copy, for better performance.
                    for (GA_Offset dest_off = start; dest_off < end; ++dest_off)
                    {
                        output_group->setElement(dest_off, target_in_group);

                        guIteratePieceElementGroup(piece_elementi, piece_element_count, targeti, piece_offset_starts,
                            num_target_points, target_to_piecei, piece_data, owner, target_in_group, target_point_list, target_group);
                    }
                }
            };
            if (copy_target_attribs_in_parallel)
                UTparallelForAppendToTaskList(task_list, output_splittable_ranges[owner], functor);
            else
                functor(output_splittable_ranges[owner]);
        }
        else if (method == AttribCombineMethod::MULTIPLY)
        {
            // Intersect group membership from target to output_geo
            exint num_elements_per_copy = source_element_counts[owner];
            auto &&functor = [output_group,target_group,&target_point_list,num_elements_per_copy,
                start_offset,piece_offset_starts,piece_offset_starts_end,
                num_target_points,target_to_piecei,piece_data,owner](const GA_SplittableRange &r)
            {
                GA_Offset start;
                GA_Offset end;
                for (GA_Iterator it(r); it.fullBlockAdvance(start, end); )
                {
                    exint targeti;
                    exint piece_elementi;
                    exint piece_element_count;
                    guFindStartInTarget(start, targeti, piece_elementi, piece_element_count,
                        nullptr, start_offset, piece_offset_starts, piece_offset_starts_end,
                        num_target_points, target_to_piecei, piece_data, owner,
                        nullptr, num_elements_per_copy);

                    GA_Offset target_off = target_point_list[targeti];
                    bool target_in_group = target_group->contains(target_off);

                    // FIXME: Find longer contiguous spans to copy, for better performance.
                    for (GA_Offset dest_off = start; dest_off < end; ++dest_off)
                    {
                        if (!target_in_group)
                            output_group->setElement(dest_off, false);

                        guIteratePieceElementGroup(piece_elementi, piece_element_count, targeti, piece_offset_starts,
                            num_target_points, target_to_piecei, piece_data, owner, target_in_group, target_point_list, target_group);
                    }
                }
            };
            if (copy_target_attribs_in_parallel)
                UTparallelForAppendToTaskList(task_list, output_splittable_ranges[owner], functor);
            else
                functor(output_splittable_ranges[owner]);
        }
        else if (method == AttribCombineMethod::ADD)
        {
            // Union group membership from target to output_geo
            exint num_elements_per_copy = source_element_counts[owner];
            auto &&functor = [output_group,target_group,&target_point_list,num_elements_per_copy,
                start_offset,piece_offset_starts,piece_offset_starts_end,
                num_target_points,target_to_piecei,piece_data,owner](const GA_SplittableRange &r)
            {
                GA_Offset start;
                GA_Offset end;
                for (GA_Iterator it(r); it.fullBlockAdvance(start, end); )
                {
                    exint targeti;
                    exint piece_elementi;
                    exint piece_element_count;
                    guFindStartInTarget(start, targeti, piece_elementi, piece_element_count,
                        nullptr, start_offset, piece_offset_starts, piece_offset_starts_end,
                        num_target_points, target_to_piecei, piece_data, owner,
                        nullptr, num_elements_per_copy);

                    GA_Offset target_off = target_point_list[targeti];
                    bool target_in_group = target_group->contains(target_off);

                    // FIXME: Find longer contiguous spans to copy, for better performance.
                    for (GA_Offset dest_off = start; dest_off < end; ++dest_off)
                    {
                        if (target_in_group)
                            output_group->setElement(dest_off, true);

                        guIteratePieceElementGroup(piece_elementi, piece_element_count, targeti, piece_offset_starts,
                            num_target_points, target_to_piecei, piece_data, owner, target_in_group, target_point_list, target_group);
                    }
                }
            };
            if (copy_target_attribs_in_parallel)
                UTparallelForAppendToTaskList(task_list, output_splittable_ranges[owner], functor);
            else
                functor(output_splittable_ranges[owner]);
        }
        else // if (method == AttribCombineMethod::SUBTRACT)
        {
            // Subtract group membership of target from output_geo
            exint num_elements_per_copy = source_element_counts[owner];
            auto &&functor = [output_group,target_group,&target_point_list,num_elements_per_copy,
                start_offset,piece_offset_starts,piece_offset_starts_end,
                num_target_points,target_to_piecei,piece_data,owner](const GA_SplittableRange &r)
            {
                GA_Offset start;
                GA_Offset end;
                for (GA_Iterator it(r); it.fullBlockAdvance(start, end); )
                {
                    exint targeti;
                    exint piece_elementi;
                    exint piece_element_count;
                    guFindStartInTarget(start, targeti, piece_elementi, piece_element_count,
                        nullptr, start_offset, piece_offset_starts, piece_offset_starts_end,
                        num_target_points, target_to_piecei, piece_data, owner,
                        nullptr, num_elements_per_copy);

                    GA_Offset target_off = target_point_list[targeti];
                    bool target_in_group = target_group->contains(target_off);

                    // FIXME: Find longer contiguous spans to copy, for better performance.
                    for (GA_Offset dest_off = start; dest_off < end; ++dest_off)
                    {
                        if (target_in_group)
                            output_group->setElement(dest_off, false);

                        guIteratePieceElementGroup(piece_elementi, piece_element_count, targeti, piece_offset_starts,
                            num_target_points, target_to_piecei, piece_data, owner, target_in_group, target_point_list, target_group);
                    }
                }
            };
            if (copy_target_attribs_in_parallel)
                UTparallelForAppendToTaskList(task_list, output_splittable_ranges[owner], functor);
            else
                functor(output_splittable_ranges[owner]);
        }
    }

    if (copy_target_attribs_in_parallel)
        task_list.spawnRootAndWait();

    cache->myTargetAttribInfo.swap(target_attrib_info);
    cache->myTargetGroupInfo.swap(target_group_info);
    target_attrib_info.clear();
    target_group_info.clear();
}

void
GUupdatePackedPrimTransforms(
    GU_Detail *output_geo,
    GU_CopyToPointsCache *cache,
    const bool had_transform_matrices,
    const exint num_packed_prims,
    const UT_Vector3 *const constant_pivot)
{
    UT_ASSERT(num_packed_prims == output_geo->getNumPoints());
    UT_ASSERT(num_packed_prims == output_geo->getNumPrimitives());
    if (num_packed_prims <= 0)
        return;

    const UT_Matrix3D *const transform_matrices_3d = cache->myTransformMatrices3D.get();
    const UT_Vector3D *const transform_translates_3d = cache->myTransformTranslates3D.get();
    GA_RWHandleV3 output_pos3f(output_geo->getP());
    GA_RWHandleV3D output_pos3d(output_geo->getP());
    GA_Offset start_ptoff = output_geo->pointOffset(GA_Index(0));
    GA_Offset start_primoff = output_geo->primitiveOffset(GA_Index(0));
    auto &&functor = [output_geo,
        start_ptoff,start_primoff,transform_translates_3d,transform_matrices_3d,
        output_pos3f,output_pos3d,had_transform_matrices,constant_pivot](const GA_Range &r)
    {
        GA_Offset start;
        GA_Offset end;
        for (GA_Iterator it(r); it.fullBlockAdvance(start, end); )
        {
            if (transform_matrices_3d || had_transform_matrices)
            {
                UT_Matrix3D identity;
                if (!transform_matrices_3d)
                    identity.identity();

                exint transformi = start - start_primoff;
                for (GA_Offset primoff = start; primoff < end; ++primoff, ++transformi)
                {
                    GA_Primitive *prim = output_geo->getPrimitive(primoff);
                    GU_PrimPacked *packed_prim = UTverify_cast<GU_PrimPacked *>(prim);
                    if (transform_matrices_3d)
                    {
                        const UT_Matrix3D &transform = transform_matrices_3d[transformi];
                        packed_prim->setLocalTransform(transform);
                    }
                    else // had_transform_matrices
                    {
                        packed_prim->setLocalTransform(identity);
                    }
                }
            }

            if (!constant_pivot)
            {
                // Apply pivots from primitives to P.
                exint transformi = start - start_primoff;
                for (GA_Offset primoff = start; primoff < end; ++primoff, ++transformi)
                {
                    GA_Primitive *prim = output_geo->getPrimitive(primoff);
                    GU_PrimPacked *packed_prim = UTverify_cast<GU_PrimPacked *>(prim);
                    GA_Offset ptoff = start_ptoff + transformi;
                    UT_Vector3 pivot;
                    packed_prim->getPivot(pivot);
                    // Need to transform pivot position before adding it to P
                    UT_Vector3D transformed_pivot(pivot);
                    if (transform_matrices_3d)
                        transformed_pivot = transformed_pivot*transform_matrices_3d[transformi];
                    if (transform_translates_3d)
                        transformed_pivot += transform_translates_3d[transformi];
                    output_pos3d.set(ptoff, transformed_pivot);
                }
                continue;
            }

            // NOTE: This conversion to ptoff relies on that there's one point per primitive,
            //       that both are contiguous, and that both are in the same order.
            exint transformi = start - start_primoff;
            GA_Offset local_start_ptoff = start_ptoff + transformi;
            GA_Offset local_end_ptoff = start_ptoff + (end-start_primoff);
            if (!transform_translates_3d)
            {
                for (GA_Offset ptoff = local_start_ptoff; ptoff < local_end_ptoff; ++ptoff, ++transformi)
                {
                    // Need to transform pivot position before adding it to P
                    UT_Vector3D transformed_pivot(*constant_pivot);
                    if (transform_matrices_3d)
                        transformed_pivot = transformed_pivot*transform_matrices_3d[transformi];

                    output_pos3d.set(ptoff, transformed_pivot);
                }
            }
            else if (constant_pivot->isZero())
            {
                if (output_pos3f->getStorage() == GA_STORE_REAL64)
                {
                    for (GA_Offset ptoff = local_start_ptoff; ptoff < local_end_ptoff; ++ptoff, ++transformi)
                    {
                        output_pos3d.set(ptoff, transform_translates_3d[transformi]);
                    }
                }
                else
                {
                    for (GA_Offset ptoff = local_start_ptoff; ptoff < local_end_ptoff; ++ptoff, ++transformi)
                    {
                        output_pos3f.set(ptoff, UT_Vector3F(transform_translates_3d[transformi]));
                    }
                }
            }
            else
            {
                for (GA_Offset ptoff = local_start_ptoff; ptoff < local_end_ptoff; ++ptoff, ++transformi)
                {
                    // Need to transform pivot position before adding it to P
                    UT_Vector3D transformed_pivot(*constant_pivot);
                    if (transform_matrices_3d)
                        transformed_pivot = transformed_pivot*transform_matrices_3d[transformi];

                    output_pos3d.set(ptoff, transform_translates_3d[transformi] + transformed_pivot);
                }
            }
        }
    };

    constexpr exint PARALLEL_THRESHOLD = 2048;
    if (num_packed_prims >= PARALLEL_THRESHOLD)
        UTparallelForLightItems(GA_SplittableRange(output_geo->getPrimitiveRange()), functor);
    else
        functor(output_geo->getPrimitiveRange());
}

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
    const UT_Vector3 *const constant_pivot)
{
    // *** Transform ***

    exint num_target_points = target_point_list.size();
    GUupdatePackedPrimTransforms(output_geo, cache, had_transform_matrices, num_target_points, constant_pivot);

    // Remove any attributes from output_geo that are not being applied from target.
    // (They've already all been removed if topology_changed.)
    if (!topology_changed)
    {
        GUremoveUnnecessaryAttribs(
            output_geo,
            nullptr,
            target,
            cache,
            &target_attrib_info,
            &target_group_info);
    }

    // Add attributes from target that are not in output_geo.
    exint num_target_attribs[3] = {0,0,0};
    SYS_STATIC_ASSERT_MSG(int(GA_ATTRIB_VERTEX)<3 && int(GA_ATTRIB_POINT)<3 && int(GA_ATTRIB_PRIMITIVE)<3,
        "Array above is assuming the order of GA_AttributeOwner enum");

    GUaddAttributesFromSourceOrTarget(
        output_geo,
        nullptr,
        nullptr,
        false,
        nullptr,
        target,
        &target_attrib_info,
        &target_group_info,
        num_target_attribs);

    // Copy attributes from target points
    GA_SplittableRange output_splittable_ranges[3] =
    {
        GA_SplittableRange(output_geo->getVertexRange()),
        GA_SplittableRange(output_geo->getPointRange()),
        GA_SplittableRange(output_geo->getPrimitiveRange())
    };
    SYS_STATIC_ASSERT_MSG(int(GA_ATTRIB_VERTEX)==0 && int(GA_ATTRIB_POINT)==1 && int(GA_ATTRIB_PRIMITIVE)==2,
        "Array above is assuming the order of GA_AttributeOwner enum");

    GUcopyAttributesFromTarget(
        output_geo,
        output_splittable_ranges,
        num_target_points,
        cache,
        1, 1, 1, // 1 point, 1 vertex, 1 primitive
        num_target_attribs,
        target_point_list,
        target,
        target_attrib_info,
        target_group_info,
        topology_changed);
}

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
    const GU_Detail *target,
    const GA_OffsetListRef *target_point_list,
    GU_CopyToPointsCache::TargetAttribInfoMap *target_attrib_info,
    GU_CopyToPointsCache::TargetAttribInfoMap *target_group_info)
{
    const bool topology_changed =
        !cache->myPrevPack ||
        num_packed_prims != cache->myPrevTargetPtCount ||
        output_geo->getNumPoints() != num_packed_prims ||
        output_geo->getUniqueId() != cache->myPrevOutputDetailID;
    const bool source_changed =
        !cache->myPrevPack ||
        source->getUniqueId() != cache->myPrevSourceUniqueID ||
        source->getMetaCacheCount() != cache->myPrevSourceMetaCacheCount ||
        source_topology_changed;
    const bool lod_changed = (lod != cache->myPrevViewportLOD);
    const bool source_intrinsic_changed =
        source_changed ||
        pivot_type != cache->myPrevPivotEnum ||
        lod_changed;

    // *** Creating Packed Primitives ***

    if (topology_changed)
    {
        output_geo->clearAndDestroy();

        GUcreateEmptyPackedGeometryPrims(output_geo, num_packed_prims);
    }
    GA_Offset start_primoff = (output_geo->getNumPrimitives() > 0) ? output_geo->primitiveOffset(GA_Index(0)) : GA_INVALID_OFFSET;

    // *** Updating Content ***

    bool centroid_pivot = (pivot_type == GU_CopyToPointsCache::PackedPivot::CENTROID);

    UT_Vector3 pivot(0,0,0);
    if ((source_intrinsic_changed || source_changed || topology_changed) && num_packed_prims > 0)
    {
        // Create a packed geometry implementation
        const GU_PackedGeometry *packed_geo = nullptr;
        GU_PackedGeometry *packed_geo_nc = nullptr;
        const bool impl_changed = (source_changed || topology_changed);
        if (impl_changed)
        {
            packed_geo_nc = new GU_PackedGeometry();
            if (!source_primgroup && !source_pointgroup)
            {
                // Easy case: just instance the source input geometry.
                packed_geo_nc->setDetailPtr(source_handle);
            }
            else
            {
                // Hard case: copy the source geometry in the groups.
                GU_DetailHandle detail_handle;
                GU_Detail *packed_detail = new GU_Detail();
                detail_handle.allocateAndSet(packed_detail);
                exint source_point_count = source_pointgroup ? source_pointgroup->entries() : source->getNumPoints();
                exint source_prim_count = source_primgroup ? source_primgroup->entries() : source->getNumPrimitives();

                GUcreateVertexListAndGeometryFromSource(
                    packed_detail,
                    source,
                    source_point_count,
                    cache->mySourceVertexCount,
                    source_prim_count,
                    cache->mySourceOffsetLists[GA_ATTRIB_POINT],
                    cache->mySourceOffsetLists[GA_ATTRIB_VERTEX],
                    cache->mySourceOffsetLists[GA_ATTRIB_PRIMITIVE],
                    source_pointgroup,
                    source_primgroup,
                    1);

                exint num_source_attribs[3];
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
                    "Array above is assuming the order of GA_AttributeOwner enum");

                GUcopyAttributesFromSource(
                    packed_detail,
                    output_splittable_ranges,
                    source,
                    1,
                    cache,
                    cache->mySourceOffsetLists,
                    num_source_attribs,
                    true,
                    false,
                    false,
                    true,
                    true);

                packed_geo_nc->setDetailPtr(detail_handle);
            }

            packed_geo = packed_geo_nc;

            // Add all of the reference counter refs at once
            intrusive_ptr_add_ref(packed_geo, num_packed_prims);
        }
        else
        {
            // source_intrinsic_changed is true, but source_changed is false and topology_changed is false.
            GA_Primitive *prim = output_geo->getPrimitive(start_primoff);
            GU_PrimPacked *packed_prim = UTverify_cast<GU_PrimPacked *>(prim);
            packed_geo = UTverify_cast<const GU_PackedGeometry*>(packed_prim->implementation());
        }

        // Cache the bounds in advance, even if we don't need the box center for the pivot,
        // to avoid thread contention downstream when anything requests it.
        UT_BoundingBox bbox;
        packed_geo->getBoundsCached(bbox);
        if (centroid_pivot)
        {
            if (bbox.isValid())
                pivot = bbox.center();
        }

        // Set the implementations of all of the primitives at once.
        auto &&functor = [output_geo,packed_geo_nc,
            &pivot,lod,impl_changed](const UT_BlockedRange<GA_Offset> &r)
        {
            for (GA_Offset primoff = r.begin(), end = r.end(); primoff < end; ++primoff)
            {
                GA_Primitive *prim = output_geo->getPrimitive(primoff);
                GU_PrimPacked *packed_prim = UTverify_cast<GU_PrimPacked *>(prim);
                if (impl_changed)
                {
                    // It's okay to set the implementation if it hasn't changed,
                    // since setImplementation checks if the pointer is equal.
                    // It doesn't incur the previous impl's decref in that case.
                    packed_prim->setImplementation(packed_geo_nc, false);
                }

                // NOTE: Applying pivot to position is handled below.
                packed_prim->setPivot(pivot);

                packed_prim->setViewportLOD(lod);
            }
        };

        const UT_BlockedRange<GA_Offset> prim_range(start_primoff, start_primoff+num_packed_prims);
        constexpr exint PARALLEL_THRESHOLD = 2048;
        if (num_packed_prims >= PARALLEL_THRESHOLD)
            UTparallelForLightItems(prim_range, functor);
        else
            functor(prim_range);
    }
    else if ((num_packed_prims > 0) && centroid_pivot)
    {
        GA_Primitive *prim = output_geo->getPrimitive(start_primoff);
        GU_PrimPacked *packed_prim = UTverify_cast<GU_PrimPacked *>(prim);
        const GU_PackedImpl *packed_impl = packed_prim->implementation();
        UT_BoundingBox bbox;
        packed_impl->getBoundsCached(bbox);
        if (bbox.isValid())
            pivot = bbox.center();
    }

    const bool pivots_changed =
        pivot_type != cache->myPrevPivotEnum ||
        (centroid_pivot && source_changed);

    if (num_packed_prims > 0)
    {
        if (target != nullptr)
        {
            UT_ASSERT(target_point_list != nullptr && num_packed_prims == target_point_list->size());
            GUhandleTargetAttribsForPackedPrims(
                output_geo,
                cache,
                topology_changed,
                had_transform_matrices,
                target,
                *target_point_list,
                *target_attrib_info,
                *target_group_info,
                &pivot);
        }
        else
        {
            // No target, so only update the primitive transforms.
            GUupdatePackedPrimTransforms(
                output_geo,
                cache,
                had_transform_matrices,
                num_packed_prims,
                &pivot);
        }
    }

    if (topology_changed)
    {
        output_geo->bumpDataIdsForAddOrRemove(true, true, true);
    }
    if (source_changed && num_packed_prims > 0)
    {
        output_geo->getPrimitiveList().bumpDataId();
    }
    if (transforms_changed || pivots_changed)
    {
        output_geo->getP()->bumpDataId();
        // If there are no transform matrices, the primitives weren't transformed.
        // If source_changed, we already bumped the primitive list data ID.
        bool has_transform_matrices = (cache->myTransformMatrices3D.get() != nullptr);
        if ((has_transform_matrices || had_transform_matrices) && !source_changed)
            output_geo->getPrimitiveList().bumpDataId();
    }
    if (lod_changed)
    {
        output_geo->getPrimitiveList().bumpDataId();
    }

    cache->myPrevPack = true;
    cache->myPrevTargetPtCount = num_packed_prims;
    cache->myPrevSourceGroupDataID = source->getUniqueId();
    cache->myPrevSourceMetaCacheCount = source->getMetaCacheCount();
    cache->myPrevPivotEnum = pivot_type;
    cache->myPrevViewportLOD = lod;
    cache->myPrevOutputDetailID = output_geo->getUniqueId();
    cache->myPrevSourceUniqueID = source->getUniqueId();
    cache->myPrevSourceMetaCacheCount = source->getMetaCacheCount();
}

} // namespace GU_Copy

} // End of HDK_Sample namespace
