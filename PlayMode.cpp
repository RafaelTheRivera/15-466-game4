#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"
#include "TextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_compile_program.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"
#include "load_save_png.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <hb.h>
#include <hb-ft.h>

#include <iostream>
#include <fstream>
#include <random>

#define FONT_SIZE 36
#define TOPMARGIN (FONT_SIZE * .5)
#define LEFTMARGIN (FONT_SIZE * 2)
#define LINE_SPACING (FONT_SIZE * 0.25)

static constexpr std::string tex_path = "out.png";
static constexpr std::string textbg_path = "textbg.png";
//static constexpr std::string bg_path = "black.png";
static std::string font_path = "SpaceGrotesk-Regular.ttf";
static constexpr uint32_t window_height = 720;
static constexpr uint32_t window_width = 1280;
static glm::u8vec4 text_render[window_height/3][window_width];
static std::vector<std::string> activeScript;
static uint32_t activeIndex = 0;
static uint32_t lastIndex = 0;
static int choices = 0;
static std::vector<std::string> links;

GLuint blank_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > blank_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("blank.pnct"));
	blank_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > blank_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("blank.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = blank_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = blank_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

Load< Sound::Sample > bgm_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("game4bgm.opus"));
});

void clear_png(glm::u8vec4 *png_in, uint32_t height, uint32_t width) {
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            png_in[y*width + x] = glm::u8vec4(0, 0, 0, 0);  // RGBA = (0, 0, 0, 0) = transparent black
        }
    }
}

void draw_png(FT_Bitmap *bitmap, glm::u8vec4 *out, uint32_t x, uint32_t y, uint32_t height, uint32_t width, glm::u8vec4 color) {
	for (uint32_t i = 0; i < bitmap->rows; i++) {
        for (uint32_t j = 0; j < bitmap->width; j++) {
            uint32_t out_x = x + j;
        	uint32_t out_y = y + i;//bitmap->rows - i - 1;
            if (out_x < width && out_y < height) {
				// According to freetype.org,  buffer bytes per row is saved as bitmap->pitch
                uint8_t alpha = bitmap->buffer[i * bitmap->pitch + j];
                if (alpha > 0) {
                    // Calculate the index in the RGBA buffer
                    int index = out_y * width + out_x;
					
					out[index] = glm::u8vec4(color.r, color.g, color.b, alpha);
                }
            }
        }
    }
}

