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
void CalculateMovementCost(astar_node *node, astar_path *path, bool diagonal);
void CalculateHeuristic(astar_node *node, astar_path *path);
void CalculateScore(astar_node *node, astar_path *path, bool diagonal);

//linked list accessors
void DebugPrintList(astar_node *head); //print the list
void InsertNodeToList(astar_node **head, astar_node *node); //keep the list insertion sorted as you create it
void RemoveNodeFromList(astar_node **head, astar_node *node); //remove this node from this list
bool IsNodeOnList(astar_node *head, astar_node *node);
void PushNodeToList(astar_node **head, astar_node *node);

//grid stuff
void CreateGrid(astar_path *path);
void DisplayGrid(astar_path *path);
void FreeGrid(astar_path *path);

bool astardebug = false;

//pathfinding
void ScanNode(astar_node *startnode, astar_path *path);

// Creates a path
int CreatePath(int roomid, int startrow, int startcol, int endrow, int endcol,
               int startrow_fine, int startcol_fine, int endrow_fine, int endcol_fine,
			   int unmoveable_list)
{
	int rows, cols, idxstart, idxend;
	int from_row_comb, from_col_comb, to_row_comb, to_col_comb;
	astar_path path;
	astar_node *startnode, *endnode;

    // build a combined value in fine precision first
	// a row has 64 fine rows, a col has 64 fine cols
	// so a square has 4096 fine squares. 
	// << 6 (LSHIFT 6) is a faster variant of (*64)
	from_row_comb = (startrow << 6) + startrow_fine;
	from_col_comb = (startcol << 6) + startcol_fine;
	to_row_comb = (endrow << 6) + endrow_fine;
	to_col_comb = (endcol << 6) + endcol_fine;

	// scale to the highprecision scale
	// a row has 4 highprecision rows, a col has 4 highprecision cols.
	// so highres grid precision is NOT as good as fine precision!
	// a highres row still has 16 finerows.
	// >> 4 (RSHIFT 4) is faster variant of (/16)
	from_row_comb = from_row_comb >> 4;
	from_col_comb = from_col_comb >> 4;
	to_row_comb = to_row_comb >> 4;
	to_col_comb = to_col_comb >> 4;

	//gets the roomdata we need to use CanMoveInRoomHighRes()
	path.room = GetRoomDataByID(roomid);
	path.startrow = from_row_comb;
	path.startcol = from_col_comb;
	path.endrow = to_row_comb;
	path.endcol = to_col_comb;
	path.path_list_id = 0;
	path.open = NULL;
	path.closed = NULL;
	CreateGrid(&path); //creates our 2d grid of rows

	rows = path.room->file_info.rowshighres;
	cols = path.room->file_info.colshighres;
	
	// rows and cols are 0-based (done in ccode.c caller)
	idxstart = from_row_comb * cols + from_col_comb;
	idxend = to_row_comb * cols + to_col_comb;

	startnode = &path.grid[idxstart];
	endnode = &path.grid[idxend];

	//Calculate its values
	CalculateScore(startnode, &path, false);
	//Add it to the open list
	InsertNodeToList(&path.open, startnode);

	//Calculate scores and parents for all our nodes
	while (!IsNodeOnList(path.closed, endnode))
	{
		//the lowest score node should always be the first item on the open list
		astar_node *lowestscorenode = path.open;
		if (lowestscorenode == NULL) //If the open list is empty the path is impossible.
		{
			//so free the memory
			FreeGrid(&path);
			//return NIL which signals Kod code to fall back to old style movement
			return NIL;
		}
		//remove it from the open list
		RemoveNodeFromList(&path.open, lowestscorenode);
		//add it to the closed list
		InsertNodeToList(&path.closed, lowestscorenode);
		//scan it
		ScanNode(lowestscorenode, &path);
	}

	if (astardebug)
		DisplayGrid(&path);

	//build our path by following the endnode's parents to the startnode
	while (endnode != NULL)
	{
		if (endnode->parent == NULL) //we dont add the last item to the list
		{
			break;
		}
		int coordinate_list;
		//Create a list [ [ row, col ], [ row, col ], etc...]
		val_type first_val,rest_val;
		first_val.v.tag = TAG_INT;
		first_val.v.data = endnode->col;
		rest_val.v.tag = TAG_NIL;
		coordinate_list = Cons(first_val,rest_val); // [ col ]

		first_val.v.tag = TAG_INT;
		first_val.v.data = endnode->row;
		rest_val.v.tag = TAG_LIST;
		rest_val.v.data = coordinate_list;
		coordinate_list = Cons(first_val,rest_val); // [ row, col ]

		first_val.v.tag = TAG_LIST;
		first_val.v.data = coordinate_list;

		if (path.path_list_id == 0)
		{	
			rest_val.v.tag = TAG_NIL; // if we dont yet have a superlist, create one
		}
		else
		{
			rest_val.v.tag = TAG_LIST;
			rest_val.v.data = path.path_list_id; // or append to it if we do
		}
		path.path_list_id = Cons(first_val,rest_val); // add [row, col] to the superlist of coordinate pairs
		

		endnode = endnode->parent;
	}
	//so kod can interpret the list, tag it before returning it
	val_type return_val;
	return_val.v.tag = TAG_LIST;
	return_val.v.data = path.path_list_id;

	//Finally, clean up our grid, our path object
	FreeGrid(&path);

	return return_val.int_val;
}

