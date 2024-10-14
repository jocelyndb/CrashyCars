// Header
#include "world_system.hpp"
#include "world_init.hpp"

// stlib
#include <cassert>
#include <iostream>
#include <sstream>
#include <xpc/xpc.h>

#include "physics_system.hpp"
#include "state_system.h"

// Game configuration
const size_t MAX_NUM_WALLS = 7;
const size_t MAX_NUM_BONUS = 5;
const size_t BARRIER_SPAWN_DELAY_MS = 500 * 3;
const size_t BONUS_SPAWN_DELAY_MS = 750 * 3;
size_t CURRENT_BONUS_SPAWN_DELAY_MS = BONUS_SPAWN_DELAY_MS;
size_t CURRENT_BARRIER_SPAWN_DELAY_MS = BARRIER_SPAWN_DELAY_MS;
const size_t WALL_GAP = 350.f;
// const size_t SPEED_FACTOR = 1.05f;

// Car speed
static float CAR_SPEED = 400.f;

std::unordered_map<int, bool> WorldSystem::key_map;
vec2 WorldSystem::mouse_pos;
float WorldSystem::target_angle = M_PI;

std::unordered_map<int, bool> WorldSystem::button_map;

// create the world
WorldSystem::WorldSystem()
	: next_barrier_spawn(0.f)
	, next_bonus_spawn(0.f) {
	// Seeding rng with random device
	rng = std::default_random_engine(std::random_device()());
}

WorldSystem::~WorldSystem() {
	
	// destroy music components
	if (background_music != nullptr)
		Mix_FreeMusic(background_music);
	if (car_crash_sound != nullptr)
		Mix_FreeChunk(car_crash_sound);
	if (point_scored_sound != nullptr)
		Mix_FreeChunk(point_scored_sound);

	Mix_CloseAudio();

	// Destroy all created components
	registry.clear_all_components();

	// Close the window
	glfwDestroyWindow(window);
}

// Debugging
namespace {
	void glfw_err_cb(int error, const char *desc) {
		fprintf(stderr, "%d: %s", error, desc);
	}
}

// World initialization
// Note, this has a lot of OpenGL specific things, could be moved to the renderer
GLFWwindow* WorldSystem::create_window() {
	///////////////////////////////////////
	// Initialize GLFW
	glfwSetErrorCallback(glfw_err_cb);
	if (!glfwInit()) {
		fprintf(stderr, "Failed to initialize GLFW");
		return nullptr;
	}

	//-------------------------------------------------------------------------
	// If you are on Linux or Windows, you can change these 2 numbers to 4 and 3 and
	// enable the glDebugMessageCallback to have OpenGL catch your mistakes for you.
	// GLFW / OGL Initialization
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#if __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
	glfwWindowHint(GLFW_RESIZABLE, 0);

	// Create the main window (for rendering, keyboard, and mouse input)
	window = glfwCreateWindow(window_width_px, window_height_px, "Drifty Cars", nullptr, nullptr);
	if (window == nullptr) {
		fprintf(stderr, "Failed to glfwCreateWindow");
		return nullptr;
	}

	// Setting callbacks to member functions (that's why the redirect is needed)
	// Input is handled using GLFW, for more info see
	// http://www.glfw.org/docs/latest/input_guide.html
	glfwSetWindowUserPointer(window, this);
	auto key_redirect = [](GLFWwindow* wnd, int _0, int _1, int _2, int _3) { ((WorldSystem*)glfwGetWindowUserPointer(wnd))->on_key(_0, _1, _2, _3); };
	auto cursor_pos_redirect = [](GLFWwindow* wnd, double _0, double _1) { ((WorldSystem*)glfwGetWindowUserPointer(wnd))->on_mouse_move({ _0, _1 }); };
	auto mouse_button_redirect = [](GLFWwindow* wnd, int button, int action, int mods) { ((WorldSystem*)glfwGetMouseButton(wnd, button))->on_mouse_click(button, action, mods); };
	glfwSetKeyCallback(window, key_redirect);
	glfwSetCursorPosCallback(window, cursor_pos_redirect);
	glfwSetMouseButtonCallback(window, mouse_button_redirect);

	//////////////////////////////////////
	// Loading music and sounds with SDL
	if (SDL_Init(SDL_INIT_AUDIO) < 0) {
		fprintf(stderr, "Failed to initialize SDL Audio");
		return nullptr;
	}
	if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) == -1) {
		fprintf(stderr, "Failed to open audio device");
		return nullptr;
	}

	background_music = Mix_LoadMUS(audio_path("background_chiptune.wav").c_str());
	car_crash_sound = Mix_LoadWAV(audio_path("car_crash.mp3").c_str());
	point_scored_sound = Mix_LoadWAV(audio_path("honk.wav").c_str());

	if (background_music == nullptr || car_crash_sound == nullptr || point_scored_sound == nullptr) {
		fprintf(stderr, "Failed to load sounds\n %s\n %s\n %s\n make sure the data directory is present",
			audio_path("background_chiptune.wav").c_str(),
			audio_path("car_crash.wav").c_str(),
			audio_path("honk.wav").c_str());
		return nullptr;
	}

	return window;
}

