# -- start license --
# Highly Optimized Object-oriented Many-particle Dynamics -- Blue Edition
# (HOOMD-blue) Open Source Software License Copyright 2009-2016 The Regents of
# the University of Michigan All rights reserved.

# HOOMD-blue may contain modifications ("Contributions") provided, and to which
# copyright is held, by various Contributors who have granted The Regents of the
# University of Michigan the right to modify and/or distribute such Contributions.

# You may redistribute, use, and create derivate works of HOOMD-blue, in source
# and binary forms, provided you abide by the following conditions:

# * Redistributions of source code must retain the above copyright notice, this
# list of conditions, and the following disclaimer both in the code and
# prominently in any materials provided with the distribution.

# * Redistributions in binary form must reproduce the above copyright notice, this
# list of conditions, and the following disclaimer in the documentation and/or
# other materials provided with the distribution.

# * All publications and presentations based on HOOMD-blue, including any reports
# or published results obtained, in whole or in part, with HOOMD-blue, will
# acknowledge its use according to the terms posted at the time of submission on:
# http://codeblue.umich.edu/hoomd-blue/citations.html

# * Any electronic documents citing HOOMD-Blue will link to the HOOMD-Blue website:
# http://codeblue.umich.edu/hoomd-blue/

# * Apart from the above required attributions, neither the name of the copyright
# holder nor the names of HOOMD-blue's contributors may be used to endorse or
# promote products derived from this software without specific prior written
# permission.

# Disclaimer

# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND/OR ANY
# WARRANTIES THAT THIS SOFTWARE IS FREE OF INFRINGEMENT ARE DISCLAIMED.

# IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
# OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# -- end license --

# Maintainer: joaander / All Developers are free to add commands for new features

from hoomd import _hoomd
from hoomd.md import _md;
import sys;
import hoomd;

## \package hoomd.force
# \brief Other types of forces
#
# This package contains various forces that don't belong in any of the other categories

