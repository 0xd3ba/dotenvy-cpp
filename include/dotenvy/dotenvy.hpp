#pragma once

#include <cctype>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <istream>
#include <optional>
#include <regex>
#include <system_error>
#include <string>
#include <string_view>
#include <unordered_map>

namespace dotenvy {

namespace {

/* A trick to defer a static_assert until template instantiation, is to use a variable template */
template <typename>
inline constexpr bool always_false_v = false;

} // namespace

class DotEnvError: public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class DotEnvy {
public:
    DotEnvy() = default;
    enum class OnError { Throw, Skip };  // Throw an error or skip the error ?

    /* Load the environment variables from a given file */
    void load(const std::filesystem::path &path, OnError on_error = OnError::Throw) {
        std::ifstream file{path};
        if(!file)
            throw DotEnvError{"unable to open the file: " + path.string()};

        load(file, on_error);
    }

    /* Load the environment variables from a given input stream */
    void load(std::istream &istream, OnError on_error = OnError::Throw) {
        std::string line;
        size_t line_num = 0;

        while(std::getline(istream, line)) {
            line_num++;
            if(line_num == 1) strip_byte_order_marks(line);
            strip_trailing_carriage_return(line);

            try {
                if(auto parsed = parse_line(line, line_num)) {
                    values_[parsed->key] = expand_variables(parsed->value);
                }
            } catch(const DotEnvError&) {
                if(on_error == OnError::Throw) throw;
                // OnError::Skip: silently move on to the next line
            }
        }
    }

    /* Loads and sets the environment variables from a given file
     * By default, overwrite = False -- it doesn't overwrite the already existing environment variables
     */
    void load_dotenv(const std::filesystem::path &path, bool overwrite = false, OnError on_error = OnError::Throw) {
        load(path, on_error);
        apply_to_environment(overwrite);
    }

    /* Loads and sets the environment variables from a given input stream
     * By default, overwrite = False -- it doesn't overwrite the already existing environment variables
     */
    void load_dotenv(std::istream &istream, bool overwrite = false, OnError on_error = OnError::Throw) {
        load(istream, on_error);
        apply_to_environment(overwrite);
    }

    /* Applies all the loaded environment variables */
    void apply_to_environment(bool overwrite = false) const {
        for (const auto& [key, value] : values_) {
            #if defined(_WIN32)
            if (!overwrite && std::getenv(key.c_str()) != nullptr) continue;
            _putenv_s(key.c_str(), value.c_str());
            #else
            setenv(key.c_str(), value.c_str(), overwrite? 1: 0);
            #endif
        }
    }

    bool contains(std::string_view key) const{
        return values_.find(std::string{key}) != values_.end();
    }

    template<typename T = std::string>
    std::optional<T> get(std::string_view key) const {
        auto it = values_.find(std::string{key});
        if(it == values_.end())
            return std::nullopt;
        return convert<T>(it->second);
    }

    template<typename T = std::string>
    T get(std::string_view key, T default_value) const {
        return get<T>(key).value_or(default_value);
    }

    /* Without this, get("KEY", "literal") would deduce T = const char*
     * and we'd get a RAW POINTER back instead of a std::string
     * This overload wins the tie against the template (an exact match beats the template)
     */
    std::string get(std::string_view key, const char* default_value) const {
        return get(key, std::string{default_value});
    }

    /* Returns the loaded environment variables as a hashmap/dictionary */
    const std::unordered_map<std::string, std::string>& dotenv_values() const {
        return values_;
    }

private:
    std::unordered_map<std::string, std::string> values_;

    struct ParsedLine {
        std::string key;
        std::string value;
    };

    std::optional<ParsedLine> parse_line(std::string line, size_t line_num) {
        std::string trimmed = trim(line);
        std::string line_str = std::to_string(line_num);

        if(trimmed.empty() || trimmed.front() == '#')
            return std::nullopt;

        // Case: "export KEY=value"
        if(trimmed.rfind("export ", 0) == 0)
            trimmed = trim(trimmed.substr(7));  // remove "export"

        auto eq = trimmed.find('=');
        if(eq == std::string::npos)
            throw DotEnvError{"in line#" + line_str + ", expected KEY=VALUE but got: " + trimmed};

        std::string key = trim(trimmed.substr(0, eq));
        std::string raw_val = trim(trimmed.substr(eq+1));

        // must begin with a letter or an underscore, and rest of the string must contain letters or numbers
        static const std::regex key_pattern(R"(^[A-Za-z_][A-Za-z0-9_]*$)");
        if(!std::regex_match(key, key_pattern))
            throw DotEnvError{"in line#" + line_str + ", invalid KEY: " + key};

        std::string val = unquote_or_strip_comment(raw_val, line_num);
        return ParsedLine{std::move(key), std::move(val)};
    }

