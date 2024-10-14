#pragma once

// internal
#include "common.hpp"

// stlib
#include <vector>
#include <random>

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_mixer.h>

#include "render_system.hpp"

// Container for all our entities and game logic. Individual rendering / update is
// deferred to the relative update() methods
class WorldSystem
{
public:
	static std::unordered_map<int, bool> button_map;

	WorldSystem();

	// Creates a window
	GLFWwindow* create_window();

	// starts the game
	void init(RenderSystem* renderer);

	// Releases all associated resources
	~WorldSystem();

	// Steps the game ahead by ms milliseconds
	bool step(float elapsed_ms);

	void basic_car_handling(float elapsed_ms_since_last_update);

	void advanced_car_handling(float elapsed_ms_since_last_update);


	// Check for collisions
	void handle_collisions();

	// Should the game be over ?
	bool is_over()const;

	// Ends the game
	void end_game() const;

	// Returns the number of points
	static int get_points();

private:
	// Input callback functions
	void on_key(int key, int, int action, int mod);
	static void on_mouse_move(vec2 pos);

	void on_mouse_click(int button, int action, int mods);

	static void click_button(int button);
	static void release_button(int button);
	static bool button_clicked(int button);

	static void press_key(int key);
	static void release_key(int key);
	static bool key_pressed(int key);

	static vec2 get_mouse_position();

	// restart level
	void restart_game();

	// OpenGL window handle
	GLFWwindow* window;

	// Number of bonuses collected by the bar, displayed in the window title
	static unsigned int points;

	// Game state
	RenderSystem* renderer;
	float current_speed;
	float next_barrier_spawn;
	float next_bonus_spawn;
	Entity player_car;
	Entity title;
	static std::unordered_map<int, bool> key_map;
	static vec2 mouse_pos;
	static float target_angle;

	// music references
	Mix_Music* background_music;
	Mix_Chunk* car_crash_sound;
	Mix_Chunk* point_scored_sound;

	// C++ random number generator
	std::default_random_engine rng;
	std::uniform_real_distribution<float> uniform_dist; // number between 0..1
};
