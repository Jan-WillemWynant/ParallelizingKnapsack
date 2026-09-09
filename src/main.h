#pragma once

#define maxn 100000  //This defines the maximum size (i.e. number of objects) we will consider in our test instances.
typedef int mat[maxn + 1]; 

// These global variables are used to store the instance data: 
extern int n, b; // n -> size of the problem, i.e nr of objects to choose from // b -> permissible weight
extern mat c, a, x; // c -> vaue of object at index i // a -> weight of object at index i 
// x will represent the solution found by the sequential algorithm: 1 if object at index i is included -> can be used as a reference for other versions later on.

void sequential_knapsack(void);

void multithreaded_knapsack(int nr_threads);

void multithreaded_knapsack_v2(int nr_threads);


