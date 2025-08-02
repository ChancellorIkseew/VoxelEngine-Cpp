#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <vector>
#include <memory>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <unordered_map>
#include <unordered_set>

class Level;
class GlobalChunks;

namespace voxels {
    struct RouteNode {
        glm::ivec3 pos;
    };

    struct Route {
        bool found;
        std::vector<RouteNode> nodes;
        std::unordered_set<glm::ivec3> visited;
    };

    struct Agent {
        bool enabled = false;
        bool mayBeIncomplete = true;
        int height = 2;
        int maxVisitedBlocks = 1e3;
        glm::ivec3 start;
        glm::ivec3 target;
        Route route;
    };

    struct Map {
        int width;
        int height;
        std::unique_ptr<uint8_t[]> map;

        Map(int width, int height)
            : width(width),
              height(height),
              map(std::make_unique<uint8_t[]>(width * height)) {
        }

        uint8_t& operator[](int i) {
            return map[i];
        }

        const uint8_t& operator[](int i) const {
            return map[i];
        }
    };

    class Pathfinding {
    public:
        Pathfinding(const Level& level);

        int createAgent();

        Route perform(
            const Agent& agent, const glm::ivec3& start, const glm::ivec3& end
        );

        Agent* getAgent(int id);

        const std::unordered_map<int, Agent>& getAgents() const;
    private:
        const Level& level;
        const GlobalChunks& chunks;
        std::unordered_map<int, Agent> agents;
        int nextAgent = 1;

        int getSurfaceAt(const glm::ivec3& pos, int maxDelta);
    };
}