void render_text(std::string line_in, glm::u8vec4 color) {
	choices = 0;
	links;

	glm::u8vec4 colorOut = color; //Overriding it if it's a choice because I want to :D
	std::string line = "";
	//std::cout << std::to_string(int(line_in[0]))<< std::endl;
	//I wanted to get this done before glyphs were in so this was the easiest solution I could find
	if (line_in.length() >= 3 && line_in[0] == -30 && line_in[1] == -126 && line_in[2] == -67 ) {
		choices += 1;
		bool display_mode = true;
		std::string link_now = "";
		int i = 3;
		while(i < line_in.length()-2){
			//new link
			if (line_in[i] == -30 && line_in[i+1] == -126 && line_in[i+2] == -68){
				display_mode = false;
				line = line + "₿";
				i+=3;
				continue;
			}
			//new choice
			if (line_in[i] == -30 && line_in[i+1] == -126 && line_in[i+2] == -67){
				display_mode = true;
				links.emplace_back(link_now);
				choices += 1;
				link_now = "";
				line = "Up: " + line + "Down: ";
				i+=3;
				continue;
			}
			if (display_mode){
				line = line + line_in[i];
			} else {
				link_now = link_now + line_in[i];
			}
			i++;
		}
		if (!display_mode){
			links.emplace_back(link_now);
		}
		//std::cout << std::endl;
		/*std::cout << line << std::endl;
		for (int j = 0; j < links.size(); j++){
			std::cout << links[j] << std::endl;
		}*/
	}
	if (choices == 0) {
		line = line_in;
	} else {
		colorOut = glm::u8vec4(0,0,255,1);
	}
	
	// Based on Harfbuzz example at: https://github.com/harfbuzz/harfbuzz-tutorial/blob/master/hello-harfbuzz-freetype.c
	// since the below code follows the code from the example basically exactly, I'm also including some annotations
	// of my understanding of what's going on
	FT_Library ft_library;
	FT_Face ft_face;
	FT_Error ft_error;

	// Initialize Freetype basics and check for failure
	// Load freetype library into ft_library
	if ((ft_error = FT_Init_FreeType (&ft_library))){ 
		std::cout << "Error: " << FT_Error_String(ft_error) << std::endl;
		std::cout << "Init Freetype Library failed, aborting..." << std::endl;
		abort();
	}
	// Load font face through path (font_path)
	if ((ft_error = FT_New_Face (ft_library, data_path(font_path).c_str(), 0, &ft_face))){ // .c_str() converts to char *
		std::cout << "Failed while loading " << data_path(font_path).c_str() << std::endl;
		std::cout << "Error: " << FT_Error_String(ft_error) << std::endl;
		std::cout << "Init Freetype Face failed, aborting..." << std::endl;
		abort();
	}
	// Define a character size based on constant literals
	if ((ft_error = FT_Set_Char_Size (ft_face, FONT_SIZE*64, FONT_SIZE*64, 0, 0))){
		std::cout << "Error: " << FT_Error_String(ft_error) << std::endl;
		std::cout << "Setting character size failed, aborting..." << std::endl;
		abort();
	}
	
	// Initialize harfbuzz shaper using freetype font
	hb_font_t *hb_font;
	hb_font = hb_ft_font_create (ft_face, NULL); //NULL destruction function
	
	//Create and fill buffer (test text)
	hb_buffer_t *hb_buffer;
	hb_buffer = hb_buffer_create ();
	hb_buffer_add_utf8 (hb_buffer, line.c_str(), -1, 0, -1); // -1 length values (buffer, string) with 0 offset
	hb_buffer_guess_segment_properties (hb_buffer);

	hb_shape (hb_font, hb_buffer, NULL, 0); // this actually defining the gylphs into hb_buffer
	
	// extract more info about the glyphs
	unsigned int len = hb_buffer_get_length (hb_buffer);
	hb_glyph_info_t *info = hb_buffer_get_glyph_infos (hb_buffer, NULL);
	hb_glyph_position_t *pos = hb_buffer_get_glyph_positions (hb_buffer, NULL);
	
	/*printf ("Raw buffer contents:\n");
	for (unsigned int i = 0; i < len; i++)
	{
		hb_codepoint_t gid   = info[i].codepoint;
		unsigned int cluster = info[i].cluster;
		double x_advance = pos[i].x_advance / 64.;
		double y_advance = pos[i].y_advance / 64.;
		double x_offset  = pos[i].x_offset / 64.;
		double y_offset  = pos[i].y_offset / 64.;

		char glyphname[32];
		hb_font_get_glyph_name (hb_font, gid, glyphname, sizeof (glyphname));

		printf ("glyph='%s'	cluster=%d	advance=(%g,%g)	offset=(%g,%g)\n",
				glyphname, cluster, x_advance, y_advance, x_offset, y_offset);
	}*/

	// Following is derived from the Freetype example at https://freetype.org/freetype2/docs/tutorial/step1.html
	FT_GlyphSlot  slot = ft_face->glyph; // 
	int           pen_x, pen_y;
	static uint32_t h = window_height/3;

	pen_x = static_cast<int>(LEFTMARGIN);
	pen_y = static_cast<int>(TOPMARGIN) + FONT_SIZE;

	double line_height = FONT_SIZE;
	bool lastWasSpace = true;
	char glyphname[32];
	for (uint32_t n = 0; n < len; n++ ) {
		hb_codepoint_t gid   = info[n].codepoint;
		hb_font_get_glyph_name (hb_font, gid, glyphname, sizeof (glyphname));
		// unsigned int cluster = info[n].cluster;
		double x_position = pen_x + pos[n].x_offset / 64.;
		double y_position = pen_y + pos[n].y_offset / 64.;

		// Cumbersome but this lets us automatically handle new lines.
		float wordWidth = 0.0f;
		std::string word = "";
		char loop_glyphname[32] = "";
		if (lastWasSpace) {
			uint32_t m = n;
			while (strcmp(loop_glyphname,"space") != 0 && strcmp(loop_glyphname,"uni20BF") != 0 && m < len){
				hb_codepoint_t loop_gid  = info[m].codepoint;
				hb_font_get_glyph_name (hb_font, loop_gid, loop_glyphname, sizeof (loop_glyphname));
				//std::cout << loop_glyphname << std::endl;
				word = word + loop_glyphname;
				wordWidth += pos[m].x_advance / 64;
				m+=1;
			}
			// std::cout << word << std::endl;
			//std::cout << std::to_string(wordWidth) << std::endl;

			if (x_position + wordWidth >= window_width - 2*LEFTMARGIN) {
				pen_x = static_cast<int>(LEFTMARGIN); 
				pen_y += static_cast<int>(line_height + LINE_SPACING); 
				x_position = pen_x + pos[n].x_offset / 64.;
				y_position = pen_y + pos[n].y_offset / 64.;

				line_height = FONT_SIZE;
			}
		}
		
		// The above solution to lines does not allow for \n since it's based in glyphs.
		// Instead of \n,  I will be using ₿ to indicate line ends.
		// I've adjusted my text asset pipeline to account for this.
		if (strcmp(glyphname, "uni20BF") == 0) {
			pen_x = static_cast<int>(LEFTMARGIN); 
			pen_y += static_cast<int>(line_height + LINE_SPACING); 
			x_position = pen_x + pos[n].x_offset / 64.;
			y_position = pen_y + pos[n].y_offset / 64.;

			line_height = FONT_SIZE;
			continue;
		}

		
		/* load glyph image into the slot (erase previous one) */
		FT_Error error = FT_Load_Glyph(ft_face, gid, FT_LOAD_RENDER); // Glyphs instead of Chars
		if (error) continue; 

		// track max line_height for unification
		if (slot->bitmap.rows > line_height) {
			line_height = slot->bitmap.rows;
		}
		
		// std::cout << typeid(&slot->bitmap).name() << std::endl;
		// bitmap drawing function
		draw_png(&slot->bitmap, &text_render[0][0], static_cast<int>(x_position + slot->bitmap_left), static_cast<int>(y_position - slot->bitmap_top), h, window_width, colorOut);

		// Move the "pen" based on x and y advance given by glyphs
		pen_x += pos[n].x_advance / 64; 
		pen_y += pos[n].y_advance / 64; 

		if (strcmp(glyphname,"space")==0){
			lastWasSpace = true;
		} else {
			lastWasSpace = false;
		}
	}
	save_png(data_path("out.png"), glm::uvec2(window_width, h), &text_render[0][0], UpperLeftOrigin); //Upper left worked.
	

	hb_buffer_destroy (hb_buffer);
	hb_font_destroy (hb_font);

	FT_Done_Face (ft_face);
	FT_Done_FreeType (ft_library);
}

