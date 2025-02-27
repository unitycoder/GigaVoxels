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

// Cuda
#include <driver_types.h>
#include <cuda_runtime.h>

// STL
#include <iostream>

/******************************************************************************
 ****************************** INLINE DEFINITION *****************************
 ******************************************************************************/

/******************************************************************************
 * Constructor
 *
 * @param pDataStructure data structure
 ******************************************************************************/
template< typename TKernelProducerType, typename TDataStructureType, typename TDataProductionManager >
inline HostProducer< TKernelProducerType, TDataStructureType, TDataProductionManager >
::HostProducer()
:	GvUtils::GvSimpleHostProducer< TKernelProducerType, TDataStructureType, TDataProductionManager >()
{
}

/******************************************************************************
 * Destructor
 ******************************************************************************/
template< typename TKernelProducerType, typename TDataStructureType, typename TDataProductionManager >
inline HostProducer< TKernelProducerType, TDataStructureType, TDataProductionManager >
::~HostProducer()
{
	finalize();
}

/******************************************************************************
 * Initialize
 *
 * @param pDataStructure data structure
 * @param pDataProductionManager data production manager
 ******************************************************************************/
template< typename TKernelProducerType, typename TDataStructureType, typename TDataProductionManager >
inline void HostProducer< TKernelProducerType, TDataStructureType, TDataProductionManager >
::initialize( TDataStructureType* pDataStructure, TDataProductionManager* pDataProductionManager )
{
	// Call parent class
	GvUtils::GvSimpleHostProducer< TKernelProducerType, TDataStructureType, TDataProductionManager >::initialize( pDataStructure, pDataProductionManager );
}

/******************************************************************************
 * Finalize
 ******************************************************************************/
template< typename TKernelProducerType, typename TDataStructureType, typename TDataProductionManager >
inline void HostProducer< TKernelProducerType, TDataStructureType, TDataProductionManager >
::finalize()
{
}

/******************************************************************************
 * This method is called by the cache manager when you have to produce data for a given pool.
 * Implement the produceData method for the channel 0 (nodes)
 *
 * @param pNumElems the number of elements you have to produce.
 * @param pNodeAddressCompactList a list containing the addresses of the numElems nodes concerned.
 * @param pElemAddressCompactList a list containing numElems addresses where you need to store the result.
 * @param pGpuPool the pool for which we need to produce elements.
 * @param pPageTable the page table associated to the pool
 * @param Loki::Int2Type< 0 > corresponds to the index of the node pool
 ******************************************************************************/
template< typename TKernelProducerType, typename TDataStructureType, typename TDataProductionManager >
inline void HostProducer< TKernelProducerType, TDataStructureType, TDataProductionManager >
::produceData( uint pNumElems,
				thrust::device_vector< uint >* pNodesAddressCompactList,
				thrust::device_vector< uint >* pElemAddressCompactList,
				Loki::Int2Type< 0 > )
{
	// NOTE : the call to taht function doesn't seem to work (compilation pb due to LOKI...)
	//GvUtils::GvSimpleHostProducer< TKernelProducerType, TDataStructureType, TDataProductionManager >::produceData( pNumElems, pNodesAddressCompactList, pElemAddressCompactList, 0 );

	cudaEvent_t start, stop;

	cudaEventCreate( &start );
	cudaEventCreate( &stop );

	// Initialize the device-side producer
	GvCore::GvIProviderKernel< 0, TKernelProducerType > kernelProvider( this->_kernelProducer );

	// Define kernel block size
	const uint3 kernelBlockSize = TKernelProducerType::NodesKernelBlockSize::get();
	const dim3 blockSize( kernelBlockSize.x, kernelBlockSize.y, kernelBlockSize.z );

	// Retrieve updated addresses
	uint* nodesAddressList = thrust::raw_pointer_cast( &(*pNodesAddressCompactList)[ 0 ] );
	uint* elemAddressList = thrust::raw_pointer_cast( &(*pElemAddressCompactList)[ 0 ] );

	// Call cache helper to write into cache
	//
	// - this function call encapsulates a kernel launch to produce data on device
	// - i.e. the associated device-side producer will call its device function "ProducerKernel::produceData< 0 >()"
	NodePoolType* pool = this->_nodePool;
	NodePageTableType* pageTable = this->_nodePageTable;

	cudaEventRecord( start, 0 );

	this->_cacheHelper.template genericWriteIntoCache< NodeTileResLinear >( pNumElems, nodesAddressList, elemAddressList, pool, kernelProvider, pageTable, blockSize );
	
	cudaEventRecord( stop, 0 );
	cudaEventSynchronize( stop );

	float elapsedTime;
	cudaEventElapsedTime( &elapsedTime, start, stop );

	std::cout << "Elapsed time : " << elapsedTime << std::endl;

	cudaEventDestroy( start );
	cudaEventDestroy( stop );
}

/******************************************************************************
 * This method is called by the cache manager when you have to produce data for a given pool.
 * Implement the produceData method for the channel 1 (bricks)
 *
 * @param pNumElems the number of elements you have to produce.
 * @param pNodeAddressCompactList a list containing the addresses of the numElems nodes concerned.
 * @param pElemAddressCompactList a list containing numElems addresses where you need to store the result.
 * @param pGpuPool the pool for which we need to produce elements.
 * @param pPageTable the page table associated to the pool
 * @param Loki::Int2Type< 1 > corresponds to the index of the brick pool
 ******************************************************************************/
template< typename TKernelProducerType, typename TDataStructureType, typename TDataProductionManager >
inline void HostProducer< TKernelProducerType, TDataStructureType, TDataProductionManager >
::produceData( uint pNumElems,
				thrust::device_vector< uint >* pNodesAddressCompactList,
				thrust::device_vector< uint >* pElemAddressCompactList,
				Loki::Int2Type< 1 > )
{
	// NOTE : the call to taht function doesn't seem to work (compilation pb due to LOKI...)
	//GvUtils::GvSimpleHostProducer< TKernelProducerType, TDataStructureType, TDataProductionManager >::produceData( pNumElems, pNodesAddressCompactList, pElemAddressCompactList, Loki::Int2Type< 1 > );

	// Initialize the device-side producer
	GvCore::GvIProviderKernel< 1, TKernelProducerType > kernelProvider( this->_kernelProducer );

	// Define kernel block size
	const uint3 kernelBlockSize = TKernelProducerType::BricksKernelBlockSize::get();
	const dim3 blockSize( kernelBlockSize.x, kernelBlockSize.y, kernelBlockSize.z );

	// Retrieve updated addresses
	uint* nodesAddressList = thrust::raw_pointer_cast( &(*pNodesAddressCompactList)[ 0 ] );
	uint* elemAddressList = thrust::raw_pointer_cast( &(*pElemAddressCompactList)[ 0 ] );

	// Call cache helper to write into cache
	//
	// - this function call encapsulates a kernel launch to produce data on device
	// - i.e. the associated device-side producer will call its device function "ProducerKernel::produceData< 1 >()"
	DataPoolType* pool = this->_dataPool;
	DataPageTableType* pageTable = this->_dataPageTable;
	this->_cacheHelper.template genericWriteIntoCache< BrickFullRes >( pNumElems, nodesAddressList, elemAddressList, pool, kernelProvider, pageTable, blockSize );
}
