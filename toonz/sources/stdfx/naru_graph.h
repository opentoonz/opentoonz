// https://github.com/patmjen/maxflow_algorithms

#ifndef REIMPLS_GRAPH2_H__
#define REIMPLS_GRAPH2_H__
#ifndef REIMPLS_PACKED
#define REIMPLS_PACKED
#endif
#include <vector>
#include <deque>
#include <cinttypes>
#include <cassert>
#include <algorithm>
#include <type_traits>

namespace reimpls {

using Time = uint32_t;
using Dist = uint16_t;

template <class Cap, class Term, class Flow, class ArcIdx = int32_t,
          class NodeIdx = int32_t>
class Graph2 {
  static_assert(std::is_integral<ArcIdx>::value,
                "ArcIdx must be an integer type");
  static_assert(std::is_integral<NodeIdx>::value,
                "NodeIdx must be an integer type");
  static_assert(std::is_signed<Term>::value, "Term must be a signed type");

  // Forward decls.
  struct Node;
  struct Arc;

public:
  static const NodeIdx INVALID_NODE =
      ~NodeIdx(0);  // -1 for signed type, max. value for unsigned type
  static const ArcIdx INVALID_ARC =
      ~ArcIdx(0);  // -1 for signed type, max. value for unsigned type
  static const ArcIdx TERMINAL_ARC = INVALID_ARC - 1;
  static const ArcIdx ORPHAN_ARC   = INVALID_ARC - 2;

  enum TermType : int32_t { SOURCE = 0, SINK = 1 };

  Graph2(size_t expected_nodes, size_t expected_arcs);

  NodeIdx add_node(size_t num = 1);

  void add_tweights(NodeIdx i, Term cap_source, Term cap_sink);

  void add_edge(NodeIdx i, NodeIdx j, Cap cap, Cap rev_cap,
                bool merge_duplicates = true);

  void init_maxflow();
  Flow maxflow();

  TermType what_segment(NodeIdx i, TermType default_segment = SOURCE) const;

  inline size_t get_node_num() const noexcept { return nodes.size() - 1; }
  inline size_t get_arc_num() const noexcept { return arcs.size(); }

private:
  std::vector<Node> nodes;
  std::vector<Arc> arcs;
  std::vector<Arc> arc_buffer;

  Flow flow;
  int32_t maxflow_iteration;

  NodeIdx first_active, last_active;
  std::deque<NodeIdx> orphan_nodes;

  Time time;

#pragma pack(1)
  struct REIMPLS_PACKED Node {
    ArcIdx first;         // First out-going arc.
    ArcIdx parent;        // Arc to parent node in search tree
    NodeIdx next_active;  // Index of next active node (or itself if this is the
                          // last one)

    Time timestamp;  // Timestamp showing when dist was computed.
    Dist dist;       // Distance to terminal.

    Term tr_cap;  // If tr_cap > 0 then tr_cap is residual capacity of the arc
                  // SOURCE->node. Otherwise         -tr_cap is residual
                  // capacity of the arc node->SINK.

    bool is_sink : 1;  // flag showing if the node is in the source or sink tree
                       // (if parent!=NULL)

    Node()
        : first(INVALID_ARC)
        , parent(INVALID_ARC)
        , next_active(INVALID_NODE)
        , timestamp(0)
        , dist(0)
        , tr_cap(0)
        , is_sink(false) {}
  };

  struct REIMPLS_PACKED Arc {
    ArcIdx sister;  // Sister arc
    NodeIdx head;   // Node this arc points to.

    Cap r_cap;  // Residual capacity

    bool sister_sat;  // Is the sister arc saturated

    Arc() {}

    Arc(NodeIdx head, ArcIdx next, Cap r_cap, bool sister_sat)
        : sister(next), head(head), r_cap(r_cap), sister_sat(sister_sat) {}
  };
#pragma pack()

  void add_half_edge(NodeIdx from, NodeIdx to, Cap cap, Cap rev_cap,
                     bool merge_duplicates = true);

