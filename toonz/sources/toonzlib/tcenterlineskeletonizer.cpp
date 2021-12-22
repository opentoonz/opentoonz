

#include "tcenterlinevectP.h"

//#define _SSDEBUG                                              // Uncomment to
// enable the debug viewer
//#define _UPDATE                                               // Shows borders
// updated to current time

//====================================================

// Forward declarations

struct VectorizationContext;

//====================================================

//**************************************************************
//      Rationale
//**************************************************************

/*
  NOTE: Input is a vector of Contours, representing the borders of a
        polygonal region. First border is the outer one, followed by internal
        counter-borders; each Contour is itself a vector of "ContourNode"s,
        ordered so that the region to be thinned is at the RIGHT of segments
        formed by successive nodes.

        Output is a Graph structure representing the straight skeleton of the
        thinned region. Original contours survive the thinning procedure.
*/

//**************************************************************
//      Straight Skeleton Debugger
//**************************************************************

#ifdef _SSDEBUG

#include <QWidget>
#include <QTransform>
#include <QEventLoop>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>

class SSDebugger final : public QWidget {
public:
  VectorizationContext &m_context;

  QPoint m_pos, m_pressPos;

  double m_scale;
  QTransform m_transform;
  QEventLoop m_loop;

public:
  double m_height;

public:
  SSDebugger(VectorizationContext &context);
  ~SSDebugger() {}

  void loop() { m_loop.exec(); }

  void paintEvent(QPaintEvent *event);
  void keyPressEvent(QKeyEvent *event);
  void mouseMoveEvent(QMouseEvent *event);
  void mousePressEvent(QMouseEvent *event);
  void mouseReleaseEvent(QMouseEvent *event);
  void wheelEvent(QWheelEvent *event);

  inline QPoint winToWorld(int x, int y);
  inline QPoint worldToWin(double x, double y);
  inline QPointF winToWorldF(int x, int y);

  inline bool isOnScreen(ContourNode *node);

  // Node Updates
  TPointD updated(ContourNode *input);
};

#endif  // _SSDEBUG

//**************************************************************
//      Classes
//**************************************************************

class ContourEdge {
public:
  enum { NOT_OPPOSITE = 0x1 };

public:
  TPointD m_direction;
  unsigned short m_attributes;

public:
  ContourEdge() : m_attributes(0) {}
  ContourEdge(TPointD dir) : m_direction(dir), m_attributes(0) {}

  int hasAttribute(int attr) const { return m_attributes & attr; }
  void setAttribute(int attr) { m_attributes |= attr; }
  void clearAttribute(int attr) { m_attributes &= ~attr; }
};

//--------------------------------------------------------------------------

class IndexTable {
public:
  typedef std::list<ContourNode *> IndexColumn;

  std::vector<IndexColumn>
      m_columns;  //!< Contours set by 'column identifier'.
  std::vector<int>
      m_identifiers;  //!< Column identifiers by original contour index.

  // NOTE: Contours are stored in 'comb' structure (vector of lists) since
  // contours may both
  // be SPLIT (new entry in a list) and MERGED (two lists merge).

public:
  IndexTable() {}

  IndexColumn *operator[](int i) { return &m_columns[i]; }
  IndexColumn &columnOfId(int id) { return m_columns[m_identifiers[id]]; }

  // Initialization
  void build(ContourFamily &family);
  void clear();

  // Specific handlers
  IndexColumn::iterator find(ContourNode *index);
  void merge(IndexColumn::iterator index1, IndexColumn::iterator index2);
  void remove(IndexColumn::iterator index);
};

//--------------------------------------------------------------------------

class Event {
public:
  /*! \remark  Values are sorted by preference at simultaneous events.    */

  enum Type            //! An event's possible types.
  { special,           //!< A vertex event that is also an edge event (V case).
    edge,              //!< An edge shrinks to 0 length.
    vertex,            //!< Two contour nodes clash.
    split_regenerate,  //!< Placeholder type for split events that must be
                       //! regenerated.
    split,             //!< An edge is split by a clashing contour node.
    failure };

public:
  double m_height;
  double m_displacement;
  ContourNode *m_generator;
  ContourNode *m_coGenerator;
  Type m_type;
  unsigned int m_algoritmicTime;

  VectorizationContext *m_context;

public:
  // In-builder event constructor
  Event(ContourNode *generator, VectorizationContext *context);

  // Event calculators
  inline void calculateEdgeEvent();
  inline void calculateSplitEvent();

  // Auxiliary event calculators
  inline double splitDisplacementWith(ContourNode *plane);
  inline bool tryRayEdgeCollisionWith(ContourNode *edge);

  // Event handlers
  inline bool process();
  inline void processEdgeEvent();
  inline void processMaxEvent();
  inline void processSplitEvent();
  inline void processVertexEvent();
  inline void processSpecialEvent();

private:
  inline bool testRayEdgeCollision(ContourNode *opposite, double &displacement,
                                   double &height, double &side1,
                                   double &side2);
};

//--------------------------------------------------------------------------

struct EventGreater {
  bool operator()(const Event &event1, const Event &event2) const {
    return event1.m_height > event2.m_height ||
           (event1.m_height == event2.m_height &&
            event1.m_type > event2.m_type);
  }
};

class Timeline final
    : public std::priority_queue<Event, std::vector<Event>, EventGreater> {
public:
  Timeline() {}

  // NOTE: Timeline construction contains the most complex part of
  // vectorization;
  // progress bar partial notification happens there, so thisVectorizer's signal
  // emission methods must be passed and used.
  void build(ContourFamily &polygons, VectorizationContext &context,
             VectorizerCore *thisVectorizer);
};

//==========================================================================

//--------------------------------------
//    Preliminary methods/functions
//--------------------------------------

// IndexTable methods

void IndexTable::build(ContourFamily &family) {
  unsigned int i;

  m_columns.resize(family.size());
  m_identifiers.resize(family.size());

  // NOTE: At the beginning, m_identifiers= 1, .. , m_columns.size() - 1;
  for (i = 0; i < m_columns.size(); ++i) {
    m_identifiers[i] = i;
    m_columns[i].push_back(&family[i][0]);
    // Each node referenced in the Table is signed as 'head' of the cirular
    // list.
    family[i][0].setAttribute(ContourNode::HEAD);
  }
}

//--------------------------------------------------------------------------

// Explanation: during the skeletonization process, ContourNodes and calculated
// Events are unaware of global index-changes generated by other events, so
// the position of index stored in one Event has to be retrieved in the
// IndexTable before event processing begins.
// NOTE: Can this be done in a more efficient way?...

inline IndexTable::IndexColumn::iterator IndexTable::find(ContourNode *sought) {
  int indexId = m_identifiers[sought->m_ancestorContour];
  IndexColumn::iterator res;

  // Search for the HEAD attribute in index's Contour
  for (; !sought->hasAttribute(ContourNode::HEAD); sought = sought->m_next)
    ;

  // Finally, find index through our column
  for (res = m_columns[indexId].begin(); (*res) != sought; ++res)
    ;

  return res;
}

//--------------------------------------------------------------------------

// Handles active contour merging due to split/vertex events
void IndexTable::merge(IndexColumn::iterator index1,
                       IndexColumn::iterator index2) {
  IndexColumn::iterator current;

  int identifier1 = m_identifiers[(*index1)->m_ancestorContour],
      identifier2 = m_identifiers[(*index2)->m_ancestorContour];

  remove(index2);  // We maintain only one index of the merged contour

  // Now, append columns
  if (!m_columns[identifier2].empty()) {
    append<IndexTable::IndexColumn, IndexTable::IndexColumn::reverse_iterator>(
        m_columns[identifier1], m_columns[identifier2]);
    m_columns[identifier2].clear();
  }

  // Then, update stored identifiers
  for (unsigned int k = 0; k < m_columns.size(); ++k) {
    if (m_identifiers[k] == identifier2) m_identifiers[k] = identifier1;
  }
}

//--------------------------------------------------------------------------

// Removes given index in Table
inline void IndexTable::remove(IndexColumn::iterator index) {
  m_columns[m_identifiers[(*index)->m_ancestorContour]].erase(index);
}

//--------------------------------------------------------------------------

inline void IndexTable::clear() { m_columns.clear(), m_identifiers.clear(); }

//==========================================================================
//                    Straight Skeleton Algorithm
//==========================================================================

//------------------------------
//      Global Variables
//------------------------------

struct VectorizationContext {
  VectorizerCoreGlobals *m_globals;