void WorldSystem::init(RenderSystem* renderer_arg) {
	this->renderer = renderer_arg;
	// Playing background music indefinitely
	Mix_PlayMusic(background_music, -1);
	fprintf(stderr, "Loaded music\n");
	// Set all states to default
    restart_game();
}

vec2 WorldSystem::get_mouse_position() {
	return mouse_pos;
}

void WorldSystem::on_mouse_move(vec2 mouse_position) {
	mouse_pos = mouse_position;
}

// Set a mouse button as clicked
void WorldSystem::click_button(int button) {
	button_map[button] = true;
}

// Set a mouse button as released
void WorldSystem::release_button(int button) {
	button_map[button] = false;
}

// Is mouse button clicked?
bool WorldSystem::button_clicked(int button) {
	return WorldSystem::button_map[button];
}

void WorldSystem::on_mouse_click(int button, int action, int mods) {
	if (action == GLFW_PRESS) {
		click_button(button);
	} else if (action == GLFW_RELEASE) {
		release_button(button);
	}
}

// Update our game world
bool WorldSystem::step(float elapsed_ms_since_last_update) {
	// Updating window title with points
	std::stringstream title_ss;
	title_ss << "Points: " << StateSystem::get_points();
	glfwSetWindowTitle(window, title_ss.str().c_str());

	// Remove debug info from the last step
	while (registry.debugComponents.entities.size() > 0)
	    registry.remove_all_components_of(registry.debugComponents.entities.back());

	// Removing out of screen entities
	auto& motions_registry = registry.motions;

	// Remove entities that leave the screen on the left side
	// Iterate backwards to be able to remove without unterfering with the next object to visit
	// (the containers exchange the last element with the current)
	for (int i = (int)motions_registry.components.size()-1; i>=0; --i) {
	    Motion& motion = motions_registry.components[i];
		if (motion.position.x + abs(motion.scale.x) < 0.f) {
			if(!registry.players.has(motions_registry.entities[i])) // don't remove the player
				registry.remove_all_components_of(motions_registry.entities[i]);
		}
	}

	// spawn new walls
	next_barrier_spawn -= elapsed_ms_since_last_update * current_speed;
	if (registry.deadlys.components.size() <= MAX_NUM_WALLS && next_barrier_spawn < 0.f) {
		next_barrier_spawn = CURRENT_BARRIER_SPAWN_DELAY_MS * (1 + (uniform_dist(rng) - 0.5f)/2.f);
		// next_barrier_spawn = CURRENT_BARRIER_SPAWN_DELAY_MS;
		float gap_loc = uniform_dist(rng) * window_height_px * .5f;

		// create Wall with random gap position
        createBarrier(renderer, vec2(window_width_px + 200.f, gap_loc - BARRIER_HEIGHT / 2.f));
        createBarrier(renderer, vec2(window_width_px + 200.f,gap_loc + WALL_GAP + BARRIER_HEIGHT / 2.f));
	}

	// spawn bonus
	next_bonus_spawn -= elapsed_ms_since_last_update * current_speed;
	// std::cout << next_bonus_spawn << std::endl;
	if (registry.eatables.components.size() <= MAX_NUM_BONUS && next_bonus_spawn < 0.f) {
		// reset timer
		next_bonus_spawn = CURRENT_BONUS_SPAWN_DELAY_MS / 2 + uniform_dist(rng) * (CURRENT_BONUS_SPAWN_DELAY_MS / 2);
		// next_bonus_spawn = CURRENT_BONUS_SPAWN_DELAY_MS;

		// create Bonus with random initial position
		createBonus(renderer, vec2(window_width_px + 200.f, uniform_dist(rng) * (window_height_px)), uniform_dist(rng) * 2 * M_PI);
	}

	// Processing the car state
	assert(registry.screenStates.components.size() <= 1);
    ScreenState &screen = registry.screenStates.components[0];

    float min_counter_ms = 3000.f;
	if (registry.deathTimers.entities.size() > 0) {Mix_PauseMusic();}
	for (Entity entity : registry.deathTimers.entities) {
		// progress timer
		DeathTimer& counter = registry.deathTimers.get(entity);
		counter.counter_ms -= elapsed_ms_since_last_update;
		if(counter.counter_ms < min_counter_ms){
		    min_counter_ms = counter.counter_ms;
		}

		// restart the game once the death timer expired
		if (counter.counter_ms < 0) {
			registry.deathTimers.remove(entity);
			screen.darken_screen_factor = 0;
            restart_game();
			return true;
		}
	}

	if (StateSystem::is_advanced()) advanced_car_handling(elapsed_ms_since_last_update);
	else basic_car_handling(elapsed_ms_since_last_update);

	// reduce window brightness if the car is dying
	screen.darken_screen_factor = 1 - min_counter_ms / 3000;

	// Manage temporarily lit objects
	for (Entity entity : registry.lit.entities) {
		// progress timer
		LightUp& counter = registry.lit.get(entity);
		counter.counter_ms -= elapsed_ms_since_last_update;
		if(counter.counter_ms < min_counter_ms){
			min_counter_ms = counter.counter_ms;
		}

		// remove light when counter drops below 0
		if (counter.counter_ms < 0) {
			registry.lit.remove(entity);
			return true;
		}
	}
	return true;
}

