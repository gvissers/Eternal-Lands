#include "actor_def_types.h"
#include "actor_init.h"
#include "cal.h"
#include "client_serv.h"
#include "elloggingwrapper.h"
#include "io/cal3d_io_wrapper.h"
#include "skeletons.h"
#include "sound.h"

typedef struct
{
    const char *fname;
    int act_idx;
    int group_idx;
    int anim_idx;
} idle_group_reg;

typedef struct
{
    const char *fname;
#ifdef NEW_SOUND
    const char *sound;
    const char *sound_scale;
#endif
    int duration;
} frames_reg;

typedef struct
{
    const char *fname;
#ifdef NEW_SOUND
    const char *sound;
    const char *sound_scale;
#endif
    int duration;
    int act_idx;
    int frame_idx;
} emote_reg;

typedef struct
{
    const char* name;
    int act_idx;
    int att_act_idx;
} bone_reg;

typedef struct
{
    const char* fname;
#ifdef NEW_SOUND
    const char* sound;
    const char* sound_scale;
#endif
    int duration;
    int act_idx;
    int held_act_idx;
    int frame_idx;
} attached_frames_reg;

#ifdef NEW_SOUND
typedef struct
{
	const char* sound;
	const char* sound_scale;
	int act_idx;
	int frame_idx;
} sound_reg;
#endif

#include "actor_defs_inc.c"

static void actor_check_int(actor_types *act, const char *section, const char *type, int value)
{
    if (value < 0)
    {
#ifdef DEBUG
        LOG_ERROR("Data Error in %s(%d): Missing %s.%s", act->actor_name, act->actor_type, section, type);
#endif // DEBUG
    }
}

static int cal_search_mesh(actor_types *act, const char *fn, const char *kind)
{
    int i;

    if (act->head && !strcmp(kind, "head"))
    {
        for (i = 0; i < 10; i++)
        {
            if (!strcmp(fn, act->head[i].model_name) && act->head[i].mesh_index != -1)
                return act->head[i].mesh_index;
        }
    }
    else if (act->shirt && !strcmp (kind, "shirt"))
    {
        for (i = 0; i < 100; i++)
        {
            if (!strcmp(fn, act->shirt[i].model_name) && act->shirt[i].mesh_index != -1)
                return act->shirt[i].mesh_index;
        }
    }
    else if (act->legs && !strcmp(kind, "legs"))
    {
        for (i = 0; i < 60; i++)
        {
            if (!strcmp(fn, act->legs[i].model_name) && act->legs[i].mesh_index != -1)
                return act->legs[i].mesh_index;
        }
    }
    else if (act->boots && !strcmp(kind, "boots"))
    {
        for (i = 0; i < 40; i++)
        {
            if (!strcmp(fn, act->boots[i].model_name) && act->boots[i].mesh_index != -1)
                return act->boots[i].mesh_index;
        }
    }
    else if (act->cape && !strcmp(kind, "cape"))
    {
        for (i = 0; i < 50; i++)
        {
            if (!strcmp(fn, act->cape[i].model_name) && act->cape[i].mesh_index != -1)
                return act->cape[i].mesh_index;
        }
    }
    else if (act->helmet && !strcmp(kind, "helmet"))
    {
        for (i = 0; i < 100; i++)
        {
            if (!strcmp(fn, act->helmet[i].model_name) && act->helmet[i].mesh_index != -1)
                return act->helmet[i].mesh_index;
        }
    }
    else if (act->neck && !strcmp(kind, "neck"))
    {
        for (i = 0; i < 20; i++)
        {
            if (!strcmp(fn, act->neck[i].model_name) && act->neck[i].mesh_index != -1)
                return act->neck[i].mesh_index;
        }
    }
    else if (act->shield && !strcmp(kind, "shield"))
    {
        for (i = 0; i < 40; i++)
        {
            if (!strcmp(fn, act->shield[i].model_name) && act->shield[i].mesh_index != -1)
                return act->shield[i].mesh_index;
        }
    }
    else if (act->weapon && !strcmp(kind, "weapon"))
    {
        for (i = 0; i < 100; i++)
        {
            if (!strcmp(fn, act->weapon[i].model_name) && act->weapon[i].mesh_index != -1)
                return act->weapon[i].mesh_index;
        }
    }

    return -1;
}

