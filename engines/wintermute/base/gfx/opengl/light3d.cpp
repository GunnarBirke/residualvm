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


#include "light3d.h"
#include "../../../math/math_util.h"
#include "../../../wintypes.h"
#include "math/glmath.h"
#include "loader3ds.h"

namespace Wintermute {

//////////////////////////////////////////////////////////////////////////
Light3D::Light3D(BaseGame* inGame): BaseScriptable(inGame, false, false) {
	_diffuseColor = BYTETORGBA(255, 255, 255, 255);
	_position = Math::Vector3d(0, 0, 0);
	_target = Math::Vector3d(0, 0, 0);
	_isSpotlight = false;
	_falloff = 0;
	_active = true;

	_distance = 0.0f;
	_isAvailable = false;
}


//////////////////////////////////////////////////////////////////////////
Light3D::~Light3D() {

}


//////////////////////////////////////////////////////////////////////////
bool Light3D::setLight(int Index) {
	//Implement this later

//	LPDIRECT3DDEVICE Device = ((CBRenderD3D*)Game->m_Renderer)->m_Device;

//	D3DLIGHT d3dLight;
//	ZeroMemory(&d3dLight, sizeof(D3DLIGHT));

//	d3dLight.Type = m_IsSpotlight ? D3DLIGHT_SPOT : D3DLIGHT_POINT;
//	d3dLight.Diffuse.r = (float)D3DCOLGetR(m_DiffuseColor) / 256.0f;
//	d3dLight.Diffuse.g = (float)D3DCOLGetG(m_DiffuseColor) / 256.0f;
//	d3dLight.Diffuse.b = (float)D3DCOLGetB(m_DiffuseColor) / 256.0f;

//	d3dLight.Position.x = m_Pos.x;
//	d3dLight.Position.y = m_Pos.y;
//	d3dLight.Position.z = m_Pos.z;
	
//	if(m_IsSpotlight)
//	{
//		Math::Vector3d Dir = m_Target - m_Pos;
//		d3dLight.Direction = Dir;
//		d3dLight.Range = D3DXVec3Length(&Dir);

//		d3dLight.Theta        = 0.5f;
//		d3dLight.Phi          = 1.0f;
//		d3dLight.Falloff      = 1.0f;
//		d3dLight.Attenuation0 = 1.0f;
//	}
//	else
//	{
//		d3dLight.Range = 100000.0f;
//		d3dLight.Attenuation0 = 1.0f;
//	}
	
//	bool ret = Device->SetLight(Index, &d3dLight);

//	if(m_Active)
//		Device->LightEnable(Index, TRUE);

//	return ret;
	return true;
}

bool Light3D::loadFrom3DS(byte **buffer) {
	uint32 whole_chunk_size = *reinterpret_cast<uint32 *>(*buffer);
	byte *end = *buffer + whole_chunk_size - 2;
	*buffer += 4;

	_position.x() = *reinterpret_cast<float*>(*buffer);
	*buffer += 4;
	_position.z() = *reinterpret_cast<float*>(*buffer);
	*buffer += 4;
	_position.y() = *reinterpret_cast<float*>(*buffer);
	*buffer += 4;

	while (*buffer < end) {
		uint16 chunk_id = *reinterpret_cast<uint16 *>(*buffer);

		switch (chunk_id) {
		case SPOTLIGHT:
			*buffer += 6;

			_target.x() = *reinterpret_cast<float *>(*buffer);
			*buffer += 4;
			_target.z() = *reinterpret_cast<float *>(*buffer);
			*buffer += 4;
			_target.y() = *reinterpret_cast<float *>(*buffer);
			*buffer += 4;

			// this is appearently not used
			*buffer += 4;

			_falloff = *reinterpret_cast<float *>(*buffer);
			*buffer += 4;

			_isSpotlight = true;
			break;

		case LIGHT_IS_OFF:
			*buffer += 6;

			_active = false;
			break;

		case RGB_BYTE: {
			*buffer += 6;

			byte r = **buffer;
			*buffer += 1;
			byte g = **buffer;
			*buffer += 1;
			byte b = **buffer;
			*buffer += 1;

			_diffuseColor = r;
			_diffuseColor |= g << 8;
			_diffuseColor |= b << 16;
			_diffuseColor |= 255 << 24;
			break;
		}

		case RGB_FLOAT: {
			*buffer += 6;

			float r = *reinterpret_cast<float *>(*buffer);
			*buffer += 4;
			float g = *reinterpret_cast<float *>(*buffer);
			*buffer += 4;
			float b = *reinterpret_cast<float *>(*buffer);
			*buffer += 4;

			_diffuseColor = static_cast<int32>(r * 255);
			_diffuseColor |= static_cast<int32>(g * 255) << 8;
			_diffuseColor |= static_cast<int32>(b * 255) << 16;
			_diffuseColor |= 255 << 24;
			break;
		}

		case RANGE_END:
		case 0x4659:
		case MULTIPLIER:
		case ROLL:
		case SPOT_SHADOW_MAP:
		case SPOT_RAY_TRACE_BIAS:
		case SPOT_RAY_TRACE:
			*buffer += *reinterpret_cast<uint32*>(*buffer + 2);
		default:
			break;
		}
	}

	return true;
}


//////////////////////////////////////////////////////////////////////////
bool Light3D::getViewMatrix(Math::Matrix4 *viewMatrix) {
	Math::Vector3d up = Math::Vector3d(0.0f, 1.0f, 0.0f);
	*viewMatrix = Math::makeLookAtMatrix(_position, _target, up);
	return true;
}


//////////////////////////////////////////////////////////////////////////
bool Light3D::persist(BasePersistenceManager* persistMgr) {
	persistMgr->transferBool("_active", &_active);
	persistMgr->transferUint32("_diffuseColor", &_diffuseColor);
	return true;
}

}
