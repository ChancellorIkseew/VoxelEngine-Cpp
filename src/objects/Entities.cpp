#define VC_ENABLE_REFLECTION
#include "Entities.hpp"

#include <glm/ext/matrix_transform.hpp>
#include <sstream>

#include "assets/Assets.hpp"
#include "content/Content.hpp"
#include "data/dv_util.hpp"
#include "debug/Logger.hpp"
#include "engine/Engine.hpp"
#include "graphics/core/DrawContext.hpp"
#include "graphics/core/LineBatch.hpp"
#include "graphics/commons/Model.hpp"
#include "graphics/render/ModelBatch.hpp"
#include "logic/scripting/scripting.hpp"
#include "maths/FrustumCulling.hpp"
#include "maths/rays.hpp"
#include "EntityDef.hpp"
#include "rigging.hpp"
#include "physics/Hitbox.hpp"
#include "physics/PhysicsSolver.hpp"
#include "world/Level.hpp"

static debug::Logger logger("entities");

static inline std::string COMP_TRANSFORM = "transform";
static inline std::string COMP_RIGIDBODY = "rigidbody";
static inline std::string COMP_SKELETON = "skeleton";
static inline std::string SAVED_DATA_VARNAME = "SAVED_DATA";

void Transform::refresh() {
    combined = glm::mat4(1.0f);
    combined = glm::translate(combined, pos);
    combined = combined * glm::mat4(rot);
    combined = glm::scale(combined, size);
    displayPos = pos;
    displaySize = size;
    dirty = false;
}

void Entity::setInterpolatedPosition(const glm::vec3& position) {
    getSkeleton().interpolation.refresh(position);
}

glm::vec3 Entity::getInterpolatedPosition() const {
    const auto& skeleton = getSkeleton();
    if (skeleton.interpolation.isEnabled()) {
        return skeleton.interpolation.getCurrent();
    }
    return getTransform().pos;
}

void Entity::destroy() {
    if (isValid()) {
        entities.despawn(id);
    }
}

rigging::Skeleton& Entity::getSkeleton() const {
    return registry.get<rigging::Skeleton>(entity);
}

void Entity::setRig(const rigging::SkeletonConfig* rigConfig) {
    auto& skeleton = registry.get<rigging::Skeleton>(entity);
    skeleton.config = rigConfig;
    skeleton.pose.matrices.resize(
        rigConfig->getBones().size(), glm::mat4(1.0f)
    );
    skeleton.calculated.matrices.resize(
        rigConfig->getBones().size(), glm::mat4(1.0f)
    );
}

Entities::Entities(Level& level)
    : level(level), sensorsTickClock(20, 3), updateTickClock(20, 3) {
}

template <void (*callback)(const Entity&, size_t, entityid_t)>
static sensorcallback create_sensor_callback(Entities* entities) {
    return [=](auto entityid, auto index, auto otherid) {
        if (auto entity = entities->get(entityid)) {
            if (entity->isValid()) {
                callback(*entity, index, otherid);
            }
        }
    };
}

static void initialize_body(
    const EntityDef& def, Rigidbody& body, entityid_t id, Entities* entities
) {
    body.sensors.resize(def.radialSensors.size() + def.boxSensors.size());
    for (auto& [i, box] : def.boxSensors) {
        SensorParams params {};
        params.aabb = box;
        body.sensors[i] = Sensor {
            true,
            SensorType::AABB,
            i,
            id,
            params,
            params,
            {},
            {},
            create_sensor_callback<scripting::on_sensor_enter>(entities),
            create_sensor_callback<scripting::on_sensor_exit>(entities)};
    }
    for (auto& [i, radius] : def.radialSensors) {
        SensorParams params {};
        params.radial = glm::vec4(radius);
        body.sensors[i] = Sensor {
            true,
            SensorType::RADIUS,
            i,
            id,
            params,
            params,
            {},
            {},
            create_sensor_callback<scripting::on_sensor_enter>(entities),
            create_sensor_callback<scripting::on_sensor_exit>(entities)};
    }
}

