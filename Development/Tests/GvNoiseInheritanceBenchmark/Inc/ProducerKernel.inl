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

// GigaVoxels
#include <GvStructure/GvNode.h>
#include <GvUtils/GvNoiseKernel.h>
#include <GvStructure/GvVolumeTreeKernel.h>
#include <GvRendering/GvNodeVisitorKernel.h>

/******************************************************************************
 ****************************** KERNEL DEFINITION *****************************
 ******************************************************************************/

/******************************************************************************
 * Constructor
 ******************************************************************************/
template< typename TDataStructureType >
ProducerKernel< TDataStructureType >
::ProducerKernel()
{
}

/******************************************************************************
 * Initialize the producer
 * 
 * @param volumeTreeKernel Reference on a volume tree data structure
 ******************************************************************************/
template< typename TDataStructureType >
inline void ProducerKernel< TDataStructureType >
::initialize( DataStructureKernel& pDataStructure )
{
	_dataStructureKernel = pDataStructure;
}

/******************************************************************************
 * Produce data on device.
 * Implement the produceData method for the channel 0 (nodes)
 *
 * Producing data mecanism works element by element (node tile or brick) depending on the channel.
 *
 * In the function, user has to produce data for a node tile or a brick of voxels :
 * - for a node tile, user has to defined regions (i.e nodes) where lies data, constant values,
 * etc...
 * - for a brick, user has to produce data (i.e voxels) at for each of the channels
 * user had previously defined (color, normal, density, etc...)
 *
 * @param pGpuPool The device side pool (nodes or bricks)
 * @param pRequestID The current processed element coming from the data requests list (a node tile or a brick)
 * @param pProcessID Index of one of the elements inside a node tile or a voxel bricks
 * @param pNewElemAddress The address at which to write the produced data in the pool
 * @param pParentLocInfo The localization info used to locate an element in the pool
 * @param Loki::Int2Type< 0 > corresponds to the index of the node pool
 *
 * @return A feedback value that the user can return.
 ******************************************************************************/
template< typename TDataStructureType >
template< typename GPUPoolKernelType >
__device__
inline uint ProducerKernel< TDataStructureType >
::produceData( GPUPoolKernelType& nodePool, uint requestID, uint processID, uint3 newElemAddress,
			  const GvCore::GvLocalizationInfo& parentLocInfo, Loki::Int2Type< 0 > )
{
	// NOTE :
	// In this method, you are inside a node tile.
	// The goal is to determine, for each node of the node tile, which type of data it holds.
	// Data type can be :
	// - a constant region,
	// - a region with data,
	// - a region where max resolution is reached.
	// So, one thread is responsible of the production of one node of a node tile.
	
	// Retrieve current node tile localization information code and depth
	const GvCore::GvLocalizationInfo::CodeType parentLocCode = parentLocInfo.locCode;
	const GvCore::GvLocalizationInfo::DepthType parentLocDepth = parentLocInfo.locDepth;

	// Process ID gives the 1D index of a node in the current node tile
	if ( processID < NodeRes::getNumElements() )
	{
		// First, compute the 3D offset of the node in the node tile
		uint3 subOffset = NodeRes::toFloat3( processID );
		
		// Node production corresponds to subdivide a node tile.
		// So, based on the index of the node, find the node child.
		// As we want to sudbivide the curent node, we retrieve localization information
		// at the next level
		uint3 regionCoords = parentLocCode.addLevel< NodeRes >( subOffset ).get();
		uint regionDepth = parentLocDepth.addLevel().get();

		// Create a new node for which you will have to fill its information.
		GvStructure::GvNode newnode;
		newnode.childAddress = 0;
		newnode.brickAddress = 0;

		// Call what we call an oracle that will determine the type of the region of the node accordingly
		GPUVoxelProducer::GPUVPRegionInfo nodeinfo = getRegionInfo( regionCoords, regionDepth );

		// Now that the type of the region is found, fill the new node information
		if ( nodeinfo == GPUVoxelProducer::GPUVP_CONSTANT )
		{
			newnode.setTerminal( true );
		}
		else if ( nodeinfo == GPUVoxelProducer::GPUVP_DATA )
		{
			newnode.setStoreBrick();
			newnode.setTerminal( false );
		}
		else if ( nodeinfo == GPUVoxelProducer::GPUVP_DATA_MAXRES )
		{
			newnode.setStoreBrick();
			newnode.setTerminal( true );
		}

		// Finally, write the new node information into the node pool by selecting channels :
		// - Loki::Int2Type< 0 >() points to node information
		// - Loki::Int2Type< 1 >() points to brick information
		//
		// newElemAddress.x + processID : is the adress of the new node in the node pool
		nodePool.getChannel( Loki::Int2Type< 0 >() ).set( newElemAddress.x + processID, newnode.childAddress );
		nodePool.getChannel( Loki::Int2Type< 1 >() ).set( newElemAddress.x + processID, newnode.brickAddress );
	}

	return 0;
}

