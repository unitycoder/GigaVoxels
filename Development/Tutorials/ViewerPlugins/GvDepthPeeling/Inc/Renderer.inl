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

/******************************************************************************
 ******************************* INCLUDE SECTION ******************************
 ******************************************************************************/

// GigaVoxels
#include <GvCore/GvError.h>

// Cuda SDK
#include <helper_cuda.h>

/******************************************************************************
 ****************************** INLINE DEFINITION *****************************
 ******************************************************************************/

/******************************************************************************
 * Constructor
 *
 * @param pVolumeTree data structure to render
 * @param pVolumeTreeCache cache
 * @param pProducer producer of data
 ******************************************************************************/
template< typename TVolumeTreeType, typename TVolumeTreeCacheType, typename TProducerType, typename TSampleShader >
Renderer< TVolumeTreeType, TVolumeTreeCacheType, TProducerType, TSampleShader >
::Renderer( TVolumeTreeType* pVolumeTree, TVolumeTreeCacheType* pVolumeTreeCache, TProducerType* pProducer )
:	GvRendering::GvVolumeTreeRenderer< TVolumeTreeType, TVolumeTreeCacheType, TProducerType >( pVolumeTree, pVolumeTreeCache, pProducer )
,	_graphicsInteroperabiltyHandler( NULL )
,	_depthPeelingDepthMinResource( NULL )
,	_depthPeelingDepthMaxResource( NULL )
{
	GV_CHECK_CUDA_ERROR( "Renderer::Renderer prestart" );

	// Init frame size
	_frameSize = make_uint2( 0, 0 );

	// Init frame dependant buffers//
	// Deferred lighting infos
	// ...

#ifndef USE_SIMPLE_RENDERER
	// Init ray buffers
	d_rayBufferT			= NULL;
	d_rayBufferTmax			= NULL;
	d_rayBufferAccCol		= NULL;
	_currentDebugRay		= make_int2( -1, -1 );
#endif

	_numUpdateFrames		= 1;
	_frameNumAfterUpdate	= 0;

	_fastBuildMode			= true;

	// Do CUDA initialization
	this->initializeCuda();

	// Initialize graphics interoperability
	_graphicsInteroperabiltyHandler = new GvRendering::GvGraphicsInteroperabiltyHandler();

	// Configure texture samplers
	_depthPeelingDepthMinTex.normalized = 0;
	_depthPeelingDepthMinTex.addressMode[ 0 ] = cudaAddressModeClamp;
	_depthPeelingDepthMinTex.addressMode[ 1 ] = cudaAddressModeClamp;
	_depthPeelingDepthMinTex.addressMode[ 2 ] = cudaAddressModeClamp;
	_depthPeelingDepthMinTex.filterMode = cudaFilterModePoint;
	_depthPeelingDepthMaxTex.normalized = 0;
	_depthPeelingDepthMaxTex.addressMode[ 0 ] = cudaAddressModeClamp;
	_depthPeelingDepthMaxTex.addressMode[ 1 ] = cudaAddressModeClamp;
	_depthPeelingDepthMaxTex.addressMode[ 2 ] = cudaAddressModeClamp;
	_depthPeelingDepthMaxTex.filterMode = cudaFilterModePoint;
}

/******************************************************************************
 * Destructor
 ******************************************************************************/
template< typename TVolumeTreeType, typename TVolumeTreeCacheType, typename TProducerType, typename TSampleShader >
Renderer< TVolumeTreeType, TVolumeTreeCacheType, TProducerType, TSampleShader >
::~Renderer()
{
	// Destroy internal buffers used during rendering
	deleteFrameObjects();

	// Finalize graphics interoperability
	delete _graphicsInteroperabiltyHandler;
}

/******************************************************************************
 * Get the graphics interoperability handler
 *
 * @return the graphics interoperability handler
 ******************************************************************************/
template< typename TVolumeTreeType, typename TVolumeTreeCacheType, typename TProducerType, typename TSampleShader >
GvRendering::GvGraphicsInteroperabiltyHandler* Renderer< TVolumeTreeType, TVolumeTreeCacheType, TProducerType, TSampleShader >
::getGraphicsInteroperabiltyHandler()
{
	return _graphicsInteroperabiltyHandler;
}

/******************************************************************************
 * Initialize Cuda objects
 ******************************************************************************/
