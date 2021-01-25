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
 * Definitions of functions for building primitives in bulk.
 */

#include "GEO_BuildPrimitives.h"

#include <GEO/GEO_Detail.h>
#include <GEO/GEO_ParallelWiringUtil.h>
#include <GA/GA_PolyCounts.h>
#include <GA/GA_SplittableRange.h>
#include <GA/GA_Types.h>
#include <UT/UT_Lock.h>
#include <UT/UT_ParallelUtil.h>
#include <SYS/SYS_Types.h>
#include <utility>

//#define TIMING_BUILDBLOCK

#ifdef TIMING_BUILDBLOCK
#include <UT/UT_StopWatch.h>
#define TIMING_DEF \
    UT_StopWatch timer; \
    timer.start();
#define TIMING_LOG(msg) \
    printf(msg ": %f milliseconds\n", 1000*timer.stop()); \
    fflush(stdout); \
    timer.start();
#else
#define TIMING_DEF
#define TIMING_LOG(msg)
#endif

namespace HDK_Sample {

template<typename INT_T>
GA_Offset GEObuildPrimitives(
    GEO_Detail *detail,
    const std::pair<int,exint> *primtype_count_pairs,
    const GA_Offset init_startpt,
    const GA_Size npoints_per_copy,
    const GA_PolyCounts &vertexlistsizelist,
    const INT_T *vertexpointnumbers,
    const bool hassharedpoints,
    const exint *closed_span_lengths,
    const exint ncopies)
{
    if (ncopies <= 0)
        return GA_INVALID_OFFSET;

    GA_Size npolygons = vertexlistsizelist.getNumPolygons();
    if (npolygons == 0)
        return GA_INVALID_OFFSET;

    TIMING_DEF;

    const GA_Size nvertices = vertexlistsizelist.getNumVertices();

    UT_ASSERT(vertexpointnumbers != nullptr || npoints_per_copy == nvertices || nvertices == 0);

    // Create the empty primitives and vertices not yet wired to points.
    GA_Offset init_startvtx;
    const GA_Offset init_startprim = detail->appendPrimitivesAndVertices(primtype_count_pairs, vertexlistsizelist, init_startvtx, closed_span_lengths, ncopies);

    TIMING_LOG("Creating primitives");

    // Nothing more to do if no vertices
    if (nvertices == 0)
        return init_startprim;

    GA_ATITopology *const vertexToPoint = detail->getTopology().getPointRef();
    GA_ATITopology *const pointToVertex = detail->getTopology().getVertexRef();
    GA_ATITopology *const vertexToNext = detail->getTopology().getVertexNextRef();
    GA_ATITopology *const vertexToPrev = detail->getTopology().getVertexPrevRef();

    // Everything from this point on is independent, except for hardening pages
    // that have copies from different tasks in them.
    auto &&functor = [init_startvtx,nvertices,init_startpt,npoints_per_copy,
        vertexToPoint,pointToVertex,vertexToNext,vertexToPrev,
        detail,vertexpointnumbers,hassharedpoints](const UT_BlockedRange<exint> &r)
    {
        GA_Offset startvtx = init_startvtx + r.begin()*nvertices;
        GA_Offset startpt = init_startpt + r.begin()*npoints_per_copy;
        for (exint copyi = r.begin(), endcopyi = r.end(); copyi < endcopyi; ++copyi, startvtx += nvertices, startpt += npoints_per_copy)
        {
            const GA_Offset endvtx = startvtx + nvertices;
            const GA_Offset endpt = startpt + npoints_per_copy;

            TIMING_DEF;

            // Set the vertex-to-point mapping
            if (vertexToPoint)
            {
                UTparallelForLightItems(GA_SplittableRange(GA_Range(detail->getVertexMap(), startvtx, endvtx)),
                    geo_SetTopoMappedParallel<INT_T>(vertexToPoint, startpt, startvtx, vertexpointnumbers));

                TIMING_LOG("Setting vtx->pt");
            }

            // Check whether the linked list topologies need to be set

            if (vertexpointnumbers == nullptr)
            {
                if (pointToVertex)
                {
                    // Only need to set pointToVertex, because next and prev will be
                    // default of GA_INVALID_OFFSET

                    UTparallelForLightItems(GA_SplittableRange(GA_Range(detail->getPointMap(), startpt, endpt)),
                        geo_SetTopoMappedParallel<INT_T>(pointToVertex, startvtx, startpt, nullptr));

                    TIMING_LOG("Setting pt->vtx fast");
                }

                // That's all, for this simple case!
                continue;
            }
            if (!hassharedpoints)
            {
                // Set the point-to-vertex mapping
                if (pointToVertex)
                {
                    pointToVertex->hardenAllPages(startpt, endpt);
                    UTparallelForLightItems(GA_SplittableRange(GA_Range(detail->getVertexMap(), startvtx, endvtx)),
                        geo_SetTopoRevMappedParallel<INT_T>(pointToVertex, startpt, startvtx, vertexpointnumbers));

                    TIMING_LOG("Setting pt->vtx no shared points");
                }

                // That's all, for this simple case!
                continue;
            }

            if (pointToVertex && vertexToNext && vertexToPrev)
            {
                // Create a trivial map from 0 to nvertices
                UT_Array<INT_T> map;
                map.setSizeNoInit(nvertices);

                TIMING_LOG("Allocating map");

                UTparallelForLightItems(UT_BlockedRange<GA_Size>(0, nvertices), geo_TrivialArrayParallel<INT_T>(map));

                TIMING_LOG("Creating trivial map");

                // Sort the map in parallel according to the point offsets of the vertices
                UTparallelSort(map.array(), map.array() + nvertices, geo_VerticesByPointCompare<INT_T,true>(vertexpointnumbers));

                TIMING_LOG("Sorting array map");

                // Create arrays for the next vertex and prev vertex in parallel
                // If we wanted to do this in geo_LinkToposParallel, we would first
                // have to create an inverse map anyway to find out where vertices
                // are in map, so this saves re-traversing things.
                UT_Array<INT_T> nextvtxarray;
                UT_Array<INT_T> prevvtxarray;
                nextvtxarray.setSizeNoInit(nvertices);
                prevvtxarray.setSizeNoInit(nvertices);
                UT_Lock lock;
                UTparallelForLightItems(UT_BlockedRange<GA_Size>(0, nvertices),
                    geo_NextPrevParallel<INT_T>(
                        map.getArray(), map.size(),
                        vertexpointnumbers,
                        nextvtxarray.getArray(), prevvtxarray.getArray(),
                        startvtx, startpt, pointToVertex, vertexToPrev, lock));

                TIMING_LOG("Finding next/prev");

                // Set the point-to-vertex topology in parallel
                // This needs to be done after constructing the next/prev array,
                // because it checks for existing vertices using the points.
                UTparallelForLightItems(GA_SplittableRange(GA_Range(detail->getPointMap(), startpt, endpt)),
                    geo_Pt2VtxTopoParallel<INT_T>(pointToVertex, map.getArray(), map.size(), vertexpointnumbers, startvtx, startpt));

                TIMING_LOG("Setting pt->vtx");

                // Clear up some memory before filling up the linked list topologies
                map.setCapacity(0);

                TIMING_LOG("Clearing map");

                // Fill in the linked list topologies
                UTparallelForLightItems(GA_SplittableRange(GA_Range(detail->getVertexMap(), startvtx, endvtx)),
                    geo_LinkToposParallel<INT_T>(vertexToNext, vertexToPrev, nextvtxarray.getArray(), prevvtxarray.getArray(), startvtx));

                TIMING_LOG("Setting links");
            }
        }
    };

    constexpr exint PARALLEL_THRESHOLD = 4096;
    if (ncopies >= 2 && ncopies*nvertices >= PARALLEL_THRESHOLD)
    {
        // Harden pages in order to parallelize safely.
        // TODO: Only harden pages that stradle task boundaries.
        GA_Offset endvtx = init_startvtx + nvertices*ncopies;
        GA_Offset endpt = init_startpt + npoints_per_copy*ncopies;
        if (vertexToPoint)
            vertexToPoint->hardenAllPages(init_startvtx, endvtx);
        if (pointToVertex)
            pointToVertex->hardenAllPages(init_startpt, endpt);
        if (vertexpointnumbers != nullptr && hassharedpoints && vertexToNext && vertexToPrev)
        {
            vertexToNext->hardenAllPages(init_startvtx, endvtx);
            vertexToPrev->hardenAllPages(init_startvtx, endvtx);
        }

        UTparallelFor(UT_BlockedRange<exint>(0,ncopies), functor);
    }
    else
    {
        functor(UT_BlockedRange<exint>(0,ncopies));
    }

    return init_startprim;
}

// Template instantiations
template GA_Offset GEObuildPrimitives<int>(
    GEO_Detail *detail,
    const std::pair<int,exint> *primtype_count_pairs,
    const GA_Offset init_startpt,
    const GA_Size npoints_per_copy,
    const GA_PolyCounts &polygonsizelist,
    const int *polygonpointnumbers,
    const bool hassharedpoints,
    const exint *closed_span_lengths,
    const exint ncopies);
template GA_Offset GEObuildPrimitives<exint>(
    GEO_Detail *detail,
    const std::pair<int,exint> *primtype_count_pairs,
    const GA_Offset init_startpt,
    const GA_Size npoints_per_copy,
    const GA_PolyCounts &polygonsizelist,
    const exint *polygonpointnumbers,
    const bool hassharedpoints,
    const exint *closed_span_lengths,
    const exint ncopies);

} // End of HDK_Sample namespace