  // Globals
  unsigned int m_totalNodes;      // Number of original contour nodes
  unsigned int m_contoursCount;   // Number of contours in input region
  IndexTable m_activeTable;       // Index table of active contours
  SkeletonGraph *m_output;        // Output skeleton of input region
  double m_currentHeight;         // Height of our 'roof-flooding' process
  Timeline m_timeline;            // Ordered queue of all possible events
  unsigned int m_algoritmicTime;  // Number of events precessed up to now

  // Containers
  std::vector<ContourEdge> m_edgesHeap;
  std::vector<ContourNode> m_nodesHeap;  // of *non-original* nodes only
  unsigned int m_nodesHeapCount;         // number of nodes used in nodesHeap

  //'Linear Axis-added' *pseudo-original* nodes and edges
  std::vector<ContourNode> m_linearNodesHeap;
  std::vector<ContourEdge> m_linearEdgesHeap;
  unsigned int m_linearNodesHeapCount;

public:
  VectorizationContext(VectorizerCoreGlobals *globals) : m_globals(globals) {}

  ContourNode *getNode() { return &m_nodesHeap[m_nodesHeapCount++]; }
  ContourNode *getLinearNode() {
    return &m_linearNodesHeap[m_linearNodesHeapCount];
  }
  ContourEdge *getLinearEdge() {
    return &m_linearEdgesHeap[m_linearNodesHeapCount++];
  }

  inline void addLinearNodeBefore(ContourNode *node);
  inline void repairDegenerations(
      const std::vector<ContourNode *> &degenerates);

  inline void prepareGlobals();
  inline void prepareContours(ContourFamily &family);

  inline void newSkeletonLink(unsigned int cur, ContourNode *node);
};

//--------------------------------------------------------------------------

// WARNING: To be launched only *after* prepareContours - node countings happen
// there
inline void VectorizationContext::prepareGlobals() {
  // NOTE: Let n be the total number of nodes in the family, k the number of
  // split events
  //      effectively happening in the process, m the number of original
  //      contours of the family.
  //      Now:
  //        * Each split event eliminates its generating reflex node and
  //        introduces
  //          two convex nodes
  //        * Each edge event eliminates its couple of generating nodes and
  //          introduces one new convex node
  //        * Each max event eliminates 3 generating nodes without introducing
  //        new ones

  // So, split events introduce 2k non-original nodes, and (k-m+2) is the number
  // of max events
  // necessarily happening, since (m-1) are the *merging* split events.
  // On the n+k-3(k-m+2) nodes remaining for pure edge events, as many
  // non-original nodes are inserted.
  //=> This yields 2k + n-2k+3m-6= n+3m-6 non-original nodes. Contemporaneous
  // events such as
  // vertex and special events can only decrease the number of non-original
  // nodes requested.

  // Initialize non-original nodes container
  m_nodesHeap.resize(m_totalNodes + 3 * m_contoursCount - 6);
  m_nodesHeapCount = 0;

  // Reset time/height variables
  m_currentHeight  = 0;
  m_algoritmicTime = 0;

  // Clean IndexTable
  m_activeTable.clear();
}

//--------------------------------------------------------------------------

inline void VectorizationContext::newSkeletonLink(unsigned int cur,
                                                  ContourNode *node) {
  if (node->hasAttribute(ContourNode::SK_NODE_DROPPED)) {
    SkeletonArc arcCopy(node);
    m_output->newLink(node->m_outputNode, cur, arcCopy);

    arcCopy.turn();
    m_output->newLink(cur, node->m_outputNode, arcCopy);
  }
}

//==========================================================================

//===================================
//    Thinning Functions/Methods
//===================================

//--------------------------------------------------------------------------

//----------------------------------------
//      Repair Polygon Degenerations
//----------------------------------------

// EXPLANATION: After "Polygonizer", there may be simpleness degenerations
// about polygons which are dangerous to deal in the thinning process.
// Typically, these correspond to cases in which node->m_direction.z ~ 0
//(too fast), and are concave.
// We then deal with them *before* the process begins, by splitting one
// such node in two slower ones (known as 'Linear Axis' method).

inline void VectorizationContext::addLinearNodeBefore(ContourNode *node) {
  ContourNode *newNode = getLinearNode();
  ContourEdge *newEdge = getLinearEdge();

  newNode->m_position = node->m_position;

  // Build new edge
  if (node->m_direction.z < 0.1)
    newEdge->m_direction = rotate270(node->m_edge->m_direction);
  else
    newEdge->m_direction = normalize(node->m_edge->m_direction +
                                     node->m_prev->m_edge->m_direction);

  newNode->m_edge = newEdge;

  // Link newNode
  newNode->m_prev      = node->m_prev;
  newNode->m_next      = node;
  node->m_prev->m_next = newNode;
  node->m_prev         = newNode;

  // Build remaining infos
  node->buildNodeInfos();  // Rebuild
  newNode->buildNodeInfos();

  newNode->m_updateTime      = 0;
  newNode->m_ancestor        = node->m_ancestor;
  newNode->m_ancestorContour = node->m_ancestorContour;

  // Set node and newNode's edges not to be recognized as possible
  // opposites by the other (could happen in *future* instants)
  //                *DO NOT REMOVE!*
  node->m_notOpposites.push_back(newNode->m_edge);
  node->m_notOpposites.push_back(newNode->m_prev->m_edge);
  newNode->m_notOpposites.push_back(node->m_edge);

  // Further sign newly added node
  newNode->setAttribute(ContourNode::LINEAR_ADDED);
}

//--------------------------------------------------------------------------

inline void VectorizationContext::repairDegenerations(
    const std::vector<ContourNode *> &degenerates) {
  unsigned int i;

  m_linearNodesHeap.resize(degenerates.size());
  m_linearEdgesHeap.resize(degenerates.size());
  m_linearNodesHeapCount = 0;

  for (i = 0; i < degenerates.size(); ++i) {
    if (!degenerates[i]->hasAttribute(ContourNode::AMBIGUOUS_LEFT)) {
      addLinearNodeBefore(degenerates[i]);
      m_totalNodes++;
    }
  }
}

//--------------------------------------------------------------------------

//--------------------------------
//    Node Infos Construction
//--------------------------------

inline void VectorizationContext::prepareContours(ContourFamily &family) {
  std::vector<ContourNode *> degenerateNodes;

  // Build circular links
  unsigned int i, j, k;
  unsigned int current;

  m_contoursCount = family.size();
  m_totalNodes    = 0;
  for (i = 0; i < family.size(); ++i) {
    for (j = 0, k = family[i].size() - 1; j < family[i].size(); k = j, ++j) {
      family[i][k].m_next = &family[i][j];
      family[i][j].m_prev = &family[i][k];
    }
    m_totalNodes += family[i].size();
  }

  // Build node edges
  m_edgesHeap.resize(m_totalNodes);
  current = 0;
  for (i = 0; i < family.size(); ++i) {
    for (j = 0, k = family[i].size() - 1; j < family[i].size(); k = j, ++j) {
      m_edgesHeap[current].m_direction = normalize(
          planeProjection(family[i][j].m_position - family[i][k].m_position));
      family[i][k].m_edge = &m_edgesHeap[current];
      current++;
    }
  }

  bool maxThicknessNotZero = m_globals->currConfig->m_maxThickness > 0.0;

  // Now build remaining infos
  for (i = 0; i < family.size(); ++i) {
    for (j = 0; j < family[i].size(); ++j) {
      family[i][j].buildNodeInfos();

      family[i][j].m_updateTime = 0;

      family[i][j].m_ancestor        = j;
      family[i][j].m_ancestorContour = i;

      // Check the following degeneration
      if (family[i][j].m_concave && family[i][j].m_direction.z < 0.3) {
        // Push this node among degenerate ones
        degenerateNodes.push_back(&family[i][j]);
      }

      // Insert output node in sharp angles
      if (!family[i][j].m_concave && family[i][j].m_direction.z < 0.6 &&
          maxThicknessNotZero) {
        family[i][j].setAttribute(ContourNode::SK_NODE_DROPPED);
        family[i][j].m_outputNode = m_output->newNode(family[i][j].m_position);
      }

      // Push on nodes having AMBIGUOUS_RIGHT attribute
      if (family[i][j].hasAttribute(ContourNode::AMBIGUOUS_RIGHT))
        family[i][j].m_position += 0.02 * family[i][j].m_direction;
    }
  }

  // Finally, ensure polygon degenerations found are solved
  if (maxThicknessNotZero) repairDegenerations(degenerateNodes);
}

//--------------------------------------------------------------------------

