#pragma once

#include "rekka.h"

inline bool IsAffineTransformAxisAligned(const AffineTransform* t) { return t->a == 0 || t->b == 0; }
inline bool IsAffineTransformIdentity(const AffineTransform* t) {
	return t->a == 1 && t->b == 0 && t->c == 0 && t->d == 1 && t->tx == 0 && t->ty == 0;
}
inline bool isPowerOfTwo(uint32_t x) { return ((x & (x - 1)) == 0); }

SDL_Color HtmlColorToColor(const char* htmlcolor);
const char* ColorToHtmlColor(const SDL_Color& color);

int SDLKeyToHtmlKey(SDL_Keycode sdlkey);

char* fetchFileContent(const char* filePath, size_t* dataLength);
bool writeFileContent(const char* filePath, const char* content, size_t dataLength);

uint32_t toUtf16(const uint8_t* src, uint32_t sizeBytes, uint16_t* dst, uint32_t sizeWideChar);
char16_t* toUtf16(const char* src, uint32_t* outSize);

int decodeURIComponent(char *src, char *dst = nullptr);