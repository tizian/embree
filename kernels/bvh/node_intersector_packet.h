// ======================================================================== //
// Copyright 2009-2017 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#pragma once

#include "bvh.h"

namespace embree
{
  namespace isa
  {
    //////////////////////////////////////////////////////////////////////////////////////
    // Fast AlignedNode intersection
    //////////////////////////////////////////////////////////////////////////////////////

    template<int N, int K>
    __forceinline vbool<K> intersectNode(const typename BVHN<N>::AlignedNode* node, size_t i,
                                         const Vec3vf<K>& org, const Vec3vf<K>& dir, const Vec3vf<K>& rdir, const Vec3vf<K>& org_rdir,
                                         const vfloat<K>& tnear, const vfloat<K>& tfar, vfloat<K>& dist)

    {
  #if defined(__AVX2__)
      const vfloat<K> lclipMinX = msub(node->lower_x[i],rdir.x,org_rdir.x);
      const vfloat<K> lclipMinY = msub(node->lower_y[i],rdir.y,org_rdir.y);
      const vfloat<K> lclipMinZ = msub(node->lower_z[i],rdir.z,org_rdir.z);
      const vfloat<K> lclipMaxX = msub(node->upper_x[i],rdir.x,org_rdir.x);
      const vfloat<K> lclipMaxY = msub(node->upper_y[i],rdir.y,org_rdir.y);
      const vfloat<K> lclipMaxZ = msub(node->upper_z[i],rdir.z,org_rdir.z);
  #else
      const vfloat<K> lclipMinX = (node->lower_x[i] - org.x) * rdir.x;
      const vfloat<K> lclipMinY = (node->lower_y[i] - org.y) * rdir.y;
      const vfloat<K> lclipMinZ = (node->lower_z[i] - org.z) * rdir.z;
      const vfloat<K> lclipMaxX = (node->upper_x[i] - org.x) * rdir.x;
      const vfloat<K> lclipMaxY = (node->upper_y[i] - org.y) * rdir.y;
      const vfloat<K> lclipMaxZ = (node->upper_z[i] - org.z) * rdir.z;
  #endif

  #if defined(__AVX512F__) && !defined(__AVX512ER__) // SKX
      if (K == 16)
      {
        /* use mixed float/int min/max */
        const vfloat<K> lnearP = maxi(min(lclipMinX, lclipMaxX), min(lclipMinY, lclipMaxY), min(lclipMinZ, lclipMaxZ));
        const vfloat<K> lfarP  = mini(max(lclipMinX, lclipMaxX), max(lclipMinY, lclipMaxY), max(lclipMinZ, lclipMaxZ));
        const vbool<K> lhit    = asInt(maxi(lnearP, tnear)) <= asInt(mini(lfarP, tfar));
        dist = lnearP;
        return lhit;
      }
      else
  #endif
      {
        const vfloat<K> lnearP = maxi(mini(lclipMinX, lclipMaxX), mini(lclipMinY, lclipMaxY), mini(lclipMinZ, lclipMaxZ));
        const vfloat<K> lfarP  = mini(maxi(lclipMinX, lclipMaxX), maxi(lclipMinY, lclipMaxY), maxi(lclipMinZ, lclipMaxZ));
  #if defined(__AVX512F__) && !defined(__AVX512ER__) // SKX
        const vbool<K> lhit    = asInt(maxi(lnearP, tnear)) <= asInt(mini(lfarP, tfar));
  #else
        const vbool<K> lhit    = maxi(lnearP, tnear) <= mini(lfarP, tfar);
  #endif
        dist = lnearP;
        return lhit;
      }
    }

    //////////////////////////////////////////////////////////////////////////////////////
    // Robust AlignedNode intersection
    //////////////////////////////////////////////////////////////////////////////////////

