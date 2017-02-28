#include <fstream>
#include <iostream>
#include <vector>

#include "xml.h"

namespace
{

std::vector<std::string> texture_names;

struct lightning_def
{
    int texture;
    float coords[4]; // left bottom right top

    lightning_def(): texture(-1), coords{0.0f, 0.0f, 0.0f, 0.0f} {}
};
std::vector<lightning_def> lightning_defs;

struct weather_def
{
    int use_sprites;
    float density;
    float color[4];
    float size;
    float speed;
    float wind_effect;
    int texture;

    weather_def(): use_sprites(0), density(0.0f), color{0.0f, 0.0f, 0.0f, 0.0f},
        size(0.0f), speed(0.0f), wind_effect(0.0f), texture(-1) {}
};
std::vector<weather_def> weather_defs;

int get_texture_id(const std::string& name)
{
    for (int i = 0; i < int(texture_names.size()); ++i)
    {
        if (texture_names[i] == name)
            return i;
    }
    texture_names.push_back(name);
    return texture_names.size() - 1;
}

int weather_parse_effect(const xmlNode *node)
{
    int id = int_property(node, "id");
    if (id < 1)
    {
        std::cerr << "wrong or missing id for weather effect\n";
        return 0;
    }

    if (id >= int(weather_defs.size()))
        weather_defs.resize(id+1);
    weather_def& def = weather_defs[id];

    int ok = 1;
    for (const xmlNode *item = node->children; item; item = item->next)
    {
        if (item->type == XML_ELEMENT_NODE)
        {
            if (xmlStrcasecmp(item->name, (xmlChar*)"sprites") == 0)
            {
                def.use_sprites = bool_value(item);
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"density") == 0)
            {
                def.density = float_value(item);
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"color") == 0)
            {
                def.color[0] = 0.0;
                def.color[1] = 0.0;
                def.color[2] = 0.0;
                def.color[3] = 0.0;
                for (const xmlAttr *attr = item->properties; attr; attr = attr->next)
                {
                    if (attr->type != XML_ATTRIBUTE_NODE)
                        continue;

                    if (!xmlStrcasecmp (attr->name, (xmlChar*)"r"))
                    {
                        def.color[0] = float_value(attr);
                    }
                    else if (!xmlStrcasecmp (attr->name, (xmlChar*)"g"))
                    {
                        def.color[1] = float_value(attr);
                    }
                    else if (!xmlStrcasecmp (attr->name, (xmlChar*)"b"))
                    {
                        def.color[2] = float_value(attr);
                    }
                    else if (!xmlStrcasecmp (attr->name, (xmlChar*)"a"))
                    {
                        def.color[3] = float_value(attr);
                    }
                    else
                    {
                        std::cerr << "unknown attribute for weather effect color: "
                            << reinterpret_cast<const char*>(attr->name) << '\n';
                        ok = 0;
                    }
                }
                def.color[0] /= 255.0;
                def.color[1] /= 255.0;
                def.color[2] /= 255.0;
                def.color[3] /= 255.0;
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"size") == 0)
            {
                def.size = float_value(item);
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"speed") == 0)
            {
                def.speed = float_value(item);
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"wind_effect") == 0)
            {
                def.wind_effect = float_value(item);
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"texture") == 0)
            {
                def.texture = get_texture_id(value(item));
            }
            else
            {
                std::cerr << "unknown node for weather effect: "
                    << reinterpret_cast<const char*>(item->name) << '\n';
                ok = 0;
            }
        }
        else if (item->type == XML_ENTITY_REF_NODE)
        {
            ok &= weather_parse_effect(item->children);
        }
    }

    return ok;
}

int weather_parse_lightning(const xmlNode *node)
{
    lightning_defs.push_back(lightning_def());
    lightning_def& def = lightning_defs.back();

    int ok = 1;
    for (const xmlAttr *attr = node->properties; attr; attr = attr->next)
    {
        if (attr->type != XML_ATTRIBUTE_NODE)
            continue;

        if (!xmlStrcasecmp (attr->name, (xmlChar*)"texture"))
        {
            def.texture = get_texture_id(value(attr));
        }
        else if (!xmlStrcasecmp (attr->name, (xmlChar*)"x1"))
        {
            def.coords[0] = float_value(attr);
        }
        else if (!xmlStrcasecmp (attr->name, (xmlChar*)"y1"))
        {
            def.coords[1] = float_value(attr);
        }
        else if (!xmlStrcasecmp (attr->name, (xmlChar*)"x2"))
        {
            def.coords[2] = float_value(attr);
        }
        else if (!xmlStrcasecmp (attr->name, (xmlChar*)"y2"))
        {
            def.coords[3] = float_value(attr);
        }
        else
        {
            std::cerr << "unknown attribute for weather effect color: "
                << reinterpret_cast<const char*>(attr->name) << '\n';
            ok = 0;
        }
    }

    return ok;
}

