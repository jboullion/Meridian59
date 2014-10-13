// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
 * astar.c
 * 
 
 Will create an AstarPath Kod object, calculate the path between the origin and target,
 and give that AstarPath Kod object a plPath list of the needed nodes to follow

 */

#include "blakserv.h"

//Takes two objects, from and to (origin and target)
void CreatePath(object_node * oFrom, object_node * oTo)
{
	astar_node *startnode;
	astarpath *path;
	class_node *c;
	int property_id;

	path = (astarpath *)AllocateMemory(MALLOC_ID_ASTAR,sizeof(astarpath));
	
	path->origin = oFrom;
	path->target = oTo;
	
	//get startrow, startcol
	c = GetClassByID(oFrom->class_id); //get the oFrom Class to lookup properties with
	property_id = GetPropertyIDByName(c,"piRow"); //get property id for Row
	path->startrow = oFrom->p[property_id].val.int_val; //set startrow from object properties
	property_id = GetPropertyIDByName(c,"piCol"); //get property id for Col
	path->startcol = oFrom->p[property_id].val.int_val; //set startcol from object properties
	
	//endrow, endcol
	c = GetClassByID(oTo->class_id); //get the oTo Class to lookup properties with
	property_id = GetPropertyIDByName(c,"piRow"); //get property id for Row
	path->endrow = oTo->p[property_id].val.int_val; //set endrow from object properties
	property_id = GetPropertyIDByName(c,"piCol"); //get property id for Col
	path->endcol = oTo->p[property_id].val.int_val; //set endcol from object properties



	//SendBlakodMessage(int object_id,int message_id,int num_parms,parm_node parms[])
	//SendBlakodMessage(oFrom->object_id,rowmessage,0,null)
	
	property_id = GetPropertyIDByName(c,"GetRow");

	//Create the start node, add it to the closed list
	startnode = (astar_node *)AllocateMemory(MALLOC_ID_ASTAR,sizeof(astar_node));
	startnode->row = path->startrow;
	startnode->col = path->startcol;
	CalculateScore(startnode,path);
	
	//Scan the node
}

void ScanNode(astar_node *node, astarpath * path)
{
	for (int xoff = -1; xoff < 2; xoff++) //loop -1, 0, +1
		for (int yoff = -1; yoff < 2; yoff++) //loop -1, 0, +1
		{
		}
}


/***********************Start: Node Calculations***********************/
void CalculateMovementCost(astar_node * node, astarpath * path, bool diagonal)
{
	if (!node->parent) //if we have a parent
	{
		if (diagonal)
			node->movement_cost = node->parent->movement_cost + 14;
		else
			node->movement_cost = node->parent->movement_cost + 10;
	}
	else
	{
		node->movement_cost = 0;
	}
}

void CalculateHeuristic(astar_node * node, astarpath * path)
{
	node->heuristic_cost = (abs(path->endrow - node->row) * 10) + (abs(path->endcol - node->col));
}

void CalculateScore(astar_node * node, astarpath * path, bool diagonal)
{
	CalculateMovementCost(node,path,diagonal);
	CalculateHeuristic(node,path);
	node->score = node->movement_cost + node->heuristic_cost;
}
/**********************************************/