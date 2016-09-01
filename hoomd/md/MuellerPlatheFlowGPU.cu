// Copyright (c) 2009-2016 The Regents of the University of Michigan
// This file is part of the HOOMD-blue project, released under the BSD 3-Clause License.

#include "hoomd/HOOMDMath.h"
#include "MuellerPlatheFlow.h"
#include "MuellerPlatheFlowGPU.h"
#include "MuellerPlatheFlowGPU.cuh"
#include <assert.h>
#include <thrust/transform_reduce.h>
#include <thrust/functional.h>
#include <thrust/device_ptr.h>

template<bool flowX,bool flowY,bool flowZ>
struct vel_search_un_opt : public thrust::unary_function< const unsigned int,Scalar3>
    {
        vel_search_un_opt(const Scalar4*const d_vel,const unsigned int *const d_tag)
            :
            m_vel(d_vel),
            m_tag(d_tag)
            {}
        const Scalar4*const m_vel;
        const unsigned int*const m_tag;
        __host__ __device__ Scalar3 operator()(const unsigned int idx)const
            {
            const unsigned int tag = m_tag[idx];
            Scalar vel;
            if(flowX)
                vel = m_vel[idx].x;
            if(flowY)
                vel = m_vel[idx].y;
            if(flowZ)
                vel = m_vel[idx].z;

            const Scalar mass = m_vel[idx].w;
            vel *= mass;
            Scalar3 result;
            result.x = vel;
            result.y = mass;
            result.z = __int_as_scalar(tag);
            return result;
            }
    };

template <typename CMP,bool slabX,bool slabY,bool slabZ>
struct vel_search_binary_opt : public thrust::binary_function< Scalar3, Scalar3, Scalar3 >
    {
        vel_search_binary_opt(const unsigned int*const d_rtag,
                              const Scalar4*const d_pos,
                              const BoxDim gl_box,
                              const unsigned int Nslabs,
                              const unsigned int slab_index,
                              const Scalar3 invalid)
            : m_rtag(d_rtag),
              m_pos(d_pos),
              m_gl_box(gl_box),
              m_Nslabs(Nslabs),
              m_slab_index(slab_index),
              m_invalid(invalid)
            {}
        const unsigned int*const m_rtag;
        const Scalar4*const m_pos;
        const BoxDim m_gl_box;
        const unsigned int m_Nslabs;
        const unsigned int m_slab_index;
        const Scalar3 m_invalid;

        __host__ __device__ Scalar3 operator()(const Scalar3& a,const Scalar3& b)const
            {
            Scalar3 result = m_invalid;
            //Early exit, if invalid args involved.
            if( a.z == m_invalid.z )
                return b;
            if( b.z == m_invalid.z )
                return a;

            const unsigned int idx_a = m_rtag[__scalar_as_int(a.z)];
            const unsigned int idx_b = m_rtag[__scalar_as_int(b.z)];

            unsigned int index_a,index_b;
            if(slabX)
                {
                index_a = (m_pos[idx_a].x/m_gl_box.getL().x +.5) * m_Nslabs;
                index_b = (m_pos[idx_b].x/m_gl_box.getL().x +.5) * m_Nslabs;
                }
            if(slabY)
                {
                index_a = (m_pos[idx_a].y/m_gl_box.getL().y +.5) * m_Nslabs;
                index_b = (m_pos[idx_b].y/m_gl_box.getL().y +.5) * m_Nslabs;
                }
            if(slabZ)
                {
                index_a = (m_pos[idx_a].z/m_gl_box.getL().z +.5) * m_Nslabs;
                index_b = (m_pos[idx_b].z/m_gl_box.getL().z +.5) * m_Nslabs;
                }
            index_a %= m_Nslabs;
            index_b %= m_Nslabs;

            if( index_a == index_b)
                {
                if( index_a == m_slab_index )
                    {
                    CMP cmp;
                    if( cmp(a.x,b.x) )
                        result = a;
                    else
                        result = b;
                    }
                }
            else
                {
                if( index_a == m_slab_index )
                    result = a;
                if( index_b == m_slab_index )
                    result = b;
                }
            return result;
            }
    };