template< typename TVolumeTreeType, typename TVolumeTreeCacheType, typename TProducerType, typename TSampleShader >
void Renderer< TVolumeTreeType, TVolumeTreeCacheType, TProducerType, TSampleShader >
::initializeCuda()
{
	GV_CHECK_CUDA_ERROR( "VoxelSceneRenderer::cuda_Init pre-start" );

	// Retrieve device properties
	cudaDeviceProp deviceProps;
	cudaGetDeviceProperties( &deviceProps, gpuGetMaxGflopsDeviceId() );	// TO DO : handle the case where user could want an other device
	std::cout << "\n******Device properties******\n";
	std::cout << "Name: " << deviceProps.name<<"\n";
	std::cout << "Compute capability: "<< deviceProps.major << "." << deviceProps.minor << "\n";
	std::cout << "Compute mode: " << deviceProps.computeMode << "\n";
	std::cout << "Can map host memory: " << deviceProps.canMapHostMemory << "\n";
	std::cout << "Can overlap transfers and kernels: " << deviceProps.deviceOverlap << "\n";
	std::cout << "Kernels timeout: " << deviceProps.kernelExecTimeoutEnabled << "\n";
	std::cout << "Integrated chip: " << deviceProps.integrated << "\n";
	std::cout << "Global memory: " << deviceProps.totalGlobalMem / 1024 / 1024 << "MB\n";
	std::cout << "Shared memory: " << deviceProps.sharedMemPerBlock / 1024 << "KB\n";
	std::cout << "Clock rate: " << deviceProps.clockRate / 1000 << "MHz\n";
	std::cout << "*****************************\n\n";

	GV_CHECK_CUDA_ERROR( "VoxelSceneRenderer::cuda_Init start" );

#ifndef USE_SIMPLE_RENDERER
	cudaStreamCreate( &_cudaStream[ 0 ] );
#endif

	GV_CHECK_CUDA_ERROR( "VoxelSceneRenderer::cuda_Init end" );
}

/******************************************************************************
 * Finalize Cuda objects
 ******************************************************************************/
template< typename TVolumeTreeType, typename TVolumeTreeCacheType, typename TProducerType, typename TSampleShader >
void Renderer< TVolumeTreeType, TVolumeTreeCacheType, TProducerType, TSampleShader >
::finalizeCuda()
{
}

/******************************************************************************
 * Initialize internal buffers used during rendering
 * (i.e. input/ouput color and depth buffers, ray buffers, etc...).
 * Buffers size are dependent of the frame size.
 *
 * @param pFrameSize the frame size
 ******************************************************************************/
template< typename TVolumeTreeType, typename TVolumeTreeCacheType, typename TProducerType, typename TSampleShader >
void Renderer< TVolumeTreeType, TVolumeTreeCacheType, TProducerType, TSampleShader >
::initFrameObjects( const uint2& pFrameSize )
{
	// Check if frame size has been modified
	if ( _frameSize.x != pFrameSize.x || _frameSize.y != pFrameSize.y )
	{
		_frameSize = pFrameSize;

		// Destruct frame based objects
		deleteFrameObjects();

#ifndef USE_SIMPLE_RENDERER
		// Ray buffers
		uint2 frameResWithBlock =
				make_uint2(		iDivUp( _frameSize.x, RenderBlockResolution::x ) * RenderBlockResolution::x,
								iDivUp( _frameSize.x, RenderBlockResolution::y ) * RenderBlockResolution::y );	// <== QUESTION : Is that _frameSize.x or _frameSize.y ?? Should be _frameSize.y, no ??
		d_rayBufferT = new GvCore::Array3DGPULinear< float >( make_uint3( frameResWithBlock.x, frameResWithBlock.y, 1 ) );
		d_rayBufferTmax = new GvCore::Array3DGPULinear< float >( make_uint3( frameResWithBlock.x, frameResWithBlock.y, 1 ) );
		d_rayBufferAccCol = new GvCore::Array3DGPULinear< float4 >( make_uint3( frameResWithBlock.x, frameResWithBlock.y, 1 ) );
#endif
	}

	GV_CHECK_CUDA_ERROR( "VoxelSceneRenderer::initFrameObjects" );
}

/******************************************************************************
 * Destroy internal buffers used during rendering
 * (i.e. input/ouput color and depth buffers, ray buffers, etc...)
 * Buffers size are dependent of the frame size.
 ******************************************************************************/
template< typename TVolumeTreeType, typename TVolumeTreeCacheType, typename TProducerType, typename TSampleShader >
void Renderer< TVolumeTreeType, TVolumeTreeCacheType, TProducerType, TSampleShader >
::deleteFrameObjects()
{
	// Destroy input/output color and depth buffers
	// ...

#ifndef USE_SIMPLE_RENDERER
	// Destroy ray buffers
	if ( d_rayBufferT )
	{
		delete d_rayBufferT;
		d_rayBufferT = NULL;
	}
	if ( d_rayBufferTmax )
	{
		delete d_rayBufferTmax;
		d_rayBufferTmax  = NULL;
	}
	if ( d_rayBufferAccCol )
	{
		delete d_rayBufferAccCol;
		d_rayBufferAccCol = NULL;
	}
#endif
}