    template<int N, int K>
    __forceinline vbool<K> intersectNodeRobust(const typename BVHN<N>::AlignedNode* node, size_t i,
                                               const Vec3vf<K>& org, const Vec3vf<K>& dir, const Vec3vf<K>& rdir, const Vec3vf<K>& org_rdir,
                                               const vfloat<K>& tnear, const vfloat<K>& tfar, vfloat<K>& dist)
    {
      // FIXME: use per instruction rounding for AVX512
      const vfloat<K> lclipMinX = (node->lower_x[i] - org.x) * rdir.x;
      const vfloat<K> lclipMinY = (node->lower_y[i] - org.y) * rdir.y;
      const vfloat<K> lclipMinZ = (node->lower_z[i] - org.z) * rdir.z;
      const vfloat<K> lclipMaxX = (node->upper_x[i] - org.x) * rdir.x;
      const vfloat<K> lclipMaxY = (node->upper_y[i] - org.y) * rdir.y;
      const vfloat<K> lclipMaxZ = (node->upper_z[i] - org.z) * rdir.z;
      const float round_down = 1.0f-2.0f*float(ulp);
      const float round_up   = 1.0f+2.0f*float(ulp);
      const vfloat<K> lnearP = max(max(min(lclipMinX, lclipMaxX), min(lclipMinY, lclipMaxY)), min(lclipMinZ, lclipMaxZ));
      const vfloat<K> lfarP  = min(min(max(lclipMinX, lclipMaxX), max(lclipMinY, lclipMaxY)), max(lclipMinZ, lclipMaxZ));
      const vbool<K> lhit   = round_down*max(lnearP,tnear) <= round_up*min(lfarP,tfar);
      dist = lnearP;
      return lhit;
    }

    //////////////////////////////////////////////////////////////////////////////////////
    // Fast AlignedNodeMB intersection
    //////////////////////////////////////////////////////////////////////////////////////

    template<int N, int K>
    __forceinline vbool<K> intersectNode(const typename BVHN<N>::AlignedNodeMB* node, const size_t i,
                                         const Vec3vf<K>& org, const Vec3vf<K>& dir, const Vec3vf<K>& rdir, const Vec3vf<K>& org_rdir,
                                         const vfloat<K>& tnear, const vfloat<K>& tfar, const vfloat<K>& time, vfloat<K>& dist)
    {
      const vfloat<K> vlower_x = madd(time,vfloat<K>(node->lower_dx[i]),vfloat<K>(node->lower_x[i]));
      const vfloat<K> vlower_y = madd(time,vfloat<K>(node->lower_dy[i]),vfloat<K>(node->lower_y[i]));
      const vfloat<K> vlower_z = madd(time,vfloat<K>(node->lower_dz[i]),vfloat<K>(node->lower_z[i]));
      const vfloat<K> vupper_x = madd(time,vfloat<K>(node->upper_dx[i]),vfloat<K>(node->upper_x[i]));
      const vfloat<K> vupper_y = madd(time,vfloat<K>(node->upper_dy[i]),vfloat<K>(node->upper_y[i]));
      const vfloat<K> vupper_z = madd(time,vfloat<K>(node->upper_dz[i]),vfloat<K>(node->upper_z[i]));

#if defined(__AVX2__)
      const vfloat<K> lclipMinX = msub(vlower_x,rdir.x,org_rdir.x);
      const vfloat<K> lclipMinY = msub(vlower_y,rdir.y,org_rdir.y);
      const vfloat<K> lclipMinZ = msub(vlower_z,rdir.z,org_rdir.z);
      const vfloat<K> lclipMaxX = msub(vupper_x,rdir.x,org_rdir.x);
      const vfloat<K> lclipMaxY = msub(vupper_y,rdir.y,org_rdir.y);
      const vfloat<K> lclipMaxZ = msub(vupper_z,rdir.z,org_rdir.z);
#else
      const vfloat<K> lclipMinX = (vlower_x - org.x) * rdir.x;
      const vfloat<K> lclipMinY = (vlower_y - org.y) * rdir.y;
      const vfloat<K> lclipMinZ = (vlower_z - org.z) * rdir.z;
      const vfloat<K> lclipMaxX = (vupper_x - org.x) * rdir.x;
      const vfloat<K> lclipMaxY = (vupper_y - org.y) * rdir.y;
      const vfloat<K> lclipMaxZ = (vupper_z - org.z) * rdir.z;
#endif

#if defined(__AVX512F__) && !defined(__AVX512ER__) // SKX
      if (K == 16)
      {
        /* use mixed float/int min/max */
        const vfloat<K> lnearP = maxi(min(lclipMinX, lclipMaxX), min(lclipMinY, lclipMaxY), min(lclipMinZ, lclipMaxZ));
        const vfloat<K> lfarP  = mini(max(lclipMinX, lclipMaxX), max(lclipMinY, lclipMaxY), max(lclipMinZ, lclipMaxZ));
        const vbool<K> lhit    = asInt(maxi(lnearP, tnear)) <= asInt(mini(lfarP, tfar));
        dist = lnearP;
        return lhit;
      }
      else
#endif
      {
        const vfloat<K> lnearP = maxi(mini(lclipMinX, lclipMaxX), mini(lclipMinY, lclipMaxY), mini(lclipMinZ, lclipMaxZ));
        const vfloat<K> lfarP  = mini(maxi(lclipMinX, lclipMaxX), maxi(lclipMinY, lclipMaxY), maxi(lclipMinZ, lclipMaxZ));
#if defined(__AVX512F__) && !defined(__AVX512ER__) // SKX
        const vbool<K> lhit    = asInt(maxi(lnearP, tnear)) <= asInt(mini(lfarP, tfar));
#else
        const vbool<K> lhit    = maxi(lnearP, tnear) <= mini(lfarP, tfar);
#endif
        dist = lnearP;
        return lhit;
      }
    }