// WARNING: m_edge field of *this* and *previous* node must already be defined.
inline void ContourNode::buildNodeInfos(bool forceConvex) {
  TPointD direction;
  double parameter;

  // Calculate node convexity
  if (forceConvex)
    m_concave = 0;
  else if (cross(m_edge->m_direction, m_prev->m_edge->m_direction) < 0) {
    m_concave = 1;
  } else
    m_concave = 0;

  // Build node direction
  direction = m_edge->m_direction - m_prev->m_edge->m_direction;
  parameter = norm(direction);
  if (parameter > 0.01) {
    direction                = direction * (1 / parameter);
    if (m_concave) direction = -direction;
  } else
    direction = rotate270(m_edge->m_direction);

  m_direction.x = direction.x;
  m_direction.y = direction.y;

  // Calculate node speed
  m_direction.z = cross(planeProjection(m_direction), m_edge->m_direction);
  if (m_direction.z < 0) m_direction.z = 0;

  // Calculate angular momentum
  m_AngularMomentum = cross(m_position, m_direction);

  if (m_concave) {
    m_AuxiliaryMomentum1 = m_AuxiliaryMomentum2 = m_AngularMomentum;
  } else {
    m_AuxiliaryMomentum1 =
        cross(m_position,
              T3DPointD(m_edge->m_direction.y, -m_edge->m_direction.x, 1));
    m_AuxiliaryMomentum2 =
        cross(m_position, T3DPointD(m_prev->m_edge->m_direction.y,
                                    -m_prev->m_edge->m_direction.x, 1));
  }
}

//--------------------------------------------------------------------------

//---------------------------------
//      Timeline Construction
//---------------------------------

// NOTE: In the following, we achieve these results:
//        * Build the timeline - events priority queue
//        * Process those split events which *necessarily* happen
// Pre-processing of split events is useful in order to lower execution times.

// Each node is first associated to a random integer; then a referencing
// vector is ordered according to those integers - events are calculated
// following this order. Split events are therefore calculated sparsely
// along the polygons, allowing a significant time reduction effect.

class RandomizedNode {
public:
  ContourNode *m_node;
  int m_number;

  RandomizedNode() {}
  RandomizedNode(ContourNode *node) : m_node(node), m_number(rand()) {}

  inline ContourNode *operator->(void) { return m_node; }
};

class RandomizedNodeLess {
public:
  RandomizedNodeLess() {}

  inline bool operator()(RandomizedNode a, RandomizedNode b) {
    return (a.m_number < b.m_number);
  }
};

//--------------------------------------------------------------------------

void Timeline::build(ContourFamily &polygons, VectorizationContext &context,
                     VectorizerCore *thisVectorizer) {
  unsigned int i, j, current;
  std::vector<RandomizedNode> nodesToBeTreated(context.m_totalNodes);
  T3DPointD momentum, ray;

  // Build casual ordered node-array
  for (i = 0, current = 0; i < polygons.size(); ++i)
    for (j                        = 0; j < polygons[i].size(); ++j)
      nodesToBeTreated[current++] = RandomizedNode(&polygons[i][j]);

  // Same for linear-added nodes
  for (i                        = 0; i < context.m_linearNodesHeapCount; ++i)
    nodesToBeTreated[current++] = RandomizedNode(&context.m_linearNodesHeap[i]);

  double maxThickness = context.m_globals->currConfig->m_maxThickness;

  // Compute events generated by nodes
  // NOTE: are edge events to be computed BEFORE split ones?
  for (i = 0; i < nodesToBeTreated.size(); ++i) {
    // Break calculation at user cancel press
    if (thisVectorizer->isCanceled()) break;

    Event currentEvent(nodesToBeTreated[i].m_node, &context);

    // Notify event calculation
    if (!nodesToBeTreated[i].m_node->hasAttribute(ContourNode::LINEAR_ADDED))
      thisVectorizer->emitPartialDone();

    if (currentEvent.m_type != Event::failure &&
        currentEvent.m_height < maxThickness)

#ifdef _PREPROCESS

      if (currentEvent.m_type == Event::split) {
        if (currentEvent.m_coGenerator->m_concave) {
          ray =
              T3DPointD(currentEvent.m_coGenerator->m_edge->m_direction.y,
                        -currentEvent.m_coGenerator->m_edge->m_direction.x, 1);
          momentum = cross(currentEvent.m_coGenerator->m_position, ray);

          if (currentEvent.m_generator->m_direction * momentum +
                  ray * currentEvent.m_generator->m_AngularMomentum <
              0) {
            timeline.push(currentEvent);
            continue;
          }
        }

        if (currentEvent.m_coGenerator->m_next->m_concave) {
          ray =
              T3DPointD(currentEvent.m_coGenerator->m_edge->m_direction.y,
                        -currentEvent.m_coGenerator->m_edge->m_direction.x, 1);
          momentum = cross(currentEvent.m_coGenerator->m_next->m_position, ray);

          if (currentEvent.m_generator->m_direction * momentum +
                  ray * currentEvent.m_generator->m_AngularMomentum >
              0) {
            timeline.push(currentEvent);
            continue;
          }
        }

        if (cross(currentEvent.m_generator->m_edge->m_direction,
                  currentEvent.m_coGenerator->m_edge->m_direction) > 0.02 &&
            cross(currentEvent.m_coGenerator->m_edge->m_direction,
                  currentEvent.m_generator->m_prev->m_edge->m_direction) >
                0.02)  // 0.02 in comparison with 'parameter' in buildNodeInfos
        {
          // Pre-processing succeeded
          currentEvent.process();
          continue;
        }
      }

#endif

    push(currentEvent);
  }
}

//--------------------------------------------------------------------------

//------------------------------
//      Event Calculation
//------------------------------

// Calculates event generated by input node
Event::Event(ContourNode *generator, VectorizationContext *context)
    : m_height(infinity)
    , m_displacement(infinity)
    , m_generator(generator)
    , m_type(failure)
    , m_algoritmicTime(context->m_algoritmicTime)
    , m_context(context) {
  if (generator->m_concave)
    calculateSplitEvent();
  else
    calculateEdgeEvent();
}

//--------------------------------------------------------------------------

// The edge event *generated by a node* is defined as the earliest edge event
// generated by its adjacent edges. Remember that 'edge events' correspond to
// those in which one edge gets 0 length.

