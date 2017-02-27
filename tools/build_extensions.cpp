#include <fstream>
#include <iostream>
#include <vector>

#include "xml.h"

namespace
{

std::vector<std::string> names;

int parse_extension(const xmlNode *node)
{
    if (xmlStrcasecmp(node->name, (xmlChar*)"extention") != 0)
    {
        std::cerr << "Unknown key \"" << node->name << "\" (\"extention\" expected).\n";
        return 0;
    }

    std::string name;
    int bit = -1;
    for (const xmlNode *child = node->children; child; child = child->next)
    {
        if (child->type == XML_ELEMENT_NODE)
        {
            if (!xmlStrcasecmp(child->name, reinterpret_cast<const xmlChar*>("name")))
                name = value(child);
            else if (!xmlStrcasecmp(child->name, reinterpret_cast<const xmlChar*>("value")))
                bit = int_value(child);
        }
    }

    if (name.empty())
    {
        std::cerr << "No extension name found\n";
        return 0;
    }
    if (bit < 0)
    {
        std::cerr << "No bit index found for extension " << name << '\n';
        return 0;
    }

    if (unsigned(bit) >= names.size())
        names.resize(bit+1);
    names[bit] = name;

    return 1;
}

int parse_extensions(const std::string& fname)
{
    xmlDoc *doc = xmlReadFile(fname.c_str(), nullptr, 0);
    if (!doc)
    {
        std::cerr << "Unable to open file " << fname << '\n';
        return 0;
    }

    int ok = 1;
    xmlNode *root = xmlDocGetRootElement(doc);
    if (!root)
    {
        std::cerr << "No XML root element found in '" << fname << "'\n";
        ok = 0;
    }
    else if (xmlStrcasecmp(root->name, (xmlChar*)"extentions") != 0)
    {
        std::cerr << "Unknown key \"" << root->name << "\" (\"extentions\" expected).\n";
        ok = 0;
    }
    else
    {
        for (const xmlNode *node = root->children; node; node = node->next)
        {
            if (node->type != XML_ELEMENT_NODE)
                continue;

            ok &= parse_extension(node);
        }
    }

    xmlFreeDoc(doc);
    return ok;
}

std::ostream& write_extensions(std::ostream& os)
{
    os << "namespace\n{\n\n"
        << "const std::string extension_names[" << names.size() << "] = {\n";
    for (const std::string& name: names)
        os << "\t\"" << name << "\",\n";
    os << "};\n"
        << "const int nr_extension_names = " << names.size() << ";\n\n"
        << "} // namespace\n\n";
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

    std::string fname = std::string(argv[1]) + "/extentions.xml";
    if (!parse_extensions(fname))
        return 1;

    std::ofstream os(argv[2]);
    if (!os)
    {
        std::cerr << "Failed to open output file " << argv[2] << '\n';
        return 1;
    }
    write_extensions(os);
    os.close();

    return 0;
}
