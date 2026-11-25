#pragma once

#include <QByteArray>
#include <QString>
#include <functional>
#include <rhi/qshader.h>

#include "SlangPreset.h"

// A .slang shader turned into what the graphics layer takes for every backend (SPIR-V, HLSL, GLSL). The compiler is the
// baker of Qt Shader Tools, which is large (about 11 MB with glslang and SPIRV-Cross), so the program does not carry it:
//   - the shaders of the filters that come with the program are compiled when it is built and kept inside it;
//   - the shaders of the presets of the user are compiled by xfusion-shaderc, a small program next to this one, and the
//     result is kept on disk by the hash of the source, so that a preset is compiled once;
//   - the tools that do carry the baker (xfusion-shaderc itself, the tests) install it with setSlangBaker.
struct SlangCompiled {
    QShader vertex;
    QShader fragment;
    QString error; // the message of the compiler when it failed

    bool valid() const { return error.isEmpty() && vertex.isValid() && fragment.isValid(); }
};

// The same for every build of Qt and of the format: the name of a shader in the baked table and in the cache.
QString slangCacheKey(const SlangShader& shader);

// The two stages as one block of bytes (what is kept on disk and inside the program), and back.
QByteArray serializeCompiled(const SlangCompiled& compiled);
SlangCompiled deserializeCompiled(const QByteArray& bytes);

// The shaders compiled ahead of time, kept inside the program as the resource :/baked/shaders.bin: a QMap<QString, QByteArray>
// written with QDataStream by `xfusion-shaderc bake` (see tools/shaderc.cpp), the key being slangCacheKey, the value serializeCompiled.
constexpr char kBakedShadersResource[] = ":/baked/shaders.bin";

// The compiler itself, for the programs that carry it.
using SlangBaker = std::function<SlangCompiled(const SlangShader&)>;
void setSlangBaker(SlangBaker baker);

// Looks for the shader among the baked ones, then in `cacheDirectory` (which may be empty: nothing is kept), and compiles it
// when it is in neither: with the installed baker, or else with xfusion-shaderc.
SlangCompiled compileSlang(const SlangShader& shader, const QString& cacheDirectory);