    //////////////////////////////////////////////////////////////////////////////////////
    // Robust AlignedNodeMB intersection
    //////////////////////////////////////////////////////////////////////////////////////

    template<int N, int K>
    __forceinline vbool<K> intersectNodeRobust(const typename BVHN<N>::AlignedNodeMB* node, const size_t i,
                                               const Vec3vf<K>& org, const Vec3vf<K>& dir, const Vec3vf<K>& rdir, const Vec3vf<K>& org_rdir,
                                               const vfloat<K>& tnear, const vfloat<K>& tfar, const vfloat<K>& time, vfloat<K>& dist)
    {
      const vfloat<K> vlower_x = madd(time,vfloat<K>(node->lower_dx[i]),vfloat<K>(node->lower_x[i]));
      const vfloat<K> vlower_y = madd(time,vfloat<K>(node->lower_dy[i]),vfloat<K>(node->lower_y[i]));
      const vfloat<K> vlower_z = madd(time,vfloat<K>(node->lower_dz[i]),vfloat<K>(node->lower_z[i]));
      const vfloat<K> vupper_x = madd(time,vfloat<K>(node->upper_dx[i]),vfloat<K>(node->upper_x[i]));
      const vfloat<K> vupper_y = madd(time,vfloat<K>(node->upper_dy[i]),vfloat<K>(node->upper_y[i]));
      const vfloat<K> vupper_z = madd(time,vfloat<K>(node->upper_dz[i]),vfloat<K>(node->upper_z[i]));

      const vfloat<K> lclipMinX = (vlower_x - org.x) * rdir.x;
      const vfloat<K> lclipMinY = (vlower_y - org.y) * rdir.y;
      const vfloat<K> lclipMinZ = (vlower_z - org.z) * rdir.z;
      const vfloat<K> lclipMaxX = (vupper_x - org.x) * rdir.x;
      const vfloat<K> lclipMaxY = (vupper_y - org.y) * rdir.y;
      const vfloat<K> lclipMaxZ = (vupper_z - org.z) * rdir.z;

      const float round_down = 1.0f-2.0f*float(ulp);
      const float round_up   = 1.0f+2.0f*float(ulp);

#if defined(__AVX512F__) && !defined(__AVX512ER__) // SKX
      if (K == 16)
      {
        const vfloat<K> lnearP = maxi(min(lclipMinX, lclipMaxX), min(lclipMinY, lclipMaxY), min(lclipMinZ, lclipMaxZ));
        const vfloat<K> lfarP  = mini(max(lclipMinX, lclipMaxX), max(lclipMinY, lclipMaxY), max(lclipMinZ, lclipMaxZ));
        const vbool<K>  lhit   = round_down*maxi(lnearP, tnear) <= round_up*mini(lfarP, tfar);
        dist = lnearP;
        return lhit;
      }
      else
#endif
      {
        const vfloat<K> lnearP = maxi(mini(lclipMinX, lclipMaxX), mini(lclipMinY, lclipMaxY), mini(lclipMinZ, lclipMaxZ));
        const vfloat<K> lfarP  = mini(maxi(lclipMinX, lclipMaxX), maxi(lclipMinY, lclipMaxY), maxi(lclipMinZ, lclipMaxZ));
        const vbool<K>  lhit   = round_down*maxi(lnearP, tnear) <= round_up*mini(lfarP, tfar);
        dist = lnearP;
        return lhit;
      }
    }