/******************************************************************************
 * Produce data on device.
 * Implement the produceData method for the channel 1 (bricks)
 *
 * Producing data mecanism works element by element (node tile or brick) depending on the channel.
 *
 * In the function, user has to produce data for a node tile or a brick of voxels :
 * - for a node tile, user has to defined regions (i.e nodes) where lies data, constant values,
 * etc...
 * - for a brick, user has to produce data (i.e voxels) at for each of the channels
 * user had previously defined (color, normal, density, etc...)
 *
 * @param pGpuPool The device side pool (nodes or bricks)
 * @param pRequestID The current processed element coming from the data requests list (a node tile or a brick)
 * @param pProcessID Index of one of the elements inside a node tile or a voxel bricks
 * @param pNewElemAddress The address at which to write the produced data in the pool
 * @param pParentLocInfo The localization info used to locate an element in the pool
 * @param Loki::Int2Type< 1 > corresponds to the index of the brick pool
 *
 * @return A feedback value that the user can return.
 ******************************************************************************/
template< typename TDataStructureType >
template< typename GPUPoolKernelType >
__device__
inline uint ProducerKernel< TDataStructureType >
::produceData( GPUPoolKernelType& dataPool, uint requestID, uint processID, uint3 newElemAddress,
			  const GvCore::GvLocalizationInfo& parentLocInfo, Loki::Int2Type< 1 > )
{
	// Type definition for the noise
	typedef GvUtils::GvNoiseKernel Noise;

	// NOTE :
	// In this method, you are inside a brick of voxels.
	// The goal is to determine, for each voxel of the brick, the value of each of its channels.
	//
	// Data type has previously been defined by the user, as a
	// typedef Loki::TL::MakeTypelist< uchar4, half4 >::Result DataType;
	//
	// In this tutorial, we have choosen two channels containing color at channel 0 and normal at channel 1.
	
	// Retrieve current brick localization information code and depth
	const GvCore::GvLocalizationInfo::CodeType parentLocCode = parentLocInfo.locCode;
	const GvCore::GvLocalizationInfo::DepthType parentLocDepth = parentLocInfo.locDepth;

	// Shared memory declaration
	//
	// Each threads of a block process one and unique brick of voxels.
	// We store in shared memory common useful variables that will be used by each thread.
	__shared__ uint3 brickRes;
	__shared__ uint3 levelRes; 
	__shared__ float3 levelResInv;
	__shared__ int3 brickPos;
	__shared__ float3 brickPosF;

	// Compute useful variables used for retrieving positions in 3D space
	brickRes = BrickRes::get();
	levelRes = make_uint3( 1 << parentLocDepth.get() ) * brickRes;	// number of voxels (in each dimension)
	levelResInv = make_float3( 1.0f ) / make_float3( levelRes );	// size of a voxel (in each dimension)

	brickPos = make_int3( parentLocCode.get() * brickRes ) - BorderSize;
	brickPosF = make_float3( brickPos ) * levelResInv;

	// Real brick size (with borders)
	uint3 elemSize = BrickRes::get() + make_uint3( 2 * BorderSize );

	// Retrieve coaser brick position in pool
	__shared__ uint smCoaserBrickPositionInPool;
	__shared__ bool smUseCoaserLevel;
	__shared__ GvStructure::GvNode smParentNode;
	__shared__ float3 smBrickCenter;
	__shared__ uint3 smParentNodeBrickAddress;
	GvCore::GvLocalizationCode parentNodeLocalizationCode; 
	float3 brickSize; 
	float3 parentBrickSize; 
	float3 parentNodePosition;
	uint3 nodeChildCoordinates; 
	float3 brickPosition; 
	//--------------------------------
	
	
	float noiseFrequency;
	float noiseAmplitude; 

	if (cInheritance) 
	{
		if ( processID == 0 )
		{
			smUseCoaserLevel = false;
				
			smParentNode.childAddress = 0;
			smParentNode.brickAddress = 0;

			smBrickCenter = make_float3( parentLocCode.get() ) * ( 1.f / static_cast< float >( 1 << parentLocDepth.get() ) ) + make_float3( 0.5f * ( 1.f / static_cast< float >( 1 << parentLocDepth.get() ) ) );

			smParentNodeBrickAddress = make_uint3( 0, 0, 0 );

			if ( parentLocDepth.get() > 0 )
			{
				smUseCoaserLevel = true;

				// Retrieve parent node
				
				
				GvRendering::GvNodeVisitorKernel::getNodeFather( _dataStructureKernel, smParentNode, smBrickCenter, parentLocDepth.get() - 1 );
				
				// Retrieve parent node's brick address
				smParentNodeBrickAddress = smParentNode.getBrickAddress();
			}
			//printf("Production d'une brique\n");
		}
		__syncthreads();
		float coeff = static_cast< float >( 1 << parentLocDepth.get() );

		//--------------------------------
	    parentNodeLocalizationCode = /*parent loc code*/parentLocCode.removeLevel< NodeRes >();
		brickSize = make_float3( 1.0f ) / static_cast< float >( 1 << parentLocDepth.get() );
		parentBrickSize = 2.f * brickSize;
		parentNodePosition = make_float3( parentNodeLocalizationCode.get() ) * parentBrickSize;
		nodeChildCoordinates = make_uint3(  floorf(   (smBrickCenter - parentNodePosition ) / brickSize) );
		brickPosition = make_float3( parentLocCode.get() * BrickRes::get() ) * levelResInv;
		//--------------------------------
	
	
		noiseFrequency = cFrequencyCoefficient * static_cast< float >( 1 << parentLocDepth.get() );
		noiseAmplitude =powf( cAmplitudeCoefficient, static_cast< float >( parentLocDepth.get() ) );

	}
	// Thread Synchronization
	
	
	// The original KERNEL execution configuration on the HOST has a 2D block size :
	// dim3 blockSize( 16, 8, 1 );
	//
	// Each block process one brick of voxels.
	//
	// One thread iterate in 3D space given a pattern defined by the 2D block size
	// and the following "for" loops. Loops take into account borders.
	// In fact, each thread of the current 2D block compute elements layer by layer
	// on the z axis.
	//
	// One thread process only a subset of the voxels of the brick.
	//
	// Iterate through z axis step by step as blockDim.z is equal to 1
	uint3 elemOffset;
	for ( elemOffset.z = 0; elemOffset.z < elemSize.z; elemOffset.z += blockDim.z )
	{
		for ( elemOffset.y = 0; elemOffset.y < elemSize.y; elemOffset.y += blockDim.y )
		{
			for ( elemOffset.x = 0; elemOffset.x < elemSize.x; elemOffset.x += blockDim.x )
			{
				// Compute position index
				uint3 locOffset = elemOffset + make_uint3( threadIdx.x, threadIdx.y, threadIdx.z );

				// Test if the computed position index is inside the brick (with borders)
				if ( locOffset.x < elemSize.x && locOffset.y < elemSize.y && locOffset.z < elemSize.z )
				{
					// Position of the current voxel's center (relative to the brick)
					//
					// In order to make the mip-mapping mecanism OK,
					// data values must be set at the center of voxels.
					float3 voxelPosInBrickF = ( make_float3( locOffset) * levelResInv) + 0.5 * levelResInv ;
					// Position of the current voxel's center (absolute, in [0.0;1.0] range)
					float3 voxelPosF = brickPosF + voxelPosInBrickF;
					// Position of the current voxel's center (scaled to the range [-1.0;1.0])
					float3 posF = voxelPosF * 2.0f - 1.0f;
					float noise;

					if (cInheritance)
					{
						// Retrieve already computed coarser noise value from parent node
						float coarserNoise = 0.f;
						if ( smUseCoaserLevel )
						{
							// Retrieve parent brick's noise value
							const float3 offsetPositionInNode = ( voxelPosF - brickPosition ) / brickSize;
						
							const float3 samplePosition = 	make_float3( nodeChildCoordinates ) *( (_dataStructureKernel.brickSizeInCacheNormalized) * 0.5f) 
														+ offsetPositionInNode * ((_dataStructureKernel.brickSizeInCacheNormalized) * 0.5f) ;
					
							//printf("%f\n", _dataStructureKernel.brickSizeInCacheNormalized.x);
							const float4 brickData = _dataStructureKernel.template getSampleValueTriLinear< 2 >( make_float3( smParentNodeBrickAddress.x, smParentNodeBrickAddress.y, smParentNodeBrickAddress.z ) * _dataStructureKernel.brickCacheResINV,
														samplePosition);
							coarserNoise = brickData.x;
							//coarserNoise = 1;
						}
					
				
						// Noise
					//	float _threshold = 0.2f;
					//	float noise = __saturatef( Noise::getValue( posF ) );
						// Marble pattern
						/*float tmp = fmod( fabs( Noise::getValue( posF ) ) , _threshold );
						if ( tmp > 0.5f * _threshold )
						{
							tmp = _threshold - tmp;
						}
						float noise = tmp / _threshold * 2.0f;*/

						// http://freespace.virgin.net/hugo.elias/models/m_perlin.htm

						// first octave
						/*float _persistence = 0.2f;
						float _first_frequency = 8.f;*/
						//int _nOctaves = parentLocDepth.get() + 1;
						float x = posF.x;
						float y = posF.y;
						float z = posF.z;
						//x = x * cFrequencyCoefficient;
						//y = y * cFrequencyCoefficient;
						//z = z * cFrequencyCoefficient;
						// Current noise
						//float result = __saturatef( Noise::getValue( x, y, z ) );
						/*float result;
						if (parentLocDepth.get() == 0)
						{
							result = 1;//(voxelPosF.y +  voxelPosF.x)/2.f;
					
						}
						else {
							result = 0;
						}*/
						float result = noiseAmplitude * (Noise::getValue( noiseFrequency * x, noiseFrequency * y, noiseFrequency * z ));

						//float persistence = _persistence;
						// Fractal sum
						/*for ( int i = 1 ; i < _nOctaves ; i++ )
						{
							x = x * 2.0f;
							y = y * 2.0f;
							z = z * 2.0f;
							result += persistence * __saturatef( Noise::getValue( x, y, z ) );
							persistence *= _persistence;
						}*/
						//float noise = result;
						noise =result + coarserNoise;
						noise = __saturatef(noise);
					} else {
						noise = 0.f;

						float3 voxelPosInBrickF = ( make_float3( locOffset) * levelResInv) + 0.5 * levelResInv ;
						// Position of the current voxel's center (absolute, in [0.0;1.0] range)
						float3 voxelPosF = brickPosF + voxelPosInBrickF;
						// Position of the current voxel's center (scaled to the range [-1.0;1.0])
						float3 posF = voxelPosF * 2.0f - 1.0f;
						float x = posF.x;
						float y = posF.y;
						float z = posF.z;
						for (int k = 0;k <=parentLocDepth.get(); k++)
						{
							const float noiseFrequency = cFrequencyCoefficient * static_cast< float >( 1 << k );
							const float noiseAmplitude =powf( cAmplitudeCoefficient, static_cast< float >( k ) );
							
							noise += noiseAmplitude * (Noise::getValue( noiseFrequency * x, noiseFrequency * y, noiseFrequency * z ));
							noise =  __saturatef(noise);
						}
					
					
					}


					// Color
					//float4 voxelColor = make_float4( 1.0f, 0.0f, 0.0f, 0.0f );
					float4 voxelColor = make_float4( noise, 0, noise, 0.0f );
					//float4 voxelColor = make_float4( noise, 0.f, 0.f, 0.0f );
					//float localColor = noise * 0.5f + 0.5f;
					//float4 voxelColor = make_float4( localColor, localColor, localColor, 0.0f );
					//float4 voxelColor = make_float4( localColor, localColor, localColor, 0.0f );

					// Normal
					float aux = posF.x*posF.x + posF.y*posF.y + posF.z*posF.z + cR*cR - cr*cr;
					float4 voxelNormal = make_float4( normalize( make_float3( 4*posF.x*aux - 8*cR*cR*posF.x,
														4*posF.y*aux - 8*cR*cR*posF.y,
														4*posF.z*aux ) ),
														1.0f );

					// Test if the voxel is located inside the unit sphere
					if ( isInTorus( posF ) )
					{
						voxelColor.w = 1.0f;
					}

					// Alpha pre-multiplication used to avoid the "color bleeding" effect
					voxelColor.x *= voxelColor.w;
					voxelColor.y *= voxelColor.w;
					voxelColor.z *= voxelColor.w;

					// Compute the new element's address
					uint3 destAddress = newElemAddress + locOffset;
					
					// Write the voxel's color in the first field
					dataPool.template setValue< 0 >( destAddress, voxelColor );
					
					// Write the voxel's normal in the second field
					dataPool.template setValue< 1 >( destAddress, voxelNormal );

					// Write the voxel's parent brick's noise value in the third field
					//dataPool.template setValue< 2 >( destAddress, coarserNoise );
					dataPool.template setValue< 2 >( destAddress, noise/*sum of noise*/ );
				}
			}
		}
	}

	return 0;
}

