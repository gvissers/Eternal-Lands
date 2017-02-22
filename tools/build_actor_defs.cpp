#include <algorithm>
#include <cstdarg>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <GL/gl.h>
#include <libxml/parser.h>
#include <SDL/SDL_types.h>

#include "../actor_def_types.h"
#include "../client_serv.h"
#include "../xml.h"

#ifdef EXT_ACTOR_DICT
static int actor_part_sizes[ACTOR_NUM_PARTS];
#else /* EXT_ACTOR_DICT */
namespace
{

const std::unordered_map<std::string, int> skin_color_dict({
    { "brown",     SKIN_BROWN     },
    { "normal",    SKIN_NORMAL    },
    { "pale",      SKIN_PALE      },
    { "tan",       SKIN_TAN       },
    { "darkblue",  SKIN_DARK_BLUE }, // Elf's only
    { "dark_blue", SKIN_DARK_BLUE }, // Elf's only, synonym
    { "white",     SKIN_WHITE     }  // Draegoni only
});
const std::unordered_map<std::string, int> glow_mode_dict({
    { "none",    GLOW_NONE    },
    { "fire",    GLOW_FIRE    },
    { "ice",     GLOW_COLD    },
    { "thermal", GLOW_THERMAL },
    { "magic",   GLOW_MAGIC   }
});
const std::unordered_map<std::string, int> head_number_dict({
    { "1",  HEAD_1 },
    { "2",  HEAD_2 },
    { "3",  HEAD_3 },
    { "4",  HEAD_4 },
    { "5",  HEAD_5 }
});
const int actor_part_sizes[ACTOR_NUM_PARTS] = {
    // Elements according to actor_parts_enum
    10, 40, 50, 100, 100, 100, 10, 20, 40, 60, 20, 20
};

} // namespace
#endif /* EXT_ACTOR_DICT */

static std::unordered_set<std::string> string_table;

namespace
{

std::string value(const xmlNode *node)
{
    if (!node->children)
        return std::string();
    return std::string(reinterpret_cast<const char*>(node->children->content));
}

std::string lc_value(const xmlNode *node)
{
    std::string res = value(node);
    std::transform(res.begin(), res.end(), res.begin(), ::tolower);
    return res;
}

std::string item_value(const xmlNode *item, const char *name)
{
    for (const xmlNode *node = item->children; node; node = node->next)
    {
        if (node->type == XML_ELEMENT_NODE
            && xmlStrcasecmp(node->name, reinterpret_cast<const xmlChar*>(name)) == 0)
        {
            return value(node);
        }
    }
    return std::string();
}

std::string property(const xmlNode *node, const char *prop)
{
    for (const xmlAttr *attr = node->properties; attr; attr = attr->next)
    {
        if (attr->type == XML_ATTRIBUTE_NODE && !xmlStrcasecmp(attr->name, (xmlChar *)prop))
            return std::string(reinterpret_cast<const char*>(attr->children->content));
    }
    return std::string();
}

std::string lc_property(const xmlNode *node, const char *prop)
{
    std::string res = property(node, prop);
    std::transform(res.begin(), res.end(), res.begin(), ::tolower);
    return res;
}

const char* char_ptr_of(const std::string& str)
{
    auto iter = string_table.find(str);
    if (iter == string_table.end())
        iter = string_table.insert(str).first;
    return iter->c_str();
}

} // namespace

extern "C"
{

char* safe_strncpy(char* dest, const char* src, size_t len)
{
	if (len > 0)
	{
		strncpy(dest, src, len - 1);
		dest[len - 1] = '\0';
	}
	return dest;
}

void log_error(const char*, const Uint32, const char* message, ...)
{
    va_list ap;
    va_start(ap, message);
    vfprintf(stderr, message, ap);
    va_end(ap);
    fputc('\n', stderr);
}

} // extern "C"

actor_types actors_defs[MAX_ACTOR_DEFS];
attached_actors_types attached_actors_defs[MAX_ACTOR_DEFS];

static body_part* all_body;
static int all_body_used = 0;
static shield_part* all_shields;
static int all_shields_used = 0;
static weapon_part* all_weapons;
static int all_weapons_used = 0;
static shirt_part* all_shirts;
static int all_shirts_used = 0;
static skin_part* all_skins;
static int all_skins_used = 0;
static hair_part* all_hairs;
static int all_hairs_used = 0;
static eyes_part* all_eyes;
static int all_eyes_used = 0;

std::vector<std::string> idle_group_regs;
std::vector<std::string> skeleton_names;

struct frames_reg
{
    std::string fname;
#ifdef NEW_SOUND
    std::string sound;
    std::string sound_scale;
#endif
    int duration;

    frames_reg(const std::string& fname,
#ifdef NEW_SOUND
               const std::string& sound, const std::string& sound_scale,
#endif
               int duration):
        fname(fname),
#ifdef NEW_SOUND
        sound(sound), sound_scale(sound_scale),
#endif
        duration(duration) {}
};

std::vector<frames_reg> frames_regs;
std::unordered_map<std::string, int> frames_regs_known;

struct emote_reg
{
    std::string fname;
#ifdef NEW_SOUND
    std::string sound;
    std::string sound_scale;
#endif
    int duration;
    int act_idx;
    int frame_idx;

    emote_reg(const std::string& fname,
#ifdef NEW_SOUND
               const std::string& sound, const std::string& sound_scale,
#endif
               int duration, int act_idx, int frame_idx):
        fname(fname),
#ifdef NEW_SOUND
        sound(sound), sound_scale(sound_scale),
#endif
        duration(duration), act_idx(act_idx), frame_idx(frame_idx) {}
};

std::vector<emote_reg> emote_regs;

struct bone_reg
{
    std::string name;
    int act_idx;
    int att_act_idx;

    bone_reg(const std::string& name, int act_idx, int att_act_idx):
        name(name), act_idx(act_idx), att_act_idx(att_act_idx) {}
};

std::vector<bone_reg> parent_regs;
std::vector<bone_reg> local_regs;

struct attached_frames_reg
{
    std::string fname;
#ifdef NEW_SOUND
    std::string sound;
    std::string sound_scale;
#endif
    int duration;
    int act_idx;
    int held_act_idx;
    int frame_idx;

    attached_frames_reg(const std::string& fname,
#ifdef NEW_SOUND
                        const std::string& sound, const std::string& sound_scale,
#endif
                        int duration, int act_idx, int held_act_idx, int frame_idx):
        fname(fname),
#ifdef NEW_SOUND
        sound(sound), sound_scale(sound_scale),
#endif
        duration(duration), act_idx(act_idx), held_act_idx(held_act_idx),
        frame_idx(frame_idx) {}
};

std::vector<attached_frames_reg> attached_frames_regs;

#ifdef NEW_SOUND
struct sound_reg
{
	std::string sound;
	std::string sound_scale;

	sound_reg(const std::string& sound, const std::string& sound_scale):
		sound(sound), sound_scale(sound_scale) {}
};

std::vector<sound_reg> sound_regs;
std::unordered_map<std::string, int> sound_regs_known;
#endif


static void my_tolower(char *src)
{
    if (src)
    {
        for ( ; *src; ++src)
            *src = tolower(*src);
    }
}

static const char* glow_mode_name(int mode)
{
    static const char* names[] = {
        "GLOW_NONE", "GLOW_FIRE", "GLOW_COLD", "GLOW_THERMAL", "GLOW_MAGIC"
    };
    static const int nr_names = sizeof(names) / sizeof(*names);
    if (mode < 0 || mode >= nr_names)
        return NULL;
    return names[mode];
}

static const char* actor_type_name(int type)
{
    static const char* names[] = {
        "human_female",
        "human_male",
        "elf_female",
        "elf_male",
        "dwarf_female",
        "dwarf_male",
        "wraith",
        "cyclops",
        "beaver",
        "rat",
        "goblin_male_2",
        "goblin_female_1",
        "town_folk4",
        "town_folk5",
        "shop_girl3",
        "deer",
        "bear",
        "wolf",
        "white_rabbit",
        "brown_rabbit",
        "boar",
        "bear2",
        "snake1",
        "snake2",
        "snake3",
        "fox",
        "puma",
        "ogre_male_1",
        "goblin_male_1",
        "orc_male_1",
        "orc_female_1",
        "skeleton",
        "gargoyle1",
        "gargoyle2",
        "gargoyle3",
        "troll",
        "chimeran_wolf_mountain",
        "gnome_female",
        "gnome_male",
        "orchan_female",
        "orchan_male",
        "draegoni_female",
        "draegoni_male",
        "skunk_1",
        "racoon_1",
        "unicorn_1",
        "chimeran_wolf_desert",
        "chimeran_wolf_forest",
        "bear_3",
        "bear_4",
        "panther",
        "feran",
        "leopard_1",
        "leopard_2",
        "chimeran_wolf_arctic",
        "tiger_1",
        "tiger_2",
        "armed_female_orc",
        "armed_male_orc",
        "armed_skeleton",
        "phantom_warrior",
        "imp",
        "brownie",
        "leprechaun",
        "spider_s_1",
        "spider_s_2",
        "spider_s_3",
        "spider_l_1",
        "spider_l_2",
        "spider_l_3",
        "wood_sprite",
        "spider_l_4",
        "spider_s_4",
        "giant_1",
        "hobgoblin",
        "yeti",
        "snake4",
        "feros",
        "dragon1"
    };
    static const int nr_names = sizeof(names) / sizeof(*names);
    if (type < 0 || type >= nr_names)
        return NULL;
    return names[type];
}

void actor_check_string(actor_types *act, const char *section,
                        const char *type, const char *value)
{
    if (!value || *value=='\0')
    {
//#ifdef DEBUG
        std::cerr << "Data Error in " << act->actor_name << '(' << act->actor_type
			<< "): Missing " << section << '.' << type << '\n';
//#endif // DEBUG
    }
}

void actor_check_int(actor_types *act, const char *section,
                     const char *type, int value)
{
    if (value < 0)
    {
//#ifdef DEBUG
        std::cerr << "Data Error in " << act->actor_name << '(' << act->actor_type
			<< "): Missing " << section << '.' << type << '\n';
//#endif // DEBUG
    }
}

const xmlNode *get_default_node(const xmlNode *cfg, const xmlNode *defaults)
{
    // first, check for errors
    if (!defaults || !cfg)
        return NULL;

    // let's find out what group to look for
    std::string group = lc_property(cfg, "group");

    // look for defaul entries with the same name
    for (const xmlNode *item = defaults->children; item; item = item->next)
    {
        if (item->type != XML_ELEMENT_NODE)
            continue;

        if (xmlStrcasecmp(item->name, cfg->name) == 0)
        {
            // either both have no group, or both groups match
            if (lc_property(item, "group") == group)
                // this is the default entry we want then!
                return item;
        }
    }

    // if we got here, there is no default node that matches
    return NULL;
}