inline void Event::calculateEdgeEvent() {
  struct locals {
    static inline void buildDisplacements(ContourNode *edgeFirst, double &d1,
                                          double &d2) {
      ContourNode *edgeSecond = edgeFirst->m_next;

      // If bisectors are almost opposite, avoid: there must be another bisector
      // colliding with m_generator *before* coGenerator - allowing a positive
      // result here may interfere with it.
      if ((edgeFirst->m_concave && edgeSecond->m_concave) ||
          edgeFirst->m_direction * edgeSecond->m_direction < -0.9) {
        d1 = d2 = -1.0;
        return;
      }

      double det = edgeFirst->m_direction.y * edgeSecond->m_direction.x -
                   edgeFirst->m_direction.x * edgeSecond->m_direction.y;

      double cx = edgeSecond->m_position.x - edgeFirst->m_position.x,
             cy = edgeSecond->m_position.y - edgeFirst->m_position.y;

      d1 = (edgeSecond->m_direction.x * cy - edgeSecond->m_direction.y * cx) /
           det;
      d2 =
          (edgeFirst->m_direction.x * cy - edgeFirst->m_direction.y * cx) / det;
    }

    static inline double height(ContourNode *node, double displacement) {
      return node->m_position.z + displacement * node->m_direction.z;
    }
  };  // locals

  double minHeight, minDisplacement;
  bool positiveEdgeDispl;

  m_type = edge;

  // Calculate the two possible displacement parameters
  double firstDisplacement, prevDisplacement, nextDisplacement,
      lastDisplacement;

  // right == prev

  locals::buildDisplacements(m_generator, nextDisplacement, lastDisplacement);
  locals::buildDisplacements(m_generator->m_prev, firstDisplacement,
                             prevDisplacement);

  // Take the smallest positive between them and assign the co-generator
  // NOTE: In a missed vertex event, the threshold value is to compare with the
  // possible pushes at the end of processSplit
  // However, admitting slightly negative displacements should be ok: due to the
  // weak linear axis imposed for concave
  // vertices, it is impossible to have little negative displacements apart from
  // the above mentioned pushed case.
  //   ..currently almost true..

  static const double minusTol = -0.03;

  bool prevDispPositive = (prevDisplacement > minusTol);
  bool nextDispPositive = (nextDisplacement > minusTol);

  if (nextDispPositive) {
    if (!prevDispPositive || nextDisplacement < prevDisplacement) {
      m_coGenerator     = m_generator;
      minDisplacement   = nextDisplacement;
      minHeight         = locals::height(m_coGenerator, nextDisplacement);
      positiveEdgeDispl = (nextDispPositive && lastDisplacement > minusTol);
    } else {
      m_coGenerator   = m_generator->m_prev;
      minDisplacement = prevDisplacement;
      minHeight       = locals::height(
          m_coGenerator,
          firstDisplacement);  // Height is built on the edge's first
      positiveEdgeDispl =
          (prevDispPositive &&
           firstDisplacement >
               minusTol);  // endpoint to have the same values on adjacent
    }                      // generators. It's important for SPECIAL events.
  } else if (prevDispPositive) {
    m_coGenerator   = m_generator->m_prev;
    minDisplacement = prevDisplacement;
    minHeight = locals::height(m_coGenerator, firstDisplacement);  // Same here
    positiveEdgeDispl = (prevDispPositive && firstDisplacement > minusTol);
  } else {
    m_type = failure;
    return;
  }

  // NOTA: Le const di tolleranza sono da confrontare tra:
  // a) i push alla fine di processSplit per evitare rogne coi vertex multipli
  // b) Le condizioni di esclusione su side1 e side2 in trySplit che evitano a)
  // c) Le condizioni di riconoscimento di vertex e special events - perche' se
  // mancano...

  // Special cases: (forse da raffinare le condizioni - comunque ora sono
  // efficaci)
  if (nextDispPositive && !m_generator->m_concave) {
    if (m_generator->m_prev->m_concave && m_generator->m_next->m_concave &&
        fabs(nextDisplacement - prevDisplacement) <
            0.1)  // condizione debole per escludere subito i casi evidentemente
                  // innocenti
    {
      // Check 'V' (special) event - can generate a new concave vertex
      ContourNode *prevRay = m_generator->m_prev,
                  *nextRay = m_generator->m_next;

      double side = prevRay->m_direction * nextRay->m_AngularMomentum +
                    nextRay->m_direction * prevRay->m_AngularMomentum;

      // NOTE: fabs(side) / || prevRay->dir x nextRay->dir ||  is the distance
      // between the two rays.
      if (fabs(side) <
          0.03 * norm(cross(prevRay->m_direction, nextRay->m_direction)))
        m_type = special, m_coGenerator = m_generator;
    } else if (fabs(nextDisplacement - prevDisplacement) < 0.01) {
      // Then choose to make the event with a concave vertex (to resemble the
      // 'LL' case)
      m_coGenerator =
          m_generator->m_next->m_concave ? m_generator : m_generator->m_prev;
    }
  }

  // Now, if calculated height is coherent, this Event is valid.
  if (positiveEdgeDispl  // Edges shrinking to a point after a FORWARD
      ||
      minHeight > m_context->m_currentHeight -
                      0.01)  // displacement are processable - this dominates
    m_height = minHeight,
    m_displacement =
        minDisplacement;  // height considerations which may be affected by
  else                    // numerical errors
    m_type = failure;
}

//--------------------------------------------------------------------------

inline void Event::calculateSplitEvent() {
  unsigned int i;
  bool forceFirst;
  ContourNode *opposite, *first, *last;
  std::list<ContourNode *>::iterator currentContour;

  // Sign *edges* not to be taken as possible opposites
  for (i = 0; i < m_generator->m_notOpposites.size(); ++i)
    m_generator->m_notOpposites[i]->setAttribute(ContourEdge::NOT_OPPOSITE);

  // Check adjacent edge events
  calculateEdgeEvent();  // DO NOT REMOVE - adjacent convexes may have
                         // been calculated too earlier
  // First check opposites in the m_generator active contour
  first =
      m_generator->m_next->m_next;     // Adjacent edges were already considered
  last = m_generator->m_prev->m_prev;  // by calculateEdgeEvents()
  for (opposite = first; opposite != last; opposite = opposite->m_next) {
    if (!opposite->m_edge->hasAttribute(ContourEdge::NOT_OPPOSITE))
      tryRayEdgeCollisionWith(opposite);
  }

  IndexTable &activeTable = m_context->m_activeTable;

  // Then, try in the remaining active contours whose identifier is != our
  for (i = 0; i < activeTable.m_columns.size(); ++i) {
    for (currentContour = activeTable[i]->begin();
         currentContour != activeTable[i]->end(); currentContour++) {
      // Da spostare sopra il 2o for
      if (activeTable.m_identifiers[(*currentContour)->m_ancestorContour] !=
          activeTable.m_identifiers[m_generator->m_ancestorContour]) {
        first = *currentContour;
        for (opposite = first, forceFirst = 1;
             // Better the first next cond. - in case of thinning errors, at
             // least it does not get loop'd.
             !opposite->hasAttribute(ContourNode::HEAD)  // opposite!=first
             || (forceFirst ? forceFirst = 0, 1 : 0);
             opposite = opposite->m_next) {
          if (!opposite->m_edge->hasAttribute(ContourEdge::NOT_OPPOSITE))
            tryRayEdgeCollisionWith(opposite);
        }
      }
    }
  }

  // Restore edge attributes
  for (i = 0; i < m_generator->m_notOpposites.size(); ++i)
    m_generator->m_notOpposites[i]->clearAttribute(ContourEdge::NOT_OPPOSITE);
}

//--------------------------------------------------------------------------

inline bool Event::testRayEdgeCollision(ContourNode *opposite,
                                        double &displacement, double &height,
                                        double &side1, double &side2) {
  // Initialize test vectors

  // NOTE: In the convex case, slab guards MUST be orthogonal to the edge, due
  // to this case:
  //
  //    ______/|                the ray would not hit the edge - AND THUS FOREGO
  //    INTERACTION
  //           |                WITH IT COMPLETELY
  //     ->    |

  T3DPointD firstSlabGuard =
      opposite->m_concave ? opposite->m_direction
                          : T3DPointD(opposite->m_edge->m_direction.y,
                                      -opposite->m_edge->m_direction.x, 1);
  T3DPointD lastSlabGuard =
      opposite->m_next->m_concave
          ? opposite->m_next->m_direction
          : T3DPointD(opposite->m_edge->m_direction.y,
                      -opposite->m_edge->m_direction.x, 1);

  T3DPointD roofSlabOrthogonal(-opposite->m_edge->m_direction.y,
                               opposite->m_edge->m_direction.x, 1);

  if (roofSlabOrthogonal * (opposite->m_position - m_generator->m_position) >
          -0.01  // Ray's vertex generator is below the roof slab
      //&& roofSlabOrthogonal * m_generator->m_direction > 0
      //// Ray must go 'against' the roof slab
      &&
      planeProjection(roofSlabOrthogonal) *
              planeProjection(m_generator->m_direction) >
          0  // Ray must go against the opposing edge
      &&
      (side1 = m_generator->m_direction *
                   opposite->m_AuxiliaryMomentum1 +  // Ray must pass inside the
                                                     // first slab guard
               firstSlabGuard * m_generator->m_AngularMomentum) > -0.01  //
      &&
      (side2 = m_generator->m_direction *
                   opposite->m_next->m_AuxiliaryMomentum2 +  // Ray must pass
                                                             // inside the
                                                             // second slab
                                                             // guard
               lastSlabGuard * m_generator->m_AngularMomentum) < 0.01  //
      &&
      (m_generator->m_ancestorContour !=
           opposite->m_ancestorContour  // Helps with immediate splits from
                                        // coincident
       || m_generator->m_ancestor != opposite->m_ancestor))  // linear vertexes
  {
    displacement = splitDisplacementWith(opposite);

    // Possible Security checks for almost complanarity cases
    //----------------------------------------

    if (displacement > -0.01 && displacement < 0.01) {
      T3DPointD slabLeftOrthogonal(-opposite->m_edge->m_direction.y,
                                   opposite->m_edge->m_direction.x, 1);
      double check1 =
          (m_generator->m_position - opposite->m_position) *
          normalize(cross(opposite->m_direction, slabLeftOrthogonal));

      double check2 =
          (m_generator->m_position - opposite->m_next->m_position) *
          normalize(cross(opposite->m_next->m_direction, slabLeftOrthogonal));

      if (check1 > 0.02 || check2 < -0.02) return false;
    }

    //----------------------------------------

    // Check height/displacement conditions
    if (displacement > -0.01 &&
        displacement < m_displacement + 0.01  // admitting concurrent events
        &&
        (height = m_generator->m_position.z +
                  displacement * m_generator->m_direction.z) >
            m_context->m_currentHeight - 0.01)
      return true;
  }

  return false;
}

//--------------------------------------------------------------------------