    //////////////////////////////////////////////////////////////////////////////////////
    // Fast AlignedNodeMB4D intersection
    //////////////////////////////////////////////////////////////////////////////////////

    template<int N, int K>
    __forceinline vbool<K> intersectNodeMB4D(const typename BVHN<N>::NodeRef ref, const size_t i,
                                             const Vec3vf<K>& org, const Vec3vf<K>& dir, const Vec3vf<K>& rdir, const Vec3vf<K>& org_rdir,
                                             const vfloat<K>& tnear, const vfloat<K>& tfar, const vfloat<K>& time, vfloat<K>& dist)
    {
      const typename BVHN<N>::AlignedNodeMB* node = ref.alignedNodeMB();

      const vfloat<K> vlower_x = madd(time,vfloat<K>(node->lower_dx[i]),vfloat<K>(node->lower_x[i]));
      const vfloat<K> vlower_y = madd(time,vfloat<K>(node->lower_dy[i]),vfloat<K>(node->lower_y[i]));
      const vfloat<K> vlower_z = madd(time,vfloat<K>(node->lower_dz[i]),vfloat<K>(node->lower_z[i]));
      const vfloat<K> vupper_x = madd(time,vfloat<K>(node->upper_dx[i]),vfloat<K>(node->upper_x[i]));
      const vfloat<K> vupper_y = madd(time,vfloat<K>(node->upper_dy[i]),vfloat<K>(node->upper_y[i]));
      const vfloat<K> vupper_z = madd(time,vfloat<K>(node->upper_dz[i]),vfloat<K>(node->upper_z[i]));

#if defined(__AVX2__)
      const vfloat<K> lclipMinX = msub(vlower_x,rdir.x,org_rdir.x);
      const vfloat<K> lclipMinY = msub(vlower_y,rdir.y,org_rdir.y);
      const vfloat<K> lclipMinZ = msub(vlower_z,rdir.z,org_rdir.z);
      const vfloat<K> lclipMaxX = msub(vupper_x,rdir.x,org_rdir.x);
      const vfloat<K> lclipMaxY = msub(vupper_y,rdir.y,org_rdir.y);
      const vfloat<K> lclipMaxZ = msub(vupper_z,rdir.z,org_rdir.z);
#else
      const vfloat<K> lclipMinX = (vlower_x - org.x) * rdir.x;
      const vfloat<K> lclipMinY = (vlower_y - org.y) * rdir.y;
      const vfloat<K> lclipMinZ = (vlower_z - org.z) * rdir.z;
      const vfloat<K> lclipMaxX = (vupper_x - org.x) * rdir.x;
      const vfloat<K> lclipMaxY = (vupper_y - org.y) * rdir.y;
      const vfloat<K> lclipMaxZ = (vupper_z - org.z) * rdir.z;
#endif

      const vfloat<K> lnearP = maxi(maxi(mini(lclipMinX, lclipMaxX), mini(lclipMinY, lclipMaxY)), mini(lclipMinZ, lclipMaxZ));
      const vfloat<K> lfarP  = mini(mini(maxi(lclipMinX, lclipMaxX), maxi(lclipMinY, lclipMaxY)), maxi(lclipMinZ, lclipMaxZ));
      vbool<K> lhit = maxi(lnearP,tnear) <= mini(lfarP,tfar);
      if (unlikely(ref.isAlignedNodeMB4D())) {
        const typename BVHN<N>::AlignedNodeMB4D* node1 = (const typename BVHN<N>::AlignedNodeMB4D*) node;
        lhit = lhit & (vfloat<K>(node1->lower_t[i]) <= time) & (time < vfloat<K>(node1->upper_t[i]));
      }
      dist = lnearP;
      return lhit;
    }

