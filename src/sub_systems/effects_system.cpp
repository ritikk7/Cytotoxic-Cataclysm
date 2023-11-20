// internal
#pragma once
#include "sub_systems/effects_system.hpp"
#include "components.hpp"
#include "tiny_ecs_registry.hpp"
#include "world_system.hpp"


// Apply effect with timer, play sound, display icon
void EffectsSystem::apply_random_effect() {

	std::uniform_real_distribution<float> random_probability(0.f, 1.f);
	CYST_EFFECT_ID effect_id;

	if (random_probability(rng) <= POS_PROB) {
		// if health full, do not heal
		if (!getEffect(CYST_EFFECT_ID::HEAL).is_active && registry.healthValues.get(player).health >= MAX_HEALTH) {
			setActiveTimer(CYST_EFFECT_ID::HEAL, 2000);
		}
		if (countActivePositive() >= cyst_neg_start) {
			// no available positive effect
			playSound(CYST_EFFECT_ID::EFFECT_COUNT);
			return;
		}

		std::discrete_distribution<> effect_dist(posWeights.begin(), posWeights.end());
		do {
			effect_id = static_cast<CYST_EFFECT_ID>(effect_dist(rng));
		} while (getEffect(effect_id).is_active);
	}
	else {
		if (countActiveNegative() >= cyst_effect_count - cyst_neg_start) {
			// no available negative effect
			playSound(CYST_EFFECT_ID::EFFECT_COUNT);
			return;
		}

		std::discrete_distribution<> effect_dist(negWeights.begin(), negWeights.end());
		do {
			effect_id = static_cast<CYST_EFFECT_ID>(effect_dist(rng) + cyst_neg_start);
		} while (getEffect(effect_id).is_active);
	}

	getEffect(effect_id).is_active = true;

	// do effect
	switch (effect_id) {
		case CYST_EFFECT_ID::DAMAGE:
			handle_damage_effect();
			break;
		case CYST_EFFECT_ID::HEAL:
			handle_heal_effect();
			break;
		case CYST_EFFECT_ID::CLEAR_SCREEN:
			handle_clear_screen();
			break;
		case CYST_EFFECT_ID::SLOW:
			handle_slow_effect();
			break;
		case CYST_EFFECT_ID::FOV:
			handle_fov_effect();
			break;
		case CYST_EFFECT_ID::DIRECTION:
			handle_direction_effect();
			break;
		case CYST_EFFECT_ID::NO_ATTACK:
			handle_no_attack_effect();
			break;
		default:
			printf("error: unknown effect");
			break;
		}

	playSound(effect_id);
}

/*************************[ positive effects ]*************************/

void EffectsSystem::handle_damage_effect() {
	Weapon& weapon = registry.weapons.get(player);

	float prev_damage = weapon.damage;
	weapon.damage *= DAMAGE_MULTIPLIER;

	weapon.attack_delay *= ATTACK_DELAY_MULTIPLIER;

	float prev_speed = weapon.bullet_speed;
	weapon.bullet_speed *= BULLET_SPEED_MULTIPLIER;

	vec2 prev_size = weapon.size;
	weapon.size *= BULLET_SIZE_MULTIPLIER;

	vec4 prev_color = weapon.color;
	weapon.color = DAMAGE_BUFF_PROJECTILE_COLOR;

	Entity entity = Entity();

	TimedEvent& effect_timer = registry.timedEvents.emplace(entity);
	effect_timer.timer_ms = DAMAGE_EFFECT_TIME;
	effect_timer.callback = [this, prev_damage, prev_speed, prev_size, prev_color]() {
		Weapon& weapon = registry.weapons.get(player);
		weapon.damage = prev_damage;
		weapon.attack_delay = ATTACK_DELAY;
		weapon.bullet_speed = prev_speed;
		weapon.size = prev_size;
		weapon.color = prev_color;
		getEffect(CYST_EFFECT_ID::DAMAGE).is_active = false;
		};

	displayEffect(entity, CYST_EFFECT_ID::DAMAGE);
}

void EffectsSystem::handle_heal_effect() {
	registry.healthValues.get(player).health = MAX_HEALTH;
	setActiveTimer(CYST_EFFECT_ID::HEAL, -1);
}

void EffectsSystem::handle_clear_screen() {
	// start death timers for on-screen enemies
	vec2 camPos = registry.camera.components[0].position;
	for (int i = 0; i < registry.enemies.entities.size(); i++) {
		float distance = length(registry.transforms.get(registry.enemies.entities[i]).position - camPos);
		if (registry.enemies.components[i].type != ENEMY_ID::BOSS && distance < SCREEN_RADIUS * 0.9) {
			ws.startEntityDeath(registry.enemies.entities[i]);
		}
	}
	setActiveTimer(CYST_EFFECT_ID::CLEAR_SCREEN, -1);
}

/*************************[ negative effects ]*************************/

