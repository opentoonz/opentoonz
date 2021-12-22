

#include "tcenterlinevectP.h"

//==========================================================================

//*********************************
//    Skeleton re-organization
//*********************************

//==========================================================================

//----------------------------------------
//    Skeleton re-organization Globals
//----------------------------------------

namespace {
VectorizerCoreGlobals *globals;
std::vector<unsigned int> contourFamilyOfOrganized;
JointSequenceGraph *currJSGraph;
ContourFamily *currContourFamily;
};

//==========================================================================

//--------------------------------------
//      Skeleton re-organization
//--------------------------------------

// EXPLANATION:  The raw skeleton data obtained from StraightSkeletonizer
// class need to be grouped in joints and sequences before proceeding with
// conversion in quadratics - which works on single sequences.

// NOTE: Due to maxHeight, we have to assume that a single SkeletonGraph can
// hold
// more connected graphs at once.

// a) Isolate graph-like part of skeleton
// b) Retrieve remaining single sequences.

typedef std::map<UINT, UINT, std::less<UINT>> uintMap;

// void organizeGraphs(SkeletonList* skeleton)
void organizeGraphs(SkeletonList *skeleton, VectorizerCoreGlobals &g) {
  globals = &g;

  SkeletonList::iterator currGraphPtr;
  Sequence currSequence;
  uintMap jointsMap;
  UINT i, j;

  UINT counter = 0;  // We also count current graph number, to associate
  // organized graphs to their contour family

  contourFamilyOfOrganized.clear();

  for (currGraphPtr = skeleton->begin(); currGraphPtr != skeleton->end();
       ++currGraphPtr) {
    SkeletonGraph &currGraph   = **currGraphPtr;
    currSequence.m_graphHolder = &currGraph;

    // Separate single Points - can happen only when a single node gets stored
    // in a SkeletonGraph.
    if (currGraph.getNodesCount() == 1) {
      globals->singlePoints.push_back(*currGraph.getNode(0));
      ++counter;
      continue;
    }

    // Discriminate between graphs, two-endpoint single sequences, and circular
    // ones
    bool has1DegreePoint = 0;
    for (i = 0; i < currGraph.getNodesCount(); ++i) {
      if (currGraph.getNode(i).degree() != 2) {
        if (currGraph.getNode(i).degree() == 1) {
          has1DegreePoint = 1;
        } else {
          goto _graph;
	}
      }
    }

    if (has1DegreePoint)
      goto _two_endpoint;
    else
      goto _circulars;

  _two_endpoint : {
    // Find head
    for (i = 0; currGraph.getNode(i).degree() != 1; ++i)
      ;

    currSequence.m_head     = i;
    currSequence.m_headLink = 0;

    // Find tail
    for (++i;
         i < currGraph.getNodesCount() && currGraph.getNode(i).degree() == 2;
         ++i)
      ;
    currSequence.m_tail     = i;
    currSequence.m_tailLink = 0;

    globals->singleSequences.push_back(currSequence);

    ++counter;
    continue;
  }

  _graph : {
    // Organize Graph-like part
    globals->organizedGraphs.push_back(JointSequenceGraph());
    JointSequenceGraph &JSGraph = globals->organizedGraphs.back();
    contourFamilyOfOrganized.push_back(counter);

    jointsMap.clear();

    // Gather all sequence extremities
    for (i = 0; i < currGraph.getNodesCount(); ++i) {
      if (currGraph.getNode(i).degree() != 2) {
        j = JSGraph.newNode(i);
        // Using a map to keep one-to-one relation between j and i
        jointsMap.insert(uintMap::value_type(i, j));
      }
    }

    // Extract Sequences
    for (i = 0; i < JSGraph.getNodesCount(); ++i) {
      UINT joint = *JSGraph.getNode(i);
      for (j = 0; j < currGraph.getNode(joint).getLinksCount(); ++j) {
        currSequence.m_head     = joint;
        currSequence.m_headLink = j;

        // Seek tail
        UINT oldNode  = joint,
             thisNode = currGraph.getNode(joint).getLink(j).getNext();
        while (currGraph.getNode(thisNode).degree() == 2) {
          currGraph.node(thisNode).setAttribute(
              ORGANIZEGRAPHS_SIGN);  // Sign thisNode as part of a JSGraph
          currSequence.advance(oldNode, thisNode);
        }

        currSequence.m_tail = thisNode;
        currSequence.m_tailLink =
            currGraph.getNode(thisNode).linkOfNode(oldNode);

        JSGraph.newLink(i, jointsMap.find(thisNode)->second, currSequence);
      }
    }
  }

  // NOTE: The following may seem uncommon - you must observe that, *WHEN
  // maxThickness<INF*,
  //      more isolated sequence groups may arise in the SAME SkeletonGraph; so,
  //      an organized
  //      graph may contain different unconnected basic graph-structures.
  //      Further, remaining circular sequences may still exist. Therefore:

  // Proceed with remaining circulars extraction

  _circulars : {
    // Extract all circular sequences
    // Find a sequence point
    for (i = 0; i < currGraph.getNodesCount(); ++i) {
      if (!currGraph.getNode(i).hasAttribute(ORGANIZEGRAPHS_SIGN) &&
          currGraph.getNode(i).degree() == 2) {
        unsigned int curr = i, currLink = 0;
        currSequence.next(curr, currLink);

        for (; curr != i; currSequence.next(curr, currLink))
          currGraph.node(curr).setAttribute(ORGANIZEGRAPHS_SIGN);

        // Add sequence
        currSequence.m_head = currSequence.m_tail = i;
        currSequence.m_headLink                   = 0;
        currSequence.m_tailLink                   = 1;

        globals->singleSequences.push_back(currSequence);
      }
    }
  }
  }
}