    //////////////////////////////////////////////////////////////////////////////////////
    // Robust AlignedNodeMB4D intersection
    //////////////////////////////////////////////////////////////////////////////////////

    template<int N, int K>
    __forceinline vbool<K> intersectNodeMB4DRobust(const typename BVHN<N>::NodeRef ref, const size_t i,
                                                   const Vec3vf<K>& org, const Vec3vf<K>& dir, const Vec3vf<K>& rdir, const Vec3vf<K>& org_rdir,
                                                   const vfloat<K>& tnear, const vfloat<K>& tfar, const vfloat<K>& time, vfloat<K>& dist)
    {
      const typename BVHN<N>::AlignedNodeMB* node = ref.alignedNodeMB();

      const vfloat<K> vlower_x = madd(time,vfloat<K>(node->lower_dx[i]),vfloat<K>(node->lower_x[i]));
      const vfloat<K> vlower_y = madd(time,vfloat<K>(node->lower_dy[i]),vfloat<K>(node->lower_y[i]));
      const vfloat<K> vlower_z = madd(time,vfloat<K>(node->lower_dz[i]),vfloat<K>(node->lower_z[i]));
      const vfloat<K> vupper_x = madd(time,vfloat<K>(node->upper_dx[i]),vfloat<K>(node->upper_x[i]));
      const vfloat<K> vupper_y = madd(time,vfloat<K>(node->upper_dy[i]),vfloat<K>(node->upper_y[i]));
      const vfloat<K> vupper_z = madd(time,vfloat<K>(node->upper_dz[i]),vfloat<K>(node->upper_z[i]));

      const vfloat<K> lclipMinX = (vlower_x - org.x) * rdir.x;
      const vfloat<K> lclipMinY = (vlower_y - org.y) * rdir.y;
      const vfloat<K> lclipMinZ = (vlower_z - org.z) * rdir.z;
      const vfloat<K> lclipMaxX = (vupper_x - org.x) * rdir.x;
      const vfloat<K> lclipMaxY = (vupper_y - org.y) * rdir.y;
      const vfloat<K> lclipMaxZ = (vupper_z - org.z) * rdir.z;

      const vfloat<K> lnearP = maxi(maxi(mini(lclipMinX, lclipMaxX), mini(lclipMinY, lclipMaxY)), mini(lclipMinZ, lclipMaxZ));
      const vfloat<K> lfarP  = mini(mini(maxi(lclipMinX, lclipMaxX), maxi(lclipMinY, lclipMaxY)), maxi(lclipMinZ, lclipMaxZ));
      const float round_down = 1.0f-2.0f*float(ulp);
      const float round_up   = 1.0f+2.0f*float(ulp);
      vbool<K> lhit = round_down*maxi(lnearP,tnear) <= round_up*mini(lfarP,tfar);

      if (unlikely(ref.isAlignedNodeMB4D())) {
        const typename BVHN<N>::AlignedNodeMB4D* node1 = (const typename BVHN<N>::AlignedNodeMB4D*) node;
        lhit = lhit & (vfloat<K>(node1->lower_t[i]) <= time) & (time < vfloat<K>(node1->upper_t[i]));
      }
      dist = lnearP;
      return lhit;
    }

    //////////////////////////////////////////////////////////////////////////////////////
    // Fast UnalignedNode intersection
    //////////////////////////////////////////////////////////////////////////////////////