void EffectsSystem::handle_slow_effect() {
	Motion& motion = registry.motions.get(player);
	float prev_acceleration = motion.acceleration_unit;
	float prev_max_velocity = motion.max_velocity;
	motion.acceleration_unit = 0.3f;
	motion.max_velocity = 200.f;

	Entity entity = Entity();

	TimedEvent& effect_timer = registry.timedEvents.emplace(entity);
	effect_timer.timer_ms = 6000;
	effect_timer.callback = [this, prev_acceleration, prev_max_velocity]() {
		Motion& motion = registry.motions.get(player);
		motion.acceleration_unit = prev_acceleration;
		motion.max_velocity = prev_max_velocity;
		getEffect(CYST_EFFECT_ID::SLOW).is_active = false;
		};

	displayEffect(entity, CYST_EFFECT_ID::SLOW);
}

void EffectsSystem::handle_fov_effect() {
	Entity screen_state = registry.screenStates.entities[0];
	registry.screenStates.get(screen_state).limit_fov = true;
	int prev_volume = Mix_Volume(-1, -1);
	Mix_Volume(-1, prev_volume - 75);

	Entity entity = Entity();

	TimedEvent& effect_timer = registry.timedEvents.emplace(entity);
	effect_timer.timer_ms = 6000;
	effect_timer.callback = [this, prev_volume]() {
		registry.screenStates.components[0].limit_fov = false;
		Mix_Volume(-1, prev_volume);
		getEffect(CYST_EFFECT_ID::FOV).is_active = false;
		};

	displayEffect(entity, CYST_EFFECT_ID::FOV);
}

void EffectsSystem::handle_direction_effect() {
	// TODO
	TimedEvent& effect_timer = registry.timedEvents.emplace(Entity());
	effect_timer.callback = [this]() {
		getEffect(CYST_EFFECT_ID::DIRECTION).is_active = false;
		};
}

void EffectsSystem::handle_no_attack_effect() {
	registry.weapons.get(player).attack_delay = 99999.f;
	Mix_Chunk* prev_sound = soundChunks["player_shoot_1"];
	soundChunks["player_shoot_1"] = soundChunks["no_ammo"];

	Entity entity = Entity();

	TimedEvent& effect_timer = registry.timedEvents.emplace(entity);
	effect_timer.timer_ms = NO_ATTACK_TIME;
	effect_timer.callback = [this, prev_sound]() {
		registry.weapons.get(player).attack_delay = ATTACK_DELAY;
		registry.weapons.get(player).attack_timer = 0;
		soundChunks["player_shoot_1"] = prev_sound;
		getEffect(CYST_EFFECT_ID::NO_ATTACK).is_active = false;
		};

	displayEffect(entity, CYST_EFFECT_ID::NO_ATTACK);
}

/*************************[ helpers ]*************************/

int EffectsSystem::countActivePositive() {
	int count = 0;
	for (int i = 0; i < effects.size(); i++) {
		if (effects[i].is_active && effects[i].type == EFFECT_TYPE::POSITIVE) {
			++count;
		}
	}
	return count;
}
int EffectsSystem::countActiveNegative() {
	int count = 0;
	for (int i = 0; i < effects.size(); i++) {
		if (effects[i].is_active && effects[i].type == EFFECT_TYPE::NEGATIVE) {
			++count;
		}
	}
	return count;
}

Effect& EffectsSystem::getEffect(CYST_EFFECT_ID id) {
	for (int i = 0; i < effects.size(); i++) {
		if (effects[i].id == id) {
			return effects[i];
		}
	}
	assert(false && "CYST_EFFECT_ID not found");
	return effects[0];
}

void EffectsSystem::playSound(CYST_EFFECT_ID id) {

	if (id == CYST_EFFECT_ID::EFFECT_COUNT) {
		// signify no available effects
		Mix_PlayChannel(-1, soundChunks["cyst_empty"], 0);
	}
	else if (getEffect(id).type == EFFECT_TYPE::POSITIVE) {
		Mix_PlayChannel(-1, soundChunks["cyst_pos"], 0);
	}
	else if (getEffect(id).type == EFFECT_TYPE::NEGATIVE) {
		Mix_PlayChannel(-1, soundChunks["cyst_neg"], 0);
	}
}

void EffectsSystem::displayEffect(Entity effect, CYST_EFFECT_ID id) {
	int icon_offset = effect_to_position[id]; // can improve to fill gaps
	float offset = icon_offset * ICON_SIZE.x * ICON_SCALE + PADDING;
	Transform& transform = registry.transforms.emplace(effect);
	transform.position = EFFECTS_POSITION;
	transform.position.x += offset;
	transform.scale = ICON_SIZE * ICON_SCALE;
	transform.is_screen_coord = true;

	registry.renderRequests.insert(
		effect,
		{ effect_to_texture[id],
		EFFECT_ASSET_ID::TEXTURED,
		GEOMETRY_BUFFER_ID::SPRITE,
		RENDER_ORDER::UI });
}

// set timer for effect. timer value of -1 uses the default timer
void EffectsSystem::setActiveTimer(CYST_EFFECT_ID id, float timer) {
	getEffect(id).is_active = true;

	TimedEvent& effect_timer = registry.timedEvents.emplace(Entity());
	if (timer >= 0) {
		effect_timer.timer_ms = timer;
	}
	effect_timer.callback = [this, id]() {
		getEffect(id).is_active = false;
		};
}