template<bool flowX,bool flowY,bool flowZ,bool slabX, bool slabY, bool slabZ>
cudaError_t gpu_search_min_max_velocity(const unsigned int group_size,
                                        const Scalar4*const d_vel,
                                        const Scalar4*const d_pos,
                                        const unsigned int *const d_tag,
                                        const unsigned int *const d_rtag,
                                        const unsigned int *const d_group_members,
                                        const BoxDim gl_box,
                                        const unsigned int Nslabs,
                                        const unsigned int max_slab,
                                        const unsigned int min_slab,
                                        Scalar3*const last_max_vel,
                                        Scalar3*const last_min_vel,
                                        const bool has_max_slab,
                                        const bool has_min_slab,
                                        const unsigned int blocksize)
    {
    thrust::device_ptr<const unsigned int> member_ptr(d_group_members);

    //Validity of template arguments.
    assert( flowX | flowY | flowZ);
    if( flowX )
        { assert(flowY == false and flowZ == false); }
    if( flowY )
        { assert(flowX == false and flowZ == false); }
    if( flowZ )
        { assert(flowY == false and flowX == false); }
    //Validity of template arguments.
    assert( slabX | slabY | slabZ);
    if( slabX )
        { assert(slabY == false and slabZ == false); }
    if( slabY )
        { assert(slabX == false and slabZ == false); }
    if( slabZ )
        { assert(slabY == false and slabX == false); }


    vel_search_un_opt<flowX,flowY,flowZ> un_opt(d_vel, d_tag);

    if( has_max_slab )
        {
        vel_search_binary_opt<thrust::greater<const Scalar>,slabX,slabY,slabZ > max_bin_opt(
            d_rtag,d_pos,gl_box,Nslabs,max_slab,*last_max_vel);
        Scalar3 init = *last_max_vel;
        *last_max_vel = thrust::transform_reduce(member_ptr,member_ptr+group_size,
                                                 un_opt,init,max_bin_opt);
        }

    if( has_min_slab )
        {
        vel_search_binary_opt<thrust::less<const Scalar>,slabX,slabY,slabZ > min_bin_opt(
            d_rtag,d_pos,gl_box,Nslabs,min_slab,*last_min_vel);
        Scalar3 init = *last_min_vel;
        *last_min_vel = thrust::transform_reduce(member_ptr,member_ptr+group_size,
                                                 un_opt,init,min_bin_opt);
        }


    return cudaPeekAtLastError();
    }


template<bool flowX,bool flowY,bool flowZ>
void __global__ gpu_update_min_max_velocity_kernel(const unsigned int *const d_rtag,
                                                   Scalar4*const d_vel,
                                                   const unsigned int Ntotal,
                                                   const Scalar3 last_max_vel,
                                                   const Scalar3 last_min_vel)
    {
    unsigned int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= 1)
        return;
    const unsigned int min_tag = __scalar_as_int( last_min_vel.z);
    const unsigned int min_idx = d_rtag[min_tag];
    const unsigned int max_tag = __scalar_as_int( last_max_vel.z);
    const unsigned int max_idx = d_rtag[max_tag];
    //Is the particle local on the processor?
    //Swap the particles the new velocities.
    if( min_idx < Ntotal)
        {
        const Scalar new_min_vel = last_max_vel.x  / last_min_vel.y;
        if(flowX)
            d_vel[min_idx].x = new_min_vel;
        if(flowY)
            d_vel[min_idx].y = new_min_vel;
        if(flowZ)
            d_vel[min_idx].z = new_min_vel;
        }
    if( max_idx < Ntotal)
      {
        const Scalar new_max_vel = last_min_vel.x  / last_max_vel.y;
        if(flowX)
            d_vel[max_idx].x = new_max_vel;
        if(flowY)
            d_vel[max_idx].y = new_max_vel;
        if(flowZ)
            d_vel[max_idx].z = new_max_vel;
      }
    }

template<bool flowX,bool flowY,bool flowZ>
cudaError_t gpu_update_min_max_velocity(const unsigned int *const d_rtag,
                                        Scalar4*const d_vel,
                                        const unsigned int Ntotal,
                                        const Scalar3 last_max_vel,
                                        const Scalar3 last_min_vel)
    {
    assert( flowX | flowY | flowZ);
    if( flowX )
        { assert(flowY == false and flowZ == false); }
    if( flowY )
        { assert(flowX == false and flowZ == false); }
    if( flowZ )
        { assert(flowY == false and flowX == false); }

    dim3 grid( 1, 1, 1);
    dim3 threads(1, 1, 1);

    gpu_update_min_max_velocity_kernel<flowX,flowY,flowZ>
        <<<grid,threads>>>(d_rtag, d_vel, Ntotal,last_max_vel, last_min_vel);

    return cudaPeekAtLastError();
    }

//Explicit instances of the templates of VALID configurations
template cudaError_t gpu_update_min_max_velocity<true,false,false>(const unsigned int *const d_rtag,
                                                                   Scalar4*const d_vel,
                                                                   const unsigned int Ntotal,
                                                                   const Scalar3 last_max_vel,
                                                                   const Scalar3 last_min_vel);
template cudaError_t gpu_update_min_max_velocity<false,true,false>(const unsigned int *const d_rtag,
                                                                   Scalar4*const d_vel,
                                                                   const unsigned int Ntotal,
                                                                   const Scalar3 last_max_vel,
                                                                   const Scalar3 last_min_vel);
template cudaError_t gpu_update_min_max_velocity<false,false,true>(const unsigned int *const d_rtag,
                                                                   Scalar4*const d_vel,
                                                                   const unsigned int Ntotal,
                                                                   const Scalar3 last_max_vel,
                                                                   const Scalar3 last_min_vel);

template cudaError_t gpu_search_min_max_velocity<true,false,false,true,false,false>(
    const unsigned int group_size,
    const Scalar4*const d_vel,
    const Scalar4*const d_pos,
    const unsigned int *const d_tag,
    const unsigned int *const d_rtag,
    const unsigned int *const d_group_members,
    const BoxDim gl_box,
    const unsigned int Nslabs,
    const unsigned int max_slab,
    const unsigned int min_slab,
    Scalar3*const last_max_vel,
    Scalar3*const last_min_vel,
    const bool has_max_slab,
    const bool has_min_slab,
    const unsigned int blocksize);
