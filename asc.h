/*!
 * \file
 * \ingroup	misc
 * \brief	Miscellaneous functions used for file handling and string utilities.
 */
#ifndef __ASC_H__
#define __ASC_H__

#include <SDL_types.h>
#include "client_serv.h"
#include "font.h"

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * Check if a character is a color character
 */
static __inline__ int is_color (Uint8 c)
{
	return c >= 127 + c_lbound && c <= 127 + c_ubound;
}

/*!
 * \brief Get the color number from a color character
 *
 * Get the color number from a character sent by the server. Only valid
 * color characters should give a valid color number between \c c_lbound
 * and \c c_ubound, but no checks are performed.
 * \param c The character to get the color number from
 * \retval int the color number
 * \sa is_color(), to_color_char()
 */
static __inline__ int from_color_char (Uint8 c)
{
	return c-127;
}

/*!
 * \brief Convert a color number into a color character
 *
 * Compute the color character for the color with index \a color.
 *
 * \param color A valid color number between \c c_lbound and \c c_ubound
 * \retval Uint8 The color character
 * \sa from_color_char()
 */
static __inline__ Uint8 to_color_char (int color)
{
	return (Uint8) (color+127);
}

/*!
 * Check if a character is printable. In this context, that means
 * printable ascii, or non-ascii if we know what symbol to use
 */
static __inline__ int is_printable (Uint8 c)
{
	return get_font_char(c) >= 0;
}

#ifndef FASTER_MAP_LOAD
/*!
 * \ingroup	misc_utils
 * \brief	Gets an integer after the given string
 *
 * 		The function finds \a needle in \a haystack and returns the integer value after the string given after it.
 *
 * \param	needle The string you wish to find
 * \param	haystack The pointer to the char array you wish to find the string from
 * \param	max_len The maximum length it should check
 * \retval Sint32	Returns the integer behind the string or -1 on failure.
 */
Sint32 get_integer_after_string (const char* needle, const char* haystack, Uint32 max_len);

/*!
 * \ingroup	misc_utils
 * \brief	Gets a float after the given string
 *
 * 		The function finds \a needle in \a haystack and returns the floating point value after it.
 *
 * \param	needle The string you wish to find
 * \param	haystack The pointer to the char array you want to search for the string in.
 * \param	max_len The maximum length it should check
 * \retval float	Returns the float after the string or -1.0f on failure.
 */
float get_float_after_string (const char* needle, const char* haystack, Uint32 max_len);
#endif // FASTER_MAP_LOAD

/*!
 * \ingroup	misc_utils
 * \brief	Gets the offset of a string in a char array
 *
 * 		The function gets the location of source_pointer in the dest_pointer char array, then returns the offset. The functio is not case-sensitive.
 *
 * \param	needle The string you wish to find
 * \param	haystack The char array you want to search for \a needle
 * \param	max_len The maximum length of \a haystack
 * \param	beginning Whether it should return the offset to the beginning of the string or the end of the string
 * \retval Sint32	Returns either the offset to the beginning of the string or to the end of the string - if the string was not found in the char array it returns -1 on failure.
 */
Sint32 get_string_occurance (const char *needle, const char *haystack, const Uint32 max_len, const char beginning);

/*!
 * \ingroup	misc_utils
 * \brief	The function copies the string from source to dest, making sure it doesn't overflow and remains null terminated.  Strncpy doesn't guarantee the null termination.
 *
 * \param	dest The destination char array
 * \param	source The source char array
 * \param	len The sizeof the array.
 */
char* safe_strncpy(char *dest, const char * source, const size_t len);

/*!
 * \ingroup	misc_utils
 * \brief	The function copies the string from source to dest, making sure it doesn't overflow and remains null terminated, and furthermore that it doesn't copy more than a certain number of chars.  Strncpy doesn't guarantee the null termination.
 *
 * \param	dest The destination char array
 * \param	source The source char array
 * \param	dest_len The sizeof the destination array.
 * \param	src_len The desired number of characters from source.
 */
char* safe_strncpy2(char *dest, const char * source, const size_t dest_len, const size_t src_len);

/*!
 * \ingroup	misc_utils
 * \brief	Like snprintf, but guarentees nul termination.
 *
 * \param	dest The destination char array
 * \param	len The sizeof the destination array.
 * \param	format A printf-style format string
 * \param	... arguments to be passed to snprintf
 */
int safe_snprintf(char *dest, const size_t len, const char* format, ...);

/*!
 * \ingroup     misc_utils
 * \brief       Append string src to dest, guaranteeing null-termination
 *
 *              Append string \a src to \a dest, making sure that the result
 *              is null-terminated and contains at most \a len characters
 *              (including the terminating nullbyte).
 *              %Note that the "safe" predicate only applies to the
 *              result, both \a dest and \a src should be null-terminated
 *              on entry. Also note that this function is \em not the same
 *              as \c strncat: the third parameter to \c strncat is the
 *              number of characters to take from \a dest, not the total
 *              number of characters in the result string.
 *
 * \param       dest The string to append to
 * \param       src The string to be appended
 * \param       len The maximum size of the result string
 * \retval char* Pointer to the concatenated string dest
 */
char* safe_strcat (char *dest, const char *src, size_t len);

/*!
 * \ingroup	misc_utils
 * \brief	Locate a substring in a case-insensitive matter
 *
 *		Find the first occurence of string \a needle of length in
 *		\a haystack, checking at most the first \a needle_len bytes
 *		of \a needle and the first \a haystack_len bytes of
 *		\a haystack, and disregarding case. This function differs
 *		from (GNU's) memmem in that it is case-insensitive and
 *		does not compare bytes beyond a null-terminator.
 *
 * \param	haystack     The string to be searched
 * \param	haystack_len The length of \a haystack
 * \param	needle       The string to search for
 * \param	needle_len   The length of \a needle
 * \retval char* Pointer to the first occurence of the search string, or
 *		NULL when \a haystack does not contain \a needle.
 */
