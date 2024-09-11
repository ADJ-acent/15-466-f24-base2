#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>

GLuint main_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > main_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("main.pnct"));
	main_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > main_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("main.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = main_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = main_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

// set up pseudo random number generator:
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<int>index_dist(0, 11);
std::uniform_real_distribution<float>pos_dist(0.0f, 1.0f);

PlayMode::PlayMode() : scene(*main_scene) {
	// get pointers to hamster
	for (auto &transform : scene.transforms) {
		if (transform.name == "Hamster") hamster = &transform;
		else if (transform.name == "StartPointsRight") {
			spawn_range = transform.position;
			y_range.x = transform.position.y;
		}
		else if (transform.name == "StartPointsLeft") y_range.y = transform.position.y;
		else if (transform.name == "EndPoint") {
			despawn_range = transform.position;
		}
		else if (transform.name.substr(0,5) == "TreeT") {
			obstacles[obstacles_count] = {&transform, 2.0f,1.5f, false};

			obstacles_count++;
		}
		else if (transform.name.substr(0,4) == "Rock") {
			obstacles[obstacles_count] = {&transform, 2.5f, 7.0f, false};
			obstacles_count++;
		}
	}
	if (hamster == nullptr) throw std::runtime_error("Hamster not found.");
	if (y_range == glm::vec2()) throw std::runtime_error("Y Range not found");
	if (spawn_range == glm::vec3(0)) throw std::runtime_error("Spawn Range not found");
	if (despawn_range == glm::vec3(0)) throw std::runtime_error("Despawn Range not found.");
	if (obstacles_count != 12) throw std::runtime_error("More or less than 12 obstacles found.");

	despawn_range.y = y_range.x;
	hamster_base_position = hamster->position;
	// vector for obstacles to go along
	up_vector = glm::normalize(despawn_range - spawn_range);
	assert(up_vector.y == 0);
	
	// set all obstacle location to the despawn area:
	for (uint8_t i = 0; i < uint8_t(obstacles.size()); ++i) {
		obstacles[i].transform->position = despawn_range;
	}

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();
	camera_origin = camera->transform->position;

}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		} else if (evt.key.keysym.sym == SDLK_a) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.downs += 1;
			down.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_r) {
			r.downs += 1;
			r.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_SPACE) {
			space.downs += 1;
			space.pressed = true;
			return true;
		}
		
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_a) {
			left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_r) {
			r.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_SPACE) {
			space.pressed = false;
			return true;
		} 
	}

	return false;
}

