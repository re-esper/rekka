#include "utils.h"

// Color converting codes shipped from Ejecta:
// https://github.com/phoboslab/Ejecta

static const uint32_t _htmlcolortable[] = {
	0xff000000, // invaid
	0xfffff8f0, // aliceblue
	0xffd7ebfa, // antiquewhite
	0xffffff00, // aqua
	0xffd4ff7f, // aquamarine
	0xfffffff0, // azure
	0xffdcf5f5, // beige
	0xffc4e4ff, // bisque
	0xff000000, // black
	0xffcdebff, // blanchedalmond
	0xffff0000, // blue
	0xffe22b8a, // blueviolet
	0xff2a2aa5, // brown
	0xff87b8de, // burlywood
	0xffa09e5f, // cadetblue
	0xff00ff7f, // chartreuse
	0xff1e69d2, // chocolate
	0xff507fff, // coral
	0xffed9564, // cornflowerblue
	0xffdcf8ff, // cornsilk
	0xff3c14dc, // crimson
	0xffffff00, // cyan
	0xff8b0000, // darkblue
	0xff8b8b00, // darkcyan
	0xff0b86b8, // darkgoldenrod
	0xffa9a9a9, // darkgray
	0xff006400, // darkgreen
	0xff6bb7bd, // darkkhaki
	0xff8b008b, // darkmagenta
	0xff2f6b55, // darkolivegreen
	0xff008cff, // darkorange
	0xffcc3299, // darkorchid
	0xff00008b, // darkred
	0xff7a96e9, // darksalmon
	0xff8fbc8f, // darkseagreen
	0xff8b3d48, // darkslateblue
	0xff4f4f2f, // darkslategray
	0xffd1ce00, // darkturquoise
	0xffd30094, // darkviolet
	0xff9314ff, // deeppink
	0xffffbf00, // deepskyblue
	0xff696969, // dimgray
	0xffff901e, // dodgerblue
	0xff2222b2, // firebrick
	0xfff0faff, // floralwhite
	0xff228b22, // forestgreen
	0xffff00ff, // fuchsia
	0xffdcdcdc, // gainsboro
	0xfffff8f8, // ghostwhite
	0xff00d7ff, // gold
	0xff20a5da, // goldenrod
	0xff808080, // gray
	0xff008000, // green
	0xff2fffad, // greenyellow
	0xfff0fff0, // honeydew
	0xffb469ff, // hotpink
	0xff5c5ccd, // indianred
	0xff82004b, // indigo
	0xfff0ffff, // ivory
	0xff8ce6f0, // khaki
	0xfffae6e6, // lavender
	0xfff5f0ff, // lavenderblush
	0xff00fc7c, // lawngreen
	0xffcdfaff, // lemonchiffon
	0xffe6d8ad, // lightblue
	0xff8080f0, // lightcoral
	0xffffffe0, // lightcyan
	0xffd2fafa, // lightgoldenrodyellow
	0xffd3d3d3, // lightgray
	0xff90ee90, // lightgreen
	0xffc1b6ff, // lightpink
	0xff7aa0ff, // lightsalmon
	0xffaab220, // lightseagreen
	0xffface87, // lightskyblue
	0xff998877, // lightslategray
	0xffdec4b0, // lightsteelblue
	0xffe0ffff, // lightyellow
	0xff00ff00, // lime
	0xff32cd32, // limegreen
	0xffe6f0fa, // linen
	0xffff00ff, // magenta
	0xff000080, // maroon
	0xffaacd66, // mediumaquamarine
	0xffcd0000, // mediumblue
	0xffd355ba, // mediumorchid
	0xffd87093, // mediumpurple
	0xff71b33c, // mediumseagreen
	0xffee687b, // mediumslateblue
	0xff9afa00, // mediumspringgreen
	0xffccd148, // mediumturquoise
	0xff8515c7, // mediumvioletred
	0xff701919, // midnightblue
	0xfffafff5, // mintcream
	0xffe1e4ff, // mistyrose
	0xffb5e4ff, // moccasin
	0xffaddeff, // navajowhite
	0xff800000, // navy
	0xffe6f5fd, // oldlace
	0xff008080, // olive
	0xff238e6b, // olivedrab
	0xff00a5ff, // orange
	0xff0045ff, // orangered
	0xffd670da, // orchid
	0xffaae8ee, // palegoldenrod
	0xff98fb98, // palegreen
	0xffeeeeaf, // paleturquoise
	0xff9370d8, // palevioletred
	0xffd5efff, // papayawhip
	0xffb9daff, // peachpuff
	0xff3f85cd, // peru
	0xffcbc0ff, // pink
	0xffdda0dd, // plum
	0xffe6e0b0, // powderblue
	0xff800080, // purple
	0xff0000ff, // red
	0xff8f8fbc, // rosybrown
	0xffe16941, // royalblue
	0xff13458b, // saddlebrown
	0xff7280fa, // salmon
	0xff60a4f4, // sandybrown
	0xff578b2e, // seagreen
	0xffeef5ff, // seashell
	0xff2d52a0, // sienna
	0xffc0c0c0, // silver
	0xffebce87, // skyblue
	0xffcd5a6a, // slateblue
	0xff908070, // slategray
	0xfffafaff, // snow
	0xff7fff00, // springgreen
	0xffb48246, // steelblue
	0xff8cb4d2, // tan
	0xff808000, // teal
	0xffd8bfd8, // thistle
	0xff4763ff, // tomato
	0xffd0e040, // turquoise
	0xffee82ee, // violet
	0xffb3def5, // wheat
	0xffffffff, // white
	0xfff5f5f5, // whitesmoke
	0xff00ffff, // yellow
	0xff32cd9a  // yellowgreen
};
static const unsigned char _colorhash2index[498] = {
	0,0,0,0,0,0,0,110,0,0,139,59,0,18,0,0,63,0,0,55,0,0,0,0,105,6,0,115,12,0,3,0,14,133,131,0,0,114,0,17,95,0,0,0,0,0,0,21,0,0,
	0,22,94,0,0,119,0,0,0,0,0,111,0,0,0,0,8,0,0,65,0,0,0,0,0,0,0,0,32,0,0,0,0,0,0,0,47,0,0,0,0,7,93,0,0,0,0,0,31,89,15,0,0,10,0,
	0,0,0,85,0,0,0,108,0,0,0,0,56,0,0,0,0,138,0,0,0,0,0,0,5,58,0,0,0,0,0,0,0,0,0,0,109,0,0,4,60,38,36,0,0,0,50,0,0,0,0,0,0,0,78,
	0,0,11,99,30,0,0,0,0,0,80,0,0,125,0,0,0,0,0,91,0,0,0,0,0,0,41,0,0,0,0,98,0,0,127,0,66,0,1,100,0,73,0,0,0,0,83,0,123,0,46,0,
	0,0,0,0,0,84,0,0,0,0,51,0,0,0,87,0,0,0,27,0,134,0,0,0,0,0,0,19,74,23,0,9,57,49,0,0,0,0,16,39,126,0,20,0,25,0,72,82,0,0,132,
	0,0,0,0,136,0,0,0,0,0,0,0,0,0,0,0,0,28,0,0,0,0,0,64,0,24,0,0,0,0,0,0,68,0,81,96,0,0,0,0,0,0,135,0,67,124,77,42,0,0,0,0,0,33,
	0,0,76,43,0,128,0,107,0,0,0,0,0,0,0,79,0,45,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,92,0,0,71,0,70,121,0,0,0,0,106,0,0,137,122,
	0,0,62,104,0,0,29,101,0,0,0,0,0,0,140,0,54,0,90,69,0,0,0,0,0,0,40,0,130,0,102,2,0,0,0,0,0,0,0,0,0,113,0,44,0,0,0,61,0,0,0,0,
	0,0,0,0,0,0,0,35,13,0,0,52,75,0,0,0,53,0,0,0,88,97,86,0,0,0,0,0,0,0,0,117,0,0,0,0,129,0,0,0,0,0,103,0,26,0,0,0,48,0,0,0,0,0,
	118,116,0,0,37,0,0,0,0,0,0,0,0,0,0,0,112,120,0,34,0,0,0
};
inline uint32_t colorHash(const char* s, int length)
{
	uint32_t h = 0;
	for (int i = 0; i < length; i++) {
		h = (h << 6) ^ (h >> 26) ^ (tolower(s[i]) * 1978555465);
	}
	return (h % 498);
};