inline bool Event::tryRayEdgeCollisionWith(ContourNode *opposite) {
  ContourNode *newCoGenerator;
  Type type;

  double displacement, height, side1, side2;

  if (testRayEdgeCollision(opposite, displacement, height, side1, side2)) {
    type           = split_regenerate;
    newCoGenerator = opposite;

    // Check against the REAL slab guards for type deduction
    double firstSide =
               opposite->m_concave
                   ? side1
                   : m_generator->m_direction * opposite->m_AngularMomentum +
                         opposite->m_direction * m_generator->m_AngularMomentum,
           secondSide = opposite->m_next->m_concave
                            ? side2
                            : m_generator->m_direction *
                                      opposite->m_next->m_AngularMomentum +
                                  opposite->m_next->m_direction *
                                      m_generator->m_AngularMomentum;

    if (firstSide > -0.01 && secondSide < 0.01) {
      double displacement_, height_;

      if (firstSide < 0.01) {
        // Ray hits first extremity of edge
        if (opposite->m_concave ||
            testRayEdgeCollision(opposite->m_prev, displacement_, height_,
                                 side1, side2))
          type = vertex;
      } else if (secondSide > -0.01) {
        // Ray hits second extremity of edge
        if (opposite->m_next->m_concave ||
            testRayEdgeCollision(opposite->m_next, displacement_, height_,
                                 side1, side2)) {
          type           = vertex;
          newCoGenerator = opposite->m_next;
        }
      } else
        type = split;
    }

    if (type == split_regenerate &&
        height <=
            m_context
                ->m_currentHeight)  // Split regeneration is allowed only at
      return false;                 // future times

    // If competing with another event split/vertex, approve replacement only if
    // the angle
    // between m_generator and newCoGenerator is < than with current
    // m_coGenerator.
    if (m_type != edge && fabs(displacement - m_displacement) < 0.01 &&
        angleLess(m_coGenerator->m_edge->m_direction,
                  newCoGenerator->m_edge->m_direction,
                  m_generator->m_edge->m_direction))
      return false;

    // Pero' nel caso di quasi contemporaneo con un convesso, puo' permettere di
    // scegliere quello con Displacement > !! ...
    // Da rivedere... (cmq succede raramente che crei grossi problemi)

    m_type = type, m_coGenerator = newCoGenerator;
    m_displacement = displacement, m_height = height;

    return true;
  }

  return false;
}

//--------------------------------------------------------------------------

inline double Event::splitDisplacementWith(ContourNode *slab) {
  TPointD slabLeftOrthogonal(-slab->m_edge->m_direction.y,
                             slab->m_edge->m_direction.x);
  double denom = m_generator->m_direction.z +
                 slabLeftOrthogonal * TPointD(m_generator->m_direction.x,
                                              m_generator->m_direction.y);

  if (denom < 0.01)
    return -1;  // generator-emitted ray is almost parallel to slab

  TPointD difference =
      planeProjection(slab->m_position - m_generator->m_position);

  return (slabLeftOrthogonal * difference + slab->m_position.z -
          m_generator->m_position.z) /
         denom;
}

//--------------------------------------------------------------------------

//------------------------------
//      Event Processing
//------------------------------

// Event::Process discriminates event types and calls their specific handlers

inline bool Event::process() {
  Timeline &timeline           = m_context->m_timeline;
  unsigned int &algoritmicTime = m_context->m_algoritmicTime;

  if (!m_generator->hasAttribute(ContourNode::ELIMINATED)) {
    switch (m_type) {
    case special:
      assert(!m_coGenerator->hasAttribute(ContourNode::ELIMINATED));

      if (m_coGenerator->m_prev->hasAttribute(
              ContourNode::ELIMINATED) ||  // These two are most probably
                                           // useless - could
          m_coGenerator->m_next->hasAttribute(
              ContourNode::ELIMINATED) ||  // try to remove them once I'm in for
                                           // some testing...
          m_algoritmicTime < m_coGenerator->m_prev->m_updateTime ||
          m_algoritmicTime < m_coGenerator->m_next->m_updateTime) {
        // recalculate event
        Event newEvent(m_generator, m_context);
        if (newEvent.m_type != failure) timeline.push(newEvent);
        return false;
      }

      // else allow processing
      algoritmicTime++;
      processSpecialEvent();

      break;

    case edge:
      if (m_coGenerator->hasAttribute(ContourNode::ELIMINATED) ||
          m_algoritmicTime < m_coGenerator->m_next->m_updateTime) {
        // recalculate event
        Event newEvent(m_generator, m_context);
        if (newEvent.m_type != failure) timeline.push(newEvent);
        return false;
      }

      // Deal with edge superposition cases *only* when m_generator has the
      // m_direction.z == 0.0
      if ((m_coGenerator->m_direction.z == 0.0 &&
           m_coGenerator != m_generator) ||
          (m_coGenerator->m_next->m_direction.z == 0.0 &&
           m_coGenerator == m_generator))
        return false;

      // else allow processing
      algoritmicTime++;  // global
      if (m_generator->m_next->m_next == m_generator->m_prev)
        processMaxEvent();
      else
        processEdgeEvent();

      break;

    case vertex:
      if (m_coGenerator->hasAttribute(ContourNode::ELIMINATED)) {
        // recalculate event
        Event newEvent(m_generator, m_context);
        if (newEvent.m_type != failure) timeline.push(newEvent);
        return false;
      }

      // Unlike the split case, we don't need to rebuild if
      // the event is not up to date with m_coGenerator - since
      // the event is not about splitting an edge

      if (m_coGenerator ==
              m_generator->m_next
                  ->m_next  // CAN devolve to a special event - which should
          ||
          m_coGenerator ==
              m_generator->m_prev
                  ->m_prev)  // already be present in the timeline
        return false;

      // then, process it
      algoritmicTime++;
      processVertexEvent();

      break;

    case split_regenerate:
      if (m_coGenerator->hasAttribute(ContourNode::ELIMINATED) ||
          (m_algoritmicTime < m_coGenerator->m_next->m_updateTime)) {
        // recalculate event
        Event newEvent(m_generator, m_context);
        if (newEvent.m_type != failure) timeline.push(newEvent);
        return false;
      }

    // This may actually happen on current implementation, due to quirky event
    // generation and preferential events rejection. See function tryRay..()
    // around the end. Historically resolved to a split event, so we maintain
    // that.

    // assert(false);

    /* fallthrough */

    case split:  // No break is intended
      if (m_coGenerator->hasAttribute(ContourNode::ELIMINATED) ||
          (m_algoritmicTime < m_coGenerator->m_next->m_updateTime)) {
        // recalculate event
        Event newEvent(m_generator, m_context);
        if (newEvent.m_type != failure) timeline.push(newEvent);
        return false;
      }

      // else allow processing (but check these conditions)
      if (m_coGenerator != m_generator->m_next &&
          m_coGenerator !=
              m_generator->m_prev
                  ->m_prev)  // Because another edge already occurs at his place
      {
        algoritmicTime++;
        processSplitEvent();
      }

      break;
    default:
      break;
    }
  }

  return true;  // Processing succeeded
}

//--------------------------------------------------------------------------

// EXPLANATION:  Here is the typical case:

//        \       /
//         \  x  /
//          2---1 = m_coGenerator

// m_coGenerator's edge reduces to 0. Then, nodes 1 and 2 gets ELIMINATED from
// the active contour and a new node at position "x" is placed instead.
// Observe also that nodes 1 or 2 may be concave (but not both)...

inline void Event::processEdgeEvent() {
  ContourNode *newNode;
  T3DPointD position(m_generator->m_position +
                     m_displacement * m_generator->m_direction);

  // Eliminate and unlink extremities of m_coGenerator's edge
  m_coGenerator->setAttribute(ContourNode::ELIMINATED);
  m_coGenerator->m_next->setAttribute(ContourNode::ELIMINATED);

  // Then, take a node from heap and insert it at their place.
  newNode             = m_context->getNode();
  newNode->m_position = position;

  newNode->m_next                       = m_coGenerator->m_next->m_next;
  m_coGenerator->m_next->m_next->m_prev = newNode;

  newNode->m_prev               = m_coGenerator->m_prev;
  m_coGenerator->m_prev->m_next = newNode;

  // Then, initialize new node (however, 3rd component is m_height...)
  newNode->m_position =
      m_generator->m_position + m_displacement * m_generator->m_direction;
  newNode->m_edge = m_coGenerator->m_next->m_edge;

  newNode->buildNodeInfos(1);  // 1 => Force convex node

  newNode->m_ancestor        = m_coGenerator->m_next->m_ancestor;
  newNode->m_ancestorContour = m_coGenerator->m_next->m_ancestorContour;
  newNode->m_updateTime      = m_context->m_algoritmicTime;

  // We allocate an output vertex on newNode's position under these conditions
  // NOTE: Update once graph_old is replaced
  if (newNode->m_direction.z < 0.7 ||
      m_coGenerator->hasAttribute(ContourNode::SK_NODE_DROPPED) ||
      m_coGenerator->m_next->hasAttribute(ContourNode::SK_NODE_DROPPED)) {
    newNode->setAttribute(ContourNode::SK_NODE_DROPPED);
    newNode->m_outputNode = m_context->m_output->newNode(position);
    m_context->newSkeletonLink(newNode->m_outputNode, m_coGenerator);
    m_context->newSkeletonLink(newNode->m_outputNode, m_coGenerator->m_next);
  }

  // If m_coGenerator or its m_next is HEAD of this contour, then
  // redefine newNode as the new head.
  if (m_coGenerator->hasAttribute(ContourNode::HEAD) ||
      m_coGenerator->m_next->hasAttribute(ContourNode::HEAD)) {
    std::list<ContourNode *>::iterator it;
    std::list<ContourNode *> &column =
        m_context->m_activeTable.columnOfId(m_generator->m_ancestorContour);

    for (it = column.begin(); !(*it)->hasAttribute(ContourNode::ELIMINATED);
         ++it)
      ;

    // assert(*it == m_coGenerator || *it == m_coGenerator->m_next);

    *it = newNode, newNode->setAttribute(ContourNode::HEAD);
  }

  // Finally, calculate the Event raising by newNode
  Event newEvent(newNode, m_context);
  if (newEvent.m_type != Event::failure) m_context->m_timeline.push(newEvent);
}

