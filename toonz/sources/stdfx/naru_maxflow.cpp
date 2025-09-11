#include "naru_graph.h"

Graph::Graph(int maxNodeNum) {
  nodesNum = maxNodeNum;
  arcsNum  = maxNodeNum * 4;
  nodes    = new node[nodesNum];
  arcs     = new arc[arcsNum];
  nodeLast = nodes + nodesNum;
  arcLast  = arcs;
  flow     = 0;

  for (int i = 0; i < maxNodeNum; ++i) {
    nodes[i].firstArc  = nullptr;
    nodes[i].parentArc = nullptr;
    nodes[i].isSink    = false;
    nodes[i].tCap      = 0;
  }
}

Graph::~Graph() {
  delete[] nodes;
  delete[] arcs;
}

void Graph::augment(arc* midArc) {
  node* n;
  arc* arc;

  int bottleneck = midArc->rCap;
  // source tree
  for (n = midArc->revArc->headNode;; n = arc->headNode) {
    arc = n->parentArc;
    if (arc == TERMINAL) break;
    if (bottleneck > arc->revArc->rCap) bottleneck = arc->revArc->rCap;
  }
  if (bottleneck > n->tCap) bottleneck = n->tCap;
  // sink tree
  for (n = midArc->headNode;; n = arc->headNode) {
    arc = n->parentArc;
    if (arc == TERMINAL) break;
    if (bottleneck > arc->rCap) bottleneck = arc->rCap;
  }
  if (bottleneck > -n->tCap) bottleneck = -n->tCap;

  // augment flow
  // source tree
  for (n = midArc->revArc->headNode;; n = arc->headNode) {
    arc = n->parentArc;
    if (arc == TERMINAL) break;
    arc->rCap += bottleneck;
    arc->revArc->rCap -= bottleneck;
    if (!arc->revArc->rCap) setOrphan(n);
  }
  n->tCap -= bottleneck;
  if (!n->tCap) setOrphan(n);
  // sink tree
  for (n = midArc->headNode;; n = arc->headNode) {
    arc = n->parentArc;
    if (arc == TERMINAL) break;
    arc->rCap -= bottleneck;
    arc->revArc->rCap += bottleneck;
    if (!arc->rCap) setOrphan(n);
  }
  n->tCap += bottleneck;
  if (!n->tCap) setOrphan(n);
  // mid arc
  midArc->revArc->rCap += bottleneck;
  midArc->rCap -= bottleneck;

  flow += bottleneck;
}

void Graph::adopt() {
  arc *arc, *arc2;
  node *n, *nn;
  for (n = nextOrphan(); n; n = nextOrphan()) {
    // find a parent for the orphan node n
    bool found = false;
    if (!n->isSink) {
      // source tree
      for (arc = n->firstArc; arc; arc = arc->nextArc) {
        if (!arc->revArc->rCap || arc->headNode->isSink ||
            !arc->headNode->parentArc)
          continue;
        for (nn = arc->headNode;; nn = nn->parentArc->headNode) {
          if (nn->parentArc == TERMINAL) {
            // found a parent
            n->parentArc = arc;
            setActive(n);
            found = true;
            break;
          } else if (nn->parentArc == ORPHAN)
            break;
          if (found) break;
        }
      }
    } else {
      // sink tree
      for (arc = n->firstArc; arc; arc = arc->nextArc) {
        if (!arc->rCap || !arc->headNode->isSink || !arc->headNode->parentArc)
          continue;
        for (nn = arc->headNode;; nn = nn->parentArc->headNode) {
          if (nn->parentArc == TERMINAL) {
            // found a parent
            n->parentArc = arc;
            setActive(n);
            found = true;
            break;
          } else if (nn->parentArc == ORPHAN)
            break;
          if (found) break;
        }
      }
    }

    // no origin found
    if (!found) {
      for (arc = n->firstArc; arc; arc = arc->nextArc) {
        nn   = arc->headNode;
        arc2 = nn->parentArc;
        if ((nn->isSink == n->isSink) && arc2) {
          if (arc2 != TERMINAL && arc2 != ORPHAN && (arc2->headNode == n))
            setOrphan(nn);
        }
      }
    }
  }
}

void Graph::mincut() {
  // initialize active nodes
  for (node* n = nodes; n < nodeLast; ++n) {
    if (n->tCap > 0) {
      n->isSink    = false;
      n->parentArc = TERMINAL;
      setActive(n);
    } else if (n->tCap < 0) {
      n->isSink    = true;
      n->parentArc = TERMINAL;
      setActive(n);
    }
  }

  for (node* currentNode = nextActive(); currentNode;
       currentNode       = nextActive()) {
    // grow phase
    node *n, *nn;
    n = currentNode;
    arc* arc;

    if (!n->isSink) {
      // grow source node
      for (arc = n->firstArc; arc; arc = arc->nextArc) {
        if (arc->rCap <= 0) continue;
        nn = arc->headNode;
        if (!nn->parentArc || nn->parentArc == ORPHAN) {
          nn->isSink    = false;
          nn->parentArc = arc->revArc;
          setActive(nn);
        } else if (nn->isSink) {
          break;
        }
      }
    } else {
      // grow sink node
      for (arc = n->firstArc; arc; arc = arc->nextArc) {
        if (arc->revArc->rCap <= 0) continue;
        nn = arc->headNode;
        if (!nn->parentArc || nn->parentArc == ORPHAN) {
          nn->isSink    = true;
          nn->parentArc = arc->revArc;
          setActive(nn);
        } else if (!nn->isSink) {
          arc = arc->revArc;
          break;
        }
      }
    }

    // found path
    if (arc) {
      // augment phase
      augment(arc);
      // adopt phase
      adopt();
    }
  }
}