int update_texture(PlayMode::TextureItem *tex_in, std::string path_in, float x0, float x1, float y0, float y1, float z){
	// Code is from Jim via Sasha's notes, cross-referenced against Jim's copied
	// code in discord where Sasha's notes were missing, with a few changes to make this
	// work in this context
	glGenTextures(1, &tex_in->tex);
	{ // upload a texture
		// note: load_png throws on failure
		glm::uvec2 size;
		std::vector<glm::u8vec4> data;
		load_png(data_path(path_in), &size, &data, LowerLeftOrigin);
		glBindTexture(GL_TEXTURE_2D, tex_in->tex);
		// here, "data()" is the member function that gives a pointer to the first element
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());

		glGenerateMipmap(GL_TEXTURE_2D); // MIPMAP!!!!!! realizing this was missing took a while...
		// i for integer. wrap mode, minification, magnification
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		// nearest neighbor
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		// aliasing at far distances. maybe moire patterns
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	glGenBuffers(1, &tex_in->tristrip);
	
	glGenVertexArrays(1, &tex_in->tristrip_for_texture_program);
	{ //set up vertex array:
		glBindVertexArray(tex_in->tristrip_for_texture_program);
		glBindBuffer(GL_ARRAY_BUFFER, tex_in->tristrip);

		glVertexAttribPointer( texture_program->Position_vec4, 3, GL_FLOAT, GL_FALSE, sizeof(PlayMode::PosTexVertex), (GLbyte *)0 + offsetof(PlayMode::PosTexVertex, Position) );
		glEnableVertexAttribArray( texture_program->Position_vec4 );

		glVertexAttribPointer( texture_program->TexCoord_vec2, 2, GL_FLOAT, GL_FALSE, sizeof(PlayMode::PosTexVertex), (GLbyte *)0 + offsetof(PlayMode::PosTexVertex, TexCoord) );
		glEnableVertexAttribArray( texture_program->TexCoord_vec2 );

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}

	std::vector< PlayMode::PosTexVertex > verts;
	
	// Define top right quadrant to full texture
	verts.emplace_back(PlayMode::PosTexVertex{
		.Position = glm::vec3(x0, y0, z),
		.TexCoord = glm::vec2(0.0f, 0.0f), // extra comma lets you make a 1 line change later
	});
	verts.emplace_back(PlayMode::PosTexVertex{
		.Position = glm::vec3(x0, y1, z),
		.TexCoord = glm::vec2(0.0f, 1.0f),
	});
	verts.emplace_back(PlayMode::PosTexVertex{
		.Position = glm::vec3(x1, y0, z),
		.TexCoord = glm::vec2(1.0f, 0.0f),
	});
	verts.emplace_back(PlayMode::PosTexVertex{
		.Position = glm::vec3(x1, y1, z),
		.TexCoord = glm::vec2(1.0f, 1.0f),
	});
	/*for (int i = 0; i < verts.size(); i++) {
		std::cout << glm::to_string(verts[i].Position) << std::endl;
		std::cout << glm::to_string(verts[i].TexCoord) << std::endl;
	}*/
	glBindBuffer(GL_ARRAY_BUFFER, tex_in->tristrip);
	glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(verts[0]), verts.data(), GL_STREAM_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	tex_in->count = GLsizei(verts.size());
	GL_ERRORS();

	
	//identity transform (just drawing "on the screen"):
	tex_in->CLIP_FROM_LOCAL = glm::mat4(1.0f);

	
	//camera transform (drawing "in the world"):
	/*tex_in.CLIP_FROM_LOCAL = camera->make_projection() * glm::mat4(camera->transform->make_world_to_local()) * glm::mat4(
		5.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 5.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 5.0f, 0.0f,
		0.0f, 0.0f, 20.0f, 1.0f
	);*/
	
	GL_ERRORS();
	return 0;
}

