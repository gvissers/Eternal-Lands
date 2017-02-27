#include <fstream>
#include <iostream>
#include <map>
#include <vector>

#include "xml.h"

namespace
{

struct time_color
{
    int t;
    float r, g, b, a;
    time_color(int t, float r, float g, float b, float a):
        t(t),
        r(r <= 1.0 ? r : r / 255.0),
        g(g <= 1.0 ? g : g / 255.0),
        b(b <= 1.0 ? b : b / 255.0),
        a(a <= 1.0 ? a : a / 255.0) {}
};
std::vector<time_color> time_colors;

std::vector<std::string> texture_names;

struct color_changes
{
    int off;
    int count;

    color_changes(): off(-1), count(0) {}
};

struct map_sky_defs
{
    color_changes clouds;
    color_changes clouds_detail;
    color_changes clouds_sunny;
    color_changes clouds_detail_sunny;
    color_changes clouds_rainy;
    color_changes clouds_detail_rainy;
    color_changes sky1;
    color_changes sky2;
    color_changes sky3;
    color_changes sky4;
    color_changes sky5;
    color_changes sky1_sunny;
    color_changes sky2_sunny;
    color_changes sky3_sunny;
    color_changes sky4_sunny;
    color_changes sky5_sunny;
    color_changes sun;
    color_changes fog;
    color_changes fog_sunny;
    color_changes fog_rainy;
    color_changes light_ambient;
    color_changes light_diffuse;
    color_changes light_ambient_rainy;
    color_changes light_diffuse_rainy;
    bool no_clouds;
    bool no_sun;
    bool no_moons;
    bool no_stars;
    int clouds_tex;
    int clouds_detail_tex;
    int freeze_time;

