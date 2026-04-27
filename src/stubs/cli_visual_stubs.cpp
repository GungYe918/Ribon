#include <Ribon/FrameBuffer.hpp>

extern "C" {
#include <Uefi.h>
}

namespace ribon::ui {

void appendConsoleOverlayText(const CHAR16*) {
}

} // namespace ribon::ui

namespace ribon::fb {

bool initFrameBuffer() {
    return false;
}

const FrameBufferInfo* getFramebuffer() {
    return nullptr;
}

bool inBounds(int, int) {
    return false;
}

UINT32 readPixel(int, int) {
    return 0;
}

void writePixel(int, int, UINT32) {
}

void writePixelClamped(int, int, UINT32) {
}

void writePixelRaw(UINTN, UINTN, UINT32) {
}

void writePixelSafe(UINTN, UINTN, UINT32) {
}

void clear(UINT8, UINT8, UINT8) {
}

void debugDump(UINTN) {
}

void debugPrintInfo() {
}

} // namespace ribon::fb
