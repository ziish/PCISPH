PCISPH - Predictive-Corrective Incompressible SPH
=================================================

This is an fluid simulation which is GPU accelerated using OpenCL. You can find some results on [youtube](https://www.youtube.com/watch?v=Na2XxPhtyb8) or with better quality as [download](https://dl.dropboxusercontent.com/u/30655292/pcisph.mkv).

The two main papers implemented are:

["Predictive-Corrective Incompressible SPH"](https://sph-sjtu-f06.googlecode.com/files/a40-solenthaler.pdf)

["Versatile Surface Tension and Adhesion for SPH Fluids"](http://cg.informatik.uni-freiburg.de/publications/siggraphasia2013/2013_SIGGRAPHASIA_surface_tension_adhesion.pdf) (adhesion is not implemented!)

All the code was created for an university project during one semester and comes as it is. It is published because we
found it quite hard to get information about the used parameters and implementation details of other similar
projects. The code was only tested on windows (and actually comes with as visual studio solution) but my expectation would be that it should not be that hard to run it on *nix systems. Before you can execute the simulation you have to run the "exec\_clogs\_tune.bat" file which does some performance tuning of the [clogs library](http://sourceforge.net/p/clogs) which is used by the project.
Don't expect too much though - There was a deadline - You know what that means ;)

The code does not come with any restrictions but it would be interesting to know if someone found it helpful or is using it to actually do something meaningful.