    map_sky_defs(): clouds(), clouds_detail(), clouds_sunny(),
        clouds_detail_sunny(), clouds_rainy(), clouds_detail_rainy(),
        sky1(), sky2(), sky3(), sky4(), sky5(), sky1_sunny(), sky2_sunny(),
        sky3_sunny(), sky4_sunny(), sky5_sunny(), sun(), fog(), fog_sunny(),
        fog_rainy(), light_ambient(), light_diffuse(), light_ambient_rainy(),
        light_diffuse_rainy(), no_clouds(true), no_sun(true), no_moons(true),
        no_stars(true), clouds_tex(-1), clouds_detail_tex(-1), freeze_time(-1)
        {}
};
std::map<std::string, map_sky_defs> sky_defs;

int skybox_parse_color_properties(const xmlNode *node, color_changes& container)
{
    int t = -1;
    float r = 0.0, g = 0.0, b = 0.0, a = 1.0;
    int ok = 1;

    for (const xmlAttr *attr = node->properties; attr; attr = attr->next)
    {
        if (attr->type != XML_ATTRIBUTE_NODE)
            continue;

        if (xmlStrcasecmp (attr->name, (xmlChar*)"t") == 0)
        {
            t = int_value(attr);
        }
        else if (xmlStrcasecmp (attr->name, (xmlChar*)"r") == 0)
        {
            r = float_value(attr);
        }
        else if (xmlStrcasecmp (attr->name, (xmlChar*)"g") == 0)
        {
            g = float_value(attr);
        }
        else if (xmlStrcasecmp (attr->name, (xmlChar*)"b") == 0)
        {
            b = float_value(attr);
        }
        else if (xmlStrcasecmp (attr->name, (xmlChar*)"a") == 0)
        {
            a = float_value(attr);
        }
        else
        {
            std::cerr << "Unknown attribute for color: " <<
                reinterpret_cast<const char*>(attr->name) << '\n';
            ok = 0;
        }
    }

    if (t >= 0 && t < 360)
    {
        time_colors.emplace_back(t, r, g, b, a);
        if (container.off < 0)
            container.off = time_colors.size() - 1;
        ++container.count;
    }
    else
    {
        std::cerr << "The time attribute of the color doesn't exist or is wrong!\n";
        ok = 0;
    }

    return ok;
}

int skybox_parse_colors(const xmlNode *node, color_changes& container)
{
    if (!node || !node->children)
        return 0;

    int	ok = 1;
    bool reset = false;
    for (const xmlAttr *attr = node->properties; attr; attr = attr->next)
    {
        if (attr->type == XML_ATTRIBUTE_NODE
            && (!xmlStrcasecmp(attr->name, (xmlChar*)"reset")
                || !xmlStrcasecmp(attr->name, (xmlChar*)"overwrite")))
        {
            reset = bool_value(attr);
        }
        else
        {
            std::cerr << "Unknown attribute for element: "
                << reinterpret_cast<const char*>(attr->name) << '\n';
            ok = 0;
        }
    }

    if (reset)
    {
        // we erase the previous color keys
        container.off = -1;
        container.count = 0;
    }
    else if (container.off >= 0)
    {
std::cerr << "off = " << container.off << ", count = " << container.count << '\n';
        std::cerr << "Colors already parsed.\n";
        return 0;
    }

    for (const xmlNode *item = node->children; item; item = item->next)
    {
        if (item->type == XML_ELEMENT_NODE)
        {
            if (xmlStrcasecmp(item->name, (xmlChar*)"color") == 0)
            {
                ok &= skybox_parse_color_properties(item, container);
            }
            else
            {
                std::cerr << "Unknown node for element: " << item->name << '\n';
                ok = 0;
            }
        }
        else if (item->type == XML_ENTITY_REF_NODE)
        {
            ok &= skybox_parse_colors(item->children, container);
        }
    }

    return ok;
}

int get_texture_index(const std::string& fname)
{
    for (unsigned int i = 0; i < texture_names.size(); ++i)
    {
        if (texture_names[i] == fname)
            return i;
    }
    texture_names.push_back(fname);
    return texture_names.size() - 1;
}

int skybox_parse_properties(const xmlNode *node, map_sky_defs& defs)
{
    int ok = 1;

    for (const xmlNode *item = node->children; item; item = item->next)
    {
        if (item->type == XML_ELEMENT_NODE)
        {
            if (xmlStrcasecmp(item->name, (xmlChar*)"clouds") == 0)
            {
                for (const xmlAttr *attr = item->properties; attr; attr = attr->next)
                {
                    if (attr->type != XML_ATTRIBUTE_NODE)
                        continue;

                    if (!xmlStrcasecmp (attr->name, (xmlChar*)"show"))
                    {
                        defs.no_clouds = !bool_value(attr);
                    }
                    else if (xmlStrcasecmp (attr->name, (xmlChar*)"texture") == 0)
                    {
                        defs.clouds_tex = get_texture_index(value(attr));
                    }
                    else if (xmlStrcasecmp (attr->name, (xmlChar*)"texture_detail") == 0)
                    {
                        defs.clouds_detail_tex = get_texture_index(value(attr));
                    }
                    else
                    {
                        std::cerr << "unknown attribute for clouds: "
                            << reinterpret_cast<const char*>(attr->name) << '\n';
                        ok = 0;
                    }
                }
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"sun") == 0)
            {
                for (const xmlAttr *attr = item->properties; attr; attr = attr->next)
                {
                    if (attr->type != XML_ATTRIBUTE_NODE)
                        continue;

                    if (!xmlStrcasecmp (attr->name, (xmlChar*)"show"))
                    {
                        defs.no_sun = !bool_value(attr);
                    }
                    else
                    {
                        std::cerr << "unknown attribute for sun: "
                            << reinterpret_cast<const char*>(attr->name) << '\n';
                        ok = 0;
                    }
                }
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"moons") == 0)
            {
                for (const xmlAttr *attr = item->properties; attr; attr = attr->next)
                {
                    if (attr->type != XML_ATTRIBUTE_NODE)
                        continue;

                    if (!xmlStrcasecmp (attr->name, (xmlChar*)"show"))
                    {
                        defs.no_moons = !bool_value(attr);
                    }
                    else
                    {
                        std::cerr << "unknown attribute for moons: "
                            << reinterpret_cast<const char*>(attr->name) << '\n';
                        ok = 0;
                    }
                }
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"stars") == 0)
            {
                for (const xmlAttr *attr = item->properties; attr; attr = attr->next)
                {
                    if (attr->type != XML_ATTRIBUTE_NODE)
                        continue;

                    if (!xmlStrcasecmp(attr->name, (xmlChar*)"show"))
                    {
                        defs.no_stars = !bool_value(attr);
                    }
                    else
                    {
                        std::cerr << "Unknown attribute for stars: "
                            << reinterpret_cast<const char*>(attr->name) << '\n';
                        ok = 0;
                    }
                }
            }
            else if (xmlStrcasecmp(item->name, (xmlChar*)"freeze_time") == 0)
            {
                int value = int_value(item);
                defs.freeze_time = (value >= 0 && value < 360) ? value : -1;
            }
            else
            {
                std::cerr << "Unknown node for properties: " << item->name << '\n';
                ok = 0;
            }
        }
        else if (item->type == XML_ENTITY_REF_NODE)
        {
            ok &= skybox_parse_properties(item->children, defs);
        }
    }

    return ok;
}

