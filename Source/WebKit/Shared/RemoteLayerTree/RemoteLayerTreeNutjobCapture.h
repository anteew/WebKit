/*
 * Copyright (C) 2026 Dan Mann. All rights reserved.
 *
 * Shared nutjob capture helper for RemoteLayerTree mirror diagnostics.
 */

#pragma once

#import <QuartzCore/QuartzCore.h>
#import <wtf/RetainPtr.h>
#import <wtf/StdLibExtras.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>

namespace WebKit {

enum class NutjobTileIngressPath : int {
    Transaction = 1,
    Async = 2,
};

enum class NutjobTileNormalization : int {
    None = 0,
    VerticalFlip = 1,
    Rotate180 = 2,
};

struct NutjobCapturedLayerContents {
    std::unique_ptr<int[]> pixels;
    int width { 0 };
    int height { 0 };
    float contentsScale { 1 };
    bool contentsAreFlipped { false };
    bool geometryFlipped { false };
    NutjobTileNormalization normalization { NutjobTileNormalization::None };
    int sourceHashBeforeNormalization { 0 };
};

static inline const char* nutjobTileIngressPathName(NutjobTileIngressPath path)
{
    switch (path) {
    case NutjobTileIngressPath::Transaction:
        return "transaction";
    case NutjobTileIngressPath::Async:
        return "async";
    }

    return "unknown";
}

static inline const char* nutjobTileNormalizationName(NutjobTileNormalization normalization)
{
    switch (normalization) {
    case NutjobTileNormalization::None:
        return "none";
    case NutjobTileNormalization::VerticalFlip:
        return "vertical_flip";
    case NutjobTileNormalization::Rotate180:
        return "rotate_180";
    }

    return "unknown";
}

static inline int nutjobJavaIntArrayHash(std::span<const int> pixels)
{
    uint32_t hash = 1;
    for (int pixel : pixels)
        hash = (31u * hash) + static_cast<uint32_t>(pixel);
    return static_cast<int32_t>(hash);
}

static inline void nutjobFlipPixelRows(std::span<int> pixels, int width, int height)
{
    for (int row = 0; row < height / 2; ++row) {
        int oppositeRow = height - 1 - row;
        for (int column = 0; column < width; ++column)
            std::swap(pixels[row * width + column], pixels[oppositeRow * width + column]);
    }
}

static inline void nutjobRotatePixels180(std::span<int> pixels)
{
    int count = static_cast<int>(pixels.size());
    for (int i = 0; i < count / 2; ++i)
        std::swap(pixels[i], pixels[count - 1 - i]);
}

static inline bool nutjobTilePolesEnabled()
{
    static bool enabled = [] {
        if (const char* value = getenv("NUTJOB_COMPOSITOR_TILE_POLES"))
            return value[0] == '1';
        return false;
    }();
    return enabled;
}

static inline void nutjobFillTileMarker(std::span<int> pixels, int width, int height, int startX, int startY, int markerWidth, int markerHeight, int color)
{
    for (int row = startY; row < startY + markerHeight; ++row) {
        for (int column = startX; column < startX + markerWidth; ++column)
            pixels[row * width + column] = color;
    }
}

static inline void nutjobStampTilePoles(std::span<int> pixels, int width, int height)
{
    constexpr int red = static_cast<int>(0xFFFF0000u);
    constexpr int green = static_cast<int>(0xFF00FF00u);
    constexpr int blue = static_cast<int>(0xFF0000FFu);
    constexpr int yellow = static_cast<int>(0xFFFFFF00u);

    int markerWidth = std::max(1, std::min(width / 6, 16));
    int markerHeight = std::max(1, std::min(height / 6, 16));

    nutjobFillTileMarker(pixels, width, height, 0, 0, markerWidth, markerHeight, red);
    nutjobFillTileMarker(pixels, width, height, width - markerWidth, 0, markerWidth, markerHeight, green);
    nutjobFillTileMarker(pixels, width, height, 0, height - markerHeight, markerWidth, markerHeight, blue);
    nutjobFillTileMarker(pixels, width, height, width - markerWidth, height - markerHeight, markerWidth, markerHeight, yellow);
}

static inline void nutjobStampRawTilePoles(std::span<int> pixels, int width, int height)
{
    constexpr int magenta = static_cast<int>(0xFFFF00FFu);
    constexpr int cyan = static_cast<int>(0xFF00FFFFu);
    constexpr int orange = static_cast<int>(0xFFFF8800u);
    constexpr int violet = static_cast<int>(0xFF7C3AEDu);

    int canonicalMarkerWidth = std::max(1, std::min(width / 6, 16));
    int canonicalMarkerHeight = std::max(1, std::min(height / 6, 16));
    int markerWidth = std::max(1, std::min(canonicalMarkerWidth, 8));
    int markerHeight = std::max(1, std::min(canonicalMarkerHeight, 8));
    int insetX = std::max(0, std::min(width - markerWidth, canonicalMarkerWidth + 4));
    int insetY = std::max(0, std::min(height - markerHeight, canonicalMarkerHeight + 4));

    nutjobFillTileMarker(pixels, width, height, insetX, insetY, markerWidth, markerHeight, magenta);
    nutjobFillTileMarker(pixels, width, height, std::max(0, width - insetX - markerWidth), insetY, markerWidth, markerHeight, cyan);
    nutjobFillTileMarker(pixels, width, height, insetX, std::max(0, height - insetY - markerHeight), markerWidth, markerHeight, orange);
    nutjobFillTileMarker(pixels, width, height, std::max(0, width - insetX - markerWidth), std::max(0, height - insetY - markerHeight), markerWidth, markerHeight, violet);
}

static inline std::optional<NutjobCapturedLayerContents> captureLayerContentsForNutjob(CALayer *layer, uint64_t layerID, NutjobTileIngressPath ingressPath)
{
    CGRect bounds = [layer bounds];
    float contentsScale = [layer contentsScale];
    int width = static_cast<int>(ceilf(bounds.size.width * contentsScale));
    int height = static_cast<int>(ceilf(bounds.size.height * contentsScale));
    if (width <= 0 || height <= 0 || width > 4096 || height > 4096 || ![layer contents])
        return std::nullopt;

    auto colorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    auto pixelData = std::make_unique<int[]>(width * height);
    auto context = adoptCF(CGBitmapContextCreate(pixelData.get(), width, height, 8, width * 4, colorSpace.get(),
        static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedFirst) | kCGBitmapByteOrder32Little));
    if (!context)
        return std::nullopt;

