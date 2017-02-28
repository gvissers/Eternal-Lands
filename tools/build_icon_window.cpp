#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include "xml.h"

namespace
{

int display_icon_size;
std::vector<std::vector<std::string>> all_lines;

class Virtual_Icon
{
public:
    virtual std::ostream& print(std::ostream& os) const = 0;
};
std::ostream& operator<<(std::ostream& os, const Virtual_Icon& icon)
{
    return icon.print(os);
}

std::vector<Virtual_Icon*> main_icon_list, new_char_icon_list;

class Basic_Icon: public Virtual_Icon
{
    public:
        Basic_Icon(int icon_id, int coloured_icon_id, const std::string& help_text,
                   const std::string& help_name, int lines_idx):
            _icon_id(icon_id), _coloured_icon_id(coloured_icon_id),
            _help_text(help_text), _help_name(help_name), _lines_idx(lines_idx) {}

        int icon_id() const { return _icon_id; }
        int coloured_icon_id() const { return _coloured_icon_id; }
        int lines_index() const { return _lines_idx; }

        std::string help_message() const
        {
            if (!_help_text.empty())
                return '"' + _help_text + '"';
            else
                return "get_named_string(\"tooltips\", \"" + _help_name + "\")";
        }

    private:
        int _icon_id;
        int _coloured_icon_id;
        std::string _help_text;
        std::string _help_name;
        int _lines_idx;
};

class Keypress_Icon: public Basic_Icon
{
    public:
        Keypress_Icon(int icon_id, int coloured_icon_id,
                      const std::string& help_text, const std::string& help_name,
                      const std::string& key_name, int lines_idx):
            Basic_Icon(icon_id, coloured_icon_id, help_text, help_name, lines_idx),
            _key_name(key_name) {}

        std::ostream& print(std::ostream& os) const
        {
            os << "new Keypress_Icon(" << icon_id()
                << ", " << coloured_icon_id()
                << ", " << help_message()
                << ", \"" << _key_name << "\", ";
            if (lines_index() < 0)
                os << "nullptr";
            else
                os << "&all_lines[" << lines_index() << ']';
            return os << ')';
        }

    private:
        std::string _key_name;
};

class Window_Icon: public Basic_Icon
{
    public:
        Window_Icon(int icon_id, int coloured_icon_id, const std::string& help_text,
                    const std::string& help_name, const std::string& window_name,
                    int lines_idx):
            Basic_Icon(icon_id, coloured_icon_id, help_text, help_name, lines_idx),
            _window_name(window_name) {}

        std::ostream& print(std::ostream& os) const
        {
            os << "new Window_Icon(" << icon_id()
                << ", " << coloured_icon_id()
                << ", " << help_message()
                << ", \"" << _window_name << "\", ";
            if (lines_index() < 0)
                os << "nullptr";
            else
                os << "&all_lines[" << lines_index() << ']';
            return os << ')';
        }

    private:
        std::string _window_name;
};

class Actionmode_Icon: public Basic_Icon
{
    public:
        Actionmode_Icon(int icon_id, int coloured_icon_id, const std::string& help_text,
                        const std::string& help_name, const std::string& action_name,
                        int lines_idx):
            Basic_Icon(icon_id, coloured_icon_id, help_text, help_name, lines_idx),
            _action_name(action_name) {}

        std::ostream& print(std::ostream& os) const
        {
            os << "new Actionmode_Icon(" << icon_id()
                << ", " << coloured_icon_id()
                << ", " << help_message()
                << ", \"" << _action_name << "\", ";
            if (lines_index() < 0)
                os << "nullptr";
            else
                os << "&all_lines[" << lines_index() << ']';
            return os << ')';
        }

    private:
        std::string _action_name;
};

class Command_Icon: public Basic_Icon
{
    public:
        Command_Icon(int icon_id, int coloured_icon_id, const std::string& help_text,
                     const std::string& help_name, const std::string& command,
                     int lines_idx):
            Basic_Icon(icon_id, coloured_icon_id, help_text, help_name, lines_idx),
            _command_text(command) {}

        std::ostream& print(std::ostream& os) const
        {
            os << "new Command_Icon(" << icon_id()
                << ", " << coloured_icon_id()
                << ", " << help_message()
                << ", \"" << _command_text << "\", ";
            if (lines_index() < 0)
                os << "nullptr";
            else
                os << "&all_lines[" << lines_index() << ']';
            return os << ')';
        }

private:
        std::string _command_text;
};

class Multi_Icon: public Virtual_Icon
{
    public:
        Multi_Icon(const std::string& control_name,
                   std::vector<Virtual_Icon*> &the_icons):
            _name(control_name), _icons(the_icons) {}

        std::ostream& print(std::ostream& os) const
        {
            os << "new Multi_Icon(\"" << _name << "\", ";
            if (_icons.empty())
            {
                os << "{}";
            }
            else
            {
                os << "{ " << *_icons[0];
                for (unsigned int i = 1; i < _icons.size(); ++i)
                    os << ", " << *_icons[i];
                os << " }";
            }
            os << ')';
            return os;
        }