## \internal
# \brief Base class for forces
#
# A force in hoomd_script reflects a ForceCompute in c++. It is responsible
# for all high-level management that happens behind the scenes for hoomd_script
# writers. 1) The instance of the c++ analyzer itself is tracked and added to the
# System 2) methods are provided for disabling the force from being added to the
# net force on each particle
class _force(hoomd.meta._metadata):
    ## \internal
    # \brief Constructs the force
    #
    # \param name name of the force instance
    #
    # Initializes the cpp_analyzer to None.
    # If specified, assigns a name to the instance
    # Assigns a name to the force in force_name;
    def __init__(self, name=None):
        # check if initialization has occured
        if not hoomd.init.is_initialized():
            hoomd.context.msg.error("Cannot create force before initialization\n");
            raise RuntimeError('Error creating force');

        # Allow force to store a name.  Used for discombobulation in the logger
        if name is None:
            self.name = "";
        else:
            self.name="_" + name;

        self.cpp_force = None;

        # increment the id counter
        id = _force.cur_id;
        _force.cur_id += 1;

        self.force_name = "force%d" % (id);
        self.enabled = True;
        self.log =True;
        hoomd.context.current.forces.append(self);

        # base class constructor
        hoomd.meta._metadata.__init__(self)

    ## \var enabled
    # \internal
    # \brief True if the force is enabled

    ## \var cpp_force
    # \internal
    # \brief Stores the C++ side ForceCompute managed by this class

    ## \var force_name
    # \internal
    # \brief The Force's name as it is assigned to the System

    ## \internal
    # \brief Checks that proper initialization has completed
    def check_initialization(self):
        # check that we have been initialized properly
        if self.cpp_force is None:
            hoomd.context.msg.error('Bug in hoomd_script: cpp_force not set, please report\n');
            raise RuntimeError();


    ## Disables the force
    # \param log Set to True if you plan to continue logging the potential energy associated with this force.
    #
    # \b Examples:
    # \code
    # force.disable()
    # force.disable(log=True)
    # \endcode
    #
    # Executing the disable command will remove the force from the simulation.
    # Any run() command executed after disabling a force will not calculate or
    # use the force during the simulation. A disabled force can be re-enabled
    # with enable()
    #
    # By setting \a log to True, the values of the force can be logged even though the forces are not applied
    # in the simulation.  For forces that use cutoff radii, setting \a log=True will cause the correct r_cut values
    # to be used throughout the simulation, and therefore possibly drive the neighbor list size larger than it
    # otherwise would be. If \a log is left False, the potential energy associated with this force will not be
    # available for logging.
    #
    # To use this command, you must have saved the force in a variable, as
    # shown in this example:
    # \code
    # force = pair.some_force()
    # # ... later in the script
    # force.disable()
    # force.disable(log=True)
    # \endcode
    def disable(self, log=False):
        hoomd.util.print_status_line();
        self.check_initialization();

        # check if we are already disabled
        if not self.enabled:
            hoomd.context.msg.warning("Ignoring command to disable a force that is already disabled");
            return;

        self.enabled = False;
        self.log = log;

        # remove the compute from the system if it is not going to be logged
        if not log:
            hoomd.context.current.system.removeCompute(self.force_name);

    ## Benchmarks the force computation
    # \param n Number of iterations to average the benchmark over
    #
    # \b Examples:
    # \code
    # t = force.benchmark(n = 100)
    # \endcode
    #
    # The value returned by benchmark() is the average time to perform the force
    # computation, in milliseconds. The benchmark is performed by taking the current
    # positions of all particles in the simulation and repeatedly calculating the forces
    # on them. Thus, you can benchmark different situations as you need to by simply
    # running a simulation to achieve the desired state before running benchmark().
    #
    # \note
    # There is, however, one subtle side effect. If the benchmark() command is run
    # directly after the particle data is initialized with an init command, then the
    # results of the benchmark will not be typical of the time needed during the actual
    # simulation. Particles are not reordered to improve cache performance until at least
    # one time step is performed. Executing run(1) before the benchmark will solve this problem.
    #
    # To use this command, you must have saved the force in a variable, as
    # shown in this example:
    # \code
    # force = pair.some_force()
    # # ... later in the script
    # t = force.benchmark(n = 100)
    # \endcode
    def benchmark(self, n):
        self.check_initialization();

        # run the benchmark
        return self.cpp_force.benchmark(int(n))

    ## Enables the force
    #
    # \b Examples:
    # \code
    # force.enable()
    # \endcode
    #
    # See disable() for a detailed description.
    def enable(self):
        hoomd.util.print_status_line();
        self.check_initialization();

        # check if we are already disabled
        if self.enabled:
            hoomd.context.msg.warning("Ignoring command to enable a force that is already enabled");
            return;

        # add the compute back to the system if it was removed
        if not self.log:
            hoomd.context.current.system.addCompute(self.cpp_force, self.force_name);

        self.enabled = True;
        self.log = True;

    ## \internal
    # \brief updates force coefficients
    def update_coeffs(self):
        pass
        raise RuntimeError("_force.update_coeffs should not be called");
        # does nothing: this is for derived classes to implement

    ## \internal
    # \brief Returns the force data
    #
    def __forces(self):
        return hoomd.data.force_data(self);

    forces = property(__forces);

    ## \internal
    # \brief Get metadata
    def get_metadata(self):
        data = hoomd.meta._metadata.get_metadata(self)
        data['enabled'] = self.enabled
        data['log'] = self.log
        if self.name is not "":
            data['name'] = self.name

        return data

# set default counter
_force.cur_id = 0;


