/*
 * FTGL - OpenGL font library
 *
 * Copyright (c) 2001-2004 Henry Maddocks <ftgl@opengl.geek.nz>
 * Copyright (c) 2008 Sam Hocevar <sam@zoy.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "config.h"

#include <ctype.h>
#include <wctype.h>

#include "FTInternals.h"
#include "FTUnicode.h"

#include "FTGlyphContainer.h"
#include "FTSimpleLayoutImpl.h"


//
//  FTSimpleLayout
//


FTSimpleLayout::FTSimpleLayout() :
FTLayout(new FTSimpleLayoutImpl())
{}


FTSimpleLayout::~FTSimpleLayout()
{}


FTBBox FTSimpleLayout::BBox(const char *string, const int len, FTPoint pos)
{
    return dynamic_cast<FTSimpleLayoutImpl*>(impl)->BBox(string, len, pos);
}


FTBBox FTSimpleLayout::BBox(const wchar_t *string, const int len, FTPoint pos)
{
    return dynamic_cast<FTSimpleLayoutImpl*>(impl)->BBox(string, len, pos);
}


void FTSimpleLayout::Render(const char *string, const int len, FTPoint pos,
                            int renderMode)
{
    return dynamic_cast<FTSimpleLayoutImpl*>(impl)->Render(string, len, pos,
                                                           renderMode);
}


void FTSimpleLayout::Render(const wchar_t* string, const int len, FTPoint pos,
                            int renderMode)
{
    return dynamic_cast<FTSimpleLayoutImpl*>(impl)->Render(string, len, pos,
                                                           renderMode);
}


void FTSimpleLayout::SetFont(FTFont *fontInit)
{
    dynamic_cast<FTSimpleLayoutImpl*>(impl)->currentFont = fontInit;
    dynamic_cast<FTSimpleLayoutImpl*>(impl)->invalidate();
}


FTFont *FTSimpleLayout::GetFont()
{
    return dynamic_cast<FTSimpleLayoutImpl*>(impl)->currentFont;
}


void FTSimpleLayout::SetLineLength(const float LineLength)
{
    dynamic_cast<FTSimpleLayoutImpl*>(impl)->lineLength = LineLength;
    dynamic_cast<FTSimpleLayoutImpl*>(impl)->invalidate();
}


float FTSimpleLayout::GetLineLength() const
{
    return dynamic_cast<FTSimpleLayoutImpl*>(impl)->lineLength;
}


void FTSimpleLayout::SetAlignment(const FTGL::TextAlignment Alignment)
{
    dynamic_cast<FTSimpleLayoutImpl*>(impl)->alignment = Alignment;
    dynamic_cast<FTSimpleLayoutImpl*>(impl)->invalidate();
}


FTGL::TextAlignment FTSimpleLayout::GetAlignment() const
{
    return dynamic_cast<FTSimpleLayoutImpl*>(impl)->alignment;
}


void FTSimpleLayout::SetLineSpacing(const float LineSpacing)
{
    dynamic_cast<FTSimpleLayoutImpl*>(impl)->lineSpacing = LineSpacing;
    dynamic_cast<FTSimpleLayoutImpl*>(impl)->invalidate();
}


float FTSimpleLayout::GetLineSpacing() const
{
    return dynamic_cast<FTSimpleLayoutImpl*>(impl)->lineSpacing;
}


//
//  FTSimpleLayoutImpl
//


FTSimpleLayoutImpl::FTSimpleLayoutImpl()
{
    currentFont = NULL;
    lineLength = 100.0f;
    alignment = FTGL::ALIGN_LEFT;
    lineSpacing = 1.0f;
    stringCache = NULL;
	stringCacheCount = 0;
}

FTSimpleLayoutImpl::~FTSimpleLayoutImpl()
{
	free(stringCache);
	stringCache = NULL;
	stringCacheCount = 0;
}


inline void FTSimpleLayoutImpl::invalidate()
{
	stringCacheCount = 0;
}


template <typename T>
inline FTBBox FTSimpleLayoutImpl::BBoxI(const T* string, const int len,
                                        FTPoint position)
{
    FTBBox tmp;
	
    WrapText(string, len, position, 0, &tmp);
	
    return tmp;
}


FTBBox FTSimpleLayoutImpl::BBox(const char *string, const int len,
                                FTPoint position)
{
    return BBoxI(string, len, position);
}


FTBBox FTSimpleLayoutImpl::BBox(const wchar_t *string, const int len,
                                FTPoint position)
{
    return BBoxI(string, len, position);
}


template <typename T>
inline void FTSimpleLayoutImpl::RenderI(const T *string, const int len,
                                        FTPoint position, int renderMode)
{
    pen = FTPoint(0.0f, 0.0f);
    WrapText(string, len, position, renderMode, NULL);
}


void FTSimpleLayoutImpl::Render(const char *string, const int len,
                                FTPoint position, int renderMode)
{
    RenderI(string, len, position, renderMode);
}


void FTSimpleLayoutImpl::Render(const wchar_t* string, const int len,
                                FTPoint position, int renderMode)
{
    RenderI(string, len, position, renderMode);
}

/**
 * DAVID PETRIE NOTE: This function has been altered to ensure efficient rendering
 * speeds on iOS devices.  The changes are the following:
 * - A cache is populated with linebreak and spacing info; it must be invalidated and 
 *   refreshed when text changes.
 * - PreRender() and PostRender() have been added to the FTFont and FTFontImpl classes.  
 *   These functions allow all FTFont implementations to perform pre-rendering setup 
 *   tasks specific to their requirements.  For example, when rendering a texture font
 *	 it is not necessary to call ftglEnd() unless the loaded texture id changes.
 *
 * With these changes in place, I can now get 60fps for a full screen of SimpleLayout
 * text on an iPad.
 */
