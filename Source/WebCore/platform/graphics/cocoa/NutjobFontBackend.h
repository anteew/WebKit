#pragma once

#if USE(CORE_TEXT) && PLATFORM(MAC)

#include <WebCore/FontPlatformData.h>
#include <WebCore/SharedBuffer.h>
#include <CoreText/CoreText.h>
#include <wtf/Assertions.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <mutex>

#include "libnutjobfont.h"

namespace WebCore::NutjobFontBackend {

enum class SourceKind : uint8_t {
    Exact,
    FamilyFallback,
};

struct Runtime {
    std::once_flag initializeOnce;
    graal_isolate_t* isolate { nullptr };
    bool failed { false };
};

inline Runtime& backendRuntime()
{
    static NeverDestroyed<Runtime> runtime;
    return runtime.get();
}

struct ThreadHandleGuard {
    graal_isolatethread_t* handle { nullptr };

    ~ThreadHandleGuard()
    {
        if (handle)
            graal_detach_thread(handle);
    }
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
inline ThreadHandleGuard& currentThreadHandleGuard()
{
    static thread_local ThreadHandleGuard guard;
    return guard;
}
#pragma clang diagnostic pop

inline graal_isolatethread_t* threadHandle();

class FontHandle final : public RefCounted<FontHandle> {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(FontHandle);
    WTF_MAKE_NONCOPYABLE(FontHandle);
public:
    static Ref<FontHandle> create(long long value, SourceKind sourceKind)
    {
        return adoptRef(*new FontHandle(value, sourceKind));
    }

    ~FontHandle()
    {
        if (!m_value)
            return;

        if (auto* thread = threadHandle())
            nutjob_font_close(thread, m_value);
    }

    long long value() const { return m_value; }
    SourceKind sourceKind() const { return m_sourceKind; }

private:
    FontHandle(long long value, SourceKind sourceKind)
        : m_value(value)
        , m_sourceKind(sourceKind)
    {
    }

