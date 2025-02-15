# Session notes Lab5 - Render to texture and post-processing

## Session 2024-01-09

**Main takeaways**
**Render targets and frame buffers:**

- Render targets are memory allocations that is represented by frame buffer objects where graphic APIs can write color and depth data. The default frame buffer is what gets displayed on the window. The frame buffer has a color texture color and a depth texture, and it is written to the back buffer for later swapping with the front buffer.
- The textures linked to a frame buffer are called attachments and, at first, are empty.

**Off-screen render target:**

- One can create several render targets that do not necessarily need to be displayed on the screen immediately. This is called an off-screen render target. It is a common technique where the whole 3D scene is rendered into a color texture, and that texture is then sampled to shade the fragments of a full-screen quad.
- Deferred rendering is a technique that uses off-screen render targets to render different information into color texture targets and depth texture, which will be used on the final render to compute the light.

**Post-processing effects**

- These algorithms change the appearance of the pixels that were rendered off-screen and, thus, the look of the game. Each algorithm requires a new shader program because it is a different pipeline that acts on the fragments of the full-screen quad and not on all of the vertices of the meshes in the scene.
- Other algorithms that use this technique are shadow maps, which render the scene from the perspective of the light and write only the depth information, screen-space reflections, and ambient occlusion.


**Rendering 3D Textures**
In OpenGL, texture and access to textures happens in 2D. There is no access to depth for the texture volume. So one needs to set on the C++ slide to what slice of the 3D texture one wants to render to. So, it becomes very expensive. This is why *Compute Shaders* becomes more in-handy.

**Full-screen triangle instead of full-screen quad**
To save some vertices computations, it is more optimal to define a triangle that covers the viewport. Just keep in mind the order, otherwise it will be culled out by it face orientation.