SDL_Color HtmlColorToColor(const char* htmlcolor)
{
	union {
		uint32_t hex;
		struct {
			uint8_t r, g, b, a;
		} rgba;
		uint8_t components[4];
	} c = { 0xff000000 };
	char str[] = "ffffff";
	auto length = strlen(htmlcolor);

	// #f0f format
	if (htmlcolor[0] == '#' && length == 4) {
		str[0] = str[1] = htmlcolor[3];
		str[2] = str[3] = htmlcolor[2];
		str[4] = str[5] = htmlcolor[1];
		c.hex = 0xff000000 | strtol(str, NULL, 16);
	}
	// #ff00ff format
	else if (htmlcolor[0] == '#' && length == 7) {
		str[0] = htmlcolor[5];
		str[1] = htmlcolor[6];
		str[2] = htmlcolor[3];
		str[3] = htmlcolor[4];
		str[4] = htmlcolor[1];
		str[5] = htmlcolor[2];
		c.hex = 0xff000000 | strtol(str, NULL, 16);
	}
	// assume rgb(255,0,255) or rgba(255,0,255,0.5) format
	else if (htmlcolor[0] == 'r' && htmlcolor[1] == 'g') {
		int component = 0;
		for (int i = 4; i < length - 1 && component < 4; i++) {
			if (component == 3) {
				// If we have an alpha component, copy the rest of the wide
				// string into a char array and use atof() to parse it.
				char alpha[8] = { 0 };
				for (int j = 0; i + j < length - 1 && j < 7; j++) {
					alpha[j] = htmlcolor[i + j];
				}
				c.components[component] = atof(alpha) * 255.0f;
				component++;
			}
			else if (isdigit(htmlcolor[i])) {
				c.components[component] = c.components[component] * 10 + (htmlcolor[i] - '0');
			}
			else if (htmlcolor[i] == ',' || htmlcolor[i] == ')') {
				component++;
			}
		}
	}
	// try color name
	else {
		auto hash = colorHash(htmlcolor, length);
		c.hex = _htmlcolortable[_colorhash2index[hash]];		
	}	
	return *(SDL_Color*)&c;
}