/******************************************************************************
 * Attach an OpenGL buffer object (i.e. a PBO, a VBO, etc...) to an internal graphics resource 
 * that will be mapped to a color or depth slot used during rendering.
 *
 * @param pGraphicsResourceSlot the associated internal graphics resource (color or depth) and its type of access (read, write or read/write)
 * @param pBuffer the OpenGL buffer
 *
 * @return a flag telling wheter or not it succeeds
 ******************************************************************************/
template< typename TVolumeTreeType, typename TVolumeTreeCacheType, typename TProducerType, typename TSampleShader >
bool Renderer< TVolumeTreeType, TVolumeTreeCacheType, TProducerType, TSampleShader >
::connect( GvRendering::GvGraphicsInteroperabiltyHandler::GraphicsResourceSlot pGraphicsResourceSlot, GLuint pBuffer )
{
	return _graphicsInteroperabiltyHandler->connect( pGraphicsResourceSlot, pBuffer );
}

/******************************************************************************
 * Attach an OpenGL texture or renderbuffer object to an internal graphics resource 
 * that will be mapped to a color or depth slot used during rendering.
 *
 * @param pGraphicsResourceSlot the associated internal graphics resource (color or depth) and its type of access (read, write or read/write)
 * @param pImage the OpenGL texture or renderbuffer object
 * @param pTarget the target of the OpenGL texture or renderbuffer object
 *
 * @return a flag telling wheter or not it succeeds
 ******************************************************************************/
template< typename TVolumeTreeType, typename TVolumeTreeCacheType, typename TProducerType, typename TSampleShader >
bool Renderer< TVolumeTreeType, TVolumeTreeCacheType, TProducerType, TSampleShader >
::connect( GvRendering::GvGraphicsInteroperabiltyHandler::GraphicsResourceSlot pGraphicsResourceSlot, GLuint pImage, GLenum pTarget )
{
	return _graphicsInteroperabiltyHandler->connect( pGraphicsResourceSlot, pImage, pTarget );
}

/******************************************************************************
 * Dettach an OpenGL buffer object (i.e. a PBO, a VBO, etc...), texture or renderbuffer object
 * to its associated internal graphics resource mapped to a color or depth slot used during rendering.
 *
 * @param pGraphicsResourceSlot the internal graphics resource slot (color or depth)
 *
 * @return a flag telling wheter or not it succeeds
 ******************************************************************************/
template< typename TVolumeTreeType, typename TVolumeTreeCacheType, typename TProducerType, typename TSampleShader >
bool Renderer< TVolumeTreeType, TVolumeTreeCacheType, TProducerType, TSampleShader >
::disconnect( GvRendering::GvGraphicsInteroperabiltyHandler::GraphicsResourceSlot pGraphicsResourceSlot )
{
	return _graphicsInteroperabiltyHandler->disconnect( pGraphicsResourceSlot );
}

/******************************************************************************
 * Disconnect all registered graphics resources
 *
 * @return a flag telling wheter or not it succeeds
 ******************************************************************************/
template< typename TVolumeTreeType, typename TVolumeTreeCacheType, typename TProducerType, typename TSampleShader >
bool Renderer< TVolumeTreeType, TVolumeTreeCacheType, TProducerType, TSampleShader >
::resetGraphicsResources()
{
	return _graphicsInteroperabiltyHandler->reset();
}

/******************************************************************************
 * This function is the specific implementation method called
 * by the parent GvIRenderer::render() method during rendering.
 *
 * @param pModelMatrix the current model matrix
 * @param pViewMatrix the current view matrix
 * @param pProjectionMatrix the current projection matrix
 * @param pViewport the viewport configuration
 ******************************************************************************/