void WorldSystem::basic_car_handling(float elapsed_ms_since_last_update) {
	if (!registry.deathTimers.has(player_car)) {
		Motion& player_motion = registry.motions.get(player_car);

		float cos_angle = cosf(player_motion.angle);
		float sin_angle = sinf(player_motion.angle);

		// left key pressed
		if (key_pressed(GLFW_KEY_LEFT)) {
			player_motion.velocity.x = cos_angle * CAR_SPEED;
			player_motion.velocity.y = sin_angle * CAR_SPEED;
		}

		// right key pressed
		if (key_pressed(GLFW_KEY_RIGHT)) {
			player_motion.velocity.x = -cos_angle * CAR_SPEED;
			player_motion.velocity.y = -sin_angle * CAR_SPEED;
		}

		// up key pressed
		if (key_pressed(GLFW_KEY_UP)) {
			player_motion.velocity.x = -sin_angle * CAR_SPEED;
			player_motion.velocity.y = cos_angle * CAR_SPEED;
		}

		// down key pressed
		if (key_pressed(GLFW_KEY_DOWN)) {
			player_motion.velocity.x = sin_angle * CAR_SPEED;
			player_motion.velocity.y = -cos_angle * CAR_SPEED;
		}

		// stop if no keys pressed
		if (!key_pressed(GLFW_KEY_RIGHT) &&
			!key_pressed(GLFW_KEY_LEFT) &&
			!key_pressed(GLFW_KEY_DOWN) &&
			!key_pressed(GLFW_KEY_UP)) {
			player_motion.velocity = vec2(0.f, 0.f);
		}

		// Basic rotation
		vec2 mouse = get_mouse_position();
		float angle_mouse = atan2f(player_motion.position.y - mouse.y,
									player_motion.position.x - mouse.x);
		player_motion.angle = angle_mouse;
		// std::cout << "Current car position: (" << player_motion.position.x << ", " << player_motion.position.y << ")" << std::endl;
	}
}