  void make_active(NodeIdx i);
  void make_front_orphan(NodeIdx i);
  void make_back_orphan(NodeIdx i);

  NodeIdx next_active();

  void augment(ArcIdx middle);
  Term tree_bottleneck(NodeIdx start, bool source_tree) const;
  void augment_tree(NodeIdx start, Term bottleneck, bool source_tree);

  ArcIdx grow_search_tree(NodeIdx start);
  template <bool source>
  ArcIdx grow_search_tree_impl(NodeIdx start);

  void process_orphan(NodeIdx i);
  template <bool source>
  void process_orphan_impl(NodeIdx i);

  /*inline ArcIdx sister_idx(ArcIdx a) const noexcept { return a ^ 1; }
  inline Arc& sister(ArcIdx a) { return arcs[sister_idx(a)]; }
  inline const Arc& sister(ArcIdx a) const { return arcs[sister_idx(a)]; }

  inline ArcIdx sister_or_arc_idx(ArcIdx a, bool sis) const noexcept { return a
  ^ (ArcIdx)sis; } inline Arc& sister_or_arc(ArcIdx a, bool sis) { return
  arcs[sister_or_arc_idx(a, sis)]; } inline const Arc& sister_or_arc(ArcIdx a,
  bool sis) const { return arcs[sister_or_arc_idx(a, sis)]; }*/

