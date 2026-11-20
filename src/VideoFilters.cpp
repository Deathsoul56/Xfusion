#include "VideoFilters.h"

#include <algorithm>
#include <cstdint>
#include <memory>

#include "hqx/hq2x.h"
// The library mixes two anonymous enums in a constant; it is third-party code and is left as it is.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-anon-enum-enum-conversion"
#include "snes_ntsc/snes_ntsc.h"
#pragma clang diagnostic pop
#include "xbrz/xbrz.h"

namespace {

using Pixel = std::uint32_t;

// Filters work on 0xAARRGGBB pixels; QImage::Format_RGB32 is that with an unused alpha of 255.
QImage toRgb32(const QImage& frame)
{
    return frame.format() == QImage::Format_RGB32 ? frame : frame.convertToFormat(QImage::Format_RGB32);
}

inline Pixel half(Pixel a, Pixel b)
{
    return (((a & 0xFEFEFEFEu) >> 1) + ((b & 0xFEFEFEFEu) >> 1) + (a & b & 0x01010101u)) | 0xFF000000u;
}

// Average of four pixels, channel by channel.
inline Pixel quarter(Pixel a, Pixel b, Pixel c, Pixel d)
{
    const Pixel hi = ((a & 0xFCFCFCFCu) >> 2) + ((b & 0xFCFCFCFCu) >> 2) + ((c & 0xFCFCFCFCu) >> 2) + ((d & 0xFCFCFCFCu) >> 2);
    const Pixel lo = (((a & 0x03030303u) + (b & 0x03030303u) + (c & 0x03030303u) + (d & 0x03030303u) + 0x02020202u) >> 2) & 0x03030303u;
    return (hi + lo) | 0xFF000000u;
}

// Three parts of `a` and one of `b`.
inline Pixel weighted31(Pixel a, Pixel b) { return half(a, half(a, b)); }

// A read-only view of the frame that repeats the edge pixels outside it.
class Source {
public:
    explicit Source(const QImage& image) : m_image(image) {}
    int width() const { return m_image.width(); }
    int height() const { return m_image.height(); }
    Pixel at(int x, int y) const
    {
        x = std::clamp(x, 0, m_image.width() - 1);
        y = std::clamp(y, 0, m_image.height() - 1);
        return reinterpret_cast<const Pixel*>(m_image.constScanLine(y))[x];
    }

private:
    const QImage& m_image;
};

QImage scanlines(const QImage& frame)
{
    const QImage source = toRgb32(frame);
    QImage out(source.width(), source.height() * 2, QImage::Format_RGB32);
    for (int y = 0; y < source.height(); ++y) {
        const auto* line = reinterpret_cast<const Pixel*>(source.constScanLine(y));
        auto* bright = reinterpret_cast<Pixel*>(out.scanLine(y * 2));
        auto* dark = reinterpret_cast<Pixel*>(out.scanLine(y * 2 + 1));
        for (int x = 0; x < source.width(); ++x) {
            bright[x] = line[x];
            dark[x] = ((line[x] >> 1) & 0x007F7F7Fu) | 0xFF000000u; // half of every channel
        }
    }
    return out;
}

QImage simple(const QImage& frame, int factor)
{
    return frame.scaled(frame.size() * factor, Qt::IgnoreAspectRatio, Qt::FastTransformation);
}

// EPX: a pixel becomes four, and a corner takes the colour of its two neighbours when they agree with each other.
QImage epx(const QImage& frame)
{
    const QImage rgb = toRgb32(frame);
    const Source src(rgb);
    QImage out(rgb.width() * 2, rgb.height() * 2, QImage::Format_RGB32);
    for (int y = 0; y < rgb.height(); ++y) {
        auto* top = reinterpret_cast<Pixel*>(out.scanLine(y * 2));
        auto* bottom = reinterpret_cast<Pixel*>(out.scanLine(y * 2 + 1));
        for (int x = 0; x < rgb.width(); ++x) {
            const Pixel p = src.at(x, y), a = src.at(x, y - 1), b = src.at(x + 1, y), c = src.at(x - 1, y), d = src.at(x, y + 1);
            top[x * 2] = (c == a && c != d && a != b) ? a : p;
            top[x * 2 + 1] = (a == b && a != c && b != d) ? b : p;
            bottom[x * 2] = (d == c && d != b && c != a) ? c : p;
            bottom[x * 2 + 1] = (b == d && b != a && d != c) ? d : p;
        }
    }
    return out;
}

// Eagle: a corner copies a diagonal neighbour when the three pixels around that corner agree. The "super" form also
// softens the other corners that sit next to two equal neighbours.
QImage superEagle(const QImage& frame)
{
    const QImage rgb = toRgb32(frame);
    const Source src(rgb);
    QImage out(rgb.width() * 2, rgb.height() * 2, QImage::Format_RGB32);
    const auto corner = [](Pixel s, Pixel diagonal, Pixel one, Pixel two) -> Pixel {
        if (one == two && diagonal == one)
            return diagonal;
        if (one == two)
            return half(s, one);
        return s;
    };
    for (int y = 0; y < rgb.height(); ++y) {
        auto* top = reinterpret_cast<Pixel*>(out.scanLine(y * 2));
        auto* bottom = reinterpret_cast<Pixel*>(out.scanLine(y * 2 + 1));
        for (int x = 0; x < rgb.width(); ++x) {
            const Pixel s = src.at(x, y);
            const Pixel tl = src.at(x - 1, y - 1), t = src.at(x, y - 1), tr = src.at(x + 1, y - 1);
            const Pixel l = src.at(x - 1, y), r = src.at(x + 1, y);
            const Pixel bl = src.at(x - 1, y + 1), b = src.at(x, y + 1), br = src.at(x + 1, y + 1);
            top[x * 2] = corner(s, tl, t, l);
            top[x * 2 + 1] = corner(s, tr, t, r);
            bottom[x * 2] = corner(s, bl, b, l);
            bottom[x * 2 + 1] = corner(s, br, b, r);
        }
    }
    return out;
}

int saiResult(Pixel a, Pixel b, Pixel c, Pixel d)
{
    int x = 0, y = 0;
    if (a == c) ++x; else if (b == c) ++y;
    if (a == d) ++x; else if (b == d) ++y;
    return (x <= 1 ? 1 : 0) - (y <= 1 ? 1 : 0);
}

// 2xSaI by its decision rules: look at the 4x4 block around the pixel and choose between the two diagonals. With
// `super` the blends lean on the pixel itself (3 to 1) instead of splitting evenly, which keeps edges tighter.
QImage saiScale(const QImage& frame, bool super)
{
    const QImage rgb = toRgb32(frame);
    const Source src(rgb);
    QImage out(rgb.width() * 2, rgb.height() * 2, QImage::Format_RGB32);
    const auto mix = [super](Pixel a, Pixel b) { return super ? weighted31(a, b) : half(a, b); };
    for (int y = 0; y < rgb.height(); ++y) {
        auto* top = reinterpret_cast<Pixel*>(out.scanLine(y * 2));
        auto* bottom = reinterpret_cast<Pixel*>(out.scanLine(y * 2 + 1));
        for (int x = 0; x < rgb.width(); ++x) {
            const Pixel I = src.at(x - 1, y - 1), E = src.at(x, y - 1), F = src.at(x + 1, y - 1), J = src.at(x + 2, y - 1);
            const Pixel G = src.at(x - 1, y), A = src.at(x, y), B = src.at(x + 1, y), K = src.at(x + 2, y);
            const Pixel H = src.at(x - 1, y + 1), C = src.at(x, y + 1), D = src.at(x + 1, y + 1), L = src.at(x + 2, y + 1);
            const Pixel M = src.at(x - 1, y + 2), N = src.at(x, y + 2), O = src.at(x + 1, y + 2);
            Pixel right, below, diagonal;
            if (A == D && B != C) {
                right = ((A == E && B == L) || (A == C && A == F && B != E && B == J)) ? A : mix(A, B);
                below = ((A == G && C == O) || (A == B && A == H && G != C && C == M)) ? A : mix(A, C);
                diagonal = A;
            } else if (B == C && A != D) {
                right = ((B == F && A == H) || (B == E && B == D && A != F && A == I)) ? B : mix(A, B);
                below = ((C == H && A == F) || (C == G && C == D && A != H && A == I)) ? C : mix(A, C);
                diagonal = B;
            } else if (A == D && B == C) {
                if (A == B) {
                    right = below = diagonal = A;
                } else {
                    const int r = saiResult(A, B, G, E) + saiResult(B, A, K, F) + saiResult(B, A, H, N) + saiResult(A, B, L, O);
                    diagonal = r > 0 ? A : r < 0 ? B : quarter(A, B, C, D);
                    right = mix(A, B);
                    below = mix(A, C);
                }
            } else {
                if (A == C && A == F && B != E && B == J)
                    right = A;
                else if (B == E && B == D && A != F && A == I)
                    right = B;
                else
                    right = mix(A, B);
                if (A == B && A == H && G != C && C == M)
                    below = A;
                else if (C == G && C == D && A != H && A == I)
                    below = C;
                else
                    below = mix(A, C);
                diagonal = quarter(A, B, C, D);
            }
            top[x * 2] = A;
            top[x * 2 + 1] = right;
            bottom[x * 2] = below;
            bottom[x * 2 + 1] = diagonal;
        }
    }
    return out;
}

QImage scaleXbrz(const QImage& frame, int factor)
{
    const QImage rgb = toRgb32(frame);
    QImage out(rgb.width() * factor, rgb.height() * factor, QImage::Format_RGB32);
    // xBRZ wants tightly packed rows; QImage rows of a 32-bit image are, so the buffers can be handed over as they are.
    xbrz::scale(static_cast<std::size_t>(factor), reinterpret_cast<const std::uint32_t*>(rgb.constBits()),
                reinterpret_cast<std::uint32_t*>(out.bits()), rgb.width(), rgb.height(), xbrz::ColorFormat::RGB);
    return out;
}

QImage hqx(const QImage& frame, int factor)
{
    QImage in = frame.convertToFormat(QImage::Format_RGB16);
    QImage out(in.width() * factor, in.height() * factor, QImage::Format_RGB16);
    auto* source = const_cast<std::uint8_t*>(in.constBits());
    const int sourcePitch = static_cast<int>(in.bytesPerLine());
    const int destinationPitch = static_cast<int>(out.bytesPerLine());
    switch (factor) {
    case 2: HQ2X_16(source, sourcePitch, out.bits(), destinationPitch, in.width(), in.height()); break;
    case 3: HQ3X_16(source, sourcePitch, out.bits(), destinationPitch, in.width(), in.height()); break;
    default: HQ4X_16(source, sourcePitch, out.bits(), destinationPitch, in.width(), in.height()); break;
    }
    return out;
}

QImage ntsc(const QImage& frame, VideoFilter filter)
{
    static std::unique_ptr<snes_ntsc_t> table;
    static VideoFilter preset = VideoFilter::None;
    static int burstPhase = 0;
    if (!table || preset != filter) {
        if (!table)
            table = std::make_unique<snes_ntsc_t>();
        snes_ntsc_setup_t setup = snes_ntsc_composite;
        switch (filter) {
        case VideoFilter::NTSCComposite: setup.merge_fields = 1; break;
        case VideoFilter::NTSCRF: setup.merge_fields = 0; break;
        case VideoFilter::NTSCSVideo: setup = snes_ntsc_svideo; setup.merge_fields = 1; break;
        default: setup = snes_ntsc_rgb; setup.merge_fields = 1; break;
        }
        snes_ntsc_init(table.get(), &setup);
        preset = filter;
    }
    const QImage in = frame.convertToFormat(QImage::Format_RGB16);
    QImage out(SNES_NTSC_OUT_WIDTH(in.width()), in.height(), QImage::Format_RGB16);
    snes_ntsc_blit(table.get(), reinterpret_cast<const SNES_NTSC_IN_T*>(in.constBits()), static_cast<int>(in.bytesPerLine() / 2), burstPhase,
                   in.width(), in.height(), out.bits(), static_cast<int>(out.bytesPerLine()));
    burstPhase = (burstPhase + 1) % 3;
    return out;
}

}