int parse_actor_shirt(actor_types *act, const xmlNode *cfg, const xmlNode *defaults)
{
    const xmlNode *item;
    int ok, col_idx;
    shirt_part *shirt;

    if (!cfg || !cfg->children)
        return 0;

    col_idx = get_int_property(cfg, "id");
    if (col_idx < 0 || col_idx >= actor_part_sizes[ACTOR_SHIRT_SIZE])
    {
        std::cerr << "Unable to find id/property node " << cfg->name << '\n';
        return 0;
    }

    if (!act->shirt)
    {
        int i;
        act->shirt = all_shirts + all_shirts_used;
        all_shirts_used += actor_part_sizes[ACTOR_SHIRT_SIZE];
        for (i = 0; i < actor_part_sizes[ACTOR_SHIRT_SIZE]; ++i)
            act->shirt[i].mesh_index= -1;
    }

    shirt = act->shirt + col_idx;
    ok = 1;
    for (item=cfg->children; item; item=item->next)
    {
        if (item->type == XML_ELEMENT_NODE)
        {
            if (xmlStrcasecmp(item->name, (xmlChar*)"arms") == 0)
            {
                shirt->arms_name = char_ptr_of(value(item));
            }
            else if(xmlStrcasecmp(item->name, (xmlChar*)"mesh") == 0)
            {
                shirt->model_name = char_ptr_of(value(item));
            }
            else if(xmlStrcasecmp(item->name, (xmlChar*)"torso") == 0)
            {
                shirt->torso_name = char_ptr_of(value(item));
            }
            else if(xmlStrcasecmp(item->name, (xmlChar*)"armsmask") == 0)
            {
                shirt->arms_mask = char_ptr_of(value(item));
            }
            else if(xmlStrcasecmp(item->name, (xmlChar*)"torsomask") == 0)
            {
                shirt->torso_mask = char_ptr_of(value(item));
            }
            else
            {
                std::cerr << "unknown shirt property \"" << item->name << "\"\n";
                ok = 0;
            }
        }
    }

    // check for default entries, if found, use them to fill in missing data
    if (defaults)
    {
        const xmlNode *default_node = get_default_node(cfg, defaults);
        if (default_node)
        {
            if (!shirt->arms_name || *shirt->arms_name == '\0')
                shirt->arms_name = char_ptr_of(item_value(default_node, "arms"));
            if (!shirt->model_name || *shirt->model_name == '\0')
                shirt->model_name = char_ptr_of(item_value(default_node, "mesh"));
            if (!shirt->torso_name || *shirt->torso_name == '\0')
                shirt->torso_name = char_ptr_of(item_value(default_node, "torso"));
        }
    }

    // check the critical information
    actor_check_string(act, "shirt", "arms", shirt->arms_name);
    actor_check_string(act, "shirt", "model", shirt->model_name);
    actor_check_string(act, "shirt", "torso", shirt->torso_name);

    return ok;
}

int parse_actor_skin(actor_types *act, const xmlNode *cfg, const xmlNode *defaults)
{
    const xmlNode *item;
    int ok, col_idx;
    skin_part *skin;

    if (!cfg || !cfg->children)
        return 0;

    col_idx= get_int_property(cfg, "id");
    if (col_idx < 0)
        col_idx = skin_color_dict.at(lc_property(cfg, "color"));
    if (col_idx < 0 || col_idx >= actor_part_sizes[ACTOR_SKIN_SIZE])
    {
        std::cerr << "Unable to find id/property node " << cfg->name << '\n';
        return 0;
    }

    if (!act->skin)
    {
        act->skin = all_skins + all_skins_used;
        all_skins_used += actor_part_sizes[ACTOR_SKIN_SIZE];
    }

    skin = act->skin + col_idx;
    ok = 1;
    for (item = cfg->children; item; item = item->next)
    {
        if (item->type == XML_ELEMENT_NODE)
        {
            if (xmlStrcasecmp(item->name, (xmlChar*)"hands") == 0)
            {
                skin->hands_name = char_ptr_of(value(item));
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"head") == 0)
            {
                skin->head_name= char_ptr_of(value(item));
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"torso") == 0)
            {
                skin->body_name = char_ptr_of(value(item));
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"arms") == 0)
            {
                skin->arms_name = char_ptr_of(value(item));
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"legs") == 0)
            {
                skin->legs_name = char_ptr_of(value(item));
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"feet") == 0)
            {
                skin->feet_name = char_ptr_of(value(item));
            }
            else
            {
                std::cerr << "unknown skin property \"" << item->name << "\"\n";
                ok = 0;
            }
        }
    }

    // check for default entries, if found, use them to fill in missing data
    if (defaults)
    {
        const xmlNode *default_node= get_default_node(cfg, defaults);
        if (default_node)
        {
            if (!skin->hands_name || *skin->hands_name == '\0')
                skin->hands_name = char_ptr_of(item_value(default_node, "hands"));
            if (!skin->head_name || *skin->head_name == '\0')
                skin->head_name = char_ptr_of(item_value(default_node, "head"));
        }
    }

    // check the critical information
    actor_check_string(act, "skin", "hands", skin->hands_name);
    actor_check_string(act, "skin", "head", skin->head_name);

    return ok;
}

int parse_actor_legs(actor_types *act, const xmlNode *cfg, const xmlNode *defaults)
{
    const xmlNode *item;
    int ok, col_idx;
    body_part *legs;

    if (!cfg || !cfg->children)
        return 0;

    col_idx= get_int_property(cfg, "id");
    if (col_idx < 0 || col_idx >= actor_part_sizes[ACTOR_LEGS_SIZE])
    {
        std::cerr << "Unable to find id/property node " << cfg->name << '\n';
        return 0;
    }

    if (!act->legs)
    {
        int i;
        act->legs = all_body + all_body_used;
        all_body_used += actor_part_sizes[ACTOR_LEGS_SIZE];
        for (i = 0; i < actor_part_sizes[ACTOR_LEGS_SIZE]; ++i)
            act->legs[i].mesh_index= -1;
    }

    legs = act->legs + col_idx;
    ok = 1;
    for (item = cfg->children; item; item = item->next)
    {
        if (item->type == XML_ELEMENT_NODE)
        {
            if (xmlStrcasecmp(item->name, (xmlChar*)"skin") == 0)
            {
                legs->skin_name = char_ptr_of(value(item));
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"mesh") == 0)
            {
                legs->model_name = char_ptr_of(value(item));
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"legsmask") == 0)
            {
                legs->skin_mask = char_ptr_of(value(item));
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"glow") == 0)
            {
                int mode = glow_mode_dict.at(lc_value(item));
                legs->glow = mode < 0 ? GLOW_NONE : mode;
            }
            else
            {
                std::cerr << "unknown legs property \"" << item->name << "\"\n";
                ok = 0;
            }
        }
    }

    // check for default entries, if found, use them to fill in missing data
    if(defaults)
    {
        const xmlNode *default_node = get_default_node(cfg, defaults);
        if (default_node)
        {
            if (!legs->skin_name || *legs->skin_name == '\0')
                legs->skin_name = char_ptr_of(item_value(default_node, "skin"));
            if (!legs->model_name || *legs->model_name == '\0')
                legs->model_name = char_ptr_of(item_value(default_node, "mesh"));
        }
    }

    // check the critical information
    actor_check_string(act, "legs", "skin", legs->skin_name);
    actor_check_string(act, "legs", "model", legs->model_name);

    return ok;
}

static int parse_actor_body_part(actor_types *act, body_part *part, const xmlNode *cfg,
                                 const char *part_name, const xmlNode *default_node)
{
    const xmlNode *item;
    int ok = 1;

    if (!cfg) return 0;

    for (item=cfg; item; item=item->next)
    {
        if (item->type != XML_ELEMENT_NODE)
            continue;

        if (xmlStrcasecmp(item->name, (xmlChar*)"mesh") == 0)
        {
            part->model_name = char_ptr_of(value(item));
        }
        else if (xmlStrcasecmp(item->name, (xmlChar*)"skin") == 0)
        {
            part->skin_name = char_ptr_of(value(item));
        }
        else if(xmlStrcasecmp(item->name, (xmlChar*)"skinmask") == 0)
        {
            part->skin_mask = char_ptr_of(value(item));
        }
        else if(xmlStrcasecmp(item->name, (xmlChar*)"glow") == 0)
        {
            int mode = glow_mode_dict.at(lc_value(item));
            part->glow = mode < 0 ? GLOW_NONE : mode;
        }
        else
        {
            std::cerr << "unknown " << part_name << " property \"" << item->name << "\"\n";
            ok = 0;
        }
    }

    // check for default entries, if found, use them to fill in missing data
    if (default_node)
    {
        if (!part->skin_name || *part->skin_name == '\0')
        {
            if (strcmp(part_name, "head"))
                part->skin_name = char_ptr_of(item_value(default_node, "skin"));
        }
        if (!part->model_name || *part->model_name == '\0')
            part->model_name = char_ptr_of(item_value(default_node, "mesh"));
    }

    // check the critical information
    if (strcmp(part_name, "head"))
    {    // heads don't have separate skins here
        actor_check_string(act, part_name, "skin", part->skin_name);
    }
    actor_check_string(act, part_name, "model", part->model_name);

    return ok;
}

int parse_actor_helmet(actor_types *act, const xmlNode *cfg, const xmlNode *defaults)
{
    const xmlNode *default_node = get_default_node(cfg, defaults);
    int type_idx;

    if (!cfg || !cfg->children)
        return 0;

    type_idx = get_int_property(cfg, "id");
    if (type_idx < 0 || type_idx >= actor_part_sizes[ACTOR_HELMET_SIZE])
    {
        std::cerr << "Unable to find id/property node " << cfg->name << '\n';
        return 0;
    }

    if (!act->helmet)
    {
        int i;
        act->helmet = all_body + all_body_used;
        all_body_used += actor_part_sizes[ACTOR_HELMET_SIZE];
        for (i = 0; i < actor_part_sizes[ACTOR_HELMET_SIZE]; ++i)
            act->helmet[i].mesh_index= -1;
    }

    return parse_actor_body_part(act, act->helmet + type_idx, cfg->children,
                                 "helmet", default_node);
}

int parse_actor_neck(actor_types *act, const xmlNode *cfg, const xmlNode *defaults)
{
    const xmlNode *default_node = get_default_node(cfg, defaults);
    int type_idx;

    if (!cfg || !cfg->children)
        return 0;

    type_idx = get_int_property(cfg, "id");
    if (type_idx < 0 || type_idx >= actor_part_sizes[ACTOR_NECK_SIZE])
    {
        std::cerr << "Unable to find id/property node " << cfg->name << '\n';
        return 0;
    }

    if (!act->neck)
    {
        int i;
        act->neck = all_body + all_body_used;
        all_body_used += actor_part_sizes[ACTOR_NECK_SIZE];
        for (i = 0; i < actor_part_sizes[ACTOR_NECK_SIZE]; ++i)
            act->neck[i].mesh_index= -1;
    }

    return parse_actor_body_part(act, act->neck + type_idx, cfg->children,
                                 "neck", default_node);
}

int parse_actor_cape(actor_types *act, const xmlNode *cfg, const xmlNode *defaults)
{
    const xmlNode *default_node = get_default_node(cfg, defaults);
    int type_idx;

    if (!cfg || !cfg->children)
        return 0;

    type_idx = get_int_property(cfg, "id");
    if (type_idx < 0 || type_idx >= actor_part_sizes[ACTOR_CAPE_SIZE])
    {
        std::cerr << "Unable to find id/property node " << cfg->name << '\n';
        return 0;
    }

    if (!act->cape)
    {
        int i;
        act->cape = all_body + all_body_used;
        all_body_used += actor_part_sizes[ACTOR_CAPE_SIZE];
        for (i = actor_part_sizes[ACTOR_CAPE_SIZE]; i--;)
            act->cape[i].mesh_index= -1;
    }

    return parse_actor_body_part(act, act->cape + type_idx, cfg->children,
                                 "cape", default_node);
}