void CreateGrid(astar_path *path)
{
	int rows = path->room->file_info.rowshighres;
	int cols = path->room->file_info.colshighres;
	int size = rows * cols * sizeof(astar_node);
	int idx = 0;
	
	// allocate
	path->grid = (astar_node *)AllocateMemory(MALLOC_ID_ASTAR, size);

	// initialize
	for (int row = 0; row < rows; row++)
	{
		for (int col = 0 ; col < cols; col++)
		{
			idx = row * cols + col;

			path->grid[idx].row = row;
			path->grid[idx].col = col;
			path->grid[idx].parent = NULL;
			path->grid[idx].score = 0;
			path->grid[idx].movement_cost = 0;
			path->grid[idx].heuristic_cost = 0;
		}
	}
}

void DisplayGrid(astar_path *path)
{
	int rows = path->room->file_info.rowshighres;
	int cols = path->room->file_info.colshighres;
	int idx = 0;

	char *rowstring = (char *)AllocateMemory(MALLOC_ID_ASTAR,10000);
	dprintf("start: %d,%d end: %d,%d",path->startrow,path->startcol,path->endrow,path->endcol);
	for (int row = 0; row < rows; row++)
	{
		sprintf(rowstring,"Row %3d- ",row);
		for (int col = 0 ; col < cols; col++)
		{
			idx = row * cols + col;

			if (path->grid[idx].parent != NULL)
				sprintf(rowstring,"%s|F=%3d;P=%3d,%3d|",rowstring,path->grid[idx].score,path->grid[idx].parent->row,path->grid[idx].parent->col);
			else
				sprintf(rowstring,"%s|F=%3d;P=NULL   |",rowstring,path->grid[idx].score);
		}
		dprintf(rowstring);
	}
	FreeMemory(MALLOC_ID_ASTAR,rowstring,10000);
}

void FreeGrid(astar_path *path)
{
	int rows = path->room->file_info.rowshighres;
	int cols = path->room->file_info.colshighres;
	int size = rows * cols * sizeof(astar_node);

	FreeMemory(MALLOC_ID_ASTAR, path->grid, size);			
}