entityid_t Entities::spawn(
    const EntityDef& def,
    glm::vec3 position,
    dv::value args,
    dv::value saved,
    entityid_t uid
) {
    auto skeleton = level.content.getSkeleton(def.skeletonName);
    if (skeleton == nullptr) {
        throw std::runtime_error("skeleton " + def.skeletonName + " not found");
    }
    entityid_t id;
    if (uid == 0) {
        id = nextID++;
    } else {
        id = uid;
        if (auto found = get(uid)) {
            std::stringstream ss;
            ss << "UID #" << uid << " is already used by an entity ";
            ss << found->getDef().name;
            if (found->getID().destroyFlag) {
                ss << " marked to destroy";
            }
            throw std::runtime_error(ss.str());
        }
    }
    auto entity = registry.create();
    entities[id] = entity;
    uids[entity] = id;

    registry.emplace<EntityId>(entity, static_cast<entityid_t>(id), def);
    const auto& tsf = registry.emplace<Transform>(
        entity,
        position,
        glm::vec3(1.0f),
        glm::mat3(1.0f),
        glm::mat4(1.0f),
        true
    );
    auto& body = registry.emplace<Rigidbody>(
        entity,
        true,
        Hitbox {def.bodyType, position, def.hitbox * 0.5f},
        std::vector<Sensor> {}
    );
    initialize_body(def, body, id, this);

    auto& scripting = registry.emplace<ScriptComponents>(entity);
    registry.emplace<rigging::Skeleton>(entity, skeleton->instance());

    for (auto& componentName : def.components) {
        auto component = std::make_unique<UserComponent>(
            componentName, EntityFuncsSet {}, nullptr
        );
        scripting.components.emplace_back(std::move(component));
    }
    dv::value componentsMap = nullptr;
    if (saved != nullptr) {
        componentsMap = saved["comps"];
        loadEntity(saved, get(id).value());
    }
    body.hitbox.position = tsf.pos;
    scripting::on_entity_spawn(
        def, id, scripting.components, args, componentsMap);
    return id;
}

void Entities::despawn(entityid_t id) {
    if (auto entity = get(id)) {
        auto& eid = entity->getID();
        if (!eid.destroyFlag) {
            eid.destroyFlag = true;
            scripting::on_entity_despawn(*entity);
        }
    }
}

void Entities::loadEntity(const dv::value& map) {
    entityid_t uid = map["uid"].asInteger();
    std::string defname = map["def"].asString();
    auto& def = level.content.entities.require(defname);
    spawn(def, {}, nullptr, map, uid);
}

void Entities::loadEntity(const dv::value& map, Entity entity) {
    auto& transform = entity.getTransform();
    auto& body = entity.getRigidbody();
    auto& skeleton = entity.getSkeleton();

    if (map.has(COMP_RIGIDBODY)) {
        auto& bodymap = map[COMP_RIGIDBODY];
        dv::get_vec(bodymap, "vel", body.hitbox.velocity);
        std::string bodyTypeName;
        map.at("type").get(bodyTypeName);
        BodyTypeMeta.getItem(bodyTypeName, body.hitbox.type);
        bodymap["crouch"].asBoolean(body.hitbox.crouching);
        bodymap["damping"].asNumber(body.hitbox.linearDamping);
    }
    if (map.has(COMP_TRANSFORM)) {
        auto& tsfmap = map[COMP_TRANSFORM];
        dv::get_vec(tsfmap, "pos", transform.pos);
        dv::get_vec(tsfmap, "size", transform.size);
        dv::get_mat(tsfmap, "rot", transform.rot);
    }
    std::string skeletonName = skeleton.config->getName();
    map.at("skeleton").get(skeletonName);
    if (skeletonName != skeleton.config->getName()) {
        skeleton.config = level.content.getSkeleton(skeletonName);
    }
    if (auto found = map.at(COMP_SKELETON)) {
        auto& skeletonmap = *found;
        if (auto found = skeletonmap.at("textures")) {
            auto& texturesmap = *found;
            for (auto& [slot, _] : texturesmap.asObject()) {
                texturesmap.at(slot).get(skeleton.textures[slot]);
            }
        }
        if (auto found = skeletonmap.at("pose")) {
            auto& posearr = *found;
            for (size_t i = 0;
                 i < std::min(skeleton.pose.matrices.size(), posearr.size());
                 i++) {
                dv::get_mat(posearr[i], skeleton.pose.matrices[i]);
            }
        }
    }
}