const char* ColorToHtmlColor(const SDL_Color& color)
{
	static char htmlcolor[32];
	sprintf(htmlcolor, "rgba(%d,%d,%d,%.3f)", color.r, color.g, color.b, (float)color.a/255.0f );
	return htmlcolor;
}

static int _speckeytable[0x200] = { 0 };
static bool _keyTableInitialized = false;
int SDLKeyToHtmlKey(SDL_Keycode sdlkey)
{
	if (!_keyTableInitialized) {
		_keyTableInitialized = true;
		_speckeytable[SDLK_CAPSLOCK - SDLK_SCANCODE_MASK] = 20;
		for (int k = 0; k < 12; ++k)
			_speckeytable[SDLK_F1 + k - SDLK_SCANCODE_MASK] = 112 + k;
		// skip SDLK_PRINTSCREEN
		_speckeytable[SDLK_SCROLLLOCK - SDLK_SCANCODE_MASK] = 145;
		_speckeytable[SDLK_PAUSE - SDLK_SCANCODE_MASK] = 19;
		_speckeytable[SDLK_INSERT - SDLK_SCANCODE_MASK] = 45;
		_speckeytable[SDLK_HOME - SDLK_SCANCODE_MASK] = 36;
		_speckeytable[SDLK_PAGEUP - SDLK_SCANCODE_MASK] = 33;
		//_speckeytable[SDLK_DELETE] = 46;
		_speckeytable[SDLK_END - SDLK_SCANCODE_MASK] = 35;
		_speckeytable[SDLK_PAGEDOWN - SDLK_SCANCODE_MASK] = 34;
		_speckeytable[SDLK_RIGHT - SDLK_SCANCODE_MASK] = 39;
		_speckeytable[SDLK_LEFT - SDLK_SCANCODE_MASK] = 37;
		_speckeytable[SDLK_DOWN - SDLK_SCANCODE_MASK] = 40;
		_speckeytable[SDLK_UP - SDLK_SCANCODE_MASK] = 38;

		_speckeytable[SDLK_NUMLOCKCLEAR - SDLK_SCANCODE_MASK] = 144;
		_speckeytable[SDLK_KP_DIVIDE - SDLK_SCANCODE_MASK] = 111;
		_speckeytable[SDLK_KP_MULTIPLY - SDLK_SCANCODE_MASK] = 106;
		_speckeytable[SDLK_KP_MINUS - SDLK_SCANCODE_MASK] = 109;		 
		_speckeytable[SDLK_KP_PLUS - SDLK_SCANCODE_MASK] = 107;
		_speckeytable[SDLK_KP_ENTER - SDLK_SCANCODE_MASK] = 13;

		_speckeytable[SDLK_KP_1 - SDLK_SCANCODE_MASK] = 97;
		_speckeytable[SDLK_KP_2 - SDLK_SCANCODE_MASK] = 98;
		_speckeytable[SDLK_KP_3 - SDLK_SCANCODE_MASK] = 99;
		_speckeytable[SDLK_KP_4 - SDLK_SCANCODE_MASK] = 100;
		_speckeytable[SDLK_KP_5 - SDLK_SCANCODE_MASK] = 101;
		_speckeytable[SDLK_KP_6 - SDLK_SCANCODE_MASK] = 102;
		_speckeytable[SDLK_KP_7 - SDLK_SCANCODE_MASK] = 103;
		_speckeytable[SDLK_KP_8 - SDLK_SCANCODE_MASK] = 104;
		_speckeytable[SDLK_KP_9 - SDLK_SCANCODE_MASK] = 105;
		_speckeytable[SDLK_KP_0 - SDLK_SCANCODE_MASK] = 96;
		_speckeytable[SDLK_KP_PERIOD - SDLK_SCANCODE_MASK] = 110;

		_speckeytable[SDLK_LSHIFT - SDLK_SCANCODE_MASK] = 16;
		_speckeytable[SDLK_RSHIFT - SDLK_SCANCODE_MASK] = 16;
		_speckeytable[SDLK_LCTRL - SDLK_SCANCODE_MASK] = 17;
		_speckeytable[SDLK_RCTRL - SDLK_SCANCODE_MASK] = 17;
		_speckeytable[SDLK_LALT - SDLK_SCANCODE_MASK] = 18;
		_speckeytable[SDLK_RALT - SDLK_SCANCODE_MASK] = 18;
		_speckeytable[SDLK_LGUI - SDLK_SCANCODE_MASK] = 91;
		_speckeytable[SDLK_RGUI - SDLK_SCANCODE_MASK] = 92;
		_speckeytable[SDLK_SELECT - SDLK_SCANCODE_MASK] = 93;		
	}
	if (sdlkey & SDLK_SCANCODE_MASK) {
		return _speckeytable[sdlkey - SDLK_SCANCODE_MASK];
	}
	else if (sdlkey >= SDLK_a && sdlkey <= SDLK_z) {
		return sdlkey - 0x20;
	}
	else if (sdlkey == SDLK_DELETE) {
		return 46;
	}
	return sdlkey;
}

