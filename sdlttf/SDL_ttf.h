/*
  SDL_ttf:  A companion library to SDL for working with TrueType (tm) fonts
  Copyright (C) 2001-2016 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

/* This library is a wrapper around the excellent FreeType 2.0 library,
   available at:
    http://www.freetype.org/
*/

/* Note: In many places, SDL_ttf will say "glyph" when it means "code point."
   Unicode is hard, we learn as we go, and we apologize for adding to the
   confusion. */

#ifndef _SDL_TTF_H
#define _SDL_TTF_H

#include "SDL.h"
#include "begin_code.h"

/* Printable format: "%d.%d.%d", MAJOR, MINOR, PATCHLEVEL
*/
#define SDL_TTF_MAJOR_VERSION   2
#define SDL_TTF_MINOR_VERSION   0
#define SDL_TTF_PATCHLEVEL      14

/* This macro can be used to fill a version structure with the compile-time
 * version of the SDL_ttf library.
 */
#define SDL_TTF_VERSION(X)                          \
{                                                   \
    (X)->major = SDL_TTF_MAJOR_VERSION;             \
    (X)->minor = SDL_TTF_MINOR_VERSION;             \
    (X)->patch = SDL_TTF_PATCHLEVEL;                \
}

/* Backwards compatibility */
#define TTF_MAJOR_VERSION   SDL_TTF_MAJOR_VERSION
#define TTF_MINOR_VERSION   SDL_TTF_MINOR_VERSION
#define TTF_PATCHLEVEL      SDL_TTF_PATCHLEVEL
#define TTF_VERSION(X)      SDL_TTF_VERSION(X)

/* Make sure this is defined (only available in newer SDL versions) */
#ifndef SDL_DEPRECATED
#define SDL_DEPRECATED
#endif

/* This function gets the version of the dynamically linked SDL_ttf library.
   it should NOT be used to fill a version structure, instead you should
   use the SDL_TTF_VERSION() macro.
 */
extern DECLSPEC const SDL_version * SDLCALL TTF_Linked_Version(void);

/* ZERO WIDTH NO-BREAKSPACE (Unicode byte order mark) */
#define UNICODE_BOM_NATIVE  0xFEFF
#define UNICODE_BOM_SWAPPED 0xFFFE

/* This function tells the library whether UNICODE text is generally
   byteswapped.  A UNICODE BOM character in a string will override
   this setting for the remainder of that string.
*/
extern DECLSPEC void SDLCALL TTF_ByteSwappedUNICODE(int swapped);

/* The internal structure containing font information */
typedef struct _TTF_Font TTF_Font;

/* Initialize the TTF engine - returns 0 if successful, -1 on error */
extern DECLSPEC int SDLCALL TTF_Init(void);

extern DECLSPEC TTF_Font * SDLCALL TTF_OpenFont(const char *file, int pixelsize);


/* Set and retrieve the font style */
#define TTF_STYLE_NORMAL        0x00
#define TTF_STYLE_BOLD          0x01
#define TTF_STYLE_ITALIC        0x02
#define TTF_STYLE_UNDERLINE     0x04
#define TTF_STYLE_STRIKETHROUGH 0x08
extern DECLSPEC int SDLCALL TTF_GetFontStyle(const TTF_Font *font);
extern DECLSPEC void SDLCALL TTF_SetFontStyle(TTF_Font *font, int style);

extern DECLSPEC float SDLCALL TTF_GetFontOutline(const TTF_Font *font);
extern DECLSPEC void SDLCALL TTF_SetFontOutline(TTF_Font *font, float outline);

#define TTF_STROKER_LINECAP_BUTT	0
#define TTF_STROKER_LINECAP_ROUND	1
#define TTF_STROKER_LINECAP_SQUARE	2
extern DECLSPEC void SDLCALL TTF_SetFontOutlineLineCap(TTF_Font *font, int line_cap);
#define TTF_STROKER_LINEJOIN_ROUND	0
#define TTF_STROKER_LINEJOIN_BEVEL	1
#define TTF_STROKER_LINEJOIN_MITER	2
extern DECLSPEC void SDLCALL TTF_SetFontOutlineLineJoin(TTF_Font *font, int line_join);
extern DECLSPEC void SDLCALL TTF_SetFontOutlineMiterLimit(TTF_Font *font, int miter_limit);

