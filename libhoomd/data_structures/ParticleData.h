/*
Highly Optimized Object-oriented Many-particle Dynamics -- Blue Edition
(HOOMD-blue) Open Source Software License Copyright 2008-2011 Ames Laboratory
Iowa State University and The Regents of the University of Michigan All rights
reserved.

HOOMD-blue may contain modifications ("Contributions") provided, and to which
copyright is held, by various Contributors who have granted The Regents of the
University of Michigan the right to modify and/or distribute such Contributions.

You may redistribute, use, and create derivate works of HOOMD-blue, in source
and binary forms, provided you abide by the following conditions:

* Redistributions of source code must retain the above copyright notice, this
list of conditions, and the following disclaimer both in the code and
prominently in any materials provided with the distribution.

* Redistributions in binary form must reproduce the above copyright notice, this
list of conditions, and the following disclaimer in the documentation and/or
other materials provided with the distribution.

* All publications and presentations based on HOOMD-blue, including any reports
or published results obtained, in whole or in part, with HOOMD-blue, will
acknowledge its use according to the terms posted at the time of submission on:
http://codeblue.umich.edu/hoomd-blue/citations.html

* Any electronic documents citing HOOMD-Blue will link to the HOOMD-Blue website:
http://codeblue.umich.edu/hoomd-blue/

* Apart from the above required attributions, neither the name of the copyright
holder nor the names of HOOMD-blue's contributors may be used to endorse or
promote products derived from this software without specific prior written
permission.

Disclaimer

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND/OR ANY
WARRANTIES THAT THIS SOFTWARE IS FREE OF INFRINGEMENT ARE DISCLAIMED.

IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// Maintainer: joaander

/*! \file ParticleData.h
    \brief Defines the ParticleData class and associated utilities
*/

#ifdef WIN32
#pragma warning( push )
#pragma warning( disable : 4103 )
#endif

#ifndef __PARTICLE_DATA_H__
#define __PARTICLE_DATA_H__

#include "HOOMDMath.h"
#include "GPUArray.h"

#ifdef ENABLE_CUDA
#include "ParticleData.cuh"
#endif

#include "ExecutionConfiguration.h"

#include <boost/shared_ptr.hpp>
#include <boost/signals.hpp>
#include <boost/function.hpp>
#include <boost/utility.hpp>
#include <boost/dynamic_bitset.hpp>

#include <stdlib.h>
#include <vector>
#include <string>
#include <bitset>

using namespace std;

// windows doesn't understand __restrict__, it is __restrict instead
#ifdef WIN32
#define __restrict__ __restrict
#endif

/*! \ingroup hoomd_lib
    @{
*/

/*! \defgroup data_structs Data structures
    \brief All classes that are related to the fundamental data
        structures for storing particles.

    \details See \ref page_dev_info for more information
*/

/*! @}
*/

// Forward declaration of Profiler
class Profiler;

class BondData;

class WallData;

// Forward declaration of AngleData
class AngleData;

// Forward declaration of DihedralData
class DihedralData;

// Forward declaration of RigidData
class RigidData;

// Forward declaration of IntegratorData
class IntegratorData;

//! List of optional fields that can be enabled in ParticleData
struct pdata_flag
    {
    //! The enum
    enum Enum
        {
        isotropic_virial=0,  //!< Bit id in PDataFlags for the isotropic virial
        potential_energy,    //!< Bit id in PDataFlags for the potential energy
        pressure_tensor,     //!< Bit id in PDataFlags for the full virial
        };
    };

//! flags determines which optional fields in in the particle data arrays are to be computed / are valid
typedef std::bitset<32> PDataFlags;

//! Defines a simple structure to deal with complex numbers
/*! This structure is useful to deal with complex numbers for such situations
    as Fourier transforms. Note that we do not need any to define any operations and the
    default constructor is good enough
*/
struct CScalar
    {
    Scalar r; //!< Real part
    Scalar i; //!< Imaginary part
    };

//! Defines a simple moment of inertia structure
/*! This moment of interia is stored per particle. Because there are no per-particle body update steps in the
    design of hoomd, these values are never read or used except at initialization. Thus, a simple descriptive
    structure is used instead of an advanced and complicated GPUArray strided data array.
    
    The member variable components stores the 6 components of an upper-trianglar moment of inertia tensor.
    The components are, in order, Ixx, Ixy, Ixz, Iyy, Iyz, Izz.
    
    They are initialized to 0 and left that way if not specified in an initialization file.
*/
struct InertiaTensor
    {
    InertiaTensor()
        {
        for (unsigned int i = 0; i < 6; i++)
            components[i] = Scalar(0.0);
        }
    
    //! Set the components of the tensor
    void set(Scalar c0, Scalar c1, Scalar c2, Scalar c3, Scalar c4, Scalar c5)
        {
        components[0] = c0;
        components[1] = c1;
        components[2] = c2;
        components[3] = c3;
        components[4] = c4;
        components[5] = c5;
        }
    
    Scalar components[6];   //!< Stores the components of the inertia tensor
    };

