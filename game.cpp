#include "precomp.h" // include (only) this in every .cpp file

constexpr auto num_tanks_blue = 2048;
constexpr auto num_tanks_red = 2048;

constexpr auto tank_max_health = 1000;
constexpr auto rocket_hit_value = 60;
constexpr auto particle_beam_hit_value = 50;

constexpr auto tank_max_speed = 1.0;

constexpr auto health_bar_width = 70;

constexpr auto max_frames = 2000;

//Global performance timer
constexpr auto REF_PERFORMANCE = 23993; //UPDATE THIS WITH YOUR REFERENCE PERFORMANCE (see console after 2k frames)
static timer perf_timer;
static float duration;

//Load sprite files and initialize sprites
static Surface* tank_red_img = new Surface("assets/Tank_Proj2.png");
static Surface* tank_blue_img = new Surface("assets/Tank_Blue_Proj2.png");
static Surface* rocket_red_img = new Surface("assets/Rocket_Proj2.png");
static Surface* rocket_blue_img = new Surface("assets/Rocket_Blue_Proj2.png");
static Surface* particle_beam_img = new Surface("assets/Particle_Beam.png");
static Surface* smoke_img = new Surface("assets/Smoke.png");
static Surface* explosion_img = new Surface("assets/Explosion.png");

static Sprite tank_red(tank_red_img, 12);
static Sprite tank_blue(tank_blue_img, 12);
static Sprite rocket_red(rocket_red_img, 12);
static Sprite rocket_blue(rocket_blue_img, 12);
static Sprite smoke(smoke_img, 4);
static Sprite explosion(explosion_img, 9);
static Sprite particle_beam_sprite(particle_beam_img, 3);

const static vec2 tank_size(7, 9);
const static vec2 rocket_size(6, 6);

const static float tank_radius = 3.f;
const static float rocket_radius = 5.f;

const int NUM_OF_THREADS = std::thread::hardware_concurrency() * 2;
ThreadPool* pool = new ThreadPool(NUM_OF_THREADS);
std::mutex mlock;
vector<future<void>> threads;

vector<int> split_evenly(int dividend, int divisor) {
    int remainder = dividend % divisor;

    int initial_value = floor(dividend / divisor);
    vector<int> results;
    for (int i = 0; i < divisor; i++)
        results.push_back(initial_value + (remainder-- > 0 ? 1 : 0));
    return results;
}

void wait_and_clear() {
    for (future<void>& t : threads) {
        t.wait();
    }
    threads.clear();
}


// -----------------------------------------------------------
// Initialize the simulation state
// This function does not count for the performance multiplier
// (Feel free to optimize anyway though ;) )
// -----------------------------------------------------------
void Game::init()
{
    frame_count_font = new Font("assets/digital_small.png", "ABCDEFGHIJKLMNOPQRSTUVWXYZ:?!=-0123456789.");

    active_tanks.reserve(num_tanks_blue + num_tanks_red);

    uint max_rows = 24;

    float start_blue_x = tank_size.x + 40.0f;
    float start_blue_y = tank_size.y + 30.0f;

    float start_red_x = 1088.0f;
    float start_red_y = tank_size.y + 30.0f;

    float spacing = 7.5f;

    //Spawn blue tanks
    for (int i = 0; i < num_tanks_blue; i++)
    {
        vec2 position{ start_blue_x + ((i % max_rows) * spacing), start_blue_y + ((i / max_rows) * spacing) };
        active_tanks.push_back(Tank(position.x, position.y, BLUE, &tank_blue, &smoke, 1100.f, position.y + 16, tank_radius, tank_max_health, tank_max_speed));
    }
    //Spawn red tanks
    for (int i = 0; i < num_tanks_red; i++)
    {
        vec2 position{ start_red_x + ((i % max_rows) * spacing), start_red_y + ((i / max_rows) * spacing) };
        active_tanks.push_back(Tank(position.x, position.y, RED, &tank_red, &smoke, 100.f, position.y + 16, tank_radius, tank_max_health, tank_max_speed));
    }

    particle_beams.push_back(Particle_beam(vec2(590, 327), vec2(100, 50), &particle_beam_sprite, particle_beam_hit_value));
    particle_beams.push_back(Particle_beam(vec2(64, 64), vec2(100, 50), &particle_beam_sprite, particle_beam_hit_value));
    particle_beams.push_back(Particle_beam(vec2(1200, 600), vec2(100, 50), &particle_beam_sprite, particle_beam_hit_value));
}