template <typename T>
inline void FTSimpleLayoutImpl::WrapTextI(const T *buf, const int len,
                                          FTPoint position, int renderMode,
                                          FTBBox *bounds)
{
	// points to the last break character
	// A break character is a white space character or a new line
	// we have to remember this because we don't hyphenate words, so if they
	// exceed our line length limit, then we must put the whole word on the
	// next line.
    FTUnicodeStringItr<T> breakItr(buf);
	
    FTUnicodeStringItr<T> lineStart(buf); // points to the line start
	

    float nextStart = 0.0;     // total width of the current line
    float breakWidth = 0.0;    // width of the line up to the last word break
    float currentWidth = 0.0;  // width of all characters on the current line
    float prevWidth;           // width of all characters but the current glyph
    float wordLength = 0.0;    // length of the block since the last break char
    int charCount = 0;         // number of characters so far on the line
    int breakCharCount = 0;    // number of characters before the breakItr
    float glyphWidth, advance;
    FTBBox glyphBounds;
	bool refreshCache = false;
	unsigned int new_stringCacheCount = 0; // length of buffer
	
    // Reset the pen position
    pen.Y(0);
	
    // If we have bounds mark them invalid
    if(bounds)
    {
		bounds->Invalidate();
    }
	
	{
		// Count up length of our given string, so we can compare with our 
		// cached string length.
		FTUnicodeStringItr<T> itr(buf);
		for (; *itr; ++itr) {}
		new_stringCacheCount = itr.getBufferFromHere() - buf + sizeof(T)/*terminator*/;
	}

	// Check if the incoming string is different to the previously
	// cached string.
	if (stringCache && stringCacheCount == new_stringCacheCount)
	{
		// cache length and new string length is the same, but check
		// if the value is different and the length just happened to be
		// the same.
		// TODO FIXME: Can't we just do this straight out? If we're going to be
		// doing it anyway... Also why the !!... Probably clearer to write != 0?
		refreshCache = !!memcmp(stringCache, buf, stringCacheCount);
	}
	else
	{
		// cache length and new string length were different, so we have to 
		// recreate our cache.
		refreshCache = true;
		stringCacheCount = new_stringCacheCount;
		
		if (!stringCache)
		{
			stringCache = (unsigned char *)malloc(stringCacheCount);
		}
		else
		{
			stringCache = (unsigned char *)realloc(stringCache, stringCacheCount);
		}

		if (!stringCache)
		{
			throw std::bad_alloc();
		}
	}
	
	// if our string has changed
	if (refreshCache)
	{
		// copy new string to our cache
		memcpy(stringCache, buf, stringCacheCount);
        layoutGlyphCache.clear();


		//
		//	RKTJMP: Disregard this commented out code, it is the start
		//			me re-implementing the existing code in a more readable 
		//			fashion.
		//
		
//		// this may be useful to understand the following code
//		// http://ftgl.sourceforge.net/docs/html/metrics.png
//		
//		float lineSoFarLength = 0.0;
//		float lineLengthWithCurrentGlyph = 0.0;
//		int lineCharCount = 0; // need this for the cache
//		
//		// we have to break on whole words, so we'll remember the last valid
//		// position we found that we could break from (this is generally
//		// a whitespace character after a word
//		FTUnicodeStringItr<T> breakPosition(buf);
//		// we need to remember where the start of the current line is so
//		// when we have to setup our cache, we'll know where to go from.
//		FTUnicodeStringItr<T> currentLineStart(buf);
//		
//		for (FTUnicodeStringItr<T> iter(buf); *iter; iter++)
//		{
//			//
//			// Find some details for our current glyph
//			//
//			
//			// Find the width of the current glyph (BBox(char *, len))
//			glyphBounds = currentFont->BBox(iter.getBufferFromHere(), 1);
//			glyphWidth = glyphBounds.Upper().Xf() - glyphBounds.Lower().Xf();
//			
//			// advance is the distance from a glyphs origin to where the
//			// next glyph would start, i.e. the total width of the glyph 
//			// (this is larger than just the visible area)
//			advance = currentFont->Advance(itr.getBufferFromHere(), 1);
//			
//			// find how long our line will be with this glyph
//			lineLengthWithCurrentGlyph = lineSoFarLength + glyphWidth
//			
//			if((lineLengthWithCurrentGlyph > lineLength) || (*iter == '\n'))
//			{
//				// line length with current glyph is over the limit
//				// or current glpyh is actually a new line, so force a break.
//
//				// create our cache item
//				layoutGlyphCacheItem_t cacheItem;
//				
//				
//				cacheItem.buf = stringCache + ptrdiff_t(lineStart.getBufferFromHere() - buf);
//				cacheItem.charCount = breakCharCount;
//				cacheItem.position = FTPoint(position.X(), position.Y(), position.Z());
//				cacheItem.remainingWidth = remainingWidth;
//				cacheItem.penDiff = FTPoint(0, currentFont->LineHeight() * lineSpacing);
//
//				
//				if(*iter == '\n'))
//				{
//					// new line, so just break right here.
//					cacheItem.position
//					
//				}
//				else
//				{
//					// line length with current glyph was over the limit
//					// so break on the last good break position we found
//					if(breakPosition == currentLineStart)
//					{
//						// We didn't find a break position for the current line
//						// so its still pointing to the start of the line
//						// so we have no choice but to break mid word.
//						// We actually want to break on the previous glyph since
//						// this one went over the limit.
//						
//						
//					}
//					
//				}
//				// we want to break on whole words, so find when the last 
//				
//			}
//			
//			
//			
//		}
//		
		
		// Scan the input for all characters that need output
		FTUnicodeStringItr<T> prevItr(buf);
		for (FTUnicodeStringItr<T> itr(buf); *itr; prevItr = itr++, charCount++)
		{
			// Find the width of the current glyph (BBox(char *, len))
			glyphBounds = currentFont->BBox(itr.getBufferFromHere(), 1);
			glyphWidth = glyphBounds.Upper().Xf() - glyphBounds.Lower().Xf();
			
			// advance is the distance from a glyphs origin to where the
			// next glyph would start, i.e. the total width of the glyph 
			// (larger than just the visible area)
			// http://ftgl.sourceforge.net/docs/html/metrics.png
			advance = currentFont->Advance(itr.getBufferFromHere(), 1);
			
			prevWidth = currentWidth; // save the previous total width
			
			// Compute the width of all glyphs up to the end of buf[i]
			// Find the current lines width, including this glyph
			currentWidth = nextStart + glyphWidth;
			
			// Compute the position of the next glyph
			nextStart += advance;
			
			// See if the current character is a space, a break or a regular character
			if((currentWidth > lineLength) || (*itr == '\n'))
			{	
				// A non whitespace character has exceeded the line length.  Or a
				// newline character has forced a line break.
				// (Whitespace characters are handled in the else)
				// Output the last line and start a new line after the break 
				// character.
				
				// If we have not yet found a break, break on the previous character
				// If current character is a new line, break on the previous character
				if(breakItr == lineStart || (*itr == '\n'))
				{
					// Break on the previous character
					breakItr = prevItr;
					if(*breakItr == '\n')
					{
						// handle \n\n
						breakCharCount = 0;
						breakWidth = 0;
						wordLength = 0;
						advance = 0;
					}
					else
					{
						breakCharCount = charCount - 1;
						breakWidth = prevWidth;
						// None of the previous words will be carried to the next line
						wordLength = 0;
						// If the current character is a newline discard its advance
						if(*itr == '\n') advance = 0;
					}
				}
				
				float remainingWidth = lineLength - breakWidth;
				
				// Render the current substring
				FTUnicodeStringItr<T> breakChar = breakItr;
				// move past the break character and don't count it on the next line either
				++breakChar; --charCount;
				// If the break character is a newline do not render it
				if(*breakChar == '\n')
				{
					++breakChar; 
					--charCount;
				}
				
				// outputwrapped takes a length argument, if that lenght is
				// -1, then it assumes you mean "the whole string"
				// if we get more than 1 \n in a row, our breakCharCount
				// can end up at -1, meaning we get the "whole string" outputted
				// which ends up putting the \n glyph on screen.
				// Check to guard against that here.
				if(breakCharCount < 0)
				{
					breakCharCount = 0;
				}
				
				// create our cache for this printable line
				layoutGlyphCacheItem_t cacheItem;
				cacheItem.buf = stringCache + ptrdiff_t(lineStart.getBufferFromHere() - buf);
				cacheItem.charCount = breakCharCount;
				cacheItem.position = FTPoint(position.X(), position.Y(), position.Z());
				cacheItem.remainingWidth = remainingWidth;
				cacheItem.penDiff = FTPoint(0, currentFont->LineHeight() * lineSpacing);
				// save the cache item
				layoutGlyphCache.push_back(cacheItem);
				
				lineStart = breakChar;
				// The current width is the width since the last break
				nextStart = wordLength + advance;
				wordLength += advance;
				currentWidth = wordLength + advance;
				// Reset the safe break for the next line
				breakItr = lineStart;
				charCount -= breakCharCount;
			}
			else if(iswspace(*itr))
			{
				// This is the last word break position
				wordLength = 0;
				breakItr = itr;
				breakCharCount = charCount;
				
				// Check to see if this is the first whitespace character in a run
				if(buf == itr.getBufferFromHere() || !iswspace(*prevItr))
				{
					// Record the width of the start of the block
					breakWidth = currentWidth;
				}
			}
			else
			{
				wordLength += advance;
			}
		}
		
		float remainingWidth = lineLength - currentWidth;
		// Render any remaining text on the last line
		// Disable justification for the last row
		if(alignment == FTGL::ALIGN_JUSTIFY)
		{
			alignment = FTGL::ALIGN_LEFT;
			layoutGlyphCacheItem_t cacheItem;
			cacheItem.buf = stringCache + ptrdiff_t(lineStart.getBufferFromHere() - buf);
			cacheItem.charCount = -1;
			cacheItem.position = FTPoint(position.X(), position.Y(), position.Z());
			cacheItem.penDiff = FTPoint(0,0,0);
			cacheItem.remainingWidth = remainingWidth;
			layoutGlyphCache.push_back(cacheItem);
			alignment = FTGL::ALIGN_JUSTIFY;
		}
		else
		{
			layoutGlyphCacheItem_t cacheItem;
			cacheItem.buf = stringCache + ptrdiff_t(lineStart.getBufferFromHere() - buf);
			cacheItem.charCount = -1;
			cacheItem.position = FTPoint(position.X(), position.Y(), position.Z());
			cacheItem.penDiff = FTPoint(0,0,0);
			cacheItem.remainingWidth = remainingWidth;
			layoutGlyphCache.push_back(cacheItem);
		}
	}
	
	// Draw each of the glyphs in the cache.
	if(bounds == NULL && layoutGlyphCache.size() != 0)
	{	
		// bounds is NULL, so we're actually going to output the text
		// so do our pre-render setup
		currentFont->PreRender();
	}
	    std::list<layoutGlyphCacheItem_t>::iterator it;
		for (it = layoutGlyphCache.begin(); it != layoutGlyphCache.end(); it++)
		{
			layoutGlyphCacheItem_t cacheItem = (*it);
			
			// If we have \n\n, some of our cache items will just be '\n'
			// so check if thats the case, if it is, don't output but do 
			// move the pen position just line normal. 
			// I did try checking charCount == 0, which is what I set \n items 
			// to but some 1 char long strings were not getting outputted...
			// Must be some edge case in FTGL that I don't understand.
			if(*(T*)(cacheItem.buf) != '\n')
			{
				OutputWrapped((T*)cacheItem.buf, 
							  cacheItem.charCount,
							  cacheItem.position,
							  renderMode,
							  cacheItem.remainingWidth,
							  bounds);
			}
			pen -= cacheItem.penDiff;
		}
	if(bounds == NULL && layoutGlyphCache.size() != 0)
	{	
		// we did output stuff, so do our post-render tear down.
		currentFont->PostRender();
	}
}


