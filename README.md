
# GLdc

This is a partial implementation of OpenGL 1.2 for the SEGA Dreamcast for use
with the KallistiOS SDK.

It began as a fork of libGL by Josh Pearson but has undergone a large refactor
which is essentially a rewrite.

The aim is to implement as much of OpenGL 1.2 as possible, and to add additional
features via extensions.

Things left to (re)implement:

 - Near-Z clipping (Tricky)
 - Spotlights (Trivial)
 - Framebuffer extension (Trivial)
 - Multitexturing (Trivial)
 - Texture Matrix (Trivial)
 - Mipmapping (Trivial)
 
Things I'd like to do:

 - Use a clean "gl.h"
 - Define an extension for modifier volumes
 - Support `GL_ALPHA_TEST` using punch-thru polys
 - Add support for point sprites
 - Optimise, add unit tests for correctness
 
  
