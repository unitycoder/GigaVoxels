/*
 * GigaVoxels is a ray-guided streaming library used for efficient
 * 3D real-time rendering of highly detailed volumetric scenes.
 *
 * Copyright (C) 2011-2012 INRIA <http://www.inria.fr/>
 *
 * Authors : GigaVoxels Team
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** 
 * @version 1.0
 */

#ifndef _SHADER_KERNEL_H_
#define _SHADER_KERNEL_H_

/******************************************************************************
 ******************************* INCLUDE SECTION ******************************
 ******************************************************************************/

// GigaVoxels
#include <GvRendering/GvIRenderShader.h>
#include <GvUtils/GvCommonShaderKernel.h>

/******************************************************************************
 ************************* DEFINE AND CONSTANT SECTION ************************
 ******************************************************************************/

/**
 * ...
 */
__constant__ float3 cLightPosition;

/******************************************************************************
 ***************************** TYPE DEFINITION ********************************
 ******************************************************************************/

/******************************************************************************
 ******************************** CLASS USED **********************************
 ******************************************************************************/

/******************************************************************************
 ****************************** CLASS DEFINITION ******************************
 ******************************************************************************/

/** 
 * @class ShaderLoadKernel
 *
 * @brief The ShaderLoadKernel struct provides...
 *
 * ...
 */
class ShaderKernel
:	public GvUtils::GvCommonShaderKernel
,	public GvRendering::GvIRenderShader< ShaderKernel >
{

	/**************************************************************************
	 ***************************** PUBLIC SECTION *****************************
	 **************************************************************************/

public:

	/******************************* ATTRIBUTES *******************************/

	/******************************** METHODS *********************************/

	/**
	 * ...
	 *
	 * @param materialColor ...
	 * @param normalVec ...
	 * @param lightVec ...
	 * @param eyeVec ...
	 * @param ambientTerm ...
	 * @param diffuseTerm ...
	 * @param specularTerm ...
	 *
	 * @return ...
	 */
	__device__
	inline float3 shadePointLight( float3 materialColor, float3 normalVec, float3 lightVec, float3 eyeVec,
		float3 ambientTerm, float3 diffuseTerm, float3 specularTerm );

	/**
	 * ...
	 *
	 * @param brickSampler ...
	 * @param samplePosScene ...
	 * @param rayDir ...
	 * @param rayStep ...
	 * @param coneAperture ...
	 */
	template< typename BrickSamplerType >
	__device__
	inline void runImpl( const BrickSamplerType& brickSampler, const float3 samplePosScene,
		const float3 rayDir, float& rayStep, const float coneAperture );

	/**************************************************************************
	 **************************** PROTECTED SECTION ***************************
	 **************************************************************************/

protected:

	/******************************* ATTRIBUTES *******************************/

	/******************************** METHODS *********************************/

	/**************************************************************************
	 ***************************** PRIVATE SECTION ****************************
	 **************************************************************************/

private:

	/******************************* ATTRIBUTES *******************************/

	/******************************** METHODS *********************************/

};

/**************************************************************************
 ***************************** INLINE SECTION *****************************
 **************************************************************************/

#include "ShaderKernel.inl"

#endif // !_SHADER_KERNEL_H_