void FTSimpleLayoutImpl::WrapText(const char *buf, const int len,
                                  FTPoint position, int renderMode,
                                  FTBBox *bounds)
{
    WrapTextI(buf, len, position, renderMode, bounds);
}


void FTSimpleLayoutImpl::WrapText(const wchar_t* buf, const int len,
                                  FTPoint position, int renderMode,
                                  FTBBox *bounds)
{
    WrapTextI(buf, len, position, renderMode, bounds);
}


template <typename T>
inline void FTSimpleLayoutImpl::OutputWrappedI(const T *buf, const int len,
                                               FTPoint position, int renderMode,
                                               const float remaining,
                                               FTBBox *bounds)
{
    float distributeWidth = 0.0;
    // Align the text according as specified by Alignment
    switch (alignment)
    {
        case FTGL::ALIGN_LEFT:
            pen.X(0);
            break;
        case FTGL::ALIGN_CENTER:
            pen.X(remaining / 2);
            break;
        case FTGL::ALIGN_RIGHT:
            pen.X(remaining);
            break;
        case FTGL::ALIGN_JUSTIFY:
            pen.X(0);
            distributeWidth = remaining;
            break;
    }
	
    // If we have bounds expand them by the line's bounds, otherwise render
    // the line.
    if(bounds)
    {
        FTBBox temp = currentFont->BBox(buf, len);
		
        // Add the extra space to the upper x dimension
        temp = FTBBox(temp.Lower() + pen,
                      temp.Upper() + pen + FTPoint(distributeWidth, 0));
		
        // See if this is the first area to be added to the bounds
        if(bounds->IsValid())
        {
            *bounds |= temp;
        }
        else
        {
            *bounds = temp;
        }
    }
    else
    {
        RenderSpace(buf, len, position, renderMode, distributeWidth);
    }
}


