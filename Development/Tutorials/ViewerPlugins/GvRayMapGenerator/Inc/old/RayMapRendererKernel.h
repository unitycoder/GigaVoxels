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

#ifndef _RAY_MAP_RENDERER_KERNEL_H_
#define _RAY_MAP_RENDERER_KERNEL_H_

/******************************************************************************
 ******************************* INCLUDE SECTION ******************************
 ******************************************************************************/

// CUDA
#include <cuda_texture_types.h>

// Cuda SDK
#include <helper_math.h>

// GigaVoxels
#include <GvCore/GPUPool.h>
#include <GvCore/RendererTypes.h>
#include <GvRenderer/GvRendererHelpersKernel.h>
#include <GvRenderer/GvVolumeTreeSamplerKernel.h>
#include <GvRenderer/GvVolumeTreeTraversalKernel.h>
#include <GvRenderer/GvVolumeTreeSamplingKernel.h>
#include <GvRenderer/GvRendererContext.h>
#include <GvPerfMon/CUDAPerfMon.h>
#include <GvPerfMon/CUDAPerfMon.hcu>

/******************************************************************************
 ************************* DEFINE AND CONSTANT SECTION ************************
 ******************************************************************************/

/**
 * Model matrix (CUDA constant memory)
 */
__constant__ float4x4 k_modelMatrix;

/**
 * Inverse model matrix (CUDA constant memory)
 */
__constant__ float4x4 k_modelMatrixInv;

/**
 * Ray map texture
 */
texture< float4, cudaTextureType2D, cudaReadModeElementType > rayMapTexture;

/******************************************************************************
 ****************************** INLINE DEFINITION *****************************
 ******************************************************************************/

/******************************************************************************
 * Initialize the pixel coordinates.
 *
 * @param Pid the input thread identifiant
 * @param blockPos the computed block position
 * @param pixelCoords the computed pixel coordinates
 ******************************************************************************/
template< class TBlockResolution >
__device__
inline void initPixelCoords( const uint Pid, /*uint2& blockPos,*/ uint2& pixelCoords )
{
#if RENDER_USE_SWIZZLED_THREADS==0
	pixelCoords.x = threadIdx.x + __uimul( blockIdx.x, TBlockResolution::x );
	pixelCoords.y = threadIdx.y + __uimul( blockIdx.y, TBlockResolution::y );
#else // Z-curve scheduling

	// Deinterleave bits
	GvRenderer::deinterleaveBits32( Pid, pixelCoords );

// As "blockPos" parameter seems to be unused anymore, the following are commented.
// TO DO : are following lines useful ?
//#if 1
//	blockPos = make_uint2( blockIdx.x, blockIdx.y );
//#else
//	uint Bid = blockIdx.x + blockIdx.y * gridDim.x;
//	deinterleaveBits32( Bid, blockPos );
//#endif

	pixelCoords.x += /*blockPos.x*/blockIdx.x * TBlockResolution::x;
	pixelCoords.y += /*blockPos.y*/blockIdx.y * TBlockResolution::y;
#endif
}

/******************************************************************************
 * This function is used to :
 * - traverse the data structure (and emit requests if necessary)
 * - render bricks
 *
 * @param pDataStructure data structure
 * @param pShader shader
 * @param pCache cahce
 * @param pPixelCoords pixel coordinates in window
 * @param pRayStartTree ray start point
 * @param pRayDirTree ray direction
 * @param ptMaxTree max distance from camera found after box intersection test and comparing with input z (from the scene)
 * @param ptTree the distance from camera found after box intersection test and comparing with input z (from the scene)
 ******************************************************************************/
