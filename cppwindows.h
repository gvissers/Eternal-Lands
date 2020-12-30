#ifndef CPPWINDOWS_H
#define CPPWINDOWS_H

/*!
 * \file
 * \ingroup elwindows
 * \brief C++ abstraction for client windows
 */

#include "elwindows.h"
#include "exceptions/extendedexception.hpp"
#include "font.h"

//! Concatenate tokens \a a and \a b
#define CONCAT_IMPL(a, b) a ## b
//! Concatenate tokens \a a and \a b
#define CONCAT(a, b) CONCAT_IMPL(a, b)
//! Return the function name for a handler for \a action events
#define HANDLER_NAME(action) CONCAT(action, _handler)
//! Return the function name for a static handler for \a action events
#define STATIC_HANDLER_NAME(action) CONCAT(static_, HANDLER_NAME(action))
//! Return the name of a function that sets the callback handler for events of type \a action
#define SET_HANDLER_NAME(action) CONCAT(set_, HANDLER_NAME(action))
//! Macro for the declaration of a static handler for \a action events
#define DECLARE_STATIC_HANDLER(action, ...) \
	static int STATIC_HANDLER_NAME(action)(window_info *win, ##__VA_ARGS__)
//! Extrcat the member function handler for events of type \a action from a \window_info structure
#define MEMBER_HANDLER(action) get_cpp_window(win).HANDLER_NAME(action)
//! Define a static callback handler for \a action events that takes no arguments
#define DEFINE_STATIC_HANDLER0(action) \
	DECLARE_STATIC_HANDLER(action) { return MEMBER_HANDLER(action)(); }
//! Define a static callback handler for \a action events that takes a single argument of type T0
#define DEFINE_STATIC_HANDLER1(action, T0) \
	DECLARE_STATIC_HANDLER(action, T0 a0) { return MEMBER_HANDLER(action)(a0); }
//! Define a static callback handler for \a action events that takes two arguments of type T0 and T1
#define DEFINE_STATIC_HANDLER2(action, T0, T1) \
	DECLARE_STATIC_HANDLER(action, T0 a0, T1 a1) { return MEMBER_HANDLER(action)(a0, a1); }
//! Define a static callback handler for \a action events that takes three argument of type T0, T1, and T2
#define DEFINE_STATIC_HANDLER3(action, T0, T1, T2) \
	DECLARE_STATIC_HANDLER(action, T0 a0, T1 a1, T2 a2) { return MEMBER_HANDLER(action)(a0, a1, a2); }
//! Check if class \c Derived ahs a member function named <em>action<tt>_handler</tt>,
#define HAS_HANDLER(action) \
	std::is_member_function_pointer<decltype(&Derived::HANDLER_NAME(action))>::value
/*!
 * \brief Macro to declare a handler setter
 *
 * Macro DECLARE_SET_HANDLER declares functions to set the callback function for an event of type
 * \a action (identified by \a constant) in a Window instance. The Window<Derived> class uses SFINAE
 * to determine which which function is used: if an <em>action</em><tt>_handler</tt> member function
 * is defined in \c Derived, the function that sets the handler is used, otherwise the variant
 * that does nothing is used.
 * \param action The event for which a callback setter is defined
 * \param constant The associated constant that defines the type of event.
 */
#define DECLARE_SET_HANDLER(action, constant) \
	template <typename Derived> \
	typename std::enable_if<HAS_HANDLER(action), void>::type \
	SET_HANDLER_NAME(action)(Window<Derived> *window) \
	{ \
		set_window_handler(window->id(), constant, \
			(int(*)())&Window<Derived>::STATIC_HANDLER_NAME(action)); \
	} \
	void SET_HANDLER_NAME(action)(...) {}

namespace eternal_lands
{

/*!
 * \brief Base class for client windows
 *
 * Class Window is a template base class that represents a client window. The template argument
 * \a Derived is the type name of the class deriving from Window. It provides
 * access to the underlying data through getter and setter functions, and will automatically
 * register event handlers if they are implemented in \a Derived.
 */
template <typename Derived>
class Window
{
public:
	/*!
	 * \brief Constructor
	 *
	 * Create a new Window using the ID of a window previously created using create_window.
	 * \param id The unique identifier of the existing window
	 */
	Window(int id): _id(id)
	{
		if (_id < 0 || _id >= windows_list.num_windows)
			EXTENDED_EXCEPTION(ExtendedException::ec_invalid_parameter, "Invalid window id " << _id);
		window().data = this;

		set_display_handler(this);
		set_mouseover_handler(this);
		set_click_handler(this);
		set_resize_handler(this);
		set_ui_scale_handler(this);
		set_font_change_handler(this);
	}

	//! Return the identifier for this window
	int id() const { return _id; }
	//! Return the width in pixels of this window
	int width() const { return window().len_x; }
	//! Return the height in pixels of this window
	int height() const { return window().len_y; }
	//! Return the current UI scale factor for this window
	float current_scale() const { return window().current_scale; }
	//! Return the size (in pixels) of a close box in this window
	int box_size() const { return window().box_size; }
	//! Return the default text font category for this window
	FontManager::Category font_category() const { return window().font_category; }
	//! Return the maximum width of a character in the default sized font for this window
	int default_font_max_char_width() const { return window().default_font_max_len_x; }
	//! Return the height of a text line in the default sized font for this window
	int default_font_height() const { return window().default_font_len_y; }
	//! Return the maximum width of a character in the small sized font for this window
	int small_font_max_char_width() const { return window().small_font_max_len_x; }
	//! Return the height of a text line in the small sized font for this window
	int small_font_height() const { return window().small_font_len_y; }

	//! Return a constant reference to the underlying window_info structure
	const window_info& window() const { return windows_list.window[_id]; }
	//! Return a variable reference to the underlying window_info structure
	window_info& window() { return windows_list.window[_id]; }
	//! Return a pointer to the underlying window_info structure
	window_info* window_ptr() { return &window(); }

	/*!
	 * \brief Static display handler
	 *
	 * Static display handler that calls \c Derived::display_handler(). This function is installed
	 * as the display handler in the underlying window_info structure if a
	 * \c Derived::display_handler() exists.
	 */
	DEFINE_STATIC_HANDLER0(display)
	/*!
	 * \brief Static click handler
	 *
	 * Static mouse click handler that calls \c Derived::click_handler(). This function is installed
	 * as the click handler in the underlying window_info structure if a
	 * \c Derived::click_handler() exists.
	 */
	DEFINE_STATIC_HANDLER3(click, int, int, std::uint32_t);
	/*!
	 * \brief Static mouseover handler
	 *
	 * Static mouseover handler that calls \c Derived::mouseover_handler(). This function is
	 * installed as the mouseover handler in the underlying window_info structure if a
	 * \c Derived::mouseover_handler() exists.
	 */
	DEFINE_STATIC_HANDLER2(mouseover, int, int)
	/*!
	 * \brief Static resize handler
	 *
	 * Static resize handler that calls \c Derived::resize_handler(). This function is installed
	 * as the resize handler in the underlying window_info structure if a
	 * \c Derived::resize_handler() exists.
	 */
	DEFINE_STATIC_HANDLER2(resize, int, int)
	/*!
	 * \brief Static UI scale handler
	 *
	 * Static user interface scale handler that calls \c Derived::ui_scale_handler(). This function
	 * is installed as the UI scale handler in the underlying window_info structure if a
	 * \c Derived::ui_scale_handler() exists.
	 */
	DEFINE_STATIC_HANDLER0(ui_scale)
	/*!
	 * \brief Static font change handler
	 *
	 * Static font change handler that calls \c Derived::font_change_handler(). This function is
	 * installed as the font change handler in the underlying window_info structure if a
	 * \c Derived::font_change_handler() exists.
	 */
	DEFINE_STATIC_HANDLER1(font_change, FontManager::Category)

protected:
	/*!
	 * \brief Extract a Window from the underlying window_info structure
	 *
	 * Return the pointer to itself that a Window stores in the \c data member of its underlying
	 * window_info structure.
	 * \param win Pointer to a window_info structure represented by a Window.
	 * \return Pointer to the child class deriving from Window that was stored in \a win.
	 */
	static Derived& get_cpp_window(window_info *win)
	{
		return *reinterpret_cast<Derived*>(win->data);
	}

private:
	//! The unique identifier for this window
	int _id;
};


DECLARE_SET_HANDLER(display, ELW_HANDLER_DISPLAY)
DECLARE_SET_HANDLER(click, ELW_HANDLER_CLICK)
DECLARE_SET_HANDLER(mouseover, ELW_HANDLER_MOUSEOVER)
DECLARE_SET_HANDLER(resize, ELW_HANDLER_RESIZE)
DECLARE_SET_HANDLER(ui_scale, ELW_HANDLER_UI_SCALE)
DECLARE_SET_HANDLER(font_change, ELW_HANDLER_FONT_CHANGE)

} // namespace eternal_lands

#endif // CPPWINDOWS_H