int parse_actor_head(actor_types *act, const xmlNode *cfg, const xmlNode *defaults)
{
    const xmlNode *default_node= get_default_node(cfg, defaults);
    int type_idx;

    if (!cfg || !cfg->children)
        return 0;

    type_idx = get_int_property(cfg, "id");
    if(type_idx < 0)
        type_idx = head_number_dict.at(lc_property(cfg, "number"));
    if (type_idx < 0 || type_idx >= actor_part_sizes[ACTOR_HEAD_SIZE])
    {
        std::cerr << "Unable to find id/property node " << cfg->name << '\n';
        return 0;
    }

    if (!act->head)
    {
        int i;
        act->head = all_body + all_body_used;
        all_body_used += actor_part_sizes[ACTOR_HEAD_SIZE];
        for (i = 0; i < actor_part_sizes[ACTOR_HEAD_SIZE]; ++i)
            act->head[i].mesh_index= -1;
    }

    return parse_actor_body_part(act, act->head + type_idx, cfg->children,
                                 "head", default_node);
}

int parse_actor_hair(actor_types *act, const xmlNode *cfg)
{
    int col_idx;

    if (!cfg || !cfg->children)
        return 0;

    col_idx = get_int_property(cfg, "id");
    if (col_idx < 0 || col_idx >= actor_part_sizes[ACTOR_HAIR_SIZE])
    {
        std::cerr << "Unable to find id/property node " << cfg->name << '\n';
        return 0;
    }

    if (!act->hair)
    {
        act->hair = all_hairs + all_hairs_used;
        all_hairs_used += actor_part_sizes[ACTOR_HAIR_SIZE];
    }

    act->hair[col_idx].hair_name = char_ptr_of(value(cfg));

    return 1;
}

int parse_actor_eyes(actor_types *act, const xmlNode *cfg)
{
    int col_idx;

    if (!cfg || !cfg->children)
        return 0;

    col_idx = get_int_property(cfg, "id");
    if (col_idx < 0 || col_idx >= actor_part_sizes[ACTOR_EYES_SIZE])
    {
        std::cerr << "Unable to find id/property node " << cfg->name << '\n';
        return 0;
    }

    if (!act->eyes)
    {
        act->eyes = all_eyes + all_eyes_used;
        all_eyes_used += actor_part_sizes[ACTOR_EYES_SIZE];
    }

    act->eyes[col_idx].eyes_name = char_ptr_of(value(cfg));

    return 1;
}

static int cal_get_idle_group(actor_types *act, const char *name)
{
    int idx;

    for (idx = 0; idx < act->group_count; ++idx)
    {
        if (!strcmp(name, act->idle_group[idx].name))
            return idx;
    }

    idx = act->group_count++;
    act->idle_group[idx].name = char_ptr_of(name);

    return idx;
}

static void parse_idle_group(actor_types *act, const char *str)
{
    char gname[255];
    char fname[255];

    if (sscanf(str, "%254s %254s", gname, fname) != 2)
        return;

    int group_idx = cal_get_idle_group(act, gname);
    int anim_idx = act->idle_group[group_idx].count++;
    act->idle_group[group_idx].anim[anim_idx].anim_index = idle_group_regs.size();
    idle_group_regs.emplace_back(fname);
}

static int get_frames_reg_index(const std::string& fname,
#ifdef NEW_SOUND
                         const std::string& sound, const std::string& sound_scale,
#endif
                         int duration)
{
    std::ostringstream os;
    os << fname << '|' << sound << '|' << sound_scale << '|' << duration;

    std::string key = os.str();
    auto iter = frames_regs_known.find(key);
    if (iter != frames_regs_known.end())
        return iter->second;

    int idx = frames_regs.size();
    frames_regs.emplace_back(fname,
#ifdef NEW_SOUND
                             sound, sound_scale,
#endif
                             duration);
    frames_regs_known.emplace(key, idx);

    return idx;
}

