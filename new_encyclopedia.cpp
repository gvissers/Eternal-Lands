#include <algorithm>
#include <sstream>
#include "new_encyclopedia.h"
#include "client_serv.h"
#include "elconfig.h"
#include "interface.h"
#include "text.h"
#include "textures.h"
#include "translate.h"
#include "url.h"

namespace
{

/*!
 * \brief Retrieve attribute disregarding case
 *
 * Return the value of the first attribute of \a node that equals attribute name \a name without
 * regards to case. If the attribute is not found, the result depends on the \a required: if it is
 * \c true, an ExtendedException is thrown, otherwise \c nullptr is returned.
 * \note This returns a pointer to the character data stored inside the node itself; there is no
 * need to use xmlFree() on the returned data as for xmlGetProp().
 * \param node     The node to search
 * \param name     The name of the attribute to search for
 * \param required If true, throw an exception if the attribute is not found
 * \retval xmlNode* The value of the attribute, or \c nullptr if no such attribute exists.
 */
const xmlChar* get_xml_attribute(const xmlNode* node, const char* name, bool required=false)
{
	for (const xmlAttr *attr = node->properties; attr; attr = attr->next)
	{
		if (attr->type==XML_ATTRIBUTE_NODE // Can it actually be anything else?
			&& !strcasecmp(reinterpret_cast<const char*>(attr->name), name))
		{
			return attr->children->content;
		}
	}
	if (required)
	{
		EXTENDED_EXCEPTION(ExtendedException::ec_item_not_found,
			"required attribute \"" << name << "\" not found for node \""
				<< reinterpret_cast<const char*>(node->name) << '"');
	}
	return nullptr;
}

std::pair<bool, bool> get_xml_bool_attribute(const xmlNode* node, const char* name)
{
	const char* value = reinterpret_cast<const char*>(get_xml_attribute(node, name));
	if (value && *value)
	{
		if (!strcasecmp(value, "true") || !strcasecmp(value, "yes"))
			return std::make_pair(true, true);
		if (!strcasecmp(value, "false") || !strcasecmp(value, "no"))
			return std::make_pair(false, true);
		try
		{
			int res = std::stoi(value);
			return std::make_pair(res != 0, true);
		}
		catch (const std::exception&)
		{
			LOG_ERROR("Invalid value \"%s\" for attribute \"%s\", expected boolean");
		}
	}
	return std::make_pair(false, false);
}

std::pair<int, bool> get_xml_int_attribute(const xmlNode* node, const char* name, bool required=false)
{
	const char* value = reinterpret_cast<const char*>(get_xml_attribute(node, name, required));
	if (!value)
		return std::make_pair(0, false);

	try
	{
		int res = std::stoi(value);
		return std::make_pair(res, true);
	}
	catch (const std::invalid_argument&)
	{
		if (required)
		{
			EXTENDED_EXCEPTION(ExtendedException::ec_invalid_parameter,
				"Invalid value \"" << value << "\" for attribute \"" << name << "\", expected integer");
		}
		else
		{
			LOG_ERROR("Invalid value \"%s\" for attribute \"%s\", expected integer", value, name);
			return std::make_pair(0, false);
		}
	}
	catch (const std::out_of_range&)
	{
		if (required)
		{
			EXTENDED_EXCEPTION(ExtendedException::ec_invalid_parameter,
				"Value \"" << value << "\" for attribute \"" << name << "\" is out of range");
		}
		else
		{
			LOG_ERROR("Value \"%s\" for attribute \"%s\" is out of range");
			return std::make_pair(0, false);
		}
	}
}

std::pair<float, bool> get_xml_float_attribute(const xmlNode* node, const char* name)
{
	const xmlChar* value = get_xml_attribute(node, name);
	if (value)
	{
		try
		{
			float res = std::stof(reinterpret_cast<const char*>(value));
			return std::make_pair(res, true);
		}
		catch (const std::invalid_argument&)
		{
			LOG_ERROR("Invalid value \"%s\" for attribute \"%s\", expected floating point number");
		}
		catch (const std::out_of_range&)
		{
			LOG_ERROR("Value \"%s\" for attribute \"%s\" is out of range");
		}
	}
	return std::make_pair(0.0f, false);
}

std::string trim(const std::string &str)
{
	auto begin = std::find_if_not(str.begin(), str.end(), [](int c) { return std::isspace(c); });
	auto end = std::find_if_not(str.rbegin(), str.rend(),[](int c) { return std::isspace(c); }).base();
	return end <= begin ? std::string() : std::string(begin, end);
}

} // namespace

