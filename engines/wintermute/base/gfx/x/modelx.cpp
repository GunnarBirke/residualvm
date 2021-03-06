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

/*
 * This file is based on WME.
 * http://dead-code.org/redir.php?target=wme
 * Copyright (c) 2003-2013 Jan Nedoma and contributors
 */

#include "engines/wintermute/base/base_file_manager.h"
#include "engines/wintermute/base/base_game.h"
#include "engines/wintermute/base/base_parser.h"
#include "engines/wintermute/base/gfx/base_renderer.h"
#include "engines/wintermute/base/gfx/opengl/base_render_opengl3d.h"
#include "engines/wintermute/base/gfx/x/frame_node.h"
#include "engines/wintermute/base/gfx/x/modelx.h"
#include "engines/wintermute/base/gfx/x/loader_x.h"
#include "engines/wintermute/dcgf.h"
#include "engines/wintermute/utils/path_util.h"
#include "engines/wintermute/utils/utils.h"

namespace Wintermute {

IMPLEMENT_PERSISTENT(ModelX, false)

//////////////////////////////////////////////////////////////////////////
ModelX::ModelX(BaseGame *inGame, BaseObject *owner) : BaseObject(inGame),
                                                      _owner(owner), _lastOffsetX(0), _lastOffsetY(0),
                                                      _BBoxStart(0.0f, 0.0f, 0.0f), _BBoxEnd(0.0f, 0.0f, 0.0f),
                                                      _rootFrame(nullptr) {
	_drawingViewport.setEmpty();
	_lastWorldMat.setToIdentity();
	_lastViewMat.setToIdentity();
	_lastProjMat.setToIdentity();
	_boundingRect.setEmpty();

	for (int i = 0; i < X_NUM_ANIMATION_CHANNELS; i++) {
		_channels[i] = nullptr;
	}

	_ticksPerSecond = kDefaultTicksPerSecond;
}

//////////////////////////////////////////////////////////////////////////
ModelX::~ModelX() {
	cleanup();
}

//////////////////////////////////////////////////////////////////////////
void ModelX::cleanup(bool complete) {
	// empty animation channels
	for (int i = 0; i < X_NUM_ANIMATION_CHANNELS; i++) {
		delete _channels[i];
		_channels[i] = nullptr;
	}

	// remove animation sets
	for (uint32 i = 0; i < _animationSets.size(); i++) {
		delete _animationSets[i];
	}

	_animationSets.clear();

	for (uint32 i = 0; i < _matSprites.size(); i++) {
		delete _matSprites[i];
		_matSprites[i] = nullptr;
	}

	_matSprites.clear();

	// remove root frame
	delete _rootFrame;
	_rootFrame = nullptr;

	_parentModel = nullptr;

	_ticksPerSecond = kDefaultTicksPerSecond;
}

//////////////////////////////////////////////////////////////////////////
bool ModelX::loadFromFile(const Common::String &filename, ModelX *parentModel) {
	cleanup(false);

	uint32 fileSize = 0;
	byte *buffer = BaseFileManager::getEngineInstance()->getEngineInstance()->readWholeFile(filename, &fileSize);

	byte *dataFormatBlock = buffer + 8;

	bool textMode = strcmp((char *)dataFormatBlock, "txt");

	if (strcmp((char *)dataFormatBlock, "bzip") == 0 || strcmp((char *)dataFormatBlock, "tzip") == 0) {
		warning("ModelX::loadFromFile compressed .X files are not supported yet");
	}

	XFileLexer lexer(buffer + 16, fileSize, textMode);

	bool res = true;

	_parentModel = parentModel;
	_rootFrame = new FrameNode(_gameRef);
	res = _rootFrame->loadFromXAsRoot(filename, lexer, this);
	setFilename(filename.c_str());

	for (int i = 0; i < X_NUM_ANIMATION_CHANNELS; ++i) {
		_channels[i] = new AnimationChannel(_gameRef, this);
	}

	if (res) {
		findBones(false, parentModel);
	}

	return res;
}

//////////////////////////////////////////////////////////////////////////
bool ModelX::loadAnimationSet(XFileLexer &lexer, const Common::String &filename) {
	bool res = true;

	AnimationSet *animSet = new AnimationSet(_gameRef, this);

	if (animSet->loadFromX(lexer, filename)) {
		_animationSets.add(animSet);
	} else {
		delete animSet;
		res = false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool ModelX::loadAnimation(const Common::String &filename, AnimationSet *parentAnimSet) {
	// not sure if we need this here (not completely implemented anyways and also not called)
	// are there animation objects in .X outside of an animation set?

	// if no parent anim set is specified, create one
	bool newAnimSet = false;
	if (parentAnimSet == nullptr) {
		parentAnimSet = new AnimationSet(_gameRef, this);

		parentAnimSet->setName(PathUtil::getFileName(filename).c_str());
		newAnimSet = true;
	}

	// create the new object
	Animation *Anim = new Animation(_gameRef);

	parentAnimSet->addAnimation(Anim);

	if (newAnimSet) {
		_animationSets.add(parentAnimSet);
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool ModelX::findBones(bool animOnly, ModelX *parentModel) {
	FrameNode *rootFrame;
	if (parentModel == nullptr)
		rootFrame = _rootFrame;
	else
		rootFrame = parentModel->getRootFrame();

	if (rootFrame && !animOnly) {
		_rootFrame->findBones(rootFrame);
	}

	for (uint32 i = 0; i < _animationSets.size(); i++) {
		_animationSets[i]->findBones(rootFrame);
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool ModelX::update() {
	// reset all bones to default position
	reset();

	// update all animation channels
	for (int i = 0; i < X_NUM_ANIMATION_CHANNELS; i++) {
		_channels[i]->update(i == 1);
	}

	// update matrices
	if (_rootFrame) {
		Math::Matrix4 tempMat;
		tempMat.setToIdentity();
		_rootFrame->updateMatrices(tempMat);

		return _rootFrame->updateMeshes();
	} else {
		return false;
	}
}

//////////////////////////////////////////////////////////////////////////
bool ModelX::playAnim(int channel, const Common::String &name, uint32 transitionTime, bool forceReset, uint32 stopTransitionTime) {
	if (channel < 0 || channel >= X_NUM_ANIMATION_CHANNELS) {
		return false;
	}

	// are we already playing this animation?
	if (!forceReset) {
		if (_channels[channel]->getName() && name.equalsIgnoreCase(_channels[channel]->getName())) {
			return true;
		}
	}

	// find animation set by name
	AnimationSet *anim = getAnimationSetByName(name);

	if (anim) {
		char *currentAnim = _channels[channel]->getName();
		if (_owner && currentAnim) {
			// clean this up later
			transitionTime = _owner->getAnimTransitionTime(currentAnim, const_cast<char *>(name.c_str()));
		}

		return _channels[channel]->playAnim(anim, transitionTime, stopTransitionTime);
	} else {
		return false;
	}
}

//////////////////////////////////////////////////////////////////////////
bool ModelX::stopAnim(int channel, uint32 transitionTime) {
	if (channel < 0 || channel >= X_NUM_ANIMATION_CHANNELS) {
		return false;
	}

	return _channels[channel]->stopAnim(transitionTime);
}

//////////////////////////////////////////////////////////////////////////
bool ModelX::stopAnim(uint32 transitionTime) {
	const int NUM_SKEL_ANI_CHANNELS = 10;

	for (int channel = 0; channel < NUM_SKEL_ANI_CHANNELS; channel++) {
		stopAnim(channel, transitionTime);
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool ModelX::reset() {
	if (_rootFrame) {
		_rootFrame->resetMatrices();
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool ModelX::isAnimPending(int channel, const char *animName) {
	if (!animName) {
		if (_channels[channel]->isPlaying()) {
			return true;
		}
	} else {
		if (_channels[channel]->isPlaying() && _channels[channel]->getName() && scumm_stricmp(animName, _channels[channel]->getName()) == 0) {
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool ModelX::isAnimPending(char *animName) {
	for (int channel = 0; channel < X_NUM_ANIMATION_CHANNELS; channel++) {
		if (isAnimPending(channel, animName)) {
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool ModelX::updateShadowVol(ShadowVolume *shadow, Math::Matrix4 &modelMat, const Math::Vector3d &light, float extrusionDepth) {
	if (_rootFrame) {
		return _rootFrame->updateShadowVol(shadow, modelMat, light, extrusionDepth);
	} else {
		return false;
	}
}

//////////////////////////////////////////////////////////////////////////
bool ModelX::render() {
	if (_rootFrame) {
		// set culling
		//		if(m_Owner && !m_Owner->m_DrawBackfaces)
		//			Rend->m_Device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
		//		else
		//			Rend->m_Device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

		//		// gameSpace stores colors in vertices, disable for now
		//		Rend->m_Device->SetRenderState(D3DRS_COLORVERTEX, FALSE);

		// render everything
		bool res = _rootFrame->render(this);

		// remember matrices for object picking purposes
		//		Rend->m_Device->GetTransform(D3DTS_WORLD,      &lastWorldMat);
		//		Rend->m_Device->GetTransform(D3DTS_VIEW,       &_lastViewMat);
		//		Rend->m_Device->GetTransform(D3DTS_PROJECTION, &_lastProjMat);

		// remember scene offset
		Rect32 rc;
		_gameRef->getCurrentViewportRect(&rc);
		float width = (float)rc.right - (float)rc.left;
		float height = (float)rc.bottom - (float)rc.top;

		// margins
		int mleft = rc.left;
		int mright = _gameRef->_renderer->getWidth() - width - rc.left;
		int mtop = rc.top;
		int mbottom = _gameRef->_renderer->getHeight() - height - rc.top;

		_lastOffsetX = _gameRef->_offsetX + (mleft - mright) / 2;
		_lastOffsetY = _gameRef->_offsetY + (mtop - mbottom) / 2;

		// update bounding box and 2D bounding rectangle
		updateBoundingRect();

		return res;
	} else {
		return false;
	}
}

//////////////////////////////////////////////////////////////////////////
Math::Matrix4 *ModelX::getBoneMatrix(const char *boneName) {
	FrameNode *bone = _rootFrame->findFrame(boneName);

	if (bone) {
		return bone->getCombinedMatrix();
	} else {
		return nullptr;
	}
}

//////////////////////////////////////////////////////////////////////////
FrameNode *ModelX::getRootFrame() {
	return _rootFrame;
}

//////////////////////////////////////////////////////////////////////////
bool ModelX::isTransparentAt(int x, int y) {
	if (!_rootFrame) {
		return false;
	}

	x += _lastOffsetX;
	y += _lastOffsetY;

	//	if (!Rend->m_Camera) {
	//		return true;
	//	}

//	float resWidth, resHeight;
//	float layerWidth, layerHeight;
	float modWidth = 0.0f;
	float modHeight = 0.0f;
	bool customViewport = false;
	//	Rend->GetProjectionParams(&ResWidth, &ResHeight, &LayerWidth, &LayerHeight, &ModWidth, &ModHeight, &CustomViewport);

	x -= _drawingViewport.left + modWidth;
	y -= _drawingViewport.top + modHeight;

	if (customViewport) {
		x += _gameRef->_renderer->_drawOffsetX;
		y += _gameRef->_renderer->_drawOffsetY;
	}

	Math::Vector3d pickRayDir;
	Math::Vector3d pickRayOrig;

	// Compute the vector of the pick ray in screen space
	Math::Vector3d vec;
	vec.x() = (((2.0f * x) / _drawingViewport.width()) - 1) / _lastProjMat(0, 0);
	vec.y() = -(((2.0f * y) / _drawingViewport.height()) - 1) / _lastProjMat(1, 1);
	vec.z() = 1.0f;

	// Get the inverse view matrix
	Math::Matrix4 m = _lastViewMat;
	m.inverse();

	// Transform the screen space pick ray into 3D space
	// TODO: Pretty sure this is not correct, since it assumes a Direct3D coordinate system
	pickRayDir.x() = vec.x() * m(0, 0) + vec.y() * m(1, 0) + vec.z() * m(2, 0);
	pickRayDir.y() = vec.x() * m(0, 1) + vec.y() * m(1, 1) + vec.z() * m(2, 1);
	pickRayDir.z() = vec.x() * m(0, 2) + vec.y() * m(1, 2) + vec.z() * m(2, 2);
	pickRayOrig.x() = m(3, 0);
	pickRayOrig.y() = m(3, 1);
	pickRayOrig.z() = m(3, 2);

	// transform to model space
	Math::Vector3d end = pickRayOrig + pickRayDir;
	m = _lastWorldMat;
	m.inverse();
	m.transform(&pickRayOrig, false);
	m.transform(&end, false);
	pickRayDir = end - pickRayOrig;

	return !_rootFrame->pickPoly(&pickRayOrig, &pickRayDir);
}

//////////////////////////////////////////////////////////////////////////
void ModelX::updateBoundingRect() {
	_BBoxStart = _BBoxEnd = Math::Vector3d(0, 0, 0);

	if (_rootFrame) {
		_rootFrame->getBoundingBox(&_BBoxStart, &_BBoxEnd);
	}

	//	m_BoundingRect.left = m_BoundingRect.top = INT_MAX;
	//	m_BoundingRect.right = m_BoundingRect.bottom = INT_MIN;

	//	CBRenderD3D* Rend = (CBRenderD3D*)Game->m_Renderer;
	//	LPDIRECT3DDEVICE dev = Rend->m_Device;

	//	D3DXMATRIX viewMat, projMat, worldMat;
	//	Math::Vector3d vec2d(0,0,0);
	//	dev->GetTransform(D3DTS_VIEW, &viewMat);
	//	dev->GetTransform(D3DTS_PROJECTION, &projMat);
	//	dev->GetTransform(D3DTS_WORLD, &worldMat);

	//	dev->GetViewport(&_drawingViewport);

	//	float x1 = m_BBoxStart.x;
	//	float x2 = m_BBoxEnd.x;
	//	float y1 = m_BBoxStart.y;
	//	float y2 = m_BBoxEnd.y;
	//	float z1 = m_BBoxStart.z;
	//	float z2 = m_BBoxEnd.z;

	//	D3DXVec3Project(&vec2d, &Math::Vector3d(x1,y1,z1), &_drawingViewport, &projMat, &viewMat, &worldMat);
	//	UpdateRect(&m_BoundingRect, &vec2d);
	//	D3DXVec3Project(&vec2d, &Math::Vector3d(x2,y1,z1), &_drawingViewport, &projMat, &viewMat, &worldMat);
	//	UpdateRect(&m_BoundingRect, &vec2d);
	//	D3DXVec3Project(&vec2d, &Math::Vector3d(x1,y1,z2), &_drawingViewport, &projMat, &viewMat, &worldMat);
	//	UpdateRect(&m_BoundingRect, &vec2d);
	//	D3DXVec3Project(&vec2d, &Math::Vector3d(x2,y1,z2), &_drawingViewport, &projMat, &viewMat, &worldMat);
	//	UpdateRect(&m_BoundingRect, &vec2d);

	//	D3DXVec3Project(&vec2d, &Math::Vector3d(x1,y2,z1), &_drawingViewport, &projMat, &viewMat, &worldMat);
	//	UpdateRect(&m_BoundingRect, &vec2d);
	//	D3DXVec3Project(&vec2d, &Math::Vector3d(x2,y2,z1), &_drawingViewport, &projMat, &viewMat, &worldMat);
	//	UpdateRect(&m_BoundingRect, &vec2d);
	//	D3DXVec3Project(&vec2d, &Math::Vector3d(x1,y2,z2), &_drawingViewport, &projMat, &viewMat, &worldMat);
	//	UpdateRect(&m_BoundingRect, &vec2d);
	//	D3DXVec3Project(&vec2d, &Math::Vector3d(x2,y2,z2), &_drawingViewport, &projMat, &viewMat, &worldMat);
	//	UpdateRect(&m_BoundingRect, &vec2d);

	//	m_BoundingRect.left -= Rend->m_DrawOffsetX;
	//	m_BoundingRect.right -= Rend->m_DrawOffsetX;
	//	m_BoundingRect.bottom -= Rend->m_DrawOffsetY;
	//	m_BoundingRect.top -= Rend->m_DrawOffsetY;
}

//////////////////////////////////////////////////////////////////////////
void ModelX::updateRect(Rect32 *rc, Math::Vector3d *vec) {
	//	rc->left   = min(rc->left,   vec->x());
	//	rc->right  = max(rc->right,  vec->x());
	//	rc->top    = min(rc->top,    vec->y());
	//	rc->bottom = max(rc->bottom, vec->y());
}

//////////////////////////////////////////////////////////////////////////
AnimationSet *ModelX::getAnimationSetByName(const Common::String &name) {
	for (uint32 i = 0; i < _animationSets.size(); i++) {
		if (name.equalsIgnoreCase(_animationSets[i]->_name)) {
			return _animationSets[i];
		}
	}

	return nullptr;
}

TOKEN_DEF_START
	TOKEN_DEF(NAME)
	TOKEN_DEF(LOOPING)
	TOKEN_DEF(EVENT)
	TOKEN_DEF(FRAME)
TOKEN_DEF_END
//////////////////////////////////////////////////////////////////////////
bool ModelX::parseAnim(byte *buffer) {
	TOKEN_TABLE_START(commands)
		TOKEN_TABLE(NAME)
		TOKEN_TABLE(LOOPING)
		TOKEN_TABLE(EVENT)
	TOKEN_TABLE_END

	byte *params;
	int cmd;
	BaseParser parser;

	char *name = nullptr;
	bool looping = false;
	bool loopingSet = false;

	while ((cmd = parser.getCommand((char **)&buffer, commands, (char **)&params)) > 0) {
		switch (cmd) {
		case TOKEN_NAME: {
			BaseUtils::setString(&name, (char *)params);

			AnimationSet *anim = getAnimationSetByName(name);
			if (!anim) {
				_gameRef->LOG(0, "Error: Animation '%s' cannot be found in the model.", name);
			}
			break;
		}

		case TOKEN_LOOPING:
			parser.scanStr((char *)params, "%b", &looping);
			loopingSet = true;
			break;

		case TOKEN_EVENT:
			if (!name) {
				_gameRef->LOG(0, "Error: NAME filed must precede any EVENT fields in actor definition files.");
			} else {
				AnimationSet *anim = getAnimationSetByName(name);
				if (anim)
					parseEvent(anim, params);
			}
			break;
		}
	}

	if (cmd != PARSERR_EOF) {
		return false;
	}

	bool ret = true;

	if (name) {
		AnimationSet *anim = getAnimationSetByName(name);
		if (anim) {
			if (loopingSet)
				anim->_looping = looping;
		}
	}

	delete[] name;

	return ret;
}

//////////////////////////////////////////////////////////////////////////
bool ModelX::parseEvent(AnimationSet *anim, byte *buffer) {
	TOKEN_TABLE_START(commands)
	TOKEN_TABLE(NAME)
	TOKEN_TABLE(FRAME)
	TOKEN_TABLE_END

	byte *params;
	int cmd;
	BaseParser parser;

	AnimationSet::AnimationEvent *event = new AnimationSet::AnimationEvent();
	if (!event) {
		return false;
	}

	while ((cmd = parser.getCommand((char **)&buffer, commands, (char **)&params)) > 0) {
		switch (cmd) {
		case TOKEN_NAME:
			BaseUtils::setString(&event->_eventName, (char *)params);
			break;

		case TOKEN_FRAME:
			parser.scanStr((char *)params, "%d", &event->_frame);
			break;
		}
	}

	if (cmd != PARSERR_EOF) {
		delete event;
		return false;
	}

	if (event->_eventName) {
		anim->addEvent(event);
	} else {
		delete event;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool ModelX::setMaterialSprite(const char *materialName, const char *spriteFilename) {
	if (!materialName || !spriteFilename) {
		return false;
	}

	if (!_rootFrame) {
		return false;
	}

	BaseSprite *sprite = new BaseSprite(_gameRef);
	if (!sprite || !sprite->loadFile(spriteFilename)) {
		delete sprite;
		return false;
	}

	ModelXMatSprite *matSprite = nullptr;
	for (uint32 i = 0; i < _matSprites.size(); i++) {
		if (scumm_stricmp(_matSprites[i]->_matName, materialName) == 0) {
			matSprite = _matSprites[i];
			break;
		}
	}
	if (matSprite) {
		matSprite->setSprite(sprite);
	} else {
		matSprite = new ModelXMatSprite(materialName, sprite);
		_matSprites.add(matSprite);
	}

	_rootFrame->setMaterialSprite(matSprite->_matName, matSprite->_sprite);

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool ModelX::setMaterialTheora(const char *materialName, const char *theoraFilename) {
	if (!materialName || !theoraFilename) {
		return false;
	}

	if (!_rootFrame) {
		return false;
	}

	VideoTheoraPlayer *theora = new VideoTheoraPlayer(_gameRef);

	if (!theora || theora->initialize(theoraFilename)) {
		delete theora;
		return false;
	}

	theora->play(VID_PLAY_POS, 0, 0, false, false, true);

	ModelXMatSprite *matSprite = nullptr;
	for (uint32 i = 0; i < _matSprites.size(); i++) {
		if (scumm_stricmp(_matSprites[i]->_matName, materialName) == 0) {
			matSprite = _matSprites[i];
			break;
		}
	}

	if (matSprite) {
		matSprite->setTheora(theora);
	} else {
		matSprite = new ModelXMatSprite(materialName, theora);
		_matSprites.add(matSprite);
	}
	_rootFrame->setMaterialTheora(matSprite->_matName, matSprite->_theora);

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool ModelX::initializeSimple() {
	if (!_rootFrame) {
		return false;
	}

	// init after load
	for (uint32 i = 0; i < _matSprites.size(); i++) {
		if (_matSprites[i]->_theora) {
			_rootFrame->setMaterialTheora(_matSprites[i]->_matName, _matSprites[i]->_theora);
		} else if (_matSprites[i]->_sprite) {
			_rootFrame->setMaterialSprite(_matSprites[i]->_matName, _matSprites[i]->_sprite);
		}
	}

	if (_parentModel) {
		findBones(false, _parentModel);
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool ModelX::persist(BasePersistenceManager *persistMgr) {
	BaseObject::persist(persistMgr);

	persistMgr->transferVector3d(TMEMBER(_BBoxStart));
	persistMgr->transferVector3d(TMEMBER(_BBoxEnd));
	persistMgr->transferRect32(TMEMBER(_boundingRect));

	if (!persistMgr->getIsSaving()) {
		_drawingViewport.setEmpty();
	}

	persistMgr->transferSint32(TMEMBER(_lastOffsetX));
	persistMgr->transferSint32(TMEMBER(_lastOffsetY));

	persistMgr->transferMatrix4(TMEMBER(_lastProjMat));
	persistMgr->transferMatrix4(TMEMBER(_lastViewMat));
	persistMgr->transferMatrix4(TMEMBER(_lastWorldMat));

	persistMgr->transferPtr(TMEMBER(_owner));

	// load model
	if (!persistMgr->getIsSaving()) {
		for (int i = 0; i < X_NUM_ANIMATION_CHANNELS; i++) {
			_channels[i] = nullptr;
		}

		_rootFrame = nullptr;

		if (getFilename()) {
			loadFromFile(getFilename());
		}
	}

	persistMgr->transferPtr(TMEMBER(_parentModel));

	// animation properties
	int numAnims;
	if (persistMgr->getIsSaving()) {
		numAnims = _animationSets.size();
	}

	persistMgr->transferSint32(TMEMBER(numAnims));

	if (persistMgr->getIsSaving()) {
		for (uint32 i = 0; i < _animationSets.size(); i++) {
			persistMgr->transferCharPtr(TMEMBER(_animationSets[i]->_name));
			_animationSets[i]->persist(persistMgr);
		}
	} else {
		for (int i = 0; i < numAnims; i++) {
			bool needsDelete = false;
			char *animName;
			persistMgr->transferCharPtr(TMEMBER(animName));
			AnimationSet *animSet = getAnimationSetByName(animName);
			if (!animSet) {
				animSet = new AnimationSet(_gameRef, this);
				needsDelete = true;
			}

			animSet->persist(persistMgr);
			if (needsDelete) {
				delete animSet;
			}

			delete[] animName;
		}
	}

	// persist channels
	for (int i = 0; i < X_NUM_ANIMATION_CHANNELS; i++) {
		_channels[i]->persist(persistMgr);
	}

	// persist material sprites
	int numMatSprites;
	if (persistMgr->getIsSaving()) {
		numMatSprites = _matSprites.size();
	}

	persistMgr->transferSint32(TMEMBER(numMatSprites));

	for (int i = 0; i < numMatSprites; i++) {
		if (persistMgr->getIsSaving()) {
			_matSprites[i]->persist(persistMgr);
		} else {
			ModelXMatSprite *MatSprite = new ModelXMatSprite();
			MatSprite->persist(persistMgr);
			_matSprites.add(MatSprite);
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool ModelX::invalidateDeviceObjects() {
	if (_rootFrame) {
		return _rootFrame->invalidateDeviceObjects();
	} else {
		return false;
	}
}

//////////////////////////////////////////////////////////////////////////
bool ModelX::restoreDeviceObjects() {
	if (_rootFrame) {
		return _rootFrame->restoreDeviceObjects();
	} else {
		return false;
	}
}

//////////////////////////////////////////////////////////////////////////
bool ModelX::unloadAnimation(const char *animName) {
	bool found = false;
	for (uint32 i = 0; i < _animationSets.size(); i++) {
		if (scumm_stricmp(animName, _animationSets[i]->_name) == 0) {
			for (int j = 0; j < X_NUM_ANIMATION_CHANNELS; j++) {
				if (_channels[j])
					_channels[j]->unloadAnim(_animationSets[i]);
			}

			found = true;
			delete _animationSets[i];
			_animationSets.remove_at(i);
			i++;
		}
	}
	return found;
}

} // namespace Wintermute
