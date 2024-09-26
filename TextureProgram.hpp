// This file was created by copying LitColorTextureProgram.hpp, then
// following Jim's process completed in class. To do this, I 
// cross-referenced the code he sent in Discord + Sasha's annotated notes

#pragma once

#include "GL.hpp"
#include "Load.hpp"
#include "Scene.hpp"

//Shader program that draws transformed, lit, textured vertices tinted with vertex colors:
struct TextureProgram {
	TextureProgram();
	~TextureProgram();

	GLuint program = 0;

	//Attribute (per-vertex variable) locations:
	GLuint Position_vec4 = -1U;
	GLuint TexCoord_vec2 = -1U;

	//Uniform (per-invocation variable) locations:
	GLuint CLIP_FROM_LOCAL_mat4 = -1U;

	//Textures:
	//TEXTURE0 - texture that is accessed by TexCoord
};

extern Load< TextureProgram > texture_program;

//For convenient scene-graph setup, copy this object:
// NOTE: by default, has texture bound to 1-pixel white texture -- so it's okay to use with vertex-color-only meshes.

// Commenting the line here because it is not present in Jim's code. Not 100% sure why we would, unless it's specifically
// invoked for lighting.
// extern Scene::Drawable::Pipeline texture_program_pipeline;