template< bool TFastUpdateMode, bool TPriorityOnBrick, class VolTreeKernelType, class SampleShaderType, class TCacheType >
__device__
void renderVolTree_Std( VolTreeKernelType& pDataStructure, SampleShaderType& pShader, TCacheType& pCache,
					   uint2 pPixelCoords, const float3 pRayStartTree, const float3 pRayDirTree, const float ptMaxTree, float& ptTree )
{
	CUDAPM_KERNEL_DEFINE_EVENT( 2 );
	CUDAPM_KERNEL_DEFINE_EVENT( 3 );
	CUDAPM_KERNEL_DEFINE_EVENT( 4 );
	CUDAPM_KERNEL_DEFINE_EVENT( 5 );

	CUDAPM_KERNEL_START_EVENT( pPixelCoords, 5 );

	// Keep root node in cache
	pCache._nodeCacheManager.setElementUsage( 0 );
	
	// Initialize the brick sampler, a helper class used to sample data in the data structure
	GvRenderer::GvVolumeTreeSamplerKernel< VolTreeKernelType > brickSampler;
	brickSampler._volumeTree = &pDataStructure;

	// Initialize the position at wich data will be sampled
	float3 samplePosTree = pRayStartTree + ptTree * pRayDirTree;

	// Shader pre-shade process
	pShader.preShade( pRayStartTree, pRayDirTree, ptTree );

	// Ray marching.
	// Step with "ptTree" along ray from start to stop bounds.
	int numLoop = 0;
	while
		( ptTree < ptMaxTree
		&& numLoop < 5000	// TO DO : remove this hard-coded value or let only for DEBUG
		&& !pShader.stopCriterion( samplePosTree ) )
	{
		CUDAPM_KERNEL_START_EVENT( pPixelCoords, 4 );

		//float3 samplePosTree = pRayStartTree + ptTree * pRayDirTree;
		const float coneAperture = pShader.getConeAperture( ptTree );
		
		// Declare an empty node of the data structure.
		// It will be filled during the traversal according to cuurent sample position "samplePosTree"
		GvStructure::OctreeNode node;

		CUDAPM_KERNEL_START_EVENT( pPixelCoords, 2 );

		// [ 1 ]- Descent the data structure (in general an octree)
		// until max depth is reach or current traversed node has no subnodes,
		// or cone aperture is greater than voxel size.
		float nodeSizeTree;
		float3 sampleOffsetInNodeTree;
		bool modifInfoWriten = false;
		GvRenderer::rendererDescentOctree< TPriorityOnBrick >
							( pDataStructure, pCache, node, samplePosTree, coneAperture,
							nodeSizeTree, sampleOffsetInNodeTree, brickSampler, modifInfoWriten );

		const float rayLengthInNodeTree = GvRenderer::getRayLengthInNode( sampleOffsetInNodeTree, nodeSizeTree, pRayDirTree );

		CUDAPM_KERNEL_STOP_EVENT( pPixelCoords, 2 );

		// Early loop termination
		if ( TFastUpdateMode && modifInfoWriten )
		{
			break;
		}

		// [ 2 ] - If node is a brick, renderer it.
		if ( node.isBrick() )
		{
			CUDAPM_KERNEL_START_EVENT( pPixelCoords, 3 );

			// PASCAL
			// This is used to stop the ray with a z-depth value smaller than the remaining brick ray length
			//
			// QUESTION : pas forc�ment, si objet qui cache est transparent !??
			// => non, comme d'hab en OpenGL => dessiner d'abord les objets opaques
			const float rayLengthInBrick = mincc( rayLengthInNodeTree, ptMaxTree - ptTree );	// ==> attention, ( ptMaxTree - ptTree < 0 ) ?? ==> non, � casue du test du WHILE !! c'est OK !!

			// This is where shader program occurs
			float dt = GvRenderer::rendererBrickSampling< TFastUpdateMode, TPriorityOnBrick >
									( pDataStructure, pShader, pCache, pRayStartTree, pRayDirTree,
									//ptTree, rayLengthInNodeTree, brickSampler, modifInfoWriten );
									ptTree, rayLengthInBrick, brickSampler, modifInfoWriten );

			CUDAPM_KERNEL_STOP_EVENT( pPixelCoords, 3 );

			ptTree += dt;
		}
		else
		{
			ptTree += rayLengthInNodeTree;// + coneAperture;
			ptTree += pShader.getConeAperture( ptTree );
		}

		samplePosTree = pRayStartTree + ptTree * pRayDirTree;

		// Update internal counter
		numLoop++;

		CUDAPM_KERNEL_STOP_EVENT( pPixelCoords, 4 );
	} // while

	CUDAPM_KERNEL_STOP_EVENT( pPixelCoords, 5 );

	// Shader post-shade process
	pShader.postShade();
}