int parse_actor_frames(actor_types *act, const xmlNode *cfg)
{
    const xmlNode *item;
    int ok = 1;

    if (!cfg)
        return 0;

    for (item = cfg; item; item = item->next)
    {
        if (item->type == XML_ELEMENT_NODE)
        {
            int index = -1;
            if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_IDLE_GROUP") == 0)
            {
                std::string str = value(item);
                parse_idle_group(act, str.c_str());
                index = -2;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_walk") == 0)
            {
                index = cal_actor_walk_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_run") == 0)
            {
                index = cal_actor_run_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_turn_left") == 0)
            {
                index = cal_actor_turn_left_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_turn_right") == 0)
            {
                index = cal_actor_turn_right_frame;
            } else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_die1") == 0)
            {
                index = cal_actor_die1_frame;
            } else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_die2") == 0)
            {
                index = cal_actor_die2_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_pain1") == 0)
            {
                index = cal_actor_pain1_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_pain2") == 0)
            {
                index = cal_actor_pain2_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_pick") == 0)
            {
                index = cal_actor_pick_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_drop") == 0)
            {
                index = cal_actor_drop_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_idle") == 0)
            {
                index = cal_actor_idle1_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_idle2") == 0)
            {
                index = cal_actor_idle2_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_idle_sit") == 0)
            {
                index = cal_actor_idle_sit_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_harvest") == 0)
            {
                index = cal_actor_harvest_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_cast") == 0)
            {
                index = cal_actor_attack_cast_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_sit_down") == 0)
            {
                index = cal_actor_sit_down_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_stand_up") == 0)
            {
                index = cal_actor_stand_up_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_in_combat") == 0)
            {
                index = cal_actor_in_combat_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_out_combat") == 0)
            {
                index = cal_actor_out_combat_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_combat_idle") == 0)
            {
                index = cal_actor_combat_idle_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_up_1") == 0)
            {
                index = cal_actor_attack_up_1_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_up_2") == 0)
            {
                index = cal_actor_attack_up_2_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_up_3") == 0)
            {
                index = cal_actor_attack_up_3_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_up_4") == 0)
            {
                index = cal_actor_attack_up_4_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_up_5") == 0)
            {
                index = cal_actor_attack_up_5_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_up_6") == 0)
            {
                index = cal_actor_attack_up_6_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_up_7") == 0)
            {
                index = cal_actor_attack_up_7_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_up_8") == 0)
            {
                index = cal_actor_attack_up_8_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_up_9") == 0)
            {
                index = cal_actor_attack_up_9_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_up_10") == 0)
            {
                index = cal_actor_attack_up_10_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_down_1") == 0)
            {
                index = cal_actor_attack_down_1_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_down_2") == 0)
            {
                index = cal_actor_attack_down_2_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_down_3") == 0)
            {
                index = cal_actor_attack_down_3_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_down_4") == 0)
            {
                index = cal_actor_attack_down_4_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_down_5") == 0)
            {
                index = cal_actor_attack_down_5_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_down_6") == 0)
            {
                index = cal_actor_attack_down_6_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_down_7") == 0)
            {
                index = cal_actor_attack_down_7_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_down_8") == 0)
            {
                index = cal_actor_attack_down_8_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_down_9") == 0)
            {
                index = cal_actor_attack_down_9_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_down_10") == 0)
            {
                index = cal_actor_attack_down_10_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_in_combat_held") == 0)
            {
                index = cal_actor_in_combat_held_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_out_combat_held") == 0)
            {
                index = cal_actor_out_combat_held_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_combat_idle_held") == 0)
            {
                index = cal_actor_combat_idle_held_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_in_combat_held_unarmed") == 0)
            {
                index = cal_actor_in_combat_held_unarmed_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_out_combat_held_unarmed") == 0)
            {
                index = cal_actor_out_combat_held_unarmed_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_combat_idle_held_unarmed") == 0)
            {
                index = cal_actor_combat_idle_held_unarmed_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_up_1_held") == 0)
            {
                index = cal_actor_attack_up_1_held_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_up_2_held") == 0)
            {
                index = cal_actor_attack_up_2_held_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_up_3_held") == 0)
            {
                index = cal_actor_attack_up_3_held_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_up_4_held") == 0)
            {
                index = cal_actor_attack_up_4_held_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_up_5_held") == 0)
            {
                index = cal_actor_attack_up_5_held_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_up_6_held") == 0)
            {
                index = cal_actor_attack_up_6_held_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_up_7_held") == 0)
            {
                index = cal_actor_attack_up_7_held_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_up_8_held") == 0)
            {
                index = cal_actor_attack_up_8_held_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_up_9_held") == 0)
            {
                index = cal_actor_attack_up_9_held_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_up_10_held") == 0)
            {
                index = cal_actor_attack_up_10_held_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_down_1_held") == 0)
            {
                index = cal_actor_attack_down_1_held_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_down_2_held") == 0)
            {
                index = cal_actor_attack_down_2_held_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_down_3_held") == 0)
            {
                index = cal_actor_attack_down_3_held_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_down_4_held") == 0)
            {
                index = cal_actor_attack_down_4_held_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_down_5_held") == 0)
            {
                index = cal_actor_attack_down_5_held_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_down_6_held") == 0)
            {
                index = cal_actor_attack_down_6_held_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_down_7_held") == 0)
            {
                index = cal_actor_attack_down_7_held_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_down_8_held") == 0)
            {
                index = cal_actor_attack_down_8_held_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_down_9_held") == 0)
            {
                index = cal_actor_attack_down_9_held_frame;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_attack_down_10_held") == 0)
            {
                index = cal_actor_attack_down_10_held_frame;
            }
            else if (!strncasecmp("CAL_emote", (const char*) item->name, 9))
            {
                emote_regs.emplace_back(value(item),
#ifdef NEW_SOUND
                                        property(item, "sound"),
                                        property(item, "sound_scale"),
#endif
                                        get_int_property(item, "duration"),
                                        act - actors_defs,
                                        get_int_property(item, "index"));
                continue;
            }

            if (index >= 0)
            {
                act->cal_frames[index].anim_index
                    = get_frames_reg_index(value(item),
#ifdef NEW_SOUND
                                           property(item, "sound"),
                                           property(item, "sound_scale"),
#endif // NEW_SOUND
                                           get_int_property(item, "duration"));
            }
            else if (index != -2)
            {
                std::cerr << "unknown frame property \"" << item->name << "\"\n";
                ok = 0;
            }
        }
    }

    return ok;
}

int parse_actor_boots(actor_types *act, const xmlNode *cfg, const xmlNode *defaults)
{
    const xmlNode *item;
    int ok, col_idx;
    body_part *boots;

    if (!cfg || !cfg->children)
        return 0;

    col_idx = get_int_property(cfg, "id");
    if (col_idx < 0 || col_idx >= actor_part_sizes[ACTOR_BOOTS_SIZE])
    {
        std::cerr << "Unable to find id/property node " << cfg->name << '\n';
        return 0;
    }

    if (!act->boots)
    {
        int i;
        act->boots = all_body + all_body_used;
        all_body_used += actor_part_sizes[ACTOR_BOOTS_SIZE];
        for (i = 0; i < actor_part_sizes[ACTOR_BOOTS_SIZE]; ++i)
            act->boots[i].mesh_index= -1;
    }

    boots = act->boots + col_idx;
    ok = 1;
    for (item = cfg->children; item; item = item->next)
    {
        if (item->type == XML_ELEMENT_NODE)
        {
            if (xmlStrcasecmp(item->name, (xmlChar*)"skin") == 0)
            {
                boots->skin_name = char_ptr_of(value(item));
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"mesh") == 0)
            {
                boots->model_name = char_ptr_of(value(item));
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"bootsmask") == 0)
            {
                boots->skin_mask = char_ptr_of(value(item));
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"glow") == 0)
            {
                int mode = glow_mode_dict.at(lc_value(item));
                boots->glow = mode < 0 ? GLOW_NONE : mode;
            }
            else
            {
                std::cerr << "unknown legs property \"" << item->name << "\"\n";
                ok = 0;
            }
        }
    }

    // check for default entries, if found, use them to fill in missing data
    if (defaults)
    {
        const xmlNode *default_node = get_default_node(cfg, defaults);

        if (default_node)
        {
            if (!boots->skin_name || *boots->skin_name == '\0')
                boots->skin_name = char_ptr_of(item_value(default_node, "skin"));
            if (!boots->model_name || *boots->model_name == '\0')
                boots->model_name = char_ptr_of(item_value(default_node, "mesh"));
        }
    }

    // check the critical information
    actor_check_string(act, "boots", "skin", boots->skin_name);
    actor_check_string(act, "boots", "model", boots->model_name);

    return ok;
}

int parse_actor_shield_part(actor_types *act, shield_part *part, const xmlNode *cfg,
                            const xmlNode *default_node)
{
    const xmlNode *item;
    int ok = 1;

    if (!cfg)
        return 0;

    for (item=cfg; item; item=item->next)
    {
        if (item->type != XML_ELEMENT_NODE)
            continue;

        if (xmlStrcasecmp(item->name, (xmlChar*)"mesh") == 0)
        {
            part->model_name = char_ptr_of(value(item));
        }
        else if(xmlStrcasecmp(item->name, (xmlChar*)"skin") == 0)
        {
            part->skin_name = char_ptr_of(value(item));
        }
        else if (xmlStrcasecmp(item->name, (xmlChar*)"skinmask") == 0)
        {
            part->skin_mask = char_ptr_of(value(item));
        }
        else if (xmlStrcasecmp(item->name, (xmlChar*)"glow") == 0)
        {
            int mode = glow_mode_dict.at(lc_value(item));
            part->glow = mode < 0 ? GLOW_NONE : mode;
        }
        else if (xmlStrcasecmp(item->name, (xmlChar*)"missile") == 0)
        {
            part->missile_type = get_int_value(item);
        }
        else
        {
            std::cerr << "unknown shield property \"" << item->name << "\"\n";
            ok = 0;
        }
    }

    // check for default entries, if found, use them to fill in missing data
    if (default_node)
    {
        if (!part->model_name || *part->model_name == '\0')
            part->model_name = char_ptr_of(item_value(default_node, "mesh"));
    }

    // check the critical information
    actor_check_string(act, "shield", "model", part->model_name);

    return ok;
}

int parse_actor_shield(actor_types *act, const xmlNode *cfg, const xmlNode *defaults)
{
    const xmlNode *default_node = get_default_node(cfg, defaults);
    int type_idx;

    if (!cfg || !cfg->children)
        return 0;

    type_idx = get_int_property(cfg, "id");
    if (type_idx < 0 || type_idx >= actor_part_sizes[ACTOR_SHIELD_SIZE])
    {
        std::cerr << "Unable to find id/property node " << cfg->name << '\n';
        return 0;
    }

    if (!act->shield)
    {
        int i;
        act->shield = all_shields + all_shields_used;
        all_shields_used += actor_part_sizes[ACTOR_SHIELD_SIZE];
        for (i = actor_part_sizes[ACTOR_SHIELD_SIZE]; i--;)
        {
            act->shield[i].mesh_index = -1;
            act->shield[i].missile_type = -1;
        }
    }

    return parse_actor_shield_part(act, act->shield + type_idx, cfg->children,
                                   default_node);
}

#ifdef NEW_SOUND
static int get_sound_reg_index(const std::string& sound,
                               const std::string& sound_scale)
{
    std::string key = sound + '|' + sound_scale;
    auto iter = sound_regs_known.find(key);
    if (iter != sound_regs_known.end())
        return iter->second;

    int idx = sound_regs.size();
    sound_regs.emplace_back(sound, sound_scale);
    sound_regs_known.emplace(key, idx);

    return idx;
}
#endif

static int parse_actor_weapon_detail(actor_types *act, weapon_part *weapon,
                                     const xmlNode *cfg, const xmlNode *defaults)
{
    const xmlNode *item;
    char name[256];
    int ok, index;

    if (!cfg || !cfg->children)
        return 0;

    ok = 1;
    for (item = cfg->children; item; item = item->next)
    {
        if (item->type == XML_ELEMENT_NODE)
        {
            strncpy(name, (const char*)item->name, sizeof(name));
            my_tolower(name);

            if (!strcmp(name, "mesh"))
            {
                weapon->model_name = char_ptr_of(value(item));
            }
            else if (!strcmp(name, "skin"))
            {
                weapon->skin_name = char_ptr_of(value(item));
            }
            else if (!strcmp(name, "skinmask"))
            {
                weapon->skin_mask = char_ptr_of(value(item));
            }
            else if (!strcmp(name, "glow"))
            {
                int mode = glow_mode_dict.at(lc_value(item));
                weapon->glow = mode < 0 ? GLOW_NONE : mode;
#ifdef NEW_SOUND
            }
            else if (!strcmp(name, "snd_attack_up1"))
            {
                weapon->cal_frames[cal_weapon_attack_up_1_frame].sound
                    = get_sound_reg_index(value(item), property(item, "sound_scale"));
            }
            else if (!strcmp(name, "snd_attack_up2"))
            {
                weapon->cal_frames[cal_weapon_attack_up_2_frame].sound
                    = get_sound_reg_index(value(item), property(item, "sound_scale"));
            }
            else if (!strcmp(name, "snd_attack_up3"))
            {
                weapon->cal_frames[cal_weapon_attack_up_3_frame].sound
                    = get_sound_reg_index(value(item), property(item, "sound_scale"));
            }
            else if (!strcmp(name, "snd_attack_up4"))
            {
                weapon->cal_frames[cal_weapon_attack_up_4_frame].sound
                    = get_sound_reg_index(value(item), property(item, "sound_scale"));
            }
            else if (!strcmp(name, "snd_attack_up5"))
            {
                weapon->cal_frames[cal_weapon_attack_up_5_frame].sound
                    = get_sound_reg_index(value(item), property(item, "sound_scale"));
            }
            else if (!strcmp(name, "snd_attack_up6"))
            {
                weapon->cal_frames[cal_weapon_attack_up_6_frame].sound
                    = get_sound_reg_index(value(item), property(item, "sound_scale"));
            }
            else if (!strcmp(name, "snd_attack_up7"))
            {
                weapon->cal_frames[cal_weapon_attack_up_7_frame].sound
                    = get_sound_reg_index(value(item), property(item, "sound_scale"));
            }
            else if (!strcmp(name, "snd_attack_up8"))
            {
                weapon->cal_frames[cal_weapon_attack_up_8_frame].sound
                    = get_sound_reg_index(value(item), property(item, "sound_scale"));
            }
            else if (!strcmp(name, "snd_attack_up9"))
            {
                weapon->cal_frames[cal_weapon_attack_up_9_frame].sound
                    = get_sound_reg_index(value(item), property(item, "sound_scale"));
            }
            else if (!strcmp(name, "snd_attack_up10"))
            {
                weapon->cal_frames[cal_weapon_attack_up_10_frame].sound
                    = get_sound_reg_index(value(item), property(item, "sound_scale"));
            }
            else if (!strcmp(name, "snd_attack_down1"))
            {
                weapon->cal_frames[cal_weapon_attack_down_1_frame].sound
                    = get_sound_reg_index(value(item), property(item, "sound_scale"));
            }
            else if (!strcmp(name, "snd_attack_down2"))
            {
                weapon->cal_frames[cal_weapon_attack_down_2_frame].sound
                    = get_sound_reg_index(value(item), property(item, "sound_scale"));
            }
            else if (!strcmp(name, "snd_attack_down3"))
            {
                weapon->cal_frames[cal_weapon_attack_down_3_frame].sound
                    = get_sound_reg_index(value(item), property(item, "sound_scale"));
            }
            else if (!strcmp(name, "snd_attack_down4"))
            {
                weapon->cal_frames[cal_weapon_attack_down_4_frame].sound
                    = get_sound_reg_index(value(item), property(item, "sound_scale"));
            }
            else if (!strcmp(name, "snd_attack_down5"))
            {
                weapon->cal_frames[cal_weapon_attack_down_5_frame].sound
                    = get_sound_reg_index(value(item), property(item, "sound_scale"));
            }
            else if (!strcmp(name, "snd_attack_down6"))
            {
                weapon->cal_frames[cal_weapon_attack_down_6_frame].sound
                    = get_sound_reg_index(value(item), property(item, "sound_scale"));
            }
            else if (!strcmp(name, "snd_attack_down7"))
            {
                weapon->cal_frames[cal_weapon_attack_down_7_frame].sound
                    = get_sound_reg_index(value(item), property(item, "sound_scale"));
            }
            else if (!strcmp(name, "snd_attack_down8"))
            {
                weapon->cal_frames[cal_weapon_attack_down_8_frame].sound
                    = get_sound_reg_index(value(item), property(item, "sound_scale"));
            }
            else if (!strcmp(name, "snd_attack_down9"))
            {
                weapon->cal_frames[cal_weapon_attack_down_9_frame].sound
                    = get_sound_reg_index(value(item), property(item, "sound_scale"));
            }
            else if (!strcmp(name, "snd_attack_down10"))
            {
                weapon->cal_frames[cal_weapon_attack_down_10_frame].sound
                    = get_sound_reg_index(value(item), property(item, "sound_scale"));
#endif	//NEW_SOUND
            }
            else
            {
                index = -1;
                if (!strcmp(name, "cal_attack_up1"))
                {
                    index = cal_weapon_attack_up_1_frame;
                }
                else if (!strcmp(name, "cal_attack_up2"))
                {
                    index = cal_weapon_attack_up_2_frame;
                }
                else if (!strcmp(name, "cal_attack_up3"))
                {
                    index = cal_weapon_attack_up_3_frame;
                }
                else if (!strcmp(name, "cal_attack_up4"))
                {
                    index = cal_weapon_attack_up_4_frame;
                }
                else if (!strcmp(name, "cal_attack_up5"))
                {
                    index = cal_weapon_attack_up_5_frame;
                }
                else if (!strcmp(name, "cal_attack_up6"))
                {
                    index = cal_weapon_attack_up_6_frame;
                }
                else if (!strcmp(name, "cal_attack_up7"))
                {
                    index = cal_weapon_attack_up_7_frame;
                }
                else if (!strcmp(name, "cal_attack_up8"))
                {
                    index = cal_weapon_attack_up_8_frame;
                }
                else if (!strcmp(name, "cal_attack_up9"))
                {
                    index = cal_weapon_attack_up_9_frame;
                }
                else if (!strcmp(name, "cal_attack_up10"))
                {
                    index = cal_weapon_attack_up_10_frame;
                }
                else if (!strcmp(name, "cal_attack_down1"))
                {
                    index = cal_weapon_attack_down_1_frame;
                }
                else if (!strcmp(name, "cal_attack_down2"))
                {
                    index = cal_weapon_attack_down_2_frame;
                }
                else if (!strcmp(name, "cal_attack_down3"))
                {
                    index = cal_weapon_attack_down_3_frame;
                }
                else if (!strcmp(name, "cal_attack_down4"))
                {
                    index = cal_weapon_attack_down_4_frame;
                }
                else if (!strcmp(name, "cal_attack_down5"))
                {
                    index = cal_weapon_attack_down_5_frame;
                }
                else if (!strcmp(name, "cal_attack_down6"))
                {
                    index = cal_weapon_attack_down_6_frame;
                }
                else if (!strcmp(name, "cal_attack_down7"))
                {
                    index = cal_weapon_attack_down_7_frame;
                }
                else if (!strcmp(name, "cal_attack_down8"))
                {
                    index = cal_weapon_attack_down_8_frame;
                }
                else if (!strcmp(name, "cal_attack_down9"))
                {
                    index = cal_weapon_attack_down_9_frame;
                }
                else if (!strcmp(name, "cal_attack_down10"))
                {
                    index = cal_weapon_attack_down_10_frame;
                }
                else if (!strcmp(name, "cal_attack_up1_held"))
                {
                    index = cal_weapon_attack_up_1_held_frame;
                }
                else if (!strcmp(name, "cal_attack_up2_held"))
                {
                    index = cal_weapon_attack_up_2_held_frame;
                }
                else if (!strcmp(name, "cal_attack_up3_held"))
                {
                    index = cal_weapon_attack_up_3_held_frame;
                }
                else if (!strcmp(name, "cal_attack_up4_held"))
                {
                    index = cal_weapon_attack_up_4_held_frame;
                }
                else if (!strcmp(name, "cal_attack_up5_held"))
                {
                    index = cal_weapon_attack_up_5_held_frame;
                }
                else if (!strcmp(name, "cal_attack_up6_held"))
                {
                    index = cal_weapon_attack_up_6_held_frame;
                }
                else if (!strcmp(name, "cal_attack_up7_held"))
                {
                    index = cal_weapon_attack_up_7_held_frame;
                }
                else if (!strcmp(name, "cal_attack_up8_held"))
                {
                    index = cal_weapon_attack_up_8_held_frame;
                }
                else if (!strcmp(name, "cal_attack_up9_held"))
                {
                    index = cal_weapon_attack_up_9_held_frame;
                }
                else if (!strcmp(name, "cal_attack_up10_held"))
                {
                    index = cal_weapon_attack_up_10_held_frame;
                }
                else if (!strcmp(name, "cal_attack_down1_held"))
                {
                    index = cal_weapon_attack_down_1_held_frame;
                }
                else if (!strcmp(name, "cal_attack_down2_held"))
                {
                    index = cal_weapon_attack_down_2_held_frame;
                }
                else if (!strcmp(name, "cal_attack_down3_held"))
                {
                    index = cal_weapon_attack_down_3_held_frame;
                }
                else if (!strcmp(name, "cal_attack_down4_held"))
                {
                    index = cal_weapon_attack_down_4_held_frame;
                }
                else if (!strcmp(name, "cal_attack_down5_held"))
                {
                    index = cal_weapon_attack_down_5_held_frame;
                }
                else if (!strcmp(name, "cal_attack_down6_held"))
                {
                    index = cal_weapon_attack_down_6_held_frame;
                }
                else if (!strcmp(name, "cal_attack_down7_held"))
                {
                    index = cal_weapon_attack_down_7_held_frame;
                }
                else if (!strcmp(name, "cal_attack_down8_held"))
                {
                    index = cal_weapon_attack_down_8_held_frame;
                }
                else if (!strcmp(name, "cal_attack_down9_held"))
                {
                    index = cal_weapon_attack_down_9_held_frame;
                }
                else if (!strcmp(name, "cal_attack_down10_held"))
                {
                    index = cal_weapon_attack_down_10_held_frame;
                }
                else if (!strcmp(name, "cal_range_fire"))
                {
                    index = cal_weapon_range_fire_frame;
                }
                else if (!strcmp(name, "cal_range_fire_out"))
                {
                    index = cal_weapon_range_fire_out_frame;
                }
                else if (!strcmp(name, "cal_range_idle"))
                {
                    index = cal_weapon_range_idle_frame;
                }
                else if (!strcmp(name, "cal_range_in"))
                {
                    index = cal_weapon_range_in_frame;
                }
                else if (!strcmp(name, "cal_range_out"))
                {
                    index = cal_weapon_range_out_frame;
                }
                else if (!strcmp(name, "cal_range_fire_held"))
                {
                    index = cal_weapon_range_fire_held_frame;
                }
                else if (!strcmp(name, "cal_range_fire_out_held"))
                {
                    index = cal_weapon_range_fire_out_held_frame;
                }
                else if (!strcmp(name, "cal_range_idle_held"))
                {
                    index = cal_weapon_range_idle_held_frame;
                }
                else if (!strcmp(name, "cal_range_in_held"))
                {
                    index = cal_weapon_range_in_held_frame;
                }
                else if (!strcmp(name, "cal_range_out_held"))
                {
                    index = cal_weapon_range_out_held_frame;
                }

                if (index > -1)
                {
                    weapon->cal_frames[index].anim_index
                        = get_frames_reg_index(value(item),
#ifdef NEW_SOUND
                                               property(item, "sound"),
                                               property(item, "sound_scale"),
#endif	//NEW_SOUND
                                               get_int_property(item, "duration"));
                }
                else
                {
                    std::cerr << "unknown weapon property \"" << item->name << "\"\n";
                    ok = 0;
                }
            }
        }
        else if (item->type == XML_ENTITY_REF_NODE)
        {
            ok &= parse_actor_weapon_detail(act, weapon, item->children, defaults);
        }
    }

    return ok;
}

int parse_actor_weapon(actor_types *act, const xmlNode *cfg, const xmlNode *defaults)
{
    int ok, type_idx;
    weapon_part *weapon;

    if (!cfg || !cfg->children)
        return 0;

    type_idx = get_int_property(cfg, "id");
    if (type_idx < 0 || type_idx >= actor_part_sizes[ACTOR_WEAPON_SIZE])
    {
        std::cerr << "Unable to find id/property node " << cfg->name << '\n';
        return 0;
    }

    if (!act->weapon)
    {
        int i, j;
        act->weapon = all_weapons + all_weapons_used;
        all_weapons_used += actor_part_sizes[ACTOR_WEAPON_SIZE];
        for (i = 0; i < actor_part_sizes[ACTOR_WEAPON_SIZE]; ++i)
        {
            act->weapon[i].mesh_index = -1;
            for (j = 0; j < NUM_WEAPON_FRAMES; j++)
            {
                act->weapon[i].cal_frames[j].anim_index = -1;
#ifdef NEW_SOUND
                act->weapon[i].cal_frames[j].sound = -1;
#endif // NEW_SOUND
            }
        }
    }

    weapon = act->weapon + type_idx;
    ok = parse_actor_weapon_detail(act, weapon, cfg, defaults);
    weapon->turn_horse = get_int_property(cfg, "turn_horse");
    weapon->unarmed = (get_int_property(cfg, "unarmed") <= 0) ? 0 : 1;

    // check for default entries, if found, use them to fill in missing data
    if (defaults)
    {
        const xmlNode *default_node = get_default_node(cfg, defaults);
        if (default_node)
        {
            if (!weapon->skin_name || *weapon->skin_name=='\0')
                weapon->skin_name = char_ptr_of(item_value(default_node, "skin"));
            if (type_idx != GLOVE_FUR && type_idx != GLOVE_LEATHER)
            {   // these dont have meshes
                if (!weapon->model_name || *weapon->model_name=='\0')
                    weapon->model_name = char_ptr_of(item_value(default_node, "mesh"));
            }
            // TODO: combat animations
        }
    }

    // check the critical information
    if (type_idx != WEAPON_NONE)
    {   // no weapon doesn't have a skin/model
        actor_check_string(act, "weapon", "skin", weapon->skin_name);
        if (type_idx != GLOVE_FUR && type_idx != GLOVE_LEATHER)
        {   // these dont have meshes
            actor_check_string(act, "weapon", "model", weapon->model_name);
        }
        // TODO: check combat animations
    }

    return ok;
}

int parse_actor_attachment(actor_types *act, const xmlNode *cfg, int actor_type)
{
    const xmlNode *item;
    int ok = 1;
    attached_actors_types *att = attached_actors_defs + act->actor_type;
    actor_types *held_act = NULL;

    if (!cfg || !cfg->children)
        return 0;

    for (item = cfg->children; item; item = item->next)
    {
        if (item->type == XML_ELEMENT_NODE)
        {
            if (xmlStrcasecmp(item->name, (xmlChar*)"holder") == 0)
            {
                att->actor_type[actor_type].is_holder = get_bool_value(item);
                if (att->actor_type[actor_type].is_holder)
                    held_act = actors_defs + actor_type;
                else
                    held_act = act;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"parent_bone") == 0)
            {
                parent_regs.emplace_back(value(item), act - actors_defs, actor_type);
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"local_bone") == 0)
            {
                local_regs.emplace_back(value(item), act - actors_defs, actor_type);
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"held_shift") == 0)
            {
                xmlAttr *attr;
                for (attr = item->properties; attr; attr = attr->next)
                {
                    if (attr->type == XML_ATTRIBUTE_NODE)
                    {
                        if (xmlStrcasecmp(attr->name, (xmlChar*)"x") == 0)
                        {
                            att->actor_type[actor_type].shift[0] = atof((char*)attr->children->content);
                        }
                        else if (xmlStrcasecmp(attr->name, (xmlChar*)"y") == 0)
                        {
                            att->actor_type[actor_type].shift[1] = atof((char*)attr->children->content);
                        }
                        else if (xmlStrcasecmp(attr->name, (xmlChar*)"z") == 0)
                        {
                            att->actor_type[actor_type].shift[2] = atof((char*)attr->children->content);
                        }
                        else
                        {
                            std::cerr << "unknown attachment shift attribute \""
                                << attr->name << "\"\n";
                            ok = 0;
                        }
                    }
                }
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_held_walk") == 0)
            {
                attached_frames_regs.emplace_back(value(item),
#ifdef NEW_SOUND
                                                  property(item, "sound"),
                                                  property(item, "sound_scale"),
#endif
                                                  get_int_property(item, "duration"),
                                                  actor_type,
                                                  held_act - actors_defs,
                                                  cal_attached_walk_frame);
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_held_run") == 0)
            {
                attached_frames_regs.emplace_back(value(item),
#ifdef NEW_SOUND
                                                  property(item, "sound"),
                                                  property(item, "sound_scale"),
#endif
                                                  get_int_property(item, "duration"),
                                                  actor_type,
                                                  held_act - actors_defs,
                                                  cal_attached_run_frame);
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_held_idle") == 0)
            {
                attached_frames_regs.emplace_back(value(item),
#ifdef NEW_SOUND
                                                  property(item, "sound"),
                                                  property(item, "sound_scale"),
#endif
                                                  get_int_property(item, "duration"),
                                                  actor_type,
                                                  held_act - actors_defs,
                                                  cal_attached_idle_frame);
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_held_pain") == 0)
            {
                attached_frames_regs.emplace_back(value(item),
#ifdef NEW_SOUND
                                                  property(item, "sound"),
                                                  property(item, "sound_scale"),
#endif
                                                  get_int_property(item, "duration"),
                                                  actor_type,
                                                  held_act - actors_defs,
                                                  cal_attached_pain_frame);
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"CAL_held_armed_pain") == 0)
            {
                attached_frames_regs.emplace_back(value(item),
#ifdef NEW_SOUND
                                                  property(item, "sound"),
                                                  property(item, "sound_scale"),
#endif
                                                  get_int_property(item, "duration"),
                                                  actor_type,
                                                  held_act - actors_defs,
                                                  cal_attached_pain_armed_frame);
            }
            else
            {
                std::cerr << "unknown attachment property \"" << item->name << "\"\n";
                ok = 0;
            }
        }
        else if (item->type == XML_ENTITY_REF_NODE)
        {
            ok &= parse_actor_attachment(act, item->children, actor_type);
        }
    }

    return ok;
}

#ifdef NEW_SOUND
int parse_actor_sounds(actor_types *act, const xmlNode *cfg)
{
    const xmlNode *item;
    int ok;

    if (!cfg) return 0;

    ok = 1;
    for (item = cfg; item; item = item->next)
    {
        if (item->type == XML_ELEMENT_NODE)
        {
            if (xmlStrcasecmp(item->name, (xmlChar*)"walk") == 0)
            {
                act->cal_frames[cal_actor_walk_frame].sound
                    = get_sound_reg_index(value(item), property(item, "sound_scale"));
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"run") == 0)
            {
                act->cal_frames[cal_actor_run_frame].sound
                    = get_sound_reg_index(value(item), property(item, "sound_scale"));
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"die1") == 0)
            {
                act->cal_frames[cal_actor_die1_frame].sound
                    = get_sound_reg_index(value(item), property(item, "sound_scale"));
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"die2") == 0)
            {
                act->cal_frames[cal_actor_die2_frame].sound
                    = get_sound_reg_index(value(item), property(item, "sound_scale"));
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"pain1") == 0)
            {
                act->cal_frames[cal_actor_pain1_frame].sound
                    = get_sound_reg_index(value(item), property(item, "sound_scale"));
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"pain2") == 0)
            {
                act->cal_frames[cal_actor_pain2_frame].sound
                    = get_sound_reg_index(value(item), property(item, "sound_scale"));
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"pick") == 0)
            {
                act->cal_frames[cal_actor_pick_frame].sound
                    = get_sound_reg_index(value(item), property(item, "sound_scale"));
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"drop") == 0)
            {
                act->cal_frames[cal_actor_drop_frame].sound
                    = get_sound_reg_index(value(item), property(item, "sound_scale"));
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"harvest") == 0)
            {
                act->cal_frames[cal_actor_harvest_frame].sound
                    = get_sound_reg_index(value(item), property(item, "sound_scale"));
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"attack_cast") == 0)
            {
                act->cal_frames[cal_actor_attack_cast_frame].sound
                    = get_sound_reg_index(value(item), property(item, "sound_scale"));
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"attack_ranged") == 0)
            {
                act->cal_frames[cal_actor_attack_ranged_frame].sound
                    = get_sound_reg_index(value(item), property(item, "sound_scale"));
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"sit_down") == 0)
            {
                act->cal_frames[cal_actor_sit_down_frame].sound
                    = get_sound_reg_index(value(item), property(item, "sound_scale"));
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"stand_up") == 0)
            {
                act->cal_frames[cal_actor_stand_up_frame].sound
                    = get_sound_reg_index(value(item), property(item, "sound_scale"));
            // These sounds are only found in the <sounds> block as they aren't tied to an animation
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"battlecry") == 0)
            {
                act->battlecry.sound
                    = get_sound_reg_index(value(item), property(item, "sound_scale"));
            }
            else
            {
                std::cerr << "Unknown sound \"" << item->name << "\"\n";
                ok = 0;
            }
        }
    }

    return ok;
}
#endif	//NEW_SOUND

static int get_skeleton_index(const std::string& name)
{
    auto iter = std::find(skeleton_names.begin(), skeleton_names.end(), name);
    if (iter != skeleton_names.end())
        return iter - skeleton_names.begin();
    skeleton_names.push_back(name);
    return skeleton_names.size() - 1;
}

int parse_actor_nodes(actor_types *act, const xmlNode *cfg,
                      const xmlNode *defaults)
{
    char name[256];
    const xmlNode *item;
    int ok= 1;

    for (item=cfg->children; item; item=item->next)
    {
        if (item->type == XML_ELEMENT_NODE)
        {
            strncpy(name, (const char*)item->name, sizeof(name));
            my_tolower(name);

            if (!strcmp(name, "ghost"))
            {
                act->ghost = get_bool_value(item);
            }
            else if (!strcmp(name, "skin"))
            {
                act->skin_name = char_ptr_of(value(item));
            }
            else if (!strcmp(name, "mesh"))
            {
                act->file_name = char_ptr_of(value(item));
            }
            else if (!strcmp(name, "actor_scale"))
            {
                act->actor_scale = get_float_value(item);
            }
            else if (!strcmp(name, "scale"))
            {
                act->scale = get_float_value(item);
            }
            else if (!strcmp(name, "mesh_scale"))
            {
                act->mesh_scale = get_float_value(item);
            }
            else if (!strcmp(name, "bone_scale"))
            {
                act->skel_scale = get_float_value(item);
            }
            else if (!strcmp(name, "skeleton"))
            {
                act->skeleton_type = get_skeleton_index(value(item));
            }
            else if (!strcmp(name, "walk_speed"))
            { // unused
                act->walk_speed= get_float_value(item);
            }
            else if (!strcmp(name, "run_speed"))
            { // unused
                act->run_speed = get_float_value(item);
            }
            else if (!strcmp(name, "step_duration"))
            {
                act->step_duration = get_int_value(item);
            }
            else if (!strcmp(name, "defaults"))
            {
                defaults = item;
            }
            else if (!strcmp(name, "frames"))
            {
                ok &= parse_actor_frames(act, item->children);
            }
            else if (!strcmp(name, "shirt"))
            {
                ok &= parse_actor_shirt(act, item, defaults);
            }
            else if (!strcmp(name, "hskin"))
            {
                ok &= parse_actor_skin(act, item, defaults);
            }
            else if (!strcmp(name, "hair"))
            {
                ok &= parse_actor_hair(act, item);
            }
            else if (!strcmp(name, "eyes"))
            {
                ok &= parse_actor_eyes(act, item);
            }
            else if (!strcmp(name, "boots"))
            {
                ok &= parse_actor_boots(act, item, defaults);
            }
            else if (!strcmp(name, "legs"))
            {
                ok &= parse_actor_legs(act, item, defaults);
            }
            else if (!strcmp(name, "cape"))
            {
                ok &= parse_actor_cape(act, item, defaults);
            }
            else if (!strcmp(name, "head"))
            {
                ok &= parse_actor_head(act, item, defaults);
            }
            else if (!strcmp(name, "shield"))
            {
                ok &= parse_actor_shield(act, item, defaults);
            }
            else if (!strcmp(name, "weapon"))
            {
                ok &= parse_actor_weapon(act, item, defaults);
            }
            else if (!strcmp(name, "helmet"))
            {
                ok &= parse_actor_helmet(act, item, defaults);
            }
            else if (!strcmp(name, "neck"))
            {
                ok &= parse_actor_neck(act, item, defaults);
            }
#ifdef NEW_SOUND
            else if (!strcmp(name, "sounds"))
            {
                ok &= parse_actor_sounds(act, item->children);
            }
#endif	//NEW_SOUND
            else if (!strcmp(name, "actor_attachment"))
            {
                int id = get_int_property(item, "id");
                if (id < 0 || id >= MAX_ACTOR_DEFS)
                {
                    std::cerr << "Unable to find id/property node " << item->name << '\n';
                    ok = 0;
                }
                else
                {
                    ok &= parse_actor_attachment(act, item, id);
                }
            }
            else
            {
                std::cerr << "Unknown actor attribute \"" << item->name << "\"\n";
                ok = 0;
            }
        }
        else if (item->type == XML_ENTITY_REF_NODE)
        {
            ok &= parse_actor_nodes(act, item->children, defaults);
        }
    }
    return ok;
}

int parse_actor_script(const xmlNode *cfg)
{
    int ok, act_idx, i;
    int j;
    actor_types *act;

    if (!cfg || !cfg->children)
        return 0;

    act_idx = get_int_property(cfg, "id");
    if (act_idx < 0 || act_idx >= MAX_ACTOR_DEFS)
    {
        std::cerr << "Data Error in " << property(cfg, "type")
            << '(' << act_idx << "): Actor ID " << act_idx << " out of range\n";
        return 0;
    }

    act = actors_defs + act_idx;
    // watch for loading an actor more then once
    if (act->actor_type > 0 || act->actor_name)
    {
        std::cerr << "Data Error in " << property(cfg, "type")
            << '(' << act_idx << "): Already loaded " << act->actor_name
            << '(' << act->actor_type << ")\n";
    }
    act->actor_type= act_idx;	// memorize the ID & name to help in debugging
    act->actor_name = char_ptr_of(property(cfg, "type"));
    actor_check_string(act, "actor", "name", act->actor_name);

    //Initialize Cal3D settings
    act->coremodel = NULL;
    act->skeleton_type = -1;
    act->actor_scale = 1.0;
    act->scale = 1.0;
    act->mesh_scale = 1.0;
    act->skel_scale = 1.0;
    act->group_count = 0;
    for (i = 0; i < 16; ++i)
    {
        act->idle_group[i].name = NULL;
        act->idle_group[i].count= 0;
    }

    for (i = 0; i < NUM_ACTOR_FRAMES; i++)
    {
        act->cal_frames[i].anim_index = -1;
#ifdef NEW_SOUND
        act->cal_frames[i].sound = -1;
#endif // NEW_SOUND
    }
#ifdef NEW_SOUND
    act->battlecry.sound = -1;
#endif // NEW_SOUND

    for (i = 0; i < MAX_ACTOR_DEFS; ++i)
    {
        for (j = 0; j < NUM_ATTACHED_ACTOR_FRAMES; j++)
        {
            attached_actors_defs[act_idx].actor_type[i].cal_frames[j].anim_index = -1;
#ifdef NEW_SOUND
            attached_actors_defs[act_idx].actor_type[i].cal_frames[j].sound = -1;
#endif // NEW_SOUND
        }
    }

    act->step_duration = DEFAULT_STEP_DURATION; // default value

    ok = parse_actor_nodes(act, cfg, NULL);

    // TODO: add error checking for missing actor information

    // If this not an enhanced actor, reserve space for the single mesh
    if ((!act->head || !*act->head[0].model_name) && !act->shirt)
    {
        act->shirt = all_shirts + all_shirts_used;
        all_shirts_used += 1; // actor_part_sizes[ACTOR_SHIRT_SIZE];
    }

    return ok;
}

int parse_actor_defs(const xmlNode *node)
{
    const xmlNode *def;
    int ok = 1;

    for (def = node->children; def; def = def->next)
    {
        if (def->type == XML_ELEMENT_NODE)
        {
            if (xmlStrcasecmp(def->name, (xmlChar*)"actor") == 0)
            {
                ok &= parse_actor_script(def);
            }
            else
            {
                std::cerr << "parse error: actor or include expected\n";
                ok = 0;
            }
        }
        else if (def->type == XML_ENTITY_REF_NODE)
        {
            ok &= parse_actor_defs(def->children);
        }
    }

    return ok;
}

static int read_actor_defs(const char *dir, const char *index)
{
    const xmlNode *root;
    xmlDoc *doc;
    char fname[120];
    int ok = 1;

    snprintf(fname, sizeof(fname), "%s/%s", dir, index);

    doc = xmlReadFile(fname, NULL, XML_PARSE_NOENT);
    if (!doc)
    {
        std::cerr << "Unable to read actor definition file " << fname << '\n';
        return 0;
    }

    root = xmlDocGetRootElement(doc);
    if (!root)
    {
        std::cerr << "Unable to parse actor definition file " << fname << '\n';
        ok = 0;
#ifndef EXT_ACTOR_DICT
    }
    else if (xmlStrcasecmp(root->name, (xmlChar*)"actors") != 0)
    {
        std::cerr << "Unknown key \"" << root->name << "\" (\"actors\" expected).\n";
#else // EXT_ACTOR_DICT
    }
    else if (xmlStrcasecmp(root->name, (xmlChar*)"actor_data") != 0)
    {
        std::cerr << "Unknown key \"" << root->name << "\" (\"actor_data\" expected).\n";
#endif // EXT_ACTOR_DICT
        ok = 0;
    }
    else
    {
#ifndef EXT_ACTOR_DICT
        ok = parse_actor_defs(root);
#else // EXT_ACTOR_DICT
        ok = parse_actor_data(root);
#endif // EXT_ACTOR_DICT
    }

    xmlFreeDoc(doc);
    return ok;
}

static int init_actor_defs(const char* dir)
{
    int nr_body = actor_part_sizes[ACTOR_HEAD_SIZE]
                + actor_part_sizes[ACTOR_CAPE_SIZE]
                + actor_part_sizes[ACTOR_HELMET_SIZE]
                + actor_part_sizes[ACTOR_NECK_SIZE]
                + actor_part_sizes[ACTOR_BOOTS_SIZE]
                + actor_part_sizes[ACTOR_LEGS_SIZE];
    int nr_shields = actor_part_sizes[ACTOR_SHIELD_SIZE];
    int nr_weapons = actor_part_sizes[ACTOR_WEAPON_SIZE];
    int nr_shirts = actor_part_sizes[ACTOR_SHIRT_SIZE];
    int nr_skins = actor_part_sizes[ACTOR_SKIN_SIZE];
    int nr_hairs = actor_part_sizes[ACTOR_HAIR_SIZE];
    int nr_eyes = actor_part_sizes[ACTOR_EYES_SIZE];

    // initialize the whole thing to zero
    std::memset(actors_defs, 0, sizeof(actors_defs));
    std::memset(attached_actors_defs, 0, sizeof(attached_actors_defs));

    all_body = new body_part[MAX_ACTOR_DEFS * nr_body];
    all_shields = new shield_part[MAX_ACTOR_DEFS * nr_shields];
    all_weapons = new weapon_part[MAX_ACTOR_DEFS * nr_weapons];
    all_shirts = new shirt_part[MAX_ACTOR_DEFS * nr_shirts];
    all_skins = new skin_part[MAX_ACTOR_DEFS * nr_skins];
    all_hairs = new hair_part[MAX_ACTOR_DEFS * nr_hairs];
    all_eyes = new eyes_part[MAX_ACTOR_DEFS * nr_eyes];

    std::memset(all_body, 0, MAX_ACTOR_DEFS * nr_body * sizeof(body_part));
    std::memset(all_shields, 0, MAX_ACTOR_DEFS * nr_shields * sizeof(shield_part));
    std::memset(all_weapons, 0, MAX_ACTOR_DEFS * nr_weapons * sizeof(weapon_part));
    std::memset(all_shirts, 0, MAX_ACTOR_DEFS * nr_shirts * sizeof(shirt_part));
    std::memset(all_skins, 0, MAX_ACTOR_DEFS * nr_skins * sizeof(skin_part));
    std::memset(all_hairs, 0, MAX_ACTOR_DEFS * nr_hairs * sizeof(hair_part));
    std::memset(all_eyes, 0, MAX_ACTOR_DEFS * nr_eyes * sizeof(eyes_part));

    return read_actor_defs(dir, "actor_defs.xml");
}

std::ostream& operator<<(std::ostream& os, const body_part& part)
{
    const char* model_name = part.model_name ? part.model_name : "";
    const char* skin_name = part.skin_name ? part.skin_name : "";
    const char* skin_mask = part.skin_mask ? part.skin_mask : "";
    const char* glow_name = glow_mode_name(part.glow);

    os << "\t{\n"
        << "\t\t.model_name = \"" << model_name << "\",\n"
        << "\t\t.skin_name = \"" << skin_name << "\",\n"
        << "\t\t.skin_mask = \"" << skin_mask << "\",\n";
    if (glow_name)
        os << "\t\t.glow = " << glow_name << ",\n";
    else
        os << "\t\t.glow = " << part.glow << ",\n";
    os << "\t\t.mesh_index = -1\n"
        << "\t},\n";
    return os;
}

std::ostream& write_actor_body(std::ostream& os)
{
    os << "static body_part actor_body[" << all_body_used << "] = {\n";
    for (int i = 0; i < all_body_used; ++i)
        os << all_body[i];
    return os << "};\n\n";
}

std::ostream &operator<<(std::ostream& os, const shield_part& part)
{
    const char* model_name = part.model_name ? part.model_name : "";
    const char* skin_name = part.skin_name ? part.skin_name : "";
    const char* skin_mask = part.skin_mask ? part.skin_mask : "";
    const char *glow_name = glow_mode_name(part.glow);

    os << "\t{\n"
        << "\t\t.model_name = \"" << model_name << "\",\n"
        << "\t\t.skin_name = \"" << skin_name << "\",\n"
        << "\t\t.skin_mask = \"" << skin_mask << "\",\n";
    if (glow_name)
        os << "\t\t.glow = " << glow_name << ",\n";
    else
        os << "\t\t.glow = " << part.glow << ",\n";
    os << "\t\t.mesh_index = -1,\n"
        << "\t\t.missile_type = " << part.missile_type << "\n"
        << "\t},\n";
    return os;
}

std::ostream& write_actor_shields(std::ostream& os)
{
    os << "static shield_part actor_shields[" << all_shields_used << "] = {\n";
    for (int i = 0; i < all_shields_used; ++i)
        os << all_shields[i];
    return os << "};\n\n";
}

std::ostream& operator<<(std::ostream& os, const weapon_part& part)
{
    const char* model_name = part.model_name ? part.model_name : "";
    const char* skin_name = part.skin_name ? part.skin_name : "";
    const char* skin_mask = part.skin_mask ? part.skin_mask : "";
    const char *glow_name = glow_mode_name(part.glow);

    os << "\t{\n"
        << "\t\t.model_name = \"" << model_name << "\",\n"
        << "\t\t.skin_name = \"" << skin_name << "\",\n"
        << "\t\t.skin_mask = \"" << skin_mask << "\",\n";
    if (glow_name)
        os << "\t\t.glow = " << glow_name << ",\n";
    else
        os << "\t\t.glow = " << part.glow << ",\n";
    os << "\t\t.mesh_index = -1,\n"
        << "\t\t.turn_horse = " << part.turn_horse << ",\n"
        << "\t\t.unarmed = " << part.unarmed << ",\n"
        << "\t\t.cal_frames = {\n";
    for (int i = 0; i < NUM_WEAPON_FRAMES; ++i)
    {
        os << "\t\t\t{ .anim_index = " << part.cal_frames[i].anim_index
            << ", .sound = " << part.cal_frames[i].sound << " },\n";
    }
    return os << "\t\t}\n\t},\n";
}

std::ostream& write_actor_weapons(std::ostream& os)
{
    os << "static weapon_part actor_weapons[" << all_weapons_used << "] = {\n";
    for (int i = 0; i < all_weapons_used; ++i)
        os << all_weapons[i];
    return os << "};\n\n";
}

std::ostream& operator<<(std::ostream& os, const shirt_part& part)
{
    const char* model_name = part.model_name ? part.model_name : "";
    const char* arms_name = part.arms_name ? part.arms_name : "";
    const char* torso_name = part.torso_name ? part.torso_name : "";
    const char* arms_mask = part.arms_mask ? part.arms_mask : "";
    const char* torso_mask = part.torso_mask ? part.torso_mask : "";

    return os << "\t{\n"
        << "\t\t.model_name = \"" << model_name << "\",\n"
        << "\t\t.arms_name = \"" << arms_name << "\",\n"
        << "\t\t.torso_name = \"" << torso_name << "\",\n"
        << "\t\t.arms_mask = \"" << arms_mask << "\",\n"
        << "\t\t.torso_mask = \"" << torso_mask << "\",\n"
        << "\t\t.mesh_index = -1\n"
        << "\t},\n";
}

std::ostream& write_actor_shirts(std::ostream& os)
{
    os << "static shirt_part actor_shirts[" << all_shirts_used << "] = {\n";
    for (int i = 0; i < all_shirts_used; ++i)
        os << all_shirts[i];
    return os << "};\n\n";
}

std::ostream& operator<<(std::ostream& os, const skin_part &part)
{
    const char* hands_name = part.hands_name ? part.hands_name : "";
    const char* head_name = part.head_name ? part.head_name : "";
    const char* arms_name = part.arms_name ? part.arms_name : "";
    const char* body_name = part.body_name ? part.body_name : "";
    const char* legs_name = part.legs_name ? part.legs_name : "";
    const char* feet_name = part.feet_name ? part.feet_name : "";

    return os << "\t{\n"
        << "\t\t.hands_name = \"" << hands_name << "\",\n"
        << "\t\t.head_name = \"" << head_name << "\",\n"
        << "\t\t.arms_name = \"" << arms_name << "\",\n"
        << "\t\t.body_name = \"" << body_name << "\",\n"
        << "\t\t.legs_name = \"" << legs_name << "\",\n"
        << "\t\t.feet_name = \"" << feet_name << "\",\n"
        << "\t},\n";
}

std::ostream& write_actor_skins(std::ostream& os)
{
    os << "static skin_part actor_skins[" << all_skins_used << "] = {\n";
    for (int i = 0; i < all_skins_used; ++i)
        os << all_skins[i];
    return os << "};\n\n";
}

std::ostream& operator<<(std::ostream& os, const hair_part& part)
{
    return os << "\t{\n"
        << "\t\t.hair_name = \""
        << (part.hair_name ? part.hair_name : "") << "\"\n"
        << "\t},\n";
}

std::ostream& write_actor_hairs(std::ostream& os)
{
    os << "static hair_part actor_hairs[" << all_hairs_used << "] = {\n";
    for (int i = 0; i < all_hairs_used; ++i)
        os << all_hairs[i];
    return os << "};\n\n";
}

std::ostream& operator<<(std::ostream& os, const eyes_part& part)
{
    return os << "\t{\n"
        << "\t\t.eyes_name = \""
        << (part.eyes_name ? part.eyes_name : "") << "\",\n"
        << "\t},\n";
}

std::ostream& write_actor_eyes(std::ostream& os)
{
    os << "static eyes_part actor_eyes[" << all_eyes_used << "] = {\n";
    for (int i = 0; i < all_eyes_used; ++i)
        os << all_eyes[i];
    return os << "};\n\n";
}

std::ostream& operator<<(std::ostream& os, const actor_types& act)
{
    const char* type_name = actor_type_name(act.actor_type);

    os << "\t{\n";
    if (type_name)
        os << "\t\t.actor_type = " << type_name << ",\n";
    else
        os << "\t\t.actor_type = " << act.actor_type << ",\n";
    if (act.actor_name)
        os << "\t\t.actor_name = \"" << act.actor_name << "\",\n";
    else
        os << "\t\t.actor_name = \"\",\n";
    if (act.skin_name)
        os << "\t\t.skin_name = \"" << act.skin_name << "\",\n";
    else
        os << "\t\t.skin_name = \"\",\n";
    if (act.file_name)
        os << "\t\t.file_name = \"" << act.file_name << "\",\n";
    else
        os << "\t\t.file_name = \"\",\n";
    os << "\t\t.actor_scale = " << act.actor_scale << ",\n"
        << "\t\t.scale = " << act.scale << ",\n"
        << "\t\t.mesh_scale = " << act.mesh_scale << ",\n"
        << "\t\t.skel_scale = " << act.skel_scale << ",\n"
        << "\t\t.coremodel = NULL,\n"
        << "\t\t.hardware_model = NULL,\n"
        << "\t\t.vertex_buffer = 0,\n"
        << "\t\t.index_buffer = 0,\n"
        << "\t\t.index_type = 0,\n"
        << "\t\t.index_size = 0,\n";
    if (act.group_count == 0)
    {
        os << "\t\t.idle_group = {},\n";
    }
    else
    {
        os << "\t\t.idle_group = {";
        for (int i = 0; i < act.group_count; ++i)
        {
            const cal_animations *group = act.idle_group + i;
            os << "\t\t\t{\n"
                << "\t\t\t\t.name = \"" << group->name << "\",\n"
                << "\t\t\t\t.count = " << group->count << ",\n";
            if (group->count == 0)
            {
                os << "\t\t\t\t.anim = {}\n";
            }
            else
            {
                os << "\t\t\t\t.anim = {\n";
                for (int j = 0; j < group->count; ++j)
                {
                    os << "\t\t\t\t\t{ .anim_index = "
                        << group->anim[j].anim_index << " },\n";
                }
                os << "\t\t\t\t}\n";
            }
            os << "\t\t\t},\n";
        }
        os << "\t\t},\n";
    }
    os << "\t\t.group_count = " << act.group_count << ",\n"
        << "\t\t.cal_frames = {\n";
    for (int i = 0; i < NUM_ACTOR_FRAMES; ++i)
    {
        os << "\t\t\t{ .anim_index = " << act.cal_frames[i].anim_index
#ifdef NEW_SOUND
            << ", .sound = " << act.cal_frames[i].sound
#endif
            << " },\n";
    }
    os << "\t\t},\n"
        << "\t\t.emote_frames = NULL,\n"
        << "\t\t.skeleton_type = " << act.skeleton_type << ",\n";
#ifdef NEW_SOUND
    os << "\t\t.battlecry = { .sound = " << act.battlecry.sound << " },\n";
#endif
    if (act.head)
        os << "\t\t.head = actor_body + " << (act.head - all_body) << ",\n";
    else
        os << "\t\t.head = NULL,\n";
    if (act.shield)
        os << "\t\t.shield = actor_shields + " << (act.shield - all_shields) << ",\n";
    else
        os << "\t\t.shield = NULL,\n";
    if (act.cape)
        os << "\t\t.cape = actor_body + " << (act.cape - all_body) << ",\n";
    else
        os << "\t\t.cape = NULL,\n";
    if (act.helmet)
        os << "\t\t.helmet = actor_body + " << (act.helmet - all_body) << ",\n";
    else
        os << "\t\t.helmet = NULL,\n";
    if (act.neck)
        os << "\t\t.neck = actor_body + " << (act.neck - all_body) << ",\n";
    else
        os << "\t\t.neck = NULL,\n";
    if (act.weapon)
        os << "\t\t.weapon = actor_weapons + " << (act.weapon - all_weapons) << ",\n";
    else
        os << "\t\t.weapon = NULL,\n";
    if (act.shirt)
        os << "\t\t.shirt = actor_shirts + " << (act.shirt - all_shirts) << ",\n";
    else
        os << "\t\t.shirt = NULL,\n";
    if (act.skin)
        os << "\t\t.skin = actor_skins + " << (act.skin - all_skins) << ",\n";
    else
        os << "\t\t.skin = NULL,\n";
    if (act.hair)
        os << "\t\t.hair = actor_hairs + " << (act.hair - all_hairs) << ",\n";
    else
        os << "\t\t.hair = NULL,\n";
    if (act.eyes)
        os << "\t\t.eyes = actor_eyes + " << (act.eyes - all_eyes) << ",\n";
    else
        os << "\t\t.eyes = NULL,\n";
    if (act.boots)
        os << "\t\t.boots = actor_body + " << (act.boots - all_body) << ",\n";
    else
        os << "\t\t.boots = NULL,\n";
    if (act.legs)
        os << "\t\t.legs = actor_body + " << (act.legs - all_body) << ",\n";
    else
        os << "\t\t.legs = NULL,\n";
    os << "\t\t.walk_speed = " << act.walk_speed << ",\n"
        << "\t\t.run_speed = " << act.run_speed << ",\n"
        << "\t\t.ghost = " << int(act.ghost) << ",\n"
        << "\t\t.step_duration = " << act.step_duration << "\n"
        << "\t},\n";

    return os;
}

std::ostream& write_actor_types(std::ostream& os, int nr_actor_defs)
{
    os << "actor_types actors_defs[" << nr_actor_defs << "] = {\n";
    for (int i = 0; i < nr_actor_defs; ++i)
        os << actors_defs[i];
    os << "};\n"
        << "attached_actors_types attached_actors_defs[" << nr_actor_defs << "];\n"
        << "const int nr_actor_defs = " << nr_actor_defs << ";\n\n";

    return os;
}

std::ostream& write_idle_group_regs(std::ostream& os)
{
    os << "static const char* idle_group_regs[" << idle_group_regs.size() << "] = {\n";
    for (const auto& reg: idle_group_regs)
        os << "\t\"" << reg << "\",\n";
    return os << "};\n\n";
}

std::ostream& operator<<(std::ostream& os, const frames_reg& reg)
{
    return os << "\t{\n"
        << "\t\t.fname = \"" << reg.fname << "\",\n"
#ifdef NEW_SOUND
        << "\t\t.sound = \"" << reg.sound << "\",\n"
        << "\t\t.sound_scale = \"" << reg.sound_scale << "\",\n"
#endif
        << "\t\t.duration = " << reg.duration << '\n'
        << "\t},\n";
}

std::ostream& write_frames_regs(std::ostream& os)
{
    os << "static const frames_reg frames_regs[" << frames_regs.size() << "] = {\n";
    for (const auto& reg: frames_regs)
        os << reg;
    os << "};\n";

    return os;
}

std::ostream& operator<<(std::ostream& os, const emote_reg& reg)
{
    return os << "\t{\n"
        << "\t\t.fname = \"" << reg.fname << "\",\n"
#ifdef NEW_SOUND
        << "\t\t.sound = \"" << reg.sound << "\",\n"
        << "\t\t.sound_scale = \"" << reg.sound_scale << "\",\n"
#endif
        << "\t\t.duration = " << reg.duration << ",\n"
        << "\t\t.act_idx = " << reg.act_idx << ",\n"
        << "\t\t.frame_idx = " << reg.frame_idx << '\n'
        << "\t},\n";
}

std::ostream& write_emote_regs(std::ostream& os)
{
    os << "static const emote_reg emote_regs[" << emote_regs.size() << "] = {\n";
    for (const auto& reg: emote_regs)
        os << reg;
    os << "};\n"
        << "static struct cal_anim emote_anims[" << emote_regs.size() << "];\n"
        << "static const int nr_emote_regs = " << emote_regs.size() << ";\n\n";
    return os;
}

std::ostream& write_skeleton_names(std::ostream &os)
{
    os << "static const char* skeleton_names[" << skeleton_names.size() << "] = {\n";
    for (const auto& name: skeleton_names)
        os << "\t\"" << name << "\",\n";
    return os << "};\n\n";
}

std::ostream& operator<<(std::ostream& os, const bone_reg& reg)
{
    return os << "\t{\n"
        << "\t\t.name = \"" << reg.name << "\",\n"
        << "\t\t.act_idx = " << reg.act_idx << ",\n"
        << "\t\t.att_act_idx = " << reg.act_idx << "\n"
        << "\t},\n";
}

std::ostream& write_parent_regs(std::ostream& os)
{
    os << "static const bone_reg parent_regs[" << parent_regs.size() << "] = {\n";
    for (const auto& reg: parent_regs)
        os << reg;
    os << "};\n"
        << "static const int nr_parent_regs = " << parent_regs.size() << ";\n\n";
    return os;
}

std::ostream& write_local_regs(std::ostream& os)
{
    os << "static const bone_reg local_regs[" << local_regs.size() << "] = {\n";
    for (const auto& reg: local_regs)
        os << reg;
    os << "};\n"
        << "static const int nr_local_regs = " << local_regs.size() << ";\n\n";
    return os;
}

std::ostream& operator<<(std::ostream& os, const attached_frames_reg& reg)
{
    return os << "\t{\n"
        << "\t\t.fname = \"" << reg.fname << "\",\n"
#ifdef NEW_SOUND
        << "\t\t.sound = \"" << reg.sound << "\",\n"
        << "\t\t.sound_scale = \"" << reg.sound_scale << "\",\n"
#endif
        << "\t\t.duration = " << reg.duration << ",\n"
        << "\t\t.act_idx = " << reg.act_idx << ",\n"
        << "\t\t.held_act_idx = " << reg.held_act_idx << ",\n"
        << "\t\t.frame_idx = " << reg.frame_idx << "\n"
        << "\t},\n";
}

std::ostream& write_attached_frames_regs(std::ostream& os)
{
    os << "static const attached_frames_reg attached_frames_regs["
        << attached_frames_regs.size() << "] = {\n";
    for (const auto& reg: attached_frames_regs)
        os << reg;
    os << "};\n"
        << "static const int nr_attached_frames_regs = " << attached_frames_regs.size()
        << ";\n\n";
    return os;
}

#ifdef NEW_SOUND
std::ostream& operator<<(std::ostream& os, const sound_reg& reg)
{
    return os << "\t{\n"
        << "\t\t.sound = \"" << reg.sound << "\",\n"
        << "\t\t.sound_scale = \"" << reg.sound_scale << "\"\n"
        << "\t},\n";
}

std::ostream& write_sound_regs(std::ostream& os)
{
    os << "static sound_reg sound_regs[" << sound_regs.size() << "] = {\n";
    for (const auto& reg: sound_regs)
        os << reg;
    os << "};\n"
        << "static const int nr_sound_regs = " << sound_regs.size() << ";\n\n";
    return os;
}
#endif

int get_nr_actor_defs()
{
    for (int nr_actor_defs = MAX_ACTOR_DEFS; nr_actor_defs > 0; --nr_actor_defs)
    {
        if (actors_defs[nr_actor_defs-1].actor_name)
            return nr_actor_defs;
    }
    return 0;
}

void write_c_file(std::ostream& os)
{
    int nr_actor_defs = get_nr_actor_defs();

    write_actor_body(os);
    write_actor_shields(os);
    write_actor_weapons(os);
    write_actor_shirts(os);
    write_actor_skins(os);
    write_actor_hairs(os);
    write_actor_eyes(os);
    write_idle_group_regs(os);
    write_frames_regs(os);
    write_emote_regs(os);
    write_parent_regs(os);
    write_local_regs(os);
    write_attached_frames_regs(os);
#ifdef NEW_SOUND
    write_sound_regs(os);
#endif
    write_skeleton_names(os);
    write_actor_types(os, nr_actor_defs);
}

void cleanup()
{
    delete[] all_body;
    delete[] all_shields;
    delete[] all_weapons;
    delete[] all_shirts;
    delete[] all_skins;
    delete[] all_hairs;
    delete[] all_eyes;
}

int main(int argc, const char *argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <directory> <out_file>\n";
        return 1;
    }

    if (!init_actor_defs(argv[1]))
    {
        std::cerr << "Problem reading actor defs\n";
        goto err;
    }

    {
        std::ofstream os(argv[2]);
        if (!os.good())
        {
            std::cerr << "Unable to open output file \"" << argv[2] << "\"\n";
            goto err;
        }
        write_c_file(os);
        os.close();
    }

    cleanup();
    return 0;

err:
    cleanup();
    return 1;
}

