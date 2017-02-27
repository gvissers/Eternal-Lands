#include <fstream>
#include <iostream>
#include <vector>

#include "xml.h"

#include "../items.h"
#include "../stats.h"

#define SIGILS_NO 64
#define SPELLS_NO 32

namespace
{

struct sigil_def
{
    int sigil_img;
    std::string name;
    std::string description;
    int have_sigil;

    sigil_def(): sigil_img(-1), name(), description(), have_sigil(0) {}
};
std::vector<sigil_def> sigils_list(SIGILS_NO);

struct spell_info
{
    int id;//The spell server id
    std::string name;//The spell name
    std::string desc;//The spell description
    int image;//image_id
    int sigils[6];//index of required sigils in sigils_list
    int mana;//required mana
    std::string lvls[NUM_WATCH_STAT];//pointers to your_info lvls
    int lvls_req[NUM_WATCH_STAT];//minimum lvls requirement
    int reagents_id[4]; //reagents needed image id
    std::uint16_t reagents_uid[4]; //reagents needed, unique item id
    int reagents_qt[4]; //their quantities
    std::uint32_t buff;
    int uncastable; //0 if castable, otherwise if something missing

    spell_info(): id(-1), name(), desc(), image(-1),
        sigils{-1, -1, -1, -1, -1, -1}, mana(0), lvls{}, lvls_req{},
        reagents_id{-1, -1, -1, -1},
        reagents_uid{unset_item_uid, unset_item_uid, unset_item_uid, unset_item_uid },
        reagents_qt{0, 0, 0, 0}, buff(0), uncastable(0) {}
};

std::vector<spell_info> spells_list;

struct group_def
{
    std::string desc;
    int spells;
    int spells_id[SPELLS_NO];
    int x, y;