// -----------------------------------------------------------
// Close down application
// -----------------------------------------------------------
void Game::shutdown()
{
}

// -----------------------------------------------------------
// Iterates through all tanks and returns the closest enemy tank for the given tank
// -----------------------------------------------------------
Tank& Game::find_closest_enemy(Tank& current_tank)
{
    float closest_distance = numeric_limits<float>::infinity();
    int closest_index = 0;

    for (int i = 0; i < active_tanks.size(); i++)
    {
        if (active_tanks.at(i).allignment != current_tank.allignment && active_tanks.at(i).active)
        {
            float sqr_dist = fabsf((active_tanks.at(i).get_position() - current_tank.get_position()).sqr_length());
            if (sqr_dist < closest_distance)
            {
                closest_distance = sqr_dist;
                closest_index = i;
            }
        }
    }

    return active_tanks.at(closest_index);
}

//Checks if a point lies on the left of an arbitrary angled line
bool Tmpl8::Game::left_of_line(vec2 line_start, vec2 line_end, vec2 point)
{
    return ((line_end.x - line_start.x) * (point.y - line_start.y) - (line_end.y - line_start.y) * (point.x - line_start.x)) < 0;
}

// -----------------------------------------------------------
// Update the game state:
// Move all objects
// Update sprite frames
// Collision detection
// Targeting etc..
// -----------------------------------------------------------
// ====
// Big-O analysis simple: O (N�)
// Big-O analysis complex: O (N + N� + N + N + N + N + N� + N� + N� + N� + N) Or O (6N + 5N�)
// ====
void Game::update(float deltaTime)
{
    threads.reserve(NUM_OF_THREADS);
    uni_grid.mlock = &mlock;
    vector<int> split_sizes_tanks = split_evenly(active_tanks.size(), NUM_OF_THREADS);
    //Calculate the route to the destination for each tank using BFS
    //Initializing routes here so it gets counted for performance..
    // ====
    // Big-O analysis: O (N)
    // ====
    if (frame_count == 0)
    {
        int start_at = 0;
        for (int count : split_sizes_tanks) {
            threads.push_back(pool->enqueue([&, start_at, count]() {
                for (int j = start_at; j < start_at + count; j++) {
                    Tank& t = active_tanks.at(j);
                    mlock.lock();
                    t.set_route(background_terrain.get_route(t, t.target));
                    mlock.unlock();

                }
                }));
            start_at += count;
        }
        wait_and_clear();

        // Voeg de moveables toe
        for (Tank& t : active_tanks)
        {
            //t.set_route(background_terrain.get_route(t, t.target));
            uni_grid.add_to_grid(&t, t.position);            
        }
        for (Rocket& r : rockets) {
            uni_grid.add_to_grid(&r, r.position);
        }
        // Voor de particle beam moet ik nog ff kijken morgen.

    }

    for (Tank& tank : active_tanks) {
        vector<movable*> collision_objects = uni_grid.get_neighboring_objects(tank.position);
        for (movable* collision_object : collision_objects)
        {
            if (collision_object->moveable_type == movableType::TANK)
            {
                Tank& collidable_tank = dynamic_cast<Tank&>(collision_object);

                if (&tank == &collidable_tank || !collidable_tank.active) continue;

                vec2 dir = tank.get_position() - collidable_tank.get_position();
                float dir_squared_len = dir.sqr_length();

                float col_squared_len = (tank.get_collision_radius() + collidable_tank.get_collision_radius());
                col_squared_len *= col_squared_len;

                if (dir_squared_len < col_squared_len)
                {
                    tank.push(dir.normalized(), 1.f);
                }
            }
        }
    }

    // TODO: Sort from left to right and up to down, might not be needed. Its only needed for the convex hull algorithm.
    /*int start_at = 0;
    for (int count : split_sizes_tanks) {
        threads.push_back(pool->enqueue([&, start_at, count]() {
            for (int j = start_at; j < start_at + count; j++) {
                Tank& tank = active_tanks.at(j);
                    // Check for tank collision.
                    for (Tank& other_tank : active_tanks)
                    {
                        if (&tank == &other_tank || !other_tank.active) continue;

                        vec2 dir = tank.get_position() - other_tank.get_position();
                        float dir_squared_len = dir.sqr_length();

                        float col_squared_len = (tank.get_collision_radius() + other_tank.get_collision_radius());
                        col_squared_len *= col_squared_len;

                        if (dir_squared_len < col_squared_len)
                        {
                            tank.push(dir.normalized(), 1.f);
                        }
                    }
                    // Check for tank collision with uni_grid.
            }
            }));
        start_at += count;
    }
    wait_and_clear();*/
    /*for (Tank& tank : tanks) {
        if (tank.active)
        {
            active_tanks.push_back(&tank);
            // Check for tank collision.
            for (Tank& other_tank : tanks)
            {
                if (&tank == &other_tank || !other_tank.active) continue;

                vec2 dir = tank.get_position() - other_tank.get_position();
                float dir_squared_len = dir.sqr_length();

                float col_squared_len = (tank.get_collision_radius() + other_tank.get_collision_radius());
                col_squared_len *= col_squared_len;

                if (dir_squared_len < col_squared_len)
                {
                    tank.push(dir.normalized(), 1.f);
                }
            }
        }
    }*/

    for (Rocket& rocket : rockets) {
        rocket.tick();
    }

    //for (vector<Tank*>::iterator tank = active_tanks.begin(); tank != active_tanks.end();) {

    //for (auto tank : active_tanks) {
    int start_at = 0;
    for (int count : split_sizes_tanks) {
        threads.push_back(pool->enqueue([&, start_at, count]() {
            for (int j = start_at; j < start_at + count; j++) {
                Tank& tank = active_tanks.at(j);

                //Move tanks according to speed and nudges (see above) also reload
                if (tank.active) {
                    tank.tick(background_terrain);
                }
                else {
                    inactive_tanks.push_back(tank);
                }

                //Shoot at closest target if reloaded
                if (tank.rocket_reloaded())
                {
                    Tank& target = find_closest_enemy(tank);

                    mlock.lock();
                    rockets.push_back(Rocket(tank.position, (target.get_position() - tank.position).normalized() * 3, rocket_radius, tank.allignment, ((tank.allignment == RED) ? &rocket_red : &rocket_blue)));
                    mlock.unlock();

                    tank.reload_rocket();
                }

                // Check for rocket collision.
                //Check if rocket collides with enemy tank, spawn explosion, and if tank is destroyed spawn a smoke plume
                /*for (Rocket& rocket : rockets)
                {
                    if ((tank.allignment != rocket.allignment) && rocket.intersects(tank.position, tank.collision_radius))
                    {
                        // TODO: Should remove rocket from list
                        rocket.active = false;

                        mlock.lock();
                        explosions.push_back(Explosion(&explosion, tank.position));
                        mlock.unlock();

                        if (tank.hit(rocket_hit_value))
                        {
                            mlock.lock();
                            smokes.push_back(Smoke(smoke, tank.position - vec2(7, 24)));
                            mlock.unlock();
                            break;
                        }
                    }
                }*/

                vector<movable*> collision_objects = uni_grid.get_neighboring_objects(tank.position);
                for (movable* collision_object : collision_objects)
                {
                    if (collision_object->moveable_type == movableType::ROCKET)
                    {
                        Rocket& rocket = dynamic_cast<Rocket&>(collision_object);

                        if ((tank.allignment != rocket.allignment) && rocket.intersects(tank.position, tank.collision_radius))
                        {
                            // TODO: Should remove rocket from list
                            rocket.active = false;

                            mlock.lock();
                            explosions.push_back(Explosion(&explosion, tank.position));
                            mlock.unlock();

                            if (tank.hit(rocket_hit_value))
                            {
                                mlock.lock();
                                smokes.push_back(Smoke(smoke, tank.position - vec2(7, 24)));
                                mlock.unlock();
                                break;
                            }
                        }
                    }
                }

                // Still need to figure out the location of particle beams to make it work with uni_form grid.
                // However there are 4 particle beams and wont add a big performance decrease.
                if (tank.active) {
                    // Check for beam collision.
                    for (Particle_beam& particle_beam : particle_beams)
                    {
                        if (particle_beam.rectangle.intersects_circle(tank.get_position(), tank.get_collision_radius()))
                        {
                            if (tank.hit(particle_beam.damage))
                            {
                                mlock.lock();
                                smokes.push_back(Smoke(smoke, tank.position - vec2(0, 48)));
                                mlock.unlock();
                                break;
                            }
                        }
                    }
                }

            }
            }));
        start_at += count;

    }
    wait_and_clear();

    active_tanks.erase(std::remove_if(active_tanks.begin(), active_tanks.end(), [](const Tank tank) { return !tank.active; }), active_tanks.end());

    // Calculate convex hull.
    if (!rockets.empty())
    {
        forcefield_hull.clear();
        
        grahamScan(active_tanks, forcefield_hull);

        bool test = true;
        if (forcefield_hull.size() > 3) {
            for (size_t i = 0; i < forcefield_hull.size() - 2; i++) {
                if (orientation(forcefield_hull[i], forcefield_hull[i + 1], forcefield_hull[i + 2]) == 1) {
                    test = false;
                }
            }
        }
        if (!test) {
            forcefield_hull.clear();
            forcefield_hull.push_back({ 0.0f, 0.0f });
            forcefield_hull.push_back({ 2000.0f, 0.0f });
            forcefield_hull.push_back({ 2000.0f, 2000.0f });
            forcefield_hull.push_back({ 0.0f, 2000.0f });
        }

        /*// TODO: sort active tanks here.
        vec2 point_on_hull = (active_tanks[0]).position;

        //Find left most tank position
        // ====
        // Big-O analysis: O (N)
        // ====
        for (Tank& tank : active_tanks)
        {
            if (tank.position.x <= point_on_hull.x)
            {
                point_on_hull = tank.position;
            }
        }

        //Calculate convex hull for 'rocket barrier'
        // ====
        // Big-O analysis: O (N�)
        // ====
        for (auto tank : active_tanks)
        {
            forcefield_hull.push_back(point_on_hull);
            vec2 endpoint = (active_tanks[0]).position;

            for (auto tank : active_tanks)
            {
                if ((endpoint == point_on_hull) || left_of_line(point_on_hull, endpoint, tank.position))
                {
                    endpoint = tank.position;
                }

            }
            point_on_hull = endpoint;

            if (endpoint == forcefield_hull.at(0))
            {
                break;
            }
        }*/
    }

    // TODO: Check if it's more efficient to also delete rockets here.
    // Check if rocket is outside the convex hull.
    start_at = 0;
    if (rockets.size() < 10 * NUM_OF_THREADS) {
        for (Rocket& rocket : rockets) {
            if (rocket.active) {
                for (size_t i = 0; i < forcefield_hull.size(); i++)
                {
                    if (left_of_line(forcefield_hull.at(i), forcefield_hull.at((i + 1) % forcefield_hull.size()), rocket.position))
                    {
                        explosions.push_back(Explosion(&explosion, rocket.position));
                        rocket.active = false;
                        break;
                    }
                }
            }
        }
    }
    else {
        for (int count : split_sizes_tanks) {
            threads.push_back(pool->enqueue([&, start_at, count]() {
                for (int j = start_at; j < start_at + count; j++) {
                    Rocket& rocket = rockets.at(j);
                    if (rocket.active) {
                        for (size_t i = 0; i < forcefield_hull.size(); i++)
                        {
                            if (left_of_line(forcefield_hull.at(i), forcefield_hull.at((i + 1) % forcefield_hull.size()), rocket.position))
                            {
                                mlock.lock();
                                explosions.push_back(Explosion(&explosion, rocket.position));
                                mlock.unlock();
                                rocket.active = false;
                                break;
                            }
                        }
                    }
                }
                }));
        }
        wait_and_clear();
    }
    //Remove exploded rockets with remove erase idiom
    rockets.erase(std::remove_if(rockets.begin(), rockets.end(), [](const Rocket& rocket) { return !rocket.active; }), rockets.end());


    //Update explosion sprites and remove when done with remove erase idiom
    // ====
    // Big-O analysis: O (N)
    // ====
    for (Explosion& explosion : explosions)
    {
        explosion.tick();
    }
    explosions.erase(std::remove_if(explosions.begin(), explosions.end(), [](const Explosion& explosion) { return explosion.done(); }), explosions.end());

    //Update particle beams
    // ====
    // Big-O analysis: O (N)
    // ====
    for (Particle_beam& particle_beam : particle_beams)
    {
        particle_beam.tick(active_tanks);
    }

    //Update smoke plumes
    // ====
    // Big-O analysis: O (N)
    // ====
    for (Smoke& smoke : smokes)
    {
        smoke.tick();
    }
}

