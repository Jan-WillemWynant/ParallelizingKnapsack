/*
* This code contains my multithreaded approach to solving the 0/1 knapsack problem.
* It is the same branch-and-bound algorithm as the sequential version, but the exploration of the search tree is distributed across a number of threads.
*/

#include <cmath>
#include <stdlib.h>
#include <thread>
#include <mutex>
#include <vector>
#include "main.h"


using namespace std;

int max_nr_threads; //The maximum amount of threads we can create.
int nr_threads_created; //The number of threads already created.
thread** threads; // array of pointers to threads. Our threads will themselves create new threads sort of in a 'distributed' fashion. We make the array of threads a global variable so we don't have to pass a reference to it to each node.
bool** in_consideration; //This matrix[nrThreads][n] of booleans will for each thread denote which objects are still in consideration.
int** currently_examining; //This matrix[nrThreads][n] will store the elements which are currently in the knapsack examined in a certain thread.
int* objects_mt;  //array with the objects from 1 to n.
int* solution_mt; //array in which the final solution will be found. This global solution is updated each time a better one is found.
int nr_objects_mt; //number of objects taken in the sack in solution.
int value_mt;   //value of the solution, updated each time a better one is found.

mutex m; //lock to lock access to shared variables.
mutex thread_mgt; //lock to acces thread management variables


int compare_mt(const void* p1, const void* p2) { //comparison function used in sorting the objects by value/weight ratio
	double v_w1 = (double)c[*(int*)p1] / a[*(int*)p1];
	double v_w2 = (double)c[*(int*)p2] / a[*(int*)p2];
	if (v_w1 > v_w2) return -1;
	if (v_w1 < v_w2) return 1;
	else return 0;
}


void rank_objects_mt(void) { // We fill the objects array with the object IDs and sort them according to value/weight ratio.
	for (int i = 0; i < n; i++) {
		objects_mt[i] = i + 1;
	}
	qsort(objects_mt, n, sizeof(int), compare_mt);
}


int bound_mt(int threadID, int start_weight, int start_value, int nr_in_sack) {   //function to calculate a local feasible bound of a node.
	if (start_weight < 0) return -1; //node infeasible so return.
	int fs_bound = start_value;
	int w1 = start_weight;
	for (int i = 0; i < n; i++) { //calculating the local feasible bound. The local feasible bound is calculated by going over the ranked remaining objects in consideration and adding them as long as they fit it the knapsack.
		int obj = objects_mt[i];
		if (in_consideration[threadID][i]) {
			if (a[obj] <= w1) {
				fs_bound += c[obj];
				w1 -= a[obj];
				currently_examining[threadID][nr_in_sack++] = obj;
			}
		}
	}
	m.lock();
	if (fs_bound > value_mt) {  //if this feasible bound is higher than the current value, the feasible bound becomes the current global solution.
		value_mt = fs_bound;
		nr_objects_mt = nr_in_sack;
		for (int i = 0; i < nr_in_sack; i++) {
			solution_mt[i] = currently_examining[threadID][i];
		}
	}
	m.unlock();
	return fs_bound;
}


