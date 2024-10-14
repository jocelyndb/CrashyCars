// internal
#include "physics_system.hpp"

#include <iostream>

#include "state_system.h"
#include "world_init.hpp"
#include "world_system.hpp"
#include "../../../../../../../../../Library/Developer/CommandLineTools/SDKs/MacOSX15.0.sdk/System/Library/Frameworks/CoreServices.framework/Frameworks/CarbonCore.framework/Headers/FixMath.h"

// Returns the local bounding coordinates (bottom left and top right)
// scaled by the current size of the entity
// rotated by the entity's current rotation, relative to the origin
std::array<vec2, 4> get_bounding_points(const Motion& motion) {
    // get vectors to top right of origin-located bounding rectangle
    vec2 top_right = vec2(abs(motion.scale.x)/2.f, abs(motion.scale.y)/2.f);
	vec2 bottom_right {top_right.x, -top_right.y};
	vec2 bottom_left {-top_right.x, -top_right.y};
	vec2 top_left {-top_right.x, top_right.y};

	// calculate rotation matrix
    float cos = ::cos(motion.angle);
    float sin = ::sin(motion.angle);
    mat2 rotation_matrix = mat2(cos, sin, -sin, cos);

	// rotate points about the origin by motion's angle
    top_right = rotation_matrix * top_right;
    bottom_right = rotation_matrix * bottom_right;
    bottom_left = rotation_matrix * bottom_left;
    top_left = rotation_matrix * top_left;
	// bottom left and top left corners of rectangle are
	// -top_right and -bottom_right respectively (by math)
    return {top_right, bottom_right, bottom_left, top_left};
}

// Returns the minimum and maximum magnitudes of points
// projected onto an axis as a vec2
vec2 get_projected_min_max(const std::array<vec2, 4>& bounding_points,
						   const vec2 axis) {
	vec2 min_max = {positiveInfinity, negativeInfinity};
	for (const vec2& point : bounding_points) {
		float projected = dot(point, axis);
		min_max = {min(projected, min_max.x), max(projected, min_max.y)};
	}
	return min_max;
}

// Returns true if neither vector has a component between those of the
// other vector, implying no overlap
// Invariant: assume the vectors contain minimum and maximum
// values for points projected onto an axis in their x and y
// fields respectively
bool no_overlap(std::array<vec2, 4> m1_bounding_points,
				std::array<vec2, 4> m2_bounding_points,
				float angle) {
	vec2 axis = {::cos(angle), ::sin(angle)};
	vec2 m1_min_max = get_projected_min_max(m1_bounding_points, axis);
	vec2 m2_min_max = get_projected_min_max(m2_bounding_points, axis);
	return m1_min_max.x > m2_min_max.y || m1_min_max.y < m2_min_max.x;
}

// Returns true if motion1 and motion2 are overlapping using a coarse
// step with radial boundaries and a fine step with the separating axis theorem
bool collides(const Motion& motion1, const Motion& motion2) {
	// see if the distance between centre points of motion1 and motion2
	// are within the maximum possible distance for them to be touching
	vec2 dp = motion1.position - motion2.position;
	float dist_squared = dot(dp, dp);
	float max_possible_collision_distance = (dot(motion1.scale, motion1.scale) +
											 dot(motion2.scale, motion2.scale)) / 2.f;
	// radial boundary-based estimate
	if (dist_squared < max_possible_collision_distance) {
		// std::cout << "Collision possible. Refining..." << std::endl;
		// move points to their screen location
		std::array<vec2, 4> m1_bounding_points = get_bounding_points(motion1);
		for (vec2& point : m1_bounding_points) {
			point += motion1.position;
		}
		std::array<vec2, 4> m2_bounding_points = get_bounding_points(motion2);
		for (vec2& point : m2_bounding_points) {
			point += motion2.position;
		}
		// define all axis angles (normals to edges)
		float axis_angles[4];
		axis_angles[0] = motion1.angle;
		axis_angles[1] = M_PI_2 + motion1.angle;
		axis_angles[2] = motion2.angle;
		axis_angles[3] = M_PI_2 + motion2.angle;
		// see if an overlap exists in any of the axes
		for (float& angle : axis_angles) {
			if (no_overlap(m1_bounding_points, m2_bounding_points, angle)) {
				// std::cout << "No collision!" << std::endl;
				return false;
			}
		}
		// std::cout << "Collision detected between motion with position (";
		// std::cout << motion1.position.x << ", " << motion1.position.y << ") ";
		// std::cout << "and motion with position ";
		// std::cout << motion2.position.x << ", " << motion2.position.y << ") " << std::endl;
		return true;
	}
	return false;
}