## Constant %force
#
# The command force.constant specifies that a %constant %force should be added to every
# particle in the simulation or optionally to all particles in a group.
#
# \MPI_SUPPORTED
class constant(_force):
    ## Specify the %constant %force
    #
    # \param fx x-component of the %force (in force units)
    # \param fy y-component of the %force (in force units)
    # \param fz z-component of the %force (in force units)
    # \param group Group for which the force will be set
    # \b Examples:
    # \code
    # force.constant(fx=1.0, fy=0.5, fz=0.25)
    # const = force.constant(fx=0.4, fy=1.0, fz=0.5)
    # const = force.constant(fx=0.4, fy=1.0, fz=0.5,group=fluid)
    # \endcode
    def __init__(self, fx, fy, fz, group=None):
        hoomd.util.print_status_line();

        # initialize the base class
        _force.__init__(self);

        # create the c++ mirror class
        if (group is not None):
            self.cpp_force = _hoomd.ConstForceCompute(hoomd.context.current.system_definition, group.cpp_group, fx, fy, fz);
        else:
            self.cpp_force = _hoomd.ConstForceCompute(hoomd.context.current.system_definition, hoomd.context.current.group_all.cpp_group, fx, fy, fz);

        # store metadata
        self.metadata_fields = ['fx','fy','fz']
        self.fx = fx
        self.fy = fy
        self.fz = fz
        if group is not None:
            self.metadata_fields.append('group')
            self.group = group

        hoomd.context.current.system.addCompute(self.cpp_force, self.force_name);

    ## Change the value of the force
    #
    # \param fx New x-component of the %force (in force units)
    # \param fy New y-component of the %force (in force units)
    # \param fz New z-component of the %force (in force units)
    # \param group Group for which the force will be set
    #
    # Using set_force() requires that you saved the created %constant %force in a variable. i.e.
    # \code
    # const = force.constant(fx=0.4, fy=1.0, fz=0.5)
    # \endcode
    #
    # \b Example:
    # \code
    # const.set_force(fx=0.2, fy=0.1, fz=-0.5)
    # const.set_force(fx=0.2, fy=0.1, fz=-0.5, group=fluid)
    # \endcode
    def set_force(self, fx, fy, fz, group=None):
        self.check_initialization();
        if (group is not None):
            self.cpp_force.setGroupForce(group.cpp_group,fx,fy,fz);
        else:
            self.cpp_force.setForce(fx, fy, fz);

        self.fx = fx
        self.fy = fy
        self.fz = fz

    # there are no coeffs to update in the constant force compute
    def update_coeffs(self):
        pass

## Active %force
#
# The command force.active specifies that an %active %force should be added to all particles.
# Obeys \f$\delta {\bf r}_i = \delta t v_0 \hat{p}_i\f$, where \f$ v_0 \f$ is the active velocity. In 2D
# \f$\hat{p}_i = (\cos \theta_i, \sin \theta_i)\f$ is the active force vector for particle \f$i\f$; and the
# diffusion of the active force vector follows \f$\delta \theta / \delta t = \sqrt{2 D_r / \delta t} \Gamma\f$,
# where \f$D_r\f$ is the rotational diffusion constant, and the gamma function is a unit-variance random variable,
# whose components are uncorrelated in time, space, and between particles.
# In 3D, \f$\hat{p}_i\f$ is a unit vector in 3D space, and diffusion follows
# \f$\delta \hat{p}_i / \delta t = \sqrt{2 D_r / \delta t} \Gamma (\hat{p}_i (\cos \theta - 1) + \hat{p}_r \sin \theta)\f$, where
# \f$\hat{p}_r\f$ is an uncorrelated random unit vector. The persistence length of an active particle's path is
# \f$ v_0 / D_r\f$.
#
# NO MPI
class active(_force):
    ## Specify the %active %force
    #
    # \param seed required user-specified seed number for random number generator.
    # \param f_list An array of (x,y,z) tuples for the active force vector for each individual particle.
    # \param group Group for which the force will be set
    # \param orientation_link if True then particle orientation is coupled to the active force vector. Only
    # relevant for non-point-like anisotropic particles.
    # \param rotation_diff rotational diffusion constant, \f$D_r\f$, for all particles in the group.
    # \param constraint specifies a constraint surface, to which particles are confined,
    # such as update.constraint_ellipsoid.
    #
    # \b Examples:
    # \code
    # force.active( seed=13, f_list=[tuple(3,0,0) for i in range(N)])
    #
    # ellipsoid = update.constraint_ellipsoid(group=groupA, P=(0,0,0), rx=3, ry=4, rz=5)
    # force.active( seed=7, f_list=[tuple(1,2,3) for i in range(N)], orientation_link=False, rotation_diff=100, constraint=ellipsoid)
    # \endcode
    def __init__(self, seed, f_lst, group, orientation_link=True, rotation_diff=0, constraint=None):
        hoomd.util.print_status_line();

        # initialize the base class
        _force.__init__(self);

        # input check
        if (f_lst is not None):
            for element in f_lst:
                if type(element) != tuple or len(element) != 3:
                    raise RuntimeError("Active force passed in should be a list of 3-tuples (fx, fy, fz)")

        # assign constraints
        if (constraint is not None):
            if (constraint.__class__.__name__ is "constraint_ellipsoid"):
                P = constraint.P
                rx = constraint.rx
                ry = constraint.ry
                rz = constraint.rz
            else:
                raise RuntimeError("Active force constraint is not accepted (currently only accepts ellipsoids)")
        else:
            P = _hoomd.make_scalar3(0, 0, 0)
            rx = 0
            ry = 0
            rz = 0

        # create the c++ mirror class
        if not hoomd.context.exec_conf.isCUDAEnabled():
            self.cpp_force = _md.ActiveForceCompute(hoomd.context.current.system_definition, group.cpp_group, seed, f_lst,
                                                      orientation_link, rotation_diff, P, rx, ry, rz);
        else:
            self.cpp_force = _md.ActiveForceComputeGPU(hoomd.context.current.system_definition, group.cpp_group, seed, f_lst,
                                                         orientation_link, rotation_diff, P, rx, ry, rz);

        # store metadata
        self.metdata_fields = ['group', 'seed', 'orientation_link', 'rotation_diff', 'constraint']
        self.group = group
        self.seed = seed
        self.orientation_link = orientation_link
        self.rotation_diff = rotation_diff
        self.constraint = constraint

        hoomd.context.current.system.addCompute(self.cpp_force, self.force_name);

    # there are no coeffs to update in the active force compute
    def update_coeffs(self):
        pass

