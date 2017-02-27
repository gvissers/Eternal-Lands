#include <algorithm>
#include "xml.h"

std::string value(const xmlNode *node)
{
    if (!node->children)
        return std::string();
    return std::string(reinterpret_cast<const char*>(node->children->content));
}

std::string value(const xmlAttr *attr)
{
    if (!attr->children)
        return std::string();
    return std::string(reinterpret_cast<const char*>(attr->children->content));
}

std::string lc_value(const xmlNode *node)
{
    std::string res = value(node);
    std::transform(res.begin(), res.end(), res.begin(), ::tolower);
    return res;
}

std::string lc_value(const xmlAttr *attr)
{
    std::string res = value(attr);
    std::transform(res.begin(), res.end(), res.begin(), ::tolower);
    return res;
}

bool bool_value(const xmlNode *node, bool def)
{
    std::string sval = lc_value(node);
    if (sval.empty())
        return def;
    return sval == "yes" || sval == "true" || sval == "1";
}

bool bool_value(const xmlAttr *attr, bool def)
{
    std::string sval = lc_value(attr);
    if (sval.empty())
        return def;
    return sval == "yes" || sval == "true" || sval == "1";
}

int int_value(const xmlNode *node, int def)
{
    std::string sval = value(node);
    if (sval.empty())
        return def;
    return std::stoi(sval);
}

int int_value(const xmlAttr *attr, int def)
{
    std::string sval = value(attr);
    if (sval.empty())
        return def;
    return std::stoi(sval);
}

int float_value(const xmlNode *node, float def)
{
    std::string sval = value(node);
    if (sval.empty())
        return def;
    return std::stof(sval);
}

int float_value(const xmlAttr *attr, float def)
{
    std::string sval = value(attr);
    if (sval.empty())
        return def;
    return std::stof(sval);
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

int int_property(const xmlNode *node, const char *prop, int def)
{
    std::string sval = property(node, prop);
    if (sval.empty())
        return def;
    return std::stoi(sval);
}

