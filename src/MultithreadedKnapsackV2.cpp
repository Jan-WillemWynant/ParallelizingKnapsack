/*
* This code contains a second multithreaded approach to solving the 0/1 knapsack problem.
* It is the same branch-and-bound algorithm as the sequential version, but the exploration of the search tree is distributed across a number of threads.
* In the first multithreaded approach, we saw that it did solve the large instance faster than the sequential version for the right number of threads (8-12).
* However, I still think improvement is definitely possible.
* My reasoning is the following: a lot of branches stop very early, since the upper bound of the branch will be lower than the already found best solution.
* So the overhead of creating a thread is not actually compensated by the work the thread does, since a lot of threads will just examine one or two nodes.
* Therefore, in this version, I will experiment with reusing threads. Once a thread has finished exploring a branch, it puts its status back to inactive,
* and in this way other threads know they can assign new work to this inactive thread.
*/

#include <cmath>
#include <stdlib.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include "main.h"


using namespace std;

int max_nr_threads_v2; //The maximum amount of threads we can create.
int nr_threads_active; //The number of threads currently active.
thread** threads_v2; // array of pointers to threads. We will create the threads in the beginning. Threads then activate other threads in a 'distributed' fashion.
bool** in_consideration_v2; //This matrix of booleans will for each thread denote which objects are still in consideration.
int** thread_starting_state; //This matrix of integers gives for each thread the starting state it has to run knapsack from: the weight, nr of elements in sack and starting value of sack.
int** currently_examining_v2; //This matrix[nrThreads][n] will store the elements which are currently in the knapsack examined in a certain thread.
bool* active; //This array will denote for each thread whether the thread is active or not. This is the predicate another thread has to set to true to wake up the waiting thread.
bool* ready; //This array will denote for each thread whether the initialization of the thread start state is ready or not.
int* objects_mt_v2;  //array with the objects from 1 to n.
int* solution_mt_v2; //array in which the final solution will be found. This global solution is updated each time a better one is found.
int nr_objects_mt_v2; //number of objects taken in the sack in solution.
int value_mt_v2;   //value of the solution, updated each time a better one is found.
bool finished; //Boolean which is set to true when all threads have finished their work, in other words when the whole search tree has been explored.

mutex m_v2; //lock to lock access to shared variables.
mutex ctr_lck; //lock to update the active threads counter.
mutex* threads_mgt; //locks linked to the condition variable of each thread.
condition_variable* cvs; //condition variables associated with each thread.


int compare_mt_v2(const void* p1, const void* p2) { //comparison function used in sorting the objects by value/weight ratio
	double v_w1 = (double)c[*(int*)p1] / a[*(int*)p1];
	double v_w2 = (double)c[*(int*)p2] / a[*(int*)p2];
	if (v_w1 > v_w2) return -1;
	if (v_w1 < v_w2) return 1;
	else return 0;
}


void rank_objects_mt_v2(void) { // We fill the objects array with the object IDs and sort them according to value/weight ratio.
	for (int i = 0; i < n; i++) {
		objects_mt_v2[i] = i + 1;
	}
	qsort(objects_mt_v2, n, sizeof(int), compare_mt_v2);
}

void knapsack_mt_v2(int threadID, int weight, int nr_elements, int start_value);

void worker(int threadID) {
	while(true){
		unique_lock<mutex> lk(threads_mgt[threadID]);
		while ((!active[threadID] || !ready[threadID]) && !finished) {
			cvs[threadID].wait(lk); //If the thread isn't active or ready, the thread waits until another thread activates us, readies its initialization and notifies it.
		}
		lk.unlock();
		if (finished) return; //If all threads non-active, return.
		knapsack_mt_v2(threadID, thread_starting_state[threadID][0], thread_starting_state[threadID][1], thread_starting_state[threadID][2]); //The thread starts from its assigned start node in the search tree.
		threads_mgt[threadID].lock();
		active[threadID] = false; //After finishing exploration, the thread sets itself to inactive and unready.
		ready[threadID] = false;
		threads_mgt[threadID].unlock();
		ctr_lck.lock(); //We decrease the number of active threads.
		nr_threads_active--;
		if (nr_threads_active == 0) { //If all threads are non-active, the threads should all exit.
			//If we get to this point, it means this is the last active thread and has just finished -> should cancel all other threads.
			finished = true;
			for (int i = 0; i < max_nr_threads_v2; i++) { //We notify the other threads that the process is finished.
				if (i != threadID) {
					threads_mgt[i].lock(); //This lock and unlock seems unnecessary, but it is to prevent the situation where another thread had done the check on finished before this thread set it to true,
					threads_mgt[i].unlock(); //and this thread notifies that thread before it has gone into waiting. This would lead to a failed termination process.
					cvs[i].notify_one();
				}
			}
			ctr_lck.unlock();
			return;
		}
		ctr_lck.unlock();
	}

}