void WorldSystem::advanced_car_handling(float elapsed_ms_since_last_update) {
	if (!registry.deathTimers.has(player_car)) {
		Motion& player_motion = registry.motions.get(player_car);
		float cos_angle = cosf(player_motion.angle);
		float sin_angle = sinf(player_motion.angle);
		float& x_velocity = player_motion.velocity.x;
		float& y_velocity = player_motion.velocity.y;

		float inline_component = -x_velocity * cos_angle -y_velocity * sin_angle;
		float normal_component = -x_velocity * sin_angle -y_velocity * cos_angle;
		// left key pressed
		// if (key_pressed(GLFW_KEY_LEFT)) {
		// 	if (inline_component > -CAR_SPEED) {
		// 		player_motion.velocity.x += cos_angle * elapsed_ms_since_last_update / 1.f;
		// 		player_motion.velocity.y += sin_angle * elapsed_ms_since_last_update / 1.f;
		// 	}
		// }

		// right key pressed
		// if (key_pressed(GLFW_KEY_RIGHT) || button_clicked(GLFW_MOUSE_BUTTON_LEFT)) {
		if (button_clicked(GLFW_MOUSE_BUTTON_LEFT)) {
			if (inline_component < CAR_SPEED) {
				player_motion.velocity.x -= cos_angle * elapsed_ms_since_last_update / 1.f;
				player_motion.velocity.y -= sin_angle * elapsed_ms_since_last_update / 1.f;
			}
		}

		// // up key pressed
		// if (key_pressed(GLFW_KEY_UP)) {
		// 	if (normal_component > -CAR_SPEED && abs(inline_component) < CAR_SPEED) {
		// 		player_motion.velocity.x -= sin_angle * elapsed_ms_since_last_update / 1.f;
		// 		player_motion.velocity.y += cos_angle * elapsed_ms_since_last_update / 1.f;
		// 	}
		// }

		// // down key pressed
		// if (key_pressed(GLFW_KEY_DOWN)) {
		// 	if (normal_component < CAR_SPEED) {
		// 		player_motion.velocity.x += sin_angle * elapsed_ms_since_last_update / 1.f;
		// 		player_motion.velocity.y -= cos_angle * elapsed_ms_since_last_update / 1.f;
		// 	}
		// }
		float player_speed = sqrt(player_motion.velocity.x * player_motion.velocity.x +
							 player_motion.velocity.y * player_motion.velocity.y);
		if (player_speed > CAR_SPEED) {
			player_motion.velocity *= (CAR_SPEED / player_speed);
		}

		// std::cout << "Current car speed: " << inline_component << std::endl;
		// std::cout << "Current car Normal: " << normal_component << std::endl;

		// Smoothed rotation
		vec2 mouse = get_mouse_position();
		float angle_mouse = atan2f(player_motion.position.y - mouse.y,
									player_motion.position.x - mouse.x);
		if (abs(player_motion.angle - angle_mouse) > 0.01f) {
			target_angle = angle_mouse;
		}
		if (player_motion.angle != target_angle) {
			if (player_motion.angle > M_PI * 0.5 && target_angle < M_PI * -0.5) {
				player_motion.angle += ((target_angle + 2 * M_PI) - player_motion.angle) * elapsed_ms_since_last_update / 100.f;
			} else if (player_motion.angle < M_PI * -0.5 && target_angle > M_PI * 0.5) {
				player_motion.angle += ((target_angle - 2 * M_PI) - player_motion.angle) * elapsed_ms_since_last_update / 100.f;
			} else {
				player_motion.angle += (target_angle - player_motion.angle) * elapsed_ms_since_last_update / 100.f;
			}
		}
		while (player_motion.angle > M_PI) {
			player_motion.angle -= 2 * M_PI;
		}
		while (player_motion.angle < -M_PI) {
			player_motion.angle += 2 * M_PI;
		}
		// std::cout << "Current car angle:" << player_motion.angle << std::endl;
	}
}