int skybox_parse_defs(const xmlNode *node, const std::string& map_name)
{
    int ok = 1;
    map_sky_defs& defs = sky_defs[map_name];
    for (const xmlNode *child = node->children; child; child = child->next)
    {
        if (child->type == XML_ELEMENT_NODE)
        {
            if (xmlStrcasecmp(child->name, (xmlChar*)"properties") == 0)
            {
                ok &= skybox_parse_properties(child, defs);
            }
            else if (xmlStrcasecmp(child->name, (xmlChar*)"clouds") == 0)
            {
                ok &= skybox_parse_colors(child, defs.clouds);
            }
            else if (xmlStrcasecmp(child->name, (xmlChar*)"clouds_detail") == 0)
            {
                ok &= skybox_parse_colors(child, defs.clouds_detail);
            }
            else if (xmlStrcasecmp(child->name, (xmlChar*)"clouds_sunny") == 0)
            {
                ok &= skybox_parse_colors(child, defs.clouds_sunny);
            }
            else if (xmlStrcasecmp(child->name, (xmlChar*)"clouds_detail_sunny") == 0)
            {
                ok &= skybox_parse_colors(child, defs.clouds_detail_sunny);
            }
            else if (xmlStrcasecmp(child->name, (xmlChar*)"clouds_rainy") == 0)
            {
                ok &= skybox_parse_colors(child, defs.clouds_rainy);
            }
            else if (xmlStrcasecmp(child->name, (xmlChar*)"clouds_detail_rainy") == 0)
            {
                ok &= skybox_parse_colors(child, defs.clouds_detail_rainy);
            }
            else if (xmlStrcasecmp(child->name, (xmlChar*)"sky1") == 0)
            {
                ok &= skybox_parse_colors(child, defs.sky1);
            }
            else if (xmlStrcasecmp(child->name, (xmlChar*)"sky2") == 0)
            {
                ok &= skybox_parse_colors(child, defs.sky2);
            }
            else if (xmlStrcasecmp(child->name, (xmlChar*)"sky3") == 0)
            {
                ok &= skybox_parse_colors(child, defs.sky3);
            }
            else if (xmlStrcasecmp(child->name, (xmlChar*)"sky4") == 0)
            {
                ok &= skybox_parse_colors(child, defs.sky4);
            }
            else if (xmlStrcasecmp(child->name, (xmlChar*)"sky5") == 0)
            {
                ok &= skybox_parse_colors(child, defs.sky5);
            }
            else if (xmlStrcasecmp(child->name, (xmlChar*)"sky1_sunny") == 0)
            {
                ok &= skybox_parse_colors(child, defs.sky1_sunny);
            }
            else if (xmlStrcasecmp(child->name, (xmlChar*)"sky2_sunny") == 0)
            {
                ok &= skybox_parse_colors(child, defs.sky2_sunny);
            }
            else if (xmlStrcasecmp(child->name, (xmlChar*)"sky3_sunny") == 0)
            {
                ok &= skybox_parse_colors(child, defs.sky3_sunny);
            }
            else if (xmlStrcasecmp(child->name, (xmlChar*)"sky4_sunny") == 0)
            {
                ok &= skybox_parse_colors(child, defs.sky4_sunny);
            }
            else if (xmlStrcasecmp(child->name, (xmlChar*)"sky5_sunny") == 0)
            {
                ok &= skybox_parse_colors(child, defs.sky5_sunny);
            }
            else if (xmlStrcasecmp(child->name, (xmlChar*)"sun") == 0)
            {
                ok &= skybox_parse_colors(child, defs.sun);
            }
            else if (xmlStrcasecmp(child->name, (xmlChar*)"fog") == 0)
            {
                ok &= skybox_parse_colors(child, defs.fog);
            }
            else if (xmlStrcasecmp(child->name, (xmlChar*)"fog_sunny") == 0)
            {
                ok &= skybox_parse_colors(child, defs.fog_sunny);
            }
            else if (xmlStrcasecmp(child->name, (xmlChar*)"fog_rainy") == 0)
            {
                ok &= skybox_parse_colors(child, defs.fog_rainy);
            }
            else if (xmlStrcasecmp(child->name, (xmlChar*)"light_ambient") == 0)
            {
                ok &= skybox_parse_colors(child, defs.light_ambient);
            }
            else if (xmlStrcasecmp(child->name, (xmlChar*)"light_diffuse") == 0)
            {
                ok &= skybox_parse_colors(child, defs.light_diffuse);
            }
            else if (xmlStrcasecmp(child->name, (xmlChar*)"light_ambient_rainy") == 0)
            {
                ok &= skybox_parse_colors(child, defs.light_ambient_rainy);
            }
            else if (xmlStrcasecmp(child->name, (xmlChar*)"light_diffuse_rainy") == 0)
            {
                ok &= skybox_parse_colors(child, defs.light_diffuse_rainy);
            }
            else if (xmlStrcasecmp(child->name, (xmlChar*)"map") == 0)
            {
                 std::string name = property(child, "name");
                 if (name != map_name)
                 {
                     sky_defs[name] = sky_defs[""];
                     ok &= skybox_parse_defs(child, name);
                 }
            }
            else
            {
                std::cerr << "Unknown element for skybox: " << child->name << '\n';
                ok = 0;
            }
        }
        else if (child->type == XML_ENTITY_REF_NODE)
        {
            ok &= skybox_parse_defs(child->children, map_name);
        }
    }

    return ok;
}


