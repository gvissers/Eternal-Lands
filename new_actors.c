#include <assert.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include "new_actors.h"
#include "actor_scripts.h"
#include "asc.h"
#include "bbox_tree.h"
#include "buffs.h"
#include "cal.h"
#include "console.h"
#include "dialogues.h"
#include "draw_scene.h"
#include "errors.h"
#include "filter.h"
#include "global.h"
#include "init.h"
#include "missiles.h"
#include "sound.h"
#include "textures.h"
#include "tiles.h"
#include "translate.h"
#ifdef CLUSTER_INSIDES
#include "cluster.h"
#endif
#include "eye_candy_wrapper.h"
#ifdef OPENGL_TRACE
#include "gl_init.h"
#endif
#include "io/elfilewrapper.h"
#include "actor_init.h"

float sitting=1.0f;
glow_color glow_colors[10];

//build the glow color table
void build_glow_color_table()
{
	glow_colors[GLOW_NONE].r=0;
	glow_colors[GLOW_NONE].g=0;
	glow_colors[GLOW_NONE].b=0;

	glow_colors[GLOW_FIRE].r=0.5f;
	glow_colors[GLOW_FIRE].g=0.1f;
	glow_colors[GLOW_FIRE].b=0.1f;

	glow_colors[GLOW_COLD].r=0.1f;
	glow_colors[GLOW_COLD].g=0.1f;
	glow_colors[GLOW_COLD].b=0.5f;

	glow_colors[GLOW_THERMAL].r=0.5f;
	glow_colors[GLOW_THERMAL].g=0.1f;
	glow_colors[GLOW_THERMAL].b=0.5f;

	glow_colors[GLOW_MAGIC].r=0.5f;
	glow_colors[GLOW_MAGIC].g=0.4f;
	glow_colors[GLOW_MAGIC].b=0.0f;
}

//return the ID (number in the actors_list[]) of the new allocated actor
#ifdef	NEW_TEXTURES
int add_enhanced_actor(enhanced_actor *this_actor, float x_pos, float y_pos,
	float z_pos, float z_rot, float scale, int actor_id, const char* name)
#else	// NEW_TEXTURES
int add_enhanced_actor(enhanced_actor *this_actor, float x_pos, float y_pos,
					   float z_pos, float z_rot, float scale, int actor_id)
#endif	// NEW_TEXTURES
{
	int texture_id;
	int i;
	int k;
	actor *our_actor;
#ifdef CLUSTER_INSIDES
	int x, y;
#endif

	no_bounding_box=1;
#ifdef EXTRA_DEBUG
	ERR();
#endif

	//get the skin
#ifdef	NEW_TEXTURES
	texture_id = load_enhanced_actor(this_actor, name);
#else	/* NEW_TEXTURES */
	texture_id= load_bmp8_enhanced_actor(this_actor, 255);
#endif	/* NEW_TEXTURES */

	our_actor = calloc(1, sizeof(actor));
#ifndef	NEW_TEXTURES
	our_actor->has_alpha= this_actor->has_alpha;
#endif	/* NEW_TEXTURES */

	memset(our_actor->current_displayed_text, 0, MAX_CURRENT_DISPLAYED_TEXT_LEN);
	our_actor->current_displayed_text_time_left =  0;

	our_actor->texture_id=texture_id;
	our_actor->is_enhanced_model=1;
	our_actor->actor_id=actor_id;

	our_actor->cal_h_rot_start = 0.0;
	our_actor->cal_h_rot_end = 0.0;
	our_actor->cal_v_rot_start = 0.0;
	our_actor->cal_v_rot_end = 0.0;
	our_actor->cal_rotation_blend = -1.0;
	our_actor->cal_rotation_speed = 0.0;
	our_actor->are_bones_rotating = 0;
	our_actor->in_aim_mode = 0;
	our_actor->range_actions_count = 0;
	our_actor->delayed_item_changes_count = 0;
	our_actor->delay_texture_item_changes = 1;

	our_actor->x_pos=x_pos;
	our_actor->y_pos=y_pos;
	our_actor->z_pos=z_pos;
	our_actor->scale=scale;

	our_actor->x_speed=0;
	our_actor->y_speed=0;
	our_actor->z_speed=0;

	our_actor->x_rot=0;
	our_actor->y_rot=0;
	our_actor->z_rot=z_rot;

	our_actor->last_range_attacker_id = -1;

	//reset the script related things
	our_actor->move_x_speed=0;
	our_actor->move_y_speed=0;
	our_actor->move_z_speed=0;
	our_actor->rotate_x_speed=0;
	our_actor->rotate_y_speed=0;
	our_actor->rotate_z_speed=0;
	our_actor->movement_time_left=0;
	our_actor->moving=0;
	our_actor->rotating=0;
	our_actor->busy=0;
	our_actor->last_command=nothing;
#ifdef	ANIMATION_SCALING
	our_actor->animation_scale = 1.0f;
#endif	/* ANIMATION_SCALING*/

	//clear the que
	for(k=0; k<MAX_CMD_QUEUE; k++)	our_actor->que[k]=nothing;
	for(k=0;k<MAX_EMOTE_QUEUE;k++)	{
		our_actor->emote_que[k].emote=NULL;
		our_actor->emote_que[k].origin=NO_EMOTE;
		our_actor->emote_que[k].create_time=0;
		}
	memset(&our_actor->cur_emote,0,sizeof(emote_anim));
	memset(&our_actor->poses,0,sizeof(emote_data*)*4);
	for(k=0;k<MAX_EMOTE_FRAME;k++) our_actor->cur_emote.frames[k].anim_index=-1;
	our_actor->cur_emote.idle.anim_index=-1;
	our_actor->cur_emote_sound_cookie=0;



//	our_actor->model_data=0;
	our_actor->stand_idle=0;
	our_actor->sit_idle=0;
	our_actor->body_parts=this_actor;

	our_actor->attached_actor = -1;
	our_actor->attachment_shift[0] = our_actor->attachment_shift[1] = our_actor->attachment_shift[2] = 0.0;

#ifdef CLUSTER_INSIDES
	x = (int) (our_actor->x_pos / 0.5f);
	y = (int) (our_actor->y_pos / 0.5f);
	our_actor->cluster = get_cluster (x, y);
#endif

	//find a free spot, in the actors_list
	LOCK_ACTORS_LISTS();	//lock it to avoid timing issues
	for(i=0; i<max_actors; i++){
		if(!actors_list[i])	break;
	}

	if (actor_id == yourself)
	{
		// We have just returned from limbo (after teleport,
		// map change, disconnect, etc.) Since our position may
		// have changed, tell the bbox tree to update so
		// that our environment is recomputed.
		set_all_intersect_update_needed (main_bbox_tree);
		set_our_actor (our_actor);
	}

	actors_list[i]=our_actor;

	if(i >= max_actors) max_actors = i+1;

	no_bounding_box=0;
	//Actors list will be unlocked later

	return i;
}

#ifdef	NEW_TEXTURES
Uint32 delay_texture_item_change(actor* a, const int which_part, const int which_id)
{
	if (a == 0)
	{
		return 0;
	}

	if (a->delay_texture_item_changes != 0)
	{
		change_enhanced_actor(a->texture_id, a->body_parts);

		if (a->delayed_item_changes_count < MAX_ITEM_CHANGES_QUEUE)
		{
			a->delayed_item_changes[a->delayed_item_changes_count] = which_id;
			a->delayed_item_type_changes[a->delayed_item_changes_count] = which_part;
			a->delayed_item_changes_count++;

			return 1;
		}
	}

	return 0;
}
#endif	/* NEW_TEXTURES */

