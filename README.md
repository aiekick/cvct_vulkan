# Cascaded Voxel Cone Tracing in Vulkan
==
A Cascaded Voxel Cone Tracing project written in C++ for my graduation.

The motivation to start this project is mainly for educational purposes; familiarize myself with the new generation of Grahpics API and research an unfamiliar indirect illumination algorithm.

# Status
==
At the moment, the CVCT project is still in a very alpha state. This project is currently in hiatus, as I am prioritizing a __[Vulkan Wrapper](https://github.com/yunyinghu/yvkwrapper)__ project, which I will use to integrate this and future projects with. You can download the first iteration of the project __[here](https://github.com/yunyinghu/CVCT_Vulkan/releases)__.

# Support
==
The algorithm currently supports the following:

* Indirect Diffuse/ Color Bleeding
* Ambient Occlusion
* Specular
* Static and Dynamic Geometry
* Anisotropic Voxels
* Indirect Draw (Vulkan feature)

# Troubles
==
* Flickering during movement
* Revoxelization every frame

# Future work
==
* Sun and Light Support
* First and Second Light Bounce
* Incremental voxelization