void ScanNode(astar_node *startnode, astar_path *path)
{
	int idx, row, col;
	astar_node *currentnode;
	int rows = path->room->file_info.rowshighres;
	int cols = path->room->file_info.colshighres;
	int bigrow_start,bigcol_start,finerow_start,finecol_start;
	int bigrow_end,bigcol_end,finerow_end,finecol_end;

	if (startnode->row >= rows ||
		startnode->col >= cols ||
		startnode->row < 0 ||
		startnode->col < 0)
	{
		dprintf("startnode Row or Col outside bounds in ScanNode()");
		return;
	}

	for (int rowoffset = -1; rowoffset < 2; rowoffset++) //loop -1, 0, +1
	{
		for (int coloffset = -1; coloffset < 2; coloffset++) //loop -1, 0, +1
		{
			row = startnode->row + rowoffset;
			col = startnode->col + coloffset;

			//if the current node is outside the map boundary, skip it
			if (row >= rows ||
				col >= cols ||
				row < 0 ||
				col < 0)
			{
				continue;
			}
			
			idx = row * cols + col;
			
			//grab our found node from the grid
			currentnode = &path->grid[idx];
			
			//if the current node is null, skip.
			if (currentnode == NULL)
				continue;
			//if currentnode == startnode, skip it
			if (currentnode == startnode)
				continue;
			//if the node is on the closed list, skip it
			if (IsNodeOnList(path->closed,currentnode))
				continue;
			
			// build big/fine coordinates for highres-gridsquare
			bigrow_start = (startnode->row * 16) / 64;
			bigcol_start = (startnode->col * 16) / 64;
			finerow_start = (startnode->row * 16) % 64;
			finecol_start = (startnode->row * 16) % 64;			
			bigrow_end = (currentnode->row * 16) / 64;
			bigcol_end = (currentnode->col * 16) / 64;
			finerow_end = (currentnode->row * 16) % 64;
			finecol_end = (currentnode->row * 16) % 64;

			//if we can move from startnode->row/col to currentnode->row/col
			//CanMoveInRoomHighRes  (roomdata_node *r,int from_row  , int from_col  , int from_finerow, int from_finecol, int to_row,int to_col,int to_finerow, int to_finecol)
			if (CanMoveInRoomHighRes(path->room,bigrow_start,bigcol_start,finerow_start,finecol_start,bigrow_end,bigcol_end,finerow_end,finecol_end))
			{
				//if the node is not on the open list
				if (!IsNodeOnList(path->open,currentnode))
				{
					//set its parent
					currentnode->parent = startnode;
					//calculate its score
					CalculateScore(currentnode,path,(rowoffset !=0 && coloffset !=0)); // (rowoffset!=0 && coloffset!=0) == true when moving diagonal
					//add it to the open list
					InsertNodeToList(&path->open,currentnode);
				}
				//if the node is alread on the open list see if the new parent is better than the old parent
				else
				{
					if (startnode->parent != NULL)
					{
						//store the movement cost if we switched our parent to this one
						int newmovementcost;
						//if we are moving diagonally
						if (rowoffset !=0 && coloffset !=0) 
							newmovementcost = startnode->movement_cost + 14;
						else
							newmovementcost = startnode->movement_cost + 10;
						//if new parent movement cost is lower, set it.
						if (currentnode->parent !=NULL)
						{
							if (newmovementcost < currentnode->parent->movement_cost)
							{
								currentnode->parent = startnode;
								CalculateScore(currentnode,path,(rowoffset !=0 && coloffset !=0));
								//remove and re-add it to the open list, to keep the list sorted
								RemoveNodeFromList(&path->open,currentnode);
								InsertNodeToList(&path->open,currentnode);
							}
						}
						else
						{
							dprintf("currentnode had NULL parent");
							currentnode->parent = startnode;
							CalculateScore(currentnode,path,(rowoffset !=0 && coloffset !=0));
							//remove and re-add it to the open list, to keep the list sorted
							RemoveNodeFromList(&path->open,currentnode);
							InsertNodeToList(&path->open,currentnode);
						}
					}
					else
					{
						dprintf("Parent is null?");
					}
				}
			}
		}
	}
}

/***********************Start: Node Calculations********************/
void CalculateMovementCost(astar_node *node, astar_path *path, bool diagonal)
{
	if (node->parent != NULL) //if we have a parent
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

void CalculateHeuristic(astar_node *node, astar_path *path)
{
	node->heuristic_cost = (abs(path->endrow - node->row) * 10) + (abs(path->endcol - node->col) * 10);
}

void CalculateScore(astar_node *node, astar_path *path, bool diagonal)
{
	CalculateMovementCost(node,path,diagonal);
	CalculateHeuristic(node,path);
	node->score = node->movement_cost + node->heuristic_cost;
}

/*******************************************************************/
/***********************Start: List Functions***********************/
//Adds the node at the beginning of the list
void PushNodeToList(astar_node **head, astar_node *node)
{
	node->next=*head;
	*head = node;
}

//Insert into list where node->score fits in ascending order
void InsertNodeToList(astar_node **head, astar_node *node)
{
	for ( ; *head; head = &(*head)->next)
	{
		if ((*head)->score > node->score)
			break;
	}
	node->next = *head;
	*head = node;
}

//Remove a node from the list
void RemoveNodeFromList(astar_node **head, astar_node *node)
{
	astar_node *current = NULL,
			   *previous = NULL;
	for (current = *head; current != NULL; previous = current, current = current->next)
	{
		if (current == node)
		{
			if (previous == NULL)
			{
				*head = current->next;
			}
			else
			{
				previous->next = current->next;
			}
			return;
		}
	}
}

//Check if a node is on a list
bool IsNodeOnList(astar_node *head, astar_node *node)
{
	astar_node *current;
	for (current = head; current != NULL; current = current->next)
	{
		if (current == node)
			return true;
	}
	return false;
}

void DebugPrintList(astar_node *head)
{
	astar_node *current = head;
	while (current != NULL) 
	{
		dprintf("node: row %d,col %d,movecost %d,heuristic %d,score %d\n",
			current->row,current->col,current->movement_cost,current->heuristic_cost,current->score);
		current = current->next;
	}
}
/*******************************************************************/