void unwear_item_from_actor(int actor_id,Uint8 which_part)
{
	int i;


	for (i = 0; i < max_actors; i++)
	{
		if (!actors_list[i] || actors_list[i]->actor_id!=actor_id)
			continue;

		if (which_part == KIND_OF_WEAPON)
		{
			ec_remove_weapon(actors_list[i]);
#ifndef	NEW_TEXTURES
			if (actors_list[i]->in_aim_mode > 0) {
				if (actors_list[i]->delayed_item_changes_count < MAX_ITEM_CHANGES_QUEUE) {
					missiles_log_message("%s (%d): unwear item type %d delayed",
											actors_list[i]->actor_name, actors_list[i]->actor_id, which_part);
					actors_list[i]->delayed_item_changes[actors_list[i]->delayed_item_changes_count] = -1;
					actors_list[i]->delayed_item_type_changes[actors_list[i]->delayed_item_changes_count] = which_part;
					++actors_list[i]->delayed_item_changes_count;
				}
				else {
					LOG_ERROR("the item changes queue is full!");
				}
				return;
			}
#endif	/* NEW_TEXTURES */
			if (actors_list[i]->cur_weapon == GLOVE_FUR
				|| actors_list[i]->cur_weapon == GLOVE_LEATHER)
			{
#ifdef XML_COMPILED
				actors_list[i]->body_parts->hands_tex = actors_list[i]->body_parts->hands_tex_save;
#else // XML_COMPILED
				my_strcp(actors_list[i]->body_parts->hands_tex,
					actors_list[i]->body_parts->hands_tex_save);
#endif // XML_COMPILED
#ifndef	NEW_TEXTURES
				glDeleteTextures(1,&actors_list[i]->texture_id);
				actors_list[i]->texture_id=load_bmp8_enhanced_actor(actors_list[i]->body_parts, 255);
#endif	/* NEW_TEXTURES */

			}
#ifdef	NEW_TEXTURES
			if (delay_texture_item_change(actors_list[i], which_part, -1))
			{
				return;
			}
#endif	/* NEW_TEXTURES */
			model_detach_mesh(actors_list[i], actors_defs[actors_list[i]->actor_type].weapon[actors_list[i]->cur_weapon].mesh_index);
#ifdef XML_COMPILED
			actors_list[i]->body_parts->weapon_tex = "";
#else // XML_COMPILED
			actors_list[i]->body_parts->weapon_tex[0]=0;
#endif // XML_COMPILED
			actors_list[i]->cur_weapon = WEAPON_NONE;
			actors_list[i]->body_parts->weapon_meshindex = -1;
			return;
		}
		else if (which_part == KIND_OF_SHIELD)
		{
			if (actors_list[i]->in_aim_mode > 0)
			{
				if (actors_list[i]->delayed_item_changes_count < MAX_ITEM_CHANGES_QUEUE) {
					missiles_log_message("%s (%d): unwear item type %d delayed",
											actors_list[i]->actor_name, actors_list[i]->actor_id, which_part);
					actors_list[i]->delayed_item_changes[actors_list[i]->delayed_item_changes_count] = -1;
					actors_list[i]->delayed_item_type_changes[actors_list[i]->delayed_item_changes_count] = which_part;
					++actors_list[i]->delayed_item_changes_count;
				}
				else {
					LOG_ERROR("the item changes queue is full!");
				}
			}
			else
			{
				model_detach_mesh(actors_list[i], actors_list[i]->body_parts->shield_meshindex);
#ifdef XML_COMPILED
				actors_list[i]->body_parts->shield_tex = "";
#else // XML_COMPILED
				actors_list[i]->body_parts->shield_tex[0]=0;
#endif // XML_COMPILED
				actors_list[i]->cur_shield = SHIELD_NONE;
				actors_list[i]->body_parts->shield_meshindex = -1;
			}
		}
		else if (which_part == KIND_OF_CAPE)
		{
			model_detach_mesh(actors_list[i], actors_list[i]->body_parts->cape_meshindex);
#ifdef XML_COMPILED
			actors_list[i]->body_parts->cape_tex = "";
#else // XML_COMPILED
			actors_list[i]->body_parts->cape_tex[0]=0;
#endif // XML_COMPILED
			actors_list[i]->body_parts->cape_meshindex = -1;
		}
		else if (which_part == KIND_OF_HELMET)
		{
			model_detach_mesh(actors_list[i], actors_list[i]->body_parts->helmet_meshindex);
#ifdef XML_COMPILED
			actors_list[i]->body_parts->helmet_tex = "";
#else
			actors_list[i]->body_parts->helmet_tex[0]=0;
#endif
			actors_list[i]->body_parts->helmet_meshindex = -1;
		}
		else if (which_part == KIND_OF_NECK)
		{
			model_detach_mesh(actors_list[i], actors_list[i]->body_parts->neck_meshindex);
#ifdef XML_COMPILED
			actors_list[i]->body_parts->neck_tex = "";
#else
			actors_list[i]->body_parts->neck_tex[0]=0;
#endif
			actors_list[i]->body_parts->neck_meshindex = -1;
		}

		return;
	}

#ifdef OPENGL_TRACE
CHECK_GL_ERRORS();
#endif //OPENGL_TRACE
}

#ifdef XML_COMPILED
#ifdef CUSTOM_LOOK
static char** all_custom_paths = NULL;
static int all_custom_paths_size = 0;
static int all_custom_paths_used = 0;

const char* add_custom_path(const char* path, const char* custom_path)
{
	char *copy;

	if (all_custom_paths_used >= all_custom_paths_size)
	{
		int new_size = all_custom_paths_size ? 2*all_custom_paths_size : 8;
		char** new_paths = realloc(all_custom_paths, sizeof(char*));
		if (!new_paths)
		{
			LOG_ERROR("Failed to allocate memory for custom paths");
			return path;
		}
		all_custom_paths = new_paths;
		all_custom_paths_size = new_size;
	}

	copy = strdup(custom_path);
	if (!copy)
	{
		LOG_ERROR("Failed to duplicate custom path.");
		return path;
	}

	all_custom_paths[all_custom_paths_used++] = copy;
	return copy;
}

void free_custom_paths()
{
	int i;
	for (i = 0; i < all_custom_paths_used; ++i)
		free(all_custom_paths[i]);
	free(all_custom_paths);
	all_custom_paths = NULL;
	all_custom_paths_size = all_custom_paths_used = 0;
}

const char* custom_path(const char* path, const char* custom1, const char* custom2)
{
	char buffer[256];

	// check to see if ANY processing needs to be done
#ifdef CUSTOM_UPDATE
	if (!path || !*path || !custom_clothing)
		return path;
#else
	if (!path || !*path )
		return path;
#endif

	/* Check if custom1 has path readable */
	safe_snprintf(buffer, sizeof(buffer), "%s%s", custom1, path);
	if (el_custom_file_exists(buffer))
		return add_custom_path(path, buffer);

	/* Check if custom2 has path readable */
	safe_snprintf(buffer, sizeof(buffer), "%s%s", custom2, path);
	if (el_custom_file_exists(buffer))
		return add_custom_path(path, buffer);

	/* leave as is */
	return path;
}
#else // CUSTOM_LOOK
const char* custom_path(const char* path, const char* custom1, const char* custom2)
{
	return path;
}
#endif // CUSTOM_LOOK
#else // XML_COMPILED
#ifdef CUSTOM_LOOK
void custom_path(char* path, const char* custom1, const char* custom2)
{
	char buffer[256];

	// check to see if ANY processing needs to be done
#ifdef CUSTOM_UPDATE
	if (!path || !*path || !custom_clothing)
		return;
#else
	if (!path || !*path )
		return;
#endif

	/* Check if custom1 has path readable */
	safe_snprintf(buffer, sizeof(buffer), "%s%s", custom1, path);
	if (el_custom_file_exists(buffer))
	{
		my_strcp(path, buffer);
		return;
	}

	/* Check if custom2 has path readable */
	safe_snprintf(buffer, sizeof(buffer), "%s%s", custom2, path);
	if (el_custom_file_exists(buffer))
	{
		my_strcp(path, buffer);
		return;
	}

	/* leave as is */
	return;
}
#endif // CUSTOM_LOOK
#endif // XML_COMPILED