std::optional<Entities::RaycastResult> Entities::rayCast(
    glm::vec3 start, glm::vec3 dir, float maxDistance, entityid_t ignore
) {
    Ray ray(start, dir);
    auto view = registry.view<EntityId, Transform, Rigidbody>();

    entityid_t foundUID = 0;
    glm::ivec3 foundNormal;

    for (auto [entity, eid, transform, body] : view.each()) {
        if (eid.uid == ignore || !body.enabled) {
            continue;
        }
        auto& hitbox = body.hitbox;
        glm::ivec3 normal;
        double distance;
        if (ray.intersectAABB(
                glm::vec3(), hitbox.getAABB(), maxDistance, normal, distance
            ) > RayRelation::None) {
            foundUID = eid.uid;
            foundNormal = normal;
            maxDistance = static_cast<float>(distance);
        }
    }
    if (foundUID) {
        return Entities::RaycastResult {foundUID, foundNormal, maxDistance};
    } else {
        return std::nullopt;
    }
}

void Entities::loadEntities(dv::value root) {
    clean();
    auto& list = root["data"];
    for (auto& map : list) {
        try {
            loadEntity(map);
        } catch (const std::runtime_error& err) {
            logger.error() << "could not read entity: " << err.what();
        }
    }
}

void Entities::onSave(const Entity& entity) {
    scripting::on_entity_save(entity);
}

dv::value Entities::serialize(const Entity& entity) {
    auto root = dv::object();
    auto& eid = entity.getID();
    auto& def = eid.def;
    root["def"] = def.name;
    root["uid"] = eid.uid;
    {
        auto& transform = entity.getTransform();
        auto& tsfmap = root.object(COMP_TRANSFORM);
        tsfmap["pos"] = dv::to_value(transform.pos);
        if (transform.size != glm::vec3(1.0f)) {
            tsfmap["size"] = dv::to_value(transform.size);
        }
        if (transform.rot != glm::mat3(1.0f)) {
            tsfmap["rot"] = dv::to_value(transform.rot);
        }
    }
    {
        auto& rigidbody = entity.getRigidbody();
        auto& hitbox = rigidbody.hitbox;
        auto& bodymap = root.object(COMP_RIGIDBODY);
        if (!rigidbody.enabled) {
            bodymap["enabled"] = false;
        }
        if (def.save.body.velocity) {
            bodymap["vel"] = dv::to_value(rigidbody.hitbox.velocity);
        }
        if (def.save.body.settings) {
            bodymap["damping"] = rigidbody.hitbox.linearDamping;
            if (hitbox.type != def.bodyType) {
                bodymap["type"] = BodyTypeMeta.getNameString(hitbox.type);
            }
            if (hitbox.crouching) {
                bodymap["crouch"] = hitbox.crouching;
            }
        }
    }
    auto& skeleton = entity.getSkeleton();
    if (skeleton.config->getName() != def.skeletonName) {
        root["skeleton"] = skeleton.config->getName();
    }
    if (def.save.skeleton.pose || def.save.skeleton.textures) {
        auto& skeletonmap = root.object(COMP_SKELETON);
        if (def.save.skeleton.textures) {
            auto& map = skeletonmap.object("textures");
            for (auto& [slot, texture] : skeleton.textures) {
                map[slot] = texture;
            }
        }
        if (def.save.skeleton.pose) {
            auto& list = skeletonmap.list("pose");
            for (auto& mat : skeleton.pose.matrices) {
                list.add(dv::to_value(mat));
            }
        }
    }
    auto& scripts = entity.getScripting();
    if (!scripts.components.empty()) {
        auto& compsMap = root.object("comps");
        for (auto& comp : scripts.components) {
            auto data =
                scripting::get_component_value(comp->env, SAVED_DATA_VARNAME);
            compsMap[comp->name] = data;
        }
    }
    return root;
}

dv::value Entities::serialize(const std::vector<Entity>& entities) {
    auto list = dv::list();
    for (auto& entity : entities) {
        const EntityId& eid = entity.getID();
        if (!entity.getDef().save.enabled || eid.destroyFlag) {
            continue;
        }
        level.entities->onSave(entity);
        if (!eid.destroyFlag) {
            list.add(level.entities->serialize(entity));
        }
    }
    return list;
}