//==========================================================================

//******************************
//       Junction Recovery
//******************************

// EXPLANATION: Junction recovery attempts to reshape skeleton junctions when
//  possible. The original polygons shape must not be exceedingly broken, and
//  no visible shape alteration must found.
// WARNING: Currently not working together with
// globals->currConfig->m_maxThickness<INF.

//--------------------------------------------------------------------------

//------------------
//    Globals
//------------------

namespace {
const double roadsMaxSlope = 0.3;
const double shapeDistMul  = 1;
const double hDiffMul      = 0.3;
const double lineDistMul   = 1;
const double pullBackMul   = 0.2;
}

//--------------------------------------------------------------------------

//-------------------------
//    Roads Extraction
//-------------------------

// SMALL ISSUE: The following is currently done for both side
// of a sequence...

inline void findRoads(const Sequence &s) {
  unsigned int curr, currLink, next;
  unsigned int roadStart, roadStartLink;
  double d, hMax, length;
  bool lowSlope, roadOn = 0;

  curr     = s.m_head;
  currLink = s.m_headLink;
  for (; curr != s.m_tail; s.next(curr, currLink)) {
    next = s.m_graphHolder->getNode(curr).getLink(currLink).getNext();

    d = planeDistance(*s.m_graphHolder->getNode(curr),
                      *s.m_graphHolder->getNode(next));
    lowSlope =
        fabs(s.m_graphHolder->getNode(curr).getLink(currLink)->getSlope()) <
                roadsMaxSlope
            ? 1
            : 0;

    // Entering event in a *possibile* road axis
    if (!roadOn && lowSlope) {
      length        = 0;
      hMax          = s.m_graphHolder->getNode(curr)->z;
      roadStart     = curr;
      roadStartLink = currLink;
    }

    // Exit event from a          """
    else if (roadOn && !lowSlope && (length > hMax))
      // Then sign ROAD the sequence from roadStart to curr.
      for (; roadStart != curr; s.next(roadStart, roadStartLink))
        s.m_graphHolder->node(roadStart)
            .link(roadStartLink)
            ->setAttribute(SkeletonArc::ROAD);

    // Now update vars
    if (lowSlope) {
      length += d;
      if (hMax < s.m_graphHolder->getNode(next)->z)
        hMax = s.m_graphHolder->getNode(next)->z;
    }

    roadOn = lowSlope;
  }

  // At sequence end, force an exit event
  if (roadOn && (length > hMax))
    for (; roadStart != curr; s.next(roadStart, roadStartLink))
      s.m_graphHolder->node(roadStart)
          .link(roadStartLink)
          ->setAttribute(SkeletonArc::ROAD);
}

