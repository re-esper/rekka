#pragma once

#include "rekka.h"
#include "SDL_ttf.h"
#include <map>

NS_REK_BEGIN

#define TEXT_CACHE_LIFETIME		8.0
#define TEXT_CACHE_CHECKTIME	2.0


typedef enum {
	kTextBaselineAlphabetic = 0,
	kTextBaselineMiddle,
	kTextBaselineTop,
	kTextBaselineHanging,
	kTextBaselineBottom,
	kTextBaselineIdeographic
} TextBaseline;
static const char *_textBaseline_enum_names[] = {
	"alphabetic",
	"middle",
	"top",
	"hanging",
	"bottom",
	"ideographic",
	nullptr
};

typedef enum {
	kTextAlignStart = 0,
	kTextAlignEnd,
	kTextAlignLeft,
	kTextAlignCenter,
	kTextAlignRight
} TextAlign;
static const char *_textAlign_enum_names[] = {
	"start",
	"end",
	"left",
	"center",
	"right",
	nullptr
};

struct Font {
	int	familyId;
	int size;
	bool italic;
	bool bold;	
};

class FontManager {
private:
	static FontManager* s_sharedFontManager;	
public:
	FontManager();
	~FontManager();
	static FontManager* getInstance();
public:
	void loadFont(const char* fileName, const char* familyName = nullptr);
	int findFontId(const std::vector<std::string>& familyNames);
	const char* familyNameById(int fontId);
	gpu::Image* drawText(const char* text, bool isOffscreen, const Font& font, TextBaseline textBaseline, TextAlign textAlign, int lineWidth = 0, int lineCap = -1, int lineJoin = -1, int miterLimit = 0);	
	int measureText(const char* text, const Font& font);
	void update(float deltaTime);
private:
	const std::string& fontDescriptor(const Font& font, int lineWidth = 0, int lineCap = -1, int lineJoin = -1, int miterLimit = 0);
	TTF_Font* openFont(const Font& font, int lineWidth = 0, int lineCap = -1, int lineJoin = -1, int miterLimit = 0);
	gpu::Image* fetch(const std::string& cacheId);
	void makeTextAnchor(gpu::Image* image, TTF_Font* ttf, TextBaseline textBaseline, TextAlign textAlign, int lineWidth = 0);
private:
	struct fontfamily_t {
		std::string family;
		std::string file;
	};
	std::vector<fontfamily_t> _familyTable;
	std::map<std::string, TTF_Font*> _fontTable;
	std::map<std::string, TTF_Font*> _strokeFontTable;
	struct cachedimage_t {
		gpu::Image* image;
		float fetchTime;
	};
	std::map<std::string, cachedimage_t> _textImageCache;
	float _time;
	float _timeToCheck;
};

NS_REK_END