    template<int N, int K>
    __forceinline vbool<K> intersectNode(const typename BVHN<N>::UnalignedNode* node, const size_t i,
                                         const Vec3vf<K>& org_i, const Vec3vf<K>& dir_i, const Vec3vf<K>& rdir_i, const Vec3vf<K>& org_rdir_i,
                                         const vfloat<K>& tnear, const vfloat<K>& tfar, vfloat<K>& dist)
    {
      const AffineSpace3vf<K> naabb(Vec3f(node->naabb.l.vx.x[i],node->naabb.l.vx.y[i],node->naabb.l.vx.z[i]),
                                    Vec3f(node->naabb.l.vy.x[i],node->naabb.l.vy.y[i],node->naabb.l.vy.z[i]),
                                    Vec3f(node->naabb.l.vz.x[i],node->naabb.l.vz.y[i],node->naabb.l.vz.z[i]),
                                    Vec3f(node->naabb.p   .x[i],node->naabb.p   .y[i],node->naabb.p   .z[i]));

      const Vec3vf<K> dir = xfmVector(naabb,dir_i);
      const Vec3vf<K> nrdir = Vec3vf<K>(vfloat<K>(-1.0f))* rcp_safe(dir); // FIXME: negate instead of mul with -1?
      const Vec3vf<K> org = xfmPoint(naabb,org_i);

      const vfloat<K> lclipMinX = org.x * nrdir.x; // (Vec3fa(zero) - org) * rdir;
      const vfloat<K> lclipMinY = org.y * nrdir.y;
      const vfloat<K> lclipMinZ = org.z * nrdir.z;
      const vfloat<K> lclipMaxX  = lclipMinX - nrdir.x; // (Vec3fa(one ) - org) * rdir;
      const vfloat<K> lclipMaxY  = lclipMinY - nrdir.y;
      const vfloat<K> lclipMaxZ  = lclipMinZ - nrdir.z;

      const vfloat<K> lnearP = maxi(mini(lclipMinX, lclipMaxX), mini(lclipMinY, lclipMaxY), mini(lclipMinZ, lclipMaxZ));
      const vfloat<K> lfarP  = mini(maxi(lclipMinX, lclipMaxX), maxi(lclipMinY, lclipMaxY), maxi(lclipMinZ, lclipMaxZ));
      const vbool<K> lhit    = maxi(lnearP, tnear) <= mini(lfarP, tfar);
      dist = lnearP;
      return lhit;
    }

    //////////////////////////////////////////////////////////////////////////////////////
    // Fast UnalignedNodeMB intersection
    //////////////////////////////////////////////////////////////////////////////////////

    template<int N, int K>
    __forceinline vbool<K> intersectNode(const typename BVHN<N>::UnalignedNodeMB* node, const size_t i,
                                         const Vec3vf<K>& org_i, const Vec3vf<K>& dir_i, const Vec3vf<K>& rdir_i, const Vec3vf<K>& org_rdir_i,
                                         const vfloat<K>& tnear, const vfloat<K>& tfar, const vfloat<K>& time, vfloat<K>& dist)
    {
      const AffineSpace3vf<K> xfm(Vec3f(node->space0.l.vx.x[i],node->space0.l.vx.y[i],node->space0.l.vx.z[i]),
                                  Vec3f(node->space0.l.vy.x[i],node->space0.l.vy.y[i],node->space0.l.vy.z[i]),
                                  Vec3f(node->space0.l.vz.x[i],node->space0.l.vz.y[i],node->space0.l.vz.z[i]),
                                  Vec3f(node->space0.p   .x[i],node->space0.p   .y[i],node->space0.p   .z[i]));

      const Vec3vf<K> b0_lower = zero;
      const Vec3vf<K> b0_upper = one;
      const Vec3vf<K> b1_lower(node->b1.lower.x[i],node->b1.lower.y[i],node->b1.lower.z[i]);
      const Vec3vf<K> b1_upper(node->b1.upper.x[i],node->b1.upper.y[i],node->b1.upper.z[i]);
      const Vec3vf<K> lower = lerp(b0_lower,b1_lower,time);
      const Vec3vf<K> upper = lerp(b0_upper,b1_upper,time);

      const Vec3vf<K> dir = xfmVector(xfm,dir_i);
      const Vec3vf<K> rdir = rcp_safe(dir);
      const Vec3vf<K> org = xfmPoint(xfm,org_i);

      const vfloat<K> lclipMinX = (lower.x - org.x) * rdir.x;
      const vfloat<K> lclipMinY = (lower.y - org.y) * rdir.y;
      const vfloat<K> lclipMinZ = (lower.z - org.z) * rdir.z;
      const vfloat<K> lclipMaxX  = (upper.x - org.x) * rdir.x;
      const vfloat<K> lclipMaxY  = (upper.y - org.y) * rdir.y;
      const vfloat<K> lclipMaxZ  = (upper.z - org.z) * rdir.z;

      const vfloat<K> lnearP = maxi(mini(lclipMinX, lclipMaxX), mini(lclipMinY, lclipMaxY), mini(lclipMinZ, lclipMaxZ));
      const vfloat<K> lfarP  = mini(maxi(lclipMinX, lclipMaxX), maxi(lclipMinY, lclipMaxY), maxi(lclipMinZ, lclipMaxZ));
      const vbool<K> lhit    = maxi(lnearP, tnear) <= mini(lfarP, tfar);
      dist = lnearP;
      return lhit;
    }