// -----------------------------------------------------------
// Update the game state:
// Move all objects
// Update sprite frames
// Collision detection
// Targeting etc..
// -----------------------------------------------------------
// ====
// Big-O analysis simple: O (N�)
// Big-O analysis complex: O (N + N� + N + N + N + N + N� + N� + N� + N� + N) Or O (6N + 5N�)
// ====
//void Game::update(float deltaTime)
//{
//    //Calculate the route to the destination for each tank using BFS
//    //Initializing routes here so it gets counted for performance..
//    // ====
//    // Big-O analysis: O (N)
//    // ====
//    if (frame_count == 0)
//    {
//        for (Tank& t : tanks)
//        {
//            t.set_route(background_terrain.get_route(t, t.target));
//        }
//    }
//
//    //Check tank collision and nudge tanks away from each other
//    // ====
//    // Big-O analysis: O (N�)
//    // ====
//    for (Tank& tank : tanks)
//    {
//        if (tank.active)
//        {
//            for (Tank& other_tank : tanks)
//            {
//                if (&tank == &other_tank || !other_tank.active) continue;
//
//                vec2 dir = tank.get_position() - other_tank.get_position();
//                float dir_squared_len = dir.sqr_length();
//
//                float col_squared_len = (tank.get_collision_radius() + other_tank.get_collision_radius());
//                col_squared_len *= col_squared_len;
//
//                if (dir_squared_len < col_squared_len)
//                {
//                    tank.push(dir.normalized(), 1.f);
//                }
//            }
//        }
//    }
//
//    //Update tanks
//    // ====
//    // Big-O analysis: O (N)
//    // ====
//    for (Tank& tank : tanks)
//    {
//        if (tank.active)
//        {
//            //Move tanks according to speed and nudges (see above) also reload
//            tank.tick(background_terrain);
//
//            //Shoot at closest target if reloaded
//            if (tank.rocket_reloaded())
//            {
//                Tank& target = find_closest_enemy(tank);
//
//                rockets.push_back(Rocket(tank.position, (target.get_position() - tank.position).normalized() * 3, rocket_radius, tank.allignment, ((tank.allignment == RED) ? &rocket_red : &rocket_blue)));
//
//                tank.reload_rocket();
//            }
//        }
//    }
//
//    //Update smoke plumes
//    // ====
//    // Big-O analysis: O (N)
//    // ====
//    for (Smoke& smoke : smokes)
//    {
//        smoke.tick();
//    }
//
//    //Calculate "forcefield" around active tanks
//    forcefield_hull.clear();
//
//    //Find first active tank (this loop is a bit disgusting, fix?)
//    // ====
//    // Big-O analysis: O (N)
//    // ====
//    int first_active = 0;
//    for (Tank& tank : tanks)
//    {
//        if (tank.active)
//        {
//            break;
//        }
//        first_active++;
//    }
//    vec2 point_on_hull = tanks.at(first_active).position;
//    //Find left most tank position
//    // ====
//    // Big-O analysis: O (N)
//    // ====
//    for (Tank& tank : tanks)
//    {
//        if (tank.active)
//        {
//            if (tank.position.x <= point_on_hull.x)
//            {
//                point_on_hull = tank.position;
//            }
//        }
//    }
//
//    //Calculate convex hull for 'rocket barrier'
//    // ====
//    // Big-O analysis: O (N�)
//    // ====
//    for (Tank& tank : tanks)
//    {
//        if (tank.active)
//        {
//            forcefield_hull.push_back(point_on_hull);
//            vec2 endpoint = tanks.at(first_active).position;
//
//            for (Tank& tank : tanks)
//            {
//                if (tank.active)
//                {
//                    if ((endpoint == point_on_hull) || left_of_line(point_on_hull, endpoint, tank.position))
//                    {
//                        endpoint = tank.position;
//                    }
//                }
//            }
//            point_on_hull = endpoint;
//
//            if (endpoint == forcefield_hull.at(0))
//            {
//                break;
//            }
//        }
//    }
//
//    //Update rockets
//    // ====
//    // Big-O analysis: O (N�)
//    // ====
//    for (Rocket& rocket : rockets)
//    {
//        rocket.tick();
//
//        //Check if rocket collides with enemy tank, spawn explosion, and if tank is destroyed spawn a smoke plume
//        for (Tank& tank : tanks)
//        {
//            if (tank.active && (tank.allignment != rocket.allignment) && rocket.intersects(tank.position, tank.collision_radius))
//            {
//                explosions.push_back(Explosion(&explosion, tank.position));
//
//                if (tank.hit(rocket_hit_value))
//                {
//                    smokes.push_back(Smoke(smoke, tank.position - vec2(7, 24)));
//                }
//
//                rocket.active = false;
//                break;
//            }
//        }
//    }
//
//    //Disable rockets if they collide with the "forcefield"
//    //Hint: A point to convex hull intersection test might be better here? :) (Disable if outside)
//    // ====
//    // Big-O analysis: O (N�)
//    // ====
//    for (Rocket& rocket : rockets)
//    {
//        if (rocket.active)
//        {
//            for (size_t i = 0; i < forcefield_hull.size(); i++)
//            {
//                if (circle_segment_intersect(forcefield_hull.at(i), forcefield_hull.at((i + 1) % forcefield_hull.size()), rocket.position, rocket.collision_radius))
//                {
//                    explosions.push_back(Explosion(&explosion, rocket.position));
//                    rocket.active = false;
//                }
//            }
//        }
//    }
//
//
//
//    //Remove exploded rockets with remove erase idiom
//    rockets.erase(std::remove_if(rockets.begin(), rockets.end(), [](const Rocket& rocket) { return !rocket.active; }), rockets.end());
//
//    //Update particle beams
//    // ====
//    // Big-O analysis: O (N�)
//    // ====
//    for (Particle_beam& particle_beam : particle_beams)
//    {
//        particle_beam.tick(tanks);
//
//        //Damage all tanks within the damage window of the beam (the window is an axis-aligned bounding box)
//        for (Tank& tank : tanks)
//        {
//            if (tank.active && particle_beam.rectangle.intersects_circle(tank.get_position(), tank.get_collision_radius()))
//            {
//                if (tank.hit(particle_beam.damage))
//                {
//                    smokes.push_back(Smoke(smoke, tank.position - vec2(0, 48)));
//                }
//            }
//        }
//    }
//
//    //Update explosion sprites and remove when done with remove erase idiom
//    // ====
//    // Big-O analysis: O (N)
//    // ====
//    for (Explosion& explosion : explosions)
//    {
//        explosion.tick();
//    }
//
//    explosions.erase(std::remove_if(explosions.begin(), explosions.end(), [](const Explosion& explosion) { return explosion.done(); }), explosions.end());
//}

