#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "Recipe.hpp" 

using recipes_map = std::unordered_map<std::string, std::unique_ptr<Recipe>>;

class Recipes {
	static inline recipes_map map;
public:

    static void add(std::string name, Recipe recipe);
    static void remove(std::string name);
    static void load(const std::string& filename, const std::string& source);
};
