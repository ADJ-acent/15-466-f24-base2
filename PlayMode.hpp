#include "Mode.hpp"

#include "Scene.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>
#include <array>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	bool check_intersection(glm::vec3 p0, glm::vec3 direction, glm::vec3 center, float radius);

	void reset();

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up, r, space;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	//game ended?
	bool game_end = false;

	//hamster:
	glm::vec3 hamster_base_position = glm::vec3();
	Scene::Transform *hamster = nullptr;
	const float hamster_radius = 5.0f;
	float rotate_speed = 1.0f;
	const float max_rotate_speed = 5.0f;
	float rotate_interval = 0;
	float since_jumped = 0.0f;
	bool in_jump = false;

	//obstacles
	struct Obstacle {
		Scene::Transform *transform;
		float z_offset = 0.0f;
		float radius = 0.0f;
		bool is_tree = false;
		bool active = false;
	};

	glm::vec2 y_range;
	glm::vec3 spawn_range = glm::vec3(0);
	glm::vec3 despawn_range = glm::vec3(0);
	glm::vec3 up_vector; // direction for obstacle to travel towards
	std::array<Obstacle, 12> obstacles;
	uint8_t obstacles_count = 0;
	float since_last_obstacle_spawn = 0.0f;
	float spawn_rate = 1.0f;
	uint8_t in_use_count = 0;
	uint32_t score = 0;
	
	//camera:
	Scene::Camera *camera = nullptr;
	glm::vec3 camera_origin;

};
