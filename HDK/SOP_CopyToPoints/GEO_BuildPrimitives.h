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
 * Declarations of functions for building primitives in bulk.
 */

#pragma once

#ifndef __HDK_GEO_BuildPrimitives_h__
#define __HDK_GEO_BuildPrimitives_h__

#include <GA/GA_PolyCounts.h>
#include <GA/GA_Types.h>
#include <SYS/SYS_Types.h>
#include <utility>

class GEO_Detail;

namespace HDK_Sample {

/// Function for building primitives in bulk, returning the first primitive offset.
/// This sets up the primitive list, vertices, and topology attributes,
/// parallelizing as much as possible.  Any primitive content other than the
/// contents of the GA_Primitive base class will be left at default values,
/// so other data may still need to be initialized in other ways.
///
/// Instantiated for INT_T of int (int32) and exint (int64)
///
/// Despiate vertexlistsizelist being a GA_PolyCounts, (a run-length encoded
/// array of vertex list sizes), this function works for any primitive types
/// specified in the first component of each pair in primtype_count_pairs.
///
/// If ncopies > 1, startpt will have npoints_per_copy added to it between each copy.
///
/// Values in vertexpointnumbers are offsets, but relative to startpt,
/// so if constructing from point offsets, be sure to subtract off startpt
/// or specify startpt of GA_Offset(0).
///
/// vertexpointnumbers can optionally be nullptr, in which case, vertices will
/// be wired to consecutive point offsets.  (This implies that there are no
/// shared points.)  This is a fairly common case for separate curve primitives,
/// packed primitives, or polygon soup primitives.
///
/// Even if vertexpointnumbers is non-null, hassharedpoints being false can
/// save time (if it applies), because not having shared points
/// avoids the need to ensure a deterministic order of the linked list
/// topology attributes: vertex-to-next-vertex and vertex-to-previous-vertex.
///
/// First value of closed_span_lengths is the number with closed false.
/// To start with closed true, have a value of 0 first, then the number with closed true.
/// nullptr means closed false for all primitives.
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
    const exint ncopies = 1);

} // End of HDK_Sample namespace

#endif
