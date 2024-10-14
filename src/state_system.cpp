//
// Created by Jocelyn Baker on 2024-09-30.
//

#include "state_system.h"

bool StateSystem::advanced;
unsigned int StateSystem::points;

// Initialize state system
void StateSystem::init() {
    advanced = true;
    points = 0;
}

// Return if game in advanced mode
bool StateSystem::is_advanced() {
    return advanced;
}

// Set game to advanced mode
void StateSystem::set_advanced() {
    advanced = true;
}

// Set game to basic mode
void StateSystem::set_basic() {
    advanced = false;
}

void StateSystem::increment_points(unsigned int value = 1) {
    points += value;
}

void StateSystem::reset_points() {
    points = 0;
}

unsigned int StateSystem::get_points() {
    return points;
}