char* safe_strcasestr (const char* haystack, size_t haystack_len, const char* needle, size_t needle_len);

/*!
 * \ingroup	misc_utils
 * \brief	The function copies the string from source to dest
 *
 * 		The function copies the string from source to destination, and put a terminating \\0
 *
 * \param	dest The destination char array
 * \param	source The source char array
 * \todo	We should just use strcpy instead...
 */
void my_strcp(char *dest,const char * source);

/*!
 * \ingroup	misc_utils
 * \brief	The function copies the string from source to dest, but no more than n characters
 *
 * 		The function copies the string from source to destination, but no more than n characters. It also puts an ending \\0
 *
 * \param	dest The destination char array
 * \param	source The source char array
 * \param	len The number of bytes you wish to copy
 */
void my_strncp (char *dest, const char *source, size_t len);

/*!
 * \ingroup	misc_utils
 * \brief	Compares n bytes of the 2 strings (case insensitive)
 *
 * 		The function compares n bytes of the 2 strings. It is not case sensitive
 *
 * \param	dest The first string
 * \param	src The second string
 * \param	len The number of bytes to compare
 * \retval Sint32	Returns 1 on match, 0 if the strings doesn't match.
 */
Sint32 my_strncompare(const char *dest, const char *src, Sint32 len);

/*!
 * \ingroup	misc_utils
 * \brief	Compares the 2 strings
 *
 * 		The function compares the 2 strings, calls my_strncompare.
 *
 * \param	dest The first string
 * \param	src The second string
 * \retval Sint32 	Returns 1 on match, 0 if the strings doesn't match.
 * \sa my_strncompare
 */
Sint32 my_strcompare(const char *dest, const char *src);

/*!
 * \ingroup	misc_utils
 * \brief	Checks if len/2 characters of the string is uppercase
 *
 * 		Checks if len/2 characters of the string is uppercase
 *
 * \param	src The string to be checked
 * \param	len The length of characters you wish to check
 * \retval Sint32	Returns 1 if enough characters are uppercase, 0 if they are lowercase.
 */
Sint32 my_isupper(const char *src, int len);

/*!
 * \ingroup	misc_utils
 * \brief	Converts all characters in the string to lowercase
 *
 * 		Converts all characters in the string to lowercase
 *
 * \param	src The string to convert
 * \retval char*	Returns the src-pointer.
 */
char *my_tolower (char *src);

/*!
 * \ingroup	misc_utils
 * \brief	Splits up the char array into multiple character arrays
 *
 * 		Splits up the char array into multiple character arrays. The new arrays will have chars_per_line+3 bytes allocated. The char ** array will have a NULL pointer as the end pointer.
 *
 * \param	str The string to split
 * \param	chars_per_line The number of characters per line
 * \retval char**	Returns a char ** to the new array. You must free the memory yourself.
 */
char ** get_lines(char * str, int chars_per_line);

/*!
 * \ingroup	misc_utils
 * \brief	Goes through the file-name and replaces \\ with /
 *
 * 		Goes through the file-name and replaces \\ with /. Leaves the source intact, and copies the string to the destination.
 *
 * \param	dest The destination string
 * \param	src The source string
 * \param	max_len The maximum length
 * \retval Uint32	Returns the length of the string
 */
Uint32 clean_file_name (char *dest, const char *src, Uint32 max_len);

int get_file_digest(const char*, Uint8[16]);
void get_string_digest(const char*, Uint8[16]);

/*!
 * \brief Append char to the string given by s.
 *
 * \param[in,out] s pointer to the string, changes if reallocation was needed.
 * \param[in] c character to append to the string.
 * \paran[in,out] len actual length of the string s.
 * \param[in,out] max_len size of memory allocated for string s.
 *
 * \note In danger of poiting out the obvious, the character buffer \a s must
 *       be dynamically allocated and not a fixed size buffer, otherwise any
 *       necessary reallocations will fail.
 */
void append_char(char** s, char c, int* len, int* max_len);

/*!
 * \brief used in append_char(), buffer for string grows by this size when
 * reallocation is needed.
 */
#define APPEND_CHAR_BLOCK 256

/*!
 * \brief Replace all occurances of a character with a string
 *
 *	The src string is copied to the output string but with all occurances
 * of the to_sub character replaced with the with_sub string.  The user
 * can allocate the memory for out_str, or pass in NULL.  Either way the
 * caller must free the memory.  If a non-null block is passed, it may be
 * reallocated anyway.
 *
 * \param str The source string.
 * \param out_str Address of the pointer for the output buffer (may be NULL).
 * \param to_sub The charater to replace
 * \param with_sub the string to substitute
 * \return a pointer to the output string
 */
char *substitute_char_with_string(const char *str, char **out_str, char to_sub, const char* with_sub);





/*!
 * \brief Get a copy of a string truncated to be no wider than specified.
 *
 *	Where there is a limited space for a string to be drawn, get a
 *  truncated copy (including the append text). The destination string
 *  must have space for the terminating null and the appended string,
 *  otherwise it will be stop at the maximum length.
 *
 * \param dest The destination string
 * \param source The source string
 * \param dest_max_len The maximum length of the destination string including the terminating '\0'
 * \param append_str The string to place on the end of the truncated copy, normally "... "
 * \param max_len_x The maximum x length in pixels of the truncated string including the appended string
 * \param font_ratio The font zoom size of the text
 * \return a pointer to the destination string
 */
char *truncated_string(char *dest, const char *source, size_t dest_max_len, const char *append_str, float max_len_x, float font_ratio);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
