#include "precomp.h"
#include "uniform_grid.h"

vector<movable*> Tmpl8::uniform_grid::get_neighboring_objects(vec2 position)
{
	vec2 center_tile = get_tile_indexes(position);
	vector<movable*> neighboring_objects;
	for (int i = -1; i < 2; i++)
	{
		for (int j = -1; j < 2; j++)
		{
			neighboring_objects.insert(neighboring_objects.end(),
				grid[center_tile.x + i][center_tile.y + j].begin(),
				grid[center_tile.x + i][center_tile.y + j].end());
		}
	}
	return neighboring_objects;
}

void Tmpl8::uniform_grid::add_to_grid(movable* movable_object, vec2 position)
{
	vec2 tile = get_tile_indexes(position);
	grid[tile.x][tile.y].push_back(movable_object);
}

void Tmpl8::uniform_grid::update_grid(movable* movable_object, vec2 old_position, vec2 new_position)
{
	vec2 old_tile = get_tile_indexes(old_position);
	vec2 new_tile = get_tile_indexes(new_position);
	if (old_tile.x != new_tile.x || old_tile.y != new_tile.y)
	{
		grid[old_tile.x][old_tile.y].remove(movable_object);
		grid[new_tile.x][new_tile.y].push_back(movable_object);
	}
}

vec2 Tmpl8::uniform_grid::get_tile_indexes(vec2 position)
{
	// Calculate which indexes it is based on the position.
	// To do this we check for each positon what the smallest number of devidends it is.
	// Tile 
	// X: 566.6 Y: 30.89
	// Position / 16 -> round down.
	int index_x = std::floor(position.x / 16);
	int index_y = std::floor(position.y / 16);
	return vec2(index_x, index_y);
}