void loadScript (std::string path_in){
	// following partially adopted from example in https://cplusplus.com/doc/tutorial/files/
	std::string line;
	std::ifstream thisScript (data_path("script/" + path_in));
	activeScript.clear();
	activeIndex = 0;
	if (thisScript.is_open()) {
		while ( getline (thisScript,line, '\r') ){
			activeScript.emplace_back(line);
		}
		thisScript.close();
	}
	else {
		std::cout << "not sure how to handle this lol" << std::endl;
	}
}

void advance_step (uint32_t x, uint32_t y){
	if (activeScript[activeIndex] != activeScript.back()) activeIndex += 1;
}
void reverse_step (uint32_t x, uint32_t y){
	if (activeScript[activeIndex] != activeScript.front()) activeIndex -= 1;
}

PlayMode::PlayMode() : scene(*blank_scene) {
	//get pointers to leg for convenience:
	// ONLY A CAMERA IN THIS SCENE, LOL

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	//start music loop playing:
	// (note: position will be over-ridden in update())
	leg_tip_loop = Sound::loop(*bgm_sample, 0.3f, 5.0f);

	loadScript("start.txt");

	render_text(activeScript[activeIndex], white);
	
	update_texture(&tex_example, tex_path, -1.0f, 1.0f, -1.0f, -0.33f, 0.0f);
	update_texture(&tex_textbg, textbg_path, -1.0f, 1.0f, -1.0f, -0.33f, 0.0001f);
}