template< typename TVolumeTreeType, typename TVolumeTreeCacheType, typename TProducerType, typename TSampleShader >
void Renderer< TVolumeTreeType, TVolumeTreeCacheType, TProducerType, TSampleShader >
::renderImpl( const float4x4& pModelMatrix, const float4x4& pViewMatrix, const float4x4& pProjectionMatrix, const int4& pViewport )
{
//	assert( _colorResource != NULL && _depthResource != NULL ); // && "You must set the input buffers first");

	// Initialize frame objects
	uint2 frameSize = make_uint2( pViewport.z - pViewport.x, pViewport.w - pViewport.y );
	initFrameObjects( frameSize );

	// Map graphics resources
	CUDAPM_START_EVENT( vsrender_pre_frame_mapbuffers );

	_graphicsInteroperabiltyHandler->mapResources();
	bindGraphicsResources();

	// Map textures
	cudaArray* depthPeelingDepthMinArray;
	cudaArray* depthPeelingDepthMaxArray;
	GV_CUDA_SAFE_CALL( cudaGraphicsMapResources( 1, &_depthPeelingDepthMinResource, 0 ) );
    GV_CUDA_SAFE_CALL( cudaGraphicsSubResourceGetMappedArray( &depthPeelingDepthMinArray, _depthPeelingDepthMinResource, 0, 0 ) );
	GV_CUDA_SAFE_CALL( cudaGraphicsMapResources( 1, &_depthPeelingDepthMaxResource, 0 ) );
    GV_CUDA_SAFE_CALL( cudaGraphicsSubResourceGetMappedArray( &depthPeelingDepthMaxArray, _depthPeelingDepthMaxResource, 0, 0 ) );
	// Bind textures
    GV_CUDA_SAFE_CALL( cudaBindTextureToArray( _depthPeelingDepthMinTex, depthPeelingDepthMinArray ) );
    GV_CUDA_SAFE_CALL( cudaBindTextureToArray( _depthPeelingDepthMaxTex, depthPeelingDepthMaxArray ) );

	CUDAPM_STOP_EVENT( vsrender_pre_frame_mapbuffers );

	// Start rendering
	doRender( pModelMatrix, pViewMatrix, pProjectionMatrix, pViewport );

	// Unmap graphics resources
	CUDAPM_START_EVENT( vsrender_post_frame_unmapbuffers );
	unbindGraphicsResources();
	_graphicsInteroperabiltyHandler->unmapResources();

	// Unmap textures
	GV_CUDA_SAFE_CALL( cudaUnbindTexture( _depthPeelingDepthMinTex ) );
	GV_CUDA_SAFE_CALL( cudaUnbindTexture( _depthPeelingDepthMaxTex ) );
	// Unbind textures
	GV_CUDA_SAFE_CALL( cudaGraphicsUnmapResources( 1, &_depthPeelingDepthMinResource, 0 ) );
	GV_CUDA_SAFE_CALL( cudaGraphicsUnmapResources( 1, &_depthPeelingDepthMaxResource, 0 ) );

	CUDAPM_STOP_EVENT( vsrender_post_frame_unmapbuffers );
}

/******************************************************************************
 * Start the rendering process.
 *
 * @param pModelMatrix the current model matrix
 * @param pViewMatrix the current view matrix
 * @param pProjectionMatrix the current projection matrix
 ******************************************************************************/