//--------------------------------------------------------------------------

// Typical triangle case

inline void Event::processMaxEvent() {
  T3DPointD position(m_generator->m_position +
                     m_displacement * m_generator->m_direction);

  unsigned int outputNode = m_context->m_output->newNode(position);

  m_context->newSkeletonLink(outputNode, m_generator);
  m_context->newSkeletonLink(outputNode, m_generator->m_prev);
  m_context->newSkeletonLink(outputNode, m_generator->m_next);

  // Then remove active contour and eliminate nodes
  std::list<ContourNode *>::iterator eventVertexIndex =
      m_context->m_activeTable.find(m_generator);

  m_context->m_activeTable.remove(eventVertexIndex);

  m_generator->setAttribute(ContourNode::ELIMINATED);
  m_generator->m_prev->setAttribute(ContourNode::ELIMINATED);
  m_generator->m_next->setAttribute(ContourNode::ELIMINATED);
}

//--------------------------------------------------------------------------

// EXPLANATION: Ordinary split event:

//   m_coGenerator = a'---------b'
//                         x
//                         b = m_generator
//                        / \
//                       c   a

// We eliminate b and split/merge the border/s represented in the scheme.

inline void Event::processSplitEvent() {
  ContourNode *newLeftNode,
      *newRightNode;  // left-right in the sense of the picture
  T3DPointD position(m_generator->m_position +
                     m_displacement * m_generator->m_direction);
  IndexTable &activeTable      = m_context->m_activeTable;
  unsigned int &algoritmicTime = m_context->m_algoritmicTime;

  // First, we find in the Index Table the contours involved
  std::list<ContourNode *>::iterator genContour, coGenContour;
  genContour = activeTable.find(m_generator);

  if (activeTable.m_identifiers[m_generator->m_ancestorContour] !=
      activeTable.m_identifiers[m_coGenerator->m_ancestorContour]) {
    // We have two different contours, that merge in one
    coGenContour = activeTable.find(m_coGenerator);
  }

  // Now, update known nodes
  m_generator->setAttribute(ContourNode::ELIMINATED);

  // Allocate 2 new nodes and link the following way:
  newLeftNode             = m_context->getNode();
  newRightNode            = m_context->getNode();
  newLeftNode->m_position = newRightNode->m_position = position;

  // On the right side
  m_coGenerator->m_next->m_prev = newRightNode;
  newRightNode->m_next          = m_coGenerator->m_next;
  m_generator->m_prev->m_next   = newRightNode;
  newRightNode->m_prev          = m_generator->m_prev;

  // On the left side
  m_coGenerator->m_next       = newLeftNode;
  newLeftNode->m_prev         = m_coGenerator;
  m_generator->m_next->m_prev = newLeftNode;
  newLeftNode->m_next         = m_generator->m_next;

  // Assign and calculate the new nodes' information
  newLeftNode->m_edge  = m_generator->m_edge;
  newRightNode->m_edge = m_coGenerator->m_edge;

  newLeftNode->m_ancestor         = m_generator->m_ancestor;
  newLeftNode->m_ancestorContour  = m_generator->m_ancestorContour;
  newRightNode->m_ancestor        = m_coGenerator->m_ancestor;
  newRightNode->m_ancestorContour = m_coGenerator->m_ancestorContour;

  // We can force the new nodes to be convex
  newLeftNode->buildNodeInfos(1);
  newRightNode->buildNodeInfos(1);

  newLeftNode->m_updateTime = newRightNode->m_updateTime = algoritmicTime;

  // Now, output the found interaction
  newLeftNode->setAttribute(ContourNode::SK_NODE_DROPPED);
  newRightNode->setAttribute(ContourNode::SK_NODE_DROPPED);
  newLeftNode->m_outputNode  = m_context->m_output->newNode(position);
  newRightNode->m_outputNode = newLeftNode->m_outputNode;
  m_context->newSkeletonLink(newLeftNode->m_outputNode, m_generator);

  // Update the active Index Table:
  if (activeTable.m_identifiers[m_generator->m_ancestorContour] !=
      activeTable.m_identifiers[m_coGenerator->m_ancestorContour]) {
    // If we have two different contours, they merge in one
    // We keep coGenContour and remove genContour
    (*genContour)->clearAttribute(ContourNode::HEAD);
    activeTable.merge(coGenContour, genContour);
  } else {
    // Else we have only one contour, which splits in two
    (*genContour)->clearAttribute(ContourNode::HEAD);
    *genContour = newLeftNode;

    newLeftNode->setAttribute(ContourNode::HEAD);
    newRightNode->setAttribute(ContourNode::HEAD);

    activeTable.columnOfId(m_generator->m_ancestorContour)
        .push_back(newRightNode);
  }

  // (Vertex compatibility): Moving newRightNode a bit on
  newRightNode->m_position += 0.02 * newRightNode->m_direction;

  // Finally, calculate the new left and right Events
  Event newLeftEvent(newLeftNode, m_context);
  if (newLeftEvent.m_type != Event::failure)
    m_context->m_timeline.push(newLeftEvent);

  Event newRightEvent(newRightNode, m_context);
  if (newRightEvent.m_type != Event::failure)
    m_context->m_timeline.push(newRightEvent);
}

//--------------------------------------------------------------------------

// EXPLANATION:

//               c     L     a'
//                \         /
//  m_generator =  b   x   b' = m_coGenerator
//                /         \
//               a     R     c'

// Reflex vertices b and b' collide. Observe that a new reflex vertex may rise
// here.

