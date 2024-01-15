
/*
13
Speed: 605 mm/s
Stopping Distance: 88.6 mm

9
Speed: 413 mm/s
Stopping Distance: 57.7 mm

5
Speed: 220 mm/s
Stopping Distance: 28 mm
*/

#ifndef _track_h_
#define _track_h_ 1
// The track initialization functions expect an array of this size.
#define TRACK_MAX 144
#define TRAIN_LEN 170

#include "train.h"
#include <stdbool.h>

typedef enum {
  NODE_NONE,
  NODE_SENSOR,
  NODE_BRANCH,
  NODE_MERGE,
  NODE_ENTER,
  NODE_EXIT,
} node_type;

#define DIR_AHEAD 0
#define DIR_STRAIGHT 0
#define DIR_CURVED 1

struct track_node;
typedef struct track_node track_node;
typedef struct track_edge track_edge;

struct track_edge {
  track_edge *reverse; // dual of the edge
  track_node *src, *dest; // from and to
  int dist;             /* in millimetres */
};

struct track_node {
  const char *name;
  node_type type;
  int id;
  int num;              /* sensor or switch number */
  track_node *reverse;  /* same location, but opposite direction */
  track_edge edge[2];
  // fields for path planning
  int inUse;
  int isCurve;
  int isVisited;
};

typedef struct next_step next_step;

struct next_step{
    bool isSensor;
    int next_node_id;
    const char* next_node_name;
    int next_sensor_id; //idx
    const char* next_sensor_name;
    int switchID[6];
    bool switch_dir[6]; // 0 for straight and 1 for curved
    const char* switch_name[6];
    int switch_count;
    int dist_to_sensor;
    bool needs_reverse; // If this step is a reverse step
    bool hard_reverse; // If we can't find a sensor to reverse on
};

inline bool isOccupied(track_node* n){
    return (n->inUse || n->reverse->inUse);
}

bool bookSegment(track_node* track, int trainID, int nextNodeId);
void UnbookSegment(track_node* track, int trainID, int nextNodeId);

bool Deadlocked(int console_server_id);

void init_tracka(track_node *track);
void init_trackb(track_node *track);

void printHeap(int* heap, int *priority, int *heapsize);

int heapPop(int *heap, int *priority, int *heapsize);

void heapInsert(int *heap, int *priority, int *heapsize, int insertionVal, int insertionPri);

void printBookings(track_node* track, int trackAB, int consoleID);

int trackNameToIdx(const char *name, track_node* track);

const char* trackIdxToName(int idx, track_node* track);

int Dijkstra_User(int source, int dest, track_node* track, next_step sequence[TRACK_MAX], int trainID, int console_server_id, bool* initialReverse);

int Dijkstra(int source, track_node* track, next_step sequence[TRACK_MAX], int seqIdx, int trainID, int console_server_id, bool* ListofSensor, bool* initialReverse);
void printPath(next_step* steps, int step_size, int trainID, int console_server_id);

// Returns 0 if a path is found, -1 if not
// Should always return 0 if 1 train
//int Dijkstra(int source, int dest, track_node* track);


/*
 * vector<int> uniform_cost_search(vector<int> goal, int start)
{
    // while the queue is not empty
    while (queue.size() > 0) {
 
        // get the top element of the 
        // priority queue
        pair<int, int> p = queue.top();
 
        // pop the element
        queue.pop();
 
        // get the original value
        p.first *= -1;

        // if not the answer
        // check for the non visited nodes
        // which are adjacent to present node
        if (visited[p.second] == 0)
            for (int i = 0; i < graph[p.second].size(); i++) {
 
                // value is multiplied by -1 so that 
                // least priority is at the top
                queue.push(make_pair((p.first + 
                  cost[make_pair(p.second, graph[p.second][i])]) * -1, 
                  graph[p.second][i]));
            }
 
        // mark as visited
        visited[p.second] = 1;
    }
 
    return answer;
}
*/

#endif