static int cal_load_mesh(actor_types *act, const char *fn, const char *kind, float scale)
{
    int res;
    if (!fn || !*fn || !act->coremodel)
        return -1;
    if (kind)
    {
        res = cal_search_mesh(act, fn, kind);
        if (res != -1)
            return res;
    }

    res = CalCoreModel_ELLoadCoreMesh(act->coremodel, fn);
    if (res >= 0)
    {
        struct CalCoreMesh *mesh = CalCoreModel_GetCoreMesh(act->coremodel, res);
        if (mesh && scale != 1.0)
            CalCoreMesh_Scale(mesh, scale);
    }
    else
    {
        LOG_ERROR("Cal3d error: %s: %s\n", fn, CalError_GetLastErrorDescription());
    }

    return res;
}

struct cal_anim cal_load_idle(actor_types *act, const char *str)
{
    struct cal_anim res = {-1, 0, 0, 0.0f, -1, 0.0f};
    struct CalCoreAnimation *coreanim;

    res.anim_index = CalCoreModel_ELLoadCoreAnimation(act->coremodel, str, act->scale);
    if (res.anim_index == -1)
    {
        LOG_ERROR("Cal3d error: %s: %s\n", str, CalError_GetLastErrorDescription());
        return res;
    }

    coreanim = CalCoreModel_GetCoreAnimation(act->coremodel, res.anim_index);
    if (coreanim)
        res.duration = CalCoreAnimation_GetDuration(coreanim);
    else
        LOG_ERROR("No Anim: %s", str);

    return res;
}