//--------------------------------------------------------------------------

// Find the 'roads' of the current Graph.
static void findRoads(const JointSequenceGraph &JSGraph) {
  unsigned int i, j;

  // For all Sequence of currGraph, extract roads
  for (i = 0; i < JSGraph.getNodesCount(); ++i) {
    for (j = 0; j < JSGraph.getNode(i).getLinksCount(); ++j)
      // if(JSGraph.getNode(i).getLink(j)->isForward())
      findRoads(*JSGraph.getNode(i).getLink(j));
  }
}

//--------------------------------------------------------------------------

//----------------------------------
//    Junction Recovery Classes
//----------------------------------

// Entering point of a Sequence inside a Junction Area
class EnteringSequence final : public Sequence {
public:
  TPointD m_direction;  // In this case, we keep
  double m_height;      // separated (x,y) and z coords.

  // Also store indices toward the next (outer) joint
  unsigned int m_initialJoint;
  unsigned int m_outerLink;

  EnteringSequence() {}
  EnteringSequence(const Sequence &s) : Sequence(s) {}
};

//--------------------------------------------------------------------------

class JunctionArea {
public:
  std::vector<EnteringSequence> m_enteringSequences;
  std::vector<unsigned int> m_jointsAbsorbed;
  TPointD m_newJointPosition;

  JunctionArea() {}

  // Initialize Junction Area
  void expandArea(unsigned int initial);

  // Calculate and evaluate area
  bool checkShape();
  bool solveJunctionPosition();
  bool makeHeights();
  bool calculateReconstruction();

  // Substitute area over old configuration
  bool sequencesPullBack();
  void apply();
};

//--------------------------------------------------------------------------

//----------------------------
//    Expansion Procedure
//----------------------------

// Junction Area expansion procedure
inline void JunctionArea::expandArea(unsigned int initial) {
  unsigned int a;
  unsigned int curr, currLink;
  unsigned int i, iLink, iNext;

  m_jointsAbsorbed.push_back(initial);
  currJSGraph->node(initial).setAttribute(
      JointSequenceGraph::REACHED);  // Nodes absorbed gets signed

  for (a = 0; a < m_jointsAbsorbed.size(); ++a) {
    // Extract a joint from vector
    curr = m_jointsAbsorbed[a];

    for (currLink = 0; currLink < currJSGraph->getNode(curr).getLinksCount();
         ++currLink) {
      Sequence &s = *currJSGraph->node(curr).link(currLink);
      if (s.m_graphHolder->getNode(s.m_head)
              .getLink(s.m_headLink)
              .getAccess()) {
        // Expand into all nearby sequences, until a ROAD is found
        i     = s.m_head;
        iLink = s.m_headLink;
        for (; i != s.m_tail &&
               !s.m_graphHolder->getNode(i).getLink(iLink)->hasAttribute(
                   SkeletonArc::ROAD);
             s.next(i, iLink))
          ;

        // If the sequence has been completely run, include next joint in the
        // expansion procedure AND sign it as 'REACHED'
        if (i == s.m_tail) {
          iNext = currJSGraph->getNode(curr).getLink(currLink).getNext();
          if (!currJSGraph->node(iNext).hasAttribute(
                  JointSequenceGraph::REACHED)) {
            currJSGraph->node(iNext).setAttribute(JointSequenceGraph::REACHED);
            m_jointsAbsorbed.push_back(iNext);
          }
          // Negate access to this sequence
          s.m_graphHolder->node(s.m_tail).link(s.m_tailLink).setAccess(0);
          s.m_graphHolder->node(s.m_head).link(s.m_headLink).setAccess(0);
        } else {
          // Initialize and copy the entering sequence found
          m_enteringSequences.push_back(EnteringSequence(s));
          m_enteringSequences.back().m_head     = i;
          m_enteringSequences.back().m_headLink = iLink;

          // Initialize entering directions
          iNext = s.m_graphHolder->getNode(i).getLink(iLink).getNext();
          m_enteringSequences.back().m_direction = planeProjection(
              *s.m_graphHolder->getNode(i) - *s.m_graphHolder->getNode(iNext));
          m_enteringSequences.back().m_direction =
              m_enteringSequences.back().m_direction *
              (1 / norm(m_enteringSequences.back().m_direction));

          // Initialize entering height / slope
          m_enteringSequences.back().m_height = s.m_graphHolder->getNode(i)->z;

          // Also store pointer to link toward the joint at the end of sequence
          m_enteringSequences.back().m_initialJoint = curr;
          m_enteringSequences.back().m_outerLink    = currLink;
        }
      }
    }
  }
}