PlayMode::~PlayMode() {

}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_s || evt.key.keysym.sym == SDLK_DOWN) {
			down.downs += 1;
			down.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w || evt.key.keysym.sym == SDLK_UP) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} 
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_s || evt.key.keysym.sym == SDLK_DOWN) {
			down.pressed = false;
			std::cout << std::to_string(choices) << std::endl;
			if (choices > 0){
				//std::cout << links[0] << std::endl;
				//std::cout << links[1] << std::endl;
				if (choices > 1){
					loadScript(links[1]);
					links.clear();
				}else{
					loadScript(links[0]);
					links.clear();
				}
				clear_png(&text_render[0][0], window_height/3, window_width);
				render_text(activeScript[activeIndex], white);
				update_texture(&tex_example, tex_path, -1.0f, 1.0f, -1.0f, -0.33f, 0.0f);
			}
			return true;
		} else if (evt.key.keysym.sym == SDLK_w || evt.key.keysym.sym == SDLK_UP) {
			up.pressed = false;
			if (choices > 0){
				loadScript(links[0]);
				links.clear();
				clear_png(&text_render[0][0], window_height/3, window_width);
				render_text(activeScript[activeIndex], white);
				update_texture(&tex_example, tex_path, -1.0f, 1.0f, -1.0f, -0.33f, 0.0f);
			}
			return true;
		} 
	} else if (evt.type == SDL_MOUSEBUTTONDOWN) {
		/*if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			return true;
		}*/
		// mouse.downs += 1;
		clear_png(&text_render[0][0], window_height/3, window_width);
		if (evt.button.button == SDL_BUTTON_RIGHT){
			reverse_step(evt.button.x , evt.button.y);
		} else {
			advance_step(evt.button.x , evt.button.y);
		}
		render_text(activeScript[activeIndex], white);
		update_texture(&tex_example, tex_path, -1.0f, 1.0f, -1.0f, -0.33f, 0.0f);
	} /*else if (evt.type == SDL_MOUSEMOTION) {
		if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
			glm::vec2 motion = glm::vec2(
				evt.motion.xrel / float(window_size.y),
				-evt.motion.yrel / float(window_size.y)
			);
			camera->transform->rotation = glm::normalize(
				camera->transform->rotation
				* glm::angleAxis(-motion.x * camera->fovy, glm::vec3(0.0f, 1.0f, 0.0f))
				* glm::angleAxis(motion.y * camera->fovy, glm::vec3(1.0f, 0.0f, 0.0f))
			);
			return true;
		}
	}*/

	return false;
}