void FTSimpleLayoutImpl::OutputWrapped(const char *buf, const int len,
                                       FTPoint position, int renderMode,
                                       const float remaining, FTBBox *bounds)
{
    OutputWrappedI(buf, len, position, renderMode, remaining, bounds);
}


void FTSimpleLayoutImpl::OutputWrapped(const wchar_t *buf, const int len,
                                       FTPoint position, int renderMode,
                                       const float remaining, FTBBox *bounds)
{
    OutputWrappedI(buf, len, position, renderMode, remaining, bounds);
}


template <typename T>
inline void FTSimpleLayoutImpl::RenderSpaceI(const T *string, const int len,
                                             FTPoint position, int renderMode,
                                             const float extraSpace)
{
    float space = 0.0;
	
    // If there is space to distribute, count the number of spaces
    if(extraSpace > 0.0)
    {
        int numSpaces = 0;
		
        // Count the number of space blocks in the input
        FTUnicodeStringItr<T> prevItr(string), itr(string);
        for(int i = 0; ((len < 0) && *itr) || ((len >= 0) && (i <= len));
            ++i, prevItr = itr++)
        {
            // If this is the end of a space block, increment the counter
            if((i > 0) && !iswspace(*itr) && iswspace(*prevItr))
            {
                numSpaces++;
            }
        }
		
        space = extraSpace/numSpaces;
    }
	
	// Output all characters of the string
    FTUnicodeStringItr<T> prevItr(string), itr(string);
    for(int i = 0; ((len < 0) && *itr) || ((len >= 0) && (i <= len));
        ++i, prevItr = itr++)
    {
        // If this is the end of a space block, distribute the extra space
        // inside it
        if((i > 0) && !iswspace(*itr) && iswspace(*prevItr))
        {
            pen += FTPoint(space, 0);
        }
		
		/**
		 * We want to minimise repeated calls to this function -- is it possible to
		 * set up a cache using Advance?
		 */
		pen = currentFont->Render(itr.getBufferFromHere(), 1, pen, FTPoint(), renderMode);
    }
}


void FTSimpleLayoutImpl::RenderSpace(const char *string, const int len,
                                     FTPoint position, int renderMode,
                                     const float extraSpace)
{
    RenderSpaceI(string, len, position, renderMode, extraSpace);
}


void FTSimpleLayoutImpl::RenderSpace(const wchar_t *string, const int len,
                                     FTPoint position, int renderMode,
                                     const float extraSpace)
{
    RenderSpaceI(string, len, position, renderMode, extraSpace);
}