    private:
        std::string _name;
        std::vector<Virtual_Icon *> _icons;
};

Virtual_Icon *icon_xml_factory(const xmlNode *cur)
{
    std::string the_type = property(cur, "type");
    int image_id = int_property(cur, "image_id");
    int alt_image_id = int_property(cur, "alt_image_id");
    std::string help_name = property(cur, "help_name");
    std::string help_text = property(cur, "help_text");
    std::string param_name = property(cur, "param_name");

    std::vector<std::string> menu_lines;
    int lines_idx = -1;
    // XXX here too, no UTF-8 -> Latin1
    std::string parsed = value(cur);
    if (!parsed.empty())
    {
        std::istringstream lines(parsed);
        std::string line;
        while (std::getline(lines, line))
        {
            if (!line.empty())
                menu_lines.push_back(line);
        }
        if (!menu_lines.empty())
        {
            all_lines.push_back(menu_lines);
            lines_idx = all_lines.size() - 1;
        }
    }

    if (the_type.empty() || (image_id < 0) || (alt_image_id < 0)
        || (help_name.empty() && help_text.empty()) || param_name.empty())
    {
        std::cerr << "icon window factory: xml field error type=\"" << the_type
            << "\" image_id=" << image_id << " alt_image_id=" << alt_image_id
            << "help_name=\"" << help_name << "\" help_text=\"" << help_text
            << "\" param_name=\"" << param_name << "\"\n";
        return nullptr;
    }

    std::string help_str;
    if (!help_text.empty())
        help_str = help_text;
    else
        help_str = "meep";//get_named_string("tooltips", help_name);

    if (the_type == "keypress")
        return new Keypress_Icon(image_id, alt_image_id, help_text, help_name,
                                 param_name, lines_idx);
    else if (the_type == "window")
        return new Window_Icon(image_id, alt_image_id, help_text, help_name,
                               param_name, lines_idx);
    else if (the_type == "action_mode")
        return new Actionmode_Icon(image_id, alt_image_id, help_text, help_name,
                                   param_name, lines_idx);
    else if (the_type == "#command")
        return new Command_Icon(image_id, alt_image_id, help_text, help_name,
                                param_name, lines_idx);

    return nullptr;
}

bool read_xml(const std::string& file_name, std::vector<Virtual_Icon*>& icons)
{
    xmlDoc *doc = xmlReadFile(file_name.c_str(), nullptr, 0);
    if (!doc)
    {
        std::cerr << "Can't open file " << file_name << '\n';
        return false;
    }

    const xmlNode *cur = xmlDocGetRootElement(doc);
    if (!cur)
    {
        std::cerr << "Empty xml document\n";
        xmlFreeDoc(doc);
        return false;
    }
    if (xmlStrcasecmp(cur->name, (const xmlChar *) "icon_window"))
    {
        std::cerr << "Not icon_window file\n";
        xmlFreeDoc(doc);
        return false;
    }

    for (cur = cur->children; cur; cur = cur->next)
    {
        if (!xmlStrcasecmp(cur->name, (const xmlChar *)"icon"))
        {
            Virtual_Icon *new_icon = icon_xml_factory(cur);
            if (new_icon)
                icons.push_back(new_icon);
        }
        else if (!xmlStrcasecmp(cur->name, (const xmlChar *)"multi_icon"))
        {
            // XXX: original code converts from UTF-8 to Latin1. Forget it.
            std::string control_name = property(cur, "control_name");
            if (control_name.empty())
            {
                std::cerr << "invalid multi icon control_name\n";
            }
            else
            {
                std::vector<Virtual_Icon*> multi_icons;
                for (const xmlNode *multi_cur = cur->children; multi_cur;
                     multi_cur = multi_cur->next)
                {
                    if (!xmlStrcasecmp(multi_cur->name, (const xmlChar *)"icon"))
                    {
                        Virtual_Icon *new_icon = icon_xml_factory(multi_cur);
                        if (new_icon)
                            multi_icons.push_back(new_icon);
                    }
                }
                icons.push_back(new Multi_Icon(control_name.c_str(), multi_icons));
            }
        }
        else if (!xmlStrcasecmp(cur->name, (const xmlChar *)"image_settings"))
        {
            display_icon_size = int_property(cur, "display_size");
        }
    }

    xmlFreeDoc(doc);
    return true;
}

std::ostream& write_icons(std::ostream& os)
{
    os << "static const std::vector<CommandQueue::Line> all_lines["
        << all_lines.size() << "] = {\n";
    for (const auto& lines: all_lines)
    {
        if (lines.empty())
        {
            os << "\t{},\n";
        }
        else
        {
            os << "\t{\n";
            for (const std::string& line: lines)
                os << "\t\tCommandQueue::Line(\"" << line << "\"),\n";
            os << "\t},\n";
        }
    }
    os << "};\n\n";

    os << "if (icon_mode == MAIN_WINDOW_ICONS)\n{\n"
        << "\ticon_list = {\n";
    for (const auto icon_ptr: main_icon_list)
        os << "\t\t" << *icon_ptr << ",\n";
    os << "\t};\n}\n"
        << "else if (icon_mode == NEW_CHARACTER_ICONS)\n{\n"
        << "\ticon_list = {\n";
    for (const auto icon_ptr: new_char_icon_list)
        os << "\t\t" << *icon_ptr << ",\n";
    os << "\t};\n}\n"
        << "else\n{\n"
        << "\tLOG_ERROR(\"%s : invalid icon mode\\n\", error_prefix);\n"
        << "\treturn false;\n"
        << "}\n";

    return os;
}

} // namespace

int main(int argc, const char* argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <directory> <out_file>\n";
        return 1;
    }

    std::string fname = std::string(argv[1]) + "/main_icon_window.xml";
    if (!read_xml(fname, main_icon_list))
        return 1;
    fname = std::string(argv[1]) + "/new_character_icon_window.xml";
    if (!read_xml(fname, new_char_icon_list))
        return 1;

    std::ofstream os(argv[2]);
    if (!os)
    {
        std::cerr << "Unable to open output file " << argv[2] << '\n';
        return 1;
    }
    write_icons(os);
    os.close();

    return 0;
}
