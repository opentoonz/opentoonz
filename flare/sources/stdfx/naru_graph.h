#include "stdfx.h"
#include <queue>

#define TERMINAL ((Graph::arc*)1)
#define ORPHAN ((Graph::arc*)2)

class Graph {
public:
  typedef enum { SOURCE = 0, SINK = 1 } terminalType;

  Graph(int maxNodeNum);
  ~Graph();

private:
  struct node;
  struct arc;

  struct node {
    arc* firstArc;
    arc* parentArc;
    bool isSink : 1;
    int tCap;
  };

  struct arc {
    node* headNode;
    arc* nextArc;
    arc* revArc;
    int rCap;
  };

  node *nodes, *nodeLast;
  arc *arcs, *arcLast;
  int nodesNum, arcsNum;

  std::queue<node*> activeQueue;
  std::queue<node*> orphanQueue;

  int flow;

public:
  void addEdge(int from, int to, int cap, int revCap) {
    if ((arcLast - arcs) + 2 >= arcsNum) {
      throw std::runtime_error("Maximum number of edges exceeded");
    }
    node* fromNode = nodes + from;
    node* toNode   = nodes + to;

    arc* newArc = arcLast++;
    arc* revArc = arcLast++;

    newArc->headNode   = toNode;
    newArc->rCap       = cap;
    newArc->nextArc    = fromNode->firstArc;
    newArc->revArc     = revArc;
    fromNode->firstArc = newArc;

    revArc->headNode = fromNode;
    revArc->rCap     = revCap;
    revArc->nextArc  = toNode->firstArc;
    revArc->revArc   = newArc;
    toNode->firstArc = revArc;
  }

  void addTerminal(int nodeIndex, int tCap) { nodes[nodeIndex].tCap = tCap; }

  terminalType getSegment(int nodeIndex, terminalType defaultType) {
    if (nodes[nodeIndex].parentArc)
      return nodes[nodeIndex].isSink ? SINK : SOURCE;
    else
      return defaultType;
  }

  void mincut();

private:
  void setActive(node* n) { activeQueue.push(n); }

  node* nextActive() {
    while (!activeQueue.empty()) {
      node* n = activeQueue.front();
      activeQueue.pop();
      if (n->parentArc == ORPHAN) continue;
      return n;
    }
    return nullptr;
  }

  void setOrphan(node* n) {
    n->parentArc = ORPHAN;
    orphanQueue.push(n);
  }

  node* nextOrphan() {
    if (!orphanQueue.empty()) {
      node* n = orphanQueue.front();
      orphanQueue.pop();
      return n;
    } else
      return nullptr;
  }

  void augment(arc* midArc);
  void adopt();
};