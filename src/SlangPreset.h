#pragma once

#include <QMap>
#include <QString>
#include <QVector>

// Shader presets in the format of RetroArch ("slang", .slangp) and the shaders they list (.slang), read here without
// compiling anything: the files are parsed and the source is made ready for the shader baker (ADR-0032, phase B).
// Written from the public description of the format.

enum class SlangWrap { ClampToBorder, ClampToEdge, Repeat, MirroredRepeat };

struct SlangParameter {
    QString id;
    QString description;
    double initial = 0;
    double minimum = 0;
    double maximum = 1;
    double step = 0.01;
    bool operator==(const SlangParameter& other) const = default;
};

struct SlangPass {
    QString shader; // absolute path of the .slang file
    bool filterSet = false;
    bool filterLinear = false;
    SlangWrap wrap = SlangWrap::ClampToBorder;
    bool mipmap = false;
    QString alias;
    bool floatFramebuffer = false;
    bool srgbFramebuffer = false;
    enum class Scale { Source, Viewport, Absolute };
    bool scaleTypeSet = false;
    Scale scaleTypeX = Scale::Source;
    Scale scaleTypeY = Scale::Source;
    double scaleX = 1;
    double scaleY = 1;
    int frameCountMod = 0;
};

struct SlangTexture {
    QString name;
    QString path; // absolute
    bool linear = true;
    SlangWrap wrap = SlangWrap::ClampToBorder;
    bool mipmap = false;
};

struct SlangPreset {
    QVector<SlangPass> passes;
    QVector<SlangTexture> textures;
    QMap<QString, double> parameters; // values the preset gives to the parameters of its shaders
    QString error; // empty when the preset was read

    // Reads a .slangp file, following `#reference` lines. Paths in it are relative to the file that names them.
    static SlangPreset load(const QString& path);
    // The same for the text of a preset, with `directory` as the base of relative paths.
    static SlangPreset parse(const QString& text, const QString& directory, int depth = 0);
};

// One .slang file ready for the baker: `#include`s resolved, the two stages split, the parameter pragmas read and
// removed, and the block of push constants (which the graphics layer here has no equivalent of) turned into a uniform block.
struct SlangShader {
    QString vertex;
    QString fragment;
    QString name; // from `#pragma name`, if any
    QVector<SlangParameter> parameters;
    QString error; // empty when the shader was read

    static SlangShader load(const QString& path);
    static SlangShader parse(const QString& text, const QString& directory, QString* error = nullptr);
};