int skybox_read_defs(const std::string& file_name)
{
    int ok = 1;

    xmlDoc *doc = xmlReadFile(file_name.c_str(), NULL, XML_PARSE_NOENT);
    if (!doc)
    {
        std::cerr << "Unable to read skybox definition file " << file_name << '\n';
        return 0;
    }

    const xmlNode *root = xmlDocGetRootElement(doc);
    if (!root)
    {
        std::cerr << "Unable to parse skybox definition file " << file_name << '\n';
        ok = 0;
    }
    else if (xmlStrcasecmp(root->name, (xmlChar*)"skybox") != 0)
    {
        std::cerr << "Unknown key \"" << root->name << "\" (\"skybox\" expected).\n";
        ok = 0;
    }
    else
    {
        ok = skybox_parse_defs(root, "");
    }

    xmlFreeDoc(doc);
    return ok;
}

std::ostream& operator<<(std::ostream& os, const time_color& tc)
{
    return os << "\t{ .t = " << tc.t << ", .r = " << tc.r << ", .g = " << tc.g
        << ", .b = " << tc.b << ", .a = " << tc.a << " },\n";
}

std::ostream& write_time_colors(std::ostream& os)
{
    os << "static const time_color time_colors[" << time_colors.size() << "] = {\n";
    for (const auto& tc: time_colors)
        os << tc;
    return os << "};\n\n";
}

std::ostream& write_texture_names(std::ostream& os)
{
    os << "static const char* texture_names[" << texture_names.size() << "] = {\n";
    for (const std::string& name: texture_names)
        os << "\t\"" << name << "\",\n";
    return os << "};\n\n";
}