char* fetchFileContent(const char* filePath, size_t* dataLength)
{
	SDL_RWops* rwops = SDL_RWFromFile(filePath, "rb");
	*dataLength = 0;
	if (!rwops) return nullptr;
	SDL_RWseek(rwops, 0, SEEK_SET);
	int dataBytes = SDL_RWseek(rwops, 0, SEEK_END);
	SDL_RWseek(rwops, 0, SEEK_SET);
	// Read in the rwops data
	char* data = (char*)malloc(dataBytes + 1);
	dataBytes = SDL_RWread(rwops, data, 1, dataBytes);
	data[dataBytes] = 0;
	SDL_RWclose(rwops);
	*dataLength = dataBytes;
	return data;
}

bool writeFileContent(const char* filePath, const char * content, size_t dataLength)
{
	SDL_RWops* rwops = SDL_RWFromFile(filePath, "wb");
	if (!rwops) return false;
	bool ok = SDL_RWwrite(rwops, content, 1, dataLength) == dataLength;
	SDL_RWclose(rwops);
	return ok;
}


// Flexible and Economical UTF-8 Decoder from Bjoern Hoehrmann <bjoern@hoehrmann.de>
// See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.
#define UTF8_ACCEPT 0
#define UTF8_REJECT 12
uint32_t inline decode(uint32_t* state, uint32_t* codep, uint32_t byte)
{
	static const unsigned char utf8d[] = {
		// The first part of the table maps bytes to character classes that
		// to reduce the size of the transition table and create bitmasks.
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
		7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
		8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
		10,3,3,3,3,3,3,3,3,3,3,3,3,4,3,3, 11,6,6,6,5,8,8,8,8,8,8,8,8,8,8,8,
		// The second part is a transition table that maps a combination
		// of a state of the automaton and a character class to a state.
		0,12,24,36,60,96,84,12,12,12,48,72, 12,12,12,12,12,12,12,12,12,12,12,12,
		12, 0,12,12,12,12,12, 0,12, 0,12,12, 12,24,12,12,12,12,12,24,12,24,12,12,
		12,12,12,12,12,12,12,24,12,12,12,12, 12,24,12,12,12,12,12,12,12,24,12,12,
		12,12,12,12,12,12,12,36,12,36,12,12, 12,36,12,12,12,12,12,36,12,36,12,12,
		12,36,12,12,12,12,12,12,12,12,12,12,
	};
	uint32_t type = utf8d[byte];
	*codep = (*state != UTF8_ACCEPT) ? (byte & 0x3fu) | (*codep << 6) : (0xff >> type) & (byte);
	*state = utf8d[256 + *state + type];
	return *state;
}
uint32_t toUtf16(const uint8_t* src, uint32_t sizeBytes, uint16_t* dst, uint32_t sizeWideChar)
{
	const uint8_t* src_actual_end = src + sizeBytes;
	const uint8_t* s = src;
	uint16_t* d = dst;
	uint32_t codepoint;
	uint32_t state = 0;
	uint32_t outSize = 0;
	while (s < src_actual_end) {
		uint32_t dst_words_free = sizeWideChar - (d - dst);
		const uint8_t* src_current_end = s + dst_words_free;		
		if (src_actual_end < src_current_end) src_current_end = src_actual_end;
		if (src_current_end <= s) break;

		while (s < src_current_end) {
			if (decode(&state, &codepoint, *s++))
				continue;
			if (codepoint > 0xffff) {
				*d++ = (uint16_t)(0xD7C0 + (codepoint >> 10));
				*d++ = (uint16_t)(0xDC00 + (codepoint & 0x3FF));
				outSize += 2;
			}
			else {
				*d++ = (uint16_t)codepoint;
				outSize += 1;
			}
		}
	}
	return outSize;
}
char16_t* toUtf16(const char * src, uint32_t* outSize)
{
	size_t srcSize = strlen(src);
	char16_t* dst = (char16_t*)malloc((srcSize + 1) * 2);
	*outSize = toUtf16((const uint8_t*)src, srcSize, (uint16_t*)dst, srcSize + 1);
	if (*outSize == 0) {
		free(dst);
		return nullptr;
	}
	return dst;
}

int decodeURIComponent(char *src, char *dst) {
	if (!dst) dst = src;
	int length;
	for (length = 0; *src; length++) {
		if (*src == '%' && src[1] && src[2] && isxdigit(src[1]) && isxdigit(src[2])) {
			src[1] -= src[1] <= '9' ? '0' : (src[1] <= 'F' ? 'A' : 'a') - 10;
			src[2] -= src[2] <= '9' ? '0' : (src[2] <= 'F' ? 'A' : 'a') - 10;
			dst[length] = 16 * src[1] + src[2];
			src += 3;
			continue;
		}
		dst[length] = *src++;
	}
	dst[length] = '\0';
	return length;
}
