#pragma once

#include "SlangCompiler.h"

// The baker of Qt Shader Tools, for the programs that carry it (xfusion-shaderc and the tests). Direct3D and Vulkan always;
// OpenGL with the oldest GLSL that can say what the shader says.
SlangCompiled bakeSlang(const SlangShader& shader);

// Makes compileSlang use bakeSlang.
void installSlangBaker();
