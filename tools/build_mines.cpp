#include <fstream>
#include <iostream>
#include <vector>

#include "xml.h"

namespace
{

struct mine_types
{
    int id;
    std::string file;

    mine_types(int id, const std::string& file): id(id), file(file) {}
};
std::vector<mine_types> mine_defs;

int parse_mine_defs(const xmlNode *node)
{
    int ok = 1;
    for (const xmlNode *def = node->children; def; def = def->next)
    {
        if (def->type == XML_ELEMENT_NODE)
        {
            if (xmlStrcasecmp(def->name, (xmlChar*)"mine") == 0)
            {
                mine_defs.emplace_back(int_property(def, "id"), value(def));
            }
            else
            {
                std::cerr << "Unknown element found: " << def->name << "\n";
                ok = 0;
            }
        }
        else if (def->type == XML_ENTITY_REF_NODE)
        {
            ok &= parse_mine_defs (def->children);
        }
    }

    return ok;
}

int load_mines_config(const std::string& fname)
{
    xmlDoc *doc = xmlReadFile(fname.c_str(), nullptr, 0);
    if (!doc)
    {
        std::cerr << "Unable to open file " << fname << '\n';
        return 0;
    }

    // Can we find a root element
    int ok = 1;
    xmlNode *root = xmlDocGetRootElement(doc);
    if (!root)
    {
        std::cerr << "No XML root element found in '" << fname << "'\n";
        ok = 0;
    }
    // Is the root the right type?
    else if (xmlStrcmp(root->name, (xmlChar*)"mines"))
    {
        std::cerr << "Invalid root element '" << root->name << "' in '" << fname << "'\n";
        ok = 0;
    }
    // We've found our expected root, now parse the children
    else
    {
        ok = parse_mine_defs(root);
    }

    xmlFreeDoc(doc);

    return ok;
}

std::ostream& write_mines(std::ostream& os)
{
    os << "static const mine_types mine_defs[" << mine_defs.size() << "] = {\n";
    for (const auto& def: mine_defs)
        os << "\t{ .id = " << def.id << ", .file = \"" << def.file << "\" },\n";
    os << "};\n"
        << "static const int num_mine_defs = " << mine_defs.size() << ";\n\n";
    return os;
}

} // namespace

int main(int argc, const char*argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <directory> <out_file>\n";
        return 1;
    }

    std::string fname = std::string(argv[1]) + "/mines.xml";
    if (!load_mines_config(fname))
        return 1;

    std::ofstream os(argv[2]);
    if (!os)
    {
        std::cerr << "Failed to open output file " << argv[2] << '\n';
        return 1;
    }
    write_mines(os);
    os.close();

    return 0;
}
