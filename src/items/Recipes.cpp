#include "Recipes.hpp"

#include "debug/Logger.hpp"

void Recipes::add(std::string name, Recipe recipe) {
    const auto found = map.find(name);
    if (found != map.end()) {
        throw std::runtime_error("recipe '" + name + "' already exists");
    }
    map.try_emplace(name, std::make_unique<Recipe>(recipe));
}

void Recipes::remove(std::string name) {
    map.erase(name);
}

#include "coders/toml.hpp"

void Recipes::load(
    const std::string& filename, const std::string& source
) {
    /*
    auto map = toml::parse(filename, source);
    for (auto& [sectionName, section] : map.asObject()) {
        for (auto& [name, value] : section.asObject()) {
            auto key = sectionName + "." + name;
            auto [prefix, codename] = util::split_at(value.asString(), ':');
            inputtype type;
            int code;
            if (prefix == "key") {
                type = inputtype::keyboard;
                code = static_cast<int>(input_util::keycode_from(codename));
            } else if (prefix == "mouse") {
                type = inputtype::mouse;
                code = static_cast<int>(input_util::mousecode_from(codename));
            } else {
                logger.error() << "unknown input type: " << prefix
                               << " (binding " << util::quote(key) << ")";
                continue;
            }
            if (bindType == BindType::BIND) {
                Events::bind(key, type, code);
            } else if (bindType == BindType::REBIND) {
                Events::rebind(key, type, code);
            }
        }
    }
    */
}
