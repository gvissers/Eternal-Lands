#include <algorithm>
#include <cassert>
#include "asc.h"
#ifndef XML_COMPILED
#include "xml/xmlhelper.hpp"
#endif // XML_COMPILED
#include "platform.h"
#include "multiplayer.h"
#include "client_serv.h"
#include "init.h"
#include "errors.h"
#include "sendvideoinfo.h"
#include "io/elfilewrapper.h"

#ifdef XML_COMPILED
#include "extensions.inc.cpp"
#endif

namespace
{
	const int vendor_names_count = 5;

	const char* vendor_names[vendor_names_count] =
	{
		"ATI", "NVIDIA", "INTEL", "SIS", "TRIDENT"
	};

	const int opengl_versions_count = 8;

	const char* opengl_versions[opengl_versions_count] =
	{
		"1.0", "1.1", "1.2", "1.3", "1.4", "1.5", "2.0", "2.1"
	};

	const int bit_set_96_size = 12;
	typedef	char bit_set_96[bit_set_96_size];

	inline void set_bit(bit_set_96 bits, int bit)
	{
		int n, i;

		assert((bit >= 0) && (bit < 96));

		i = bit % 8;
		n = bit / 8;
		bits[n] |= 1 << i;
	}

	inline int get_first_set_bit(int value)
	{
		int i;

		i = 0;

		while ((1 << i) < value)
		{
			i++;
		}

		return i;
	}

#ifndef XML_COMPILED
	inline void parse_extention(const xmlNodePtr extension_element, std::string extensions, bit_set_96 bits)
	{
		xmlNodePtr cur_node;
		std::string name;
		int value;

		NODE_NAME_CHECK(extension_element, "extention");

		cur_node = get_node_element_children(extension_element);

		NODE_NAME_CHECK(cur_node, "name");

		name = get_value_from_node<std::string>(cur_node);

		cur_node = get_next_element_node(cur_node);

		NODE_NAME_CHECK(cur_node, "value");

		value = get_value_from_node<unsigned int>(cur_node);

		if (extensions.find(name) != extensions.npos)
		{
			set_bit(bits, value);
		}
	}

	inline void parse_extentions(const xmlNodePtr extensions_element,
		const char* extensions, bit_set_96 bits)
	{
		xmlNodePtr cur_node;

		NODE_NAME_CHECK(extensions_element, "extentions");

		for (cur_node = get_node_element_children(extensions_element); cur_node != 0;
			cur_node = get_next_element_node(cur_node))
		{
			parse_extention(cur_node, extensions, bits);
		}
	}
#endif // XML_COMPILED

	inline int get_version_idx()
	{
		std::string str;
		int i;

		str = reinterpret_cast<const char *>(glGetString(GL_VERSION));

		for (i = 0; i < opengl_versions_count; i++)
		{
			if (str.find(opengl_versions[i]) == 0)
			{
				return i;
			}
		}

		return 0xFF;
	}

	inline int get_vendor_idx()
	{
		std::string str;
		int i;

		str = reinterpret_cast<const char *>(glGetString(GL_VENDOR));

		std::transform(str.begin(), str.end(), str.begin(), toupper);

		for (i = 0; i < vendor_names_count; i++)
		{
			if (str.find(vendor_names[i]) == 0)
			{
				return i;
			}
		}

		return 0xFF;
	}

	void do_send_video_info(const bit_set_96 caps)
	{
		Uint8 data[33];
		GLint i;

		data[0] = SEND_VIDEO_INFO;
		data[1] = get_vendor_idx();
		data[2] = get_version_idx();

		glGetIntegerv(GL_MAX_TEXTURE_SIZE, &i);
		data[3] = get_first_set_bit(i);

		glGetIntegerv(GL_MAX_TEXTURE_UNITS, &i);
		data[4] = i;

		glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &i);
		data[5] = i % 256;
		data[6] = i / 256;

		glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &i);
		data[7] = i % 256;
		data[8] = i / 256;

		std::copy(caps, caps+bit_set_96_size, data+9);
		if (my_tcp_send(my_socket, data, sizeof(data)) < static_cast<int>(sizeof(data)))
		{
			LOG_ERROR("Error sending video info");
		}
		else
		{
			video_info_sent = 1;
		}
		my_tcp_flush(my_socket);
	}

} // namespace

#include <iomanip>
extern "C" void send_video_info()
{
	if (video_info_sent == 0)
	{
		std::string extensions = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
		bit_set_96 caps;
		std::fill(caps, caps+bit_set_96_size, 0);

#ifdef XML_COMPILED
		for (int i = 0; i < nr_extension_names; ++i)
		{
			if (extensions.find(extension_names[i]) != extensions.npos)
				set_bit(caps, i);
		}
		do_send_video_info(caps);
#else // XML_COMPILED
		xmlDoc *document = xmlReadFile("extentions.xml", NULL, 0);
		if (document)
		{
			/*Get the root element node */
			xmlNodePtr root_element = xmlDocGetRootElement(document);

			if (root_element != 0)
			{
				try
				{
					parse_extentions(root_element, extensions, caps);
				}
				CATCH_AND_LOG_EXCEPTIONS

				do_send_video_info(caps);
			}
		}
		xmlFree(document);
#endif // XML_COMPILED
	}
}

