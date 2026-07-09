#include <cstdlib>
#include <sstream>
#include <catch2/catch_test_macros.hpp>
#include "dotenvy/dotenvy.hpp"

using dotenvy::DotEnvy;
using dotenvy::DotEnvError;

/* Helper method to load the string representation of an *.env */
static DotEnvy load_env_from_string(const std::string& contents) {
    std::istringstream in(contents);
    DotEnvy env{};
    env.load(in);
    return env;
}

/* TEST SUITE - 1
 * Core parsing Logic
 */
TEST_CASE("basic KEY=VALUE parsing", "[core]") {
    auto env = load_env_from_string("NAME=hello\nPORT=8080\n");
    REQUIRE(env.contains("NAME"));
    REQUIRE(env.get("NAME") == "hello");
    REQUIRE(env.get<int>("PORT") == 8080);
}

TEST_CASE("blank lines and comments are ignored", "[core]") {
    auto env = load_env_from_string(
        "# comment\n"
        "\n"
        "   \n"
        "NAME=hello\n"
        "# another comment\n"
    );
    REQUIRE(env.dotenv_values().size() == 1);
    REQUIRE(env.get("NAME") == "hello");
}

TEST_CASE("export prefix is stripped", "[core]") {
    auto env = load_env_from_string("export NAME=hello\n");
    REQUIRE(env.get("NAME") == "hello");
}

TEST_CASE("surrounding whitespace around key and value is trimmed", "[core]") {
    auto env = load_env_from_string("  NAME  =   hello world   \n");
    REQUIRE(env.get("NAME") == "hello world");
}

TEST_CASE("trailing whitespace is actually trimmed", "[core]") {
    // Regression test: the original trim() had `end < start` instead of
    // `end > start`, so the second while-loop never ran and trailing
    // whitespace silently survived.
    auto env = load_env_from_string("NAME=hello   \n");
    REQUIRE(env.get("NAME") == "hello");
    REQUIRE(env.get("NAME")->size() == 5);
}

TEST_CASE("VAR= with no value stores an empty string, not a missing key", "[core]") {
    auto env = load_env_from_string("VAR=\n");
    REQUIRE(env.contains("VAR"));
    REQUIRE(env.get("VAR") == "");
    REQUIRE_FALSE(env.contains("NEVER_DEFINED"));
}

TEST_CASE("duplicate keys: last one wins", "[core]") {
    auto env = load_env_from_string("NAME=first\nNAME=second\n");
    REQUIRE(env.get("NAME") == "second");
}

/* TEST SUITE - 2
 * Quote Logic
 */
TEST_CASE("single-quoted values are taken literally", "[quoting]") {
    auto env = load_env_from_string(R"(VAR='raw # not a comment \n stays literal')");
    REQUIRE(env.get("VAR") == R"(raw # not a comment \n stays literal)");
}

TEST_CASE("double-quoted values process escape sequences", "[quoting]") {
    auto env = load_env_from_string(R"(VAR="line1\nline2\ttabbed\"quoted\"")");
    REQUIRE(env.get("VAR") == "line1\nline2\ttabbed\"quoted\"");
}