    long long m_value { 0 };
    SourceKind m_sourceKind { SourceKind::Exact };
};

inline bool ensureRuntime()
{
    auto& runtime = backendRuntime();
    std::call_once(runtime.initializeOnce, [&] {
        graal_isolatethread_t* initialThread = nullptr;
        if (graal_create_isolate(nullptr, &runtime.isolate, &initialThread) == 0 && runtime.isolate) {
            currentThreadHandleGuard().handle = initialThread;
            return;
        }
        runtime.failed = true;
        runtime.isolate = nullptr;
    });
    return runtime.isolate && !runtime.failed;
}

inline graal_isolatethread_t* threadHandle()
{
    auto& guard = currentThreadHandleGuard();
    if (guard.handle)
        return guard.handle;
    if (!ensureRuntime())
        return nullptr;
    if (guard.handle)
        return guard.handle;

    auto& runtime = backendRuntime();
    if (!runtime.isolate)
        return nullptr;
    if (graal_attach_thread(runtime.isolate, &guard.handle) != 0)
        return nullptr;
    return guard.handle;
}

inline String nutjobRoot()
{
    if (const char* explicitRoot = getenv("NUTJOB_ROOT"))
        return String::fromUTF8(explicitRoot);
    if (const char* home = getenv("HOME"))
        return makeString(String::fromUTF8(home), "/tools/nutjob"_s);
    return "/Users/danmann/tools/nutjob"_s;
}

inline String bundledFontPathForFamilyName(const String& familyName)
{
    auto lowered = familyName.convertToASCIILowercase();
    auto root = nutjobRoot();
    if (lowered.contains("courier"_s) || lowered.contains("consolas"_s) || lowered.contains("mono"_s))
        return makeString(root, "/fonts/NotoSansMono-Regular.ttf"_s);
    if (lowered.contains("times"_s) || lowered.contains("georgia"_s) || lowered.contains("garamond"_s) || lowered.contains("palatino"_s) || lowered.contains("serif"_s))
        return makeString(root, "/fonts/NotoSerif-Regular.ttf"_s);
    if (lowered.contains("helvetica"_s) || lowered.contains("arial"_s) || lowered.contains("sans"_s) || lowered.contains("system"_s))
        return makeString(root, "/fonts/NotoSans-Regular.ttf"_s);
    return { };
}

inline String fontFilePath(CTFontRef ctFont)
{
    if (!ctFont)
        return { };

    auto copyURLAttribute = [&](CFStringRef attribute) -> RetainPtr<CFURLRef> {
        RetainPtr<CFTypeRef> value = adoptCF(CTFontCopyAttribute(ctFont, attribute));
        if (!value || CFGetTypeID(value.get()) != CFURLGetTypeID())
            return nullptr;
        return adoptCF(static_cast<CFURLRef>(value.leakRef()));
    };

    RetainPtr<CFURLRef> url = copyURLAttribute(kCTFontURLAttribute);
    if (!url)
        url = copyURLAttribute(kCTFontReferenceURLAttribute);
    if (!url)
        return { };

    if (RetainPtr<CFStringRef> path = adoptCF(CFURLCopyFileSystemPath(url.get(), kCFURLPOSIXPathStyle)))
        return path.get();
    return { };
}

inline RefPtr<FontHandle> loadFontFromBytes(const SharedBuffer& buffer, const String& name)
{
    auto* thread = threadHandle();
    if (!thread)
        return nullptr;

    auto bytes = buffer.span();
    if (bytes.empty() || bytes.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
        return nullptr;

    CString utf8Name = name.utf8();
    long long value = nutjob_font_load_bytes(thread, const_cast<char*>(utf8Name.data()), reinterpret_cast<char*>(const_cast<uint8_t*>(bytes.data())), static_cast<int>(bytes.size()));
    if (value < 0)
        return nullptr;
    return FontHandle::create(value, SourceKind::Exact);
}

inline RefPtr<FontHandle> loadFontFromPath(const String& path, SourceKind sourceKind)
{
    auto* thread = threadHandle();
    if (path.isEmpty() || !thread)
        return nullptr;

    CString utf8Path = path.utf8();
    long long value = nutjob_font_load(thread, const_cast<char*>(utf8Path.data()));
    if (value < 0)
        return nullptr;
    int unitsPerEm = nutjob_font_units_per_em(thread, value);
    float ascent = nutjob_font_ascent(thread, value, 16.0f);
    float descent = nutjob_font_descent(thread, value, 16.0f);
    float lineHeight = nutjob_font_line_height(thread, value, 16.0f);
    if (unitsPerEm <= 0 || !std::isfinite(ascent) || !std::isfinite(descent) || !std::isfinite(lineHeight) || lineHeight <= 0) {
        nutjob_font_close(thread, value);
        return nullptr;
    }

    return FontHandle::create(value, sourceKind);
}

inline RefPtr<FontHandle> createFontHandle(CTFontRef ctFont, const FontPlatformData::CreationData* creationData)
{
    if (!ctFont)
        return nullptr;

    if (creationData) {
        auto name = creationData->itemInCollection;
        if (name.isEmpty()) {
            if (RetainPtr<CFStringRef> postScriptName = adoptCF(CTFontCopyPostScriptName(ctFont)))
                name = postScriptName.get();
        }
        if (RefPtr handle = loadFontFromBytes(creationData->fontFaceData.get(), name))
            return handle;
    }

    if (RefPtr handle = loadFontFromPath(fontFilePath(ctFont), SourceKind::Exact))
        return handle;

    if (RetainPtr<CFStringRef> familyName = adoptCF(CTFontCopyFamilyName(ctFont)))
        return loadFontFromPath(bundledFontPathForFamilyName(familyName.get()), SourceKind::FamilyFallback);
    return nullptr;
}

inline int unitsPerEm(const FontHandle& handle)
{
    auto* thread = threadHandle();
    if (!thread)
        return 0;
    return nutjob_font_units_per_em(thread, handle.value());
}

inline float ascent(const FontHandle& handle, float size)
{
    auto* thread = threadHandle();
    if (!thread)
        return 0;
    return nutjob_font_ascent(thread, handle.value(), size);
}

inline float descent(const FontHandle& handle, float size)
{
    auto* thread = threadHandle();
    if (!thread)
        return 0;
    return nutjob_font_descent(thread, handle.value(), size);
}

inline float lineHeight(const FontHandle& handle, float size)
{
    auto* thread = threadHandle();
    if (!thread)
        return 0;
    return nutjob_font_line_height(thread, handle.value(), size);
}

inline float advanceWidth(const FontHandle& handle, int glyph, float size)
{
    auto* thread = threadHandle();
    if (!thread)
        return 0;
    return nutjob_font_advance_width(thread, handle.value(), glyph, size);
}

} // namespace WebCore::NutjobFontBackend

#endif