void init_actor_defs()
{
    int i, j, k;

memset(attached_actors_defs, 0, sizeof (attached_actors_defs));

#ifdef	NEW_TEXTURES
    set_invert_v_coord();
#endif // NEW_TEXTURES

    for (i = 0; i < nr_actor_defs; ++i)
    {
        actor_types *act = actors_defs + i;
        if (!*act->actor_name)
            continue;
        if (*skeleton_names[i])
        {
            act->coremodel = CalCoreModel_New("Model");
            if (!CalCoreModel_ELLoadCoreSkeleton(act->coremodel, skeleton_names[i]))
            {
                LOG_ERROR("Cal3d error: %s: %s", skeleton_names[i], CalError_GetLastErrorDescription());
                act->skeleton_type = -1;
            }
            else
            {
                act->skeleton_type = get_skeleton(act->coremodel, skeleton_names[i]);
            }
        }
        if (act->coremodel)
        {
            struct CalCoreSkeleton *skel = CalCoreModel_GetCoreSkeleton(act->coremodel);
            if (skel)
                CalCoreSkeleton_Scale(skel, act->skel_scale);
            if (use_animation_program)
                build_buffers(act);

        }
        if (!act->head || !*act->head[0].model_name)
        {
            act->shirt[0].mesh_index = cal_load_mesh(act, act->file_name, NULL,
                                                     act->mesh_scale);
            continue;
        }
        for (j = 0; j < 10; ++j)
        {
            body_part *part = act->head + j;
            if (*part->model_name)
            {
                part->mesh_index = cal_load_mesh(act, part->model_name, "head", act->mesh_scale);
                actor_check_int(act, "head", "mesh", part->mesh_index);
            }
        }
        if (act->shield)
        {
            for (j = 0; j < 40; ++j)
            {
                shield_part *part = act->shield + j;
                if (*part->model_name)
                {
                    part->mesh_index = cal_load_mesh(act, part->model_name, "shield", act->skel_scale);
                    actor_check_int(act, "shield", "mesh", part->mesh_index);
                }
            }
        }
        if (act->cape)
        {
            for (j = 0; j < 50; ++j)
            {
                body_part *part = act->cape + j;
                if (*part->model_name)
                {
                    part->mesh_index = cal_load_mesh(act, part->model_name, "cape", act->mesh_scale);
                    actor_check_int(act, "cape", "mesh", part->mesh_index);
                }
            }
        }
        if (act->helmet)
        {
            for (j = 0; j < 100; ++j)
            {
                body_part *part = act->helmet + j;
                if (*part->model_name)
                {
                    part->mesh_index = cal_load_mesh(act, part->model_name, "helmet", act->mesh_scale);
                    actor_check_int(act, "helmet", "mesh", part->mesh_index);
                }
            }
        }
        if (act->neck)
        {
            for (j = 0; j < 20; ++j)
            {
                body_part *part = act->neck + j;
                if (*part->model_name)
                {
                    part->mesh_index = cal_load_mesh(act, part->model_name, "neck", act->mesh_scale);
                    actor_check_int(act, "neck", "mesh", part->mesh_index);
                }
            }
        }
        if (act->weapon)
        {
            for (j = 0; j < 100; ++j)
            {
                weapon_part *part = act->weapon + j;
                if (*part->model_name)
                {
                    part->mesh_index = cal_load_mesh(act, part->model_name, "weapon", act->skel_scale);
                    actor_check_int(act, "weapon.mesh", part->model_name, part->mesh_index);
                }
                for (k = 0; k < NUM_WEAPON_FRAMES; ++k)
                {
                    int reg_idx = part->cal_frames[k].anim_index;
                    if (reg_idx >= 0)
                    {
                        const frames_reg *reg = frames_regs + reg_idx;
                        part->cal_frames[k]
                            = cal_load_anim(act, reg->fname,
#ifdef NEW_SOUND
                                            reg->sound, reg->sound_scale,
#endif // NEW_SOUND
                                            reg->duration);
                    }

                    reg_idx = part->cal_frames[k].sound;
                    if (reg_idx >= 0)
                    {
                        const sound_reg *reg = sound_regs + reg_idx;
                        cal_set_anim_sound(part->cal_frames + k, reg->sound,
                               reg->sound_scale);
                    }
                }
            }
        }
        if (act->shirt)
        {
            for (j = 0; j < 100; ++j)
            {
                shirt_part *part = act->shirt + j;
                if (*part->model_name)
                {
                    part->mesh_index = cal_load_mesh(act, part->model_name, "shirt", act->mesh_scale);
                    actor_check_int(act, "shirt", "mesh", part->mesh_index);
                }
            }
        }
        if (act->boots)
        {
            for (j = 0; j < 40; ++j)
            {
                body_part *part = act->boots + j;
                if (*part->model_name)
                {
                    part->mesh_index = cal_load_mesh(act, part->model_name, "boots", act->mesh_scale);
                    actor_check_int(act, "boots", "mesh", part->mesh_index);
                }
            }
        }
        if (act->legs)
        {
            for (j = 0; j < 60; ++j)
            {
                body_part *part = act->legs + j;
                if (*part->skin_name)
                {
                    part->mesh_index = cal_load_mesh(act, part->model_name, "legs", act->mesh_scale);
                    actor_check_int(act, "legs", "mesh", part->mesh_index);
                }
            }
        }

#ifdef NEW_SOUND
        if (act->battlecry.sound >= 0)
        {
            const sound_reg *reg = sound_regs + act->battlecry.sound;
            int idx = get_index_for_sound_type_name(reg->sound);
            if (idx == -1)
            {
                LOG_ERROR("Unknown battlecry sound (%s) in actor def: %s\n",
                    reg->sound, act->actor_name);
            }
            act->battlecry.sound = idx;
            act->battlecry.scale = *reg->sound_scale ? atof(reg->sound_scale) : 1.0f;
        }
#endif

        for (j = 0; j < NUM_ACTOR_FRAMES; ++j)
        {
            int reg_idx = act->cal_frames[j].anim_index;
            if (reg_idx >= 0)
            {
                const frames_reg *reg = frames_regs + reg_idx;
                act->cal_frames[j] = cal_load_anim(act, reg->fname,
#ifdef NEW_SOUND
                                                   reg->sound, reg->sound_scale,
#endif // NEW_SOUND
                                                   reg->duration);
            }

#ifdef NEW_SOUND
            reg_idx = act->cal_frames[j].sound;
            if (reg_idx >= 0)
            {
                const sound_reg *reg = sound_regs + reg_idx;
                cal_set_anim_sound(act->cal_frames + j, reg->sound, reg->sound_scale);
            }
#endif
        }
    }

    for (i = 0; i < nr_idle_group_regs; ++i)
    {
        const idle_group_reg *reg = idle_group_regs + i;
        actor_types *act = actors_defs + reg->act_idx;
        act->idle_group[reg->group_idx].anim[reg->anim_idx] = cal_load_idle(act, reg->fname);
    }

    for (i = 0; i < nr_emote_regs; ++i)
    {
        const emote_reg *reg = emote_regs + i;
        actor_types *act = actors_defs + reg->act_idx;
        if (!act->emote_frames)
            act->emote_frames = create_hash_table(EMOTES_FRAMES, hash_fn_int, cmp_fn_int, free);
        emote_anims[i] = cal_load_anim(act, reg->fname,
#ifdef NEW_SOUND
                                       reg->sound, reg->sound_scale,
#endif // NEW_SOUND
                                       reg->duration);
        hash_add(act->emote_frames, (void*)(NULL+reg->frame_idx), emote_anims + i);
    }

    for (i = 0; i < nr_parent_regs; ++i)
    {
        const bone_reg *reg = parent_regs + i;
        actor_types *att_act = actors_defs + reg->att_act_idx;
        attached_actors_types *att = attached_actors_defs + reg->act_idx;
        struct CalCoreSkeleton *skel = CalCoreModel_GetCoreSkeleton(att_act->coremodel);
        if (skel)
        {
            att->actor_type[reg->att_act_idx].parent_bone_id = find_core_bone_id(skel, reg->name);
            if (att->actor_type[reg->att_act_idx].parent_bone_id < 0)
            {
                LOG_ERROR("bone %s was not found in skeleton of actor type %d\n",
                          reg->name, reg->att_act_idx);
            }
        }
        else
        {
            LOG_ERROR("the skeleton for actor type %d doesn't exist!\n",
                      reg->att_act_idx);
        }
    }
    for (i = 0; i < nr_local_regs; ++i)
    {
        const bone_reg *reg = local_regs + i;
        actor_types *act = actors_defs + reg->act_idx;
        attached_actors_types *att = attached_actors_defs + reg->act_idx;
        struct CalCoreSkeleton *skel = CalCoreModel_GetCoreSkeleton(act->coremodel);
        if (skel)
        {
            att->actor_type[reg->att_act_idx].local_bone_id = find_core_bone_id(skel, reg->name);
            if (att->actor_type[reg->att_act_idx].local_bone_id < 0)
            {
                LOG_ERROR("bone %s was not found in skeleton of actor type %d\n",
                          reg->name, reg->act_idx);
            }
        }
        else
        {
            LOG_ERROR("the skeleton for actor type %d doesn't exist!\n",
                      reg->act_idx);
        }
    }
    for (i = 0; i < nr_attached_frames_regs; ++i)
    {
        const attached_frames_reg *reg = attached_frames_regs + i;
        attached_actors_types *att = attached_actors_defs + reg->act_idx;
        att->actor_type[reg->act_idx].cal_frames[reg->frame_idx]
            = cal_load_anim(actors_defs + reg->held_act_idx, reg->fname,
#ifdef NEW_SOUND
                            reg->sound, reg->sound_scale,
#endif
                            reg->duration);
    }
}

void free_actor_defs()
{
    int i;
    for (i = 0; i < nr_actor_defs; ++i)
    {
        if (actors_defs[i].hardware_model)
            clear_buffers(actors_defs + i);
        CalCoreModel_Delete(actors_defs[i].coremodel);
    }
}

void free_emotes()
{
	int i;
	for (i = 0; i < nr_actor_defs; i++)
		destroy_hash_table(actors_defs[i].emote_frames);
	destroy_hash_table(emote_cmds);
	destroy_hash_table(emotes);
}

