#pragma once

#include "common.hpp"
#include "tiny_ecs.hpp"
#include "render_system.hpp"

// These are hardcoded to the dimensions of the entity texture
// BB = bounding box
const float BONUS_WIDTH  = 1.7f * 70.f;
const float BONUS_HEIGHT = 1.7f * 48.f;
const float CAR_WIDTH  = 1.5f * 71.f;
const float CAR_HEIGHT = 1.5f * 116.f;
const float TITLE_WIDTH = 1.5f * 396.f;
const float TITLE_HEIGHT = 1.5f * 60.f;
const float BARRIER_WIDTH   = 2.f * 62.f;	// 1001
const float BARRIER_HEIGHT  = 2.f * 210.f;	// 870

const float BARRIER_SPEED = -400.f;

const float SPEED_FACTOR = 1.05f;

// the player
Entity createCar(RenderSystem* renderer, vec2 pos);

// the prey
Entity createBonus(RenderSystem* renderer, vec2 position, float angle);

// the enemy
Entity createBarrier(RenderSystem* renderer, vec2 position);

// a red line for debugging purposes
Entity createLine(vec2 position, vec2 size);

// a egg
Entity createEgg(vec2 pos, vec2 size);

// the title
Entity createTitle(RenderSystem* renderer, vec2 pos);