    /* Converts a raw stored string into rudimentary type T */
    template<typename T>
    static T convert(const std::string &raw) {
        /* Case-1: string itself */
        if constexpr(std::is_same_v<T, std::string>)
        return raw;

        /* Case-2: boolean */
        else if constexpr(std::is_same_v<T, bool>) {
            std::string lower{};
            lower.reserve(raw.size());

            for(auto c: raw)
                lower.push_back(static_cast<char>(std::tolower(c)));

            if(lower == "true" || lower == "1") return true;
            else if(lower == "false" || lower == "0") return false;
            else throw DotEnvError{"unable to convert to boolean: " + raw};
        }

        /* Case-3: Is a number */
        else if constexpr (std::is_arithmetic_v<T>) {
            T value{};
            const auto *start = raw.data();
            const auto *end = start + raw.size();
            auto [ptr, ec] = std::from_chars(start, end, value);

            if(ec == std::errc::invalid_argument)
                throw DotEnvError{"unable to convert to number: " + raw};
            if(ec == std::errc::result_out_of_range)
                throw DotEnvError{"number out of range: " + raw};
            if(ptr != end)
                throw DotEnvError{"invalid number: " + raw};

            return value;
        }

        /* Case-4: Not a number, std::string or a boolean -- Invalid case */
        else
            static_assert(always_false_v<T>, "DotEnvy::get<T>() only supports std::string, bool, and arithmetic types");
    }

    void strip_byte_order_marks(std::string &line) {
        static constexpr unsigned char bom[] = {0xEF, 0xBB, 0xBF};
        if (line.size() >= 3 &&
            static_cast<unsigned char>(line[0]) == bom[0] &&
            static_cast<unsigned char>(line[1]) == bom[1] &&
            static_cast<unsigned char>(line[2]) == bom[2]) {
            line.erase(0, 3);
        }
    }

    static void strip_trailing_carriage_return(std::string& line) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
    }

    static std::string trim(std::string_view s) {
        size_t start = 0;
        size_t end = s.size();

        // Note: isspace considers all cases of ' ', '\n', '\r' etc. and is not just limited to spaces
        while(start < end && std::isspace(static_cast<unsigned char>(s[start]))) start++;
        while(end > start && std::isspace(static_cast<unsigned char>(s[end-1]))) end--;
        return std::string(s.substr(start, end-start));
    }

    /* Handles three cases for the right-hand side of KEY=value:
     * - 'literal' -> no escapes, no expansion, taken verbatim
     * - "text"    -> C-style escapes processed, expansion happens later
     * - bare text -> trimmed, with an unquoted '#' treated as a comment
     */
    static std::string unquote_or_strip_comment(const std::string &raw, size_t line_num) {
        if (raw.size() >= 2 && raw.front() == '\'' && raw.back() == '\'') {
            return raw.substr(1, raw.size() - 2);
        }

        if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') {
            return process_escapes(raw.substr(1, raw.size() - 2), line_num);
        }

        std::string result;
        result.reserve(raw.size());
        for (size_t i=0; i<raw.size(); i++) {
            if (raw[i] == '#') break;
            result.push_back(raw[i]);
        }
        return trim(result);
    }

    static std::string process_escapes(const std::string &s, size_t line_num) {
        std::string out;
        std::string line_s = std::to_string(line_num);
        out.reserve(s.size());

        for (size_t i=0; i<s.size(); i++) {
            if (s[i] != '\\') {
                out.push_back(s[i]);
                continue;
            }

            if (i+1 >= s.size())
                throw DotEnvError("trailing backslash in quoted value for line number " + line_s + ": " + s);

            switch (s[++i]) {
                case 'n':  out.push_back('\n'); break;
                case 't':  out.push_back('\t'); break;
                case 'r':  out.push_back('\r'); break;
                case '"':  out.push_back('"');  break;
                case '\\': out.push_back('\\'); break;
                case '$':  out.push_back('$');  break;
                default:
                    throw DotEnvError(std::string("unknown escape sequence '\\") + s[i] + "' in line number " + line_s);
            }
        }

        return out;
    }

    /* Replaces ${VAR} and $VAR with values already parsed from this file,
     * falling back to the process environment. A variable that resolves to
     * nothing is left as an empty string, matching typical dotenv behavior.
     */
    std::string expand_variables(const std::string& value) const {
        std::string out;
        out.reserve(value.size());

        for (size_t i=0; i<value.size(); i++) {
            if (value[i] != '$' || i+1 >= value.size()) {
                out.push_back(value[i]);
                continue;
            }

            size_t name_start = i + 1;
            size_t name_end;
            bool braced = value[name_start] == '{';
            if (braced) name_start++;

            // Identifiers must start with a letter or underscore - same rule
            // as key names elsewhere in this class. Without this check,
            // "$5 only" would treat "5" as a (nonexistent) variable name and
            // silently eat the digit instead of leaving it untouched.
            size_t j = name_start;
            if (j < value.size() &&
                (std::isalpha(static_cast<unsigned char>(value[j])) || value[j] == '_')) {
                ++j;
                while (j < value.size() && (std::isalnum(static_cast<unsigned char>(value[j])) || value[j] == '_'))
                    j++;
            }

            name_end = j;

            if (name_end == name_start) {
                // Lone '$' with no valid identifier after it - keep it literal.
                out.push_back('$');
                continue;
            }

            if (braced) {
                if (j >= value.size() || value[j] != '}') {
                    // Unterminated ${... - treat literally rather than throwing,
                    // since this is a value-level nicety, not a syntax rule.
                    out.push_back('$');
                    continue;
                }
                j++; // consume '}'
            }

            std::string name = value.substr(name_start, name_end - name_start);
            if (auto it = values_.find(name); it != values_.end()) {
                out += it->second;
            } else if (const char* fromEnv = std::getenv(name.c_str())) {
                out += fromEnv;
            }
            // else: undefined reference expands to empty string.

            i = j-1; // -1 because the for-loop will i++
        }

        return out;
    }
};

} // namespace dotenvy