    CGContextScaleCTM(context.get(), contentsScale, contentsScale);

    RetainPtr savedSublayers = [layer sublayers];
    [layer setSublayers:nil];
    [layer renderInContext:context.get()];
    [layer setSublayers:savedSublayers.get()];

    int pixelCount = width * height;
    auto pixels = unsafeMakeSpan(pixelData.get(), static_cast<size_t>(pixelCount));
    int sourceHashBeforeNormalization = nutjobJavaIntArrayHash(pixels);
    bool contentsAreFlipped = [layer contentsAreFlipped];
    bool geometryFlipped = [layer isGeometryFlipped];
    NutjobTileNormalization normalization = contentsAreFlipped ? NutjobTileNormalization::None : NutjobTileNormalization::Rotate180;
    if (nutjobTilePolesEnabled())
        nutjobStampRawTilePoles(pixels, width, height);
    switch (normalization) {
    case NutjobTileNormalization::None:
        break;
    case NutjobTileNormalization::VerticalFlip:
        nutjobFlipPixelRows(pixels, width, height);
        break;
    case NutjobTileNormalization::Rotate180:
        nutjobRotatePixels180(pixels);
        break;
    }

    if (nutjobTilePolesEnabled())
        nutjobStampTilePoles(pixels, width, height);

    int normalizedHash = nutjobJavaIntArrayHash(pixels);
    WTFLogAlways("njc: TILE path=%s layer=%llu %dx%d contentsFlipped=%d geomFlipped=%d scale=%g normalization=%s poles=%d rawPoles=%d hashBefore=%d hashAfter=%d",
        nutjobTileIngressPathName(ingressPath),
        static_cast<unsigned long long>(layerID),
        width, height,
        contentsAreFlipped, geometryFlipped,
        contentsScale,
        nutjobTileNormalizationName(normalization),
        nutjobTilePolesEnabled(),
        nutjobTilePolesEnabled(),
        sourceHashBeforeNormalization,
        normalizedHash);

    return NutjobCapturedLayerContents {
        std::move(pixelData),
        width,
        height,
        contentsScale,
        contentsAreFlipped,
        geometryFlipped,
        normalization,
        sourceHashBeforeNormalization,
    };
}

} // namespace WebKit
