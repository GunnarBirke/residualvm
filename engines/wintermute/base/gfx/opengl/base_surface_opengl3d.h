/* ResidualVM - A 3D game interpreter
 *
 * ResidualVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef WINTERMUTE_BASE_SURFACE_OPENGL3D_H
#define WINTERMUTE_BASE_SURFACE_OPENGL3D_H

#include "../base_surface.h"
#include "../../../../../graphics/opengl/texture.h"

namespace Wintermute {

class BaseGame;
class BaseRenderOpenGL3D;

class BaseSurfaceOpenGL3D : public BaseSurface {
public:
	BaseSurfaceOpenGL3D(BaseGame* game, BaseRenderOpenGL3D* renderer);

	virtual bool invalidate();

	virtual bool displayHalfTrans(int x, int y, Rect32 rect);
	virtual bool isTransparentAt(int x, int y);
	virtual bool displayTransZoom(int x, int y, Rect32 rect, float zoomX, float zoomY, uint32 alpha = 0xFFFFFFFF, Graphics::TSpriteBlendMode blendMode = Graphics::BLEND_NORMAL, bool mirrorX = false, bool mirrorY = false);
	virtual bool displayTrans(int x, int y, Rect32 rect, uint32 alpha = 0xFFFFFFFF, Graphics::TSpriteBlendMode blendMode = Graphics::BLEND_NORMAL, bool mirrorX = false, bool mirrorY = false);
	virtual bool displayTransOffset(int x, int y, Rect32 rect, uint32 alpha = 0xFFFFFFFF, Graphics::TSpriteBlendMode blendMode = Graphics::BLEND_NORMAL, bool mirrorX = false, bool mirrorY = false, int offsetX = 0, int offsetY = 0);
	virtual bool display(int x, int y, Rect32 rect, Graphics::TSpriteBlendMode blendMode = Graphics::BLEND_NORMAL, bool mirrorX = false, bool mirrorY = false);
	virtual bool displayTransform(int x, int y, Rect32 rect, Rect32 newRect, const Graphics::TransformStruct &transform);
	virtual bool displayZoom(int x, int y, Rect32 rect, float zoomX, float zoomY, uint32 alpha = 0xFFFFFFFF, bool transparent = false, Graphics::TSpriteBlendMode blendMode = Graphics::BLEND_NORMAL, bool mirrorX = false, bool mirrorY = false);
	virtual bool displayTiled(int x, int y, Rect32 rect, int numTimesX, int numTimesY);
	virtual bool restore();
	virtual bool create(const Common::String &filename, bool defaultCK, byte ckRed, byte ckGreen, byte ckBlue, int lifeTime = -1, bool keepLoaded = false);
	virtual bool create(int width, int height);
	virtual bool putSurface(const Graphics::Surface &surface, bool hasAlpha = false) {
		return STATUS_FAILED;
	}
	virtual bool putPixel(int x, int y, byte r, byte g, byte b, int a = -1);
	virtual bool getPixel(int x, int y, byte *r, byte *g, byte *b, byte *a = nullptr);
	virtual bool comparePixel(int x, int y, byte r, byte g, byte b, int a = -1);
	virtual bool startPixelOp();
	virtual bool endPixelOp();
	virtual bool isTransparentAtLite(int x, int y);

	virtual int getWidth() {
		return tex->getWidth();
	}
	virtual int getHeight() {
		return tex->getHeight();
	}

private:
	OpenGL::Texture* tex;
	BaseRenderOpenGL3D* renderer;
	bool pixelOpReady;
};

}

#endif
