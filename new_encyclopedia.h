#ifndef NEW_ENCYCLOPEDIA_H
#define NEW_ENCYCLOPEDIA_H

#ifdef __cplusplus

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <libxml/tree.h>
#include "colors.h"
#include "elwindows.h"
#include "exceptions/extendedexception.hpp"
#include "font.h"

namespace eternal_lands
{

enum class EncyclopediaPageElementType
{
	Color,
	Image,
	Link,
	Position,
	Size,
	Newline,
	NewlineKeepX,
	Text
};

enum class EncyclopediaPageElementSize
{
	Small,
	Medium,
	Big
};

struct EncyclopediaPageElementColor
{
	float r, g, b;

	EncyclopediaPageElementColor(int idx): r(colors_list[idx].r1), g(colors_list[idx].g1), b(colors_list[idx].b1) {}
	EncyclopediaPageElementColor(const xmlNode *node);
};

struct EncyclopediaPageElementText
{
	ustring text;
	bool have_x, have_y;
	int x, y;

	EncyclopediaPageElementText(const xmlNode* node);
};

enum class EncyclopediaPageElementImageType
{
	Image,
	SImage
};

struct EncyclopediaPageElementImage
{
	EncyclopediaPageElementImageType type;
	std::uint32_t texture_id;
	float u_start, v_start, u_end, v_end;
	bool have_x, have_y;
	int x, y, width, height;
	bool mouseover, update_x, update_y;

	EncyclopediaPageElementImage(const xmlNode *node, EncyclopediaPageElementImageType type);
};

struct EncyclopediaPageElementPosition
{
	int x_pixels, x_char_big, x_char_small;
	int y_pixels;

	EncyclopediaPageElementPosition(int x, int y):
		x_pixels(x), x_char_big(0), x_char_small(0), y_pixels(y) {}
	EncyclopediaPageElementPosition(const xmlNode *node);

	int x_scaled(float scale) const
	{
		return std::round(scale
			* (x_pixels + x_char_big * DEFAULT_FIXED_FONT_WIDTH + x_char_small * SMALL_FIXED_FONT_WIDTH));
	}
	int y_scaled(float scale) const
	{
		return std::round(scale * y_pixels);
	}

	void set(int x, int y)
	{
		set_x(x);
		set_y(y);
	}
	void set_x(int x)
	{
		x_pixels = x;
		x_char_big = x_char_small = 0;
	}
	void add_x(int dx)
	{
		x_pixels += dx;
	}
	void add_char_counts(int nr_big, int nr_small)
	{
		x_char_big += nr_big;
		x_char_small += nr_small;
	}
	void sub_char_counts(int nr_big, int nr_small)
	{
		x_char_big -= nr_big;
		x_char_small -= nr_small;
	}
	void set_y(int y)
	{
		y_pixels = y;
	}
	void add_y(int dy)
	{
		y_pixels += dy;
	}
};

struct EncyclopediaPageElementLink
{
	ustring text;
	std::string target;
	bool have_x, have_y;
	int x, y;

	EncyclopediaPageElementLink(const xmlNode *node);
};

class EncyclopediaPageElement
{
public:
	EncyclopediaPageElement(EncyclopediaPageElementType type): _type(type) {}
	EncyclopediaPageElement(const EncyclopediaPageElementColor& color):
		_type(EncyclopediaPageElementType::Color)
	{
		new(&_color) EncyclopediaPageElementColor(color);
	}
	EncyclopediaPageElement(const EncyclopediaPageElementImage& image):
		_type(EncyclopediaPageElementType::Image)
	{
		new(&_image) EncyclopediaPageElementImage(image);
	}
    EncyclopediaPageElement(EncyclopediaPageElementLink&& link):
        _type(EncyclopediaPageElementType::Link)
	{
		new(&_link) EncyclopediaPageElementLink(std::move(link));
	}
	EncyclopediaPageElement(const EncyclopediaPageElementPosition& position):
		_type(EncyclopediaPageElementType::Size)
	{
		new(&_position) EncyclopediaPageElementPosition(position);
	}
    EncyclopediaPageElement(EncyclopediaPageElementSize size):
        _type(EncyclopediaPageElementType::Size)
	{
		new (&_size) EncyclopediaPageElementSize(size);
	}
    EncyclopediaPageElement(EncyclopediaPageElementText&& text):
        _type(EncyclopediaPageElementType::Text)
	{
		new(&_text) EncyclopediaPageElementText(std::move(text));
	}
	EncyclopediaPageElement(EncyclopediaPageElement&& elem);
	~EncyclopediaPageElement();