void PlayMode::update(float elapsed) {
	if (r.pressed) reset();
	if (game_end) return;

	//rotates through [0,1):
	rotate_interval += elapsed * rotate_speed;
	if (rotate_interval > 1.0f) {
		score ++;
		rotate_interval -= std::floor(rotate_interval);
	}
	rotate_speed += elapsed * 0.2f;

	{// move obstacles closer to hamster and spawn new obstacles
		since_last_obstacle_spawn += elapsed;
		uint8_t unused = 255;
		for (uint8_t i = 0; i < uint8_t(obstacles.size()); ++i) {
			if (obstacles[i].active) {
				glm::vec3 old_pos = obstacles[i].transform->position;
				glm::vec3 direction = glm::vec3(up_vector * rotate_speed * hamster_radius * 0.5f);
				obstacles[i].transform->position += direction;

				if (check_intersection(old_pos - obstacles[i].z_offset/2.0f, direction, hamster->position, hamster_radius + obstacles[i].radius)){
					game_end = true;
				}
				if (obstacles[i].transform->position.x <= despawn_range.x) {
					obstacles[i].active = false;
					in_use_count --;
				}
			}
			else {
				unused = i;
			}
		}

		if (since_last_obstacle_spawn > spawn_rate && in_use_count < 11) {
			spawn_rate = std::max(0.2f, spawn_rate - 0.05f);
			since_last_obstacle_spawn -= spawn_rate;
			in_use_count++;
			uint8_t i = uint8_t(index_dist(gen));
			if (obstacles[i].active) {
				assert(unused != 255);
				assert(!obstacles[unused].active);
				i = unused;
			}

			obstacles[i].active = true;
			float rand_float = pos_dist(gen);
			obstacles[i].transform->position = glm::vec3{
				spawn_range.x, 
				rand_float * y_range.x + (1.0f - rand_float) * y_range.y, 
				spawn_range.z + obstacles[i].z_offset
			};
			obstacles[i].transform->rotation = glm::angleAxis(
				glm::radians(360.0f * pos_dist(gen)),
				glm::vec3(0.0f, 0.0f, 1.0f)
			);
			
		}
	}

	{// rotate and move hamster on the y axis
		hamster->rotation = glm::angleAxis(
			glm::radians(0.0f),
			glm::vec3(0.0f, 1.0f, 0.0f)
		);

		float movement_direction = float(left.pressed) - float(right.pressed);
		if (movement_direction != 0) {
			hamster->rotation = glm::rotate(hamster->rotation, glm::radians(movement_direction * 10.0f), glm::vec3(0.0f, 0.0f, 1.0f));
			float old_y = hamster->position.y;
			hamster->position.y += elapsed * movement_direction * rotate_speed * 15.0f;
			hamster->position.y = std::clamp(hamster->position.y, y_range.x + 10.0f, y_range.y - 10.0f);

			camera->transform->position.y += hamster->position.y - old_y;
		}

		hamster->rotation *= glm::angleAxis(
			glm::radians(360.0f * rotate_interval),
			glm::vec3(0.0f, 1.0f, 0.0f)
		);
	}

	if (space.pressed && !in_jump) {
		in_jump = true;
	}
	if (in_jump) {
		if (since_jumped >= 1.0f) {
			since_jumped = 0.0f;
			in_jump = false;
			hamster->position.z = hamster_base_position.z;
		} else {
			hamster->position.z = hamster_base_position.z + float(std::sin(M_PI * 1.0f * since_jumped) * 10.0f);
			since_jumped += elapsed;
		}
	}

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
	r.downs = 0;
	space.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);
	const glm::vec3 fog_start_color = glm::vec3(0.6f, 0.8f, 1.0f);
	const glm::vec3 fog_end_color = glm::vec3(0.8f, .5f, .5f);
	// blend background color closer to death screen
	glm::vec3 FOG_COLOR = glm::mix(fog_start_color, fog_end_color, std::clamp(rotate_speed-1.0f, 0.0f, 10.0f) / 10.0f);
	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUniform3fv(lit_color_texture_program->FOG_COLOR, 1, glm::value_ptr(FOG_COLOR));
	glUseProgram(0);

	{//let player know they are dead
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));
		constexpr float H = 0.09f;
		if (game_end) {
			glClearColor(0.8f, .5f, .5f, 1.0f);
			float ofs = 6.0f / drawable_size.y;
			lines.draw_text("DEAD HAMSTER",
				glm::vec3(-float(drawable_size.x)*H / 200.0f, 0.35f, 0.0),
				glm::vec3(H*3, 0.0f, 0.0f), glm::vec3(0.0f, H*3, 0.0f),
				glm::u8vec4(0x00, 0x00, 0x00, 0x00));
			lines.draw_text("DEAD HAMSTER",
				glm::vec3(-float(drawable_size.x)*H / 200.0f + ofs, ofs +0.35f, 0.0),
				glm::vec3(H*3, 0.0f, 0.0f), glm::vec3(0.0f, H*3, 0.0f),
				glm::u8vec4(0xff, 0x00, 0x00, 0x00));
			lines.draw_text("Press 'r' to restart",
				glm::vec3(-float(drawable_size.x)*H / 225.0f, -0.5f, 0.0),
				glm::vec3(H*2.0f, 0.0f, 0.0f), glm::vec3(0.0f, H*2.0f, 0.0f),
				glm::u8vec4(0x00, 0x00, 0x00, 0x00));
			lines.draw_text("Press 'r' to restart",
				glm::vec3(-float(drawable_size.x)*H  / 225.0f+ofs, -.5f, 0.0),
				glm::vec3(H*2, 0.0f, 0.0f), glm::vec3(0.0f, H*2.0f, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));
			lines.draw_text("Score: " + std::to_string(score),
				glm::vec3(-float(drawable_size.x)*H / 350.0f, -.25f, 0.0),
				glm::vec3(H*2, 0.0f, 0.0f), glm::vec3(0.0f, H*2, 0.0f),
				glm::u8vec4(0x00, 0x00, 0x00, 0x00));
			lines.draw_text("Score: " + std::to_string(score),
				glm::vec3(-float(drawable_size.x)*H / 350.0f + ofs, -0.25f + ofs, 0.0),
				glm::vec3(H*2, 0.0f, 0.0f), glm::vec3(0.0f, H*2, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));
			glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			GL_ERRORS(); //print any errors produced by this setup code
			return;
		}
	}

	glClearColor(FOG_COLOR.x, FOG_COLOR.y, FOG_COLOR.z, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	GL_ERRORS(); //print any errors produced by this setup code

	scene.draw(*camera);

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
		lines.draw_text("WASD to move, Space to jump",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		if (!game_end) {
			lines.draw_text("WASD to move, Space to jump",
				glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + 0.1f * H + ofs, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));
			lines.draw_text("Score: " + std::to_string(score),
				glm::vec3(-aspect + 0.1f * H, 1.0 - H, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0x00, 0x00, 0x00, 0x00));
			lines.draw_text("Score: " + std::to_string(score),
				glm::vec3(-aspect + 0.1f * H + ofs, 1.0 - H + ofs, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		}
	}
	
}

bool PlayMode::check_intersection(glm::vec3 p0, glm::vec3 direction, glm::vec3 center, float radius)
{
    glm::vec3 origin_from_center = p0 - center;

    float A = glm::dot(direction, direction);
    float B = 2.0f * glm::dot(origin_from_center, direction);
    float C_value = glm::dot(origin_from_center, origin_from_center) - radius * radius;

    float discriminant = B * B - 4.0f * A * C_value;

    if (discriminant < 0) {
        return false; // No intersection
    } else {
        float sqrt_discriminant = std::sqrt(discriminant);

        float t1 = (-B - sqrt_discriminant) / (2.0f * A);
        float t2 = (-B + sqrt_discriminant) / (2.0f * A);

        // Check if either t1 or t2 is within the interval [0, 1]
        if ((t1 >= 0.0f && t1 <= 1.0f) || (t2 >= 0.0f && t2 <= 1.0f)) {
            return true; // Intersection occurs
        }
    }
    return false; // No intersection within the segment
}

void PlayMode::reset()
{
	game_end = false;

	//reset hamster position and rotation speed
	hamster->position = hamster_base_position;
	rotate_speed = 0;
	rotate_interval = 0;

	//reset obstacles
	since_last_obstacle_spawn = 0.0f;
	spawn_rate = 1.0f;
	in_use_count = 0;
	for (uint8_t i = 0; i < uint8_t(obstacles.size()); ++i) {
		obstacles[i].active = false;
		obstacles[i].transform->position = despawn_range;
	}
	//reset camera
	camera->transform->position = camera_origin;

	//reset score
	score = 0;

	//reset jump
	in_jump = false;
	since_jumped = 0.0f;
}
