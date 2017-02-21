#ifndef ACTOR_DEF_TYPES_H
#define ACTOR_DEF_TYPES_H

#include "cal_types.h"
#include "hash.h"
#include "platform.h"

#define MAX_ACTOR_DEFS  256
#define	MAX_FILE_PATH	128	// the max chars allowed int a path/filename for actor textures/masks

// default duration in ms of a step when an actor is walking
#define DEFAULT_STEP_DURATION 250

//GLOWS
#define GLOW_NONE 0 	/*!< RGB: 0.0, 0.0, 0.0*/
#define GLOW_FIRE 1 	/*!< RGB: 0.5, 0.1, 0.1*/
#define GLOW_COLD 2 	/*!< RGB: 0.1, 0.1, 0.5*/
#define GLOW_THERMAL 3 	/*!< RGB: 0.5, 0.1, 0.5*/
#define GLOW_MAGIC 4	/*!< RGB: 0.5, 0.4, 0.0*/

typedef enum
{
	ACTOR_HEAD_SIZE = 0,
	ACTOR_SHIELD_SIZE,
	ACTOR_CAPE_SIZE,
	ACTOR_HELMET_SIZE,
	ACTOR_WEAPON_SIZE,
	ACTOR_SHIRT_SIZE,
	ACTOR_SKIN_SIZE,
	ACTOR_HAIR_SIZE,
	ACTOR_BOOTS_SIZE,
	ACTOR_LEGS_SIZE,
	ACTOR_NECK_SIZE,
	ACTOR_EYES_SIZE,
	ACTOR_NUM_PARTS
} actor_parts_enum;

/*! Sets the main model type*/
typedef struct
{
	char model_name[MAX_FILE_PATH];
	char skin_name[MAX_FILE_PATH];
	char skin_mask[MAX_FILE_PATH];
	int glow;
	int mesh_index;
} body_part;

/*! Sets the shield type*/
typedef struct
{
	char model_name[MAX_FILE_PATH];
	char skin_name[MAX_FILE_PATH];
	char skin_mask[MAX_FILE_PATH];
	int glow;
	int mesh_index;

	int missile_type; /*!< The type of equipped missiles (>=0 if a quiver is equipped, -1 if a regular shield is equipped) */
} shield_part;

/*! Sets the weapon type (including animation frame names)*/
typedef struct
{
	char model_name[MAX_FILE_PATH];
	char skin_name[MAX_FILE_PATH];
	char skin_mask[MAX_FILE_PATH];
	int glow;
	int mesh_index;
	int turn_horse;
	int unarmed;

	struct cal_anim cal_frames[NUM_WEAPON_FRAMES];
} weapon_part;

/*! Defines the main models looks*/
typedef struct
{
	char model_name[MAX_FILE_PATH];
	char arms_name[MAX_FILE_PATH];
	char torso_name[MAX_FILE_PATH];
	char arms_mask[MAX_FILE_PATH];
	char torso_mask[MAX_FILE_PATH];
	int mesh_index;
} shirt_part;

/*! Sets the models hands and head*/
typedef struct
{
	char hands_name[MAX_FILE_PATH];
	char head_name[MAX_FILE_PATH];
	char arms_name[MAX_FILE_PATH];
	char body_name[MAX_FILE_PATH];
	char legs_name[MAX_FILE_PATH];
	char feet_name[MAX_FILE_PATH];
} skin_part;

/*! Sets the models hair name*/
typedef struct
{
	char hair_name[MAX_FILE_PATH];
} hair_part;

/*! Sets the models eyes name*/
typedef struct
{
	char eyes_name[MAX_FILE_PATH];
} eyes_part;

/*! A structure used when loading the actor definitions
 * \sa init_actor_defs*/
typedef struct cal_anim_group
{
	char name[32];
	int count;
	struct cal_anim anim[16];
} cal_animations;

#ifdef NEW_SOUND
typedef struct
{
	int sound;
	float scale;
} act_extra_sound;
#endif // NEW_SOUND

typedef struct
{
	int is_holder;      /*!< Specifies if this type of actor hold the actor to which it is attached or if he is held */
	int parent_bone_id; /*!< The bone to use on the actor to which it is attached */
	int local_bone_id;  /*!< The bone to use on the actor that is attached */
	float shift[3];     /*!< The shift to apply to the actor that is held */
	struct cal_anim cal_frames[NUM_ATTACHED_ACTOR_FRAMES];
} attachment_props;

/*!
 * Structure containing how an actor type is attached to all other actors types
 */
typedef struct
{
	attachment_props actor_type[MAX_ACTOR_DEFS]; /*!< Attachment properties for each kind of actor */
} attached_actors_types;

typedef struct
{
	/*! \name Model data*/
	/*! \{ */
	int actor_type;
	char actor_name[66];
	char skin_name[MAX_FILE_PATH];
	char file_name[256];
	/*! \} */

	float actor_scale;
	float scale;
	float mesh_scale;
	float skel_scale;

	struct CalCoreModel *coremodel;
	struct CalHardwareModel* hardware_model;
	GLuint vertex_buffer;
	GLuint index_buffer;
	GLenum index_type;
	Uint32 index_size;
	//Animation indexes
	cal_animations idle_group[16];//16 animation groups
	int group_count;

	struct cal_anim cal_frames[NUM_ACTOR_FRAMES];
	hash_table *emote_frames;

	int skeleton_type;

#ifdef NEW_SOUND
	// Extra sounds
	act_extra_sound battlecry;
#endif // NEW_SOUND

	/*! \name The different body parts (different head shapes, different armour/weapon shapes etc.)*/
	/*! \{ */
	body_part *head;
	shield_part *shield;
	body_part *cape;
	body_part *helmet;
	body_part *neck;
	weapon_part *weapon;
	/*! \} */

	/*! \name Clothing*/
	/*! \{ */
	shirt_part *shirt;
	skin_part  *skin;
	hair_part  *hair;
	eyes_part  *eyes;
	body_part *boots;
	body_part *legs;
	/*! \} */

	/*! \name The current actors walk/run speeds*/
	/*! \{ */
	double walk_speed; // unused
	double run_speed; // unused
	char ghost;
	/*! \} */

	int step_duration;
} actor_types;

#endif /* ACTOR_DEF_TYPES_H */
