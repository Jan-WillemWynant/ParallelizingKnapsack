/*
* This main file reads the knapsack problem instances, fills the global variables which represent the instance data, and starts the testing process (time measurements etc.).
* A few knapsack instances are provided in the directory. A few smaller ones to quickly check the correctness of the code, and a few larger ones to inspect performance.
* The skeleton of this testing code was provided to me by professor Erik Demeulemeester of KU Leuven when I took his course Combinatorial Optimization in 2022.
*/

// hide unsafe warnings wrt deprecated input/output commands
#define _CRT_SECURE_NO_WARNINGS 1
#pragma warning(disable: 4996)

// memory leak detection
#define _CRTDBG_MAP_ALLOC

#include <iostream>
#include <crtdbg.h>

#ifdef _DEBUG
#define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif

#include <string.h>
#include <conio.h>
#include <time.h>
#include "main.h"

// These global variables are used to store the instance data: 
int n, b;  // n -> size of the problem, i.e nr of objects to choose from // b -> permissible weight
mat c, a, x; // c -> value of object i // a -> weight of object i 
// x will represent the solution found by the sequential algorithm: 1 if object i is included -> can be used as a reference for other versions later on.

//The input function reads the instance file, and fills the instance data variables.
void input (void) {
	int i; char filename[20]; FILE *fp;
	printf ("\n\n	Give the problem name: "); 
	scanf ("%s", filename); strcat (filename, ".knp");
	fp = fopen (filename, "r"); fscanf (fp, "%d %d", &n, &b);
	if (n > maxn) {printf ("Too many objects\n\n"); return;}
	for (i = 0; ++i <= n;) fscanf (fp, "%d %d", a+i, c+i);   //Note that the way the loop is performed, arrays a and c actually start at index 1. In this way, there is a direct link between the ID of the object and the index in the a and c arrays.
	fclose (fp); return;
}

//The output file reads the solution the algorithm gives, calculates the total weight used and total value of the solution and prints these.
void output (void) {
	int i, j, k;
	//printf ("\n	Objects:\n "); 
	for (i = j = k = 0; ++i <= n;) if (x[i]) 
	{	//printf ("%d, ", i); //Outcomment this is you want the objects included in the sack to be printed.
		j += a[i]; k += c[i]; break;
	}
	while (++i <= n) if (x[i])
	{	//printf ("%4d, ", i); //Outcomment this is you want the objects included in the sack to be printed.
		j += a[i]; k += c[i];
	} 
	printf ("\n	Weight = %d <= %d", j, b);
	printf ("\n	Value = %d", k);
	return;
}

//The main function calls the input function, then calls the (different versions of the) algorithm and measures the computation time. It prints the findings.
void main () {
	double elapsed_time; clock_t start_time;
	input ();

	printf("Sequential version:");

	elapsed_time = 0.0;
	for (int i = 0; i < 5; i++) {
		start_time = clock();
		sequential_knapsack();
		elapsed_time += (float)(clock() - start_time) / CLK_TCK;
		if (i==4) output();
		for (int s = 0; s <= n; s++) {
			x[s] = 0;
		}
	}
	double reference_time = elapsed_time / 5; //We take the average computation time of the 5 sequential runs as reference for the speed-up of the other versions.
	printf("\n	Average Computation time (5 runs): %.3f seconds\n", elapsed_time/5);
	//Before calling the multithreaded version, we reset the x array.
	//for (int i = 0; i <= n; i++) {
	//	x[i] = 0;
	//}

	int nr_threads_range = 17;
	//printf("\n\n	With how many threads would you like to test the multithreaded versions: ");
	//scanf("%d", &nr_threads); 

	printf("Multithreaded version:");
	
	for (int i = 1; i < nr_threads_range; i++) {
		printf("\n	Nr of threads %i", i);
		elapsed_time = 0.0;
		for (int r = 0; r < 5; r++) {
			start_time = clock();
			multithreaded_knapsack(i);
			elapsed_time += (float)(clock() - start_time) / CLK_TCK;
			if(r==4) output();
			//Before calling the multithreaded version v2, we reset the x array.
			for (int s = 0; s <= n; s++) {
				x[s] = 0;
			}
		}
		printf("\n	Average Computation time (5 runs): %.3f seconds\n", elapsed_time/5);
		printf("	Speed-up: %.3f \n", reference_time/(elapsed_time/5));
	}


	printf("Multithreaded version v2:");

	for (int i = 1; i < nr_threads_range; i++) {
		printf("\n	Nr of threads %i", i);
		elapsed_time = 0.0;
		for (int r = 0; r < 5; r++) {
			start_time = clock();
			multithreaded_knapsack_v2(i);
			elapsed_time += (float)(clock() - start_time) / CLK_TCK;
			if (r==4) output();
			//Before calling the multithreaded version v2, we reset the x array.
			for (int s = 0; s <= n; s++) {
				x[s] = 0;
			}
		}
		printf("\n	Average Computation time (5 runs): %.3f seconds\n", elapsed_time/5);
		printf("	Speed-up: %.3f \n", reference_time / (elapsed_time / 5));
	}
	

	_CrtDumpMemoryLeaks();
	getch (); return;
}