template< typename TVolumeTreeType, typename TVolumeTreeCacheType, typename TProducerType, typename TSampleShader >
void Renderer< TVolumeTreeType, TVolumeTreeCacheType, TProducerType, TSampleShader >
::doRender( const float4x4& modelMatrix, const float4x4& viewMatrix, const float4x4& projMatrix, const int4& pViewport )
{
	// Create a render view context to access to useful variables during (view matrix, model matrix, etc...)
	GvRendering::GvRendererContext viewContext;

	// Check if a "clear request" has been asked
	if ( this->_clearRequested )
	{
		CUDAPM_START_EVENT( vsrender_clear );

			_frameNumAfterUpdate = 0;
			_fastBuildMode = true;

			// Clear the cache
			if ( this->_volumeTreeCache )
			{
				this->_volumeTreeCache->clearCache();
			}

			// bug correction
			//
			this->_volumeTree->clearVolTree();
			//

		CUDAPM_STOP_EVENT( vsrender_clear );

		// Update "clear request" flag
		this->_clearRequested = false;
	}

	// Extract zNear, zFar as well as the distance in view space
	// from the center of the screen to each side of the screen.
	float fleft   = projMatrix._array[ 14 ] * ( projMatrix._array[ 8 ] - 1.0f ) / ( projMatrix._array[ 0 ] * ( projMatrix._array[ 10 ] - 1.0f ) );
	float fright  = projMatrix._array[ 14 ] * ( projMatrix._array[ 8 ] + 1.0f ) / ( projMatrix._array[ 0 ] * ( projMatrix._array[ 10 ] - 1.0f ) );
	float ftop    = projMatrix._array[ 14 ] * ( projMatrix._array[ 9 ] + 1.0f ) / ( projMatrix._array[ 5 ] * ( projMatrix._array[ 10 ] - 1.0f ) );
	float fbottom = projMatrix._array[ 14 ] * ( projMatrix._array[ 9 ] - 1.0f ) / ( projMatrix._array[ 5 ] * ( projMatrix._array[ 10 ] - 1.0f ) );
	float fnear   = projMatrix._array[ 14 ] / ( projMatrix._array[ 10 ] - 1.0f );
	float ffar    = projMatrix._array[ 14 ] / ( projMatrix._array[ 10 ] + 1.0f );
	
	float2 viewSurfaceVS[ 2 ];
	viewSurfaceVS[ 0 ] = make_float2( fleft, fbottom );
	viewSurfaceVS[ 1 ] = make_float2( fright, ftop );

	float3 viewPlane[ 2 ];
	viewPlane[ 0 ] = make_float3( fleft, fbottom, fnear );
	viewPlane[ 1 ] = make_float3( fright, ftop, fnear );
	// float3 viewSize = ( viewPlane[ 1 ] - viewPlane[ 0 ] );

	// Projected 2D Bounding Box of the GigaVoxels 3D BBox.
	// It holds its bottom left corner and its size.
	// ( bottomLeftCorner.x, bottomLeftCorner.y, frameSize.x, frameSize.y )
	viewContext._projectedBBox = this->_projectedBBox;
	
	float2 viewSurfaceVS_Size = viewSurfaceVS[ 1 ] - viewSurfaceVS[ 0 ];
	
	// Transform matrices
	float4x4 invModelMatrixT = transpose( inverse( modelMatrix ) );
	float4x4 invViewMatrixT = transpose( inverse( viewMatrix ) );

	float4x4 modelMatrixT = transpose( modelMatrix );
	float4x4 viewMatrixT = transpose( viewMatrix );

	//float4x4 viewMatrix=(inverse(invViewMatrix));

	viewContext.invViewMatrix = invViewMatrixT;
	viewContext.viewMatrix = viewMatrixT;
	viewContext.invModelMatrix = invModelMatrixT;
	viewContext.modelMatrix = modelMatrixT;

	// Store frustum parameters
	viewContext.frustumNear = fnear;
	viewContext.frustumNearINV = 1.0f / fnear;
	viewContext.frustumFar = ffar;
	viewContext.frustumRight = fright;
	viewContext.frustumTop = ftop;
	viewContext.frustumC = projMatrix._array[ 10 ]; // - ( ffar + fnear ) / ( ffar - fnear );
	viewContext.frustumD = projMatrix._array[ 14 ]; // ( -2.0f * ffar * fnear ) / ( ffar - fnear );

	// Graphics resource settings
	viewContext._clearColor = this->_clearColor;
	viewContext._clearDepth = this->_clearDepth;
	/*bindGraphicsResources();*/
	_graphicsInteroperabiltyHandler->setRendererContextInfo( viewContext );
	// TO DO : add texture offsets !!!!!!!!!!!!
	// ...
	
	// WORLD
	float3 viewPlanePosWP = mul( viewContext.invViewMatrix, make_float3( fleft, fbottom, -fnear ) );
	viewContext.viewCenterWP = mul( viewContext.invViewMatrix, make_float3( 0.0f, 0.0f, 0.0f ) );
	viewContext.viewPlaneDirWP = viewPlanePosWP - viewContext.viewCenterWP;
	// TREE
	float3 viewPlanePosTP = mul( viewContext.invModelMatrix, viewPlanePosWP );
	viewContext.viewCenterTP = mul( viewContext.invModelMatrix, viewContext.viewCenterWP );
	viewContext.viewPlaneDirTP = viewPlanePosTP - viewContext.viewCenterTP;

	// Resolution dependant stuff
	viewContext.frameSize = _frameSize;
	float2 pixelSize = viewSurfaceVS_Size / make_float2( static_cast< float >( viewContext.frameSize.x ), static_cast< float >( viewContext.frameSize.y ) );
	viewContext.pixelSize = pixelSize;
	/*viewContext.viewPlaneXAxisWP = ( mul( viewContext.invViewMatrix, make_float3( fright, fbottom, -fnear ) ) - viewPlanePosWP ) / static_cast< float >( viewContext.frameSize.x );
	viewContext.viewPlaneYAxisWP = ( mul( viewContext.invViewMatrix, make_float3( fleft, ftop, -fnear ) ) - viewPlanePosWP ) / static_cast< float >( viewContext.frameSize.y );*/
	// WORLD
	viewContext.viewPlaneXAxisWP = mul( viewContext.invViewMatrix, make_float3( fright, fbottom, -fnear ) );
	viewContext.viewPlaneYAxisWP = mul( viewContext.invViewMatrix, make_float3( fleft, ftop, -fnear ) );
	// TREE
	viewContext.viewPlaneXAxisTP = mul( viewContext.invModelMatrix, viewContext.viewPlaneXAxisWP );
	viewContext.viewPlaneYAxisTP = mul( viewContext.invModelMatrix, viewContext.viewPlaneYAxisWP );
	// WORLD
	viewContext.viewPlaneXAxisWP = ( viewContext.viewPlaneXAxisWP - viewPlanePosWP ) / static_cast< float >( viewContext.frameSize.x );
	viewContext.viewPlaneYAxisWP = ( viewContext.viewPlaneYAxisWP - viewPlanePosWP ) / static_cast< float >( viewContext.frameSize.y );
	// TREE
	viewContext.viewPlaneXAxisTP = ( viewContext.viewPlaneXAxisTP - viewPlanePosTP ) / static_cast< float >( viewContext.frameSize.x );
	viewContext.viewPlaneYAxisTP = ( viewContext.viewPlaneYAxisTP - viewPlanePosTP ) / static_cast< float >( viewContext.frameSize.y );

	CUDAPM_START_EVENT( vsrender_copyconsts_frame );

		int maxVolTreeDepth = this->_volumeTree->getMaxDepth();

		// TEST, OPTIM : early preRender pass
		//this->_volumeTreeCache->preRenderPass();

		GV_CUDA_SAFE_CALL( cudaMemcpyToSymbol( k_renderViewContext, &viewContext, sizeof( viewContext ) ) );
		GV_CUDA_SAFE_CALL( cudaMemcpyToSymbol( k_currentTime, (&this->_currentTime), sizeof( this->_currentTime ), 0, cudaMemcpyHostToDevice) );
		GV_CUDA_SAFE_CALL( cudaMemcpyToSymbol( k_maxVolTreeDepth, &maxVolTreeDepth, sizeof( maxVolTreeDepth ), 0, cudaMemcpyHostToDevice) );
	
		// TO DO : move this "Performance monitor" piece of code in another place
	#ifdef USE_CUDAPERFMON
		// Performance monitor
		if ( GvPerfMon::CUDAPerfMon::getApplicationPerfMon()._requestResize ) 
		{
			// Update device memory
			GvCore::Array3DGPULinear< GvCore::uint64 >* my_d_timersArray = GvPerfMon::CUDAPerfMon::getApplicationPerfMon().getKernelTimerArray();
			GvCore::Array3DKernelLinear< GvCore::uint64 > h_timersArray = my_d_timersArray->getDeviceArray();
			GV_CUDA_SAFE_CALL( cudaMemcpyToSymbol( GvPerfMon::k_timersArray, &h_timersArray, sizeof( h_timersArray ), 0, cudaMemcpyHostToDevice ) );
		
			// Update device memory
			uchar* my_d_timersMask = GvPerfMon::CUDAPerfMon::getApplicationPerfMon().getKernelTimerMask();
			GV_CUDA_SAFE_CALL( cudaMemcpyToSymbol( GvPerfMon::k_timersMask, &my_d_timersMask, sizeof( my_d_timersMask ), 0, cudaMemcpyHostToDevice ) );

			// Update the performnace monitor's state
			GvPerfMon::CUDAPerfMon::getApplicationPerfMon()._requestResize = false;
		}
	#endif
	
	CUDAPM_STOP_EVENT( vsrender_copyconsts_frame );

	float voxelSizeMultiplier = 1.0f;
	GV_CUDA_SAFE_CALL( cudaMemcpyToSymbol( GvRendering::k_voxelSizeMultiplier, (&voxelSizeMultiplier), sizeof( voxelSizeMultiplier ), 0, cudaMemcpyHostToDevice ) );
	
	// Do pre-render pass
	this->_volumeTreeCache->preRenderPass();

	_frameNumAfterUpdate = 0;

	_fastBuildMode = false;

	CUDAPM_START_EVENT_GPU( vsrender_gigavoxels );

	dim3 blockSize( RenderBlockResolution::x, RenderBlockResolution::y, 1 );
	dim3 gridSize( iDivUp( _frameSize.x, RenderBlockResolution::x ), iDivUp( _frameSize.y, RenderBlockResolution::y ), 1 );
	// FUTUR optimization
	//
	//dim3 gridSize( iDivUp( /*projectedBBoxSize*/_projectedBBox.z, RenderBlockResolution::x ), iDivUp( /*projectedBBoxSize*/_projectedBBox.w, RenderBlockResolution::y ), 1 );
	
	if ( this->_dynamicUpdate )
	{
		if ( this->_hasPriorityOnBricks )
		{
			// Priority on brick is set to TRUE to force loading data at low resolution first
			RenderKernelSimple< RenderBlockResolution, false, true, TSampleShader >
							<<< gridSize, blockSize >>>(
								this->_volumeTree->volumeTreeKernel,
								this->_volumeTreeCache->getKernelObject() );
		}
		else
		{
			RenderKernelSimple< RenderBlockResolution, false, false, TSampleShader >
							<<< gridSize, blockSize >>>(
								this->_volumeTree->volumeTreeKernel,
								this->_volumeTreeCache->getKernelObject() );
		}
	}
	else
	{
		RenderKernelSimple< RenderBlockResolution, false, false, TSampleShader >
						<<< gridSize, blockSize >>>(
								this->_volumeTree->volumeTreeKernel,
								this->_volumeTreeCache->getKernelObject() );
	}

	GV_CHECK_CUDA_ERROR( "RenderKernelSimple" );

	CUDAPM_STOP_EVENT_GPU( vsrender_gigavoxels );
	
	// TEST, OPTIM : early unmap
	// Unmap graphics resources
	//_graphicsInteroperabiltyHandler->unmapResources();
		
	CUDAPM_START_EVENT( vsrender_load );

	// Bricks loading
	if ( this->_dynamicUpdate )
	{
		// dynamicUpdate = false;

		this->_volumeTreeCache->_intraFramePass = false;

		// Post render pass
		// This is where requests are processed : produce or load data
		this->_volumeTreeCache->postRenderPass();
	}

	CUDAPM_STOP_EVENT( vsrender_load );

	CUDAPM_RENDER_CACHE_INFO( 256, 512 );

	/*{
		uint2 poolRes = make_uint2(180, 150);
		uint2 poolScale = make_uint2(2, 2);

		dim3 blockSize(10, 10, 1);
		dim3 gridSize(poolRes.x * poolScale.x / blockSize.x, poolRes.y * poolScale.y / blockSize.y, 1);
		RenderDebug<<<gridSize, blockSize, 0>>>(d_outFrameColor->getDeviceArray(), poolRes, poolScale);
	}*/

	_frameNumAfterUpdate++;
}