//! Stores box dimensions
/*! All particles in the ParticleData structure are inside of a box. This struct defines
    that box. Inside is defined as x >= xlo && x < xhi, and similarly for y and z.
    \note Requirements state that xhi = -xlo, and the same goes for y and z
    \ingroup data_structs
*/
struct BoxDim
    {
    Scalar xlo; //!< Minimum x coord of the box
    Scalar xhi; //!< Maximum x coord of the box
    Scalar ylo; //!< Minimum y coord of the box
    Scalar yhi; //!< Maximum y coord of the box
    Scalar zlo; //!< Minimum z coord of the box
    Scalar zhi; //!< Maximum z coord of the box
    
    //! Constructs a useless box
    BoxDim();
    //! Constructs a box from -Len/2 to Len/2
    BoxDim(Scalar Len);
    //! Constructs a box from -Len_x/2 to Len_x/2 for each dimension x
    BoxDim(Scalar Len_x, Scalar Len_y, Scalar Len_z);
    };

//! Sentinel value in \a body to signify that this particle does not belong to a rigid body
const unsigned int NO_BODY = 0xffffffff;

//! Handy structure for passing around per-particle data
/* TODO: document me
*/
struct SnapshotParticleData {
    //! constructor
    //! \param N number of particles to allocate memory for
    SnapshotParticleData(unsigned int N)
       {
       pos.resize(N);
       vel.resize(N);
       accel.resize(N);
       type.resize(N);
       mass.resize(N);
       charge.resize(N);
       diameter.resize(N);
       image.resize(N);
       rtag.resize(N);
       global_tag.resize(N);
       body.resize(N);
       size = N;
       }

    std::vector<Scalar3> pos;       //!< positions
    std::vector<Scalar3> vel;       //!< velocities
    std::vector<Scalar3> accel;     //!< accelerations
    std::vector<unsigned int> type; //!< types
    std::vector<Scalar> mass;       //!< masses
    std::vector<Scalar> charge;     //!< charges
    std::vector<Scalar> diameter;   //!< diameters
    std::vector<int3> image;        //!< images
    std::vector<unsigned int> rtag; //!< reverse-lookup tags
    std::vector<unsigned int> global_tag; //! global tag
    std::vector<unsigned int> body; //!< body ids
    unsigned int size;              //!< number of particles in this snapshot
    };

//! Abstract interface for initializing a ParticleData
/*! A ParticleDataInitializer should only be used with the appropriate constructor
    of ParticleData(). That constructure calls the methods of this class to determine
    the number of particles, number of particle types, the simulation box, and then
    initializes itself. Then initSnapshot() is called to fill out the ParticleDataSnapshot
    to be used to initalize the particle data arrays

    \note This class is an abstract interface with pure virtual functions. Derived
    classes must implement these methods.
    \ingroup data_structs
    */