inline void Event::processVertexEvent() {
  ContourNode *newLeftNode,
      *newRightNode;  // left-right in the sense of the picture
  T3DPointD position(m_generator->m_position +
                     m_displacement * m_generator->m_direction);
  IndexTable &activeTable      = m_context->m_activeTable;
  unsigned int &algoritmicTime = m_context->m_algoritmicTime;

  // First, we find in the Index Table the contours involved
  std::list<ContourNode *>::iterator genContour, coGenContour;
  genContour = activeTable.find(m_generator);

  if (activeTable.m_identifiers[m_generator->m_ancestorContour] !=
      activeTable.m_identifiers[m_coGenerator->m_ancestorContour]) {
    // We have two different contours, that merge in one
    coGenContour = activeTable.find(m_coGenerator);
  }

  // Now, update known nodes
  m_generator->setAttribute(ContourNode::ELIMINATED);
  m_coGenerator->setAttribute(ContourNode::ELIMINATED);

  // Allocate 2 new nodes and link the following way:
  newLeftNode             = m_context->getNode();
  newRightNode            = m_context->getNode();
  newLeftNode->m_position = newRightNode->m_position = position;

  // On the right side
  m_coGenerator->m_next->m_prev = newRightNode;
  newRightNode->m_next          = m_coGenerator->m_next;
  m_generator->m_prev->m_next   = newRightNode;
  newRightNode->m_prev          = m_generator->m_prev;

  // On the left side
  m_coGenerator->m_prev->m_next = newLeftNode;
  newLeftNode->m_prev           = m_coGenerator->m_prev;
  m_generator->m_next->m_prev   = newLeftNode;
  newLeftNode->m_next           = m_generator->m_next;

  // Assign and calculate the new nodes' information
  newLeftNode->m_edge  = m_generator->m_edge;
  newRightNode->m_edge = m_coGenerator->m_edge;

  newLeftNode->m_ancestor         = m_generator->m_ancestor;
  newLeftNode->m_ancestorContour  = m_generator->m_ancestorContour;
  newRightNode->m_ancestor        = m_coGenerator->m_ancestor;
  newRightNode->m_ancestorContour = m_coGenerator->m_ancestorContour;

  // We *CAN'T* force the new nodes to be convex here
  newLeftNode->buildNodeInfos();
  newRightNode->buildNodeInfos();

  newLeftNode->m_updateTime = newRightNode->m_updateTime = algoritmicTime;

  // Now, output the found interaction
  newLeftNode->setAttribute(ContourNode::SK_NODE_DROPPED);
  newRightNode->setAttribute(ContourNode::SK_NODE_DROPPED);
  newLeftNode->m_outputNode  = m_context->m_output->newNode(position);
  newRightNode->m_outputNode = newLeftNode->m_outputNode;
  m_context->newSkeletonLink(newLeftNode->m_outputNode, m_generator);
  m_context->newSkeletonLink(newLeftNode->m_outputNode, m_coGenerator);

  // Update the active Index Table
  if (activeTable.m_identifiers[m_generator->m_ancestorContour] !=
      activeTable.m_identifiers[m_coGenerator->m_ancestorContour]) {
    // If we have two different contours, they merge in one
    (*coGenContour)->clearAttribute(ContourNode::HEAD);
    activeTable.merge(genContour, coGenContour);

    // Check if the generator is head, if so update.
    if (m_generator->hasAttribute(ContourNode::HEAD)) {
      newLeftNode->setAttribute(ContourNode::HEAD);
      *genContour = newLeftNode;
    }

  } else {
    // Else we have only one contour, which splits in two
    (*genContour)->clearAttribute(ContourNode::HEAD);
    *genContour = newLeftNode;

    newLeftNode->setAttribute(ContourNode::HEAD);
    newRightNode->setAttribute(ContourNode::HEAD);

    activeTable.columnOfId(m_generator->m_ancestorContour)
        .push_back(newRightNode);
  }

  // Before calculating the new interactions, to each new node we assign
  // as impossible opposite edges the adjacent of the other node.
  if (newLeftNode->m_concave) {
    newLeftNode->m_notOpposites = m_generator->m_notOpposites;
    append<std::vector<ContourEdge *>,
           std::vector<ContourEdge *>::reverse_iterator>(
        newLeftNode->m_notOpposites, m_coGenerator->m_notOpposites);

    newLeftNode->m_notOpposites.push_back(newRightNode->m_edge);
    newLeftNode->m_notOpposites.push_back(newRightNode->m_prev->m_edge);
  } else if (newRightNode->m_concave) {
    newRightNode->m_notOpposites = m_generator->m_notOpposites;
    append<std::vector<ContourEdge *>,
           std::vector<ContourEdge *>::reverse_iterator>(
        newRightNode->m_notOpposites, m_coGenerator->m_notOpposites);

    newRightNode->m_notOpposites.push_back(newLeftNode->m_edge);
    newRightNode->m_notOpposites.push_back(newLeftNode->m_prev->m_edge);
  }

  // We also forbid newRightNode to be involved in events at the same location
  // of this one.
  // We just push its position in the m_direction by 0.02.
  newRightNode->m_position += 0.02 * newRightNode->m_direction;

  // Finally, calculate the new left and right Events
  Event newLeftEvent(newLeftNode, m_context);
  if (newLeftEvent.m_type != Event::failure)
    m_context->m_timeline.push(newLeftEvent);

  Event newRightEvent(newRightNode, m_context);
  if (newRightEvent.m_type != Event::failure)
    m_context->m_timeline.push(newRightEvent);
}

//--------------------------------------------------------------------------

// EXPLANATION:

//             x
//        ---c   a---
//            \ /
//             b = m_coGenerator

// Typical "V" event in which rays emitted from a, b and c collide.
// This events have to be recognized different from vertex events, and
// better treated as a whole event, rather than two simultaneous edge events.

inline void Event::processSpecialEvent() {
  ContourNode *newNode;
  T3DPointD position(m_generator->m_position +
                     m_displacement * m_generator->m_direction);

  m_coGenerator->setAttribute(ContourNode::ELIMINATED);
  m_coGenerator->m_prev->setAttribute(ContourNode::ELIMINATED);
  m_coGenerator->m_next->setAttribute(ContourNode::ELIMINATED);

  // Get and link newNode to the rest of this contour
  newNode             = m_context->getNode();
  newNode->m_position = position;

  m_coGenerator->m_prev->m_prev->m_next = newNode;
  newNode->m_prev                       = m_coGenerator->m_prev->m_prev;

  m_coGenerator->m_next->m_next->m_prev = newNode;
  newNode->m_next                       = m_coGenerator->m_next->m_next;

  // Then, initialize newNode infos
  newNode->m_edge = m_coGenerator->m_next->m_edge;

  newNode->m_ancestor        = m_coGenerator->m_next->m_ancestor;
  newNode->m_ancestorContour = m_coGenerator->m_next->m_ancestorContour;

  // Neither this case can be forced convex
  newNode->buildNodeInfos();
  newNode->m_updateTime = m_context->m_algoritmicTime;

  // Now build output
  newNode->setAttribute(ContourNode::SK_NODE_DROPPED);
  newNode->m_outputNode = m_context->m_output->newNode(position);
  m_context->newSkeletonLink(newNode->m_outputNode, m_coGenerator->m_prev);
  m_context->newSkeletonLink(newNode->m_outputNode, m_coGenerator);
  m_context->newSkeletonLink(newNode->m_outputNode, m_coGenerator->m_next);

  // If m_coGenerator or one of his adjacents is HEAD of this contour, then
  // redefine newNode as the new head.
  if (m_coGenerator->hasAttribute(ContourNode::HEAD) ||
      m_coGenerator->m_next->hasAttribute(ContourNode::HEAD) ||
      m_coGenerator->m_prev->hasAttribute(ContourNode::HEAD)) {
    std::list<ContourNode *>::iterator it;
    std::list<ContourNode *> &column =
        m_context->m_activeTable.columnOfId(m_generator->m_ancestorContour);

    for (it = column.begin(); !(*it)->hasAttribute(ContourNode::ELIMINATED);
         ++it)
      ;

    // assert(*it == m_coGenerator || *it == m_coGenerator->m_next || *it ==
    // m_coGenerator->m_prev);

    *it = newNode, newNode->setAttribute(ContourNode::HEAD);
  }

  // Finally, calculate the Event raising by newNode
  Event newEvent(newNode, m_context);
  if (newEvent.m_type != Event::failure) m_context->m_timeline.push(newEvent);
}

//==========================================================================

//-------------------------------
//    Straight Skeleton mains
//-------------------------------

