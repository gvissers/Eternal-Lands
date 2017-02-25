#ifndef TOOLS_XML_H
#define TOOLS_XML_H

#include <string>
#include <libxml/parser.h>

std::string value(const xmlNode *node);
std::string value(const xmlAttr *attr);
std::string lc_value(const xmlNode *node);
bool bool_value(const xmlNode *node, bool def=false);
int int_value(const xmlNode *node, int def=0);
int float_value(const xmlNode *node, float def=0.0f);
int float_value(const xmlAttr *attr, float def=0.0f);
std::string item_value(const xmlNode *item, const char *name);
std::string property(const xmlNode *node, const char *prop);
std::string lc_property(const xmlNode *node, const char *prop);
int int_property(const xmlNode *node, const char *prop, int def=-1);

#endif // TOOLS_XML_H