void actor_wear_item(int actor_id,Uint8 which_part, Uint8 which_id)
{
	int i;
#ifdef CUSTOM_LOOK
	char playerpath[256], guildpath[256], onlyname[32]={0};
	int j, off;
#endif

	for (i = 0; i < max_actors; i++)
	{
		actor *act = actors_list[i];
		const actor_types *def;

		if (!act || act->actor_id != actor_id)
			continue;

		def = actors_defs + act->actor_type;

#ifdef CUSTOM_LOOK
		safe_snprintf(guildpath, sizeof(guildpath), "custom/guild/%d/",
			act->body_parts->guild_id);
		off = act->actor_name[0] > 'z' ? 1 : 0;
		for (j = 0; j < 30; j++)
		{
			if (act->actor_name[j] == ' ' || act->actor_name[j] > 125)
				break;
		}
		safe_strncpy(onlyname, act->actor_name + off, j - off + 1);
		my_tolower(onlyname);
		safe_snprintf(playerpath, sizeof(playerpath), "custom/player/%s/", onlyname);
#endif
#ifndef	NEW_TEXTURES
		if (act->in_aim_mode > 0
			&& (which_part == KIND_OF_WEAPON || which_part == KIND_OF_SHIELD))
		{
			if (act->delayed_item_changes_count < MAX_ITEM_CHANGES_QUEUE)
			{
				missiles_log_message("%s (%d): wear item type %d delayed",
					act->actor_name, act->actor_id, which_part);
				act->delayed_item_changes[act->delayed_item_changes_count] = which_id;
				act->delayed_item_type_changes[act->delayed_item_changes_count] = which_part;
				++act->delayed_item_changes_count;
			}
			else
			{
				LOG_ERROR("the item changes queue is full!");
			}
			return;
		}
#endif	/* NEW_TEXTURES */
		if (which_part==KIND_OF_WEAPON)
		{
			if (which_id == GLOVE_FUR || which_id == GLOVE_LEATHER)
			{
#ifdef XML_COMPILED
				act->body_parts->hands_tex
					= custom_path(def->weapon[which_id].skin_name, playerpath, guildpath);
				act->body_parts->hands_mask
					 = custom_path(def->weapon[which_id].skin_mask, playerpath, guildpath);
#else // XML_COMPILED
				my_strcp(act->body_parts->hands_tex, def->weapon[which_id].skin_name);
				my_strcp(act->body_parts->hands_mask, def->weapon[which_id].skin_mask);
#ifdef CUSTOM_LOOK
				custom_path(act->body_parts->hands_tex, playerpath, guildpath);
				custom_path(act->body_parts->hands_mask, playerpath, guildpath);
#endif // CUSTOM_LOOK
#endif // XML_COMPILED
			}
			else
			{
#ifdef XML_COMPILED
				act->body_parts->weapon_tex
					= custom_path(def->weapon[which_id].skin_name, playerpath, guildpath);
#else // XML_COMPILED
				my_strcp(act->body_parts->weapon_tex, def->weapon[which_id].skin_name);
#ifdef CUSTOM_LOOK
				custom_path(act->body_parts->weapon_tex, playerpath, guildpath);
#endif // CUSTOM_LOOK
#endif // XML_COMPILED
			}
#ifdef	NEW_TEXTURES
			if (delay_texture_item_change(act, which_part, which_id))
			{
				return;
			}
#endif	/* NEW_TEXTURES */
			model_attach_mesh(act, def->weapon[which_id].mesh_index);
			act->cur_weapon=which_id;
			act->body_parts->weapon_meshindex = def->weapon[which_id].mesh_index;
			act->body_parts->weapon_glow=def->weapon[which_id].glow;
			switch (which_id)
			{
				case SWORD_1_FIRE:
				case SWORD_2_FIRE:
				case SWORD_3_FIRE:
				case SWORD_4_FIRE:
				case SWORD_4_THERMAL:
				case SWORD_5_FIRE:
				case SWORD_5_THERMAL:
				case SWORD_6_FIRE:
				case SWORD_6_THERMAL:
				case SWORD_7_FIRE:
				case SWORD_7_THERMAL:
					ec_create_sword_of_fire(act, (poor_man ? 6 : 10));
					break;
				case SWORD_2_COLD:
				case SWORD_3_COLD:
				case SWORD_4_COLD:
				case SWORD_5_COLD:
				case SWORD_6_COLD:
				case SWORD_7_COLD:
					ec_create_sword_of_ice(act, (poor_man ? 6 : 10));
					break;
				case SWORD_3_MAGIC:
				case SWORD_4_MAGIC:
				case SWORD_5_MAGIC:
				case SWORD_6_MAGIC:
				case SWORD_7_MAGIC:
					ec_create_sword_of_magic(act, (poor_man ? 6 : 10));
					break;
				case SWORD_EMERALD_CLAYMORE:
					ec_create_sword_emerald_claymore(act, (poor_man ? 6 : 10));
					break;
				case SWORD_CUTLASS:
					ec_create_sword_cutlass(act, (poor_man ? 6 : 10));
					break;
				case SWORD_SUNBREAKER:
					ec_create_sword_sunbreaker(act, (poor_man ? 6 : 10));
					break;
				case SWORD_ORC_SLAYER:
					ec_create_sword_orc_slayer(act, (poor_man ? 6 : 10));
					break;
				case SWORD_EAGLE_WING:
					ec_create_sword_eagle_wing(act, (poor_man ? 6 : 10));
					break;
				case SWORD_JAGGED_SABER:
					ec_create_sword_jagged_saber(act, (poor_man ? 6 : 10));
					break;
				case STAFF_3: // staff of protection
					ec_create_staff_of_protection(act, (poor_man ? 6 : 10));
					break;
				case STAFF_4: // staff of the mage
					ec_create_staff_of_the_mage(act, (poor_man ? 6 : 10));
					break;
			}
		}
		else if (which_part==KIND_OF_SHIELD)
		{
#ifdef XML_COMPILED
			act->body_parts->shield_tex
				= custom_path(def->shield[which_id].skin_name, playerpath, guildpath);
#else // XML_COMPILED
			my_strcp(act->body_parts->shield_tex, def->shield[which_id].skin_name);
#ifdef CUSTOM_LOOK
			custom_path(act->body_parts->shield_tex, playerpath, guildpath);
#endif // CUSTOM_LOOK
#endif // XML_COMPILED
#ifdef	NEW_TEXTURES
			if (delay_texture_item_change(act, which_part, which_id))
			{
				return;
			}
#endif	/* NEW_TEXTURES */
			model_attach_mesh(act, def->shield[which_id].mesh_index);
							act->body_parts->shield_meshindex=def->shield[which_id].mesh_index;
			act->cur_shield=which_id;
			act->body_parts->shield_meshindex = def->shield[which_id].mesh_index;
		}
		else if (which_part==KIND_OF_CAPE)
		{
#ifdef XML_COMPILED
			act->body_parts->cape_tex
				= custom_path(def->cape[which_id].skin_name, playerpath, guildpath);
#else // XML_COMPILED
			my_strcp(act->body_parts->cape_tex, def->cape[which_id].skin_name);
#ifdef CUSTOM_LOOK
			custom_path(act->body_parts->cape_tex, playerpath, guildpath);
#endif // CUSTOM_LOOK
#endif // XML_COMPILED
#ifdef	NEW_TEXTURES
			if (delay_texture_item_change(act, which_part, which_id))
			{
				return;
			}
#endif	/* NEW_TEXTURES */
			model_attach_mesh(act, def->cape[which_id].mesh_index);
			act->body_parts->cape_meshindex=def->cape[which_id].mesh_index;
		}
		else if (which_part==KIND_OF_HELMET)
		{
#ifdef XML_COMPILED
			act->body_parts->helmet_tex
				= custom_path(def->helmet[which_id].skin_name, playerpath, guildpath);
#else // XML_COMPILED
			my_strcp(act->body_parts->helmet_tex, def->helmet[which_id].skin_name);
#ifdef CUSTOM_LOOK
			custom_path(act->body_parts->helmet_tex, playerpath, guildpath);
#endif // CUSTOM_LOOK
#endif // XML_COMPILED
#ifdef	NEW_TEXTURES
			if (delay_texture_item_change(act, which_part, which_id))
			{
				return;
			}
#endif	/* NEW_TEXTURES */
			model_attach_mesh(act, def->helmet[which_id].mesh_index);
			act->body_parts->helmet_meshindex=def->helmet[which_id].mesh_index;
		}
		else if (which_part==KIND_OF_NECK)
		{
			assert(!"Using old client data" || def->neck != NULL);
#ifdef XML_COMPILED
			act->body_parts->neck_tex
				= custom_path(def->neck[which_id].skin_name, playerpath, guildpath);
#else // XML_COMPILED
			my_strcp(act->body_parts->neck_tex, def->neck[which_id].skin_name);
#ifdef CUSTOM_LOOK
			custom_path(act->body_parts->neck_tex, playerpath, guildpath);
#endif // CUSTOM_LOOK
#endif // XML_COMPILED
#ifdef	NEW_TEXTURES
			if (delay_texture_item_change(act, which_part, which_id))
			{
				return;
			}
#endif	/* NEW_TEXTURES */
			model_attach_mesh(act, def->neck[which_id].mesh_index);
			act->body_parts->neck_meshindex=def->neck[which_id].mesh_index;
		}
		else if (which_part==KIND_OF_BODY_ARMOR)
		{
#ifdef XML_COMPILED
			act->body_parts->arms_tex
				= custom_path(def->shirt[which_id].arms_name, playerpath, guildpath);
			act->body_parts->torso_tex
				= custom_path(def->shirt[which_id].torso_name, playerpath, guildpath);
			act->body_parts->arms_mask
				= custom_path(def->shirt[which_id].arms_mask, playerpath, guildpath);
			act->body_parts->torso_mask
				= custom_path(def->shirt[which_id].torso_mask, playerpath, guildpath);
#else // XML_COMPILED
			my_strcp(act->body_parts->arms_tex, def->shirt[which_id].arms_name);
			my_strcp(act->body_parts->torso_tex, def->shirt[which_id].torso_name);
			my_strcp(act->body_parts->arms_mask, def->shirt[which_id].arms_mask);
			my_strcp(act->body_parts->torso_mask, def->shirt[which_id].torso_mask);
#ifdef CUSTOM_LOOK
			custom_path(act->body_parts->arms_tex, playerpath, guildpath);
			custom_path(act->body_parts->torso_tex, playerpath, guildpath);
			custom_path(act->body_parts->arms_mask, playerpath, guildpath);
			custom_path(act->body_parts->torso_mask, playerpath, guildpath);
#endif // CUSTOM_LOOK
#endif // XML_COMPILED
#ifdef	NEW_TEXTURES
			if (delay_texture_item_change(act, which_part, which_id))
			{
				return;
			}
#endif	/* NEW_TEXTURES */
			if(def->shirt[which_id].mesh_index != act->body_parts->torso_meshindex)
			{
				model_detach_mesh(act, act->body_parts->torso_meshindex);
				model_attach_mesh(act, def->shirt[which_id].mesh_index);
				act->body_parts->torso_meshindex=def->shirt[which_id].mesh_index;
			}
		}
		else if (which_part==KIND_OF_LEG_ARMOR)
		{
#ifdef XML_COMPILED
			act->body_parts->pants_tex
				= custom_path(def->legs[which_id].skin_name, playerpath, guildpath);
			act->body_parts->pants_mask
				= custom_path(def->legs[which_id].skin_mask, playerpath, guildpath);
#else // XML_COMPILED
			strcpy(act->body_parts->pants_tex, def->legs[which_id].skin_name);
			strcpy(act->body_parts->pants_mask, def->legs[which_id].skin_mask);
#ifdef CUSTOM_LOOK
			custom_path(act->body_parts->pants_tex, playerpath, guildpath);
			custom_path(act->body_parts->pants_mask, playerpath, guildpath);
#endif // CUSTOM_LOOK
#endif // XML_COMPILED
#ifdef	NEW_TEXTURES
			if (delay_texture_item_change(act, which_part, which_id))
			{
				return;
			}
#endif	/* NEW_TEXTURES */
			if(def->legs[which_id].mesh_index != act->body_parts->legs_meshindex)
			{
				model_detach_mesh(act, act->body_parts->legs_meshindex);
				model_attach_mesh(act, def->legs[which_id].mesh_index);
				act->body_parts->legs_meshindex=def->legs[which_id].mesh_index;
			}
		}
		else if (which_part==KIND_OF_BOOT_ARMOR)
		{
#ifdef XML_COMPILED
			act->body_parts->boots_tex
				= custom_path(def->boots[which_id].skin_name, playerpath, guildpath);
			act->body_parts->boots_mask
				= custom_path(def->boots[which_id].skin_mask, playerpath, guildpath);
#else // XML_COMPILED
			my_strcp(act->body_parts->boots_tex, def->boots[which_id].skin_name);
			my_strcp(act->body_parts->boots_mask, def->boots[which_id].skin_mask);
#ifdef CUSTOM_LOOK
			custom_path(act->body_parts->boots_tex, playerpath, guildpath);
			custom_path(act->body_parts->boots_mask, playerpath, guildpath);
#endif // CUSTOM_LOOK
#endif // XML_COMPILED
#ifdef	NEW_TEXTURES
			if (delay_texture_item_change(act, which_part, which_id))
			{
				return;
			}
#endif	/* NEW_TEXTURES */
			if(def->boots[which_id].mesh_index != act->body_parts->boots_meshindex)
			{
				model_detach_mesh(act, act->body_parts->boots_meshindex);
				model_attach_mesh(act, def->boots[which_id].mesh_index);
				act->body_parts->boots_meshindex=def->boots[which_id].mesh_index;
			}
		}
		else
		{
			return;
		}

#ifndef	NEW_TEXTURES
		glDeleteTextures(1,&act->texture_id);
		act->texture_id = load_bmp8_enhanced_actor(act->body_parts, 255);
		act->has_alpha = act->body_parts->has_alpha;
#endif	/* NEW_TEXTURES */
		return;
	}
}