int weather_parse_defs(const xmlNode *node)
{
    int ok = 1;
    for (const xmlNode *def = node->children; def; def = def->next)
    {
        if (def->type == XML_ELEMENT_NODE)
        {
            if (xmlStrcasecmp(def->name, (xmlChar*)"effect") == 0)
            {
                ok &= weather_parse_effect(def);
            }
            else if (xmlStrcasecmp(def->name, (xmlChar*)"lightning") == 0)
            {
                ok &= weather_parse_lightning(def);
            }
            else
            {
                std::cerr << "unknown element for weather: " << def->name << '\n';
                ok = 0;
            }
        }
        else if (def->type == XML_ENTITY_REF_NODE)
        {
            ok &= weather_parse_defs(def->children);
        }
    }

    return ok;
}

int weather_read_defs(const std::string& file_name)
{
    xmlDoc *doc = xmlReadFile(file_name.c_str(), nullptr, 0);
    if (!doc)
    {
        std::cerr << "Unable to read weather definition file " << file_name << '\n';
        return 0;
    }

    int ok = 1;
    const xmlNode *root = xmlDocGetRootElement(doc);
    if (!root)
    {
        std::cerr << "Unable to parse weather definition file " << file_name << '\n';
        ok = 0;
    }
    else if (xmlStrcasecmp(root->name, (xmlChar*)"weather") != 0)
    {
        std::cerr << "Unknown key \"" << root->name << "\" (\"weather\" expected).\n";
        ok = 0;
    }
    else
    {
        ok = weather_parse_defs(root);
    }

    xmlFreeDoc(doc);
    return ok;
}

std::ostream& operator<<(std::ostream& os, const lightning_def& def)
{
    return os << "\t{ .texture = " << def.texture << ", .coords = { "
        << def.coords[0] << ", " << def.coords[1] << ", "
        << def.coords[2] << ", " << def.coords[3] << " } },\n";
}

std::ostream& operator<<(std::ostream& os, const weather_def& def)
{
    return os << "\t{\n"
        << "\t\t.use_sprites = " << def.use_sprites << ",\n"
        << "\t\t.color = { " << def.color[0] << ", " << def.color[1]
        << ", " << def.color[2] << ", " << def.color[3] << " },\n"
        << "\t\t.size = " << def.size << ",\n"
        << "\t\t.speed = " << def.speed << ",\n"
        << "\t\t.wind_effect = " << def.wind_effect << ",\n"
        << "\t\t.texture = " << def.texture << "\n"
        << "\t},\n";
}

std::ostream& write_c_file(std::ostream& os)
{
    os << "static const char* texture_names[" << texture_names.size() << "] = {\n";
    for (const auto& name: texture_names)
        os << "\t\"" << name << "\",\n";
    os << "};\n\n"
        << "static lightning_def lightnings_defs[" << lightning_defs.size() << "] = {\n";
    for (const auto& def: lightning_defs)
        os << def;
    os << "};\n"
        << "static const int lightnings_defs_count = " << lightning_defs.size() << ";\n\n"
        << "static weather_def weather_defs[" << weather_defs.size() << "] = {\n";
    for (const auto& def: weather_defs)
        os << def;
    os << "};\n"
        << "static const int nr_weather_defs = " << weather_defs.size() << ";\n\n"
        << "static float weather_ratios[" << weather_defs.size() << "] = { 0.0f };\n"
        << "static int weather_drops_count[" << weather_defs.size() << "] = { 0 };\n"
        << "static weather_drop weather_drops[" << weather_defs.size() << "][MAX_RAIN_DROPS];\n\n";

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

    std::string fname = std::string(argv[1]) + "/weather.xml";
    if (!weather_read_defs(fname))
        return 1;

    std::ofstream os(argv[2]);
    if (!os)
    {
        std::cerr << "Unable to open output file " << argv[2] << '\n';
        return 1;
    }
    write_c_file(os);
    os.close();

    return 0;
}