	EncyclopediaPageElementType type() const { return _type; }
	const EncyclopediaPageElementColor& color() const
	{
		if (_type != EncyclopediaPageElementType::Color)
			EXTENDED_EXCEPTION(ExtendedException::ec_invalid_parameter, "Element is not a color directive");
		return _color;
	}
	const EncyclopediaPageElementImage& image() const
	{
		if (_type != EncyclopediaPageElementType::Image)
			EXTENDED_EXCEPTION(ExtendedException::ec_invalid_parameter, "Element is not an image directive");
		return _image;
	}
	const EncyclopediaPageElementLink& link() const
	{
		if (_type != EncyclopediaPageElementType::Link)
			EXTENDED_EXCEPTION(ExtendedException::ec_invalid_parameter, "Element is not an link directive");
		return _link;
	}
	const EncyclopediaPageElementPosition& position() const
	{
		if (_type != EncyclopediaPageElementType::Position)
			EXTENDED_EXCEPTION(ExtendedException::ec_invalid_parameter, "Element is not a position directive");
		return _position;
	}
	EncyclopediaPageElementSize size() const
	{
		if (_type != EncyclopediaPageElementType::Size)
			EXTENDED_EXCEPTION(ExtendedException::ec_invalid_parameter, "Element is not a size directive");
		return _size;
	}
	const EncyclopediaPageElementText& text() const
	{
		if (_type != EncyclopediaPageElementType::Text)
			EXTENDED_EXCEPTION(ExtendedException::ec_invalid_parameter, "Element is not a text directive");
		return _text;
	}

private:
	EncyclopediaPageElementType _type;
	union
    {
		EncyclopediaPageElementColor _color;
		EncyclopediaPageElementImage _image;
		EncyclopediaPageElementLink _link;
		EncyclopediaPageElementPosition _position;
		EncyclopediaPageElementSize _size;
		EncyclopediaPageElementText _text;
	};
};

class EncyclopediaFormattedElement
{
public:
	EncyclopediaFormattedElement(int x, int y, int width, int height):
		_x(x), _y(y), _width(width), _height(height) {}

	int x() const { return _x; }
	int y() const { return _y; }
	int width() const { return _width; }
	int height() const { return _height; }

	virtual void display(const window_info *win, int y_min) const  = 0;

private:
	int _x, _y;
	int _width, _height;
};

class EncyclopediaFormattedImage: public EncyclopediaFormattedElement
{
public:
	EncyclopediaFormattedImage(int x, int y, int width, int height,
		std::uint32_t texture_id, float u_start, float v_start, float u_end, float v_end):
		EncyclopediaFormattedElement(x, y, width, height),
		_texture_id(texture_id), _u_start(u_start), _v_start(v_start), _u_end(u_end), _v_end(v_end) {}

	void display(const window_info *win, int y_min) const {}

private:
	std::uint32_t _texture_id;
	float _u_start, _v_start, _u_end, _v_end;
};

class EncyclopediaFormattedText: public EncyclopediaFormattedElement
{
public:
	EncyclopediaFormattedText(int x, int y, int width, int height, const ustring& text,
		const EncyclopediaPageElementColor& color, float scale, bool is_link):
		EncyclopediaFormattedElement(x, y, width, height), _text(text), _color(color),_scale(scale),
		_is_link(is_link) {}

	void display(const window_info *win, int y_min) const;

private:
	ustring _text;
	EncyclopediaPageElementColor _color;
	float _scale;
	bool _is_link;
};

class EncyclopediaFormattedLink
{
public:
	EncyclopediaFormattedLink(const std::string& target, int x, int y, int width, int height):
		_target(target), _x(x), _y(y), _width(width), _height(height) {}