#ifndef USE_SIMPLE_RENDERER
	/******************************************************************************
	 * ...
	 *
	 * @param pFrameAfterUpdate ...
	 *
	 * @return ...
	 ******************************************************************************/
	template< typename TVolumeTreeType, typename TVolumeTreeCacheType, typename TProducerType, typename TSampleShader >
	float Renderer< TVolumeTreeType, TVolumeTreeCacheType, TProducerType, TSampleShader >
	::getVoxelSizeMultiplier( uint pFrameAfterUpdate )
	{
		float quality;

		if ( this->_frameNumAfterUpdate < this->_numUpdateFrames )
		{
			quality = this->_updateQuality + ( this->_generalQuality - this->_updateQuality ) * ( static_cast< float >( pFrameAfterUpdate ) / static_cast< float >( this->_numUpdateFrames ) );
		}
		else
		{
			quality = this->_generalQuality;
		}

		return 1.f / quality;
	}
#endif

/******************************************************************************
 * TEST - optimization
 *
 * Launch the post-render phase
 ******************************************************************************/
template< typename TVolumeTreeType, typename TVolumeTreeCacheType, typename TProducerType, typename TSampleShader >
void Renderer< TVolumeTreeType, TVolumeTreeCacheType, TProducerType, TSampleShader >
::doPostRender()
{
	// Bricks loading
	if ( this->_dynamicUpdate )
	{
		// dynamicUpdate = false;

		this->_volumeTreeCache->_intraFramePass = false;

		// Post render pass
		this->_volumeTreeCache->postRenderPass();
	}

	_frameNumAfterUpdate++;
}