/******************************************************************************
 * CUDA kernel
 * This is the main GigaVoxels KERNEL
 * It is in charge of casting rays and found the color and depth values at pixels.
 *
 * @param pVolumeTree data structure
 * @param pCache cache
 ******************************************************************************/
template<	class TBlockResolution, bool TFastUpdateMode, bool TPriorityOnBrick, 
			class TSampleShaderType, class TVolTreeKernelType, class TCacheType >
__global__
void RenderKernelSimple( TVolTreeKernelType pVolumeTree, TCacheType pCache )
{
	// Per-pixel shader instance
	typename TSampleShaderType::KernelType sampleShader;

	// Shared memory
	//__shared__ float3 rayStartInWorld;
	__shared__ float3 rayStartInTree;

	CUDAPM_KERNEL_DEFINE_EVENT( 0 );
	CUDAPM_KERNEL_DEFINE_EVENT( 1 );

	// Compute thread ID
	uint Pid = threadIdx.x + threadIdx.y * TBlockResolution::x;

	// Retrieve current processed pixel position
	// This function modifies the pixel accessing pattern (i.e. z-curve)
	uint2 pixelCoords;
	/*uint2 blockPos;*/ // NOTE : this "block position" parameter seemed not used anymore
	initPixelCoords< TBlockResolution >( Pid, /*blockPos,*/ pixelCoords );

	CUDAPM_KERNEL_START_EVENT( pixelCoords, 0 );

	// Check if were are inside the frame (window or viewport ?)
	bool outOfFrame = ( pixelCoords.x >= k_renderViewContext.frameSize.x ) || ( pixelCoords.y >= k_renderViewContext.frameSize.y );
	// FUTUR optimization
	//
	//bool outOfFrame = ( ( pixelCoords.x >= /*projectedBBoxSize*/k_renderViewContext._projectedBBox.z ) || ( pixelCoords.y >= /*projectedBBoxSize*/k_renderViewContext._projectedBBox.w ) );
	//bool outOfFrame = ( ( pixelCoords.x > /*projectedBBoxSize*/k_renderViewContext._projectedBBox.z ) || ( pixelCoords.y > /*projectedBBoxSize*/k_renderViewContext._projectedBBox.w ) );
	if ( ! outOfFrame )
	//bool inFrame = ( ( pixelCoords.x < k_renderViewContext._projectedBBox.z ) || ( pixelCoords.y < k_renderViewContext._projectedBBox.w ) );
	//if ( inFrame )
	{
		// Read depth from the input depth buffer.
		// Depth buffer contains the Zwindow (distance to camera plane) which is different from Zeye (distance to camera)
		// Zwindow is between 0.0 and 1.0
		// The depth buffer doesn't contain distance values from the camera.
		// The depth values are the perpendicular distance to the plane of the camera.
		float frameDepth = GvRenderer::getInputDepth( pixelCoords );		// TO DO : this read memory could be placed before to avoid cycles waiting...

		// FUTUR optimization
		//
		//// Add offset of the projected BBox bottom left corner
		//pixelCoords.x += /*projectedBBoxBottomLeft*/k_renderViewContext._projectedBBox.x;
		//pixelCoords.y += /*projectedBBoxBottomLeft*/k_renderViewContext._projectedBBox.y;

		//// Calculate eye ray in world space

		//float3 pixelVecWP = k_renderViewContext.viewPlaneDirWP
		//					+ k_renderViewContext.viewPlaneXAxisWP * static_cast< float >( pixelCoords.x )
		//					+ k_renderViewContext.viewPlaneYAxisWP * static_cast< float >( pixelCoords.y );

		//rayStartInWorld = k_renderViewContext.viewCenterWP;
		//float3 rayDirInWorld = normalize( pixelVecWP );

		//// Transform the ray from World to Tree Space
		//rayStartInTree = mul( k_renderViewContext.invModelMatrix, rayStartInWorld );	// ce terme pourrait/devrait �tre calcul� sur le HOST car il est constant...
		//
		//// Beware, vectors and transformed by inverse transpose...
		//// TO DO : do correction
		//float3 rayDirInTree = normalize( mulRot( k_renderViewContext.invModelMatrix, rayDirInWorld ) );

		//---------------------------------------
		// TEST
		// Calculate eye ray in tree space
		/*float3 rayDirInTree = k_renderViewContext.viewPlaneDirTP
							+ k_renderViewContext.viewPlaneXAxisTP * static_cast< float >( pixelCoords.x )
							+ k_renderViewContext.viewPlaneYAxisTP * static_cast< float >( pixelCoords.y );*/
		
		float4 rayDirection = tex2D( rayMapTexture, pixelCoords.x, pixelCoords.y );

		/*float3*/ rayStartInTree = k_renderViewContext.viewCenterTP;
		// + PASCAL

		//rayDirInTree = normalize( rayDirInTree );
		
		//float3 rayDirInTree = normalize( make_float3( rayDirection.x, rayDirection.y, rayDirection.z ) );
		float3 rayDirInTree = normalize( mulRot( k_renderViewContext.invModelMatrix, normalize( make_float3( rayDirection.x, rayDirection.y, rayDirection.z ) ) ) );
		 //---------------------------------------
			
		float boxInterMin = 0.0f;
		float boxInterMax = 10000.0f;
		int hit = GvRenderer::intersectBox( rayStartInTree, rayDirInTree, make_float3( 0.001f ), make_float3( 0.999f ), boxInterMin, boxInterMax );
		bool masked = ! ( hit && ( boxInterMax > 0.0f ) );
		boxInterMin = maxcc( boxInterMin, k_renderViewContext.frustumNear );

		float t = boxInterMin + sampleShader.getConeAperture( boxInterMin );
		float tMax = boxInterMax;

		if ( frameDepth < 1.0f )
		{
			// Retrieve the view-space depth from the depth buffer. Only works if w was 1.0f.
			// See: http://www.opengl.org/discussion_boards/ubbthreads.php?ubb=showflat&Number=304624&page=2
			float clipZ = 2.0f * frameDepth - 1.0f;
			float frameT = k_renderViewContext.frustumD / ( -clipZ - k_renderViewContext.frustumC );
			frameT = -frameT;

			tMax = mincc( frameT, boxInterMax );
			//tMax = boxInterMax;
		}

		if ( t == 0.0f || t >= tMax )
		{
			masked = true;
		}

		if ( ! masked )
		{
			// Read color from the input color buffer
			uchar4 frameColor = GvRenderer::getInputColor( pixelCoords );

			// Launch N3-tree traversal and rendering
			CUDAPM_KERNEL_START_EVENT( pixelCoords, 1 );
			renderVolTree_Std< TFastUpdateMode, TPriorityOnBrick >( pVolumeTree, sampleShader, pCache, pixelCoords, rayStartInTree, rayDirInTree, tMax, t );
			CUDAPM_KERNEL_STOP_EVENT( pixelCoords, 1 );

			// Retrieve the accumulated color
			float4 accCol = sampleShader.getColor();

			// Convert color from uchar [ 0 ; 255 ] to float [ 0.0 ; 1.0 ]
			float4 scenePixelColorF = make_float4( (float)frameColor.x / 255.0f, (float)frameColor.y / 255.0f, (float)frameColor.z / 255.0f, (float)frameColor.w / 255.0f );
			
			// Update color
			float4 pixelColorF = accCol + scenePixelColorF * ( 1.0f - accCol.w );

			// Clamp color to be within the interval [+0.0, 1.0]
			pixelColorF.x = __saturatef( pixelColorF.x );
			pixelColorF.y = __saturatef( pixelColorF.y );
			pixelColorF.z = __saturatef( pixelColorF.z );
			pixelColorF.w = 1.0f;		// <== why 1.0f and not __saturatef( pixelColorF.w ) ?	// Pour �viter une op�ration OpenGL de ROP ? Car �a a �t� penser pour rendre en dernier au d�part ?
			//pixelColorF.w = __saturatef( pixelColorF.w );
			
			// Convert color from float [ 0.0 ; 1.0 ] to uchar [ 0 ; 255 ]
			frameColor = make_uchar4( (uchar)( pixelColorF.x * 255.0f ), (uchar)( pixelColorF.y * 255.0f ), (uchar)( pixelColorF.z * 255.0f ), (uchar)( pixelColorF.w * 255.0f ) );
			
			// Project the depth and check against the current one
			float pixDepth = 1.0f;

			if ( accCol.w > cOpacityStep )
			{
				float VP = -fabsf( t * rayDirInTree.z );
				//http://www.opengl.org/discussion_boards/ubbthreads.php?ubb=showflat&Number=234519&page=2
				float clipZ = ( VP * k_renderViewContext.frustumC + k_renderViewContext.frustumD ) / -VP;
				
				//pixDepth = clamp( ( clipZ + 1.0f ) / 2.0f, 0.0f, 1.0f );		// TO DO : use __saturatef instead !!
				pixDepth = __saturatef( ( clipZ + 1.0f ) / 2.0f );		// TO DO : use __saturatef instead !!	=====> ( [ x 0.5f ] instead ) ??
			}

			//frameDepth = getFrameDepthIn( pixelCoords );
			frameDepth = min( frameDepth, pixDepth );

			// Write color in color buffer
			GvRenderer::setOutputColor( pixelCoords, frameColor );
			
			// Write depth in depth buffer
			GvRenderer::setOutputDepth( pixelCoords, frameDepth );
		}
	} // !outOfFrame

	CUDAPM_KERNEL_STOP_EVENT( pixelCoords, 0 );
}