	const std::string& target() const { return _target; }
	int contains(int x, int y) const
	{
		return x >= _x && x <= _x + _width && y >= _y && y <= _y + _height;
	}

private:
	std::string _target;
	int _x, _y, _width, _height;
};

class EncyclopediaPage
{
public:
	EncyclopediaPage(const std::string& name): _name(name), _elements(), _laid_out(false),
		_formatted(), _links(), _height(0) {}

	//! Return the name of this page.
	const std::string& name() const { return _name; }
	//! Return the height of the contents on this page
	int height() const { return _height; }
	const std::string& link_clicked(int x, int y) const;

	void read_xml(const xmlNode *node);

	void display(const window_info *win, int y_min);

private:
	std::string _name;
	std::vector<EncyclopediaPageElement> _elements;
	bool _laid_out;
	std::vector<std::unique_ptr<EncyclopediaFormattedElement>> _formatted;
	std::vector<EncyclopediaFormattedLink> _links;
	int _height;

	void layout(const window_info *win);
};

class EncyclopediaCategory
{
public:
	typedef std::vector<EncyclopediaPage>::iterator iterator;
	typedef std::vector<EncyclopediaPage>::const_iterator const_iterator;

	EncyclopediaCategory(const std::string& name): _name(name), _pages() {}

	const std::string& name() const { return _name; }
	bool empty() const { return _pages.empty(); }
	iterator begin() { return _pages.begin(); }
	iterator end() { return _pages.end(); }
	const_iterator begin() const { return _pages.begin(); }
	const_iterator end() const { return _pages.end(); }

	void read_xml(const xmlNode *node);

private:
	std::string _name;
	std::vector<EncyclopediaPage> _pages;
};

class Encyclopedia
{
public:
	static Encyclopedia& get_instance()
	{
		static Encyclopedia encyclopedia;
		return encyclopedia;
	}

	EncyclopediaPage* first_page()
	{
		if (_categories.empty() || _categories[0].empty())
			return nullptr;
		else
			return &*_categories[0].begin();
	}
	EncyclopediaPage* find_page(const std::string& name)
	{
		auto it = _pages.find(name);
		if (it == _pages.end())
			return nullptr;
		else
			return it->second;
	}

private:
	//! The list of categories in this encyclopedia.
	std::vector<EncyclopediaCategory> _categories;
	//! Map from page name to pointer to the page
	std::unordered_map<std::string, EncyclopediaPage*> _pages;

	//! Read a new encyclopedia from file \a file_name.
	Encyclopedia();
	//! Prevent the encyclopedia from being copied
	Encyclopedia(const Encyclopedia&) = delete;
	//! Prevent the encyclopedia from being copied
	Encyclopedia& operator=(const Encyclopedia&) = delete;

	void read_xml(const xmlNode* node);
};

class EncyclopediaWindow
{
public:
	static EncyclopediaWindow& get_instance()
	{
		static EncyclopediaWindow window;
		return window;
	}

	void initialize(int window_id);
	void set_minimum_size();

private:
	int _window_id;
	int _scroll_id;
	EncyclopediaPage* _current_page;

	EncyclopediaWindow(): _window_id(-1), _scroll_id(-1), _current_page(nullptr) {}
	//! Prevent the encyclopedia window from being copied
	EncyclopediaWindow(const EncyclopediaWindow&) = delete;
	//! Prevent the encyclopedia from being copied
	EncyclopediaWindow& operator=(const EncyclopediaWindow&) = delete;

	//! Handler for displaying the window
	int display_handler(window_info *win);
	//! Static handler for displaying the window, calls display_handler()
	static int static_display_handler(window_info *win);
	int click_handler(window_info *win, int mx, int my, std::uint32_t flags);
	static int static_click_handler(window_info *win, int mx, int my, std::uint32_t flags);
};

} //namespace eternal_lands

#endif // __cplusplus

#ifdef __cplusplus
extern "C"
{
#endif

void init_new_encyclopedia();
void fill_new_encyclopedia_win(int window_id);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // NEW_ENCYCLOPEDIA_H
