#include "font_manager.h"
#include <algorithm>

NS_REK_BEGIN

FontManager* FontManager::s_sharedFontManager = nullptr;
FontManager* FontManager::getInstance()
{
	if (!s_sharedFontManager) s_sharedFontManager = new (std::nothrow) FontManager();
	return s_sharedFontManager;
}

FontManager::FontManager() : _time(0)
{
	TTF_Init();	
	loadFont("fonts/micross.ttf", "sans-serif");
	_timeToCheck = TEXT_CACHE_CHECKTIME;
}
FontManager::~FontManager()
{
	for (auto& itr : _textImageCache) {
		gpu::FreeImage(itr.second.image);
	}
	_textImageCache.clear();
	for (auto& itr : _fontTable) {
		TTF_CloseFont(itr.second);
	}
	_fontTable.clear();
	for (auto& itr : _strokeFontTable) {
		TTF_CloseFont(itr.second);
	}
	_strokeFontTable.clear();
	TTF_Quit();
}

void FontManager::loadFont(const char * fileName, const char * familyName)
{
	if (familyName) {
		std::string namestr = familyName;
		std::transform(namestr.begin(), namestr.end(), namestr.begin(), ::tolower);
		_familyTable.push_back({ namestr, fileName });
	}
	else {
		TTF_Font* ttf = TTF_OpenFont(fileName, 10);		
		std::string namestr = TTF_FontFaceFamilyName(ttf);
		std::transform(namestr.begin(), namestr.end(), namestr.begin(), ::tolower);
		_familyTable.push_back({ namestr, fileName });
		TTF_CloseFont(ttf);
	}
}

int FontManager::findFontId(const std::vector<std::string>& familyNames)
{
	for (auto& name : familyNames) {
		std::string namestr = name;
		std::transform(namestr.begin(), namestr.end(), namestr.begin(), ::tolower);
		for (int id = 0; id < _familyTable.size(); ++id) {
			if (_familyTable[id].family.find(namestr) != std::string::npos) {
				return id;
			}
		}
	}	
	return -1;
}

const char* FontManager::familyNameById(int fontId)
{
	if (fontId < 0 || fontId >= _familyTable.size()) return nullptr;
	return _familyTable[fontId].family.c_str();
}

gpu::Image * FontManager::drawText(const char * text, bool isOffscreen, const Font & font, TextBaseline textBaseline, TextAlign textAlign, int lineWidth, int lineCap, int lineJoin, int miterLimit)
{
	auto descriptor = fontDescriptor(font, lineWidth, lineCap, lineJoin, miterLimit);
	TTF_Font* ttf = nullptr;
	if (lineWidth > 0) {
		auto itr = _strokeFontTable.find(descriptor);
		ttf = itr == _strokeFontTable.end() ? openFont(font, lineWidth, lineCap, lineJoin, miterLimit) : itr->second;
	}
	else {
		auto itr = _fontTable.find(descriptor);
		ttf = itr == _fontTable.end() ? openFont(font) : itr->second;
	}
	if (!ttf) return nullptr;

	gpu::Image* image = nullptr;
	if (isOffscreen) { // ÀëÆÁ»æÖÆ²»»º´æ
		auto surf = TTF_RenderUTF8_Blended_PremultipliedAlpha(ttf, text);
		image = gpu::CopyImageFromSurface(surf);		
		SDL_FreeSurface(surf);
	}
	else { // Ö÷ÆÁ»æÖÆ×ö¼òµ¥»º´æ
		std::string cacheId = std::string(text) + descriptor;
		image = fetch(cacheId);
		if (image == nullptr) {
			auto surf = TTF_RenderUTF8_Blended_PremultipliedAlpha(ttf, text);
			image = gpu::CopyImageFromSurface(surf);			
			SDL_FreeSurface(surf);			
			_textImageCache[cacheId] = { image, _time };
		}
	}
	makeTextAnchor(image, ttf, textBaseline, textAlign, lineWidth);
	return image;
}

