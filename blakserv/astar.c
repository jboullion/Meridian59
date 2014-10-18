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

//local prototypes
//node calculations
void CalculateMovementCost(astar_node * node, astar_path * path, bool diagonal);
void CalculateHeuristic(astar_node * node, astar_path * path);
void CalculateScore(astar_node * node, astar_path * path, bool diagonal);

//linked list accessors
void AddNodeToList(astar_node **head, astar_node *node);
void DebugPrintList(astar_node *head);



//Takes two objects, from and to (origin and target)
void CreatePath(int startrow, int startcol, int endrow, int endcol, int roomid)
{
	astar_node *startnode;
	astar_path *path;
	//message_node * GetCol;

	path = (astar_path *)AllocateMemory(MALLOC_ID_ASTAR,sizeof(astar_path)); //get some memories, because everyone loves memories

	path->room = GetRoomDataByID(roomid); //gets the roomdata we need to use CanMoveInRoom()
	path->startrow = startrow; //Initializing Stuff
	path->startcol = startcol;
	path->endrow = endrow;
	path->endcol = endcol;
	dprintf("startrow,col %d,%d endrow,col %d,%d, roomid %d ",startrow, startcol, endrow, endcol, roomid);
	
	/*Old method, kept for documentation purposes, we all should be aware of how to pass messages to kod from C and interpret the return values.
	the code: "& 0x0FFFFFFF" is stripping out the 1st 4 bits, which leads me to believe the int returned by SendBlakodMessage is infact a 
	ret_val struct, and should be typecasted.
	class_node *c;
	message_node *m;
	//get startrow, startcol
	//Need to send object GetRow/GetCol because objects don't always store their location.
	c = GetClassByID(oFrom->class_id); //get the oFrom Class to lookup properties with
	m = GetMessageByName(c->class_id,"GetRow",&c); //get the message we need
	path->startrow = SendBlakodMessage(oFrom->object_id,m->message_id,0,0) & 0x0FFFFFFF; //send the message to the object, account for flags
	m = GetMessageByName(c->class_id,"GetCol",&c); //get the message we need
	path->startcol = SendBlakodMessage(oFrom->object_id,m->message_id,0,0) & 0x0FFFFFFF; //send the message to the object, account for flags
	
	
	//endrow, endcol
	c = GetClassByID(oTo->class_id); //get the oTo Class to lookup properties with
	m = GetMessageByName(c->class_id,"GetRow",&c); //get the message we need
	path->endrow = SendBlakodMessage(oTo->object_id,m->message_id,0,0) & 0x0FFFFFFF; //send the message to the object, account for flags
	m = GetMessageByName(c->class_id,"GetCol",&c); //get the message we need
	path->endcol = SendBlakodMessage(oTo->object_id,m->message_id,0,0) & 0x0FFFFFFF; //send the message to the object, account for flags
	*/

	//Create the start node, add it to the open list

	startnode = (astar_node *)AllocateMemory(MALLOC_ID_ASTAR,sizeof(astar_node)); //get some more memories, because everyone loves more memories
	//Create the start node, add it to the open list
	startnode->row = path->startrow;
	startnode->col = path->startcol;
	CalculateScore(startnode,path,false);
	dprintf("startnode row: %d col: %d score: %d\n", startnode->row, startnode->col, startnode->score);
	AddNodeToList(&path->open,startnode);
	//DebugPrintList(path->open);
	//Scan the node

}

void AddNodeToList(astar_node **head, astar_node *node)
{
	astar_node *current = *head;
	if (*head == NULL) //first node
	{
		*head = node;
		return;
	}
	while ( current != NULL ) 
	{
		current = current->next;
	}
	current->next = node;

	return;
}

void DebugPrintList(astar_node *head)
{
	astar_node *current = head;
	while ( current != NULL ) 
	{
		dprintf("node: row %d,col %d,score %d,movecost %d,heuristic %d\n",
			current->row,current->col,current->score,current->movement_cost,current->heuristic_cost);
		current = current->next;
	}
}

void ScanNode(astar_node *startnode, astar_path * path)
{
	for (int rowoffset = -1; rowoffset < 2; rowoffset++) //loop -1, 0, +1
		for (int coloffset = -1; coloffset < 2; coloffset++) //loop -1, 0, +1
		{
			//if we can move from startnode->row/col to startnode->row/col+row/col offset
			if (CanMoveInRoom(path->room,startnode->row,startnode->col,startnode->row+rowoffset,startnode->col+coloffset))
			{
				//walkable node found, lets fill it out and add it to the open list
				dprintf("Walkable node found at row,col %d,%d\n",startnode->row+rowoffset,startnode->col+coloffset);
				astar_node * new_node = (astar_node *)AllocateMemory(MALLOC_ID_ASTAR,sizeof(astar_node));
				new_node->row = startnode->row+rowoffset;
				new_node->col = startnode->col+coloffset;
				new_node->parent = startnode;
				CalculateScore(new_node,path,(rowoffset!=0 && coloffset!=0)); // (rowoffset!=0 && coloffset!=0) == true when moving diagonal
				AddNodeToList(&path->open,new_node);
			}
		}
}


/***********************Start: Node Calculations***********************/
void CalculateMovementCost(astar_node * node, astar_path * path, bool diagonal)
{
	if (node->parent) //if we have a parent
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

void CalculateHeuristic(astar_node * node, astar_path * path)
{
	node->heuristic_cost = (abs(path->endrow - node->row) * 10) + (abs(path->endcol - node->col) * 10);
}

void CalculateScore(astar_node * node, astar_path * path, bool diagonal)
{
	CalculateMovementCost(node,path,diagonal);
	CalculateHeuristic(node,path);
	node->score = node->movement_cost + node->heuristic_cost;
}
/**********************************************/