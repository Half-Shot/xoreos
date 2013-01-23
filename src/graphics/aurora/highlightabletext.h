/* xoreos - A reimplementation of BioWare's Aurora engine
 *
 * xoreos is the legal property of its developers, whose names can be
 * found in the AUTHORS file distributed with this source
 * distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *
 * The Infinity, Aurora, Odyssey, Eclipse and Lycium engines, Copyright (c) BioWare corp.
 * The Electron engine, Copyright (c) Obsidian Entertainment and BioWare corp.
 */

#ifndef HIGHLIGHTABLE_TEXT_H
#define HIGHLIGHTABLE_TEXT_H

#include "graphics/aurora/text.h"

namespace Graphics {

namespace Aurora {

class HighlightableText: public Text {

  public:
	HighlightableText(const FontHandle &font, const Common::UString &str,
	     float r = 1.0, float g = 1.0, float b = 1.0, float a = 1.0, float align = 0.0);
	~HighlightableText();

	void render (RenderPass pass);

	bool isHightlighted();
	void setHighlighted(bool hightlighted);

	// This is how much the quad changes per render. Positive number increment the color, negative numbers decrement it.
	void setHighlightDelta(float r, float g, float b, float a);

	//When any of the quad properties are greater than this bound, the signs of the delta floats will flip
	void setHighlightUpperBound(float r, float g, float b, float a);

	//When any of the quad properties are less than this bound, the signs of the delta floats will flip
	void setHighlightLowerBound(float r, float g, float b, float a);

private:
	void flipHighlightDelta();

	bool _isHighlighted;

	float _deltaR;
	float _deltaG;
	float _deltaB;
	float _deltaA;

	float _upperBoundR;
	float _upperBoundG;
	float _upperBoundB;
	float _upperBoundA;

	float _lowerBoundR;
	float _lowerBoundG;
	float _lowerBoundB;
	float _lowerBoundA;
};

} // End of namespace Aurora

} // End of namespace Graphics

#endif // HIGHLIGHTABLE_TEXT_H