// Reset the world state to its initial state
void WorldSystem::restart_game() {
	// Debugging for memory/component leaks
	registry.list_all_components();
	printf("Restarting\n");

	// Reset the game speed
	current_speed = 1.f;

	// Reset point total
	StateSystem::reset_points();

	// Switch to Advanced Mode
	StateSystem::set_advanced();

	// Resume music
	Mix_ResumeMusic();

	// Remove all entities that we created
	while (registry.motions.entities.size() > 0)
	    registry.remove_all_components_of(registry.motions.entities.back());

	// Debugging for memory/component leaks
	registry.list_all_components();

	// display title
	title = createTitle(renderer, {window_width_px/2, window_height_px/3});

	// create a new Car
	player_car = createCar(renderer, { window_width_px/4, 2 * window_height_px / 3 });
	registry.colors.insert(player_car, {1, 0.8f, 0.8f});


	// Create eggs on the floor, use this for reference
	/*
	for (uint i = 0; i < 20; i++) {
		int w, h;
		glfwGetWindowSize(window, &w, &h);
		float radius = 30 * (uniform_dist(rng) + 0.3f); // range 0.3 .. 1.3
		Entity egg = createEgg({ uniform_dist(rng) * w, h - uniform_dist(rng) * 20 },
			         { radius, radius });
		float brightness = uniform_dist(rng) * 0.5 + 0.5;
		registry.colors.insert(egg, { brightness, brightness, brightness});
	}
	*/
}

// Compute collisions between entities
void WorldSystem::handle_collisions() {
	// Loop over all collisions detected by the physics system
	auto& collisionsRegistry = registry.collisions;
	for (uint i = 0; i < collisionsRegistry.components.size(); i++) {
		// The entity and its collider
		Entity entity = collisionsRegistry.entities[i];
		Entity entity_other = collisionsRegistry.components[i].other;

		// for now, we are only interested in collisions that involve the car
		if (registry.players.has(entity)) {
			//Player& player = registry.players.get(entity);

			// Checking Player - Deadly collisions
			if (registry.deadlys.has(entity_other)) {
				// initiate death unless already dying
				if (!registry.deathTimers.has(entity)) {
					// Crash sound, reset timer, and make the car bounce
					registry.deathTimers.emplace(entity);
					Mix_PlayChannel(-1, car_crash_sound, 0);
					Motion& player_motion = registry.motions.get(player_car);
					// TODO: Update collision system to get angle between colliders
					// player_motion.velocity = vec2(-player_motion.velocity.x + 300 * sin(-player_motion.angle), -player_motion.velocity.y - 300 * cos(-player_motion.angle));
					// player_motion.velocity = vec2( current_speed * BARRIER_SPEED - player_motion.velocity.x * .1f, 0.f - player_motion.velocity.y * .1f);
					// test new reflection system!
					Motion& light_motion = player_motion;
					Motion& reflective_surface_motion = registry.motions.get(entity_other);
					vec2 reflective_inline = {cos(reflective_surface_motion.angle), sin(reflective_surface_motion.angle)};
					vec2 reflective_normal = {reflective_inline.y, -reflective_inline.x};
					float angle_between = atan2(reflective_normal.y, reflective_normal.x) - atan2(light_motion.velocity.y, light_motion.velocity.x);
					std::cout << angle_between * 180.f / M_PI << std::endl;
					vec2 reflected_velocity = -light_motion.velocity + 2.f * dot(light_motion.velocity, reflective_normal) * reflective_normal;
					std::cout << reflected_velocity.x << ", " << reflected_velocity.y << std::endl;
					std::cout << light_motion.velocity.x << ", " << light_motion.velocity.y << std::endl;
					player_motion.velocity = reflected_velocity;
					angle_between = atan2(reflective_normal.y, reflective_normal.x) - atan2(light_motion.velocity.y, light_motion.velocity.x);
					std::cout << angle_between * 180.f / M_PI << std::endl;
					player_motion.velocity.x += BARRIER_SPEED;
					if (StateSystem::is_advanced()) {
						registry.colors.get(player_car) = vec3(1.f, 0.f, 0.f);
					}
				}
			}
			// Checking Player - Eatable collisions
			else if (registry.eatables.has(entity_other)) {
				if (!registry.deathTimers.has(entity)) {
					// chew, count points, and set the LightUp timer
					registry.remove_all_components_of(entity_other);
					Mix_PlayChannel(-1, point_scored_sound, 0);
					StateSystem::increment_points(1);
					current_speed *= SPEED_FACTOR;
					if (StateSystem::is_advanced()) {
						if (registry.lit.has(player_car)) registry.lit.remove(player_car);
						registry.lit.emplace(player_car);
					}
				}
			}
		}

		if (registry.eatables.has(entity) && registry.deadlys.has(entity_other)) {
			if (registry.motions.get(entity).position.x > window_height_px) {
				registry.remove_all_components_of(entity);
			}
		}
	}
	// Remove all collisions from this simulation step
	registry.collisions.clear();
}