// Handle drift
void PhysicsSystem::car_drift(float elapsed_ms) {
	Entity& player_car = registry.players.entities[0];
	assert(registry.players.entities.size() == 1);
	Motion& player_motion = registry.motions.get(player_car);
	float cos_angle = cosf(player_motion.angle);
	float sin_angle = sinf(player_motion.angle);
	float& x_velocity = player_motion.velocity.x;
	float& y_velocity = player_motion.velocity.y;

	float inline_component = -x_velocity * cos_angle -y_velocity * sin_angle;
	float normal_component = -x_velocity * sin_angle -y_velocity * cos_angle;
	// Calculate drag using Stokes' law: F_d = 6 * pi * width * velocity * viscosity
	// Here, to simplify calculation, we note that the velocity change due to drag is F_d * t,
	float normal_drag = 6.f * M_PI * 0.00085f * player_motion.scale.x;
	float inline_drag = -6.f * M_PI * 0.00045f * player_motion.scale.y;
	// std::cout << normal_drag << std::endl;
	// std::cout << inline_drag << std::endl;
	float x_drag = ((normal_component * normal_drag * sin_angle) +
			(inline_component * inline_drag * cos_angle)) * (elapsed_ms / 1000.f);
	float y_drag = ((normal_component * normal_drag * cos_angle) +
			(inline_component * inline_drag * sin_angle)) * (elapsed_ms / 1000.f);
	if (abs(player_motion.velocity.x) > 7.f && abs(player_motion.velocity.x) >= abs(x_drag)) {
		player_motion.velocity.x += x_drag;
	} else {
		player_motion.velocity.x = 0;
	}
	if (abs(player_motion.velocity.y) > 7.f && abs(player_motion.velocity.y) >= abs(y_drag)) {
		player_motion.velocity.y += y_drag;
	} else {
		player_motion.velocity.y = 0;
	}

	// // Drift backwards with road
	// player_motion.position += vec2(BARRIER_SPEED * elapsed_ms * 0.001f * 0.5f, 0.f);
}

void PhysicsSystem::step(float elapsed_ms)
{
	// std::cout << "Current salmon angle:" << player_motion.angle << std::endl;
	// Move car based on how much time has passed, this is to (partially) avoid
	// having entities move at different speed based on the machine.
	auto& motion_registry = registry.motions;
	unsigned int points = StateSystem::get_points();
	float point_multiplier = pow(SPEED_FACTOR, points);
	for(uint i = 0; i< motion_registry.size(); i++)
	{
		Motion& motion = motion_registry.components[i];
		// Entity entity = motion_registry.entities[i];
		float step_seconds = elapsed_ms / 1000.f;
		motion.position += step_seconds * motion.velocity * vec2(point_multiplier, point_multiplier);
		// (void)elapsed_ms; // placeholder to silence unused warning until implemented
	}

	// Handle drift if in advanced mode
	if (StateSystem::is_advanced() && !registry.deathTimers.has(registry.players.entities[0])) {
		car_drift(elapsed_ms);
	}

	// Check for collisions between all moving entities
    ComponentContainer<Motion> &motion_container = registry.motions;
	for(uint i = 0; i<motion_container.components.size(); i++)
	{
		Motion& motion_i = motion_container.components[i];
		Entity entity_i = motion_container.entities[i];
		
		// note starting j at i+1 to compare all (i,j) pairs only once (and to not compare with itself)
		for(uint j = i+1; j<motion_container.components.size(); j++)
		{
			Motion& motion_j = motion_container.components[j];
			if (collides(motion_i, motion_j))
			{
				Entity entity_j = motion_container.entities[j];
				// Create a collisions event
				// We are abusing the ECS system a bit in that we potentially insert muliple collisions for the same entity
				registry.collisions.emplace_with_duplicates(entity_i, entity_j);
				registry.collisions.emplace_with_duplicates(entity_j, entity_i);
			}
		}
	}
}