/* Set and retrieve FreeType hinter settings */
#define TTF_HINTING_NORMAL    0
#define TTF_HINTING_LIGHT     1
#define TTF_HINTING_MONO      2
#define TTF_HINTING_NONE      3
extern DECLSPEC int SDLCALL TTF_GetFontHinting(const TTF_Font *font);
extern DECLSPEC void SDLCALL TTF_SetFontHinting(TTF_Font *font, int hinting);

/* Get the total height of the font - usually equal to point size */
extern DECLSPEC int SDLCALL TTF_FontHeight(const TTF_Font *font);

/* Get the offset from the baseline to the top of the font
   This is a positive value, relative to the baseline.
 */
extern DECLSPEC int SDLCALL TTF_FontAscent(const TTF_Font *font);

/* Get the offset from the baseline to the bottom of the font
   This is a negative value, relative to the baseline.
 */
extern DECLSPEC int SDLCALL TTF_FontDescent(const TTF_Font *font);

/* Get the recommended spacing between lines of text for this font */
extern DECLSPEC int SDLCALL TTF_FontLineSkip(const TTF_Font *font);

/* Get/Set whether or not kerning is allowed for this font */
extern DECLSPEC int SDLCALL TTF_GetFontKerning(const TTF_Font *font);
extern DECLSPEC void SDLCALL TTF_SetFontKerning(TTF_Font *font, int allowed);

/* Get the number of faces of the font */
extern DECLSPEC long SDLCALL TTF_FontFaces(const TTF_Font *font);

/* Get the font face attributes, if any */
extern DECLSPEC int SDLCALL TTF_FontFaceIsFixedWidth(const TTF_Font *font);
extern DECLSPEC char * SDLCALL TTF_FontFaceFamilyName(const TTF_Font *font);
extern DECLSPEC char * SDLCALL TTF_FontFaceStyleName(const TTF_Font *font);

/* Check wether a glyph is provided by the font or not */
extern DECLSPEC int SDLCALL TTF_GlyphIsProvided(const TTF_Font *font, Uint16 ch);

/* Get the metrics (dimensions) of a glyph
   To understand what these metrics mean, here is a useful link:
    http://freetype.sourceforge.net/freetype2/docs/tutorial/step2.html
 */
extern DECLSPEC int SDLCALL TTF_GlyphMetrics(TTF_Font *font, Uint16 ch,
                     int *minx, int *maxx,
                                     int *miny, int *maxy, int *advance);

/* Get the dimensions of a rendered string of text */
extern DECLSPEC int SDLCALL TTF_SizeUTF8(TTF_Font *font, const char *text, int *w, int *h);

/* Create a 32-bit ARGB surface and render the given text at high quality,
   using alpha blending to dither the font with the given color.
   This function returns the new surface, or NULL if there was an error.
*/

extern DECLSPEC SDL_Surface *TTF_RenderUTF8_Blended_PremultipliedAlpha(TTF_Font *font, const char *text);

/* Close an opened font file */
extern DECLSPEC void SDLCALL TTF_CloseFont(TTF_Font *font);

/* De-initialize the TTF engine */
extern DECLSPEC void SDLCALL TTF_Quit(void);

/* Check if the TTF engine is initialized */
extern DECLSPEC int SDLCALL TTF_WasInit(void);

/* Get the kerning size of two glyphs indices */
/* DEPRECATED: this function requires FreeType font indexes, not glyphs,
   by accident, which we don't expose through this API, so it could give
   wildly incorrect results, especially with non-ASCII values.
   Going forward, please use TTF_GetFontKerningSizeGlyphs() instead, which
   does what you probably expected this function to do. */
extern DECLSPEC int TTF_GetFontKerningSize(TTF_Font *font, int prev_index, int index) SDL_DEPRECATED;

/* Get the kerning size of two glyphs */
extern DECLSPEC int TTF_GetFontKerningSizeGlyphs(TTF_Font *font, Uint16 previous_ch, Uint16 ch);

/* We'll use SDL for reporting errors */
#define TTF_SetError    SDL_SetError
#define TTF_GetError    SDL_GetError

#include "close_code.h"

#endif /* _SDL_TTF_H */
