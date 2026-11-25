#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "GpuFilters.h"
#include "PresetProgram.h"
#include "SlangBaker.h"
#include "SlangCompiler.h"
#include "SlangPreset.h"

namespace {

void write(const QString& path, const QString& text)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(text.toUtf8());
}

// A shader of our own in the slang format: a UBO, a block of push constants with two parameters and the two stages.
const char kShader[] = R"(#version 450
layout(push_constant) uniform Push {
    float GAIN;
    float TINT;
} params;
#pragma name Gain
#pragma parameter GAIN "Brightness" 1.0 0.0 2.0 0.05
#pragma parameter TINT "Tint amount" 0.25 0.0 1.0
layout(std140, set = 0, binding = 0) uniform UBO {
    mat4 MVP;
    vec4 OutputSize;
    vec4 OriginalSize;
    vec4 SourceSize;
} global;
#include "common.inc"
#pragma stage vertex
layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 vTexCoord;
void main()
{
    gl_Position = global.MVP * Position;
    vTexCoord = TexCoord;
}
#pragma stage fragment
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 2) uniform sampler2D Source;
void main()
{
    FragColor = vec4(texture(Source, vTexCoord).rgb * params.GAIN * SHARED_FACTOR, 1.0);
}
)";

}

class ShaderTests : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { installSlangBaker(); }

    void aPresetListsItsPassesTexturesAndParameters()
    {
        const SlangPreset preset = SlangPreset::parse(R"(
# a comment
shaders = 2
shader0 = shaders/first.slang
filter_linear0 = false
scale_type0 = source
scale0 = 2.0
alias0 = "Pre"
shader1 = "/abs/second.slang"
filter_linear1 = true
wrap_mode1 = repeat
scale_type_x1 = viewport
scale_x1 = 1.0
scale_type_y1 = absolute
scale_y1 = 480
float_framebuffer1 = true
textures = "grid;noise"
grid = lut/grid.png
grid_linear = false
noise = lut/noise.png
noise_wrap_mode = mirrored_repeat
parameters = "GAIN;MISSING"
GAIN = 1.5
)", "/base/dir");
        QVERIFY2(preset.error.isEmpty(), qPrintable(preset.error));
        QCOMPARE(preset.passes.size(), 2);
        QCOMPARE(preset.passes[0].shader, QString("/base/dir/shaders/first.slang"));
        QVERIFY(preset.passes[0].filterSet && !preset.passes[0].filterLinear);
        QCOMPARE(preset.passes[0].scaleTypeX, SlangPass::Scale::Source);
        QCOMPARE(preset.passes[0].scaleX, 2.0);
        QCOMPARE(preset.passes[0].alias, QString("Pre"));
        QCOMPARE(preset.passes[1].shader, QString("/abs/second.slang"));
        QVERIFY(preset.passes[1].filterLinear && preset.passes[1].floatFramebuffer);
        QCOMPARE(preset.passes[1].wrap, SlangWrap::Repeat);
        QCOMPARE(preset.passes[1].scaleTypeX, SlangPass::Scale::Viewport);
        QCOMPARE(preset.passes[1].scaleTypeY, SlangPass::Scale::Absolute);
        QCOMPARE(preset.passes[1].scaleY, 480.0);
        QCOMPARE(preset.textures.size(), 2);
        QVERIFY(!preset.textures[0].linear);
        QVERIFY(preset.textures[1].linear);
        QCOMPARE(preset.textures[1].wrap, SlangWrap::MirroredRepeat);
        QCOMPARE(preset.textures[1].path, QString("/base/dir/lut/noise.png"));
        QCOMPARE(preset.parameters.size(), 1); // MISSING has no value
        QCOMPARE(preset.parameters.value("GAIN"), 1.5);
    }

    void aPresetThatReferencesAnotherOverridesIt()
    {
        QTemporaryDir dir;
        write(dir.filePath("base/base.slangp"), "shaders = 1\nshader0 = a.slang\nfilter_linear0 = false\nparameters = \"GAIN\"\nGAIN = 1.0\n");
        write(dir.filePath("top.slangp"), "#reference \"base/base.slangp\"\nfilter_linear0 = true\nGAIN = 0.5\n");
        const SlangPreset preset = SlangPreset::load(dir.filePath("top.slangp"));
        QVERIFY2(preset.error.isEmpty(), qPrintable(preset.error));
        QCOMPARE(preset.passes.size(), 1);
        QCOMPARE(QDir::cleanPath(preset.passes[0].shader), QDir::cleanPath(dir.filePath("base/a.slang"))); // relative to the file that names it
        QVERIFY(preset.passes[0].filterLinear);
        QCOMPARE(preset.parameters.value("GAIN"), 0.5);
    }

    void badPresetsSayWhy()
    {
        QVERIFY(!SlangPreset::parse("shader0 = a.slang", "/x").error.isEmpty()); // no count
        QVERIFY(!SlangPreset::parse("shaders = 2\nshader0 = a.slang\n", "/x").error.isEmpty()); // shader1 missing
        QVERIFY(!SlangPreset::load("/no/such/file.slangp").error.isEmpty());
        QTemporaryDir dir;
        write(dir.filePath("loop.slangp"), "#reference \"loop.slangp\"\n");
        QVERIFY(!SlangPreset::load(dir.filePath("loop.slangp")).error.isEmpty());
    }

    void aShaderIsSplitIntoStagesWithItsParametersRead()
    {
        QTemporaryDir dir;
        write(dir.filePath("common.inc"), "#define SHARED_FACTOR 1.0\n");
        write(dir.filePath("gain.slang"), kShader);
        const SlangShader shader = SlangShader::load(dir.filePath("gain.slang"));
        QVERIFY2(shader.error.isEmpty(), qPrintable(shader.error));
        QCOMPARE(shader.name, QString("Gain"));
        QCOMPARE(shader.parameters.size(), 2);
        QCOMPARE(shader.parameters[0].id, QString("GAIN"));
        QCOMPARE(shader.parameters[0].description, QString("Brightness"));
        QCOMPARE(shader.parameters[0].initial, 1.0);
        QCOMPARE(shader.parameters[0].maximum, 2.0);
        QCOMPARE(shader.parameters[0].step, 0.05);
        QCOMPARE(shader.parameters[1].initial, 0.25);
        QVERIFY(shader.vertex.contains("gl_Position"));
        QVERIFY(!shader.vertex.contains("FragColor"));
        QVERIFY(shader.fragment.contains("FragColor"));
        QVERIFY(!shader.fragment.contains("gl_Position"));
        QVERIFY(shader.vertex.contains("SHARED_FACTOR")); // the include is in both stages
        QVERIFY(!shader.vertex.contains("#pragma"));
        QVERIFY(!shader.fragment.contains("push_constant")); // the push constants are a uniform block now...
        QVERIFY(shader.fragment.contains("binding = 3")); // ...after the highest binding the shader uses (2)
        QVERIFY(shader.vertex.contains("binding = 3"));
    }

    void aFileThatIsNotSlangIsRefused()
    {
        QString error;
        SlangShader::parse("#version 450\nvoid main() {}\n", "/x", &error);
        QVERIFY(!error.isEmpty());
        QVERIFY(!SlangShader::parse("#include \"missing.inc\"\n", "/nowhere", &error).error.isEmpty());
    }

    void theBakerCompilesItForEveryBackendAndKeepsTheResult()
    {
        QTemporaryDir dir;
        write(dir.filePath("common.inc"), "#define SHARED_FACTOR 1.0\n");
        write(dir.filePath("gain.slang"), kShader);
        const SlangShader shader = SlangShader::load(dir.filePath("gain.slang"));
        const QString cache = dir.filePath("cache");
        const SlangCompiled first = compileSlang(shader, cache);
        QVERIFY2(first.valid(), qPrintable(first.error));
        QVERIFY(first.fragment.availableShaders().contains(QShaderKey(QShader::HlslShader, QShaderVersion(50))));
        QVERIFY(first.fragment.availableShaders().contains(QShaderKey(QShader::SpirvShader, QShaderVersion(100))));
        // The block of push constants is reflected as a uniform block, with the parameters in it.
        const QShaderDescription description = first.fragment.description();
        QStringList members;
        for (const QShaderDescription::UniformBlock& block : description.uniformBlocks())
            for (const QShaderDescription::BlockVariable& member : block.members)
                members << member.name;
        QVERIFY2(members.contains("GAIN") && members.contains("TINT"), qPrintable(members.join(",")));
        QCOMPARE(QDir(cache).entryList(QStringList{"*.qsbs"}, QDir::Files).size(), 1);
        const SlangCompiled second = compileSlang(shader, cache); // from the cache
        QVERIFY(second.valid());
        QCOMPARE(second.fragment.serialized(), first.fragment.serialized());
    }

    void everyFilterOfTheGpuIsInsideTheProgramAndCompiles()
    {
        const QStringList presets = allGpuFilterPresets();
        QVERIFY(presets.size() >= 12);
        for (const QString& path : presets) {
            const std::shared_ptr<PresetProgram> program = loadPresetProgram(path, QString());
            QVERIFY2(program->error.isEmpty(), qPrintable(path + ": " + program->error));
            QVERIFY2(program->passes.size() >= 2, qPrintable(path));
        }
        // What only the CPU does has no preset.
        for (const VideoFilter filter : {VideoFilter::None, VideoFilter::NTSCComposite, VideoFilter::NTSCRF, VideoFilter::NTSCSVideo, VideoFilter::NTSCRGB,
                                         VideoFilter::TwoXSaI, VideoFilter::Super2xSaI, VideoFilter::SuperEagle, VideoFilter::Scanlines})
            QVERIFY(gpuFilterPreset(filter).isEmpty());
        QVERIFY(gpuFilterPreset(VideoFilter::XBRZ6x).endsWith("xbrz6x.slangp"));
        QVERIFY(gpuFilterPreset(VideoFilter::HQ4x).endsWith("hq4x.slangp"));
    }

    void aShaderThatDoesNotCompileReportsTheError()
    {
        SlangShader broken = SlangShader::parse("#version 450\n#pragma stage vertex\nvoid main() { oops }\n#pragma stage fragment\nvoid main() {}\n", "/x");
        QVERIFY(broken.error.isEmpty());
        const SlangCompiled result = compileSlang(broken, QString());
        QVERIFY(!result.valid());
        QVERIFY(!result.error.isEmpty());
    }
};

QTEST_MAIN(ShaderTests)
#include "ShaderTests.moc"