  Node& head_node(const Arc& a) { return nodes[static_cast<size_t>(a.head)]; }
  Node& head_node(ArcIdx a) { return head_node(arcs[a]); }
  const Node& head_node(const Arc& a) const { return nodes[a.head]; }
  const Node& head_node(ArcIdx a) const { return head_node(arcs[a]); }
};

template <class Cap, class Term, class Flow, class ArcIdx, class NodeIdx>
Graph2<Cap, Term, Flow, ArcIdx, NodeIdx>::Graph2(size_t expected_nodes,
                                                 size_t expected_arcs)
    : nodes()
    , arcs()
    , arc_buffer()
    , flow(0)
    , maxflow_iteration(0)
    , first_active(INVALID_NODE)
    , last_active(INVALID_NODE)
    , orphan_nodes()
    , time(0) {
  nodes.reserve(expected_nodes + 1);
  nodes.resize(1);  // Make room for sentinel node now
  arcs.reserve(2 * expected_arcs);
  arc_buffer.reserve(2 * expected_arcs);
}

template <class Cap, class Term, class Flow, class ArcIdx, class NodeIdx>
inline NodeIdx Graph2<Cap, Term, Flow, ArcIdx, NodeIdx>::add_node(size_t num) {
  NodeIdx crnt = nodes.size();

#ifndef REIMPLS_NO_OVERFLOW_CHECKS
  if (crnt > std::numeric_limits<NodeIdx>::max() - num) {
    throw std::overflow_error(
        "Node count exceeds capacity of index type. "
        "Please increase capacity of NodeIdx type.");
  }
#endif

  nodes.resize(crnt + num);
  return crnt;
}

template <class Cap, class Term, class Flow, class ArcIdx, class NodeIdx>
inline void Graph2<Cap, Term, Flow, ArcIdx, NodeIdx>::add_tweights(
    NodeIdx i, Term cap_source, Term cap_sink) {
  assert(i >= 0 && i < nodes.size());
  Term delta = nodes[i].tr_cap;
  if (delta > 0) {
    cap_source += delta;
  } else {
    cap_sink -= delta;
  }
  flow += std::min(cap_source, cap_sink);
  nodes[i].tr_cap = cap_source - cap_sink;
}

template <class Cap, class Term, class Flow, class ArcIdx, class NodeIdx>
inline void Graph2<Cap, Term, Flow, ArcIdx, NodeIdx>::add_edge(
    NodeIdx i, NodeIdx j, Cap cap, Cap rev_cap, bool merge_duplicates) {
  assert(i >= 0 && i < nodes.size());
  assert(j >= 0 && j < nodes.size());
  assert(i != j);
  assert(cap >= 0);
  assert(rev_cap >= 0);

  if (merge_duplicates && cap == 0 && rev_cap == 0) {
    return;
  }

#ifndef REIMPLS_NO_OVERFLOW_CHECKS
  if (arcs.size() > std::numeric_limits<ArcIdx>::max() - 2) {
    throw std::overflow_error(
        "Arc count exceeds capacity of index type. "
        "Please increase capacity of ArcIdx type.");
  }
#endif

  add_half_edge(i, j, cap, rev_cap, merge_duplicates);
  add_half_edge(j, i, rev_cap, cap, merge_duplicates);
}

template <class Cap, class Term, class Flow, class ArcIdx, class NodeIdx>
inline void Graph2<Cap, Term, Flow, ArcIdx, NodeIdx>::add_half_edge(
    NodeIdx from, NodeIdx to, Cap cap, Cap rev_cap, bool merge_duplicates) {
  ArcIdx ai;

  if (merge_duplicates) {
    // Search for existing arcs between nodes.
    ai = nodes[from].first;
    while (ai != INVALID_ARC) {
      Arc& arc = arcs[ai];
      if (arc.head == to) {
        // Existing arc found, adding capacity.
        arc.r_cap += cap;
        return;
      }
      ai = arc.sister;
    }
  }

  // Create new arc.
  ai = arcs.size();
  arcs.emplace_back(to, nodes[from].first, cap, rev_cap == 0);
  nodes[from].first = ai;
}

template <class Cap, class Term, class Flow, class ArcIdx, class NodeIdx>
inline typename Graph2<Cap, Term, Flow, ArcIdx, NodeIdx>::TermType
Graph2<Cap, Term, Flow, ArcIdx, NodeIdx>::what_segment(
    NodeIdx i, TermType default_segment) const {
  if (nodes[i].parent != INVALID_ARC) {
    return (nodes[i].is_sink) ? SINK : SOURCE;
  } else {
    return default_segment;
  }
}

template <class Cap, class Term, class Flow, class ArcIdx, class NodeIdx>
inline Flow Graph2<Cap, Term, Flow, ArcIdx, NodeIdx>::maxflow() {
  // init_maxflow();

  NodeIdx crnt_node = INVALID_NODE;

  // main loop
  while (true) {
    NodeIdx i = crnt_node;

    // Check if we are already exploring a valid active node
    if (i != INVALID_NODE) {
      Node& n       = nodes[i];
      n.next_active = INVALID_NODE;
      if (n.parent == INVALID_ARC) {
        // Active node was not valid so don't explore after all
        i = INVALID_NODE;
      }
    }

    // If we are not already exploring a node try to get a new one
    if (i == INVALID_NODE) {
      i = next_active();
      if (i == INVALID_NODE) {
        // No more nodes to explore so we are done
        break;
      }
    }

    // At this point i must point to a valid active node
    ArcIdx source_sink_connector = grow_search_tree(i);

#ifndef REIMPLS_NO_OVERFLOW_CHECKS
    // Check for overflow in time variable.
    if (time == std::numeric_limits<Time>::max()) {
      throw std::overflow_error(
          "Overflow in 'time' variable. Please increase capacity of Time "
          "type.");
    }
#endif
    time++;

    if (source_sink_connector != INVALID_ARC) {
      // Growth was aborted because we found a node from the other tree
      nodes[i].next_active = i;  // Mark as active
      crnt_node            = i;

      augment(source_sink_connector);

      std::deque<NodeIdx> crnt_orphan_nodes =
          std::move(orphan_nodes);  // Snapshot of current ophans
      for (NodeIdx orphan : crnt_orphan_nodes) {
        process_orphan(orphan);
        // If any additional orphans were added during processing, we process
        // them immediately This leads to a significant decrease of the overall
        // runtime
        while (!orphan_nodes.empty()) {
          NodeIdx o = orphan_nodes.front();
          orphan_nodes.pop_front();
          process_orphan(o);
        }
      }
    } else {
      crnt_node = INVALID_NODE;
    }
  }

  maxflow_iteration++;
  return flow;
}

template <class Cap, class Term, class Flow, class ArcIdx, class NodeIdx>
inline void Graph2<Cap, Term, Flow, ArcIdx, NodeIdx>::init_maxflow() {
  first_active = INVALID_NODE;
  last_active  = INVALID_NODE;
  orphan_nodes.clear();
  time = 0;

  // Reorder arcs so outgoing arcs for a node are consecutive
  ArcIdx crnt = 0;
  arc_buffer.resize(arcs.size());
  for (size_t i = 0; i < nodes.size() - 1; ++i) {
    ArcIdx new_first = crnt;
    ArcIdx next;
    for (ArcIdx a = nodes[i].first; a != INVALID_ARC; a = next) {
      next                        = arcs[a].sister;
      arcs[a].sister              = crnt;
      arc_buffer[crnt].head       = arcs[a].head;
      arc_buffer[crnt].sister     = a;
      arc_buffer[crnt].r_cap      = arcs[a].r_cap;
      arc_buffer[crnt].sister_sat = arcs[a].sister_sat;
      ++crnt;
    }
    nodes[i].first = new_first;
  }

  // Set sentinel node
  nodes[nodes.size() - 1].first = crnt;

  // Adjust sister indices
  for (auto& arc : arc_buffer) {
    arc.sister = arcs[arc.sister ^ 1].sister;
  }

  // Swap buffer
  std::swap(arcs, arc_buffer);

  // Init nodes and make relevant ones active
  for (size_t i = 0; i < nodes.size() - 1; ++i) {
    Node& n       = nodes[i];
    n.next_active = INVALID_NODE;
    n.timestamp   = time;
    if (n.tr_cap != 0) {
      // n is connected to the source or sink
      n.is_sink = n.tr_cap < 0;  // negative capacity goes to sink
      n.parent  = TERMINAL_ARC;
      n.dist    = 1;
      make_active(i);
    } else {
      n.parent = INVALID_ARC;
    }
  }
}

template <class Cap, class Term, class Flow, class ArcIdx, class NodeIdx>
inline void Graph2<Cap, Term, Flow, ArcIdx, NodeIdx>::make_active(NodeIdx i) {
  if (nodes[i].next_active == INVALID_NODE) {
    // It's not in the active list yet
    if (last_active != INVALID_NODE) {
      nodes[last_active].next_active = i;
    } else {
      first_active = i;
    }
    last_active          = i;
    nodes[i].next_active = i;
  }
}

template <class Cap, class Term, class Flow, class ArcIdx, class NodeIdx>
inline void Graph2<Cap, Term, Flow, ArcIdx, NodeIdx>::make_front_orphan(
    NodeIdx i) {
  nodes[i].parent = ORPHAN_ARC;
  orphan_nodes.push_front(i);
}

template <class Cap, class Term, class Flow, class ArcIdx, class NodeIdx>
inline void Graph2<Cap, Term, Flow, ArcIdx, NodeIdx>::make_back_orphan(
    NodeIdx i) {
  nodes[i].parent = ORPHAN_ARC;
  orphan_nodes.push_back(i);
}

template <class Cap, class Term, class Flow, class ArcIdx, class NodeIdx>
inline NodeIdx Graph2<Cap, Term, Flow, ArcIdx, NodeIdx>::next_active() {
  NodeIdx i;
  // Pop nodes from the active list until we find a valid one or run out of
  // nodes
  for (i = first_active; i != INVALID_NODE; i = first_active) {
    // Pop node from active list
    Node& n = nodes[i];
    if (n.next_active == i) {
      // This is the last node in the active list so "clear" it
      first_active = INVALID_NODE;
      last_active  = INVALID_NODE;
    } else {
      first_active = n.next_active;
    }
    n.next_active = INVALID_NODE;  // Mark as not active

    if (n.parent != INVALID_ARC) {
      // Valid active node found
      break;
    }
  }
  return i;
}

template <class Cap, class Term, class Flow, class ArcIdx, class NodeIdx>
inline void Graph2<Cap, Term, Flow, ArcIdx, NodeIdx>::augment(
    ArcIdx middle_idx) {
  Arc& middle        = arcs[middle_idx];
  Arc& middle_sister = arcs[middle.sister];
  // Step 1: Find bottleneck capacity
  Term bottleneck = middle.r_cap;
  bottleneck = std::min(bottleneck, tree_bottleneck(middle_sister.head, true));
  bottleneck = std::min(bottleneck, tree_bottleneck(middle.head, false));

  // Step  2: Augment along source and sink tree
  middle_sister.r_cap += bottleneck;
  middle.r_cap -= bottleneck;
  middle_sister.sister_sat = middle.r_cap == 0;
  middle.sister_sat        = middle_sister.r_cap == 0;
  augment_tree(middle_sister.head, bottleneck, true);
  augment_tree(middle.head, bottleneck, false);

  // Step 3: Add bottleneck to overall flow
  flow += bottleneck;
}

template <class Cap, class Term, class Flow, class ArcIdx, class NodeIdx>
inline Term Graph2<Cap, Term, Flow, ArcIdx, NodeIdx>::tree_bottleneck(
    NodeIdx start, bool source_tree) const {
  NodeIdx i       = start;
  Term bottleneck = std::numeric_limits<Term>::max();
  while (true) {
    ArcIdx a = nodes[i].parent;
    if (a == TERMINAL_ARC) {
      break;
    }
    Cap r_cap  = source_tree ? arcs[arcs[a].sister].r_cap : arcs[a].r_cap;
    bottleneck = std::min<Term>(bottleneck, r_cap);
    i          = arcs[a].head;
  }
  const Term tr_cap = nodes[i].tr_cap;
  return std::min<Term>(bottleneck, source_tree ? tr_cap : -tr_cap);
}

template <class Cap, class Term, class Flow, class ArcIdx, class NodeIdx>
inline void Graph2<Cap, Term, Flow, ArcIdx, NodeIdx>::augment_tree(
    NodeIdx start, Term bottleneck, bool source_tree) {
  NodeIdx i = start;
  while (true) {
    ArcIdx ai = nodes[i].parent;
    if (ai == TERMINAL_ARC) {
      break;
    }
    Arc& a = source_tree ? arcs[ai] : arcs[arcs[ai].sister];
    Arc& b = source_tree ? arcs[arcs[ai].sister] : arcs[ai];
    a.r_cap += bottleneck;
    b.r_cap -= bottleneck;
    a.sister_sat = b.r_cap == 0;
    b.sister_sat = a.r_cap == 0;
    if (b.r_cap == 0) {
      make_front_orphan(i);
    }
    i = arcs[ai].head;
  }
  nodes[i].tr_cap += source_tree ? -bottleneck : bottleneck;
  if (nodes[i].tr_cap == 0) {
    make_front_orphan(i);
  }
}

template <class Cap, class Term, class Flow, class ArcIdx, class NodeIdx>
inline ArcIdx Graph2<Cap, Term, Flow, ArcIdx, NodeIdx>::grow_search_tree(
    NodeIdx start) {
  return nodes[start].is_sink ? grow_search_tree_impl<false>(start)
                              : grow_search_tree_impl<true>(start);
}

template <class Cap, class Term, class Flow, class ArcIdx, class NodeIdx>
template <bool source>
inline ArcIdx Graph2<Cap, Term, Flow, ArcIdx, NodeIdx>::grow_search_tree_impl(
    NodeIdx start_idx) {
  const Node& start = nodes[start_idx];
  const Node& next  = nodes[start_idx + 1];
  // Add neighbor nodes search tree until we find a node from the other search
  // tree or run out of neighbors
  for (ArcIdx ai = start.first; ai != next.first; ++ai) {
    if (source ? arcs[ai].r_cap : !arcs[ai].sister_sat) {
      Node& n = head_node(ai);
      if (n.parent == INVALID_ARC) {
        // This node is not yet in a tree so claim it for this one
        n.is_sink   = !source;
        n.parent    = arcs[ai].sister;
        n.timestamp = start.timestamp;
        n.dist      = start.dist + 1;
        make_active(arcs[ai].head);
      } else if (n.is_sink == source) {
        // Found a node from the other search tree so abort
        if (!source) {
          // If we are growing the sink tree we instead return the sister arc
          ai = arcs[ai].sister;
        }
        return ai;
      } else if (n.timestamp <= start.timestamp && n.dist > start.dist) {
        // Heuristic: trying to make the distance from j to the source/sink
        // shorter
        n.parent    = arcs[ai].sister;
        n.timestamp = n.timestamp;
        n.dist      = start.dist + 1;
      }
    }
  }
  return INVALID_ARC;
}

template <class Cap, class Term, class Flow, class ArcIdx, class NodeIdx>
inline void Graph2<Cap, Term, Flow, ArcIdx, NodeIdx>::process_orphan(
    NodeIdx i) {
  if (nodes[i].is_sink) {
    process_orphan_impl<false>(i);
  } else {
    process_orphan_impl<true>(i);
  }
}

template <class Cap, class Term, class Flow, class ArcIdx, class NodeIdx>
template <bool source>
inline void Graph2<Cap, Term, Flow, ArcIdx, NodeIdx>::process_orphan_impl(
    NodeIdx i) {
  Node& n                       = nodes[i];
  Node& next                    = nodes[i + 1];
  static const int32_t INF_DIST = std::numeric_limits<int32_t>::max();
  int32_t min_d                 = INF_DIST;
  ArcIdx min_a0                 = INVALID_ARC;
  // Try to find a new parent
  for (ArcIdx a0 = n.first; a0 != next.first; ++a0) {
    if (source ? !arcs[a0].sister_sat : arcs[a0].r_cap) {
      NodeIdx j = arcs[a0].head;
      ArcIdx a  = nodes[j].parent;
      if (nodes[j].is_sink != source && a != INVALID_ARC) {
        // Check origin of m
        int32_t d = 0;
        while (true) {
          Node& m = nodes[j];
          if (m.timestamp == time) {
            d += m.dist;
            break;
          }
          a = m.parent;
          d++;
          if (a == TERMINAL_ARC) {
            m.timestamp = time;
            m.dist      = 1;
            break;
          }
          if (a == ORPHAN_ARC) {
            d = INF_DIST;  // infinite distance
            break;
          }
          j = arcs[a].head;
        }
        if (d < INF_DIST) {
          // m originates from the source
          if (d < min_d) {
            min_a0 = a0;
            min_d  = d;
          }
          // Set marks along the path
          j = arcs[a0].head;
          while (nodes[j].timestamp != time) {
            Node& m     = nodes[j];
            m.timestamp = time;
            m.dist      = d--;
            j           = arcs[m.parent].head;
          }
        }
      }
    }
  }
  n.parent = min_a0;
  if (min_a0 != INVALID_ARC) {
    n.timestamp = time;
    n.dist      = min_d + 1;
  } else {
    // No parent was found so process neighbors
    for (ArcIdx a0 = n.first; a0 != next.first; ++a0) {
      NodeIdx j = arcs[a0].head;
      Node& m   = nodes[j];
      if (m.is_sink != source && m.parent != INVALID_ARC) {
        if (source ? !arcs[a0].sister_sat : arcs[a0].r_cap) {
          make_active(j);
        }
        ArcIdx pa = m.parent;
        if (pa != TERMINAL_ARC && pa != ORPHAN_ARC && arcs[pa].head == i) {
          make_back_orphan(j);
        }
      }
    }
  }
}

}  // namespace reimpls

#endif  // REIMPLS_GRAPH_H__