int bound_mt_v2(int threadID, int start_weight, int start_value, int nr_in_sack) {   //function to calculate a local feasible bound of a node.
	if (start_weight < 0) return -1; //node infeasible so return.
	int fs_bound = start_value;
	int w1 = start_weight;
	for (int i = 0; i < n; i++) { //calculating the local feasible bound. The local feasible bound is calculated by going over the ranked remaining objects in consideration and adding them as long as they fit it the knapsack.
		int obj = objects_mt_v2[i];
		if (in_consideration_v2[threadID][i]) {
			if (a[obj] <= w1) {
				fs_bound += c[obj];
				w1 -= a[obj];
				currently_examining_v2[threadID][nr_in_sack++] = obj;
			}
		}
	}
	m_v2.lock();
	if (fs_bound > value_mt_v2) {  //if this feasible bound is higher than the current value, the feasible bound becomes the current global solution.
		value_mt_v2 = fs_bound;
		nr_objects_mt_v2 = nr_in_sack;
		for (int i = 0; i < nr_in_sack; i++) {
			solution_mt_v2[i] = currently_examining_v2[threadID][i];
		}
	}
	m_v2.unlock();
	return fs_bound;
}


void knapsack_mt_v2(int threadID, int weight, int nr_elements, int node_value) {  //Parameters threadID, weight,  nr_elements in sack in node, value of node with elements in sack until now (i.e. in this particular partial solution).

	if (weight < 0) return; //infeasible so return
	int local_upper_bound = node_value; //we'll in each node calculate the local upper bound.
	int check_weight = weight;
	int index;
	bool found = false; //indicates whether a next fractional object to branch on, is found. If none found, it is a leaf node.
	int nr_to_add = nr_elements;
	int obj;

	for (int k = 0; k < n; k++) { //run through objects ranked by value/weight ratio to calculate the local upper bound and find a fractional object. False in in_consideration[threadId] indicates the object isn't in consideration anymore (already processed in a previous node of this thread).
		obj = objects_mt_v2[k];
		if (in_consideration_v2[threadID][k]) { //Check if the object is still in consideration.
			int m = a[obj];
			if (m <= check_weight) { //If the object can still fit in the knapsack, we add it to the provisional knapsack.
				local_upper_bound += c[obj];
				check_weight -= m;
				currently_examining_v2[threadID][nr_to_add++] = obj;
			}
			else { //If there is an object which doesn't fully fit in the knapsack anymore, this is our fractional object which we will branch on.
				// We can then compute the local upper bound by still adding the fractional value of that object.
				local_upper_bound += ((double)check_weight / (double)m) * (double)c[obj];
				if (local_upper_bound <= value_mt_v2) return;  // If the local upper bound is lower or equal to currently best found value, we won't find optimal solution continuing this branch -> return.
				index = k;  //the index of the fractional object.
				found = true; // A fractional bject was found -> not yet a leaf node.
				break;
			}
		}
	}
	if (!found) { // If this is the case, we've reached a leaf node and the value of the leaf node is equal to the local upper bound.
		int leaf_value = local_upper_bound;
		m_v2.lock();
		if (leaf_value > value_mt_v2) { //if the value of the leaf node is higher than current value, this leaf node becomes the global solution.
			nr_objects_mt_v2 = nr_to_add;
			for (int j = 0; j < nr_objects_mt_v2; j++) {
				solution_mt_v2[j] = currently_examining_v2[threadID][j];
			}
			value_mt_v2 = leaf_value;
		}
		m_v2.unlock();
	}
	else {
		//the fractional object is obj after the break from the loop through the ranked objects above.
		in_consideration_v2[threadID][index] = false;	//we make a decision on the object in this node, so it shouldn't be considered in further nodes anymore.
		currently_examining_v2[threadID][nr_elements] = obj;
		int bound_in = bound_mt_v2(threadID, weight - a[obj], node_value + c[obj], nr_elements + 1); //we calculate the local feasible bound of the next node when the object is taken in the knapsack.
		int bound_out = bound_mt_v2(threadID, weight, node_value, nr_elements);	 //the local feasible bound of the next node with the object left out of the knapsack.

		if (bound_in >= bound_out) { //of the children node, the node with the highest feasible bound is the one we'll pursue first.

			//If there is still a currently inactive thread, we will give the first branch to the inactive thread to explore.
			//The second branch will be explored in this current thread.
			ctr_lck.lock();
			if (nr_threads_active < max_nr_threads_v2) { //Check if there still is an inactive thread.
				nr_threads_active++; //We will wake up an inactive thread, so we augment the active threads counter.
				ctr_lck.unlock();
				//Now that we have established that there still is an inactive thread, let's find it.
				int t;
				for (t = 0; t < max_nr_threads_v2; t++) {
					threads_mgt[t].lock();
					if (!active[t]) { //If we find an inactive thread, we will activate it, so we set its active flag to true. (By protecting this with a mutex, we avoid two different threads concurrently activating the same thread).
						active[t] = true;
						threads_mgt[t].unlock();
						break;
					}
					threads_mgt[t].unlock();
				}
				int newThreadID = t;

				//Before waking up the inactive thread to explore this branch, we have to initialize a few variables for it.
				for (int i = 0; i < n; i++) {
					in_consideration_v2[newThreadID][i] = in_consideration_v2[threadID][i]; //The starting node for the activated thread has to know which objects have already been branched on further up in the tree.
				}
				currently_examining_v2[threadID][nr_elements] = obj;
				for (int i = 0; i < nr_elements + 1; i++) {
					currently_examining_v2[newThreadID][i] = currently_examining_v2[threadID][i]; //We initialize the partial solution from which the activated thread should start.
				}
				//We give the branch where the object is included, to the activated thread. So we initialize it with the remaining weight, number of elements and value of the partial solution.
				thread_starting_state[newThreadID][0] = weight - a[obj];
				thread_starting_state[newThreadID][1] = nr_elements + 1;
				thread_starting_state[newThreadID][2] = node_value + c[obj];
				//Now we can safely say the activated thread is ready to start its exploration.
				threads_mgt[newThreadID].lock();
				ready[newThreadID] = true;
				threads_mgt[newThreadID].unlock();
				cvs[newThreadID].notify_one();
			}
			else { //If there is no room for a thread to be created, we explore both branches in this thread.
				ctr_lck.unlock();
				currently_examining_v2[threadID][nr_elements] = obj;
				knapsack_mt_v2(threadID, weight - a[obj], nr_elements + 1, node_value + c[obj]);
			}

			knapsack_mt_v2(threadID, weight, nr_elements, node_value);

		}

		else {
			//Exactly same logic as above (but now for the case where the first branch is the branch were the object is not included).
			ctr_lck.lock();
			if (nr_threads_active < max_nr_threads_v2) {
				nr_threads_active++; //We will wake up an inactive thread, so we augment the active threads counter.
				ctr_lck.unlock();
				//Now that we have established that there still is an inactive thread, let's find it.
				int t;
				for (t = 0; t < max_nr_threads_v2; t++) {
					threads_mgt[t].lock();
					if (!active[t]) { //If we find an inactive thread, we will activate it, so we set its active flag to true.
						active[t] = true;
						threads_mgt[t].unlock();
						break;
					}
					threads_mgt[t].unlock();
				}
				int newThreadID = t;

				//Before calling the new thread to explore its part of the search three, we have to initialize a few variables for it.
				for (int i = 0; i < n; i++) {
					in_consideration_v2[newThreadID][i] = in_consideration_v2[threadID][i];
				}
				for (int i = 0; i < nr_elements; i++) {
					currently_examining_v2[newThreadID][i] = currently_examining_v2[threadID][i];
				}
				thread_starting_state[newThreadID][0] = weight;
				thread_starting_state[newThreadID][1] = nr_elements;
				thread_starting_state[newThreadID][2] = node_value;
				threads_mgt[newThreadID].lock();
				ready[newThreadID] = true;
				threads_mgt[newThreadID].unlock();
				cvs[newThreadID].notify_one();
			}
			else {
				ctr_lck.unlock();
				knapsack_mt_v2(threadID, weight, nr_elements, node_value);
			}

			currently_examining_v2[threadID][nr_elements] = obj;
			knapsack_mt_v2(threadID, weight - a[obj], nr_elements + 1, node_value + c[obj]);
		}

		in_consideration_v2[threadID][index] = true;  //We'll be returning to a higher up node, so the object should be considered again.
	}

	return;
}