//--------------------------------------------------------------------------

//-----------------------
//    Area Shape Test
//-----------------------

inline bool JunctionArea::checkShape() {
  std::vector<EnteringSequence>::iterator a, b;
  unsigned int node, contour, first, last, n;
  TPointD A, A1, B, B1, P, P1;
  bool result = 1;

  // First, sign all outgoing arcs' m_leftGeneratingNode as end of
  // control procedure
  for (a = m_enteringSequences.begin(); a != m_enteringSequences.end(); ++a) {
    node = a->m_graphHolder->getNode(a->m_head)
               .getLink(a->m_headLink)
               ->getLeftGenerator();
    contour = a->m_graphHolder->getNode(a->m_head)
                  .getLink(a->m_headLink)
                  ->getLeftContour();

    (*currContourFamily)[contour][node].setAttribute(ContourNode::JR_RESERVED);
  }

  // Now, check shape
  for (a = m_enteringSequences.begin(), b = m_enteringSequences.end() - 1;
       a != m_enteringSequences.end(); b = a, ++a) {
    // Initialize contour check
    first = a->m_graphHolder->getNode(a->m_head)
                .getLink(a->m_headLink)
                ->getRightGenerator();
    // last= b->m_graphHolder->getNode(b->m_head).getLink(b->m_headLink)
    //  ->getLeftGenerator();
    contour = a->m_graphHolder->getNode(a->m_head)
                  .getLink(a->m_headLink)
                  ->getRightContour();
    n = (*currContourFamily)[contour].size();

    // Better this effective way to find the last
    unsigned int numChecked = 0;
    for (last = first; !(*currContourFamily)[contour][last].hasAttribute(
                           ContourNode::JR_RESERVED) &&
                       numChecked < n;
         last = (last + 1) % n, ++numChecked)
      ;

    // Security check
    if (numChecked == n) return 0;

    A  = planeProjection((*currContourFamily)[contour][first].m_position);
    A1 = planeProjection(
        (*currContourFamily)[contour][(first + 1) % n].m_position);
    B  = planeProjection((*currContourFamily)[contour][last].m_position);
    B1 = planeProjection(
        (*currContourFamily)[contour][(last + 1) % n].m_position);

    for (node = first; !(*currContourFamily)[contour][node].hasAttribute(
             ContourNode::JR_RESERVED);
         node = (node + 1) % n) {
      P  = planeProjection((*currContourFamily)[contour][node].m_position);
      P1 = planeProjection(
          (*currContourFamily)[contour][(node + 1) % n].m_position);

      // EXPLANATION:
      // Segment P-P1  must be included in fat lines passing for A-A1 or B-B1
      result &=
          (fabs(cross(P - A, normalize(A1 - A))) < a->m_height * shapeDistMul &&
           fabs(cross(P1 - A, normalize(A1 - A))) < a->m_height * shapeDistMul)

          ||

          (fabs(cross(P - B, normalize(B1 - B))) < b->m_height * shapeDistMul &&
           fabs(cross(P1 - B, normalize(B1 - B))) < b->m_height * shapeDistMul);
    }
  }

  // Finally, restore nodes attributes
  for (a = m_enteringSequences.begin(); a != m_enteringSequences.end(); ++a) {
    node = a->m_graphHolder->getNode(a->m_head)
               .getLink(a->m_headLink)
               ->getLeftGenerator();
    contour = a->m_graphHolder->getNode(a->m_head)
                  .getLink(a->m_headLink)
                  ->getLeftContour();

    (*currContourFamily)[contour][node].clearAttribute(
        ContourNode::JR_RESERVED);
  }

  return result;
}

//--------------------------------------------------------------------------

//--------------------------------------------
//    Solve new junction position problem
//--------------------------------------------