void add_enhanced_actor_from_server (const char *in_data, int len)
{
	short actor_id;
	Uint32 buffs;
	short x_pos;
	short y_pos;
	short z_rot;
	short max_health;
	short cur_health;
	Uint32 actor_type;
	Uint8 skin;
	Uint8 hair;
#ifdef NEW_EYES
	Uint8 eyes;
#endif
	Uint8 shirt;
	Uint8 pants;
	Uint8 boots;
	Uint8 frame;
	Uint8 cape;
	Uint8 head;
	Uint8 shield;
	Uint8 weapon;
	Uint8 helmet;
	Uint8 neck;
	int i;
	int dead=0;
	int kind_of_actor;
	enhanced_actor *this_actor;
#if defined(CUSTOM_LOOK) || defined(NEW_TEXTURES)
	char playerpath[256], guildpath[256];
	char onlyname[32]={0};
	Uint32 j;
#endif
	Uint32 uniq_id; // - Post ported.... We'll come up with something later...
	Uint32 guild_id;
	double f_x_pos,f_y_pos,f_z_rot;
	float   scale=1.0f;
	emote_data *pose=NULL;
	int attachment_type = -1;
	actor_types *def;

#ifdef EXTRA_DEBUG
	ERR();
#endif
	actor_id=SDL_SwapLE16(*((short *)(in_data)));
	buffs=(((SDL_SwapLE16(*((char*)(in_data+3)))>>3)&0x1F) | (SDL_SwapLE16(((*((char*)(in_data+5)))>>3)&0x1F)<<5));	// Strip the last 5 bits of the X and Y coords for the buffs
	x_pos=SDL_SwapLE16(*((short *)(in_data+2))) & 0x7FF;
	y_pos=SDL_SwapLE16(*((short *)(in_data+4))) & 0x7FF;
	buffs |= (SDL_SwapLE16(*((short *)(in_data+6))) & 0xFF80) << 3; // we get the 9 MSB for the buffs and leave the 7 LSB for a further use
	z_rot=SDL_SwapLE16(*((short *)(in_data+8)));
	actor_type=*(in_data+10);
	skin=*(in_data+12);
	hair=*(in_data+13);
	shirt=*(in_data+14);
	pants=*(in_data+15);
	boots=*(in_data+16);
	head=*(in_data+17);
	shield=*(in_data+18);
	weapon=*(in_data+19);
	cape=*(in_data+20);
	helmet=*(in_data+21);

#ifdef EXTRA_DEBUG
	ERR();
#endif

	if (actor_type >= nr_actor_defs
		|| (actor_type > 0 && actors_defs[actor_type].actor_type != actor_type) )
	{
		LOG_ERROR("Illegal/missing enhanced actor definition %d", actor_type);
		return;		// We cannot load an actor without a def (seg fault) so bail here.
	}
	def = actors_defs + actor_type;

	frame=*(in_data+22);
	max_health=SDL_SwapLE16(*((short *)(in_data+23)));
	cur_health=SDL_SwapLE16(*((short *)(in_data+25)));
	kind_of_actor=*(in_data+27);
#if defined CUSTOM_LOOK && defined UID
	//experimental code, not supported by the server
	uniq_id = SDL_SwapLE32(*((Uint32*)(in_data+28)));
	if(len > 32+(int)strlen(in_data+32)+2){
		scale=((float)SDL_SwapLE16(*((short *)(in_data+32+strlen(in_data+32)+1)))/((float)ACTOR_SCALE_BASE));
		if(len > 32+(int)strlen(in_data+32)+3)
			attachment_type = in_data[32+strlen(in_data+32)+3];
	}
#else
	if(len > 28+(int)strlen(in_data+28)+2){
		scale=((float)SDL_SwapLE16(*((short *)(in_data+28+strlen(in_data+28)+1)))/((float)ACTOR_SCALE_BASE));
		if(len > 28+(int)strlen(in_data+28)+3)
			attachment_type = (unsigned char)in_data[28+strlen(in_data+28)+3];
	}
#endif
	//the last bytes of the packet even if scale+attachment is not sent
#ifdef NEW_EYES
	if((*(in_data+len-2) > EYES_GOLD)|| (*(in_data+len-2) < 0))
	{
		eyes=0;
	} else {
		eyes=*(in_data+len-2);
	}
#endif
	neck=*(in_data+len-1);

	//translate from tile to world
	f_x_pos=x_pos*0.5;
	f_y_pos=y_pos*0.5;
	f_z_rot=z_rot;

#ifdef EXTRA_DEBUG
	ERR();
#endif
	//get the current frame
	switch(frame) {
	case frame_walk:
	case frame_run:
		break;
	case frame_die1:
		dead=1;
		break;
	case frame_die2:
		dead=1;
		break;
	case frame_pain1:
	case frame_pain2:
	case frame_drop:
	case frame_idle:
	case frame_sit_idle:
	case frame_harvest:
	case frame_cast:
	case frame_ranged:
	case frame_attack_up_1:
	case frame_attack_up_2:
	case frame_attack_up_3:
	case frame_attack_up_4:
	case frame_attack_up_5:
	case frame_attack_up_6:
	case frame_attack_up_7:
	case frame_attack_up_8:
	case frame_attack_up_9:
	case frame_attack_up_10:
	case frame_attack_down_1:
	case frame_attack_down_2:
	case frame_attack_down_3:
	case frame_attack_down_4:
	case frame_attack_down_5:
	case frame_attack_down_6:
	case frame_attack_down_7:
	case frame_attack_down_8:
	case frame_attack_down_9:
	case frame_attack_down_10:
	case frame_combat_idle:
		break;
	default:
		if(frame>=frame_poses_start&&frame<=frame_poses_end) {
			//we have a pose, get it! (frame is the emote_id)
			hash_entry *he;
			he=hash_get(emotes,(void*)(NULL+frame));
			if(!he) LOG_ERROR("unknown pose %d", frame);
			else pose = he->item;
			break;
		}
#ifdef UID
		LOG_ERROR("%s %d - %s\n", unknown_frame, frame, &in_data[32]);
#else
		LOG_ERROR("%s %d - %s\n", unknown_frame, frame, &in_data[28]);
#endif
	}

#ifdef EXTRA_DEBUG
	ERR();
#endif


	//find out if there is another actor with that ID
	//ideally this shouldn't happen, but just in case
	for(i=0;i<max_actors;i++)
		{
			if(actors_list[i])
				{
					if(actors_list[i]->actor_id==actor_id)
						{
#ifdef UID
							LOG_ERROR("%s %d = %s => %s\n", duplicate_actors_str, actor_id, actors_list[i]->actor_name, &in_data[32]);
#else
							LOG_ERROR("%s %d = %s => %s\n",duplicate_actors_str,actor_id, actors_list[i]->actor_name ,&in_data[28]);
#endif
							destroy_actor(actors_list[i]->actor_id);//we don't want two actors with the same ID
							i--;// last actor was put here, he needs to be checked too
						}
#ifdef UID
					else if(kind_of_actor==COMPUTER_CONTROLLED_HUMAN && (actors_list[i]->kind_of_actor==COMPUTER_CONTROLLED_HUMAN || actors_list[i]->kind_of_actor==PKABLE_COMPUTER_CONTROLLED) && !my_strcompare(&in_data[32], actors_list[i]->actor_name))
#else
					else if(kind_of_actor==COMPUTER_CONTROLLED_HUMAN && (actors_list[i]->kind_of_actor==COMPUTER_CONTROLLED_HUMAN || actors_list[i]->kind_of_actor==PKABLE_COMPUTER_CONTROLLED) && !my_strcompare(&in_data[28], actors_list[i]->actor_name))
#endif
						{
#ifdef UID
							LOG_ERROR("%s(%d) = %s => %s\n", duplicate_npc_actor, actor_id, actors_list[i]->actor_name, &in_data[32]);
#else
							LOG_ERROR("%s(%d) = %s => %s\n",duplicate_npc_actor,actor_id, actors_list[i]->actor_name ,&in_data[28]);
#endif
							destroy_actor(actors_list[i]->actor_id);//we don't want two actors with the same ID
							i--;// last actor was put here, he needs to be checked too
						}
				}
		}

#ifdef EXTRA_DEBUG
	ERR();
#endif

	this_actor=calloc(1,sizeof(enhanced_actor));

	/* build a clean player name and a guild id */
	{
		/* get the name string into a working buffer */
		char buffer[256], *name, *guild;
#ifdef UID
		my_strncp(buffer,&in_data[32],sizeof(buffer));
#else
		my_strncp(buffer,&in_data[28],sizeof(buffer));
		uniq_id = 0;
#endif

#if defined(CUSTOM_LOOK) || defined(NEW_TEXTURES)
		/* skip leading color codes */
		for (name=buffer; *name && is_color (*name); name++) /* nothing */ ;
		/* trim off any guild tag, leaving solely the name (onlyname)*/
		for(j=0; name[j] && name[j]>32;j++){
			onlyname[j]=name[j];
		}
#else
		name = buffer;
#endif //CUSTOM_LOOK

		/* search for string end or color mark */
		this_actor->guild_tag_color = 0;
		for (guild = name; *guild && is_printable (*guild); guild++);
		if (*guild) {
			/* separate the two strings */
			this_actor->guild_tag_color = from_color_char (*guild);
			*guild = 0;
			guild++;
		}

		/* perform case insensitive comparison/hashing */
		my_tolower(name);
#if defined(CUSTOM_LOOK) || defined(NEW_TEXTURES)
		my_tolower(onlyname);
#endif //CUSTOM_LOOK

		//perfect hashing of guildtag
 		switch(strlen(guild))
 		{
		case 0:
			guild_id = 0;
			break;
		case 1:
			guild_id = guild[0];
			break;
 		case 2:
 			guild_id = guild[0] + (guild[1] << 8);
 			break;
 		case 3:
 			guild_id = guild[0] + (guild[1] << 8) + (guild[2] << 16);
 			break;
 		default:
 			guild_id = guild[0] + (guild[1] << 8) + (guild[2] << 16) + (guild[3] << 24);
 			break;
		}
	}

#ifdef  CUSTOM_LOOK
	/* precompute the paths to custom files */
	safe_snprintf(playerpath, sizeof(playerpath), "custom/player/%s/", onlyname);
	safe_snprintf(guildpath, sizeof(guildpath), "custom/guild/%u/", guild_id);
#endif  //CUSTOM_LOOK

	/* store the ids */
	this_actor->uniq_id = uniq_id;
	this_actor->guild_id = guild_id;

	//get the torso
#ifdef XML_COMPILED
	this_actor->arms_tex = custom_path(def->shirt[shirt].arms_name, playerpath, guildpath);
	this_actor->torso_tex = custom_path(def->shirt[shirt].torso_name, playerpath, guildpath);
	this_actor->arms_mask = custom_path(def->shirt[shirt].arms_mask, playerpath, guildpath);
	this_actor->torso_mask = custom_path(def->shirt[shirt].torso_mask, playerpath, guildpath);
	//skin
	this_actor->hands_tex = custom_path(def->skin[skin].hands_name, playerpath, guildpath);
	this_actor->hands_tex_save = custom_path(def->skin[skin].hands_name, playerpath, guildpath);
	this_actor->hands_mask = "";
	this_actor->head_tex = custom_path(def->skin[skin].head_name, playerpath, guildpath);
	this_actor->head_base = custom_path(def->skin[skin].head_name, playerpath, guildpath);
	if (*def->head[head].skin_name)
		this_actor->head_tex = custom_path(def->head[head].skin_name, playerpath, guildpath);
	this_actor->head_mask = custom_path(def->head[head].skin_mask, playerpath, guildpath);
	this_actor->body_base = custom_path(def->skin[skin].body_name, playerpath, guildpath);
	this_actor->arms_base = custom_path(def->skin[skin].arms_name, playerpath, guildpath);
	this_actor->legs_base = custom_path(def->skin[skin].legs_name, playerpath, guildpath);
	this_actor->boots_base = custom_path(def->skin[skin].feet_name, playerpath, guildpath);
	//hair
	this_actor->hair_tex = custom_path(def->hair[hair].hair_name, playerpath, guildpath);
#ifdef NEW_EYES
	//eyes
	if (def->eyes && def->eyes[eyes].eyes_name)
	{
		this_actor->eyes_tex = custom_path(def->eyes[eyes].eyes_name, playerpath, guildpath);
	}
	else
	{
		static int already_said = 0;
		if (!already_said)
		{
			const char *message = "Looks like we compiled with NEW_EYES but do not have the textures";
			LOG_ERROR(message);
			LOG_TO_CONSOLE(c_red2,message);
			already_said = 1;
		}
	}
#endif // NEW_EYES
	//boots
	this_actor->boots_tex = custom_path(def->boots[boots].skin_name, playerpath, guildpath);
	this_actor->boots_mask = custom_path(def->boots[boots].skin_mask, playerpath, guildpath);
	//legs
	this_actor->pants_tex = custom_path(def->legs[pants].skin_name, playerpath, guildpath);
	this_actor->pants_mask = custom_path(def->legs[pants].skin_mask, playerpath, guildpath);
#else // XML_COMPILED
	my_strncp(this_actor->arms_tex,def->shirt[shirt].arms_name,sizeof(this_actor->arms_tex));
	my_strncp(this_actor->torso_tex,def->shirt[shirt].torso_name,sizeof(this_actor->torso_tex));
	my_strncp(this_actor->arms_mask,def->shirt[shirt].arms_mask,sizeof(this_actor->arms_mask));
	my_strncp(this_actor->torso_mask,def->shirt[shirt].torso_mask,sizeof(this_actor->torso_mask));
	//skin
	my_strncp(this_actor->hands_tex,def->skin[skin].hands_name,sizeof(this_actor->hands_tex));
	my_strncp(this_actor->hands_tex_save,def->skin[skin].hands_name,sizeof(this_actor->hands_tex_save));
	my_strncp(this_actor->hands_mask,"",sizeof(this_actor->hands_mask));	// by default, nothing
	my_strncp(this_actor->head_tex,def->skin[skin].head_name,sizeof(this_actor->head_tex));
	my_strncp(this_actor->head_base,def->skin[skin].head_name,sizeof(this_actor->head_base));
	my_strncp(this_actor->head_mask,"",sizeof(this_actor->head_mask));	// by default, nothing
	if(*def->head[head].skin_name)
		my_strncp(this_actor->head_tex,def->head[head].skin_name,sizeof(this_actor->head_tex));
	if(*def->head[head].skin_mask)
		my_strncp(this_actor->head_mask,def->head[head].skin_mask,sizeof(this_actor->head_mask));
	my_strncp(this_actor->body_base,def->skin[skin].body_name,sizeof(this_actor->body_base));
	my_strncp(this_actor->arms_base,def->skin[skin].arms_name,sizeof(this_actor->arms_base));
	my_strncp(this_actor->legs_base,def->skin[skin].legs_name,sizeof(this_actor->legs_base));
	my_strncp(this_actor->boots_base,def->skin[skin].feet_name,sizeof(this_actor->boots_base));
	//hair
	my_strncp(this_actor->hair_tex,def->hair[hair].hair_name,sizeof(this_actor->hair_tex));
#ifdef NEW_EYES
	//eyes
	if (def->eyes && def->eyes[eyes].eyes_name)
	{
		my_strncp(this_actor->eyes_tex,def->eyes[eyes].eyes_name,sizeof(this_actor->eyes_tex));
	}
	else
	{
		static int already_said = 0;
		if (!already_said)
		{
			char *message = "Looks like we compiled with NEW_EYES but do not have the textures";
			LOG_ERROR(message);
			LOG_TO_CONSOLE(c_red2,message);
			already_said = 1;
		}
	}
#endif // NEW_EYES
	//boots
	safe_strncpy(this_actor->boots_tex, def->boots[boots].skin_name, sizeof(this_actor->boots_tex));
	safe_strncpy(this_actor->boots_mask, def->boots[boots].skin_mask, sizeof(this_actor->boots_mask));
	//legs
	safe_strncpy(this_actor->pants_tex, def->legs[pants].skin_name, sizeof(this_actor->pants_tex));
	safe_strncpy(this_actor->pants_mask, def->legs[pants].skin_mask, sizeof(this_actor->pants_mask));
#endif // XML_COMPILED

#ifdef CUSTOM_LOOK
	if(kind_of_actor != NPC)
	{
		//torso
		custom_path(this_actor->arms_tex, playerpath, guildpath);
		custom_path(this_actor->torso_tex, playerpath, guildpath);
		custom_path(this_actor->arms_mask, playerpath, guildpath);
		custom_path(this_actor->torso_mask, playerpath, guildpath);
		//skin
		custom_path(this_actor->hands_tex, playerpath, guildpath);
		custom_path(this_actor->hands_tex_save, playerpath, guildpath);
		custom_path(this_actor->head_tex, playerpath, guildpath);
		custom_path(this_actor->body_base, playerpath, guildpath);
		custom_path(this_actor->arms_base, playerpath, guildpath);
		custom_path(this_actor->legs_base, playerpath, guildpath);
		//hair
		custom_path(this_actor->hair_tex, playerpath, guildpath);
#ifdef NEW_EYES
		//eyes
		custom_path(this_actor->eyes_tex, playerpath, guildpath);
#endif
		//boots
		custom_path(this_actor->boots_tex, playerpath, guildpath);
		custom_path(this_actor->boots_mask, playerpath, guildpath);
		//legs
		custom_path(this_actor->pants_tex, playerpath, guildpath);
		custom_path(this_actor->pants_mask, playerpath, guildpath);
	}
#endif //CUSTOM_LOOK

	//cape
	if (cape != CAPE_NONE)
	{
#ifdef XML_COMPILED
		this_actor->cape_tex = custom_path(def->cape[cape].skin_name, playerpath, guildpath);
#else // XML_COMPILED
		my_strncp(this_actor->cape_tex, def->cape[cape].skin_name, sizeof(this_actor->cape_tex));
#ifdef CUSTOM_LOOK
		if(kind_of_actor != NPC)
			custom_path(this_actor->cape_tex, playerpath, guildpath);
#endif //CUSTOM_LOOK
#endif // XML_COMPILED
	}
	else
	{
#ifdef XML_COMPILED
		this_actor->cape_tex = "";
#else // XML_COMPILED
		my_strncp(this_actor->cape_tex,"",sizeof(this_actor->cape_tex));
#endif // XML_COMPILED
	}
	//shield
	if (shield != SHIELD_NONE)
	{
#ifdef XML_COMPILED
		this_actor->shield_tex = custom_path(def->shield[shield].skin_name, playerpath, guildpath);
#else // XML_COMPILED
		my_strncp(this_actor->shield_tex, def->shield[shield].skin_name, sizeof(this_actor->shield_tex));
#ifdef CUSTOM_LOOK
		if(kind_of_actor != NPC)
			custom_path(this_actor->shield_tex, playerpath, guildpath);
#endif //CUSTOM_LOOK
#endif // XML_COMPILED
	}
	else
	{
#ifdef XML_COMPILED
		this_actor->shield_tex = "";
#else
		my_strncp(this_actor->shield_tex,"",sizeof(this_actor->shield_tex));
#endif
	}

	if (weapon == GLOVE_FUR || weapon == GLOVE_LEATHER)
	{
#ifdef XML_COMPILED
		this_actor->hands_tex = custom_path(def->weapon[weapon].skin_name, playerpath, guildpath);
#else // XML_COMPILED
		my_strncp(this_actor->hands_tex, def->weapon[weapon].skin_name, sizeof(this_actor->hands_tex));
#ifdef CUSTOM_LOOK
		if(kind_of_actor != NPC)
			custom_path(this_actor->hands_tex, playerpath, guildpath);
#endif //CUSTOM_LOOK
#endif // XML_COMPILED
	}
	else
	{
#ifdef XML_COMPILED
		this_actor->weapon_tex = custom_path(def->weapon[weapon].skin_name, playerpath, guildpath);
#else // XML_COMPILED
		my_strncp(this_actor->weapon_tex, def->weapon[weapon].skin_name, sizeof(this_actor->weapon_tex));
#ifdef CUSTOM_LOOK
		if(kind_of_actor != NPC)
			custom_path(this_actor->weapon_tex, playerpath, guildpath);
#endif //CUSTOM_LOOK
#endif // XML_COMPILED
	}

	this_actor->weapon_glow=def->weapon[weapon].glow;

	//helmet
	if (helmet != HELMET_NONE)
	{
#ifdef XML_COMPILED
		this_actor->helmet_tex = custom_path(def->helmet[helmet].skin_name, playerpath, guildpath);
#else // XML_COMPILED
		my_strncp(this_actor->helmet_tex, def->helmet[helmet].skin_name, sizeof(this_actor->helmet_tex));
#ifdef CUSTOM_LOOK
		if(kind_of_actor != NPC)
			custom_path(this_actor->helmet_tex, playerpath, guildpath);
#endif //CUSTOM_LOOK
#endif // XML_COMPILED
	}
	else
	{
#ifdef XML_COMPILED
		this_actor->helmet_tex = "";
#else // XML_COMPILED
		my_strncp(this_actor->helmet_tex,"",sizeof(this_actor->helmet_tex));
#endif // XML_COMPILED
	}

	//neck
	if (neck != NECK_NONE)
	{
		assert(!"Using old client data" || def->neck != NULL);
#ifdef XML_COMPILED
		this_actor->neck_tex = custom_path(def->neck[neck].skin_name, playerpath, guildpath);
#else // XML_COMPILED
		my_strncp(this_actor->neck_tex, def->neck[neck].skin_name, sizeof(this_actor->neck_tex));
#ifdef CUSTOM_LOOK
		if(kind_of_actor != NPC)
			custom_path(this_actor->neck_tex, playerpath, guildpath);
#endif //CUSTOM_LOOK
#endif // XML_COMPILED
	}
	else
	{
#ifdef XML_COMPILED
		this_actor->neck_tex = "";
#else // XML_COMPILED
		my_strncp(this_actor->neck_tex,"",sizeof(this_actor->neck_tex));
#endif // XML_COMPILED
	}

#ifdef	NEW_TEXTURES
	i=add_enhanced_actor(this_actor,f_x_pos,f_y_pos,0.0,f_z_rot,scale,actor_id, onlyname);
#else	/* NEW_TEXTURES */
	i=add_enhanced_actor(this_actor,f_x_pos,f_y_pos,0.0,f_z_rot,scale,actor_id);
#endif	/* NEW_TEXTURES */
#ifdef EXTRA_DEBUG
	ERR();
#endif //EXTRA_DEBUG
	//The actors list is already locked here

	actors_list[i]->async_fighting = 0;
	actors_list[i]->async_x_tile_pos = x_pos;
	actors_list[i]->async_y_tile_pos = y_pos;
	actors_list[i]->async_z_rot = z_rot;

	actors_list[i]->x_tile_pos=x_pos;
	actors_list[i]->y_tile_pos=y_pos;
	actors_list[i]->buffs=buffs;
	actors_list[i]->actor_type=actor_type;
	actors_list[i]->damage=0;
	actors_list[i]->damage_ms=0;
	actors_list[i]->sitting=0;
	actors_list[i]->fighting=0;
	//test only
	actors_list[i]->max_health=max_health;
	actors_list[i]->cur_health=cur_health;

    actors_list[i]->step_duration = def->step_duration;
	if (actors_list[i]->buffs & BUFF_DOUBLE_SPEED)
		actors_list[i]->step_duration /= 2;

    actors_list[i]->z_pos = get_actor_z(actors_list[i]);
	if(frame==frame_sit_idle||(pose!=NULL&&pose->pose==EMOTE_SITTING)){ //sitting pose sent by the server
			actors_list[i]->poses[EMOTE_SITTING]=pose;
			if(actors_list[i]->actor_id==yourself)
				you_sit=1;
			actors_list[i]->sitting=1;
		}
	else if(frame==frame_stand||(pose!=NULL&&pose->pose==EMOTE_STANDING)){//standing pose sent by server
			actors_list[i]->poses[EMOTE_STANDING]=pose;
			if(actors_list[i]->actor_id==yourself)
				you_sit=0;
		}
	else if(frame==frame_walk||(pose!=NULL&&pose->pose==EMOTE_WALKING)){//walking pose sent by server
			actors_list[i]->poses[EMOTE_WALKING]=pose;
			if(actors_list[i]->actor_id==yourself)
				you_sit=0;
		}
	else if(frame==frame_run||(pose!=NULL&&pose->pose==EMOTE_RUNNING)){//running pose sent by server
			actors_list[i]->poses[EMOTE_RUNNING]=pose;
			if(actors_list[i]->actor_id==yourself)
				you_sit=0;
		}
	else
		{
			if(actors_list[i]->actor_id==yourself)
				you_sit=0;
			if(frame==frame_combat_idle)
				actors_list[i]->fighting=1;
			else if (frame == frame_ranged)
				actors_list[i]->in_aim_mode = 1;
		}
	//ghost or not?
	actors_list[i]->ghost=def->ghost;

	actors_list[i]->dead=dead;
	actors_list[i]->stop_animation=1;//helps when the actor is dead...
	actors_list[i]->cur_weapon=weapon;
	actors_list[i]->cur_shield=shield;
	actors_list[i]->kind_of_actor=kind_of_actor;
	if(strlen(&in_data[28]) >= 30)
	{
		LOG_ERROR("%s (%d): %s/%d\n", bad_actor_name_length, actors_list[i]->actor_type,&in_data[28], (int)strlen(&in_data[28]));
	}
	else
	{
		/* Extract the name for use in the tab completion list. */
		const unsigned char *name = (unsigned char*) in_data+28;
		char *ptr;
		char buffer[32];

		if(kind_of_actor != NPC) {
			/* Skip leading color codes */
			for (; *name && is_color (*name); name++) /* nothing */;
			safe_snprintf(buffer, sizeof(buffer), "%.30s", name);
			/* Remove guild tag, etc. */
			for(ptr = buffer; *ptr && *ptr > 0 && !isspace(*ptr); ptr++);
			*ptr = '\0';
			add_name_to_tablist(buffer);
		}
		my_strncp(actors_list[i]->actor_name,&in_data[28],sizeof(actors_list[i]->actor_name));
		if(caps_filter && my_isupper(actors_list[i]->actor_name, -1)) {
			my_tolower(actors_list[i]->actor_name);
		}
	}


	if (attachment_type >= 0 &&attachment_type < 255) //255 is not necessary, but it suppresses a warning in errorlog
		add_actor_attachment(actor_id, attachment_type);

	if (def->coremodel!=NULL) {
		actors_list[i]->calmodel=model_new(def->coremodel);

		if (actors_list[i]->calmodel!=NULL) {
			//Setup cal3d model
			//actors_list[i]->calmodel=CalModel_New(def->coremodel);
			//Attach meshes
			model_attach_mesh(actors_list[i], def->head[head].mesh_index);
			model_attach_mesh(actors_list[i], def->shirt[shirt].mesh_index);
			model_attach_mesh(actors_list[i], def->legs[pants].mesh_index);
			model_attach_mesh(actors_list[i], def->boots[boots].mesh_index);
			actors_list[i]->body_parts->torso_meshindex=def->shirt[shirt].mesh_index;
			actors_list[i]->body_parts->legs_meshindex=def->legs[pants].mesh_index;
			actors_list[i]->body_parts->head_meshindex=def->head[head].mesh_index;
			actors_list[i]->body_parts->boots_meshindex=def->boots[boots].mesh_index;

			if (cape!=CAPE_NONE) model_attach_mesh(actors_list[i], def->cape[cape].mesh_index);
			if (helmet!=HELMET_NONE) model_attach_mesh(actors_list[i], def->helmet[helmet].mesh_index);
			if (weapon!=WEAPON_NONE) model_attach_mesh(actors_list[i], def->weapon[weapon].mesh_index);
			if (shield!=SHIELD_NONE) model_attach_mesh(actors_list[i], def->shield[shield].mesh_index);
			assert(!"Using old client data" || def->neck != NULL);
			if (neck!=NECK_NONE) model_attach_mesh(actors_list[i], def->neck[neck].mesh_index);
			actors_list[i]->body_parts->neck_meshindex=def->neck[neck].mesh_index;
			actors_list[i]->body_parts->helmet_meshindex=def->helmet[helmet].mesh_index;
			actors_list[i]->body_parts->cape_meshindex=def->cape[cape].mesh_index;
			actors_list[i]->body_parts->shield_meshindex=def->shield[shield].mesh_index;

			actors_list[i]->cur_anim.anim_index=-1;
#ifdef NEW_SOUND
			stop_sound(actors_list[i]->cur_anim_sound_cookie);
#endif // NEW_SOUND
			actors_list[i]->cur_anim_sound_cookie= 0;
			actors_list[i]->anim_time= 0.0;
			actors_list[i]->last_anim_update= cur_time;

			if(dead){
				cal_actor_set_anim(i, actors_defs[actors_list[i]->actor_type].cal_frames[cal_actor_die1_frame]);
				actors_list[i]->stop_animation=1;
				CalModel_Update(actors_list[i]->calmodel,1000);
			}
            else {
                /* Schmurk: we explicitly go on idle here to avoid weird
                 * flickering when actors appear */
                set_on_idle(i);
                /* CalModel_Update(actors_list[i]->calmodel,0); */
            }
			build_actor_bounding_box(actors_list[i]);
			if (use_animation_program)
			{
				set_transformation_buffers(actors_list[i]);
			}
			switch (weapon)
			{
				case SWORD_1_FIRE:
				case SWORD_2_FIRE:
				case SWORD_3_FIRE:
				case SWORD_4_FIRE:
				case SWORD_4_THERMAL:
				case SWORD_5_FIRE:
				case SWORD_5_THERMAL:
				case SWORD_6_FIRE:
				case SWORD_6_THERMAL:
				case SWORD_7_FIRE:
				case SWORD_7_THERMAL:
					ec_create_sword_of_fire(actors_list[i], (poor_man ? 6 : 10));
					break;
				case SWORD_2_COLD:
				case SWORD_3_COLD:
				case SWORD_4_COLD:
				case SWORD_5_COLD:
				case SWORD_6_COLD:
				case SWORD_7_COLD:
					ec_create_sword_of_ice(actors_list[i], (poor_man ? 6 : 10));
					break;
				case SWORD_3_MAGIC:
				case SWORD_4_MAGIC:
				case SWORD_5_MAGIC:
				case SWORD_6_MAGIC:
				case SWORD_7_MAGIC:
					ec_create_sword_of_magic(actors_list[i], (poor_man ? 6 : 10));
					break;
				case SWORD_EMERALD_CLAYMORE:
					ec_create_sword_emerald_claymore(actors_list[i], (poor_man ? 6 : 10));
					break;
				case SWORD_CUTLASS:
					ec_create_sword_cutlass(actors_list[i], (poor_man ? 6 : 10));
					break;
				case SWORD_SUNBREAKER:
					ec_create_sword_sunbreaker(actors_list[i], (poor_man ? 6 : 10));
					break;
				case SWORD_ORC_SLAYER:
					ec_create_sword_orc_slayer(actors_list[i], (poor_man ? 6 : 10));
					break;
				case SWORD_EAGLE_WING:
					ec_create_sword_eagle_wing(actors_list[i], (poor_man ? 6 : 10));
					break;
				case SWORD_JAGGED_SABER:
					ec_create_sword_jagged_saber(actors_list[i], (poor_man ? 6 : 10));
					break;
				case STAFF_3: // staff of protection
					ec_create_staff_of_protection(actors_list[i], (poor_man ? 6 : 10));
					break;
				case STAFF_4: // staff of the mage
					ec_create_staff_of_the_mage(actors_list[i], (poor_man ? 6 : 10));
					break;
			}
		}
	} else actors_list[i]->calmodel=NULL;

	/* //DEBUG
	if (actors_list[i]->actor_id==yourself) {
		//actor_wear_item(actors_list[i]->actor_id,KIND_OF_WEAPON,SWORD_1);
		//unwear_item_from_actor(actors_list[i]->actor_id,KIND_OF_WEAPON);
		//actor_wear_item(actors_list[i]->actor_id,KIND_OF_WEAPON,SWORD_2);
		//actor_wear_item(actors_list[i]->actor_id,KIND_OF_HELMET,HELMET_IRON);
	}*/

	if(frame==frame_combat_idle) {
		//if fighting turn the horse and the fighter
			actors_list[i]->fighting=1;
			if(actors_list[i]->attached_actor>=0) {
				//printf("A fighting horse! (%s)\n", actors_list[i]->actor_name);
				MY_HORSE(i)->fighting=1;
				if(ACTOR_WEAPON(i)->turn_horse) rotate_actor_and_horse(i,-1);
			}
	}


    if (actor_id == yourself) {
        reset_camera_at_next_update = 1;
    }
	update_actor_buffs(actor_id, buffs);
	UNLOCK_ACTORS_LISTS();  //unlock it
#ifdef EXTRA_DEBUG
	ERR();
#endif
}