// -----------------------------------------------------------
// Draw all sprites to the screen
// (It is not recommended to multi-thread this function)
// -----------------------------------------------------------
void Game::draw()
{

    int blue_count = 0;
    int red_count = 0;
    for (Tank tank : active_tanks) {
        if (tank.allignment == BLUE) {
            blue_count++;
        }
        else {
            red_count++;
        }
    }

    vector<int*> blue_tanks; blue_tanks.reserve(blue_count);
    vector<int*> red_tanks; red_tanks.reserve(red_count);
    int blue = 0;
    int red = 0;

    for (Tank tank : active_tanks) {
        if (tank.allignment == BLUE) {
            blue_tanks.push_back(&int(tank.health));
        }
        else {
            red_tanks.push_back(&int(tank.health));
        }
    }

    auto sort_blue = pool->enqueue([&]() {
        std::sort(blue_tanks.begin(), blue_tanks.end());
        });
    auto sort_red = pool->enqueue([&]() {
        std::sort(red_tanks.begin(), red_tanks.end());
        });

    // clear the graphics window
    screen->clear(0);

    //Draw background
    background_terrain.draw(screen);

    
    for (Tank tank : active_tanks) {
        tank.draw(screen);
    }

    for (Tank tank : inactive_tanks) {
        tank.draw(screen);
    }

    for (Rocket& rocket : rockets)
    {
        rocket.draw(screen);
    }

    for (Smoke& smoke : smokes)
    {
        smoke.draw(screen);
    }

    for (Particle_beam& particle_beam : particle_beams)
    {
        particle_beam.draw(screen);
    }

    for (Explosion& explosion : explosions)
    {
        explosion.draw(screen);
    }

    //Draw forcefield (mostly for debugging, its kinda ugly..)
    for (size_t i = 0; i < forcefield_hull.size(); i++)
    {
        vec2 line_start = forcefield_hull.at(i);
        vec2 line_end = forcefield_hull.at((i + 1) % forcefield_hull.size());
        line_start.x += HEALTHBAR_OFFSET;
        line_end.x += HEALTHBAR_OFFSET;
        screen->line(line_start, line_end, 0x0000ff);
    }

    sort_blue.wait();
    sort_red.wait();


    draw_health_bars(blue_tanks, 0, blue_count);
    draw_health_bars(red_tanks, 1, red_count);
}