inline bool JunctionArea::solveJunctionPosition() {
  std::vector<EnteringSequence>::iterator a;
  double Sx2 = 0, Sy2 = 0, Sxy = 0;
  TPointD P, v, b;
  double h;

  // Build preliminary sums for the linear system
  for (a = m_enteringSequences.begin(); a != m_enteringSequences.end(); ++a) {
    h = a->m_height;
    v = a->m_direction;
    P = planeProjection(*a->m_graphHolder->getNode(a->m_head));

    // Height-weighted problem
    Sx2 += sq(v.x) * h;
    Sy2 += sq(v.y) * h;
    Sxy += v.x * v.y * h;
    b.x += h * (sq(v.y) * P.x - (v.x * v.y * P.y));
    b.y += h * (sq(v.x) * P.y - (v.x * v.y * P.x));
  }

  // First check problem determinant
  double det = Sx2 * Sy2 - sq(Sxy);
  if (fabs(det) < 0.1) return 0;

  // Now construct linear system
  TAffine M(Sx2 / det, Sxy / det, 0, Sxy / det, Sy2 / det, 0);

  m_newJointPosition = M * b;

  // Finally, check if J is too far from the line extensions of the entering
  // sequences
  for (a = m_enteringSequences.begin(); a != m_enteringSequences.end(); ++a) {
    P = planeProjection(*a->m_graphHolder->getNode(a->m_head));
    if (tdistance(m_newJointPosition, a->m_direction, P) >
        a->m_height * lineDistMul)
      return 0;
  }

  return 1;
}

//--------------------------------------------------------------------------

//---------------------------------------
//    Calculate optimal joint heights
//---------------------------------------

// Globals

namespace {
const std::vector<EnteringSequence> *currEnterings;
const std::vector<unsigned int> *heightIndicesPtr;

std::vector<double> *optHeights;
double optMeanError;
double hMax;
}

//--------------------------------------------------------------------------

inline bool checkCircles(std::vector<double> &heights) {
  unsigned int i, j;
  double cos, sin, frac;
  TPointD vi, vj;

  // Execute test on angle-consecutive EnteringSequences couples
  for (j = 0, i = currEnterings->size() - 1; j < currEnterings->size();
       i = j, ++j) {
    vi = (*currEnterings)[i].m_direction;
    vj = (*currEnterings)[j].m_direction;

    sin = cross(vi, vj);

    if (heights[i] == heights[j]) goto test_against_max_height;

    frac = heights[i] / heights[j];
    if (sin < 0) return 0;
    cos = vi * vj;

    if (cos < 0 && (frac < -cos || frac > (-1 / cos))) return 0;

  test_against_max_height:
    // Reusing cos
    cos = (sin < 0.1)
              ? std::max(heights[i], heights[j])
              : norm((vi * (heights[j] / sin)) + (vj * (heights[i] / sin)));
    if (cos < hMax) return 0;
  }

  return 1;
}

//--------------------------------------------------------------------------

inline void tryConfiguration(const std::vector<unsigned int> &bounds) {
  std::vector<double> currHeights(currEnterings->size());
  double mean, currMeanError = 0;
  unsigned int i, j, first, end;

  for (i = 0, first = 0; i <= bounds.size(); first = end, ++i) {
    end = i < bounds.size() ? end = bounds[i] + 1 : currEnterings->size();

    // Calculate mean from first (included) to end (not included)
    for (j = first, mean = 0; j < end; ++j)
      mean += (*currEnterings)[(*heightIndicesPtr)[j]].m_height;

    mean /= end - first;

    // Check if the distance from extremities to mean is tolerable
    if (std::max((*currEnterings)[(*heightIndicesPtr)[end - 1]].m_height - mean,
                 mean - (*currEnterings)[(*heightIndicesPtr)[first]].m_height) >
        hDiffMul * mean)
      return;

    // Calculate squared error to mean
    for (j = first; j < end; ++j)
      currMeanError +=
          sq((*currEnterings)[(*heightIndicesPtr)[j]].m_height - mean);

    // Set calculated currHeights
    for (j = first; j < end; ++j) currHeights[(*heightIndicesPtr)[j]] = mean;
  }

  // Update current maximum height
  hMax = mean;  // Global

  // If this configuration could be better than current, launch circle test
  if (optHeights->empty() || currMeanError < optMeanError) {
    if (checkCircles(currHeights)) {
      (*optHeights) = currHeights;
      optMeanError  = currMeanError;
    }
  }
}