const std::vector<VideoFilterInfo>& allVideoFilters()
{
    static const std::vector<VideoFilterInfo> filters = {
        {VideoFilter::None, "none", "None"},
        {VideoFilter::Scanlines, "scanlines", "Scanlines"},
        {VideoFilter::Simple2x, "simple2x", "Simple 2x"},
        {VideoFilter::Simple3x, "simple3x", "Simple 3x"},
        {VideoFilter::Simple4x, "simple4x", "Simple 4x"},
        {VideoFilter::SuperEagle, "super_eagle", "Super Eagle"},
        {VideoFilter::TwoXSaI, "2xsai", "2xSaI"},
        {VideoFilter::Super2xSaI, "super_2xsai", "Super 2xSaI"},
        {VideoFilter::EPX, "epx", "EPX"},
        {VideoFilter::HQ2x, "hq2x", "HQ2x"},
        {VideoFilter::HQ3x, "hq3x", "HQ3x"},
        {VideoFilter::HQ4x, "hq4x", "HQ4x"},
        {VideoFilter::XBRZ2x, "2xbrz", "2xBRZ"},
        {VideoFilter::XBRZ3x, "3xbrz", "3xBRZ"},
        {VideoFilter::XBRZ4x, "4xbrz", "4xBRZ"},
        {VideoFilter::XBRZ5x, "5xbrz", "5xBRZ"},
        {VideoFilter::XBRZ6x, "6xbrz", "6xBRZ"},
        {VideoFilter::SharpBilinear4x, "sharp_bilinear", "SharpBilinear4x"},
        {VideoFilter::NTSCComposite, "ntsc_composite", "NTSC (Composite)"},
        {VideoFilter::NTSCRF, "ntsc_rf", "NTSC (RF)"},
        {VideoFilter::NTSCSVideo, "ntsc_svideo", "NTSC (S-Video)"},
        {VideoFilter::NTSCRGB, "ntsc_rgb", "NTSC (RGB)"},
    };
    return filters;
}

