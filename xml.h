#ifndef XML_H
#define XML_H

#include <libxml/tree.h>

#ifdef __cplusplus
extern "C"
{
#endif

// Element type and dictionaries for actor definitions
typedef struct {
#ifndef EXT_ACTOR_DICT
	char *desc;
#else
	char desc[100];
#endif
	int index;
} dict_elem;

/*!
 * A macro for the my_xmlstrncopy function that copies and converts an xml-string.
 * Sets the length to 0, hence it will copy until \\0 is reached.
 */
#define MY_XMLSTRCPY(d,s) my_xmlStrncopy(d,s,0)

/*!
 * \ingroup	xml_utils
 * \brief	Finds the xml-attribute with the identifier p in the xmlNode and returns it as a floating point value
 *
 * 		Finds the xml-attribute with the identifier p in the xmlNode and returns it as a floating point value
 *
 * \param	n The xml-node you wish to search
 * \param	p The attribute name you wish to search for
 * \retval float	The floating point value of the string. Returns 0 on failure.
 */
float xmlGetFloat(xmlNode * n, xmlChar * p);

/*!
 * \ingroup	xml_utils
 * \brief	Finds the xml-attribute with the identifier p in the xmlNode and returns it as an integer value
 *
 * 		Finds the xml-attribute with the identifier p in the xmlNode and returns it as an integer value
 *
 * \param	n The node you wish to search
 * \param	p The attribute name you wish to search for
 * \retval int	The integer value of the string. Returns 0 on failure.
 */
int xmlGetInt(xmlNode *n, xmlChar *p);

/*!
 * \ingroup	xml_utils
 * \brief	Copies and converts the UTF8-string pointed to by src into the destination.
 *
 * 		Copies and converts the UTF8-string pointed to by src into the destination. It will max copy n characters, but if n is 0 it will copy the entire string.
 * 		The function allocates appropriate buffer sizes using strlen and xmlUTF8Strlen. The src is copied to the in-buffer, then the in-buffer is converted using iconv() to iso-8859-1 and the converted string is put in the outbuffer.
 * 		The main case is where the pointer pointed to by dest is non-NULL. In that case it will copy the content of the out-buffer to the *dest. Next it will free() the allocated buffers.
 * 		A second case is where the pointer pointed to by dest is NULL - here it will set the pointer to the out-buffer and only free() the in-buffer.
 *
 * \param	dest A pointer to the destination character array pointer
 * \param	src The source string
 * \param	len The maximum length of chars that will be copied
 * \retval int	Returns the number of characters that have been copied, or -1 on failure.
 * \sa my_UTF8Toisolat1
 */
int my_xmlStrncopy(char ** dest, const char * src, int len);

void get_string_value(char *buf, size_t maxlen, const xmlNode *node);
void get_item_string_value(char *buf, size_t maxlen, const xmlNode *node,
                           const unsigned char *name);
int get_bool_value(const xmlNode *node);
int get_int_value(const xmlNode *node);
double get_float_value(const xmlNode *node);
int get_int_property(const xmlNode *node, const char *prop);
const char *get_string_property(const xmlNode *node, const char *prop);
int get_property(const xmlNode *node, const char *prop, const char *desc,
                 const dict_elem dict[]);

/*!
 * \brief Convert a string to UTF-8
 *
 *	Convert string \a str of length \a len bytes from the ISO Latin 1
 *	encoding used in EL to UTF-8. Memory for the output string is
 *	dynamically allocated, and should be freed by the caller.
 *
 * \param str The string to be converted.
 * \param len The length of \a str in bytes.
 * \return pointer to the UTF-8 encoded string if the conversion is
 *         successfull, NULL otherwise.
 */
xmlChar* toUTF8 (const char* str, int len);

/*!
 * \brief Convert a string from UTF-8
 *
 *	Convert string \a str of length \a len bytes from UTF-8 encoding to
 *	the ISO Latin 1 encoding used in EL. Memory for the output string is
 *	dynamically allocated, and should be freed by the caller. Note that
 *	the parameter \a len is the length in \em bytes, not in
 *	characters.
 *
 * \param str The string to be converted.
 * \param len The length of \a str in bytes.
 * \return pointer to the ISO Latin 1 encoded string if the conversion
 *         is successfull, NULL otherwise.
 */
char* fromUTF8 (const xmlChar* str, int len);

int find_description_index(const dict_elem dict[], const char *elem, const char *desc);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XML_H