// -----------------------------------------------------------
// Sort tanks by health value using insertion sort
// -----------------------------------------------------------
//void Tmpl8::Game::insertion_sort_tanks_health(const std::vector<int*> sorting_vector, int const begin_index, int const end_index)
//{
//    const int NUM_TANKS = end - begin;
//    sorted_tanks.reserve(NUM_TANKS);
//    sorted_tanks.emplace_back(&original.at(begin));
//
//    for (int i = begin + 1; i < (begin + NUM_TANKS); i++)
//    {
//        const Tank& current_tank = original.at(i);
//
//        for (int s = (int)sorted_tanks.size() - 1; s >= 0; s--)
//        {
//            const Tank* current_checking_tank = sorted_tanks.at(s);
//
//            if ((current_checking_tank.compare_health(current_tank) <= 0))
//            {
//                sorted_tanks.insert(1 + sorted_tanks.begin() + s, &current_tank);
//                break;
//            }
//
//            if (s == 0)
//            {
//                sorted_tanks.insert(sorted_tanks.begin(), &current_tank);
//                break;
//            }
//        }
//    }
//}

// -----------------------------------------------------------
// Draw the health bars based on the given tanks health values
// -----------------------------------------------------------
void Tmpl8::Game::draw_health_bars(const vector<int*> sorted_health, const int team, const int team_size)
{
    int health_bar_start_x = (team < 1) ? 0 : (SCRWIDTH - HEALTHBAR_OFFSET) - 1;
    int health_bar_end_x = (team < 1) ? health_bar_width : health_bar_start_x + health_bar_width - 1;

    for (int i = 0; i < SCRHEIGHT - 1; i++)
    {
        //Health bars are 1 pixel each
        int health_bar_start_y = i * 1;
        int health_bar_end_y = health_bar_start_y + 1;

        screen->bar(health_bar_start_x, health_bar_start_y, health_bar_end_x, health_bar_end_y, REDMASK);
    }

    //Draw the <SCRHEIGHT> least healthy tank health bars
    int draw_count = std::min(SCRHEIGHT, team_size);
    for (int i = 0; i < draw_count - 1; i++)
    {
        //Health bars are 1 pixel each
        int health_bar_start_y = i * 1;
        int health_bar_end_y = health_bar_start_y + 1;

        float health_fraction = (1 - ((double)*sorted_health.at(i) / (double)tank_max_health));

        if (team == 0) { screen->bar(health_bar_start_x + (int)((double)health_bar_width * health_fraction), health_bar_start_y, health_bar_end_x, health_bar_end_y, GREENMASK); }
        else { screen->bar(health_bar_start_x, health_bar_start_y, health_bar_end_x - (int)((double)health_bar_width * health_fraction), health_bar_end_y, GREENMASK); }
    }
}

