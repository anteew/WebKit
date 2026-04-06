/*
 * Copyright (C) 2026 Dan Mann. All rights reserved.
 *
 * Shared nutjob capture helper for RemoteLayerTree mirror diagnostics.
 */

#pragma once

#import <QuartzCore/QuartzCore.h>
#import <wtf/RetainPtr.h>
#import <wtf/StdLibExtras.h>

#include <cmath>
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
    NutjobTileNormalization normalization = contentsAreFlipped ? NutjobTileNormalization::VerticalFlip : NutjobTileNormalization::Rotate180;
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

    int normalizedHash = nutjobJavaIntArrayHash(pixels);
    WTFLogAlways("njc: TILE path=%s layer=%llu %dx%d contentsFlipped=%d geomFlipped=%d scale=%g normalization=%s hashBefore=%d hashAfter=%d",
        nutjobTileIngressPathName(ingressPath),
        static_cast<unsigned long long>(layerID),
        width, height,
        contentsAreFlipped, geometryFlipped,
        contentsScale,
        nutjobTileNormalizationName(normalization),
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