    //////////////////////////////////////////////////////////////////////////////////////
    // Node intersectors used in ray traversal
    //////////////////////////////////////////////////////////////////////////////////////

    /*! Intersects N nodes with K rays */
    template<int N, int K, int types, bool robust>
    struct BVHNNodeIntersectorK;

    template<int N, int K>
    struct BVHNNodeIntersectorK<N,K,BVH_AN1,false>
    {
      /* vmask is both an input and an output parameter! Its initial value should be the parent node
         hit mask, which is used for correctly computing the current hit mask. The parent hit mask
         is actually required only for motion blur node intersections (because different rays may
         have different times), so for regular nodes vmask is simply overwritten. */
      static __forceinline bool intersect(const typename BVHN<N>::NodeRef& node, const size_t i, const Vec3vf<K>& org, const Vec3vf<K>& dir, const Vec3vf<K>& rdir, const Vec3vf<K>& org_rdir,
                                          const vfloat<K>& tnear, const vfloat<K>& tfar, const vfloat<K>& time, vfloat<K>& dist, vbool<K>& vmask)
      {
        vmask = intersectNode<N,K>(node.alignedNode(),i,org,dir,rdir,org_rdir,tnear,tfar,dist);
        return true;
      }
    };

    template<int N, int K>
    struct BVHNNodeIntersectorK<N,K,BVH_AN1,true>
    {
      static __forceinline bool intersect(const typename BVHN<N>::NodeRef& node, const size_t i, const Vec3vf<K>& org, const Vec3vf<K>& dir, const Vec3vf<K>& rdir, const Vec3vf<K>& org_rdir,
                                          const vfloat<K>& tnear, const vfloat<K>& tfar, const vfloat<K>& time, vfloat<K>& dist, vbool<K>& vmask)
      {
        vmask = intersectNodeRobust<N,K>(node.alignedNode(),i,org,dir,rdir,org_rdir,tnear,tfar,dist);
        return true;
      }
    };

    template<int N, int K>
    struct BVHNNodeIntersectorK<N,K,BVH_AN2,false>
    {
      static __forceinline bool intersect(const typename BVHN<N>::NodeRef& node, const size_t i, const Vec3vf<K>& org, const Vec3vf<K>& dir, const Vec3vf<K>& rdir, const Vec3vf<K>& org_rdir,
                                          const vfloat<K>& tnear, const vfloat<K>& tfar, const vfloat<K>& time, vfloat<K>& dist, vbool<K>& vmask)
      {
        vmask = intersectNode<N,K>(node.alignedNodeMB(),i,org,dir,rdir,org_rdir,tnear,tfar,time,dist);
        return true;
      }
    };

    template<int N, int K>
    struct BVHNNodeIntersectorK<N,K,BVH_AN2,true>
    {
      static __forceinline bool intersect(const typename BVHN<N>::NodeRef& node, const size_t i, const Vec3vf<K>& org, const Vec3vf<K>& dir, const Vec3vf<K>& rdir, const Vec3vf<K>& org_rdir,
                                          const vfloat<K>& tnear, const vfloat<K>& tfar, const vfloat<K>& time, vfloat<K>& dist, vbool<K>& vmask)
      {
        vmask = intersectNodeRobust<N,K>(node.alignedNodeMB(),i,org,dir,rdir,org_rdir,tnear,tfar,time,dist);
        return true;
      }
    };

