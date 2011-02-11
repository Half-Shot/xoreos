/* eos - A reimplementation of BioWare's Aurora engine
 * Copyright (c) 2010-2011 Sven Hesse (DrMcCoy), Matthew Hoops (clone2727)
 *
 * The Infinity, Aurora, Odyssey and Eclipse engines, Copyright (c) BioWare corp.
 * The Electron engine, Copyright (c) Obsidian Entertainment and BioWare corp.
 *
 * This file is part of eos and is distributed under the terms of
 * the GNU General Public Licence. See COPYING for more informations.
 */

/** @file graphics/images/screenshot.h
 *  Screenshot writing.
 */

#ifndef GRAPHICS_IMAGES_SCREENSHOT_H
#define GRAPHICS_IMAGES_SCREENSHOT_H

namespace Graphics {

/** Saves a screenshot to a file.
 *
 *  Warning: Unsafe outside the main thread.
 */
bool takeScreenshot();

} // End of namespace Graphics

#endif // GRAPHICS_IMAGES_SCREENSHOT_H