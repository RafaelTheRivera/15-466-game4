#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----
	
	// Following is largely copied from Jim's shared code + Sasha's nodes.
	// Some minor things have been altered for taste reasons.

	struct PosTexVertex {
		glm::vec3 Position;
		glm::vec2 TexCoord;
	};
	static_assert( sizeof(PosTexVertex) == 3*4 + 2*4, "PosTexVertex is size 20");
	struct TextureItem{
	// handles
		GLuint tex = 0;
		GLuint tristrip = 0; // buffer for tristrip
		GLuint tristrip_for_texture_program = 0; // vao

		GLsizei count = 0; 
		glm::mat4 CLIP_FROM_LOCAL = glm::mat4(1.0f);
	} tex_example, tex_bg, tex_textbg;


	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up, mouse;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;
	
	glm::u8vec4 white = glm::u8vec4(255,255,255,1);
	glm::u8vec4 red = glm::u8vec4(255,0,0,1);
	glm::u8vec4 blue = glm::u8vec4(0,255,0,1);
	glm::u8vec4 green = glm::u8vec4(0,0,255,1);

	//hexapod leg to wobble:

	//void render_text(std::string line_in);
	//void update_texture(TextureItem *tex_in, std::string path);

	std::vector<TextureItem> allTextures;

	//music coming from the tip of the leg (as a demonstration):
	std::shared_ptr< Sound::PlayingSample > leg_tip_loop;
	
	//camera:
	Scene::Camera *camera = nullptr;

};
