Hello Vulkan
============

Welcome to a Vulkan&trade; "Hello Triangle" sample which shows how to set up a window, set up a Vulkan context, and render a triangle. This sample is designed to help you get started with Vulkan and does not use any wrappers or helper libraries - pure Vulkan all the way.

License
-------

MIT: see `LICENSE.txt` for details.

System requirements
-------------------

* AMD Radeon&trade; GCN-based GPU (HD 7000 series or newer)
  * Or other Vulkan&trade; compatible discrete GPU 
* 64-bit Windows&reg; 7 (SP1 with the [Platform Update](https://msdn.microsoft.com/en-us/library/windows/desktop/jj863687.aspx)), Windows&reg; 8.1, or Windows&reg; 10
* Visual Studio&reg; 2013 or Visual Studio&reg; 2015
* Graphics driver with Vulkan support
  * For example, AMD Radeon Software Crimson Edition 16.5.2 or later
* The [Vulkan SDK](https://vulkan.lunarg.com) must be installed

Building
--------

Visual Studio files can be found in the `hellovulkan\build` directory.

If you need to regenerate the Visual Studio files, open a command prompt in the `hellovulkan\premake` directory and run `..\..\premake\premake5.exe vs2015` (or `..\..\premake\premake5.exe vs2013` for Visual Studio 2013.)

Vulkan also supports Linux&reg;, of course, and Premake can generate GNU Makefiles. However, at this time, the sample itself is Windows specific (because the helper code in Window.h/.cpp is Windows specific).

Third-party software
------------------

* Premake is distributed under the terms of the BSD License. See `premake\LICENSE.txt`.

Known issues
------------

The sample uses `PRESENT_MODE_FIFO` to run with VSync. On the NVIDIA 365.19 driver, FIFO is ignored and the sample will exit after a very brief period of time. You can increase the number of frames rendered to ensure it remains visible for a couple of seconds.

Attribution
-----------

* AMD, the AMD Arrow logo, Radeon, and combinations thereof are either registered trademarks or trademarks of Advanced Micro Devices, Inc. in the United States and/or other countries.
* Microsoft, Visual Studio, and Windows are either registered trademarks or trademarks of Microsoft Corporation in the United States and/or other countries.
* Linux is the registered trademark of Linus Torvalds in the U.S. and other countries.
* Vulkan and the Vulkan logo are trademarks of the Khronos Group, Inc.
