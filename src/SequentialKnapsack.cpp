/*
* This code contains my sequential approach to solving the 0/1 knapsack problem. It is a branch-and-bound approach.
* The objects to choose from are ranked by the value/weight ratio. Objects are then added to the knapsack in the order of this ranking until an object is encountered which doesn't fully fit in the knapsack anymore.
* The algorithm then branches on the decision to include this fractional object or not, one branch exploring the case where this object is included in the knapsack and one branch exploring the other case.
* The ranking of objects by value/weight means that the algorithm first explores solutions filled with objects with a high value/weight ratio, since these are the most probable to lead to the optimal solution.
*/

#include <cmath>
#include <stdio.h>
#include <stdlib.h>
#include "main.h"

int* objects;  //array with the objects from 1 to n. Once the algorithm branches on an object, the value at its index becomes -1, to flag that it shouldn't be further considered.
int* solution; //array in which the final solution will be found. The solution is updated each time a better one is found.
int nr_objects; //number of objects taken in the sack in solution.
int value;   //value of the solution, updated each time a better one is found.


int compare(const void* p1, const void* p2) { //comparison function used in sorting the objects by value/weight ratio
	double v_w1 = (double)c[*(int*)p1] / a[*(int*)p1];
	double v_w2 = (double)c[*(int*)p2] / a[*(int*)p2];
	if (v_w1 > v_w2) return -1;
	if (v_w1 < v_w2) return 1;
	else return 0;
}


void rank_objects(void) { // We fill the objects array with the object IDs and sort them according to value/weight ratio.
	for (int i = 0; i < n; i++) {
		objects[i] = i + 1;
	}
	qsort(objects, n, sizeof(int), compare);
}


int bound(int start_weight, int start_value, int nr_in_sack, int* elements_until_now) {   //Function to calculate a local feasible bound of a node.
	if (start_weight < 0) return -1; //node infeasible so return.
	int fs_bound = start_value;
	int w1 = start_weight;
	for (int i = 0; i < n; i++) { //calculating the local feasible bound. The local feasible bound is calculated by going over the ranked remaining objects in consideration and adding them as long as they fit it the knapsack.
		int obj = objects[i];
		if (obj!=-1){
			if (a[obj]<=w1){
				fs_bound += c[obj];
				w1 -= a[obj];
				elements_until_now[nr_in_sack++] = obj;
			}
		}
	}
	if (fs_bound > value) {  //if this feasible bound is higher than the current value, the feasible bound becomes the current global solution.
		value = fs_bound;
		nr_objects = nr_in_sack;
		for (int i = 0; i < nr_in_sack; i++) {
			solution[i] = elements_until_now[i];
		}
	}
	return fs_bound;
}


void knapsack(int weight, int nr_elements, int node_value, int* elements_until_now) {  //Parameters weight,  nr_elements in sack in node, value of node with elements in sack until now, pointer to the array with the elements until now.

	if (weight < 0) return; //infeasible so return
	int local_upper_bound=node_value; //we'll in each node calculate the local upper bound.
	int check_weight = weight;
	int element;
	int index;
	bool found = false; //indicates whether a next fractional object to branch on, is found. If none found, it is a leaf node.
	int nr_to_add = nr_elements;
	int obj;
	 
	for (int k = 0; k < n; k++) { //run through objects ranked by value/weight ratio to calculate the local upper bound and find a fractional object. -1 in objects[] indicates the object isn't in consideration anymore (already processed in a previous node).
		obj = objects[k];
		if (obj != -1) { //Check if the object is still in consideration.
			int m = a[obj];
			if (m <= check_weight) {
				local_upper_bound += c[obj];
				check_weight -= m;
				elements_until_now[nr_to_add++] = obj;
			}
			else {
				local_upper_bound += ((double)check_weight / (double)m) * (double)c[obj];
				if (local_upper_bound <= value) return;  // If the local upper bound is lower or equal to value of current value, we won't find optimal solution continuing this branch -> return.
				index = k;  //the index of the fractional object.
				found = true;
				break;
			}
		}
	}
	if (!found) { //if this is the case, we've reached a leaf and the value of the leaf node is equal to the local upper bound.
		int leaf_value = local_upper_bound;
		if (leaf_value > value) { //if the value of the leaf node is higher than current value, this leaf node becomes the global solution.
			nr_objects = nr_to_add;
			for (int j = 0; j < nr_objects; j++) {
				solution[j] = elements_until_now[j];
			}
			value = leaf_value;
		}
	}
	else {
		element = objects[index];  //the fractional object
		objects[index] = -1;	//we make a decision on the object in this node, so it shouldn't be considered in further nodes anymore.
		elements_until_now[nr_elements] = element;
		int bound_in = bound(weight - a[element], node_value + c[element], nr_elements+1, elements_until_now); //we calculate the local feasible bound of the next node when the object is taken in the knapsack.
		int bound_out = bound(weight, node_value, nr_elements, elements_until_now);							   //the local feasible bound of the next node with the object left out of the knapsack.
		if (bound_in >= bound_out) {														   //the next node with the highest feasible bound is the one we'll pursue first.
			elements_until_now[nr_elements] = element; //We first explore the node with the object included. This statement is necessary to undo potential alteration by the bound function.
			knapsack(weight - a[element], nr_elements + 1, node_value + c[element], elements_until_now);
			knapsack(weight, nr_elements, node_value, elements_until_now);

		}
		else {
			knapsack(weight, nr_elements, node_value, elements_until_now); //We first explore the branch which does not include the object.
			elements_until_now[nr_elements] = element; //This statement is necessary to undo potential alteration of the partial solution at index nr_elements by the first branch.
			knapsack(weight - a[element], nr_elements + 1, node_value + c[element], elements_until_now);
		}
		objects[index] = element;  //we'll be returning to a higher up node, so the object should be considered again. Thus we undo the -1 in the objects array.
	}
	return;

}


void sequential_knapsack() {

	objects = new int[n]; //This will hold the IDs of the objects. Note that index 0 holds ID 1, and so on. 
	//When an object is branched on, it shouldn't be taken into account further down the search tree (the decision on that object has already been taken). We denote this by putting a -1 at its place in the objects array.
	solution = new int[n]; //This will hold the IDs of the objects which are included in our solution for the knapsack problem.
	value = 0;
	rank_objects(); //We rank the objects according to the value/weight ratio.

	int* elements_until_now = new int[n]; //As we move through the search tree, we explore different configurations of the knapsack. This array stores the objects included in the sack in the currently examined node.

	knapsack(b, 0, 0, elements_until_now);

	for (int i = 0; i < nr_objects; i++) { //From the solution array, we fill the final solution array x, where 1 at a certain index denotes that that object is included.
		x[solution[i]] = 1;
	}

	delete[] elements_until_now;
	delete[] solution;
	delete[] objects;
}