class ParticleDataInitializer
    {
    public:
        //! Empty constructor
        ParticleDataInitializer() { }
        //! Empty Destructor
        virtual ~ParticleDataInitializer() { }
        
        //! Returns the number of local particles to be initialized
        virtual unsigned int getNumParticles() const = 0;
        
        //! Returns the number of global particles in the simulation
        virtual unsigned int getNumGlobalParticles() const = 0;

        //! Returns the number of particles types to be initialized
        virtual unsigned int getNumParticleTypes() const = 0;
        
        //! Returns the box the particles will sit in
        virtual BoxDim getBox() const = 0;
        
        //! Initializes the snapshot of the particle data arrays
        /*! \param snapshot snapshot to initialize
        */
        virtual void initSnapshot(SnapshotParticleData& snapshot) const = 0;
        
        //! Initialize the simulation walls
        /*! \param wall_data Shared pointer to the WallData to initialize
            This base class defines an empty method, as walls are optional
        */
        virtual void initWallData(boost::shared_ptr<WallData> wall_data) const {}

        //! Initialize the integrator variables
        /*! \param integrator_data Shared pointer to the IntegratorData to initialize
            This base class defines an empty method, since initializing the 
            integrator variables is optional
        */
        virtual void initIntegratorData(boost::shared_ptr<IntegratorData> integrator_data) const {}
        
        //! Intialize the type mapping
        virtual std::vector<std::string> getTypeMapping() const = 0;

        //! Returns the number of dimensions
        /*! The base class returns 3 */
        virtual unsigned int getNumDimensions() const
            {
            return 3;
            }
        
        //! Returns the number of bond types to be created
        /*! Bonds are optional: the base class returns 1 */
        virtual unsigned int getNumBondTypes() const
            {
            return 1;
            }
            
        /*! Angles are optional: the base class returns 1 */
        virtual unsigned int getNumAngleTypes() const
            {
            return 1;
            }
            
        /*! Dihedrals are optional: the base class returns 1 */
        virtual unsigned int getNumDihedralTypes() const
            {
            return 1;
            }
            
        /*! Impropers are optional: the base class returns 1 */
        virtual unsigned int getNumImproperTypes() const
            {
            return 1;
            }
            
        //! Initialize the bond data
        /*! \param bond_data Shared pointer to the BondData to be initialized
            Bonds are optional: the base class does nothing
        */
        virtual void initBondData(boost::shared_ptr<BondData> bond_data) const {}
        
        //! Initialize the angle data
        /*! \param angle_data Shared pointer to the AngleData to be initialized
            Angles are optional: the base class does nothing
        */
        virtual void initAngleData(boost::shared_ptr<AngleData> angle_data) const {}
        
        //! Initialize the dihedral data
        /*! \param dihedral_data Shared pointer to the DihedralData to be initialized
            Dihedrals are optional: the base class does nothing
        */
        virtual void initDihedralData(boost::shared_ptr<DihedralData> dihedral_data) const {}
        
        //! Initialize the improper data
        /*! \param improper_data Shared pointer to the ImproperData to be initialized
            Impropers are optional: the base class does nothing
        */
        virtual void initImproperData(boost::shared_ptr<DihedralData> improper_data) const {}
        
        //! Initialize the rigid data
        /*! \param rigid_data Shared pointer to the RigidData to be initialized
            Rigid bodies are optional: the base class does nothing
        */
        virtual void initRigidData(boost::shared_ptr<RigidData> rigid_data) const {}
        
        //! Initialize the orientation data
        /*! \param orientation Pointer to one orientation per particle to be initialized
        */
        virtual void initOrientation(Scalar4 *orientation) const {}
        
        //! Initialize the inertia tensor data
        /*! \param moment_inertia Pointer to one inertia tensor per particle to be initialize (in tag order!)
        */
        virtual void initMomentInertia(InertiaTensor *moment_inertia) const {}
            
    };

