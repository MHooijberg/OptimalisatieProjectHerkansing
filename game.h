#pragma once

namespace Tmpl8
{
    //forward declarations
    class Tank;
    class Rocket;
    class Smoke;
    class Particle_beam;

    class Game
    {
    public:
        void set_target(Surface* surface) { screen = surface; }
        void init();
        void shutdown();
        void update(float deltaTime);
        void draw();
        void tick(float deltaTime);
        int orientation(vec2& a, vec2& b, vec2& c);
        void grahamScan(vector<Tank>& tankList, vector<vec2>& convex_hull);
        void insertion_sort_tanks_health(const std::vector<int*> sorting_array, int const begin_index, int const end_index);
        void merge(vector<int*>& left, vector<int*>& right, vector<int*>& tanks_health);
        void merge_sort(vector<int*>& tanks_health);
        void draw_health_bars(const std::vector<int*> sorted_health, const int team, const int team_size);
        void measure_performance();

        Tank& find_closest_enemy(Tank& current_tank);

        void mouse_up(int button)
        { /* implement if you want to detect mouse button presses */
        }

        void mouse_down(int button)
        { /* implement if you want to detect mouse button presses */
        }

        void mouse_move(int x, int y)
        { /* implement if you want to detect mouse movement */
        }

        void key_up(int key)
        { /* implement if you want to handle keys */
        }

        void key_down(int key)
        { /* implement if you want to handle keys */
        }

    private:
        Surface* screen;

        vector<Tank> active_tanks;
        vector<Tank> inactive_tanks;
        vector<Rocket> rockets;
        vector<Smoke> smokes;
        vector<Explosion> explosions;
        vector<Particle_beam> particle_beams;

        Terrain background_terrain;
        std::vector<vec2> forcefield_hull;

        Font* frame_count_font;
        long long frame_count = 0;

        bool lock_update = false;

        //Checks if a point lies on the left of an arbitrary angled line
        bool left_of_line(vec2 line_start, vec2 line_end, vec2 point);
    };

}; // namespace Tmpl8

vector<int> split_evenly(int size, int split_size);

void wait_and_clear(vector<future<void>>& threads);