class const_external_field_dipole(_force):
    ## Specicify the %constant %field and %dipole moment
    #
    # \param field_x x-component of the %field (units?)
    # \param field_y y-component of the %field (units?)
    # \param field_z z-component of the %field (units?)
    # \param p magnitude of the particles' dipole moment in z direction
    # \b Examples:
    # \code
    # force.external_field_dipole(field_x=0.0, field_y=1.0 ,field_z=0.5, p=1.0)
    # const_ext_f_dipole = force.external_field_dipole(field_x=0.0, field_y=1.0 ,field_z=0.5, p=1.0)
    # \endcode
    def __init__(self, field_x,field_y,field_z,p):
        hoomd.util.print_status_line()

        # initialize the base class
        _force.__init__(self)

        # create the c++ mirror class
        self.cpp_force = _md.ConstExternalFieldDipoleForceCompute(hoomd.context.current.system_definition, field_x, field_y, field_z, p)

        hoomd.context.current.system.addCompute(self.cpp_force, self.force_name)

        # store metadata
        self.metdata_fields = ['field_x', 'field_y', 'field_z']
        self.field_x = field_x
        self.field_y = field_y
        self.field_z = field_z

    ## Change the %constant %field and %dipole moment
    #
    # \param field_x x-component of the %field (units?)
    # \param field_y y-component of the %field (units?)
    # \param field_z z-component of the %field (units?)
    # \param p magnitude of the particles' dipole moment in z direction
    # \b Examples:
    # \code
    # const_ext_f_dipole = force.external_field_dipole(field_x=0.0, field_y=1.0 ,field_z=0.5, p=1.0)
    # const_ext_f_dipole.setParams(field_x=0.1, field_y=0.1, field_z=0.0, p=1.0))
    # \endcode
    def set_params(field_x, field_y,field_z,p):
        self.check_initialization()

        self.cpp_force.setParams(field_x,field_y,field_z,p)

    # there are no coeffs to update in the constant ExternalFieldDipoleForceCompute
    def update_coeffs(self):
        pass