/******************************************************************************
 * Helper function used to determine the type of zones in the data structure.
 *
 * The data structure is made of regions containing data, empty or constant regions.
 * Besides, this function can tell if the maximum resolution is reached in a region.
 *
 * @param regionCoords region coordinates
 * @param regionDepth region depth
 *
 * @return the type of the region
 ******************************************************************************/
template< typename TDataStructureType >
__device__
inline GPUVoxelProducer::GPUVPRegionInfo ProducerKernel< TDataStructureType >
::getRegionInfo( uint3 regionCoords, uint regionDepth )
{
	// Limit the depth.
	// Currently, 32 is the max depth of the GigaVoxels engine.
	if ( regionDepth >= 32 )
	{
		return GPUVoxelProducer::GPUVP_DATA_MAXRES;
	}

	// Shared memory declaration
	__shared__ uint3 brickRes;
	__shared__ float3 brickSize;
	__shared__ uint3 levelRes;
	__shared__ float3 levelResInv;

	brickRes = BrickRes::get();

	levelRes = make_uint3( 1 << regionDepth ) * brickRes;
	levelResInv = make_float3( 1.f ) / make_float3( levelRes );

	//int3 brickPos = make_int3( regionCoords * brickRes ) - BorderSize;
	//float3 brickPosF = make_float3( brickPos ) * levelResInv;

	// Since we work in the range [-1;1] below, the brick size is two time bigger
	brickSize = make_float3( 1.f ) / make_float3( 1 << regionDepth ) * 2.f;

	// Build the eight brick corners of a sphere centered in [0;0;0]
	float3 q000 = make_float3( regionCoords * brickRes ) * levelResInv * 2.f - 1.f;
	float3 q001 = make_float3( q000.x + brickSize.x, q000.y,			   q000.z);
	float3 q010 = make_float3( q000.x,				 q000.y + brickSize.y, q000.z);
	float3 q011 = make_float3( q000.x + brickSize.x, q000.y + brickSize.y, q000.z);
	float3 q100 = make_float3( q000.x,				 q000.y,			   q000.z + brickSize.z);
	float3 q101 = make_float3( q000.x + brickSize.x, q000.y,			   q000.z + brickSize.z);
	float3 q110 = make_float3( q000.x,				 q000.y + brickSize.y, q000.z + brickSize.z);
	float3 q111 = make_float3( q000.x + brickSize.x, q000.y + brickSize.y, q000.z + brickSize.z);

	// Test if any of the eight brick corner lies in the sphere
	if ( intersectTorus( q000, q001, q010, q011, q100, q101, q110, q111 ) )
	{
		return GPUVoxelProducer::GPUVP_DATA;
	}

	return GPUVoxelProducer::GPUVP_CONSTANT;
}

