﻿////////////////////////////////////////////////////////////////////////////////
//
// FRAGMENT SHADER
//
// Fullscreen quad rendering
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// VERSION
////////////////////////////////////////////////////////////////////////////////

#version 400

////////////////////////////////////////////////////////////////////////////////
// INPUT
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// UNIFORM
////////////////////////////////////////////////////////////////////////////////

// Texture sampler for fullscreen quad rendering
//uniform sampler2DRect uTextureSampler;
uniform sampler2D uTextureSampler;

////////////////////////////////////////////////////////////////////////////////
// OUTPUT
////////////////////////////////////////////////////////////////////////////////

layout (location = 0) out vec4 oColor;

////////////////////////////////////////////////////////////////////////////////
// PROGRAM
////////////////////////////////////////////////////////////////////////////////
void main()
{
	//oColor = texelFetch( uTextureSampler, ivec2( gl_FragCoord.x, gl_FragCoord.y ) );
	oColor = texture( uTextureSampler, vec2( gl_FragCoord.x / 512.0, gl_FragCoord.y / 512.0 ) );
	//oColor = vec4( 0.0, 1.0, 0.0, 1.0 );
}
