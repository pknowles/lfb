LFB
===

The *layered fragment buffer* (LFB) demo framework. An updated library from the original we
introduced in the [OpenGL Insights: *Efficient Layered Fragment Buffer Techniques*](http://openglinsights.com/bendingthepipeline.html#EfficientLayeredFragmentBufferTechniques) book chapter.

Includes factory style deep image construction, storage and access implementations, and attempts to abstract the LFB implementation in both C++ and GLSL.

This project uses [pyarlib](https://github.com/pknowles/pyarlib/) for shader and GL buffer management.

The most straight forward application is *order independent transparency*, for which a [demo application](https://github.com/pknowles/oit) is also provided.
The techniques included there are from [*Fast Sorting for Exact OIT of Complex Scenes*](http://heuristic42.com/research/).


LICENSE
=======

This library is distributed under the terms of the GNU LGPL license.
See `LICENSE.txt` for details.