/******************************************************************************
 * Helper class to test if a point is inside a torus centered in [0,0,0]
 *
 * @param pPoint the point to test
 *
 * @return flag to tell wheter or not the point is inside the torus
 ******************************************************************************/
template< typename TDataStructureType >
__device__
inline bool ProducerKernel< TDataStructureType >::isInTorus( const float3 pPoint )
{
	//  (R - sqrt(x2+y2) )2 + z2 = r2
	//	(x2 + y2 + z2 + R2 -r2)2 = 4R2 ( x2 + y2)
	/*float x = pPoint.x, y = pPoint.y, z = pPoint.z;
	float tmp = ( _R - sqrtf( x * x + y * y ) );
	return ( tmp * tmp + z * z ) < ( _r * _r );*/

	// return (pPoint.x*pPoint.x + pPoint.y*pPoint.y + pPoint.z*pPoint.z + cR*cR -cr*cr)
	// 	*(pPoint.x*pPoint.x + pPoint.y*pPoint.y + pPoint.z*pPoint.z + cR*cR -cr*cr) <=
	// 	4*cR*cR*(pPoint.x*pPoint.x + pPoint.y*pPoint.y);
	return ( cR - sqrt( pPoint.x*pPoint.x + pPoint.y*pPoint.y ) )*( cR - sqrt( pPoint.x*pPoint.x + pPoint.y*pPoint.y ) ) + pPoint.z*pPoint.z < cr*cr;
}

