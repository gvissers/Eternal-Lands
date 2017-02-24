#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <vector>

#include <glob.h>
#include <libxml/parser.h>

#include "../rules.h"

#ifdef EL_BIG_ENDIAN
#include <byteswap.h>
inline uint32_t write_le(std::ostream& os, uint32_t x)
{
	uint32_t y = bswap_32(x);
    os.write(reinterpret_cast<const char*>(&y), 4);
    return 4;
}
#else
template<typename T>
inline uint32_t write_le(std::ostream& os, T x)
{
    os.write(reinterpret_cast<const char*>(&x), sizeof(T));
    return sizeof(T);
}
#endif

namespace
{

iconv_t conv_desc;

int glob_err(const char* epath, int eerrno)
{
    std::cerr << "Failed to glob " << epath << ": " << std::strerror(eerrno) << '\n';
    return 0;
}

std::vector<std::string> find_rules_files(const std::string& dir_name)
{
    std::string pattern = dir_name + "/*/rules.xml";
    glob_t glob_res;
    std::vector<std::string> res;

    if (glob(pattern.c_str(), 0, glob_err, &glob_res))
    {
        std::cerr << "Failed to find rules files.\n";
        return res;
    }

    for (unsigned int i = 0; i < glob_res.gl_pathc; ++i)
        res.emplace_back(glob_res.gl_pathv[i]);

    globfree(&glob_res);

    return res;
}

void clean(std::string& str)
{
    // iconv chokes on this
    const std::string search = "…", replace = "...";
    size_t idx = str.find(search);
    while (idx != std::string::npos)
    {
        str.replace(idx, search.size(), replace);
        idx = str.find(search, idx);
    }
}

int add_rule(const char* short_desc, const char* long_desc, rule_type type,
             rules_struct& rules)
{
    rules_struct::rule_struct* rule = rules.rule + rules.no++;
    if (short_desc)
    {
        std::string sin = short_desc;
        clean(sin);
        size_t in_len = sin.size(), out_size = in_len + 1;
        rule->short_desc = new char[out_size];
        if (!rule->short_desc)
        {
            std::cerr << "Failed to allocate short description\n";
            return 1;
        }

        size_t out_len = in_len;
        char *in = const_cast<char*>(sin.c_str());
        char *out = rule->short_desc;
        size_t res = iconv(conv_desc, &in, &in_len, &out, &out_len);
        if (res == size_t(-1))
        {
            std::cerr << "Failed to convert short description\n";
            //return 1;
        }
        rule->short_len = out_size - 1 - out_len;
        rule->short_desc[rule->short_len] = '\0';
    }
    if (long_desc)
    {
        std::string sin = long_desc;
        clean(sin);
        size_t in_len = sin.size(), out_size = in_len + 1;
        rule->long_desc = new char[out_size];
        if (!rule->long_desc)
        {
            std::cerr << "Failed to allocate long description\n";
            return 1;
        }

        size_t out_len = in_len;
        char *in = const_cast<char*>(sin.c_str());
        char *out = rule->long_desc;
        size_t res = iconv(conv_desc, &in, &in_len, &out, &out_len);
        if (res == size_t(-1))
        {
            std::cerr << "Failed to convert long description\n";
            //return 1;
        }
        rule->long_len = out_size - 1 - out_len;
        rule->long_desc[rule->long_len] = '\0';
    }
    rule->type = type;

    return 0;
}

const char *get_id_str(const xmlNode* node, const char *id)
{
    for (const xmlNode *cur = node; cur; cur = cur->next)
    {
        if (cur->type == XML_ELEMENT_NODE && cur->children
            && !xmlStrcasecmp(cur->name, reinterpret_cast<const xmlChar*>(id)))
        {
            return reinterpret_cast<const char*>(cur->children->content);
        }
    }
    return NULL;
}

int parse_rules(const xmlNode* root, rules_struct &rules)
{
    if (!root)
        return 1;

    for (const xmlNode *cur = root; cur; cur = cur->next)
    {
        if (cur->type != XML_ELEMENT_NODE)
            continue;

        if (cur->children)
        {
            if (!xmlStrcasecmp(cur->name, (xmlChar*)"title"))
            {
                if (add_rule((const char*)cur->children->content, NULL,
                             RULE_TITLE, rules))
                    return 1;
            }
            else if (!xmlStrcasecmp(cur->name, (xmlChar*)"rule"))
            {
                add_rule(get_id_str(cur->children, "short"),
                         get_id_str(cur->children, "long"), RULE_RULE, rules);
            }
            else if (!xmlStrcasecmp(cur->name, (xmlChar*)"info"))
            {
                add_rule(get_id_str(cur->children, "short"),
                         get_id_str(cur->children, "long"), RULE_INFO, rules);
            }
        }
    }

    return 0;
}


int parse_rules_file(const std::string& fname,
                     std::map<std::string, rules_struct>& all_rules)
{
    size_t eidx = fname.rfind('/');
    size_t bidx = fname.rfind('/', eidx-1);
    if (eidx == std::string::npos || bidx == std::string::npos)
    {
        std::cerr << "Failed to parse language from file name \"" << fname << "\"\n";
        return 1;
    }

    std::string lang = fname.substr(bidx+1, eidx-bidx-1);

    xmlDoc *doc = xmlReadFile(fname.c_str(), NULL, 0);
    if (!doc)
    {
        std::string en_fname = fname.substr(0, bidx+1) + "en" + fname.substr(eidx);
        doc = xmlReadFile(en_fname.c_str(), NULL, 0);
        if (!doc)
        {
            std::cerr << "Failed to read rules file \"" << fname << "\"\n";
            return 1;
        }
    }

    xmlNode *root = xmlDocGetRootElement(doc);
    int err = 0;
    if (!root)
    {
        std::cerr << "Failed to find root element in rules file \"" << fname << "\"\n";
        err = 1;
    }
    else
    {
        err = parse_rules(root->children, all_rules[lang]);
    }

    xmlFreeDoc(doc);

    return err;
}

void free_rules(std::map<std::string, rules_struct>& all_rules)
{
    for (auto& nv: all_rules)
    {
        for (int i = 0; i < nv.second.no; ++i)
        {
            delete[] nv.second.rule[i].short_desc;
            delete[] nv.second.rule[i].long_desc;
        }
    }
    all_rules.clear();
}

int write_rules(std::ostream& os, const std::map<std::string, rules_struct>& all_rules)
{
    uint32_t pos = 0;
    uint32_t nr_lang = all_rules.size();

    pos += write_le(os, nr_lang);
    uint32_t hdr_off = pos;
    // Reserve space for offset
    for (unsigned int i = 0; i < nr_lang * 12; ++i, ++pos)
        os.put('\0');

    // Now start writing the rules
    for (const auto& nv: all_rules)
    {
        const std::string& lang = nv.first;

        os.seekp(hdr_off);
        os.write(lang.data(), std::min(lang.size(), 7ul));
        os.seekp(hdr_off + 8);
        write_le(os, pos);
        hdr_off += 12;
        os.seekp(pos);

        const rules_struct& rules = nv.second;
        pos += write_le(os, uint32_t(rules.no));
        for (int i = 0; i < rules.no; ++i)
        {
            const rules_struct::rule_struct& rule = rules.rule[i];
            pos += write_le(os, uint32_t(rule.type));
            pos += write_le(os, uint32_t(rule.short_len));
            os.write(rule.short_desc, rule.short_len);
            pos += rule.short_len;
            for (; pos%4; ++pos)
                os.put('\0');
            pos += write_le(os, uint32_t(rule.long_len));
            os.write(rule.long_desc, rule.long_len);
            pos += rule.long_len;
            for (; pos%4; ++pos)
                os.put('\0');
        }
    }

    return 0;
}

} // namespace

int main(int argc, const char* argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <directory> <out_file>\n";
        return 1;
    }

    std::string dir_name = std::string(argv[1]) + "/languages";
    std::vector<std::string> fnames = find_rules_files(dir_name);
    if (fnames.empty())
        return 1;

    int err = 0;
    std::map<std::string, rules_struct> all_rules;

    conv_desc = iconv_open("ISO_8859-1", "UTF-8");
    for (const auto& fname: fnames)
    {
        if (parse_rules_file(fname, all_rules))
        {
            err = 1;
            break;
        }
    }
    iconv_close(conv_desc);

    if (!err)
    {
        std::ofstream os(argv[2]);
        if (!os)
        {
            std::cerr << "Failed to open output file \"" << argv[2] << "\"\n";
            err = 1;
        }
        else
        {
            err = write_rules(os, all_rules);
        }
        os.close();
    }

    free_rules(all_rules);

    return err;
}
