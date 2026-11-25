#pragma once

#include <QImage>
#include <QMap>
#include <QString>
#include <QVector>
#include <memory>

#include "SlangCompiler.h"
#include "SlangPreset.h"

// A preset read and compiled, ready for the renderer: its passes with their shaders, the images of its textures and
// the parameters its shaders declare with their values.
struct PresetProgram {
    struct Pass {
        SlangPass config;
        SlangCompiled shaders;
    };
    QString path;
    QVector<Pass> passes;
    QVector<SlangTexture> textures;
    QVector<QImage> images; // one for each texture
    QVector<SlangParameter> parameters; // of all the shaders, each once
    QMap<QString, double> defaults; // what the parameters are when the user has not touched them: the shader's value, then the preset's
    QString error; // empty when the preset is ready
};

// Reads the preset at `path` and compiles its shaders (kept in `cacheDirectory`). Never null; look at `error`.
std::shared_ptr<PresetProgram> loadPresetProgram(const QString& path, const QString& cacheDirectory);