//! Manages all of the data arrays for the particles
/*! ParticleData stores and manages particle coordinates, velocities, accelerations, type,
    and tag information. This data must be available both via the CPU and GPU memories.
    All copying of data back and forth from the GPU is accomplished transparently by GPUArray.

    For performance reasons, data is stored as simple arrays. Once a handle to the particle data
    GPUArrays has been acquired, the coordinates of the particle with
    <em>index</em> \c i can be accessed with <code>pos_array_handle.data[i].x</code>,
    <code>pos_array_handle.data[i].y</code>, and <code>pos_array_handle.data[i].z</code>
    where \c i runs from 0 to <code>getN()</code>.

    Velocities and other propertys can be accessed in a similar manner.
    
    \note Position and type are combined into a single Scalar4 quantity. x,y,z specifies the position and w specifies
    the type. Use __scalar_as_int() / __int_as_scalar() (or __int_as_float() / __float_as_int()) to extract / set
    this integer that is masquerading as a scalar.
    
    \note Velocity and mass are combined into a single Scalar4 quantity. x,y,z specifies the velocity and w specifies
    the mass.

    \warning Local particles can and will be rearranged in the arrays throughout a simulation.
    So, a particle that was once at index 5 may be at index 123 the next time the data
    is acquired. Individual particles can be tracked through all these changes by their local tag.
    The tag of a particle is stored in the \c m_tag array, and the ith element contains the tag of the particle
    with index i. Conversely, the the index of a particle with tag \c tag can be read from
    the element at position \c tag in the a \c m_rtag array.

    In additon to a local tag, there is also a global tag that is unique among all processors in a parallel
    simulation. The tag of a particle with index i is stored in the \c m_global_tag array.
>>>>>>> Partial set of changes to enable domain decomposition

    In order to help other classes deal with particles changing indices, any class that
    changes the order must call notifyParticleSort(). Any class interested in being notified
    can subscribe to the signal by calling connectParticleSort().

    Some fields in ParticleData are not computed and assigned by default because they require additional processing
    time. PDataFlags is a bitset that lists which flags (enumerated in pdata_flag) are enable/disabled. Computes should
    call getFlags() and compute the requested quantities whenever the corresponding flag is set. Updaters and Analyzers
    can request flags be computed via their getRequestedPDataFlags() methods. A particular updater or analyzer should 
    return a bitset PDataFlags with only the bits set for the flags that it needs. During a run, System will query
    the updaters and analyzers that are to be executed on the current step. All of the flag requests are combined
    with the binary or operation into a single set of flag requests. System::run() then sets the flags by calling
    setPDataFlags so that the computes produce the requested values during that step.
    
    These fields are:
     - pdata_flag::isotropic_virial - specify that the net_virial should be/is computed (getNetVirial)
     - pdata_flag::potential_energy - specify that the potential energy .w component stored in the net force array 
       (getNetForce) is valid
     - pdata_flag::pressure_tensor - specify that the full virial tensor is valid
       
    If these flags are not set, these arrays can still be read but their values may be incorrect.
    
    If any computation is unable to supply the appropriate values (i.e. rigid body virial can not be computed
    until the second step of the simulation), then it should remove the flag to signify that the values are not valid.
    Any analyzer/updater that expects the value to be set should check the flags that are actually set.
    
    \note When writing to the particle data, particles must not be moved outside the box.
    In debug builds, any aquire will fail an assertion if this is done.
    \ingroup data_structs
    
    Anisotropic particles are handled by storing an orientation quaternion for every particle in the simulation.
    Similarly, a net torque is computed and stored for each particle. The design decision made is to not
    duplicate efforts already made to enable composite bodies of anisotropic particles. So the particle orientation
    is a read only quantity when used by most of HOOMD. To integrate this degree of freedom forward, the particle
    must be part of a composite body (stored and handled by RigidData) (there can be single-particle bodies,
    of course) where integration methods like NVERigid will handle updating the degrees of freedom of the composite
    body and then set the constrained position, velocity, and orientation of the constituent particles.
    
    To enable correct initialization of the composite body moment of inertia, each particle is also assigned
    an individual moment of inertia which is summed up correctly to determine the composite body's total moment of
    inertia. As such, the initial particle moment of inertias are only ever used during initialization and do not
    need to be stored in an efficient GPU data structure. Nor does the inertia tensor data need to be resorted,
    so it will always remain in tag order.
    
    Access the orientation quaternion of each particle with the GPUArray gotten from getOrientationArray(), the net
    torque with getTorqueArray(). Individual inertia tensor values can be accessed with getInertiaTensor() and
    setInertiaTensor()
*/
class ParticleData : boost::noncopyable
    {
    public:
        //! Construct with N particles in the given box
        ParticleData(unsigned int N,
                     const BoxDim &box,
                     unsigned int n_types,
                     boost::shared_ptr<ExecutionConfiguration> exec_conf);
        
        //! Construct from an initializer
        ParticleData(const ParticleDataInitializer& init,
                     boost::shared_ptr<ExecutionConfiguration> exec_conf);
        
        //! Destructor
        virtual ~ParticleData() {}
        
        //! Get the simulation box
        const BoxDim& getBox() const;
        //! Set the simulation box
        void setBox(const BoxDim &box);

        //! Access the execution configuration
        boost::shared_ptr<const ExecutionConfiguration> getExecConf() const
            {
            return m_exec_conf;
            }
            
        //! Get the number of particles
        /*! \return Number of particles in the box
        */
        inline unsigned int getN() const
            {
            return m_nparticles;
            }

        //! Get the currrent maximum number of particles
        /*\ return Maximum number of particles that can be stored in the particle array
        * this number has to be larger than getN() + getNGhosts()
        */
        inline unsigned int getMaxN() const
            {
            return m_max_nparticles;
            }

        //! Get current number of ghost particles
        /*\ return Number of ghost particles
        */
        inline unsigned int getNGhosts() const
            {
            return m_nghosts;
            }

        //! Get the global number of particles in the simulation
        /*!\ return Global number of particles
         */
        inline unsigned int getNGlobal() const
            {
            return m_nglobal;
            }

        //! Get the number of particle types
        /*! \return Number of particle types
            \note Particle types are indexed from 0 to NTypes-1
        */
        unsigned int getNTypes() const
            {
            return m_ntypes;
            }
            
        //! Get the maximum diameter of the particle set
        /*! \return Maximum Diameter Value
        */
        Scalar getMaxDiameter() const
            {
            Scalar maxdiam = 0;
            ArrayHandle< Scalar > h_diameter(getDiameters(), access_location::host, access_mode::read);
            for (unsigned int i = 0; i < m_nparticles; i++) if (h_diameter.data[i] > maxdiam) maxdiam = h_diameter.data[i];
            return maxdiam;
            }
            
        //! return positions and types
        const GPUArray< Scalar4 >& getPositions() const { return m_pos; }

        //! return velocities and masses
        const GPUArray< Scalar4 >& getVelocities() const { return m_vel; }
        
        //! return accelerations
        const GPUArray< Scalar3 >& getAccelerations() const { return m_accel; }

        //! return charges
        const GPUArray< Scalar >& getCharges() const { return m_charge; }

        //! return diameters
        const GPUArray< Scalar >& getDiameters() const { return m_diameter; }

        //! return images
        const GPUArray< int3 >& getImages() const { return m_image; }

        //! return tags
        const GPUArray< unsigned int >& getTags() const { return m_tag; }

        //! return reverse-lookup tags
        const GPUArray< unsigned int >& getRTags() const { return m_rtag; }

        //! return body ids
        const GPUArray< unsigned int >& getBodies() const { return m_body; }

        //! return global tags
        const GPUArray< unsigned int >& getGlobalTags() const { return m_global_tag; }

        //! return map of global reverse lookup tags
        const GPUArray< unsigned int >& getGlobalRTags() { return m_global_rtag; }

#ifdef ENABLE_CUDA
        //! Get the box for the GPU
        /*! \returns Box dimensions suitable for passing to the GPU code
        */
        const gpu_boxsize& getBoxGPU() const
            {
            return m_gpu_box;
            }
            
#endif
        
        //! Set the profiler to profile CPU<-->GPU memory copies
        /*! \param prof Pointer to the profiler to use. Set to NULL to deactivate profiling
        */
        void setProfiler(boost::shared_ptr<Profiler> prof)
            {
            m_prof=prof;
            }
            
        //! Connects a function to be called every time the particles are rearranged in memory
        boost::signals::connection connectParticleSort(const boost::function<void ()> &func);
        
        //! Notify listeners that the particles have been rearranged in memory
        void notifyParticleSort();
        
        //! Connects a function to be called every time the box size is changed
        boost::signals::connection connectBoxChange(const boost::function<void ()> &func);

        //! Connects a function to be called every time the maximum particle number changes
        boost::signals::connection connectMaxParticleNumberChange(const boost::function< void()> &func);

        //! Connects a function to be called every time particles are added or deleted from the system
        boost::signals::connection connectParticleNumberChange(const boost::function< void() > &func);

        //! Notify listeners that the current particle number has changed
        void notifyParticleNumberChange();

        //! Gets the particle type index given a name
        unsigned int getTypeByName(const std::string &name) const;

        //! Gets the name of a given particle type index
        std::string getNameByType(unsigned int type) const;
        
        //! Get the net force array
        const GPUArray< Scalar4 >& getNetForce() const { return m_net_force; }
        
        //! Get the net virial array
        const GPUArray< Scalar >& getNetVirial() const { return m_net_virial; }
        
        //! Get the net torque array
        const GPUArray< Scalar4 >& getNetTorqueArray() const { return m_net_torque; }
        
        //! Get the orientation array
        const GPUArray< Scalar4 >& getOrientationArray() const { return m_orientation; }

        //! Find out if the particle identified by a global tag is stored in the local ParticleData
        /*! By definition, it is local if there exists a reverse lookup entry for that global
            tag and it points to a particle with idx < getN(), i.e. it is not a ghost particle
         */
        inline bool isLocal(unsigned int global_tag) const
            {
            return m_is_local[global_tag];
            }

        //! Set a flag to indicate that a particle with a specified global tag is local
        void setLocal(unsigned int global_tag)
            {
            m_is_local[global_tag] = true;
            }

        //! Get the current position of a particle
        Scalar3 getPosition(unsigned int global_tag) const
            {
            unsigned int idx = getGlobalRTag(global_tag);

            ArrayHandle< Scalar4 > h_pos(m_pos, access_location::host, access_mode::read);
            Scalar3 result = make_scalar3(h_pos.data[idx].x, h_pos.data[idx].y, h_pos.data[idx].z);
            return result;
            }

        //! Get the current velocity of a particle
        Scalar3 getVelocity(unsigned int global_tag) const
            {
            unsigned int idx = getGlobalRTag(global_tag);
            assert(idx < getN());

            ArrayHandle< Scalar4 > h_vel(m_vel, access_location::host, access_mode::read);
            Scalar3 result = make_scalar3(h_vel.data[idx].x, h_vel.data[idx].y, h_vel.data[idx].z);
            return result;
            }
        //! Get the current acceleration of a particle
        Scalar3 getAcceleration(unsigned int global_tag) const
            {
            unsigned int idx = getGlobalRTag(global_tag);
            assert(idx < getN());

            ArrayHandle< Scalar3 > h_accel(m_accel, access_location::host, access_mode::read);
            Scalar3 result = make_scalar3(h_accel.data[idx].x, h_accel.data[idx].y, h_accel.data[idx].z);
            return result;
            }
        //! Get the current image flags of a particle
        int3 getImage(unsigned int global_tag) const
            {
            unsigned int idx = getGlobalRTag(global_tag);
            assert(idx < getN());

            ArrayHandle< int3 > h_image(m_image, access_location::host, access_mode::read);
            int3 result = make_int3(h_image.data[idx].x, h_image.data[idx].y, h_image.data[idx].z);
            return result;
            }
        //! Get the current charge of a particle
        Scalar getCharge(unsigned int global_tag) const
            {
            unsigned int idx = getGlobalRTag(global_tag);

            ArrayHandle< Scalar > h_charge(m_charge, access_location::host, access_mode::read);
            Scalar result = h_charge.data[idx];
            return result;
            }
        //! Get the current mass of a particle
        Scalar getMass(unsigned int global_tag) const
            {
            unsigned int idx = getGlobalRTag(global_tag);
            assert(idx < getN());

            ArrayHandle< Scalar4 > h_vel(m_vel, access_location::host, access_mode::read);
            Scalar result = h_vel.data[idx].w;
            return result;
            }
        //! Get the current diameter of a particle
        Scalar getDiameter(unsigned int global_tag) const
            {
            unsigned int idx = getGlobalRTag(global_tag);

            ArrayHandle< Scalar > h_diameter(m_diameter, access_location::host, access_mode::read);
            Scalar result = h_diameter.data[idx];
            return result;
            }
        //! Get the current diameter of a particle
        unsigned int getBody(unsigned int global_tag) const
            {
            unsigned int idx = getGlobalRTag(global_tag);
            assert(idx < getN());

            ArrayHandle< unsigned int > h_body(m_body, access_location::host, access_mode::read);
            unsigned int result = h_body.data[idx];
            return result;
            }
        //! Get the current type of a particle
        unsigned int getType(unsigned int global_tag) const
            {
            unsigned int idx = getGlobalRTag(global_tag);

            ArrayHandle< Scalar4 > h_pos(m_pos, access_location::host, access_mode::read);
            unsigned int result = __scalar_as_int(h_pos.data[idx].w);
            return result;
            }

        //! Get the current index of a particle with a given local tag
        unsigned int getRTag(unsigned int tag) const
            {
            assert(tag < getN());
            ArrayHandle< unsigned int> h_rtag(m_rtag, access_location::host, access_mode::read);
            unsigned int idx = h_rtag.data[tag];
            return idx;
            }

        //! Get the current index of a particle with a given global tag
        inline unsigned int getGlobalRTag(unsigned int global_tag) const
            {
            assert(global_tag < m_nglobal);
            ArrayHandle< unsigned int> h_global_rtag(m_global_rtag,access_location::host, access_mode::read);
            unsigned int idx = h_global_rtag.data[global_tag];
            assert(idx < getN() + getNGhosts());
            return idx;
            }

        //! Get the orientation of a particle with a given tag
        Scalar4 getOrientation(unsigned int global_tag) const
            {
            ArrayHandle< Scalar4 > h_orientation(m_orientation, access_location::host, access_mode::read);
            unsigned int idx = getGlobalRTag(global_tag);
            assert(idx < getN());
            return h_orientation.data[idx];
            }
        //! Get the inertia tensor of a particle with a given tag
        const InertiaTensor& getInertiaTensor(unsigned int tag) const
            {
            return m_inertia_tensor[tag];
            }
        //! Get the net force / energy on a given particle
        Scalar4 getPNetForce(unsigned int global_tag) const
            {
            unsigned int idx = getGlobalRTag(global_tag);
            assert(idx < getN());
            ArrayHandle< Scalar4 > h_net_force(m_net_force, access_location::host, access_mode::read);
            return h_net_force.data[idx];
            }
        //! Get the net torque on a given particle
        Scalar4 getNetTorque(unsigned int tag)
            {
            assert(tag < getN());
            ArrayHandle< Scalar4 > h_net_torque(m_net_force, access_location::host, access_mode::read);
            ArrayHandle< unsigned int> h_rtag(m_rtag, access_location::host, access_mode::read);
            unsigned int idx = h_rtag.data[tag];
            return h_net_torque.data[idx];
            }

        //! Set the current position of a particle
        void setPosition(unsigned int global_tag, const Scalar3& pos)
            {
            unsigned int idx = getGlobalRTag(global_tag);
            assert(idx < getN());
            ArrayHandle< Scalar4 > h_pos(m_pos, access_location::host, access_mode::readwrite);
            h_pos.data[idx].x = pos.x; h_pos.data[idx].y = pos.y; h_pos.data[idx].z = pos.z;
            }
        //! Set the current velocity of a particle
        void setVelocity(unsigned int global_tag, const Scalar3& vel)
            {
            unsigned int idx = getGlobalRTag(global_tag);
            assert(idx < getN());
            ArrayHandle< Scalar4 > h_vel(m_vel, access_location::host, access_mode::readwrite);
            h_vel.data[idx].x = vel.x; h_vel.data[idx].y = vel.y; h_vel.data[idx].z = vel.z;
            }
        //! Set the current image flags of a particle
        void setImage(unsigned int global_tag, const int3& image)
            {
            unsigned int idx = getGlobalRTag(global_tag);
            assert(idx < getN());
            ArrayHandle< int3 > h_image(m_image, access_location::host, access_mode::readwrite);
            h_image.data[idx] = image;
            }
        //! Set the current charge of a particle
        void setCharge(unsigned int global_tag, Scalar charge)
            {
            unsigned int idx = getGlobalRTag(global_tag);
            assert(idx < getN());
            ArrayHandle< Scalar > h_charge(m_charge, access_location::host, access_mode::readwrite);
            h_charge.data[idx] = charge;
            }
        //! Set the current mass of a particle
        void setMass(unsigned int global_tag, Scalar mass)
            {
            unsigned int idx = getGlobalRTag(global_tag);
            assert(idx < getN());
            ArrayHandle< Scalar4 > h_vel(m_vel, access_location::host, access_mode::readwrite);
            h_vel.data[idx].w = mass;
            }
        //! Set the current diameter of a particle
        void setDiameter(unsigned int global_tag, Scalar diameter)
            {
            unsigned int idx = getGlobalRTag(global_tag);
            assert(idx < getN());
            ArrayHandle< Scalar > h_diameter(m_diameter, access_location::host, access_mode::readwrite);
            h_diameter.data[idx] = diameter;
            }
        //! Set the current diameter of a particle
        void setBody(unsigned int global_tag, int body)
            {
            unsigned int idx = getGlobalRTag(global_tag);
            assert(idx < getN());
            ArrayHandle< unsigned int > h_body(m_body, access_location::host, access_mode::readwrite);
            h_body.data[idx] = body;
            }
        //! Set the current type of a particle
        void setType(unsigned int global_tag, unsigned int typ)
            {
            unsigned int idx = getGlobalRTag(global_tag);
            assert(idx < getN());
            assert(typ < getNTypes());
            ArrayHandle< Scalar4 > h_pos(m_pos, access_location::host, access_mode::readwrite);
            h_pos.data[idx].w = __int_as_scalar(typ);
            }
        //! Set the orientation of a particle with a given tag
        void setOrientation(unsigned int global_tag, const Scalar4& orientation)
            {
            unsigned int idx = getGlobalRTag(global_tag);
            assert(idx < getN());
            ArrayHandle< Scalar4 > h_orientation(m_orientation, access_location::host, access_mode::readwrite);
            h_orientation.data[idx] = orientation;
            }
        //! Get the inertia tensor of a particle with a given tag
        void setInertiaTensor(unsigned int global_tag, const InertiaTensor& tensor)
            {
            unsigned int idx = getGlobalRTag(global_tag);
            assert(idx < getN());
            ArrayHandle< unsigned int > h_tag(m_tag, access_location::host, access_mode::read);
            unsigned int tag = h_tag.data[idx];
            m_inertia_tensor[tag] = tensor;
            }
            
        //! Get the particle data flags
        PDataFlags getFlags() { return m_flags; }
        
        //! Set the particle data flags
        /*! \note Setting the flags does not make the requested quantities immediately available. Only after the next
            set of compute() calls will the requested values be computed. The System class talks to the various
            analyzers and updaters to determine the value of the flags for any given time step.
        */
        void setFlags(const PDataFlags& flags) { m_flags = flags; }
        
        //! Remove the given flag
        void removeFlag(pdata_flag::Enum flag) { m_flags[flag] = false; }

        //! Initialize from a snapshot
        void initializeFromSnapshot(const SnapshotParticleData & snapshot);

        //! Take a snapshot
        void takeSnapshot(SnapshotParticleData &snapshot);

        //! Remove particles from the domain
        void removeParticles(unsigned int *indices, const unsigned int n);

        //! Add particles to the domain
        void addParticles(const unsigned int n);

        //! Add ghost particles to system
        void addGhostParticles(const unsigned int nghosts);

        //! Remove all ghost particles from system
        void removeAllGhostParticles()
            {
            m_nghosts = 0;
            }

    private:
        BoxDim m_box;                               //!< The simulation box
        boost::shared_ptr<ExecutionConfiguration> m_exec_conf; //!< The execution configuration
        unsigned int m_ntypes;                      //!< Number of particle types
        
        std::vector<std::string> m_type_mapping;    //!< Mapping between particle type indices and names
        
        boost::signal<void ()> m_sort_signal;       //!< Signal that is triggered when particles are sorted in memory
        boost::signal<void ()> m_boxchange_signal;  //!< Signal that is triggered when the box size changes
        boost::signal<void ()> m_max_particle_num_signal; //!< Signal that is triggered when the maximum particle number changes
        boost::signal<void ()> m_particle_num_signal; //!< Signal that is triggered when the current particle number changes

        unsigned int m_nparticles;                  //!< number of particles
        unsigned int m_nghosts;                     //!< number of ghost particles
        unsigned int m_max_nparticles;              //!< maximum number of particles
        unsigned int m_nglobal;                     //!< global number of particles

        // per-particle data
        GPUArray<Scalar4> m_pos;                    //!< particle positions and types
        GPUArray<Scalar4> m_vel;                    //!< particle velocities and masses
        GPUArray<Scalar3> m_accel;                  //!<  particle accelerations
        GPUArray<Scalar> m_charge;                  //!<  particle charges
        GPUArray<Scalar> m_diameter;                //!< particle diameters
        GPUArray<int3> m_image;                     //!< particle images
        GPUArray<unsigned int> m_tag;               //!< particle tags
        GPUArray<unsigned int> m_rtag;              //!< reverse lookup tags
        GPUArray<unsigned int> m_global_tag;        //!< global particle tags
        GPUArray<unsigned int> m_global_rtag;       //! reverse lookup of local particle indices from global tags
        GPUArray<unsigned int> m_body;              //!< rigid body ids
        boost::dynamic_bitset<> m_is_local;         //!< One bit per global tag, indicates whether a particle is local

        boost::shared_ptr<Profiler> m_prof;         //!< Pointer to the profiler. NULL if there is no profiler.
        
        GPUArray< Scalar4 > m_net_force;             //!< Net force calculated for each particle
        GPUArray< Scalar > m_net_virial;             //!< Net virial calculated for each particle (2D GPU array of dimensions 6*number of particles)
        GPUArray< Scalar4 > m_net_torque;            //!< Net torque calculated for each particle
        GPUArray< Scalar4 > m_orientation;           //!< Orientation quaternion for each particle (ignored if not anisotropic)
        std::vector< InertiaTensor > m_inertia_tensor; //!< Inertia tensor for each particle
        
        PDataFlags m_flags;                          //!< Flags identifying which optional fields are valid
        
#ifdef ENABLE_CUDA
        //! Simple type for identifying where the most up to date particle data is
        gpu_boxsize m_gpu_box;              //!< Mirror structure of m_box for the GPU
#endif
        
        //! Helper function to allocate particle data
        void allocate(unsigned int N, unsigned int nglobal);

        //! Helper function to reallocate particle data
        void reallocate(unsigned int max_n);

        //! Helper function to check that particles are in the box
        bool inBox();
    };


//! Exports the BoxDim class to python
void export_BoxDim();
//! Exports ParticleDataInitializer to python
void export_ParticleDataInitializer();
//! Exports ParticleData to python
void export_ParticleData();
//! Export SnapshotParticleData to python
void export_SnapshotParticleData();

#endif

#ifdef WIN32
#pragma warning( pop )
#endif