void Entities::despawn(std::vector<Entity> entities) {
    for (auto& entity : entities) {
        entity.destroy();
    }
}

void Entities::clean() {
    for (auto it = entities.begin(); it != entities.end();) {
        if (!registry.get<EntityId>(it->second).destroyFlag) {
            ++it;
        } else {
            auto& rigidbody = registry.get<Rigidbody>(it->second);
            // todo: refactor
            auto physics = level.physics.get();
            for (auto& sensor : rigidbody.sensors) {
                physics->removeSensor(&sensor);
            }
            uids.erase(it->second);
            registry.destroy(it->second);
            it = entities.erase(it);
        }
    }
}

void Entities::updateSensors(
    Rigidbody& body, const Transform& tsf, std::vector<Sensor*>& sensors
) {
    for (size_t i = 0; i < body.sensors.size(); i++) {
        auto& sensor = body.sensors[i];
        for (auto oid : sensor.prevEntered) {
            if (sensor.nextEntered.find(oid) == sensor.nextEntered.end()) {
                sensor.exitCallback(sensor.entity, i, oid);
            }
        }
        sensor.prevEntered = sensor.nextEntered;
        sensor.nextEntered.clear();

        switch (sensor.type) {
            case SensorType::AABB:
                sensor.calculated.aabb = sensor.params.aabb;
                sensor.calculated.aabb.transform(tsf.combined);
                break;
            case SensorType::RADIUS:
                sensor.calculated.radial = glm::vec4(
                    body.hitbox.position.x,
                    body.hitbox.position.y,
                    body.hitbox.position.z,
                    sensor.params.radial.w * sensor.params.radial.w
                );
                break;
        }
        sensors.push_back(&sensor);
    }
}

void Entities::preparePhysics(float delta) {
    if (sensorsTickClock.update(delta)) {
        auto part = sensorsTickClock.getPart();
        auto parts = sensorsTickClock.getParts();

        auto view = registry.view<EntityId, Transform, Rigidbody>();
        auto physics = level.physics.get();
        std::vector<Sensor*> sensors;
        for (auto [entity, eid, transform, rigidbody] : view.each()) {
            if (!rigidbody.enabled) {
                continue;
            }
            if ((eid.uid + part) % parts != 0) {
                continue;
            }
            updateSensors(rigidbody, transform, sensors);
        }
        physics->setSensors(std::move(sensors));
    }
}

void Entities::updatePhysics(float delta) {
    preparePhysics(delta);

    auto view = registry.view<EntityId, Transform, Rigidbody>();
    auto physics = level.physics.get();
    for (auto [entity, eid, transform, rigidbody] : view.each()) {
        if (!rigidbody.enabled || rigidbody.hitbox.type == BodyType::STATIC) {
            continue;
        }
        auto& hitbox = rigidbody.hitbox;
        auto prevVel = hitbox.velocity;
        bool grounded = hitbox.grounded;

        float vel = glm::length(prevVel);
        int substeps = static_cast<int>(delta * vel * 20);
        substeps = std::min(100, std::max(2, substeps));
        physics->step(*level.chunks, hitbox, delta, substeps, eid.uid);
        hitbox.linearDamping = hitbox.grounded * 24;
        transform.setPos(hitbox.position);
        if (hitbox.grounded && !grounded) {
            scripting::on_entity_grounded(
                *get(eid.uid), glm::length(prevVel - hitbox.velocity)
            );
        }
        if (!hitbox.grounded && grounded) {
            scripting::on_entity_fall(*get(eid.uid));
        }
    }
}

void Entities::update(float delta) {
    if (updateTickClock.update(delta)) {
        scripting::on_entities_update(
            updateTickClock.getTickRate(),
            updateTickClock.getParts(),
            updateTickClock.getPart()
        );
    }
}