TEST_CASE("double-quoted values allow '#' and internal spaces", "[quoting]") {
    auto env = load_env_from_string(R"(VAR="  has # inside and spaces  ")");
    REQUIRE(env.get("VAR") == "  has # inside and spaces  ");
}

TEST_CASE("unquoted values strip inline comments", "[quoting]") {
    auto env = load_env_from_string("TIMEOUT=30 # seconds\n");
    REQUIRE(env.get("TIMEOUT") == "30");
}

/* TEST SUITE - 3
 * Variable Expansion Logic
 */
TEST_CASE("${VAR} expands using an earlier key in the same file", "[expansion]") {
    auto env = load_env_from_string("BASE=https://example.com\nFULL=${BASE}/v1\n");
    REQUIRE(env.get("FULL") == "https://example.com/v1");
}

TEST_CASE("$VAR (unbraced) also expands", "[expansion]") {
    auto env = load_env_from_string("BASE=example\nFULL=$BASE/v1\n");
    REQUIRE(env.get("FULL") == "example/v1");
}

TEST_CASE("undefined variable reference expands to empty string", "[expansion]") {
    auto env = load_env_from_string("FULL=${NOPE}/v1\n");
    REQUIRE(env.get("FULL") == "/v1");
}

TEST_CASE("expansion falls back to the process environment", "[expansion]") {
    setenv("DOTENVY_TEST_VAR", "from_process_env", 1);
    auto env = load_env_from_string("VAR=${DOTENVY_TEST_VAR}\n");
    REQUIRE(env.get("VAR") == "from_process_env");
}

TEST_CASE("a leading digit does not count as a variable name", "[expansion]") {
    auto env = load_env_from_string("PRICE=$5 only\n");
    REQUIRE(env.get("PRICE") == "$5 only");
}

TEST_CASE("unterminated ${ is kept literal rather than throwing", "[expansion]") {
    auto env = load_env_from_string("VAR=${OOPS\n");
    REQUIRE(env.get("VAR") == "${OOPS");
}

/* TEST SUITE - 4
 * Parsing Errors
 */
TEST_CASE("malformed line throws DotEnvError", "[errors]") {
    std::istringstream in("GOOD=1\nnot valid\n");
    DotEnvy env{};
    REQUIRE_THROWS_AS(env.load(in), DotEnvError);
}

TEST_CASE("invalid key names are rejected", "[errors]") {
    std::istringstream in("1BAD=value\n");
    DotEnvy env{};
    REQUIRE_THROWS_AS(env.load(in), DotEnvError);
}

TEST_CASE("OnError::Skip continues past malformed lines", "[errors]") {
    std::istringstream in("GOOD=1\nnot valid\nALSO_GOOD=2\n");
    DotEnvy env{};
    env.load(in, DotEnvy::OnError::Skip);
    REQUIRE(env.get("GOOD") == "1");
    REQUIRE(env.get("ALSO_GOOD") == "2");
}

TEST_CASE("trailing backslash in a quoted value is an error", "[errors]") {
    std::istringstream in("VAR=\"ab\\\"\n");   // VAR="ab\"  (dangling escape)
    DotEnvy env{};
    REQUIRE_THROWS_AS(env.load(in), DotEnvError);
}

/* TEST SUITE - 5
 * Line Ending / Encoding Edge Cases
 */
TEST_CASE("Windows-style CRLF line endings don't leak into values", "[line-endings]") {
    auto env = load_env_from_string("VAR=value\r\nOTHER=next\r\n");
    REQUIRE(env.get("VAR") == "value");
    REQUIRE(env.get("OTHER") == "next");
}

TEST_CASE("UTF-8 BOM at the start of the file is stripped", "[line-endings]") {
    auto env = load_env_from_string("\xEF\xBB\xBFVAR=value\n");
    REQUIRE(env.get("VAR") == "value");
}

/* TEST SUITE - 6
 * Typed Accessors
 * 6.1 - Missing key returns std::nullopt
 */
TEST_CASE("get<T>() returns nullopt for a missing key, for every T", "[accessors][missing]") {
    auto env = load_env_from_string("VAR=1\n");
    REQUIRE(env.get("MISSING") == std::nullopt);
    REQUIRE(env.get<int>("MISSING") == std::nullopt);
    REQUIRE(env.get<double>("MISSING") == std::nullopt);
    REQUIRE(env.get<bool>("MISSING") == std::nullopt);
}

/* 6.2 - Mismatch of datatype */
TEST_CASE("get<int>() throws DotEnvError when the value isn't numeric", "[accessors][mismatch]") {
    auto env = load_env_from_string("NAME=hello\n");
    REQUIRE_THROWS_AS(env.get<int>("NAME"), DotEnvError);
}

TEST_CASE("get<double>() throws DotEnvError when the value isn't numeric", "[accessors][mismatch]") {
    auto env = load_env_from_string("NAME=hello\n");
    REQUIRE_THROWS_AS(env.get<double>("NAME"), DotEnvError);
}

TEST_CASE("get<int>() throws DotEnvError on trailing garbage", "[accessors][mismatch]") {
    // "8080abc" doesn't fully consume via from_chars -> ptr != end -> throw.
    auto env = load_env_from_string("PORT=8080abc\n");
    REQUIRE_THROWS_AS(env.get<int>("PORT"), DotEnvError);
}

TEST_CASE("get<bool>() throws DotEnvError on an unrecognized spelling", "[accessors][mismatch]") {
    auto env = load_env_from_string("DEBUG=maybe\n");
    REQUIRE_THROWS_AS(env.get<bool>("DEBUG"), DotEnvError);
}

TEST_CASE("get<T>(key, default) also throws on mismatch rather than silently using the default", "[accessors][mismatch]") {
    // This is worth calling out explicitly: since get<T>(key, default) is
    // implemented in terms of get<T>(key), a present-but-invalid value
    // propagates the exception through the defaulted overload too — the
    // default only kicks in when the key is *absent*, never when it's
    // malformed. If that's not the behavior you want, get<T>(key, default)
    // needs its own try/catch around the mismatch case specifically.
    auto env = load_env_from_string("PORT=notanumber\n");
    REQUIRE_THROWS_AS(env.get("PORT", 0), DotEnvError);
    // TODO
}

/* 6.3 - Present and valid */
TEST_CASE("get<int> and get<double> parse valid numeric values", "[accessors][valid]") {
    auto env = load_env_from_string("PORT=8080\nRATIO=3.14\n");
    REQUIRE(env.get<int>("PORT") == 8080);
    REQUIRE(env.get<double>("RATIO") == 3.14);
}

TEST_CASE("get<bool> accepts true/false/1/0 (case-insensitive)", "[accessors][valid]") {
    auto env = load_env_from_string("A=true\nB=false\nC=1\nD=0\nE=TRUE\n");
    REQUIRE(env.get<bool>("A") == true);
    REQUIRE(env.get<bool>("B") == false);
    REQUIRE(env.get<bool>("C") == true);
    REQUIRE(env.get<bool>("D") == false);
    REQUIRE(env.get<bool>("E") == true);
}

TEST_CASE("get(key, default) returns the stored value when present and valid", "[accessors][valid]") {
    auto env = load_env_from_string("NAME=svc\nPORT=8080\n");
    REQUIRE(env.get("NAME", "fallback") == "svc");
    REQUIRE(env.get("PORT", 0) == 8080);
}

TEST_CASE("get(key, default) falls back only when the key is missing", "[accessors][valid]") {
    auto env = load_env_from_string("NAME=svc\n");
    REQUIRE(env.get("MISSING", "fallback") == "fallback");
    REQUIRE(env.get("MISSING", 1234) == 1234);
    REQUIRE(env.get("MISSING", false) == false);
}

TEST_CASE("string literal default deduces std::string, not const char*", "[accessors][valid]") {
    auto env = load_env_from_string("NAME=svc\n");
    std::string result = env.get("NAME", "fallback");
    REQUIRE(result == "svc");
    static_assert(std::is_same_v<decltype(env.get("NAME", "fallback")), std::string>);
}

/* TEST SUITE - 7
 * Environment variables in the map match
 */
TEST_CASE("dotenv_values() returns a snapshot copy of all parsed entries", "[values]") {
    auto env = load_env_from_string("A=1\nB=2\nC=3\n");
    auto all = env.dotenv_values();
    REQUIRE(all.size() == 3);
    REQUIRE(all.at("A") == "1");
    REQUIRE(all.at("B") == "2");
    REQUIRE(all.at("C") == "3");
}

/* TEST SUITE - 8
 * Applying env variable to the environment
 */
TEST_CASE("apply_to_environment pushes parsed values into the process environment", "[env]") {
    auto env = load_env_from_string("DOTENVY_APPLY_TEST=applied_value\n");
    env.apply_to_environment(/*overwrite=*/true);

    const char* from_env = std::getenv("DOTENVY_APPLY_TEST");
    REQUIRE(from_env != nullptr);
    REQUIRE(std::string(from_env) == "applied_value");
}

TEST_CASE("apply_to_environment respects overwrite=false", "[env]") {

    #if defined(_WIN32)
    _putenv_s("DOTENVY_NO_OVERWRITE", "original");
    #else
    setenv("DOTENVY_NO_OVERWRITE", "original", 1);
    #endif

    auto env = load_env_from_string("DOTENVY_NO_OVERWRITE=new_value\n");
    env.apply_to_environment(/*overwrite=*/false);

    const char* value = std::getenv("DOTENVY_NO_OVERWRITE");
    REQUIRE(value != nullptr);
    REQUIRE(std::string{value} == "original");
}

TEST_CASE("load_dotenv loads and applies to the process environment in one call", "[env]") {
    std::istringstream in("DOTENVY_LOAD_DOTENV_TEST=combined\n");
    DotEnvy env{};
    env.load_dotenv(in, /*overwrite=*/true);

    REQUIRE(env.get("DOTENVY_LOAD_DOTENV_TEST") == "combined");
    const char* value = std::getenv("DOTENVY_LOAD_DOTENV_TEST");
    REQUIRE(value != nullptr);
    REQUIRE(std::string{value} == "combined");
}