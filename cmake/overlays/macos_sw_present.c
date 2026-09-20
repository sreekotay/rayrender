/* Fast macOS software present for raylib RGFW.
 *
 * Replaces RGFW's per-frame NSBitmapImageRep/NSImage path with a CGImage
 * wrapping the CPU framebuffer (no extra channel scramble / copy), tagged
 * sRGB + absolute colorimetric to reduce Quartz CMS work.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <CoreGraphics/CoreGraphics.h>
#include <objc/message.h>
#include <objc/runtime.h>

typedef struct {
    int w, h, stride;
    CGColorSpaceRef colorSpace;
    CGDataProviderRef provider;
    void *pixels; /* non-owning */
} PresentState;

static PresentState g_present;

static void present_reset(void)
{
    if (g_present.provider) {
        CGDataProviderRelease(g_present.provider);
        g_present.provider = NULL;
    }
    if (g_present.colorSpace) {
        CGColorSpaceRelease(g_present.colorSpace);
        g_present.colorSpace = NULL;
    }
    g_present.w = g_present.h = g_present.stride = 0;
    g_present.pixels = NULL;
}

static const void *present_provider_get_byte_pointer(void *info)
{
    (void)info;
    return g_present.pixels;
}

static size_t present_provider_get_bytes_at_position(void *info, void *buffer, off_t pos, size_t count)
{
    (void)info;
    size_t total = (size_t)g_present.stride * (size_t)g_present.h * 4u;
    if (pos < 0 || (size_t)pos >= total) return 0;
    if ((size_t)pos + count > total) count = total - (size_t)pos;
    memcpy(buffer, (const uint8_t *)g_present.pixels + pos, count);
    return count;
}

static void present_ensure(void *pixels, int w, int h)
{
    if (g_present.pixels == pixels && g_present.w == w && g_present.h == h && g_present.provider)
        return;

    present_reset();
    g_present.pixels = pixels;
    g_present.w = w;
    g_present.h = h;
    g_present.stride = w;
    g_present.colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);

    static const CGDataProviderDirectCallbacks callbacks = {
        0,
        present_provider_get_byte_pointer,
        NULL,
        present_provider_get_bytes_at_position,
        NULL,
    };
    size_t nbytes = (size_t)w * (size_t)h * 4u;
    g_present.provider = CGDataProviderCreateDirect(NULL, (off_t)nbytes, &callbacks);
}

void RayrenderMacOSPresent(void *nsview, void *pixels, int width, int height)
{
    if (!nsview || !pixels || width <= 0 || height <= 0) return;

    present_ensure(pixels, width, height);
    if (!g_present.provider || !g_present.colorSpace) return;

    /* RGBA8888 in memory: byte-order 32-big + alpha last. */
    CGBitmapInfo bitmapInfo = (CGBitmapInfo)kCGImageAlphaLast | (CGBitmapInfo)kCGBitmapByteOrder32Big;
    CGImageRef image = CGImageCreate(
        (size_t)width,
        (size_t)height,
        8,
        32,
        (size_t)width * 4u,
        g_present.colorSpace,
        bitmapInfo,
        g_present.provider,
        NULL,
        false,
        kCGRenderingIntentAbsoluteColorimetric);
    if (!image) return;

    id view = (id)nsview;
    id layer = ((id (*)(id, SEL))objc_msgSend)(view, sel_getUid("layer"));
    if (layer) {
        /* Avoid setContentsColorSpace: — NSOpenGLViewBackingLayer does not implement it. */
        ((void (*)(id, SEL, id))objc_msgSend)(layer, sel_getUid("setContents:"), (id)image);
    }

    CGImageRelease(image);
}