template cudaError_t gpu_search_min_max_velocity<true,false,false,false,true,false>(
    const unsigned int group_size,
    const Scalar4*const d_vel,
    const Scalar4*const d_pos,
    const unsigned int *const d_tag,
    const unsigned int *const d_rtag,
    const unsigned int *const d_group_members,
    const BoxDim gl_box,
    const unsigned int Nslabs,
    const unsigned int max_slab,
    const unsigned int min_slab,
    Scalar3*const last_max_vel,
    Scalar3*const last_min_vel,
    const bool has_max_slab,
    const bool has_min_slab,
    const unsigned int blocksize);
template cudaError_t gpu_search_min_max_velocity<true,false,false,false,false,true>(
    const unsigned int group_size,
    const Scalar4*const d_vel,
    const Scalar4*const d_pos,
    const unsigned int *const d_tag,
    const unsigned int *const d_rtag,
    const unsigned int *const d_group_members,
    const BoxDim gl_box,
    const unsigned int Nslabs,
    const unsigned int max_slab,
    const unsigned int min_slab,
    Scalar3*const last_max_vel,
    Scalar3*const last_min_vel,
    const bool has_max_slab,
    const bool has_min_slab,
    const unsigned int blocksize);

template cudaError_t gpu_search_min_max_velocity<false,true,false,true,false,false>(
    const unsigned int group_size,
    const Scalar4*const d_vel,
    const Scalar4*const d_pos,
    const unsigned int *const d_tag,
    const unsigned int *const d_rtag,
    const unsigned int *const d_group_members,
    const BoxDim gl_box,
    const unsigned int Nslabs,
    const unsigned int max_slab,
    const unsigned int min_slab,
    Scalar3*const last_max_vel,
    Scalar3*const last_min_vel,
    const bool has_max_slab,
    const bool has_min_slab,
    const unsigned int blocksize);
template cudaError_t gpu_search_min_max_velocity<false,true,false,false,true,false>(
    const unsigned int group_size,
    const Scalar4*const d_vel,
    const Scalar4*const d_pos,
    const unsigned int *const d_tag,
    const unsigned int *const d_rtag,
    const unsigned int *const d_group_members,
    const BoxDim gl_box,
    const unsigned int Nslabs,
    const unsigned int max_slab,
    const unsigned int min_slab,
    Scalar3*const last_max_vel,
    Scalar3*const last_min_vel,
    const bool has_max_slab,
    const bool has_min_slab,
    const unsigned int blocksize);
template cudaError_t gpu_search_min_max_velocity<false,true,false,false,false,true>(
    const unsigned int group_size,
    const Scalar4*const d_vel,
    const Scalar4*const d_pos,
    const unsigned int *const d_tag,
    const unsigned int *const d_rtag,
    const unsigned int *const d_group_members,
    const BoxDim gl_box,
    const unsigned int Nslabs,
    const unsigned int max_slab,
    const unsigned int min_slab,
    Scalar3*const last_max_vel,
    Scalar3*const last_min_vel,
    const bool has_max_slab,
    const bool has_min_slab,
    const unsigned int blocksize);

template cudaError_t gpu_search_min_max_velocity<false,false,true,true,false,false>(
    const unsigned int group_size,
    const Scalar4*const d_vel,
    const Scalar4*const d_pos,
    const unsigned int *const d_tag,
    const unsigned int *const d_rtag,
    const unsigned int *const d_group_members,
    const BoxDim gl_box,
    const unsigned int Nslabs,
    const unsigned int max_slab,
    const unsigned int min_slab,
    Scalar3*const last_max_vel,
    Scalar3*const last_min_vel,
    const bool has_max_slab,
    const bool has_min_slab,
    const unsigned int blocksize);
template cudaError_t gpu_search_min_max_velocity<false,false,true,false,true,false>(
    const unsigned int group_size,
    const Scalar4*const d_vel,
    const Scalar4*const d_pos,
    const unsigned int *const d_tag,
    const unsigned int *const d_rtag,
    const unsigned int *const d_group_members,
    const BoxDim gl_box,
    const unsigned int Nslabs,
    const unsigned int max_slab,
    const unsigned int min_slab,
    Scalar3*const last_max_vel,
    Scalar3*const last_min_vel,
    const bool has_max_slab,
    const bool has_min_slab,
    const unsigned int blocksize);
template cudaError_t gpu_search_min_max_velocity<false,false,true,false,false,true>(
    const unsigned int group_size,
    const Scalar4*const d_vel,
    const Scalar4*const d_pos,
    const unsigned int *const d_tag,
    const unsigned int *const d_rtag,
    const unsigned int *const d_group_members,
    const BoxDim gl_box,
    const unsigned int Nslabs,
    const unsigned int max_slab,
    const unsigned int min_slab,
    Scalar3*const last_max_vel,
    Scalar3*const last_min_vel,
    const bool has_max_slab,
    const bool has_min_slab,
    const unsigned int blocksize);