std::ostream& operator<<(std::ostream& os, const color_changes& cs)
{
    return os << "{ .off = " << cs.off << ", .count = " << cs.count << " }";
}

std::ostream& operator<<(std::ostream& os, const map_sky_defs& defs)
{
    os << "\t{\n"
        << "\t\t.clouds = " << defs.clouds << ",\n"
        << "\t\t.clouds_detail = " << defs.clouds_detail << ",\n"
        << "\t\t.clouds_sunny = " << defs.clouds_sunny << ",\n"
        << "\t\t.clouds_detail_sunny = " << defs.clouds_detail_sunny << ",\n"
        << "\t\t.clouds_rainy = " << defs.clouds_rainy << ",\n"
        << "\t\t.clouds_detail_rainy = " << defs.clouds_detail_rainy << ",\n"
        << "\t\t.sky1 = " << defs.sky1 << ",\n"
        << "\t\t.sky2 = " << defs.sky2 << ",\n"
        << "\t\t.sky3 = " << defs.sky3 << ",\n"
        << "\t\t.sky4 = " << defs.sky4 << ",\n"
        << "\t\t.sky5 = " << defs.sky5 << ",\n"
        << "\t\t.sky1_sunny = " << defs.sky1_sunny << ",\n"
        << "\t\t.sky2_sunny = " << defs.sky2_sunny << ",\n"
        << "\t\t.sky3_sunny = " << defs.sky3_sunny << ",\n"
        << "\t\t.sky4_sunny = " << defs.sky4_sunny << ",\n"
        << "\t\t.sky5_sunny = " << defs.sky5_sunny << ",\n"
        << "\t\t.sun = " << defs.sun << ",\n"
        << "\t\t.fog = " << defs.fog << ",\n"
        << "\t\t.fog_sunny = " << defs.fog_sunny << ",\n"
        << "\t\t.fog_rainy = " << defs.fog_rainy << ",\n"
        << "\t\t.light_ambient = " << defs.light_ambient << ",\n"
        << "\t\t.light_diffuse = " << defs.light_diffuse << ",\n"
        << "\t\t.light_ambient_rainy = " << defs.light_ambient_rainy << ",\n"
        << "\t\t.light_diffuse_rainy = " << defs.light_diffuse_rainy << ",\n"
        << "\t\t.no_clouds = " << int(defs.no_clouds) << ",\n"
        << "\t\t.no_sun = " << int(defs.no_sun) << ",\n"
        << "\t\t.no_moons = " << int(defs.no_moons) << ",\n"
        << "\t\t.no_stars = " << int(defs.no_stars) << ",\n"
        << "\t\t.clouds_tex = " << defs.clouds_tex << ",\n"
        << "\t\t.clouds_detail_tex = " << defs.clouds_detail_tex << ",\n"
        << "\t\t.freeze_time = " << defs.freeze_time << "\n"
        << "\t},\n";
    return os;
}

std::ostream& write_sky_defs(std::ostream& os)
{
    std::vector<std::string> names;

    os << "static const map_sky_defs sky_defs[" << sky_defs.size() << "] = {\n";
    for (const auto& nv: sky_defs)
    {
        names.push_back(nv.first);
        os << nv.second;
    }
    os << "};\n"
        << "static const char* sky_def_map_names[" << sky_defs.size() << "] = {\n";
    for (const std::string& name: names)
        os << "\t\"" << name << "\",\n";
    os << "};\n"
        << "static const int nr_sky_defs = " << sky_defs.size() << ";\n\n";
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

    std::string fname = std::string(argv[1]) + "/skybox/skybox_defs.xml";
    if (!skybox_read_defs(fname))
        return 1;

    std::ofstream os(argv[2]);
    if (!os)
    {
        std::cerr << "Failed to open output file " << argv[2] << '\n';
        return 1;
    }
    write_time_colors(os);
    write_texture_names(os);
    write_sky_defs(os);
    os.close();

    return 0;
}