//--------------------------------------------------------------------------

class hLess {
public:
  std::vector<EnteringSequence> &m_entSequences;

  hLess(std::vector<EnteringSequence> &v) : m_entSequences(v) {}

  inline bool operator()(unsigned int a, unsigned int b) {
    return m_entSequences[a].m_height < m_entSequences[b].m_height;
  }
};

//--------------------------------------------------------------------------

// EXPLANATION: We build intervals on which height means are done.
// Their right Interval Bounds are supposed INCLUDED.

// Example: heights[]= {1, 2, 3, 4}; rightIntervalBounds[]={0, 2};
//  =>  do height means on: {1}, {2,3}, {4}. (*)

// After means are calculated, a test on the obtained configuration is
// performed. Among those configurations which pass the test, the one
// with rightIntervalBounds.size()->min and, on same sizes,
// currMeanError->min is the 'best' configuration possible.
// If no height configuration pass the test, reconstruction fails.

//(*) NOTE: The right Interval Bounds will never include index n-1, which
// interferes with push_backs.

inline bool JunctionArea::makeHeights() {
  std::vector<unsigned int> heightOrderedIndices;
  std::vector<unsigned int> rightIntervalBounds;
  std::vector<double> optimalHeights;

  unsigned int i, n, m;

  // Sort entering sequences' indices for increasing height/thickness
  heightOrderedIndices.resize(m_enteringSequences.size());
  for (i = 0; i < m_enteringSequences.size(); ++i) heightOrderedIndices[i] = i;
  sort(heightOrderedIndices.begin(), heightOrderedIndices.end(),
       hLess(m_enteringSequences));

  // Initialize globals/references
  currEnterings    = &m_enteringSequences;
  heightIndicesPtr = &heightOrderedIndices;

  optMeanError = 0;
  optHeights   = &optimalHeights;

  // Now build height-mean configurations and launch their tests
  n = m_enteringSequences.size();

  // The m=0 case is done first, apart
  rightIntervalBounds.resize(0);
  tryConfiguration(rightIntervalBounds);
  for (m = 1; m < n && optimalHeights.empty(); ++m) {
    // Initialize bounds
    rightIntervalBounds.resize(1);
    rightIntervalBounds[0] = 0;

    while (!rightIntervalBounds.empty()) {
      // Fill bounds if necessary
      while (rightIntervalBounds.size() < m)
        rightIntervalBounds.push_back(rightIntervalBounds.back() + 1);

      tryConfiguration(rightIntervalBounds);

      //'Advance' configuration: increment last index and pop those
      // exceeding valid size. If bounds gets empty, done all configs
      // with m+1 intervals.
      while ((
          ++rightIntervalBounds.back(),
          rightIntervalBounds.back() < n - 1 - (m - rightIntervalBounds.size())
              ? 0
              : (rightIntervalBounds.pop_back(), !rightIntervalBounds.empty())))
        ;
    }
  }

  if (!optimalHeights.empty()) {
    for (i = 0; i < n; ++i) m_enteringSequences[i].m_height = optimalHeights[i];
    return 1;
  }

  return 0;
}

//==========================================================================

//------------------------------
//    Area Calculation Main
//------------------------------

class EntSequenceLess {
public:
  EntSequenceLess() {}

  inline bool operator()(const EnteringSequence &a, const EnteringSequence &b) {
    // m_direction is normalized, therefore:
    return a.m_direction.y >= 0
               ? b.m_direction.y >= 0 ? a.m_direction.x > b.m_direction.x : 1
               : b.m_direction.y < 0 ? a.m_direction.x < b.m_direction.x : 0;
  }
};

//--------------------------------------------------------------------------