// FIXME: Move this to another place
/******************************************************************************
 * CUDA kernel ...
 *
 * @param syntheticBuffer ...
 * @param totalNumElems ...
 ******************************************************************************/
__global__
void SyntheticInfo_Render( uchar4 *syntheticBuffer, uint totalNumElems )
{
	uint2 pixelCoords;
	pixelCoords.x = threadIdx.x + __uimul(blockIdx.x, blockDim.x);
	pixelCoords.y = threadIdx.y + __uimul(blockIdx.y, blockDim.y);

	uint elemIdx= pixelCoords.x+pixelCoords.y* (blockDim.x*gridDim.x);

	//uint totalNumElems=syntheticBuffer.getResolution().x;

	uchar4 pixelColor;

	if ( elemIdx < totalNumElems )
	{
		uchar4 sval = syntheticBuffer[ elemIdx ];

		if ( sval.w )
		{
			pixelColor = make_uchar4( 255, 0, 0, 255 );
		}
		else if ( sval.x )
		{
			pixelColor = make_uchar4( 0, 255, 0, 255 );
		}
		else
		{
			pixelColor = make_uchar4( 0, 0, 0, 255 );
		}

		GvRenderer::setOutputColor( pixelCoords, pixelColor );
	}
	else
	{
		//pixelColor = make_uchar4( 255, 255, 255, 255 );
	}
}

#endif // !_RAY_MAP_RENDERER_KERNEL_H_
