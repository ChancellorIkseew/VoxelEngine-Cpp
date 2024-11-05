#include "coders/commons.hpp"
#include "items/Recipes.hpp"
#include "util/stringutil.hpp"
#include "api_lua.hpp"

using namespace scripting;

static int l_add_recipe(lua::State* L) {
    std::string name = lua::require_string(L, 1);
    itemid_t product = lua::tointeger(L, 2);
    auto strOrder = lua::require_string(L, 3);
    ItemsOrder itOrd = ItemsOrder::FREE;
    if (std::strcmp(strOrder, "free") != 0) {
        itOrd = ItemsOrder::POSITIONAL;
    }
    const std::string strIngr = lua::require_string(L, 4);
    std::vector<std::string> vec = util::split(strIngr, 2);

    std::vector<itemid_t> ingr;

    Recipe recipe(product, itOrd, ingr);
    Recipes::add(name, std::move(recipe));
	return 0;
}

static int l_remove_recipe(lua::State* L) {


	return 0;
}

const luaL_Reg craftinglib[] = {
    {"add_recipe", lua::wrap<l_add_recipe>},
    {"remove_recipe", lua::wrap<l_remove_recipe>},
    {NULL, NULL}};
