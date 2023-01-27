#pragma once
#include "precomp.h"
#include "movable.h"
// Notes Uniform Grid:
// To have a grid where we can save in what tile each tank is then we can retrieve all neighboring tiles.
// We need to have a tile save an general object from which we can parse an class.
// We need to have a grid class to save the grid, get tiles and save a two dimensional array of tiles.
// By using a two dimensional array for each column and row we can retrieve the neighboring tiles easily by
// adding -1, 0 and 1 to the column and row to get the surrounding neighbors.
// Terrain: 45 x 80 tiles
// Screen: 1280 x 720
// Need:
//      Generalisation class for movable objects
//          Type to be able to easily parse the object
//      Grid class to build, manage and save the grid.
//          List of tiles
//          Get tile based on position
//          Get moveables from all neighbors excluding the current object based on tile / position.
//          Update grid based on old and new position to see if a object needs to be moved from a list.
//
// Working:
//		Initialize grid
//		Add frame 0, add all thanks to the grid.
//		At every other frame update the grid during movement.
//		Use Tmpl8::uniform_grid::get_neighboring_objects() to retrieve objects.
namespace Tmpl8
{
	class uniform_grid
	{
	public:
		vector<movable*> get_neighboring_objects(vec2 position);
		void add_to_grid(movable* movable_object, vec2 position);
		void update_grid(movable* movable_object, vec2 old_position, vec2 new_position);
		mutex* mlock;

	private:
		std::array<std::array<std::list<movable*>, 45>, 80> grid;
		static const int screen_width = 1280;
		static const int screen_height = 720;
		vec2 get_tile_indices(vec2 position);
	};
}