void PlayMode::update(float elapsed) {

	//slowly rotates through [0,1):
	/*wobble += elapsed / 10.0f;
	wobble -= std::floor(wobble);

	hip->rotation = hip_base_rotation * glm::angleAxis(
		glm::radians(5.0f * std::sin(wobble * 2.0f * float(M_PI))),
		glm::vec3(0.0f, 1.0f, 0.0f)
	);
	upper_leg->rotation = upper_leg_base_rotation * glm::angleAxis(
		glm::radians(7.0f * std::sin(wobble * 2.0f * 2.0f * float(M_PI))),
		glm::vec3(0.0f, 0.0f, 1.0f)
	);
	lower_leg->rotation = lower_leg_base_rotation * glm::angleAxis(
		glm::radians(10.0f * std::sin(wobble * 3.0f * 2.0f * float(M_PI))),
		glm::vec3(0.0f, 0.0f, 1.0f)
	);

	//move sound to follow leg tip position:
	leg_tip_loop->set_position(get_leg_tip_position(), 1.0f / 60.0f);*/

	//move camera:
	{

		//combine inputs into a move:
		constexpr float PlayerSpeed = 30.0f;
		glm::vec2 move = glm::vec2(0.0f);
		if (left.pressed && !right.pressed) move.x =-1.0f;
		if (!left.pressed && right.pressed) move.x = 1.0f;
		if (down.pressed && !up.pressed) move.y =-1.0f;
		if (!down.pressed && up.pressed) move.y = 1.0f;

		//make it so that moving diagonally doesn't go faster:
		if (move != glm::vec2(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;

		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 frame_right = frame[0];
		//glm::vec3 up = frame[1];
		glm::vec3 frame_forward = -frame[2];

		camera->transform->position += move.x * frame_right + move.y * frame_forward;
	}

	{ //update listener to camera position:
		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 frame_right = frame[0];
		glm::vec3 frame_at = frame[3];
		Sound::listener.set_position_right(frame_at, frame_right, 1.0f / 60.0f);
	}

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;

}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*camera);

	//Code taken from Jim's copied code + Sashas
	{ //texture example drawing
	
		glEnable(GL_BLEND);
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glUseProgram(texture_program->program);
		glActiveTexture(GL_TEXTURE0);
		//std::cout << "Texture ID: " << tex_example.tex << std::endl;
		glBindVertexArray(tex_textbg.tristrip_for_texture_program);
		glBindTexture(GL_TEXTURE_2D, tex_textbg.tex);
		glUniformMatrix4fv( texture_program->CLIP_FROM_LOCAL_mat4, 1, GL_FALSE, glm::value_ptr(tex_textbg.CLIP_FROM_LOCAL) );
		glDrawArrays(GL_TRIANGLE_STRIP, 0, tex_textbg.count);

		glBindVertexArray(tex_example.tristrip_for_texture_program);
		glBindTexture(GL_TEXTURE_2D, tex_example.tex);
		glUniformMatrix4fv( texture_program->CLIP_FROM_LOCAL_mat4, 1, GL_FALSE, glm::value_ptr(tex_example.CLIP_FROM_LOCAL) );
		glDrawArrays(GL_TRIANGLE_STRIP, 0, tex_example.count);
		
		/*glBindVertexArray(tex_bg.tristrip_for_texture_program);
		glBindTexture(GL_TEXTURE_2D, tex_bg.tex);
		glUniformMatrix4fv( texture_program->CLIP_FROM_LOCAL_mat4, 1, GL_FALSE, glm::value_ptr(tex_bg.CLIP_FROM_LOCAL) );
		glDrawArrays(GL_TRIANGLE_STRIP, 0, tex_bg.count);*/



		// std::cout << std::to_string(size_t(tex_example.count)) << std::endl;

		glBindTexture(GL_TEXTURE_2D, 0);
		glBindVertexArray(0);
		glUseProgram(0);
		glDisable(GL_BLEND);
	}
	GL_ERRORS();
	/*
	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.09f;
		lines.draw_text("Mouse motion rotates camera; WASD moves; escape ungrabs mouse",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("Mouse motion rotates camera; WASD moves; escape ungrabs mouse",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}
	GL_ERRORS();*/
	
}
/*
glm::vec3 PlayMode::get_leg_tip_position() {
	//the vertex position here was read from the model in blender:
	return lower_leg->make_local_to_world() * glm::vec4(-1.26137f, -11.861f, 0.0f, 1.0f);
}*/
