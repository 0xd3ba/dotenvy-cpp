#include <iostream>
#include "dotenvy/dotenvy.hpp"


int main() {
    dotenvy::DotEnvy env{};

    /* load() only parses the file into memory. Use load_dotenv() instead if
     * you also want the values pushed into the process environment (i.e.
     * visible to std::getenv / child processes)
     */
    try {
        env.load("example.env");
    } catch (const dotenvy::DotEnvyError& e) {
        std::cerr << "Failed to load example.env: " << e.what() << '\n';
        return 1;
    }

    // get(key, default) -- never throws for a missing key, falls back cleanly
    std::cout << "APP_NAME    = " << env.get("APP_NAME", "unknown") << '\n';
    std::cout << "APP_ENV     = " << env.get("APP_ENV", "unknown") << '\n';
    std::cout << "PORT        = " << env.get("PORT", 0) << '\n';
    std::cout << "DEBUG       = " << std::boolalpha << env.get("DEBUG", false) << '\n';
    std::cout << "DB_PASSWORD = " << env.get("DB_PASSWORD", "") << '\n';
    std::cout << "GREETING    = " << env.get("GREETING", "") << '\n';
    std::cout << "TIMEOUT     = " << env.get("TIMEOUT", -1) << '\n';
    std::cout << "BASE_URL    = " << env.get("BASE_URL", "") << '\n';
    std::cout << "FULL_URL    = " << env.get("FULL_URL", "") << '\n';
    std::cout << "HOME_DIR    = " << env.get("HOME_DIR", "") << '\n';
    std::cout << "MISSING_KEY present? " << env.contains("MISSING_KEY") << '\n';
    std::cout << '\n';

    // get<T>(key) with no default -- std::nullopt if missing, throws
    // DotEnvError if present but not convertible to T. This is the
    // distinction that matters: a typo'd key and a garbage value are
    // different bugs and now fail differently.
    if (auto port = env.get<int>("PORT")) {
        std::cout << "PORT as optional<int> = " << *port << '\n';
    }

    try {
        env.get<int>("APP_NAME");  // present, but not numeric. Throws error.
    } catch (const dotenvy::DotEnvyError& e) {
        std::cout << "Expected conversion failure: " << e.what() << '\n';
    }

    // dotenv_values() -- a snapshot copy of everything that was parsed
    std::cout << "\nAll loaded keys:\n";
    for (const auto& [key, value] : env.dotenv_values())
        std::cout << "  " << key << " = " << value << '\n';

    /* load_dotenv() = load() + apply_to_environment() in one call, handy
     * for "just make these visible to std::getenv / subprocesses" use cases
     */
    dotenvy::DotEnvy env2{};
    env2.load_dotenv("example.env", /*overwrite=*/true);
    std::cout << "\nAPP_NAME via std::getenv after load_dotenv(): "
              << (std::getenv("APP_NAME") ? std::getenv("APP_NAME") : "(not set)") << '\n';

    return 0;
}