QString videoFilterKey(VideoFilter filter)
{
    for (const VideoFilterInfo& info : allVideoFilters())
        if (info.id == filter)
            return info.key;
    return "none";
}

VideoFilter videoFilterFromKey(const QString& key, VideoFilter fallback)
{
    for (const VideoFilterInfo& info : allVideoFilters())
        if (key == info.key)
            return info.id;
    return fallback;
}

QImage applyVideoFilter(const QImage& frame, VideoFilter filter)
{
    if (frame.isNull())
        return frame;
    switch (filter) {
    case VideoFilter::None: return frame;
    case VideoFilter::Scanlines: return scanlines(frame);
    case VideoFilter::Simple2x: return simple(frame, 2);
    case VideoFilter::Simple3x: return simple(frame, 3);
    case VideoFilter::Simple4x: return simple(frame, 4);
    case VideoFilter::SuperEagle: return superEagle(frame);
    case VideoFilter::TwoXSaI: return saiScale(frame, false);
    case VideoFilter::Super2xSaI: return saiScale(frame, true);
    case VideoFilter::EPX: return epx(frame);
    case VideoFilter::HQ2x: return hqx(frame, 2);
    case VideoFilter::HQ3x: return hqx(frame, 3);
    case VideoFilter::HQ4x: return hqx(frame, 4);
    case VideoFilter::XBRZ2x: return scaleXbrz(frame, 2);
    case VideoFilter::XBRZ3x: return scaleXbrz(frame, 3);
    case VideoFilter::XBRZ4x: return scaleXbrz(frame, 4);
    case VideoFilter::XBRZ5x: return scaleXbrz(frame, 5);
    case VideoFilter::XBRZ6x: return scaleXbrz(frame, 6);
    case VideoFilter::SharpBilinear4x: return simple(frame, 4);
    case VideoFilter::NTSCComposite:
    case VideoFilter::NTSCRF:
    case VideoFilter::NTSCSVideo:
    case VideoFilter::NTSCRGB: return ntsc(frame, filter);
    }
    return frame;
}
