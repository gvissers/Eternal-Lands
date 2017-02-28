#include <fstream>
#include <iostream>
#include <vector>

#include "xml.h"

namespace
{

struct colour_record
{
    std::string name;
    float r, g, b, a;

    colour_record(const std::string& name, float r, float g, float b, float a):
        name(name), r(r), g(g), b(b), a(a) {}
};
std::vector<colour_record> colour_records;

bool load_xml(const std::string& file_name)
{
    xmlDoc *doc = xmlReadFile(file_name.c_str(), nullptr, 0);
    if (!doc)
    {
        std::cerr << "Can't open file \"" << file_name << "\"\n";
        return false;
    }

    const xmlNode *cur = xmlDocGetRootElement(doc);
    if (!cur)
    {
        std::cerr << "Empty xml document\n";
        xmlFreeDoc(doc);
        return false;
    }

    if (xmlStrcasecmp(cur->name, (const xmlChar *) "named_colours"))
    {
        std::cerr << "No named_colours element\n";
        xmlFreeDoc(doc);
        return false;
    }

    for (cur = cur->children; cur; cur = cur->next)
    {
        if (xmlStrcasecmp(cur->name, (const xmlChar *)"colour"))
            continue;

        std::string name = property(cur, "name");
        if (!name.empty())
        {
            float r = float_property(cur, "r", -1.0f);
            float g = float_property(cur, "g", -1.0f);
            float b = float_property(cur, "b", -1.0f);
            float a = float_property(cur, "a", 1.0f);
            if (r >= 0 and g >= 0 and b >= 0 and a >= 0)
                colour_records.emplace_back(name, r, g, b, a);
        }
    }

    xmlFreeDoc(doc);
    return true;
}

std::ostream& write_colours(std::ostream& os)
{
    for (const auto& record: colour_records)
    {
        os << "add(\"" << record.name << "\", Colour_Tuple(" << record.r << ", "
            << record.g << ", " << record.b << ", " << record.a << "));\n";
    }
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

    std::string fname = std::string(argv[1]) + "/named_colours.xml";
    if (!load_xml(fname))
        return 1;

    std::ofstream os(argv[2]);
    if (!os)
    {
        std::cerr << "Unable to open output file " << argv[2] << '\n';
        return 1;
    }
    write_colours(os);
    os.close();

    return 0;
}