// Should the game be over ?
bool WorldSystem::is_over() const {
	return bool(glfwWindowShouldClose(window));
}

// Set the game to be over
void WorldSystem::end_game() const {
	glfwSetWindowShouldClose(window, true);
}

// Set a key to pressed
void WorldSystem::press_key(int key) {
	key_map[key] = true;
}

// Set a key to released
void WorldSystem::release_key(int key) {
	key_map[key] = false;
}

// Is key pressed?
bool WorldSystem::key_pressed(int key) {
	return key_map[key];
}

// On key callback
void WorldSystem::on_key(int key, int, int action, int mod) {
	if (action == GLFW_PRESS) {
		press_key(key);
	}
	if (action == GLFW_RELEASE) {
		release_key(key);
	}

	// Turning on advanced mode
	if (action == GLFW_RELEASE && key == GLFW_KEY_A) {
		std::cout << "Advanced Mode";
		StateSystem::set_advanced();
	}

	// Turning on basic mode
	if (action == GLFW_RELEASE && key == GLFW_KEY_B) {
		std::cout << "Basic Mode" << std::endl;
		StateSystem::set_basic();
	}

	// Resetting game
	if (action == GLFW_RELEASE && key == GLFW_KEY_R) {
		int w, h;
		glfwGetWindowSize(window, &w, &h);

        restart_game();
	}

	// Exiting game
	if (action == GLFW_RELEASE && key == GLFW_KEY_ESCAPE) {
		end_game();
	}

	// Debugging
	if (key == GLFW_KEY_D) {
		if (action == GLFW_RELEASE)
			debugging.in_debug_mode = false;
		else
			debugging.in_debug_mode = true;
	}

	// Control the current speed with `<` `>`
	if (action == GLFW_RELEASE && (mod & GLFW_MOD_SHIFT) && key == GLFW_KEY_COMMA) {
		current_speed -= 0.1f;
		printf("Current speed = %f\n", current_speed);
	}
	if (action == GLFW_RELEASE && (mod & GLFW_MOD_SHIFT) && key == GLFW_KEY_PERIOD) {
		current_speed += 0.1f;
		printf("Current speed = %f\n", current_speed);
	}
	current_speed = fmax(0.f, current_speed);
}