/******************************************************************************
 * Bind all graphics resources used by the GL interop handler during rendering.
 *
 * Internally, it binds textures and surfaces to arrays associated to mapped graphics reources.
 *
 * NOTE : this method should be in the GvGraphicsInteroperabiltyHandler but it seems that
 * there are conflicts with textures ans surfaces symbols. The binding succeeds but not the
 * read/write operations.
 *
 * @return a flag telling wheter or not it succeeds
 ******************************************************************************/
template< typename TVolumeTreeType, typename TVolumeTreeCacheType, typename TProducerType, typename TSampleShader >
bool Renderer< TVolumeTreeType, TVolumeTreeCacheType, TProducerType, TSampleShader >
::bindGraphicsResources()
{
	// Iterate through graphics resources info
	std::vector< std::pair< GvRendering::GvGraphicsInteroperabiltyHandler::GraphicsResourceSlot, GvRendering::GvGraphicsResource* > >& graphicsResources = _graphicsInteroperabiltyHandler->editGraphicsResources();
	for ( int i = 0; i < graphicsResources.size(); i++ )
	{
		// Get current graphics resources info
		std::pair< GvRendering::GvGraphicsInteroperabiltyHandler::GraphicsResourceSlot, GvRendering::GvGraphicsResource* >& graphicsResourceInfo = graphicsResources[ i ];
		GvRendering::GvGraphicsInteroperabiltyHandler::GraphicsResourceSlot graphicsResourceSlot = graphicsResourceInfo.first;
		GvRendering::GvGraphicsResource* graphicsResource  = graphicsResourceInfo.second;
		assert( graphicsResource != NULL );

		// [ 2 ] - Bind array to texture or surface if needed
		if ( graphicsResource->getMemoryType() == GvRendering::GvGraphicsResource::eCudaArray )
		{
			struct cudaArray* imageArray = static_cast< struct cudaArray* >( graphicsResource->getMappedAddress() );

			cudaError_t error;
			switch ( graphicsResourceSlot )
			{
				case GvRendering::GvGraphicsInteroperabiltyHandler::eColorReadSlot:
					error = cudaBindTextureToArray( GvRendering::_inputColorTexture, imageArray );
					break;

				case GvRendering::GvGraphicsInteroperabiltyHandler::eColorWriteSlot:
				case GvRendering::GvGraphicsInteroperabiltyHandler::eColorReadWriteSlot:
					error = cudaBindSurfaceToArray( GvRendering::_colorSurface, imageArray );
					break;

				case GvRendering::GvGraphicsInteroperabiltyHandler::eDepthReadSlot:
					error = cudaBindTextureToArray( GvRendering::_inputDepthTexture, imageArray );
					break;

				case GvRendering::GvGraphicsInteroperabiltyHandler::eDepthWriteSlot:
				case GvRendering::GvGraphicsInteroperabiltyHandler::eDepthReadWriteSlot:
					error = cudaBindSurfaceToArray( GvRendering::_depthSurface, imageArray );
					break;

				default:
					assert( false );
					break;
			}
		}
	}

	return false;
}