    group_def(): desc(), spells(0), x(0), y(0)
    {
        std::fill(spells_id, spells_id+SPELLS_NO, -1);
    }
};
std::vector<group_def> groups_list;

//returns a node with tagname, starts searching from the_node
const xmlNode *get_XML_node(const xmlNode *start, const char *tagname)
{

    for (const xmlNode *node = start; node; node = node->next)
    {
        if (node->type == XML_ELEMENT_NODE
            && xmlStrcasecmp (node->name, reinterpret_cast<const xmlChar*>(tagname)) == 0) return node;
    }
    return nullptr;
}

int init_spells(const std::string& fname)
{
    xmlDoc *doc = xmlReadFile(fname.c_str(), nullptr, 0);
    if (!doc)
    {
        std::cerr << "Unable to read spells definition file " << fname
            << ": " << strerror(errno)  << '\n';
        return 0;
    }

    xmlNode *root = xmlDocGetRootElement(doc);
    if (!root)
    {
        std::cerr << "Unable to parse spells definition file " << fname << '\n';
        xmlFreeDoc(doc);
        return 0;
    }
    if (xmlStrcasecmp(root->name, (xmlChar*)"Magic") != 0)
    {
        std::cerr << "Unknown key \"" << root->name << "\" (\"Magic\" expected).\n";
        xmlFreeDoc(doc);
        return 0;
    }

    const int expected_version = 1;
    int actual_version = int_property(root, "version");
    if (actual_version < expected_version)
    {
        std::cerr << "Warning: " << fname << " file is out of date expecting "
            << expected_version << ", actual " << actual_version << ".\n";
    }

    // Parse spells
    const xmlNode *node = get_XML_node(root->children, "Spell_list");
    node = get_XML_node(node->children, "spell");
    for (int i = 0; node; node = get_XML_node(node->next, "spell"), ++i)
    {
        const xmlNode *data = get_XML_node(node->children, "name");
        if (!data)
            std::cerr << "No name for " << i << " spell\n";
        std::string name = value(data);
        spells_list.push_back(spell_info());
        spell_info& info = spells_list.back();
        info.name = name;

        data = get_XML_node(node->children, "desc");
        if (!data)
            std::cerr << "No desc for spell '" << name << "'[" << i << "]\n";
        info.desc = value(data);

        data = get_XML_node(node->children, "id");
        if (!data)
            std::cerr << "No id for spell '" << name << "'[" << i << "]\n";
        info.id = int_value(data);

        data=get_XML_node(node->children, "icon");
        if (!data)
            std::cerr << "No icon for spell '" << name << "'[" << i << "]\n";
        info.image = int_value(data);

        data = get_XML_node(node->children, "mana");
        if (!data)
            std::cerr << "No mana for spell '" << name << "'[" << i << "]\n";
        info.mana = int_value(data);

        data = get_XML_node(node->children, "lvl");
        if (!data)
            std::cerr << "No lvl for spell '" << name << "'[" << i << "]\n";
        for (int j = 0; data; data = get_XML_node(data->next, "lvl"), ++j)
        {
            info.lvls_req[j] = int_value(data);
            info.lvls[j] = property(data, "skill");
        }

        data = get_XML_node(node->children, "group");
        if (!data)
            std::cerr << "No group for spell '" << name << "'[" << i << "]\n";
        while (data)
        {
            std::uint32_t g = int_value(data);
            if (g >= groups_list.size())
                groups_list.resize(g+1);
            groups_list[g].spells_id[groups_list[g].spells] = i;
            groups_list[g].spells++;
            data = get_XML_node(data->next, "group");
        }

        data = get_XML_node(node->children, "group");
        if (!data)
            std::cerr << "No sigil for spell '" << name << "'[" << i << "]\n";
        for (int j = 0; data; data = get_XML_node(data->next, "sigil"), ++j)
            info.sigils[j] = int_value(data);

        data = get_XML_node(node->children, "group");
        if (!data)
            std::cerr << "No reagent for spell '" << name << "'[" << i << "]\n";
        for (int j = 0; data; data = get_XML_node(data->next, "reagent"), ++j)
        {
            info.reagents_id[j] = int_property(data, "id");
            int tmpval = int_property(data, "uid");
            if (tmpval >= 0)
                info.reagents_uid[j] = uint16_t(tmpval);
            info.reagents_qt[j] = int_value(data);
        }

        data = get_XML_node(node->children, "buff");
        info.buff = data ? int_value(data) : 0xFFFFFFFF;
    }

    // Parse sigils
    node = get_XML_node(root->children, "Sigil_list");
    node = get_XML_node(node->children, "sigil");
    while (node)
    {
        std::uint32_t k = int_property(node, "id");
        if (k >= sigils_list.size())
            sigils_list.resize(k+1);
        sigils_list[k].sigil_img = k;
        sigils_list[k].description = value(node);
        sigils_list[k].name = property(node, "name");
        sigils_list[k].have_sigil = 1;
        node = get_XML_node(node->next, "sigil");
    }

    // Parse groups
    node = get_XML_node(root->children, "Groups");
    node = get_XML_node(node->children, "group");
    while (node)
    {
        int k = int_property(node, "id");
        groups_list[k].desc = value(node);
        node = get_XML_node(node->next, "group");
    }

    xmlFreeDoc (doc);

    return 1;
}

const char* get_skill_address(const std::string& skillname)
{
    // Let's hope that when someone adds or renames a skill, they remember
    // to update this function.
    if (skillname == "man")
        return "&your_info.manufacturing_skill";
    if (skillname == "alc")
        return "&your_info.alchemy_skill";
    if (skillname == "mag")
        return "&your_info.magic_skill";
    if (skillname == "sum")
        return "&your_info.summoning_skill";
    if (skillname == "att")
        return "&your_info.attack_skill";
    if (skillname == "def")
        return "&your_info.defense_skill";
    if (skillname == "cra")
        return "&your_info.crafting_skill";
    if (skillname == "eng")
        return "&your_info.engineering_skill";
    if (skillname == "pot")
        return "&your_info.potion_skill";
    if (skillname == "tai")
        return "&your_info.tailoring_skill";
    if (skillname == "ran")
        return "&your_info.ranging_skill";
    if (skillname == "oa")
        return "&your_info.overall_skill";
    if (skillname == "har")
        return "&your_info.harvesting_skill";
    return nullptr;
}

std::ostream& operator<<(std::ostream& os, const spell_info& info)
{
    os << "\t{\n"
        << "\t\t.id = " << info.id << ",\n"
        << "\t\t.name = \"" << info.name << "\",\n"
        << "\t\t.desc = \"" << info.desc << "\",\n"
        << "\t\t.image = " << info.image << ",\n"
        << "\t\t.sigils = { " << info.sigils[0] << ", " << info.sigils[1]
        << ", " << info.sigils[2] << ", " << info.sigils[3]
        << ", " << info.sigils[4] << ", " << info.sigils[5] << " },\n"
        << "\t\t.mana = " << info.mana << ",\n"
        << "\t\t.lvls = {\n";
    for (int i = 0; i < NUM_WATCH_STAT; ++i)
    {
        const char* addr = get_skill_address(info.lvls[i]);
        if (addr)
            os << "\t\t\t" << addr << ",\n";
        else
            os << "\t\t\tNULL,\n";
    }
    os << "\t\t},\n"
        << "\t\t.lvls_req = { " << info.lvls_req[0];
    for (int i = 1; i < NUM_WATCH_STAT; ++i)
        os << ", " << info.lvls_req[i];
    os << " },\n"
        << "\t\t.reagents_uid = { " << info.reagents_uid[0] << ", "
        << info.reagents_uid[1] << ", " << info.reagents_uid[2] << ", "
        << info.reagents_uid[3] << " },\n"
        << "\t\t.reagents_qt = { " << info.reagents_qt[0] << ", "
        << info.reagents_qt[1] << ", " << info.reagents_qt[2] << ", "
        << info.reagents_qt[3] << " },\n"
        << "\t\t.buff = " << info.buff << ",\n"
        << "\t\t.uncastable = " << info.uncastable << "\n"
        << "\t},\n";

    return os;
}

std::ostream& write_spells(std::ostream& os)
{
    os << "static spell_info spells_list[" << spells_list.size() << "] = {\n";
    for (const auto& info: spells_list)
        os << info;
    os << "};\n"
        << "static const int num_spells = " << spells_list.size() << ";\n\n";

    return os;
}

std::ostream& operator<<(std::ostream& os, const sigil_def& def)
{
    return os << "\t{\n"
        << "\t\t.sigil_img = " << def.sigil_img << ",\n"
        << "\t\t.name = \"" << def.name << "\",\n"
        << "\t\t.description = \"" << def.description << "\",\n"
        << "\t\t.have_sigil = " << def.have_sigil << "\n"
        << "\t},\n";
}

std::ostream& write_sigils(std::ostream& os)
{
    os << "static sigil_def sigils_list[" << sigils_list.size() << "] = {\n";
    for (const auto& def: sigils_list)
        os << def;
    os << "};\n\n";
    return os;
}

std::ostream& operator<<(std::ostream& os, const group_def& def)
{
    os << "\t{\n"
        << "\t\t.desc = (const unsigned char*)\"" << def.desc << "\",\n"
        << "\t\t.spells = " << def.spells << ",\n"
        << "\t\t.spells_id = { " << def.spells_id[0];
    for (int i = 1; i < SPELLS_NO; ++i)
        os << ", " << def.spells_id[i];
    return os << " },\n\t},\n";
}

std::ostream& write_groups(std::ostream& os)
{
    os << "static group_def groups_list[" << groups_list.size() << "] = {\n";
    for (const auto& def: groups_list)
        os << def;
    os << "};\n"
        << "static const int num_groups = " << groups_list.size() << ";\n\n";
    return os;
}

} // namespace

int main(int argc, const char* argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <directory> <out_file>\n";
        return 1;
    }

    std::string fname = std::string(argv[1]) + "/spells.xml";
    if (!init_spells(fname))
        return 1;

    std::ofstream os(argv[2]);
    if (!os)
    {
        std::cerr << "Failed to open output file " << argv[2] << '\n';
        return 1;
    }
    write_sigils(os);
    write_spells(os);
    write_groups(os);
    os.close();

    return 0;
}
