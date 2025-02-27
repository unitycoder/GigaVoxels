/*
 * GigaVoxels is a ray-guided streaming library used for efficient
 * 3D real-time rendering of highly detailed volumetric scenes.
 *
 * Copyright (C) 2011-2013 INRIA <http://www.inria.fr/>
 *
 * Authors : GigaVoxels Team
 *
 * GigaVoxels is distributed under a dual-license scheme.
 * You can obtain a specific license from Inria at gigavoxels-licensing@inria.fr.
 * Otherwise the default license is the GPL version 3.
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

/******************************************************************************
 ******************************* INCLUDE SECTION ******************************
 ******************************************************************************/

/******************************************************************************
 ******************************* CONSTANTES SECTION ***************************
 ******************************************************************************/

#define KA 0.2f // Ambiant reflection constant
#define KD 0.6f // Diffuse reflection constant
#define KS 1.2f // Specular reflection constant

/******************************************************************************
 ****************************** KERNEL DEFINITION *****************************
 ******************************************************************************/

/******************************************************************************
 * This method is called for each sample. For example, shading or secondary rays
 * should be done here.
 *
 * @param pBrickSampler brick sampler
 * @param pSamplePosScene position of the sample in the scene
 * @param pRayDir ray direction
 * @param pRayStep ray step
 * @param pConeAperture cone aperture
 ******************************************************************************/
template< typename SamplerType >
__device__
inline void ShaderKernel::runImpl( const SamplerType& brickSampler, const float3 samplePosScene,
										const float3 rayDir, float& rayStep, const float coneAperture )
{
	// Retrieve first channel element : color
	float4 color = brickSampler.template getValue< 0 >( coneAperture );

	// Test opacity
	if ( color.w > 0.0f )
	{
		// Retrieve second channel element : normal
		float4 normal = brickSampler.template getValue< 1 >( coneAperture );

		float3 normalVec = normalize( make_float3( normal.x, normal.y, normal.z ) );
		float3 lightVec = normalize( cLightPosition );
		float3 lightColor = make_float3 (1.0f, 1.0f, 1.0f);

		float3 rgb = make_float3(0.f);
		float3 ambiant = KA * lightColor;
		float3 lambert = KD * max( 0.0f, dot( normalVec, lightVec ) ) * lightColor;

		if ( cLightingType == PHONG ) {
			// Phong lighting
			float3 rayDirVec = normalize(rayDir);
			float3 reflection = normalize( 2 * dot( lightVec, normalVec ) * normalVec - lightVec );
			float3 phong = KS * powf( max( 0.0f, dot( reflection, -rayDirVec )), cBrightness ) * lightColor;

			rgb = phong;
		}

		rgb += lambert + ambiant;
		rgb *= make_float3(color.x, color.y, color.z);

		// Due to alpha pre-multiplication
		color.x = rgb.x / color.w;
		color.y = rgb.y / color.w;
		color.z = rgb.z / color.w;

		// -- [ Opacity correction ] --
		// The standard equation :
		//		_accColor = _accColor + ( 1.0f - _accColor.w ) * color;
		// must take alpha correction into account
		// NOTE : if ( color.w == 0 ) then alphaCorrection equals 0.f
		float alphaCorrection = ( 1.0f -_accColor.w ) * ( 1.0f - powf( 1.0f - color.w, rayStep * 512.f ) );

		// Accumulate the color
		_accColor.x += alphaCorrection * color.x;
		_accColor.y += alphaCorrection * color.y;
		_accColor.z += alphaCorrection * color.z;
		_accColor.w += alphaCorrection;
	}
}