bool JunctionArea::calculateReconstruction() {
  unsigned int i;

  if (m_enteringSequences.size() == 0) return 0;

  // Apply preliminary tests for this Junction Area

  // a) Check if endpoints got absorbed by the expansion process
  for (i = 0; i < m_jointsAbsorbed.size(); ++i)
    if (currJSGraph->getNode(m_jointsAbsorbed[i]).degree() == 1) return 0;

  // b) Check if the contours shape resembles that of a crossing

  sort(m_enteringSequences.begin(), m_enteringSequences.end(),
       EntSequenceLess());

  if (!checkShape()) return 0;

  // c) Build the new junction Point plane position
  if (!solveJunctionPosition()) return 0;

  // d) Build joint optimal heights (each for entering sequence...)
  if (!makeHeights()) return 0;

  return 1;
}

//==========================================================================

//---------------------------
//    Sequences Pull Back
//---------------------------

// EXPLANATION: We have to insure that connecting entering sequences to the
// new junction point happens *smoothly*. In order to do this, wh withdraw
// entering sequences along the enterin road, until the angle given by the
// connecting line and the entering direction is small.
// However, sequence pull back can be done only under some constraints:
//  * we have to remain inside a road axis
//  * ........................ a given fat line around the entering direction

inline bool JunctionArea::sequencesPullBack() {
  std::vector<EnteringSequence>::iterator a;
  double alongLinePosition, distanceFromLine;
  unsigned int i, iLink, tail;
  TPointD P;

  for (a = m_enteringSequences.begin(); a != m_enteringSequences.end(); ++a) {
    i     = a->m_head;
    iLink = a->m_headLink;
    // NOTE: updated tails are stored this way, *DONT* look in a->m_tail
    // because these typically store old infos
    tail =
        currJSGraph->getNode(a->m_initialJoint).getLink(a->m_outerLink)->m_tail;
    P = planeProjection(*a->m_graphHolder->getNode(a->m_head));

    while (i != tail) {
      alongLinePosition = a->m_direction * (m_newJointPosition - P);
      distanceFromLine  = tdistance(m_newJointPosition, a->m_direction, P);

      if (alongLinePosition >= 0 &&
          (distanceFromLine / alongLinePosition) <= 0.5)
        goto found_pull_back;

      // We then take the next arc and check it

      if (!a->m_graphHolder->getNode(i).getLink(iLink)->hasAttribute(
              SkeletonArc::ROAD))
        return 0;  // Pull back failed

      a->next(i, iLink);

      P = planeProjection(*a->m_graphHolder->getNode(i));
      if (tdistance(P, a->m_direction, m_newJointPosition) >
          std::max(pullBackMul * a->m_height, 1.0))
        return 0;  // Pull back failed
    }

    // Now checking opposite sequence extremity
    if (alongLinePosition < 0 || (distanceFromLine / alongLinePosition) > 0.5)
      return 0;

  found_pull_back:

    a->m_head     = i;
    a->m_headLink = iLink;
  }

  return 1;
}

//--------------------------------------------------------------------------

//----------------------------------------------
//    Substitute new junction configuration
//----------------------------------------------

