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
//#include <GvStructure/GvVolumeTree.h>

/******************************************************************************
 ****************************** KERNEL DEFINITION *****************************
 ******************************************************************************/

/******************************************************************************
 * Initialize the producer
 *
 * @param volumeTreeKernel Reference on a volume tree data structure
 ******************************************************************************/
template< typename TDataStructureType >
inline void ProducerKernel< TDataStructureType >
::initialize( DataStructureKernel& pDataStructure )
{
	//_dataStructureKernel = pDataStructure;
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
template <typename TDataStructureType>
template <typename GPUPoolKernelType>
__device__
inline uint ProducerKernel<TDataStructureType>::produceData(
	GPUPoolKernelType& nodePool,
	uint requestID,
	uint processID,
	uint3 newElemAddress,
	const GvCore::GvLocalizationInfo& parentLocInfo,
	Loki::Int2Type<0>
) {
	// NOTE :
	// In this method, you are inside a node tile.
	// The goal is to determine, for each node of the node tile, which type of data it holds.
	// Data type can be :
	// - a constant region,
	// - a region with data,
	// - a region where max resolution is reached.
	// So, one thread is responsible of the production of one node of a node tile.

	// Retrieve current node tile localization information code and depth
	const GvCore::GvLocalizationInfo::CodeType *parentLocCode = &parentLocInfo.locCode;
	const GvCore::GvLocalizationInfo::DepthType *parentLocDepth = &parentLocInfo.locDepth;

	// Process ID gives the 1D index of a node in the current node tile
	if(processID >= NodeRes::getNumElements()) return 0U;

	// First, compute the 3D offset of the node in the node tile
	const uint3 subOffset = NodeRes::toFloat3(processID);

	// Node production corresponds to subdivide a node tile.
	// So, based on the index of the node, find the node child.
	// As we want to sudbivide the curent node, we retrieve localization information
	// at the next level
	const uint3 regionCoords = parentLocCode->addLevel<NodeRes>(subOffset).get();
	const uint regionDepth = parentLocDepth->addLevel().get();

	// Create a new node for which you will have to fill its information.
	GvStructure::GvNode newnode;
	newnode.childAddress = 0U;
	newnode.brickAddress = 0U;

	// Call what we call an oracle that will determine the type of the region of the node accordingly
	GPUVoxelProducer::GPUVPRegionInfo nodeinfo = getRegionInfo(regionCoords, regionDepth);

	// Now that the type of the region is found, fill the new node information
	switch(nodeinfo) {
		case GPUVoxelProducer::GPUVP_CONSTANT:
			newnode.setTerminal(true);
			break;
		case GPUVoxelProducer::GPUVP_DATA:
			newnode.setStoreBrick();
			newnode.setTerminal(false);
			break;
		case GPUVoxelProducer::GPUVP_DATA_MAXRES:
			newnode.setStoreBrick();
			newnode.setTerminal(true);
			break;
	}

	// Finally, write the new node information into the node pool by selecting channels :
	// - Loki::Int2Type< 0 >() points to node information
	// - Loki::Int2Type< 1 >() points to brick information
	//
	// newElemAddress.x + processID : is the adress of the new node in the node pool
	nodePool.getChannel(Loki::Int2Type<0>()).set(newElemAddress.x + processID, newnode.childAddress);
	nodePool.getChannel(Loki::Int2Type<1>()).set(newElemAddress.x + processID, newnode.brickAddress);

	return 0U;
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
template <typename TDataStructureType>
template <typename GPUPoolKernelType>
__device__
inline uint ProducerKernel<TDataStructureType>::produceData(
	GPUPoolKernelType& dataPool,
	uint requestID,
	uint processID,
	uint3 newElemAddress,
	const GvCore::GvLocalizationInfo& parentLocInfo,
	Loki::Int2Type<1>
) {
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
	levelRes = make_uint3( 1 << parentLocDepth.get() ) * brickRes;
	levelResInv = make_float3( 1.0f ) / make_float3( levelRes );

	brickPos = make_int3( parentLocCode.get() * brickRes ) - BorderSize;
	brickPosF = make_float3( brickPos ) * levelResInv;

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

    // - number of voxels
    const uint3 elemSize = BrickRes::get() + make_uint3(2 * BorderSize); //brick's edge length with borders
    const uint nbVoxels = elemSize.x * elemSize.y * elemSize.z;
    // - number of threads
    const uint nbThreads = blockDim.x;
    // - global thread index in the block (linearized)
    //const int threadIndex1D = threadIdx.z * ( blockDim.x * blockDim.y ) + ( threadIdx.y * blockDim.x + threadIdx.x ); // written in FMAD-style
    const int threadIndex1D = threadIdx.x;
    uint3 locOffset;
    for ( int index = threadIndex1D; index < nbVoxels; index += nbThreads )
    {
       // Transform 1D per blocks global thread index to associated threads 3D voxel position
       locOffset.x = index % elemSize.x;
	   locOffset.y = ( index / elemSize.x ) % elemSize.y;
	   locOffset.z = index / ( elemSize.x * elemSize.y );

	   // Position of the current voxel's center (relative to the brick)
	   float3 voxelPosInBrickF = ( make_float3( locOffset ) + 0.5f ) * levelResInv;
	   // Position of the current voxel's center (absolute, in [0.0;1.0] range)
	   float3 voxelPosF = brickPosF + voxelPosInBrickF;
	   // Position of the current voxel's center (scaled to the range [-1.0;1.0])
	   float3 posF = voxelPosF * 2.0f - 1.0f;

	   float4 voxelColor = make_float4( cShapeColor.x, cShapeColor.y, cShapeColor.z, 0.0f );

		// //for the sphere
		// float4 voxelNormal = make_float4( normalize( posF ), 1.0f );

		// //for the cube
		// float m;//	= max(fabsf(posF.x), fabsf(posF.y), fabsf(posF.z));
		// if(fabsf(posF.x) >= fabsf(posF.y) && fabsf(posF.x) >= fabsf(posF.z))
		// 	m = fabsf(posF.x);
		// else if(fabsf(posF.y) >= fabsf(posF.x) && fabsf(posF.y) >= fabsf(posF.z))
		// 	m = fabsf(posF.y);
		// else
		// 	m = fabsf(posF.z);
		// float4 voxelNormal = make_float4( normalize(make_float3(
		// 	((float)(powf(-1.f, posF.x < 0.0f) * posF.x == m)),
		// 	((float)(powf(-1.f, posF.y < 0.0f) * posF.y == m)),
		// 	((float)(powf(-1.f, posF.z < 0.0f) * posF.z == m)))),
		// 	1.0f
		// );

		//for the cylinder
		const float4 voxelNormal = make_float4(cylinderNormal(posF), 1.f);

	   // Test if the voxel is located inside the unit cube.
	   if ( isInCylinder( posF ) )
	   {
		   voxelColor.w = cShapeOpacity;
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
template <typename TDataStructureType>
__device__
inline GPUVoxelProducer::GPUVPRegionInfo ProducerKernel<TDataStructureType>::getRegionInfo(
	uint3 regionCoords,
	uint regionDepth
) {
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

	// brickRes = BrickRes::get();
	brickRes = dim3(9U);

	levelRes = make_uint3( 1 << regionDepth ) * brickRes;
	levelResInv = make_float3( 1.f ) / make_float3( levelRes );

	int3 brickPos = make_int3(regionCoords * brickRes) - BorderSize;
	float3 brickPosF = make_float3( brickPos ) * levelResInv;

	// Since we work in the range [-1;1] below, the brick size is two time bigger
	brickSize = make_float3( 1.f ) / make_float3( 1 << regionDepth ) * 2.f;

	////////////////////////////////////////////// variable level of subdivision to check node data type
	float3 q000 = make_float3( regionCoords * brickRes ) * levelResInv * 2.f - 1.f;
	uint inNb = 0U;
	uint i, j, k;
	for(i=0U; i<=brickRes.x; i++)
		for(j=0U; j<=brickRes.y; j++)
			for(k=0U; k<=brickRes.z; k++)
				inNb += isInCylinder(make_float3(
					q000.x + i*brickSize.x/brickRes.x,
					q000.y + j*brickSize.y/brickRes.y,
					q000.z + k*brickSize.z/brickRes.z
				));

	if(inNb == 0U)
		return GPUVoxelProducer::GPUVP_CONSTANT;
	else if(inNb < (brickRes.x + 1U) * (brickRes.y + 1U) * (brickRes.z + 1U))
		return GPUVoxelProducer::GPUVP_DATA;
	else
		return GPUVoxelProducer::GPUVP_DATA_MAXRES;
	////////////////////////////////////////////// subdivision to check node data type limited to corners
	// // Build the eight brick corners of a cube centered in [0;0;0]
	// float3 q000 = make_float3( regionCoords * brickRes ) * levelResInv * 2.f - 1.f;
	// float3 q001 = make_float3( q000.x + brickSize.x, q000.y,			   q000.z);
	// float3 q010 = make_float3( q000.x,				 q000.y + brickSize.y, q000.z);
	// float3 q011 = make_float3( q000.x + brickSize.x, q000.y + brickSize.y, q000.z);
	// float3 q100 = make_float3( q000.x,				 q000.y,			   q000.z + brickSize.z);
	// float3 q101 = make_float3( q000.x + brickSize.x, q000.y,			   q000.z + brickSize.z);
	// float3 q110 = make_float3( q000.x,				 q000.y + brickSize.y, q000.z + brickSize.z);
	// float3 q111 = make_float3( q000.x + brickSize.x, q000.y + brickSize.y, q000.z + brickSize.z);
	//
	// // // Test if any of the eight brick corner lies in the cube
	// // if ( (isInObject( q000 ) && isInObject( q001 ) && isInObject( q010 ) && isInObject( q011 ) &&
	// // 	isInObject( q100 ) && isInObject( q101 ) && isInObject( q110 ) && isInObject( q111 ))
	// // ) return GPUVoxelProducer::GPUVP_DATA_MAXRES;
	// // else if(!(isInObject( q000 ) && isInObject( q001 ) && isInObject( q010 ) && isInObject( q011 ) &&
	// // 		isInObject( q100 ) && isInObject( q101 ) && isInObject( q110 ) && isInObject( q111 ))
	// // ) return GPUVoxelProducer::GPUVP_DATA_MAXRES;
	// // else return GPUVoxelProducer::GPUVP_DATA;
	//
	// if ( isInObject( q000 ) || isInObject( q001 ) || isInObject( q010 ) || isInObject( q011 ) ||
	// 	isInObject( q100 ) || isInObject( q101 ) || isInObject( q110 ) || isInObject( q111 ) )
	// {
	// 	return GPUVoxelProducer::GPUVP_DATA;
	// }
	//
	// return GPUVoxelProducer::GPUVP_CONSTANT;
	/////////////////////////////////////////////////////////////////////////////////////////////////////////
}


template <typename TDataStructureType>
__device__
inline bool ProducerKernel<TDataStructureType>::isInObject(const float3 pPoint) {
	return isInCylinder(pPoint);
}

template <typename TDataStructureType>
__device__
inline bool ProducerKernel<TDataStructureType>::isInSphere(const float3 pPoint) {
	return length(pPoint) < 1.f;
}

template <typename TDataStructureType>
__device__
inline bool ProducerKernel<TDataStructureType>::isInCube(const float3 pPoint) {
	return pPoint.x < 1.f && pPoint.y < 1.f && pPoint.z < 1.f;
}

template <typename TDataStructureType>
__device__
inline bool ProducerKernel<TDataStructureType>::isInCylinder(const float3 pPoint) {
	//The default cylinder's first base center is placed on the floor (Oxy) at the origin, the second up of 1z unit and has a unit radius influenced by the displacement map.
	//The direction of the angle 0 is aligned with the x axis.
	const float3 center1			= make_float3(0.f);
	const float3 baseToBase 		= make_float3(0.f, 0.f, 1.f);
	const float height 				= 1.f;
	const float3 axis 				= make_float3(0.f, 0.f, 1.f);
	const float3 baseToPoint 		= pPoint - center1;
	const float scalar 				= dot(baseToPoint, axis);
	const float3 projected 			= center1 + axis * scalar;
	const float3 projectedToPoint 	= pPoint - projected;

	//WARNING! A point on the cylinder's axis has no horizontal/angle parameterization.
	if(length(projectedToPoint) <= 0.f) return false;

	const float u = acos(dot(make_float3(1.f, 0.f, 0.f), normalize(projectedToPoint)))/PRODUCER_PI; //horizontal/angle parameterization
	const float v = scalar/height; //vertical parameterization
	const float displacement = tex2D(cDisplacementMap, u, v);
	//TODO:parameter for max/min radius
	const float radius = 0.5f * displacement;//clamp(displacement, 0.f, 1000.f)/1000.f;

	return 0.f <= scalar && scalar <= height && length(projectedToPoint) <= radius;
}

template <typename TDataStructureType>
__device__
inline float3 ProducerKernel<TDataStructureType>::objectNormal(const float3 pPoint) {
	return cylinderNormal(pPoint);
}

template <typename TDataStructureType>
__device__
inline float3 ProducerKernel<TDataStructureType>::cylinderNormal(const float3 pPoint) {
	//TODO: take the future transformation matrix of the cylinder into account

	//This function should be called knowing we are already on or in the cylinder.
	const float3 center1			= make_float3(0.f);
	const float3 baseToBase 		= make_float3(0.f, 0.f, 1.f);
	const float height 				= 1.f;
	const float3 axis 				= make_float3(0.f, 0.f, 1.f);
	const float3 baseToPoint 		= pPoint - center1;
	const float scalar 				= dot(baseToPoint, axis);
	const float3 projected 			= center1 + axis * scalar;
	const float3 projectedToPoint 	= pPoint - projected;

	const float epsilon = 0.01f;
	const float heightRatio = scalar/height;

	//Normal of the upper cup.
	if(1.f - epsilon < heightRatio)
		return axis;
	//Normal of the under cup.
	else if(heightRatio < 0.f + epsilon)
		return -1.f * axis;
	else
		return normalize(projectedToPoint);
}