    template<int N, int K>
    struct BVHNNodeIntersectorK<N,K,BVH_AN1_UN1,false>
    {
      static __forceinline bool intersect(const typename BVHN<N>::NodeRef& node, const size_t i,
                                          const Vec3vf<K>& org, const Vec3vf<K>& dir, const Vec3vf<K>& rdir, const Vec3vf<K>& org_rdir,
                                          const vfloat<K>& tnear, const vfloat<K>& tfar, const vfloat<K>& time, vfloat<K>& dist, vbool<K>& vmask)
      {
        if (likely(node.isAlignedNode()))              vmask = intersectNode<N,K>(node.alignedNode(),i,org,dir,rdir,org_rdir,tnear,tfar,dist);
        else /*if (unlikely(node.isUnalignedNode()))*/ vmask = intersectNode<N,K>(node.unalignedNode(),i,org,dir,rdir,org_rdir,tnear,tfar,dist);
        return true;
      }
    };

    template<int N, int K>
    struct BVHNNodeIntersectorK<N,K,BVH_AN2_UN2,false>
    {
      static __forceinline bool intersect(const typename BVHN<N>::NodeRef& node, const size_t i,
                                          const Vec3vf<K>& org, const Vec3vf<K>& dir, const Vec3vf<K>& rdir, const Vec3vf<K>& org_rdir,
                                          const vfloat<K>& tnear, const vfloat<K>& tfar, const vfloat<K>& time, vfloat<K>& dist, vbool<K>& vmask)
      {
        if (likely(node.isAlignedNodeMB()))              vmask = intersectNode<N,K>(node.alignedNodeMB(),i,org,dir,rdir,org_rdir,tnear,tfar,time,dist);
        else /*if (unlikely(node.isUnalignedNodeMB()))*/ vmask = intersectNode<N,K>(node.unalignedNodeMB(),i,org,dir,rdir,org_rdir,tnear,tfar,time,dist);
        return true;
      }
    };

    template<int N, int K>
    struct BVHNNodeIntersectorK<N,K,BVH_AN2_AN4D,false>
    {
      static __forceinline bool intersect(const typename BVHN<N>::NodeRef& node, const size_t i,
                                          const Vec3vf<K>& org, const Vec3vf<K>& dir, const Vec3vf<K>& rdir, const Vec3vf<K>& org_rdir,
                                          const vfloat<K>& tnear, const vfloat<K>& tfar, const vfloat<K>& time, vfloat<K>& dist, vbool<K>& vmask)
      {
        vmask &= intersectNodeMB4D<N,K>(node,i,org,dir,rdir,org_rdir,tnear,tfar,time,dist);
        return true;
      }
    };

    template<int N, int K>
    struct BVHNNodeIntersectorK<N,K,BVH_AN2_AN4D,true>
    {
      static __forceinline bool intersect(const typename BVHN<N>::NodeRef& node, const size_t i,
                                          const Vec3vf<K>& org, const Vec3vf<K>& dir, const Vec3vf<K>& rdir, const Vec3vf<K>& org_rdir,
                                          const vfloat<K>& tnear, const vfloat<K>& tfar, const vfloat<K>& time, vfloat<K>& dist, vbool<K>& vmask)
      {
        vmask &= intersectNodeMB4DRobust<N,K>(node,i,org,dir,rdir,org_rdir,tnear,tfar,time,dist);
        return true;
      }
    };

    template<int N, int K>
    struct BVHNNodeIntersectorK<N,K,BVH_AN2_AN4D_UN2,false>
    {
      static __forceinline bool intersect(const typename BVHN<N>::NodeRef& node, const size_t i,
                                          const Vec3vf<K>& org, const Vec3vf<K>& dir, const Vec3vf<K>& rdir, const Vec3vf<K>& org_rdir,
                                          const vfloat<K>& tnear, const vfloat<K>& tfar, const vfloat<K>& time, vfloat<K>& dist, vbool<K>& vmask)
      {
        if (likely(node.isAlignedNodeMB() || node.isAlignedNodeMB4D())) {
          vmask &= intersectNodeMB4D<N,K>(node,i,org,dir,rdir,org_rdir,tnear,tfar,time,dist);
        } else /*if (unlikely(node.isUnalignedNodeMB()))*/ {
          assert(node.isUnalignedNodeMB());
          vmask &= intersectNode<N,K>(node.unalignedNodeMB(),i,org,dir,rdir,org_rdir,tnear,tfar,time,dist);
        }
        return true;
      }
    };
  }
}