void knapsack_mt(int threadID, int weight, int nr_elements, int node_value) {  //Parameters threadID, weight,  nr_elements in sack in node, value of node with elements in sack until now.

	if (weight < 0) return; //infeasible so return
	int local_upper_bound = node_value; //we'll in each node calculate the local upper bound.
	int check_weight = weight;
	int index;
	bool found = false; //indicates whether a next fractional object to branch on, is found. If none found, it is a leaf node.
	int nr_to_add = nr_elements;
	int obj;

	for (int k = 0; k < n; k++) { //run through objects ranked by value/weight ratio to calculate the local upper bound and find a fractional object. False in in_consideration[threadId] indicates the object isn't in consideration anymore (already processed in a previous node of this thread).
		obj = objects_mt[k];
		if (in_consideration[threadID][k]) { //Check if the object is still in consideration.
			int m = a[obj];
			if (m <= check_weight) { //If the object can still fit in the knapsack, we add it to the provisional knapsack.
				local_upper_bound += c[obj];
				check_weight -= m;
				currently_examining[threadID][nr_to_add++] = obj;
			}
			else { //If there is an object which doesn't fully fit in the knapsack anymore, this is our fractional object which we will branch on.
				// We can then compute the local upper bound by still adding the fractional value of that object.
				local_upper_bound += ((double)check_weight / (double)m) * (double)c[obj]; 
				if (local_upper_bound <= value_mt) return;  // If the local upper bound is lower or equal to currently best found value, we won't find optimal solution continuing this branch -> return.
				index = k;  //the index of the fractional object.
				found = true; // A fractional bject was found -> not yet a leaf node.
				break;
			}
		}
	}
	if (!found) { // If this is the case, we've reached a leaf node and the value of the leaf node is equal to the local upper bound.
		int leaf_value = local_upper_bound;
		m.lock();
		if (leaf_value > value_mt) { //if the value of the leaf node is higher than current value, this leaf node becomes the global solution.
			nr_objects_mt = nr_to_add;
			for (int j = 0; j < nr_objects_mt; j++) {
				solution_mt[j] = currently_examining[threadID][j];
			}
			value_mt = leaf_value;
		}
		m.unlock();
	}
	else {
		//the fractional object is obj after the break from the loop through the ranked objects above.
		in_consideration[threadID][index] = false;	//we make a decision on the object in this node, so it shouldn't be considered in further nodes anymore.
		currently_examining[threadID][nr_elements] = obj;
		int bound_in = bound_mt(threadID, weight - a[obj], node_value + c[obj], nr_elements + 1); //we calculate the local feasible bound of the next node when the object is taken in the knapsack.
		int bound_out = bound_mt(threadID, weight, node_value, nr_elements);	 //the local feasible bound of the next node with the object left out of the knapsack.

		if (bound_in >= bound_out) { //of the children node, the node with the highest feasible bound is the one we'll pursue first.

			//If there is still room for a thread to be created, we will give the first branch to a thread to explore.
			//The second branch will be explored in this current thread.
			thread_mgt.lock();
			if (nr_threads_created < max_nr_threads) {
				int newThreadID = nr_threads_created++; //This is the only critical section. The thread insertion in the array uses different indices.
				thread_mgt.unlock();

				//Before calling the new thread to explore its part of the search three, we have to initialize a few variables for it.
				for (int i = 0; i < n; i++) {
					if (!in_consideration[threadID][i]) { //The thread we create will explore a deeper branch, so all objects we already made a decision on, shouldn't be in consideration anymore.
						in_consideration[newThreadID][i] = false;
					}
				}
				currently_examining[threadID][nr_elements] = obj;

				for (int i = 0; i < nr_elements+1; i++) { //We initialize the partial solution from which the newly created thread will explore the search tree.
					currently_examining[newThreadID][i] = currently_examining[threadID][i];
				}
				threads[newThreadID] = new thread(knapsack_mt, newThreadID, weight - a[obj], nr_elements + 1, node_value + c[obj]);
			}
			else { //If there is no room for a thread to be created, we explore both branches in this thread.
				thread_mgt.unlock();
				currently_examining[threadID][nr_elements] = obj;
				knapsack_mt(threadID, weight - a[obj], nr_elements + 1, node_value + c[obj]);
			}

			knapsack_mt(threadID, weight, nr_elements, node_value);

		}

		else {
			//Exactly same logic as above (case where we first explore the branch with the fractional object not included -> assign this branch to a newly created thread if possible).
			thread_mgt.lock();
			if (nr_threads_created < max_nr_threads) {
				int newThreadID = nr_threads_created++; //This is the only critical section. 
				thread_mgt.unlock();

				//Before calling the new thread to explore its part of the search three, we have to initialize a few variables for it.
				for (int i = 0; i < n; i++) {
					if (!in_consideration[threadID][i]) {
						in_consideration[newThreadID][i] = false;
					}
				}
				for (int i = 0; i < nr_elements; i++) {
					currently_examining[newThreadID][i] = currently_examining[threadID][i];
				}
				threads[newThreadID] = new thread(knapsack_mt, newThreadID, weight, nr_elements, node_value);
			}
			else {
				thread_mgt.unlock();
				knapsack_mt(threadID, weight, nr_elements, node_value);
			}

			currently_examining[threadID][nr_elements] = obj;
			knapsack_mt(threadID, weight - a[obj], nr_elements + 1, node_value + c[obj]);
		}

		in_consideration[threadID][index] = true;  //We'll be returning to a higher up node, so the object should be considered again.
	}

	return;
}


void multithreaded_knapsack(int nr_threads) {

	max_nr_threads = nr_threads;
	nr_threads_created = 0;
	objects_mt = new int[n];
	currently_examining = new int* [max_nr_threads]; //As we move through the search tree, we explore different configurations of the knapsack. This matrix stores the objects included in the sack in the currently examined node of a thread.
	in_consideration = new bool* [max_nr_threads]; //When a thread has made a branching decision on an object, it shouldn't consider this object anymore further down in the tree. This matrix stores this for each thread.
	for (int i = 0; i < max_nr_threads; i++) {
		currently_examining[i] = new int[n];
		in_consideration[i] = new bool[n];
		//We also initialize the values to true
		for (int j = 0; j < n; j++) {
			in_consideration[i][j] = true;
		}
	}
	solution_mt = new int[n];
	value_mt = 0;
	rank_objects_mt(); //We rank the objects according to the value/weight ratio.

	threads = new thread*[max_nr_threads];

	threads[0] = new thread(knapsack_mt, nr_threads_created++, b, 0, 0);

	// *** waiting for all threads to finish, in other words waiting for the whole search tree to be explored.
	for (int t = 0; t < max_nr_threads; t++) {
		threads[t]->join();
		delete threads[t];

		if (t == nr_threads_created - 1) { //This if condition is necessary for the case where less threads than the max_nr_threads are created (for example case with a very strong initial bound, where nearly all branches are immediately cut/returned from).
			break; //If all threads up until t have finished running, and the variable nr_threads_created is equal to t+1, all threads that were created, have finished running.
		}
	}

	for (int i = 0; i < nr_objects_mt; i++) { //From the solution array, we fill the final solution array x, where 1 at a certain index denotes that that object is included.
		x[solution_mt[i]] = 1;
	}

	delete[] solution_mt;
	delete[] objects_mt;
	delete[] threads;
	for (int i = 0; i < max_nr_threads; i++) {
		delete[] currently_examining[i];
		delete[] in_consideration[i];
	}
	delete[] currently_examining;
	delete[] in_consideration;

}