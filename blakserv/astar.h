// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
 * astar.h:  
 */

#ifndef _ASTAR_H
#define _ASTAR_H

#define ASTAR_GRID_SIZE 128
#define ASTAR_LIST_SIZE 128

typedef struct astarnode_struct
{
	int row, col;
	//MovementCost + HeuristicCost
	int score;
	//In order to calculate G (MovementCost), we need to take the G of its parent (the square where we came from) and to add 1 to it. Therefore, the G of each square will represent the total cost of the generated path from point A until the square).
	int movement_cost; 
	//To put it simply, we will use the “Manhattan distance method” (Also called “Manhattan length” or “city block distance”) that just counts the number of horizontal and vertical square remaining to reach point B without taking into account of any obstacles or differences of land.
	int heuristic_cost;
	//Next node on the path
	struct astarnode_struct *parent;

} astar_node;

typedef struct astarpath_struct
{
	int startrow;
	int startcol;
	int endrow;
	int endcol;
	object_node *origin;
	object_node *target;

	astar_node *grid[ASTAR_GRID_SIZE][ASTAR_GRID_SIZE];

	int numclosed; // how many we have
	astar_node *closed[ASTAR_LIST_SIZE]; //array of closed

	int numopen; //how many we have
	astar_node *open[ASTAR_LIST_SIZE]; //array of open

} astarpath;

void CreatePath(object_node * oFrom, object_node * oTo);

#endif /*#ifndef _ASTAR_H */