void multithreaded_knapsack_v2(int nr_threads) {

	finished = false;
	max_nr_threads_v2 = nr_threads;
	nr_threads_active = 0;
	objects_mt_v2 = new int[n];
	thread_starting_state = new int* [max_nr_threads_v2]; //This matrix will contain the information on the start node from which a thread has to start its exploration of the search tree each time the thread is activated.
	currently_examining_v2 = new int* [max_nr_threads_v2]; //As we move through the search tree, we explore different configurations of the knapsack. This matrix stores the objects included in the sack in the currently examined node of a thread.
	in_consideration_v2 = new bool* [max_nr_threads_v2];
	for (int i = 0; i < max_nr_threads_v2; i++) {
		thread_starting_state[i] = new int[3]; //For each thread, the starting state consists of the remaining weight, number of elements included in the partial solution and the value of that partial solution.
		currently_examining_v2[i] = new int[n];
		in_consideration_v2[i] = new bool[n];
		//We also initialize the values to true
		for (int j = 0; j < n; j++) {
			in_consideration_v2[i][j] = true;
		}
	}
	solution_mt_v2 = new int[n];
	value_mt_v2 = 0;
	rank_objects_mt_v2(); //We rank the objects according to the value/weight ratio.

	threads_mgt = new mutex[max_nr_threads_v2]; //Mutexes which protect the activation of each thread (linked to the condition variable associated with each thread).
	active = new bool[max_nr_threads_v2](); //Active values for each thread initialized to false.
	ready = new bool[max_nr_threads_v2](); //Ready values for each thread initialized to false.
	threads_v2 = new thread * [max_nr_threads_v2];
	cvs = new condition_variable[max_nr_threads_v2]; //Condition variables associated with each thread, these will receive a notification when another thread activates the corresponding thread.

	//We create the threads.
	for (int t = 0; t < max_nr_threads_v2; t++) {
		threads_v2[t] = new thread(worker, t);
	}

	//We initialize the starting state for the first thread. The first thread starts from the root node, so starts with the total weight and an empty partial solution (nr_elements 0 and node_value 0).
	thread_starting_state[0][0] = b; 
	thread_starting_state[0][1] = 0;
	thread_starting_state[0][2] = 0;

	//We activate the first thread.
	nr_threads_active++;
	threads_mgt[0].lock();
	active[0] = true;
	ready[0] = true;
	threads_mgt[0].unlock();
	cvs[0].notify_one();


	// *** waiting for all threads to finish, in other words waiting for the whole search tree to be explored.
	for (int t = 0; t < max_nr_threads_v2; t++) {
		threads_v2[t]->join();
		delete threads_v2[t];

	}

	for (int i = 0; i < nr_objects_mt_v2; i++) { //From the solution array, we fill the final solution array x, where 1 at a certain index denotes that that object is included.
		x[solution_mt_v2[i]] = 1;
	}


	delete[] solution_mt_v2;
	delete[] objects_mt_v2;
	delete[] threads_v2;
	for (int i = 0; i < max_nr_threads_v2; i++) {
		delete[] in_consideration_v2[i];
		delete[] currently_examining_v2[i];
		delete[] thread_starting_state[i];
	}
	delete[] in_consideration_v2;
	delete[] currently_examining_v2;
	delete[] thread_starting_state;
	delete[] active;
	delete[] ready;
	delete[] threads_mgt;
	delete[] cvs;

}