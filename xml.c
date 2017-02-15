#include <stdlib.h>
#include <string.h>

#include "asc.h"
#include "elloggingwrapper.h"
#include "xml.h"

float xmlGetFloat(xmlNode * n, xmlChar * c)
{
	char * t=(char*)xmlGetProp(n,c);
	float f=t?atof(t):0.0f;
	xmlFree(t);
	return f;
}

int xmlGetInt(xmlNode *n, xmlChar *c)
{
	char *t=(char*)xmlGetProp(n,c);
	int i=t?atoi(t):0;
	xmlFree(t);
	return i;
}

#ifndef LINUX
static int my_UTF8Toisolat1(char **dest, size_t * lu, const char **src, size_t * l)
#else
static int my_UTF8Toisolat1(char **dest, size_t * lu, char **src, size_t * l)
#endif
{
	iconv_t t=iconv_open("ISO_8859-1","UTF-8");

	iconv(t, src, l, dest, lu);

	iconv_close(t);
	return 1;
}

int my_xmlStrncopy(char ** out, const char * in, int len)
{
	if(in) {
		size_t lin=0;
		size_t lout=0;
		int l1=0;
		int l2=0;
		int retval=1;
		char *inbuf;
		char *inbuf2;
		char *outbuf;
		char *outbuf2;

		lin=strlen(in);
		l2=xmlUTF8Strlen((xmlChar*)in);

		if(l2<0) lout=l1;
		else if (len>0 && len<l2) lout=len;
		else lout=l2;

		inbuf=inbuf2=(char *)malloc((lin+1)*sizeof(char));
		outbuf=outbuf2=(char *)malloc((lout+1)*sizeof(char));

		memcpy(inbuf,in,lin);

		l1=lin;
		l2=lout;

#ifdef LINUX
		if(my_UTF8Toisolat1(&outbuf2,&lout,&inbuf2,&lin)<0) {
#else
		if(my_UTF8Toisolat1(&outbuf2,&lout,(const char **)&inbuf2,&lin)<0) {
#endif
			retval=-1;
		}

		free(inbuf);

		outbuf[l2]=0;

		if(*out) {
			memcpy(*out,outbuf,l2+1);
			free(outbuf);
		} else {
			*out=outbuf;
		}

		return retval<0?-1:l2;
	} else return -1;
}

void get_string_value(char *buf, size_t maxlen, const xmlNode *node)
{
	if (!node)
	{
		LOG_ERROR("Node is null!");
		buf[0] = '\0';
		return;
	}

	if (!node->children)
		buf[0] = '\0';
	else
		safe_strncpy(buf, (const char*)node->children->content, maxlen);
}

void get_item_string_value(char *buf, size_t maxlen, const xmlNode *item,
	const unsigned char *name)
{
	const xmlNode *node;

	if (!item)
	{
		LOG_ERROR("Item is null!");
		buf[0] = '\0';
		return;
	}

	// look for this entry in the children
	for (node = item->children; node; node = node->next)
	{
		if (node->type == XML_ELEMENT_NODE
			&& xmlStrcasecmp(node->name, name) == 0)
		{
			get_string_value(buf, maxlen, node);
			return;
		}
	}
}

int get_bool_value(const xmlNode *node)
{
	const xmlChar *tval;

	if (!node)
	{
		LOG_ERROR("Node is null!");
		return 0;
	}

	if (!node->children)
		return 0;

	tval = node->children->content;
	return (xmlStrcasecmp(tval, (xmlChar*)"yes") == 0) ||
		(xmlStrcasecmp(tval, (xmlChar*)"true") == 0) ||
		(xmlStrcasecmp(tval, (xmlChar*)"1") == 0);
}

int get_int_value(const xmlNode *node)
{
	if (!node)
	{
		LOG_ERROR("Node is null!");
		return 0;
	}

	if (!node->children)
		return 0;

	return atoi((const char*)node->children->content);
}

double get_float_value(const xmlNode *node)
{
	if (!node)
	{
		LOG_ERROR("Node is null!");
		return 0.0;
	}

	if (!node->children)
		return 0.0;

	return atof((const char*)node->children->content);
}

int get_int_property(const xmlNode *node, const char *prop)
{
	const xmlAttr *attr;

	if (!node)
	{
		LOG_ERROR("Node is null!");
		return 0;
	}

	for (attr = node->properties; attr; attr = attr->next)
	{
		if (attr->type == XML_ATTRIBUTE_NODE &&
			xmlStrcasecmp(attr->name, (const xmlChar*)prop) == 0)
		{
			return atoi((const char*)attr->children->content);
		}
	}

	return -1;
}

int get_property(const xmlNode *node, const char *prop, const char *desc,
	const dict_elem dict[])
{
	const xmlAttr *attr;

	if (!node)
	{
		LOG_ERROR("Node is null!");
		return 0;
	}

	for (attr = node->properties; attr; attr = attr->next)
	{
		if (attr->type == XML_ATTRIBUTE_NODE &&
			xmlStrcasecmp (attr->name, (const xmlChar*)prop) == 0)
		{
			return find_description_index(dict,
				(const char*)attr->children->content, desc);
		}
	}

	LOG_ERROR("Unable to find property %s in node %s\n", prop, node->name);
	return -1;
}

const char *get_string_property(const xmlNode *node, const char *prop)
{
	const xmlAttr *attr;

	if (node == NULL)
	{
		LOG_ERROR("Node is null!");
		return "";
	}

	for (attr = node->properties; attr; attr = attr->next)
	{
		if (attr->type == XML_ATTRIBUTE_NODE &&
			xmlStrcasecmp (attr->name, (xmlChar *)prop) == 0)
		{
			return (const char*)attr->children->content;
		}
	}

#ifdef	DEBUG_XML
	// don't normally report this, or optional properties will report errors
	LOG_ERROR("Unable to find property %s in node %s\n", prop, node->name);
#endif	//DEBUG_XML
	return "";
}

xmlChar* toUTF8 (const char* str, int len)
{
	int out_size = 2*len;
	int out_len;
	xmlChar* out = calloc (out_size, sizeof (xmlChar));

	while (1)
	{
		int in_len = len;
		out_len = out_size;

		if (isolat1ToUTF8 (out, &out_len, BAD_CAST str, &in_len) < 0)
		{
			// Conversion error
			free (out);
			return NULL;
		}
		if (in_len >= len)
			break;

		out_size *= 2;
		out = realloc (out, out_size * sizeof (xmlChar));
	}

	if (out_len >= out_size)
		// drats, no space to store a terminator
		out = realloc (out, (out_size + 1) * sizeof (xmlChar));
	out[out_len] = '\0';

	return out;
}

char* fromUTF8 (const xmlChar* str, int len)
{
	int out_size = len+1;
	int out_len = out_size;
	int in_len = len;
	char* out = calloc (out_size, 1);

	if (UTF8Toisolat1 (BAD_CAST out, &out_len, str, &in_len) < 0)
	{
		// Conversion error
		free (out);
		return NULL;
	}

	out[out_len] = '\0';

	return out;
}

int find_description_index(const dict_elem dict[], const char *elem, const char *desc)
{
	int idx = 0;
	const char *key;

	while ((key = dict[idx].desc) != NULL) {
		if (strcasecmp (key, elem) == 0)
			return dict[idx].index;
		idx++;
	}

	LOG_ERROR("Unknown %s \"%s\"\n", desc, elem);
	return -1;
}