void FontManager::makeTextAnchor(gpu::Image * image, TTF_Font * ttf, TextBaseline textBaseline, TextAlign textAlign, int lineWidth)
{
	float anchor_x = 0, anchor_y = 0;
	float ascent, descent, margin = 0;
	image->anchor_fixed = true;	
	switch (textBaseline) {
	case kTextBaselineTop:
	case kTextBaselineHanging:
		anchor_y = lineWidth * 0.5;
		break;
	case kTextBaselineBottom:
	case kTextBaselineIdeographic:
		anchor_y = image->base_h - lineWidth * 0.5;
		break;
	case kTextBaselineMiddle:
		anchor_y = image->base_h * 0.5;
		break;
	case kTextBaselineAlphabetic:
		ascent = TTF_FontAscent(ttf);
		anchor_y = ascent + lineWidth * 0.5;
		break;
	}
	switch (textAlign) {
	case kTextAlignStart:
	case kTextAlignLeft:
		anchor_x = lineWidth * 0.5;		
		break;
	case kTextAlignEnd:
	case kTextAlignRight:
		anchor_x = image->base_w  - lineWidth * 0.5;
		break;
	case kTextAlignCenter:
		anchor_x = 0.5 * image->base_w;
		break;
	}
	gpu::SetAnchor(image, anchor_x, anchor_y);
}

int FontManager::measureText(const char * text, const Font & font)
{
	auto descriptor = fontDescriptor(font);
	auto itr = _fontTable.find(descriptor);	
	TTF_Font* ttf = itr == _fontTable.end() ? openFont(font) : itr->second;
	int w, h;
	TTF_SizeUTF8(ttf, text, &w, &h);
	return w;
}

void FontManager::update(float deltaTime)
{
	_time += deltaTime;
	_timeToCheck -= deltaTime;
	if (_timeToCheck < 0) {
		// ¼ì²é»º´æ
		for (auto itr = _textImageCache.begin(); itr != _textImageCache.end(); ) {
			if (_time - itr->second.fetchTime > TEXT_CACHE_LIFETIME) {
				gpu::FreeImage(itr->second.image);
				itr = _textImageCache.erase(itr);
			}
			else itr++;
		}
		_timeToCheck += TEXT_CACHE_CHECKTIME;
	}
}

const std::string& FontManager::fontDescriptor(const Font& font, int lineWidth, int lineCap, int lineJoin, int miterLimit)
{
	static std::string descriptor;
	static char buf[0x40];
	if (lineWidth > 0) { // stroke font
		sprintf(buf, "%d_%d_%d_%d_%d_%d_%d_%d", font.familyId, font.size, font.italic ? 1 : 0, font.bold ? 1 : 0, lineWidth, lineCap, lineJoin, miterLimit);
	}
	else { // normal font
		sprintf(buf, "%d_%d_%d_%d", font.familyId, font.size, font.italic ? 1 : 0, font.bold ? 1 : 0);
	}
	descriptor = buf;
	return descriptor;
}

TTF_Font* FontManager::openFont(const Font & font, int lineWidth, int lineCap, int lineJoin, int miterLimit)
{
	if (font.familyId < 0 || font.familyId >= _familyTable.size()) return nullptr;
	TTF_Font* ttf = TTF_OpenFont(_familyTable[font.familyId].file.c_str(), font.size);
	if (!ttf) return nullptr;
	auto descriptor = fontDescriptor(font, lineWidth, lineCap, lineJoin, miterLimit);
	TTF_SetFontHinting(ttf, TTF_HINTING_LIGHT);
	int style = 0;
	if (font.italic) style |= TTF_STYLE_ITALIC;
	if (font.bold) style |= TTF_STYLE_BOLD;
	TTF_SetFontStyle(ttf, style);
	if (lineWidth > 0) {
		TTF_SetFontOutline(ttf, lineWidth * 0.5);
		TTF_SetFontOutlineLineCap(ttf, lineCap);
		TTF_SetFontOutlineLineJoin(ttf, lineJoin);		
		TTF_SetFontOutlineMiterLimit(ttf, miterLimit);
		_strokeFontTable[descriptor] = ttf;
	}
	else {
		_fontTable[descriptor] = ttf;
	}
	return ttf;
}

gpu::Image* FontManager::fetch(const std::string & cacheId)
{
	auto itr = _textImageCache.find(cacheId);
	if (itr == _textImageCache.end()) return nullptr;
	itr->second.fetchTime = _time;
	return itr->second.image;
}

NS_REK_END