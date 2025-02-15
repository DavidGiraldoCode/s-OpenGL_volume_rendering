# Sessions - Render Doc

```C++
// Opening a context with {}, just for readbility
//
{
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 2, -1, "COMPOSITE");

    glObjectLabel(GL_TEXTURE, depthBuffer, -1, "depth_buffer_target");
    glObjectLabel(GL_FRAMEBUFFER, framebufferId, -1, "off_screen_framebuffer");
    glObjectLabel(GL_PROGRAM, backgroundProgram, -1, "BackgroundProgram");

    glPopDebugGroup();
}
//
//
```
For this project to work as an executable with RenderDoc, the `SDL2.dll` and `glew32.dll` file should be included in the binaries `/bin` directory.