void JunctionArea::apply() {
  std::vector<EnteringSequence>::iterator a;
  unsigned int newJoint, newSkeletonNode;
  unsigned int head, headLink, tail, tailLink;
  unsigned int outerJoint, outerLink;
  unsigned int i, next, nextLink;

  // First, check if Entering Sequences pullback is possible
  if (!sequencesPullBack()) return;

  // Then, we can substitute the old configuration
  // First, sign as 'ELIMINATED' all absorbed joints
  for (i = 0; i < m_jointsAbsorbed.size(); ++i)
    currJSGraph->node(m_jointsAbsorbed[i])
        .setAttribute(JointSequenceGraph::ELIMINATED);

  newJoint = currJSGraph->newNode();

  for (a = m_enteringSequences.begin(); a != m_enteringSequences.end(); ++a) {
    // Initialize infos
    newSkeletonNode =
        a->m_graphHolder->newNode(T3DPointD(m_newJointPosition, a->m_height));

    // Retrieve sequence infos to substitute
    // NOTE: We update *tail* infos in currJSGraph sequences after each "apply"

    const JointSequenceGraph::Link &tempLink =
        currJSGraph->getNode(a->m_initialJoint).getLink(a->m_outerLink);

    head     = tempLink->m_head;
    headLink = tempLink->m_headLink;
    tail     = tempLink->m_tail;
    tailLink = tempLink->m_tailLink;

    outerJoint = tempLink.getNext();

    // Find outerLink - from outerJoint to a->m_initialJoint
    for (i = 0;
         (currJSGraph->getNode(outerJoint).getLink(i)->m_tail != head) ||
         (currJSGraph->getNode(outerJoint).getLink(i)->m_tailLink != headLink);
         ++i)
      ;
    outerLink = i;

    // Now, provide skeleton linkages
    // NOTE: Discriminate between the following cases
    // a) m_head->degree == 2
    // b) m_head == m_tail
    // c) m_head == original m_head - or, (m_head!=m_tail && m_head->deg>2)

    if (a->m_graphHolder->getNode(a->m_head).degree() == 2) {
      a->m_graphHolder->newLink(newSkeletonNode, a->m_head);

      a->m_graphHolder->node(a->m_head)
          .link(!a->m_headLink)
          .setNext(newSkeletonNode);
      a->m_graphHolder->node(a->m_head)
          .link(!a->m_headLink)
          ->setAttribute(SkeletonArc::ROAD);
      // Better clear road attribute or set it?
    } else if (a->m_head == tail) {
      a->m_graphHolder->newLink(newSkeletonNode, tail);

      a->m_graphHolder->node(tail).link(tailLink).setNext(newSkeletonNode);
      a->m_graphHolder->node(tail).link(tailLink)->setAttribute(
          SkeletonArc::ROAD);
    } else  //(a->m_head == head)
    {
      // In this case, better introduce further a new substitute of head
      unsigned int newHead = a->m_graphHolder->newNode(
          T3DPointD(*a->m_graphHolder->getNode(a->m_head)));

      a->m_graphHolder->newLink(newSkeletonNode, newHead);
      a->m_graphHolder->newLink(newHead, newSkeletonNode);

      // Link newHead on the other side
      next     = a->m_graphHolder->getNode(head).getLink(headLink).getNext();
      nextLink = a->m_graphHolder->getNode(next).linkOfNode(head);

      a->m_graphHolder->newLink(newHead, next);
      a->m_graphHolder->node(next).link(nextLink).setNext(newHead);
    }

    // Finally, update joint linkings and sequence tails.
    Sequence newSequence;

    newSequence.m_graphHolder = a->m_graphHolder;
    newSequence.m_head        = newSkeletonNode;
    newSequence.m_headLink    = 0;
    newSequence.m_tail        = tail;
    newSequence.m_tailLink    = tailLink;

    currJSGraph->node(outerJoint).link(outerLink).setNext(newJoint);
    currJSGraph->node(outerJoint).link(outerLink)->m_tail     = newSkeletonNode;
    currJSGraph->node(outerJoint).link(outerLink)->m_tailLink = 0;

    currJSGraph->newLink(newJoint, outerJoint, newSequence);
  }
}

//--------------------------------------------------------------------------

//-------------------------------
//    Junction Recovery Main
//-------------------------------

// EXPLANATION: Junction Recovery attempts reconstruction of badly-behaved
// crossings in the raw skeleton of the image.

// void inline junctionRecovery(Contours* polygons)
void junctionRecovery(Contours *polygons, VectorizerCoreGlobals &g) {
  globals = &g;

  unsigned int i, j;
  std::vector<JunctionArea> junctionAreasList;

  // For all joints not processed by the Recoverer, launch a new junction
  // area reconstruction
  for (i = 0; i < globals->organizedGraphs.size(); ++i) {
    currJSGraph       = &globals->organizedGraphs[i];
    currContourFamily = &(*polygons)[contourFamilyOfOrganized[i]];
    junctionAreasList.clear();

    // First, graph roads are found and signed on the skeleton
    findRoads(*currJSGraph);

    // Then, junction areas are identified and reconstructions are calculated
    for (j = 0; j < currJSGraph->getNodesCount(); ++j)
      if (!currJSGraph->getNode(j).hasAttribute(JointSequenceGraph::REACHED)) {
        junctionAreasList.push_back(JunctionArea());
        junctionAreasList.back().expandArea(j);
        if (!junctionAreasList.back().calculateReconstruction())
          junctionAreasList.pop_back();
      }

    // Finally, reconstructions are substituted inside the skeleton
    for (j = 0; j < junctionAreasList.size(); ++j) junctionAreasList[j].apply();
  }
}