actor *add_actor_interface(float x, float y, float z_rot, float scale,
	int actor_type, short skin, short hair, short eyes, short shirt,
	short pants, short boots, short head)
{
	const actor_types *def = actors_defs + actor_type;
	enhanced_actor *this_actor = calloc(1, sizeof(enhanced_actor));
	actor *a;

	//get the torso
#ifdef XML_COMPILED
	this_actor->pants_tex = def->legs[pants].skin_name;
	this_actor->pants_mask = def->legs[pants].skin_mask;
	this_actor->boots_tex = def->boots[boots].skin_name;
	this_actor->boots_mask = def->boots[boots].skin_mask;
	this_actor->torso_tex = def->shirt[shirt].torso_name;
	this_actor->arms_tex = def->shirt[shirt].arms_name;
	this_actor->torso_mask = def->shirt[shirt].torso_mask;
	this_actor->arms_mask = def->shirt[shirt].arms_mask;
	this_actor->hands_tex = def->skin[skin].hands_name;
	this_actor->head_tex = def->skin[skin].head_name;
	this_actor->hands_mask = "";
	this_actor->head_mask = "";
	this_actor->head_base = "";
	this_actor->body_base = "";
	this_actor->arms_base = "";
	this_actor->legs_base = "";
	this_actor->boots_base = "";
	this_actor->hair_tex = def->hair[hair].hair_name;
#ifdef NEW_EYES
	this_actor->eyes_tex = def->eyes[eyes].eyes_name;
#endif
	this_actor->weapon_tex = "";
	this_actor->shield_tex = "";
	this_actor->helmet_tex = "";
	this_actor->neck_tex = "";
	this_actor->cape_tex = "";
	this_actor->hands_tex_save = "";
#else // XML_COMPILED
	safe_strncpy(this_actor->arms_tex, def->shirt[shirt].arms_name, sizeof(this_actor->arms_tex));
	safe_strncpy(this_actor->arms_mask, def->shirt[shirt].arms_mask, sizeof(this_actor->arms_mask));
	safe_strncpy(this_actor->torso_tex, def->shirt[shirt].torso_name, sizeof(this_actor->torso_tex));
	safe_strncpy(this_actor->torso_mask, def->shirt[shirt].torso_mask, sizeof(this_actor->torso_mask));
	safe_strncpy(this_actor->hands_tex, def->skin[skin].hands_name, sizeof(this_actor->hands_tex));
	safe_strncpy(this_actor->head_tex, def->skin[skin].head_name, sizeof(this_actor->head_tex));
	safe_strncpy(this_actor->hair_tex, def->hair[hair].hair_name, sizeof(this_actor->hair_tex));
#ifdef NEW_EYES
	safe_strncpy(this_actor->eyes_tex, def->eyes[eyes].eyes_name, sizeof(this_actor->eyes_tex));
#endif
	safe_strncpy(this_actor->boots_tex, def->boots[boots].skin_name, sizeof(this_actor->boots_tex));
	safe_strncpy(this_actor->boots_mask, def->boots[boots].skin_mask, sizeof(this_actor->boots_mask));
	safe_strncpy(this_actor->pants_tex, def->legs[pants].skin_name, sizeof(this_actor->pants_tex));
	safe_strncpy(this_actor->pants_mask, def->legs[pants].skin_mask, sizeof(this_actor->pants_mask));
#endif // XML_COMPILED

#ifdef	NEW_TEXTURES
	a=actors_list[add_enhanced_actor(this_actor, x*0.5f, y*0.5f, 0.00000001f, z_rot, scale, 0, 0)];
#else	/* NEW_TEXTURES */
	a=actors_list[add_enhanced_actor(this_actor, x*0.5f, y*0.5f, 0.00000001f, z_rot, scale, 0)];
#endif	/* NEW_TEXTURES */

	a->x_tile_pos=x;
	a->y_tile_pos=y;
	a->actor_type=actor_type;
	//test only
	a->max_health=20;
	a->cur_health=20;

	a->stop_animation=1;//helps when the actor is dead...
	a->kind_of_actor=HUMAN;

	safe_strncpy(a->actor_name, "Player", sizeof(a->actor_name));

	if (def->coremodel)
	{
		a->calmodel = model_new(def->coremodel);

		if (a->calmodel)
		{
			//Setup cal3d model
			//a->calmodel=CalModel_New(def->coremodel);
			//Attach meshes
			model_attach_mesh(a, def->head[head].mesh_index);
			model_attach_mesh(a, def->shirt[shirt].mesh_index);
			model_attach_mesh(a, def->legs[pants].mesh_index);
			model_attach_mesh(a, def->boots[boots].mesh_index);

			a->body_parts->torso_meshindex = def->shirt[shirt].mesh_index;
			a->body_parts->legs_meshindex = def->legs[pants].mesh_index;
			a->body_parts->head_meshindex = def->head[head].mesh_index;
			a->body_parts->boots_meshindex = def->boots[boots].mesh_index;

			assert(!"Using old client data" || def->neck != NULL);
			a->body_parts->neck_meshindex = def->neck[NECK_NONE].mesh_index;
			a->body_parts->helmet_meshindex = def->helmet[HELMET_NONE].mesh_index;
			a->body_parts->cape_meshindex = def->cape[CAPE_NONE].mesh_index;
			a->body_parts->shield_meshindex = def->shield[SHIELD_NONE].mesh_index;

			a->cur_anim.anim_index = -1;
			a->anim_time = 0.0;
			a->last_anim_update = cur_time;
			CalModel_Update(a->calmodel, 0);
			build_actor_bounding_box(a);
			if (use_animation_program)
			{
				set_transformation_buffers(a);
			}
		}
	}
	else
	{
		a->calmodel = NULL;
	}

	UNLOCK_ACTORS_LISTS();  //unlock it

	return a;
}