static void debug_render_skeleton(
    LineBatch& batch,
    const rigging::Bone* bone,
    const rigging::Skeleton& skeleton
) {
    size_t pindex = bone->getIndex();
    for (auto& sub : bone->getSubnodes()) {
        size_t sindex = sub->getIndex();
        batch.line(
            glm::vec3(
                skeleton.calculated.matrices[pindex] * glm::vec4(0, 0, 0, 1)
            ),
            glm::vec3(
                skeleton.calculated.matrices[sindex] * glm::vec4(0, 0, 0, 1)
            ),
            glm::vec4(0, 0.5f, 0, 1)
        );
        debug_render_skeleton(batch, sub.get(), skeleton);
    }
}

void Entities::renderDebug(
    LineBatch& batch, const Frustum* frustum, const DrawContext& pctx
) {
    {
        auto ctx = pctx.sub(&batch);
        ctx.setLineWidth(1);
        auto view = registry.view<Transform, Rigidbody>();
        for (auto [entity, transform, rigidbody] : view.each()) {
            const auto& hitbox = rigidbody.hitbox;
            const auto& pos = transform.pos;
            const auto& size = transform.size;
            if (frustum && !frustum->isBoxVisible(pos - size, pos + size)) {
                continue;
            }
            batch.box(hitbox.position, hitbox.halfsize * 2.0f, glm::vec4(1.0f));

            for (auto& sensor : rigidbody.sensors) {
                if (sensor.type != SensorType::AABB) continue;
                batch.box(
                    sensor.calculated.aabb.center(),
                    sensor.calculated.aabb.size(),
                    glm::vec4(1.0f, 1.0f, 0.0f, 1.0f)
                );
            }
        }
    }
    {
        auto view = registry.view<Transform, rigging::Skeleton>();
        auto ctx = pctx.sub(&batch);
        ctx.setDepthTest(false);
        ctx.setDepthMask(false);
        ctx.setLineWidth(2);
        for (auto [entity, transform, skeleton] : view.each()) {
            auto config = skeleton.config;
            const auto& pos = transform.pos;
            const auto& size = transform.size;
            if (frustum && !frustum->isBoxVisible(pos - size, pos + size)) {
                continue;
            }
            auto bone = config->getRoot();
            debug_render_skeleton(batch, bone, skeleton);
        }
    }
}

void Entities::render(
    const Assets& assets,
    ModelBatch& batch,
    const Frustum* frustum,
    float delta,
    bool pause
) {
    auto view = registry.view<Transform, rigging::Skeleton>();
    for (auto [entity, transform, skeleton] : view.each()) {
        if (transform.dirty) {
            transform.refresh();
        }
        if (skeleton.interpolation.isEnabled()) {
            skeleton.interpolation.updateTimer(delta);
        }
        const auto& pos = transform.pos;
        const auto& size = transform.size;
        if (!frustum || frustum->isBoxVisible(pos - size, pos + size)) {
            const auto* rigConfig = skeleton.config;
            rigConfig->render(assets, batch, skeleton, transform.combined, pos);
        }
    }
}

bool Entities::hasBlockingInside(AABB aabb) {
    auto view = registry.view<EntityId, Rigidbody>();
    for (auto [entity, eid, body] : view.each()) {
        if (eid.def.blocking && aabb.intersect(body.hitbox.getAABB(), -0.05f)) {
            return true;
        }
    }
    return false;
}

std::vector<Entity> Entities::getAllInside(AABB aabb) {
    std::vector<Entity> collected;
    auto view = registry.view<EntityId, Transform>();
    for (auto [entity, eid, transform] : view.each()) {
        if (!eid.destroyFlag && aabb.contains(transform.pos)) {
            const auto& found = uids.find(entity);
            if (found == uids.end()) {
                continue;
            }
            if (auto wrapper = get(found->second)) {
                collected.push_back(*wrapper);
            }
        }
    }
    return collected;
}

std::vector<Entity> Entities::getAllInRadius(glm::vec3 center, float radius) {
    std::vector<Entity> collected;
    auto view = registry.view<Transform>();
    for (auto [entity, transform] : view.each()) {
        if (glm::distance2(transform.pos, center) <= radius * radius) {
            const auto& found = uids.find(entity);
            if (found == uids.end()) {
                continue;
            }
            if (auto wrapper = get(found->second)) {
                collected.push_back(*wrapper);
            }
        }
    }
    return collected;
}