/******************************************************************************
 * Unbind all graphics resources used by the GL interop handler during rendering.
 *
 * Internally, it unbinds textures and surfaces to arrays associated to mapped graphics reources.
 *
 * NOTE : this method should be in the GvGraphicsInteroperabiltyHandler but it seems that
 * there are conflicts with textures ans surfaces symbols. The binding succeeds but not the
 * read/write operations.
 *
 * @return a flag telling wheter or not it succeeds
 ******************************************************************************/
template< typename TVolumeTreeType, typename TVolumeTreeCacheType, typename TProducerType, typename TSampleShader >
bool Renderer< TVolumeTreeType, TVolumeTreeCacheType, TProducerType, TSampleShader >
::unbindGraphicsResources()
{
	// Iterate through graphics resources info
	std::vector< std::pair< GvRendering::GvGraphicsInteroperabiltyHandler::GraphicsResourceSlot, GvRendering::GvGraphicsResource* > >& graphicsResources = _graphicsInteroperabiltyHandler->editGraphicsResources();
	for ( int i = 0; i < graphicsResources.size(); i++ )
	{
		// Get current graphics resources info
		std::pair< GvRendering::GvGraphicsInteroperabiltyHandler::GraphicsResourceSlot, GvRendering::GvGraphicsResource* >& graphicsResourceInfo = graphicsResources[ i ];
		GvRendering::GvGraphicsInteroperabiltyHandler::GraphicsResourceSlot graphicsResourceSlot = graphicsResourceInfo.first;
		GvRendering::GvGraphicsResource* graphicsResource  = graphicsResourceInfo.second;
		assert( graphicsResource != NULL );

		// [ 2 ] - Bind array to texture or surface if needed
		if ( graphicsResource->getMemoryType() == GvRendering::GvGraphicsResource::eCudaArray )
		{
			struct cudaArray* imageArray = static_cast< struct cudaArray* >( graphicsResource->getMappedAddress() );

			cudaError_t error;
			switch ( graphicsResourceSlot )
			{
				case GvRendering::GvGraphicsInteroperabiltyHandler::eColorReadSlot:
					error = cudaUnbindTexture( GvRendering::_inputColorTexture );
					break;

				case GvRendering::GvGraphicsInteroperabiltyHandler::eColorWriteSlot:
				case GvRendering::GvGraphicsInteroperabiltyHandler::eColorReadWriteSlot:
					// There is no "unbind surface" function in CUDA
					break;

				case GvRendering::GvGraphicsInteroperabiltyHandler::eDepthReadSlot:
					error = cudaUnbindTexture( GvRendering::_inputDepthTexture );
					break;

				case GvRendering::GvGraphicsInteroperabiltyHandler::eDepthWriteSlot:
				case GvRendering::GvGraphicsInteroperabiltyHandler::eDepthReadWriteSlot:
					// There is no "unbind surface" function in CUDA
					break;

				default:
					assert( false );
					break;
			}
		}
	}

	return false;
}

/******************************************************************************
 * Set the Depth Peeling's depth min texture
 *
 * @param pGraphicsResource the associated graphics resource
 ******************************************************************************/
template< typename VolumeTreeType, typename VolumeTreeCacheType, typename ProducerType, typename SampleShader >
void Renderer< VolumeTreeType, VolumeTreeCacheType, ProducerType, SampleShader >
::setDepthPeelingDepthMinTexture( struct cudaGraphicsResource* pGraphicsResource )
{
	_depthPeelingDepthMinResource = pGraphicsResource;
}

/******************************************************************************
 * Set the Depth Peeling's depth max texture
 *
 * @param pGraphicsResource the associated graphics resource
 ******************************************************************************/
template< typename VolumeTreeType, typename VolumeTreeCacheType, typename ProducerType, typename SampleShader >
void Renderer< VolumeTreeType, VolumeTreeCacheType, ProducerType, SampleShader >
::setDepthPeelingDepthMaxTexture( struct cudaGraphicsResource* pGraphicsResource )
{
	_depthPeelingDepthMaxResource = pGraphicsResource;
}