/******************************************************************************
 * Helper class to test if a cube intersect the torus
 *
 * @param q01 point 1 of the plan 0 ( the plan of lowest height )
 * @param q02 point 2 of the plan 0 ( the plan of lowest height )
 * @param q03 point 3 of the plan 0 ( the plan of lowest height )
 * @param q04 point 4 of the plan 0 ( the plan of lowest height )
 * @param q11 point 1 of the plan 1 ( the plan of lowest height )
 * @param q12 point 2 of the plan 1 ( the plan of lowest height )
 * @param q13 point 3 of the plan 1 ( the plan of lowest height )
 * @param q14 point 4 of the plan 1 ( the plan of lowest height )
 *
 * @return flag to tell wheter or not the cube intersects the torus
 ******************************************************************************/
template< typename TDataStructureType >
__device__
inline bool ProducerKernel< TDataStructureType >::intersectTorus( float3 q01, float3 q02, float3 q03, float3 q04, float3 q11, float3 q12, float3 q13, float3 q14 )
{
	// Rmq : we will only try if one of the both plans intersect the torus, 
	// it is enough because the torus is centered in (0.0, 0.0, 0.0) and his rotation axe is 0z.
	
	// We try if the first plan intersect the torus
	float3 aux = make_float3(0.0, 0.0, q01.z);
	if ( cr > aux.z ) {
		
		float a = cR - sqrt(cr*cr - aux.z*aux.z);
		float b = cR + sqrt(cr*cr - aux.z*aux.z);
		float d1 = length(q01 - aux);
		float d2 = length(q02 - aux);
		float d3 = length(q03 - aux);
		float d4 = length(q04 - aux);
		
		if ( d1 < a || d2 < a || d3 < a || d4 < a ) {
			if ( d1 > a || d2 > a || d3 > a || d4 > a ) {
				return true;
			}
		} else if ( d1 < b || d2 < b || d3 < b || d4 < b ) {
			return true;
		}
	}

	// We try if the second plan intersect the torus
	aux = make_float3(0.0, 0.0, q11.z);
	if ( cr > aux.z ) {
		
		float a = cR - sqrt(cr*cr - aux.z*aux.z);
		float b = cR + sqrt(cr*cr - aux.z*aux.z);
		float d1 = length(q11 - aux);
		float d2 = length(q12 - aux);
		float d3 = length(q13 - aux);
		float d4 = length(q14 - aux);
		
		if ( d1 < a || d2 < a || d3 < a || d4 < a ) {
			if ( d1 > a || d2 > a || d3 > a || d4 > a ) {
				return true;
			}
		} else if ( d1 < b || d2 < b || d3 < b || d4 < b ) {
			return true;
		}
	}
		     
	return false;
}