namespace eternal_lands
{

const EncyclopediaPageElementColor EncyclopediaFormattedText::link_mouseover_color{0.3, 0.6, 1.0};

EncyclopediaPageElementColor::EncyclopediaPageElementColor(const xmlNode *node):
	r(get_xml_float_attribute(node, "r").first),
	g(get_xml_float_attribute(node, "g").first),
	b(get_xml_float_attribute(node, "b").first)
{
	static const std::array<std::pair<std::string, std::array<unsigned char, 3> >, 16> known_colors {{
		{ "silver",  std::array<unsigned char, 3>{ 192, 192, 192 } },
		{ "grey",    std::array<unsigned char, 3>{ 128, 128, 128 } },
		{ "maroon",  std::array<unsigned char, 3>{ 128,   0,   0 } },
		{ "green",   std::array<unsigned char, 3>{   0, 128,   0 } },
		{ "navy",    std::array<unsigned char, 3>{   0,   0, 128 } },
		{ "olive",   std::array<unsigned char, 3>{ 128, 128,   0 } },
		{ "purple",  std::array<unsigned char, 3>{ 128,   0, 128 } },
		{ "teal",    std::array<unsigned char, 3>{ 0,   128, 128 } },
		{ "white",   std::array<unsigned char, 3>{ 255, 255, 255 } },
		{ "black",   std::array<unsigned char, 3>{   0,   0,   0 } },
		{ "red",     std::array<unsigned char, 3>{ 255,   0,   0 } },
		{ "lime",    std::array<unsigned char, 3>{   0, 255,   0 } },
		{ "blue",    std::array<unsigned char, 3>{   0,   0, 255 } },
		{ "magenta", std::array<unsigned char, 3>{ 255,   0, 255 } },
		{ "yellow",  std::array<unsigned char, 3>{ 255, 255,   0 } },
		{ "cyan",    std::array<unsigned char, 3>{   0, 255,   0 } }
	}};

	if (node->children && node->children->content)
	{
		std::string desc = trim(reinterpret_cast<const char*>(node->children->content));
		if (!desc.empty())
		{
			for (const auto& color: known_colors)
			{
				if (color.first == desc)
				{
					r = float(color.second[0]) / 255;
					g = float(color.second[1]) / 255;
					b = float(color.second[2]) / 255;
					return;
				}
			}

			EXTENDED_EXCEPTION(ExtendedException::ec_invalid_parameter, "Unknown color \"" << desc << '"');
		}
	}
}

EncyclopediaPageElementText::EncyclopediaPageElementText(const xmlNode *node):
	text(node->children->content), have_x(false), have_y(false), x(), y()
{
	std::tie(x, have_x) = get_xml_int_attribute(node, "x");
	std::tie(y, have_y) = get_xml_int_attribute(node, "y");
}

EncyclopediaPageElementImage::EncyclopediaPageElementImage(const xmlNode *node,
	EncyclopediaPageElementImageType type):
	type(type), texture_id(Uint32(-1)),
	u_start(0.0f), v_start(0.0f), u_end(1.0f), v_end(1.0f),
	have_x(false), have_y(false),
	x(), y(), width(), height(),
	mouseover(false), update_x(true), update_y(true)
{
	const xmlChar* texture_name = get_xml_attribute(node, "name", true);
	texture_id = load_texture_cached(reinterpret_cast<const char*>(texture_name), tt_gui);

	std::tie(x, have_x) = get_xml_int_attribute(node, "x");
	std::tie(y, have_y) = get_xml_int_attribute(node, "y");
	bool ok, val;
	std::tie(val, ok) = get_xml_bool_attribute(node, "mouseover");
	if (ok)
		mouseover = val;
	std::tie(val, ok) = get_xml_bool_attribute(node, "xposupdate");
	if (ok)
		update_x = val;
	std::tie(val, ok) = get_xml_bool_attribute(node, "yposupdate");
	if (ok)
		update_y = val;

	if (type == EncyclopediaPageElementImageType::Image)
	{
		u_start = get_xml_float_attribute(node, "u").first;
		v_start = get_xml_float_attribute(node, "v").first;
		float val;
		std::tie(val, ok) = get_xml_float_attribute(node, "uend");
		if (ok)
			u_end = val;
		std::tie(val, ok) = get_xml_float_attribute(node, "vend");
		if (ok)
			v_end = val;
		width = get_xml_int_attribute(node, "xlen", true).first;
		height = get_xml_int_attribute(node, "ylen", true).first;
	}
	else if (type == EncyclopediaPageElementImageType::SImage)
	{
		int img_size = get_xml_int_attribute(node, "isize", true).first;
		int block_size = get_xml_int_attribute(node, "tsize", true).first;
		int img_nr = get_xml_int_attribute(node, "tid", true).first;
		int pixels_per_row = img_size / block_size;
		int col = img_nr % pixels_per_row;
		int row = img_nr / pixels_per_row;
		float rel_block_size = float(block_size) / img_size;
		u_start = col * rel_block_size;
		u_end = (col + 1) * rel_block_size;
		v_start = row * rel_block_size;
		v_end = (row + 1) * rel_block_size;

		int scale;
		std::tie(scale, ok) = get_xml_int_attribute(node, "size");
		if (ok)
		{
			width = height = (block_size * scale) / 100;
		}
		else
		{
			width = height = block_size;
		}
	}
}

EncyclopediaPageElementPosition::EncyclopediaPageElementPosition(const xmlNode *node):
	x_pixels(get_xml_int_attribute(node, "x", true).first), x_char_big(0), x_char_small(0),
	y_pixels(get_xml_int_attribute(node, "x", true).first) {}

EncyclopediaPageElementLink::EncyclopediaPageElementLink(const xmlNode *node):
	text(get_xml_attribute(node, "title", true)),
	target(reinterpret_cast<const char*>(get_xml_attribute(node, "ref", true))),
	have_x(false), have_y(false), x(), y()
{
	std::tie(x, have_x) = get_xml_int_attribute(node, "x");
	std::tie(y, have_y) = get_xml_int_attribute(node, "y");
}

EncyclopediaPageElement::EncyclopediaPageElement(EncyclopediaPageElement&& elem):
	_type(elem._type)
{
	switch (_type)
	{
		case EncyclopediaPageElementType::Color:
			new(&_color) EncyclopediaPageElementColor(elem._color);
			break;
		case EncyclopediaPageElementType::Image:
			new(&_image) EncyclopediaPageElementImage(elem._image);
			break;
		case EncyclopediaPageElementType::Link:
			new(&_link) EncyclopediaPageElementLink(std::move(elem._link));
			break;
		case EncyclopediaPageElementType::Position:
			new(&_position) EncyclopediaPageElementPosition(elem._position);
			break;
		case EncyclopediaPageElementType::Size:
			new(&_size) EncyclopediaPageElementSize(elem._size);
			break;
		case EncyclopediaPageElementType::Text:
			new(&_text) EncyclopediaPageElementText(std::move(elem._text));
			break;
		default: /* nothing to copy */ ;
	}
}

EncyclopediaPageElement::~EncyclopediaPageElement()
{
	switch (_type)
	{
		case EncyclopediaPageElementType::Link: _link.~EncyclopediaPageElementLink(); break;
		case EncyclopediaPageElementType::Text: _text.~EncyclopediaPageElementText(); break;
		default: /* nothing to destruct */ ;
	}
}

void EncyclopediaFormattedImage::display(const window_info *win, int y_min) const
{
	glColor3f(1.0f, 1.0f, 1.0f);
	::bind_texture(_texture_id);
	glBegin(GL_QUADS);
	draw_2d_thing(_u_start, _v_start, _u_end, _v_end, x(), y() - y_min, x_end(), y_end() - y_min);
	glEnd();
}

void EncyclopediaFormattedText::display(const window_info *win, int y_min) const
{
	const EncyclopediaPageElementColor& color = is_under_mouse() ? _mouseover_color : _color;
	TextDrawOptions options = TextDrawOptions().set_foreground(color.r, color.g, color.b)
		.set_zoom(_scale);
	FontManager::get_instance().draw(win->font_category, _text.data(), _text.size(),
		x(), y() - y_min, options);
	if (_is_link)
	{
		// Add line under link text
		glColor3f(0.5,0.5,0.5);
		glDisable(GL_TEXTURE_2D);
		glBegin(GL_LINES);
		glVertex3i(x() + 4, y() + height() - y_min, 0);
		glVertex3i(x() + width() - 4, y() + height() - y_min, 0);
		glEnd();
		glEnable(GL_TEXTURE_2D);
	}
}

void EncyclopediaPage::read_xml(const xmlNode* node)
{
	for (const xmlNode *cur_node = node->children; cur_node; cur_node = cur_node->next)
	{
		if (cur_node->type != XML_ELEMENT_NODE)
			continue;

		try
		{
			const char* name = reinterpret_cast<const char*>(cur_node->name);
			if (!strcasecmp(name, "color"))
			{
				_elements.emplace_back(EncyclopediaPageElementColor(cur_node));
			}
			else if (!strcasecmp(name, "ddsimage") || !strcasecmp(name, "simage"))
			{
				_elements.emplace_back(EncyclopediaPageElementImage(cur_node, EncyclopediaPageElementImageType::SImage));
			}
			else if (!strcasecmp(name, "image"))
			{
				_elements.emplace_back(EncyclopediaPageElementImage(cur_node, EncyclopediaPageElementImageType::Image));
			}
			else if (!strcasecmp(name, "link"))
			{
				_elements.emplace_back(EncyclopediaPageElementLink(cur_node));
			}
			else if (!strcasecmp(name, "nl"))
			{
				_elements.emplace_back(EncyclopediaPageElementType::Newline);
			}
			else if (!strcasecmp(name, "nlkx"))
			{
				_elements.emplace_back(EncyclopediaPageElementType::NewlineKeepX);
			}
			else if (!strcasecmp(name, "pos"))
			{
				_elements.emplace_back(EncyclopediaPageElementPosition(cur_node));
			}
			else if (!strcasecmp(name, "size"))
			{
				const char* content = reinterpret_cast<const char*>(cur_node->children->content);
				if (content)
				{
					if (!strcasecmp(content, "small"))
						_elements.push_back(EncyclopediaPageElementSize::Small);
					else if (!strcasecmp(content, "medium"))
						_elements.emplace_back(EncyclopediaPageElementSize::Medium);
					else if (!strcasecmp(content, "big"))
						_elements.emplace_back(EncyclopediaPageElementSize::Big);
				}
			}
			else if (!strcasecmp(name, "text"))
			{
				_elements.emplace_back(EncyclopediaPageElementText(cur_node));
			}
		}
		CATCH_AND_LOG_EXCEPTIONS;
	}
}

void EncyclopediaPage::layout(const window_info *win)
{
	float x_scale = float(win->default_font_max_len_x) / DEFAULT_FIXED_FONT_WIDTH;
	float y_scale = float(win->default_font_len_y) / DEFAULT_FIXED_FONT_HEIGHT;
	float min_scale = std::min(x_scale, y_scale);
	int offset = 2;

	EncyclopediaPageElementColor cur_color(c_grey1);
	EncyclopediaPageElementPosition cur_position(offset, offset);
	bool cur_size_is_big = true;
	int last_len_big = 0, last_len_small = 0;
	for (const auto& element: _elements)
	{
		switch (element.type())
		{
			case EncyclopediaPageElementType::Color:
				cur_color = element.color();
				break;
			case EncyclopediaPageElementType::Image:
			{
				const EncyclopediaPageElementImage& image = element.image();
				int width = std::round(min_scale * image.width);
				int height = std::round(min_scale * image.height);
				if (image.mouseover)
				{
					int x = std::round(x_scale * image.x);
					int y = std::round(y_scale * image.y);
					_formatted.emplace_back(new EncyclopediaFormattedImage(x, y, width, height,
						image.texture_id, image.u_start, image.v_start, image.u_end, image.v_end));
				}
				else
				{
					int x = image.have_x ? std::round(x_scale * image.x) : cur_position.x_scaled(win);
					int y = image.have_y ? std::round(y_scale * image.y) : cur_position.y_scaled(win);
					_formatted.emplace_back(new EncyclopediaFormattedImage(x, y, width, height,
						image.texture_id, image.u_start, image.v_start, image.u_end, image.v_end));
					if (image.update_x)
						cur_position.add_x(image.width);
					if (image.update_y)
					{
						cur_position.add_y(image.height
							- (cur_size_is_big ? DEFAULT_FIXED_FONT_HEIGHT : SMALL_FIXED_FONT_HEIGHT));
					}
				}
				break;
			}
			case EncyclopediaPageElementType::Link:
			{
				const EncyclopediaPageElementLink& link = element.link();
				int x = link.have_x ? std::round(x_scale * link.x) : cur_position.x_scaled(win);
				int y = link.have_y ? std::round(y_scale * link.y) : cur_position.y_scaled(win);
				float text_scale = cur_size_is_big ? win->current_scale : win->current_scale_small;
				int width, height;
				std::tie(width, height) = FontManager::get_instance().dimensions(win->font_category,
					link.text.data(), link.text.size(), text_scale);
				_formatted.emplace_back(
					new EncyclopediaFormattedText(x, y, width, height, link.text, cur_color, text_scale, true)
				);

				if (cur_size_is_big)
				{
					last_len_big = link.text.size();
					last_len_small = 0;
				}
				else
				{
					last_len_big = 0;
					last_len_small = link.text.size();
				}

				if (link.have_x)
					cur_position.set_x(link.x);
				cur_position.add_char_counts(last_len_big, last_len_small);
				if (link.have_y)
					cur_position.set_y(link.y);

				_links.emplace_back(link.target, x, y, width, height);

				break;
			}
			case EncyclopediaPageElementType::Newline:
				cur_position.set_x(offset);
				cur_position.add_y(cur_size_is_big ? DEFAULT_FIXED_FONT_HEIGHT : SMALL_FIXED_FONT_HEIGHT);
				break;
			case EncyclopediaPageElementType::NewlineKeepX:
				cur_position.sub_char_counts(last_len_big, last_len_small);
				cur_position.add_y(cur_size_is_big ? DEFAULT_FIXED_FONT_HEIGHT : SMALL_FIXED_FONT_HEIGHT);
				break;
			case EncyclopediaPageElementType::Position:
				cur_position = element.position();
				break;
			case EncyclopediaPageElementType::Size:
				cur_size_is_big = element.size() == EncyclopediaPageElementSize::Big;
				break;
			case EncyclopediaPageElementType::Text:
			{
				const EncyclopediaPageElementText& text = element.text();
				int x = text.have_x ? std::round(x_scale * text.x) : cur_position.x_scaled(win);
				int y = text.have_y ? std::round(y_scale * text.y) : cur_position.y_scaled(win);
				float text_scale = cur_size_is_big ? win->current_scale : win->current_scale_small;
				int width, height;
				std::tie(width, height) = FontManager::get_instance().dimensions(win->font_category,
					text.text.data(), text.text.size(), text_scale);
				_formatted.emplace_back(
					new EncyclopediaFormattedText(x, y, width, height, text.text, cur_color, text_scale, false)
				);

				if (cur_size_is_big)
				{
					last_len_big = text.text.size();
					last_len_small = 0;
				}
				else
				{
					last_len_big = 0;
					last_len_small = text.text.size();
				}

				if (text.have_x)
					cur_position.set_x(text.x);
				cur_position.add_char_counts(last_len_big, last_len_small);
				if (text.have_y)
					cur_position.set_y(text.y);

				break;
			}
		}
	}

	_height = 0;
	for (const auto& element: _formatted)
		_height = std::max(_height, element->y() + element->height());

	_laid_out = true;
}

void EncyclopediaPage::display(const window_info *win, int y_min)
{
	if (!_laid_out)
		layout(win);

	int y_max = y_min + win->len_y;
	for (const auto& element: _formatted)
	{
		if (element->is_visible(y_min, y_max))
		{
			element->display(win, y_min);
		}
	}
}

const std::string& EncyclopediaPage::link_clicked(int x, int y) const
{
	static const std::string empty;

	for (const auto& link: _links)
	{
		if (link.contains(x, y))
			return link.target();
	}
	return empty;
}

void EncyclopediaCategory::read_xml(const xmlNode* node)
{
	for (const xmlNode *cur_node = node->children; cur_node; cur_node = cur_node->next)
	{
		if (cur_node->type == XML_ELEMENT_NODE
			&& !strcasecmp(reinterpret_cast<const char*>(cur_node->name), "Page"))
		{
			try
			{
				const char* name = reinterpret_cast<const char*>(get_xml_attribute(cur_node, "name"));
				EncyclopediaPage page(name ? name : "");
				page.read_xml(cur_node);
				_pages.push_back(std::move(page));
			}
			catch (const ExtendedException& exc)
			{
				LOG_ERROR("Failed to read page in category \"%s\": %s\n", name(), exc.what());
			}
		}
	}
}

Encyclopedia::Encyclopedia(const std::string& file_name): _categories(), _pages()
{
	xmlDocPtr doc = nullptr;
	try
	{
		std::string path = std::string("languages/") + lang + '/' + file_name;
		doc = xmlReadFile(path.c_str(), nullptr, 0);
		if (!doc)
		{
			path = "languages/en/" + file_name;
			doc = xmlReadFile(path.c_str(), nullptr, 0);
			if (!doc)
			{
				std::string err = std::string(cant_open_file) + " \"" + path + '"';
				EXTENDED_EXCEPTION(ExtendedException::ec_file_not_found, err);
			}
		}

		xmlNode *root = xmlDocGetRootElement(doc);
		if (!root)
			EXTENDED_EXCEPTION(ExtendedException::ec_invalid_parameter, "Error while parsing: " << path);
		read_xml(root);

		xmlFreeDoc(doc);
	}
	catch (const std::exception& exc)
	{
		if (doc)
			xmlFreeDoc(doc);
		LOG_ERROR("Failed to read encyclopedia from file \"%s\": %s", file_name.c_str(), exc.what());
	}

// 	// Sanitize all of the page lengths.
// 	for (i = 0; i < numpage+1; i++) {
// 		if(Page[i].max_y > encyclopedia_menu_y_len - ENCYC_OFFSET)
// 		{
// 			Page[i].max_y -= encyclopedia_menu_y_len - ENCYC_OFFSET;
// 		} else {
// 			Page[i].max_y = 0;
// 		}
// 	}
}

void Encyclopedia::read_xml(const xmlNode* node)
{
	for (const xmlNode *cur_node = node->children; cur_node; cur_node = cur_node->next)
	{
		if (cur_node->type == XML_ELEMENT_NODE
			&& !strcasecmp(reinterpret_cast<const char*>(cur_node->name), "Category"))
		{
			try
			{
				EncyclopediaCategory category(reinterpret_cast<const char*>(cur_node->children->content));

				std::string file_name = std::string("languages/") + lang + "/Encyclopedia/" + category.name() + ".xml";
				xmlDocPtr doc = xmlReadFile(file_name.c_str(), nullptr, 0);
				if (!doc)
				{
					file_name = "languages/en/Encyclopedia/" + category.name() + ".xml";
					doc = xmlReadFile(file_name.c_str(), nullptr, 0);
					if (!doc)
					{
						std::string err = std::string(cant_open_file) + " \"" + file_name + '"';
						EXTENDED_EXCEPTION(ExtendedException::ec_file_not_found, err);
					}
				}

				xmlNode *root = xmlDocGetRootElement(doc);
				if (!root)
				{
					xmlFreeDoc(doc);
					EXTENDED_EXCEPTION(ExtendedException::ec_invalid_parameter,
						"Error while parsing: " << file_name);
				}

				try
				{
					category.read_xml(root);
				}
				catch (...)
				{
					xmlFreeDoc(doc);
					throw;
				}

				xmlFreeDoc(doc);

				for (auto& page: category)
					_pages.insert(std::make_pair(page.name(), &page));
				_categories.push_back(std::move(category));
			}
			catch (const ExtendedException& exc)
			{
				LOG_TO_CONSOLE(c_red1, exc.what());
			}
		}
	}
}

void EncyclopediaWindow::initialize(int window_id)
{
	_window_id = window_id;

	set_window_custom_scale(_window_id, MW_HELP);
	set_window_font_category(_window_id, ENCYCLOPEDIA_FONT);
	set_window_handler(_window_id, ELW_HANDLER_DISPLAY, (int (*)())&static_display_handler);
	set_window_handler(_window_id, ELW_HANDLER_MOUSEOVER, (int (*)())&static_mouseover_handler);
	set_window_handler(_window_id, ELW_HANDLER_CLICK, (int (*)())&static_click_handler);
	set_window_handler(_window_id, ELW_HANDLER_RESIZE, (int (*)())&static_resize_handler);
// 	set_window_handler(window_id, ELW_HANDLER_UI_SCALE, &ui_scale_encyclopedia_handler);
// 	set_window_handler(window_id, ELW_HANDLER_FONT_CHANGE, &change_encyclopedia_font_handler);

	set_minimum_size();

	_scroll_id = vscrollbar_add_extended(_window_id, 1, nullptr, 0, 0, 0, 0, 0, 1.0, 0, 30, 0);
	try
	{
		set_current_page(&windows_list.window[_window_id], _encyclopedia.first_page_name());
	}
	CATCH_AND_LOG_EXCEPTIONS;

	if (!_current_page)
	{
		LOG_TO_CONSOLE(c_red1, cant_load_encycl);
	}
	else
	{
// 	set_window_handler(window_id, ELW_HANDLER_KEYPRESS, (int (*)())&keypress_encyclopedia_handler);
//
// 	if (!cm_valid(cm_encycl))
// 	{
// 		cm_encycl = cm_create(cm_encycl_base_str, cm_encycl_handler);
// 		cm_set_pre_show_handler(cm_encycl, cm_encycl_pre_show_handler);
// 		cm_add_window(cm_encycl, window_id);
// 		init_ipu(&ipu_encycl, -1, 1, 1, 1, NULL, NULL);
// 		find_base_pages();
// 		process_encycl_links();
// 	}
	}
}

void EncyclopediaWindow::set_minimum_size()
{
	if (_window_id >= 0 && _window_id < windows_list.num_windows)
	{
		window_info* window = &windows_list.window[_window_id];
		int min_width = max2i(63 * window->small_font_max_len_x, 46 * window->default_font_max_len_x);
		int min_height = max2i(24 * window->small_font_len_y, 20 * window->default_font_len_y);
		set_window_min_size(_window_id, min_width, min_height);
	}
}

int EncyclopediaWindow::display_handler(window_info *win)
{
	if (_current_page)
	{
		int y_min = vscrollbar_get_pos(_window_id, _scroll_id);
		_current_page->display(win, y_min);
	}
	return 0;
}

int EncyclopediaWindow::mouseover_handler(window_info *win, int mouse_x, int mouse_y)
{
	if (!_current_page)
		return 0;

	int y_min = vscrollbar_get_pos(_window_id, _scroll_id);
	_current_page->mouseover(mouse_x, mouse_y + y_min);
	return 1;
}

int EncyclopediaWindow::click_handler(window_info *win, int mx, int my, std::uint32_t flags)
{
	if (flags &ELW_WHEEL_UP)
	{
		vscrollbar_scroll_up(_window_id, _scroll_id);
	}
	else if (flags & ELW_WHEEL_DOWN)
	{
		vscrollbar_scroll_down(_window_id, _scroll_id);
	}
	else if (_current_page)
	{
		int y_min = vscrollbar_get_pos(_window_id, _scroll_id);
		const std::string& target = _current_page->link_clicked(mx, my + y_min);
		if (!target.empty())
		{
			if (target.compare(0, 7, "http://") == 0)
			{
				open_web_link(target.c_str());
			}
			else
			{
				set_current_page(win, target);
			}
		}
	}

	return 1;
}

int EncyclopediaWindow::resize_handler(const window_info *win, int new_width, int new_height)
{
	widget_resize(_window_id, _scroll_id, win->box_size, win->len_y);
	widget_move(_window_id, _scroll_id, win->len_x - win->box_size, 0);

	// Adjust layout of current page (and scrollbar length)
	_encyclopedia.invalidate_layout();
	if (_current_page)
		set_current_page(win, _current_page->name());

	return 0;
}

void EncyclopediaWindow::set_current_page(const window_info *win, const std::string& page_name)
{
	EncyclopediaPage *page = _encyclopedia.find_page(page_name);
	if (page)
	{
		int scroll_margin = win->default_font_len_y;

		page->layout_if_needed(win);
		_current_page = page;
		vscrollbar_set_pos(_window_id, _scroll_id, 0);
		vscrollbar_set_bar_len(_window_id, _scroll_id,
			std::max(0, _current_page->height() - win->len_y + scroll_margin));
	}
}

int EncyclopediaWindow::static_display_handler(window_info *win)
{
	return EncyclopediaWindow::get_instance().display_handler(win);
}

int EncyclopediaWindow::static_mouseover_handler(window_info *win, int mouse_x, int mouse_y)
{
	return EncyclopediaWindow::get_instance().mouseover_handler(win, mouse_x, mouse_y);
}

int EncyclopediaWindow::static_click_handler(window_info *win, int mx, int my, std::uint32_t flags)
{
	return EncyclopediaWindow::get_instance().click_handler(win, mx, my, flags);
}

int EncyclopediaWindow::static_resize_handler(const window_info *win, int new_width, int new_height)
{
	return EncyclopediaWindow::get_instance().resize_handler(win, new_width, new_height);
}

} // namespace eternal_lands

extern "C"
void fill_new_encyclopedia_win(int window_id)
{
	eternal_lands::EncyclopediaWindow::get_instance().initialize(window_id);
}
