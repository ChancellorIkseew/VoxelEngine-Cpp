#include "api_lua.hpp"

#include "voxels/Pathfinding.hpp"
#include "world/Level.hpp"

using namespace scripting;

static voxels::Agent* get_agent(lua::State* L) {
    return level->pathfinding->getAgent(lua::tointeger(L, 1));
}

static int l_create_agent(lua::State* L) {
    return lua::pushinteger(L, level->pathfinding->createAgent());
}

static int l_set_enabled(lua::State* L) {
    if (auto agent = get_agent(L)) {
        agent->enabled = lua::toboolean(L, 2);
    }
    return 0;
}

static int l_is_enabled(lua::State* L) {
    if (auto agent = get_agent(L)) {
        return lua::pushboolean(L, agent->enabled);
    }
    return lua::pushboolean(L, false);
}

static int l_make_route(lua::State* L) {
    if (auto agent = get_agent(L)) {
        auto start = lua::tovec3(L, 2);
        auto target = lua::tovec3(L, 3);
        auto route = level->pathfinding->perform(*agent, start, target);
        agent->route = route;
        if (!route.found) {
            return 0;
        }
        lua::createtable(L, route.nodes.size(), 0);
        for (int i = 0; i < route.nodes.size(); i++) {
            lua::pushvec3(L, route.nodes[i].pos);
            lua::rawseti(L, i + 1);
        }
        return 1;
    }
    return 0;
}

static int l_set_max_visited_blocks(lua::State* L) {
    if (auto agent = get_agent(L)) {
        agent->maxVisitedBlocks = lua::tointeger(L, 2);
    }
    return 0;
}

const luaL_Reg pathfindinglib[] = {
    {"create_agent", lua::wrap<l_create_agent>},
    {"set_enabled", lua::wrap<l_set_enabled>},
    {"is_enabled", lua::wrap<l_is_enabled>},
    {"make_route", lua::wrap<l_make_route>},
    {"set_max_visited", lua::wrap<l_set_max_visited_blocks>},
    {NULL, NULL}
};