// -----------------------------------------------------------
// When we reach max_frames print the duration and speedup multiplier
// Updating REF_PERFORMANCE at the top of this file with the value
// on your machine gives you an idea of the speedup your optimizations give
// -----------------------------------------------------------
void Tmpl8::Game::measure_performance()
{
    char buffer[128];
    if (frame_count >= max_frames)
    {
        if (!lock_update)
        {
            duration = perf_timer.elapsed();
            cout << "Duration was: " << duration << " (Replace REF_PERFORMANCE with this value)" << endl;
            lock_update = true;
        }

        frame_count--;
    }

    if (lock_update)
    {
        screen->bar(420 + HEALTHBAR_OFFSET, 170, 870 + HEALTHBAR_OFFSET, 430, 0x030000);
        int ms = (int)duration % 1000, sec = ((int)duration / 1000) % 60, min = ((int)duration / 60000);
        sprintf(buffer, "%02i:%02i:%03i", min, sec, ms);
        frame_count_font->centre(screen, buffer, 200);
        sprintf(buffer, "SPEEDUP: %4.1f", REF_PERFORMANCE / duration);
        frame_count_font->centre(screen, buffer, 340);
    }
}

// -----------------------------------------------------------
// Main application tick function
// -----------------------------------------------------------
void Game::tick(float deltaTime)
{
    if (!lock_update)
    {
        update(deltaTime);
    }
    draw();

    measure_performance();

    // print something in the graphics window
    //screen->Print("hello world", 2, 2, 0xffffff);

    // print something to the text window
    //cout << "This goes to the console window." << std::endl;

    //Print frame count
    frame_count++;
    string frame_count_string = "FRAME: " + std::to_string(frame_count);
    frame_count_font->print(screen, frame_count_string.c_str(), 350, 580);
}