static SkeletonGraph *skeletonize(ContourFamily &regionContours,
                                  VectorizationContext &context,
                                  VectorizerCore *thisVectorizer) {
  SkeletonGraph *output = context.m_output = new SkeletonGraph;

  context.prepareContours(regionContours);
  context.prepareGlobals();

  IndexTable &activeTable = context.m_activeTable;
  activeTable.build(regionContours);

  double maxThickness = context.m_globals->currConfig->m_maxThickness;

  if (maxThickness > 0.0)  // if(!currConfig->m_outline)
  {
    Timeline &timeline = context.m_timeline;
    timeline.build(regionContours, context, thisVectorizer);

#ifdef _SSDEBUG
    SSDebugger debugger(context);

    bool spawnDebugger = false;
    if (timeline.size() > 1000) {
      debugger.m_height = context.m_currentHeight;

      debugger.show();
      debugger.raise();

      debugger.repaint();
      debugger.loop();

      spawnDebugger = true;
    }
#endif

    if (thisVectorizer->isCanceled()) {
      // Bailing out
      while (!timeline.empty()) timeline.pop();

      context.m_nodesHeap.clear();
      context.m_edgesHeap.clear();

      context.m_linearNodesHeap.clear();
      context.m_linearEdgesHeap.clear();

      return output;
    }

    // Process timeline
    while (!timeline.empty()) {
      Event currentEvent = timeline.top();
      timeline.pop();

      // If maxThickness hit, stop before processing
      if (currentEvent.m_height >= maxThickness) break;

// Redraw debugger window
#ifdef _SSDEBUG

      if (spawnDebugger && debugger.isOnScreen(currentEvent.m_generator)) {
        debugger.m_height = currentEvent.m_height;

        debugger.repaint();
        debugger.loop();

        if (currentEvent.m_type == Event::split ||
            currentEvent.m_type == Event::vertex)
          currentEvent.tryRayEdgeCollisionWith(currentEvent.m_coGenerator);

        if (currentEvent.m_type == Event::edge)
          currentEvent.calculateEdgeEvent();
      }

#endif  // _SSDEBUG

      // Process event
      currentEvent.process();
      context.m_currentHeight = currentEvent.m_height;
    }

    // The thinning process terminates: deleting non-original nodes and edges.
    while (!timeline.empty()) timeline.pop();

#ifdef _SSDEBUG
    if (spawnDebugger) {
      debugger.m_height = context.m_currentHeight;

      debugger.repaint();
      debugger.loop();
    }
#endif  // _SSDEBUG
  }

  // Finally, update remaining nodes not processed due to maxThickness and
  // connect them to output skeleton
  unsigned int i, l, n;
  IndexTable::IndexColumn::iterator j;
  ContourNode *k;

  for (i = 0; i < regionContours.size(); ++i)
    for (j = activeTable[i]->begin(); j != activeTable[i]->end(); ++j) {
      unsigned int count = 0;
      unsigned int addedNode;
      for (k = *j; !k->hasAttribute(ContourNode::HEAD) || !count;
           k = k->m_next) {
        addedNode = output->newNode(
            k->m_position +
            k->m_direction *
                ((maxThickness - k->m_position.z) /
                 (k->m_direction.z > 0.01 ? k->m_direction.z : 1)));
        context.newSkeletonLink(addedNode, k);
        // output->node(addedNode).setAttribute(ContourNode::SS_OUTLINE);
        ++count;
      }

      n = output->getNodesCount();

      SkeletonArc arcCopy;
      SkeletonArc arcCopyRev;
      arcCopy.setAttribute(SkeletonArc::SS_OUTLINE);
      arcCopyRev.setAttribute(SkeletonArc::SS_OUTLINE_REVERSED);
      for (l = 1; l < count; ++l) {
        output->newLink(n - l, n - l - 1, arcCopyRev);
        output->newLink(n - l - 1, n - l, arcCopy);
      }
      output->newLink(n - l, n - 1, arcCopyRev);
      output->newLink(n - 1, n - l, arcCopy);
    }

  context.m_nodesHeap.clear();
  context.m_edgesHeap.clear();

  context.m_linearNodesHeap.clear();
  context.m_linearEdgesHeap.clear();

  return output;
}

//--------------------------------------------------------------------------

SkeletonList *skeletonize(Contours &contours, VectorizerCore *thisVectorizer,
                          VectorizerCoreGlobals &g) {
  VectorizationContext context(&g);

  SkeletonList *res = new SkeletonList;
  unsigned int i, j;

  // Find overall number of nodes
  unsigned int overallNodes = 0;
  for (i = 0; i < contours.size(); ++i)
    for (j = 0; j < contours[i].size(); ++j)
      overallNodes += contours[i][j].size();

  thisVectorizer->setOverallPartials(overallNodes);

  for (i = 0; i < contours.size(); ++i) {
    res->push_back(skeletonize(contours[i], context, thisVectorizer));

    if (thisVectorizer->isCanceled()) break;
  }

  return res;
}

//--------------------------------------------------------------------------

//--------------
//    DEBUG
//--------------

#ifdef _SSDEBUG

SSDebugger::SSDebugger(VectorizationContext &context)
    : m_context(context)
    , m_scale(1.0)
    , m_loop(this)
    , m_transform(1, 0, 0, -1, 0, height()) {
  setMouseTracking(true);
}

//------------------------------------------------------

inline TPointD SSDebugger::updated(ContourNode *node) {
#ifndef _PREPROCESS
#ifdef _UPDATE
  if (node->m_direction.z > 1e-4) {
    return planeProjection(
        node->m_position +
        ((m_height - node->m_position.z) / node->m_direction.z) *
            node->m_direction);
  } else
    return planeProjection(node->m_position);

#endif
#endif

  return planeProjection(node->m_position);
}

//------------------------------------------------------

#define line(a, b) p.drawLine(QLineF((a).x, (a).y, (b).x, (b).y));

//------------------------------------------------------

void SSDebugger::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setTransform(m_transform);

  // Draw currently produced skeleton
  {
    const SkeletonGraph &skeleton = *m_context.m_output;

    p.setPen(Qt::blue);

    int n, nCount = skeleton.getNodesCount();
    for (n = 0; n != nCount; ++n) {
      const SkeletonGraph::Node &node = skeleton.getNode(n);

      int l, lCount = node.getLinksCount();
      for (l = 0; l != lCount; ++l)
        line(*node, *skeleton.getNode(node.getLink(l).getNext()));
    }
  }

  // Draw background Debug Point

  // Versione updated
  IndexTable &activeTable = m_context.m_activeTable;

  unsigned int i;
  ContourNode *first, *last, *currNode;
  std::list<ContourNode *>::iterator currentContour;

  for (i = 0; i < activeTable.m_columns.size(); ++i) {
    for (currentContour = activeTable[i]->begin();
         currentContour != activeTable[i]->end(); currentContour++) {
      // Draw edge
      p.setPen(Qt::black);
      last = first = *currentContour;
      first        = first->m_next;
      // assert(!last->hasAttribute(ContourNode::ELIMINATED));
      line(updated(last), updated(first));
      for (currNode = first; !currNode->hasAttribute(ContourNode::HEAD);
           currNode = currNode->m_next) {
        // assert(!currNode->hasAttribute(ContourNode::ELIMINATED));
        line(updated(currNode), updated(currNode->m_next));
      }

      // Draw bisector
      p.setPen(Qt::red);
      last = first = *currentContour;
      first        = first->m_next;
      line(updated(last), updated(last) + planeProjection(last->m_direction));
      for (currNode = first; !currNode->hasAttribute(ContourNode::HEAD);
           currNode = currNode->m_next)
        line(updated(currNode),
             updated(currNode) + planeProjection(currNode->m_direction));

      // Draw edge
      p.setPen(Qt::green);
      last = first = *currentContour;
      first        = first->m_next;
      line(updated(last), updated(last) + last->m_edge->m_direction);
      for (currNode = first; !currNode->hasAttribute(ContourNode::HEAD);
           currNode = currNode->m_next)
        line(updated(currNode),
             updated(currNode) + currNode->m_edge->m_direction);
    }
  }

  // Finally, draw text strings
  {
    p.setPen(Qt::red);
    p.setTransform(QTransform());

    const QPointF &worldPos = winToWorldF(m_pos.x(), m_pos.y());

    p.drawText(rect().bottomLeft(), QString("WinPos: %1 %2  WorldPos: %3 %4")
                                        .arg(m_pos.x())
                                        .arg(m_pos.y())
                                        .arg(worldPos.x())
                                        .arg(worldPos.y()));
  }
}

//------------------------------------------------------

void SSDebugger::keyPressEvent(QKeyEvent *event) { m_loop.exit(); }

//------------------------------------------------------

void SSDebugger::mouseMoveEvent(QMouseEvent *event) {
  m_pos = event->pos();

  if (event->buttons() == Qt::MiddleButton) {
    m_transform.translate((event->x() - m_pressPos.x()) / m_scale,
                          (m_pressPos.y() - event->y()) / m_scale);
    m_pressPos = event->pos();
  }

  update();
}

//------------------------------------------------------

void SSDebugger::mousePressEvent(QMouseEvent *event) {
  m_pressPos = m_pos = event->pos();
}

//------------------------------------------------------

void SSDebugger::mouseReleaseEvent(QMouseEvent *event) {}

//------------------------------------------------------

QPoint SSDebugger::worldToWin(double x, double y) {
  return m_transform.map(QPointF(x, y)).toPoint();
}

//------------------------------------------------------

QPoint SSDebugger::winToWorld(int x, int y) {
  return m_transform.inverted().map(QPoint(x, y));
}

//------------------------------------------------------

QPointF SSDebugger::winToWorldF(int x, int y) {
  return m_transform.inverted().map(QPointF(x, y));
}

//------------------------------------------------------

void SSDebugger::wheelEvent(QWheelEvent *event) {
  QPoint w_coords;
  double zoom_par = 1 + event->delta() * 0.001;

  m_scale *= zoom_par;

  w_coords = winToWorld(event->x(), event->y());

  m_transform.translate(w_coords.x(), w_coords.y());
  m_transform.scale(zoom_par, zoom_par);
  m_transform.translate(-w_coords.x(), -w_coords.y());

  update();
}

//------------------------------------------------------

inline bool SSDebugger::isOnScreen(ContourNode *node) {
  const TPointD &pos = updated(node);
  return rect().contains(worldToWin(pos.x, pos.y));
}

#endif
