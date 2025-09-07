local body = entity.rigidbody
local tsf = entity.transform
local rig = entity.skeleton

local props = {}

local function def_prop(name, def_value)
    props[name] = SAVED_DATA[name] or ARGS[name] or def_value
    this["get_"..name] = function() return props[name] end
    this["set_"..name] = function(value)
        props[name] = value
        if math.abs(value - def_value) < 1e-7 then
            SAVED_DATA[name] = nil
        else
            SAVED_DATA[name] = value
        end
    end
end

def_prop("jump_force", 0.0)
def_prop("air_damping", 1.0)
def_prop("ground_damping", 1.0)
def_prop("movement_speed", 3.0)
def_prop("run_speed_mul", 1.5)
def_prop("crouch_speed_mul", 0.35)
def_prop("flight_speed_mul", 4.0)
def_prop("gravity_scale", 1.0)

local dir = {0, 0, -1}
local flight = false

function jump(multiplier)
    if body:is_grounded() then
        local vel = body:get_vel()
        body:set_vel(
            vec3.add(vel, {0, props.jump_force * (multiplier or 1.0), 0}, vel))
    end
end

function elevate(speed, delta, vel)
    vel = vel or body:get_vel()
    body:set_vel(
        vec3.add(vel, {0, speed * delta * props.movement_speed, 0}, vel))
end

function lower(speed, delta, vel)
    vel = vel or body:get_vel()
    body:set_vel(
        vec3.add(vel, {0, -speed * delta * props.movement_speed, 0}, vel))
end

local function move_horizontal(speed, dir, vel)
    vel = vel or body:get_vel()
    if vec3.length(dir) > 0.0 then
        vec3.normalize(dir, dir)

        local magnitude = vec3.length({vel[1], 0, vel[3]})

        if magnitude <= 1e-4 or (magnitude < speed or vec3.dot(
            {vel[1] / magnitude, 0.0, vel[3] / magnitude}, dir) < 0.9)
        then
            vel[1] = vel[1] + dir[1] * speed * 0.8
            vel[3] = vel[3] + dir[3] * speed * 0.8
        end
        magnitude = vec3.length({vel[1], 0, vel[3]})
        if vec3.dot({vel[1] / magnitude, 0.0, vel[3] / magnitude}, dir) > 0.5 then
            vel[1] = vel[1] / magnitude * speed
            vel[3] = vel[3] / magnitude * speed
        end
    end
    body:set_vel(vel)
end

function go(dir, speed_multiplier, sprint, crouch, vel)
    local speed = props.movement_speed * speed_multiplier
    if flight then
        speed = speed * props.flight_speed_mul
    end
    if sprint then
        speed = speed * props.run_speed_mul
    elseif crouch then
        speed = speed * props.crouch_speed_mul
    end
    move_horizontal(speed, dir, vel)
end

local headIndex = rig:index("head")

-- todo: move somewhere
local function update_head(point)
    local pos = tsf:get_pos()
    local viewdir = vec3.normalize(vec3.sub(point, pos))

    local dot = vec3.dot(viewdir, dir)
    if dot > 0.0 then
        dir[1] = dir[1] * 0.8 + viewdir[1] * 0.2
        dir[3] = dir[3] * 0.8 + viewdir[3] * 0.2
    else
        viewdir = mat4.mul(tsf:get_rot(), {0, 0, -1})
    end

    local headrot = mat4.idt()
    local curdir = mat4.mul(mat4.mul(tsf:get_rot(), rig:get_matrix(headIndex)), {0, 0, -1})

    vec3.mix(curdir, viewdir, 0.2, viewdir)

    headrot = mat4.inverse(mat4.look_at({0,0,0}, viewdir, {0, 1, 0}))
    headrot = mat4.mul(mat4.inverse(tsf:get_rot()), headrot)
    rig:set_matrix(headIndex, headrot)
end

function follow_waypoints(pathfinding)
    local pos = tsf:get_pos()
    local waypoint = pathfinding.next_waypoint()
    if not waypoint then
        return
    end
    local speed = props.movement_speed
    local vel = body:get_vel()
    dir = vec3.sub(
        vec3.add(waypoint, {0.5, 0, 0.5}),
        {pos[1], math.floor(pos[2]), pos[3]}
    )
    local upper = dir[2] > 0
    dir[2] = 0.0
    vec3.normalize(dir, dir)
    move_horizontal(speed, dir, vel)
    if upper and body:is_grounded() then
        jump(1.0)
    end
end

function set_dir(new_dir)
    dir = new_dir
end

function is_flight() return flight end

function set_flight(flag) flight = flag end

local prev_angle = 0.0

local function normalize_angle(angle)
    while angle > 180 do
        angle = angle - 360
    end
    while angle <= -180 do
        angle = angle + 360
    end
    return angle
end

local function angle_delta(a, b)
    return normalize_angle(a - b)
end

function on_physics_update(tps)
    local delta = (1.0 / tps)

    local grounded = body:is_grounded()
    body:set_vdamping(flight)
    body:set_gravity_scale({0, flight and 0.0 or props.gravity_scale, 0})
    body:set_linear_damping(
        (flight or not grounded) and props.air_damping or props.ground_damping
    )

    local new_angle = (vec2.angle({dir[3], dir[1]})) % 360
    local angle = prev_angle

    local adelta = angle_delta(
        normalize_angle(new_angle),
        normalize_angle(prev_angle)
    )
    local rotate_speed = entity:get_player() == -1 and 200 or 400

    if math.abs(adelta) > 5 then
        if adelta > 0 then
            angle = angle + delta * rotate_speed
        else
            angle = angle - delta * rotate_speed
        end
    end
    tsf:set_rot(mat4.rotate({0, 1, 0}, angle + 180))
    prev_angle = angle

    if entity:get_player() == -1 then
        update_head({player.get_pos(hud.get_player())})
    else
        local cam = cameras.get("core:first-person")
        update_head(vec3.add({player.get_pos(entity:get_player())}, cam:get_front()))
    end
end