// -----------------------------------------------------------
// Find orientation for three points in order
// -----------------------------------------------------------
int Game::orientation(vec2& a, vec2& b, vec2& c) {
    // Return 0 if points are on a line
    // Return 1 if points are in clockwise direction
    // Return 2 if points are in counterclockwise direction
    int val = (b.y - a.y) * (c.x - b.x) -
        (b.x - a.x) * (c.y - b.y);
    if (val == 0) return 0;
    return (val > 0) ? 1 : 2;
}

// -----------------------------------------------------------
// Optimalisation Modifications
// -----------------------------------------------------------
void Game::grahamScan(vector<Tank>& tankList, vector<vec2>& convex_hull) {
    // Create copy of active tank list
    // Get the vec2 with lowest y value and put it at the front as p0
    // Sort list by ascending polar angle, if equal put closest point first
    // Check if any points have the same polar angle, save index of close-by one
    // Make copy of sorted list, removing duplicate polar angle incices
    // Check if list is bigger than 3
    // Run through Graham Scan algorithm, continuously checking if resulting hull is convex, otherwise removing a value
    vector<vec2> sorted_list;
    for (auto tank : tankList) {
        sorted_list.push_back(tank.position);
    }
    float y_min = sorted_list[0].y, min_index = 0;
    for (size_t i = 1; i < sorted_list.size(); i++) {
        float y = sorted_list[i].y;
        float y_diff = fabs(y - y_min);
        if ((y < y_min) || (y_diff <= 0.00000001f && sorted_list[i].x < sorted_list[min_index].x)) {
            y_min = sorted_list[i].y, min_index = i;
        }
    }
    std::swap(sorted_list[0], sorted_list[min_index]);

    vec2 p0(sorted_list[0].x, sorted_list[0].y);
    std::sort(sorted_list.begin() + 1, sorted_list.end(),
        [&](vec2& a, vec2& b) -> bool
        {
            int o = orientation(p0, a, b);
    if (o == 0)
    {
        float dist_a = p0.distance_square(a);
        float dist_b = p0.distance_square(b);
        const float difference = fabs(dist_a - dist_b);
        if (difference >= 0.00000001f) return false;
        else return dist_a < dist_b;
    }
    else
    {
        return o == 2;
    }
        });

    vector<vec2> non_duplicate_sorted_list{ sorted_list.front() };
    //non_duplicate_sorted_list.push_back(sorted_list.front());
    for (size_t i = 1; i < sorted_list.size() - 1; i++) {
        if (orientation(p0, sorted_list[i], sorted_list[i + 1]) == 0) continue;
        else non_duplicate_sorted_list.push_back(sorted_list[i]);
    }
    non_duplicate_sorted_list.push_back(sorted_list.back());


    if (non_duplicate_sorted_list.size() < 3) return;

    convex_hull.insert(convex_hull.end(), non_duplicate_sorted_list.begin(), non_duplicate_sorted_list.begin() + 3);

    for (size_t i = 3; i < non_duplicate_sorted_list.size(); i++)
    {
        while (convex_hull.size() > 1 && orientation(convex_hull.rbegin()[1], convex_hull.back(), non_duplicate_sorted_list[i]) != 2) {
            convex_hull.pop_back();
        }
        convex_hull.push_back(non_duplicate_sorted_list[i]);
    }
}


