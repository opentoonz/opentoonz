

#include "tcenterlinevectP.h"

// tcg includes
#include "tcg/tcg_numeric_ops.h"

// Boost includes
#include <boost/container/flat_map.hpp>

namespace boost_c = boost::container;

//==========================================================================

//*************************
//*    Colors handling    *
//*************************

// Riassunto: Nel caso di normali raster, i tratti di penna sono colorati con
// l'elemento della palette data maggiormente tendente al nero.
// Per le Toonz colormap abilitiamo una gestione piu' complessa, che tiene
// conto del colore dell'inchiostro specificato direttamente nell'immagine.

// Nello specifico:
//  a) I tratti di penna vengono rilevati in base al valore del campo *tone*
//     di un TPixleCM32, non in base alla luminosita' del colore.
//     (vv. Poligonizzazione)
//  b) Sulle centerline grezze viene costruito un insieme di 'punti di assaggio'
//     dell'immagine; gli id di inchiostro rilevati vengono assegnati
//     direttamente alla stroke: se si verifica un cambio nell'id del colore,
//     il punto di cambio del colore viene identificato e la centerline viene
//     spezzata li'.
//  c) Una volta identificati i colori delle stroke, le si ordina *prima*
//     di inserirle nella vector image di output, in base al colore
//     dell'immagine
//     ai loro estremi (attualmente ordinamento solo parziale).

//--------------------------------------------------------------------------

static TPixelCM32 pixel(const TRasterCM32 &ras, int x, int y) {
  // Seems that raster access was not very much double-checked at the time
  // I wrote this. Too bad. Enforcing it now.

  return ras.pixels(tcrop(y, 0, ras.getLy() - 1))[tcrop(x, 0, ras.getLx() - 1)];
}

//--------------------------------------------------------------------------

static T3DPointD firstInkChangePosition(const TRasterCM32P &ras,
                                        const T3DPointD &start,
                                        const T3DPointD &end, int threshold) {
  double dist = norm(end - start);

  int sampleMax = tceil(dist), sampleCount = sampleMax + 1;
  double sampleMaxD = double(sampleMax);

  // Get first ink color
  int s, color = -1;

  for (s = 0; s != sampleCount; ++s) {
    T3DPointD p           = tcg::numeric_ops::lerp(start, end, s / sampleMaxD);
    const TPixelCM32 &pix = pixel(*ras, p.x, p.y);

    if (pix.getTone() < threshold) {
      color = pix.getInk();
      break;
    }
  }

  // Get second color
  for (; s != sampleCount; ++s) {
    T3DPointD p           = tcg::numeric_ops::lerp(start, end, s / sampleMaxD);
    const TPixelCM32 &pix = pixel(*ras, p.x, p.y);

    if (pix.getTone() < threshold && pix.getInk() != color) break;
  }

  // Return middle position between s-1 and s
  if (s < sampleCount)
    return tcg::numeric_ops::lerp(start, end, (s - 0.5) / sampleMaxD);

  return TConsts::nap3d;
}

//------------------------------------------------------------------------

// Find color of input sequence. Will be copied to its equivalent stroke.
// Currently in use only on colormaps

// Riassunto: Per saggiare il colore da assegnare alle strokes e' meglio
// controllare
// le sequenze *prima* di convertirle in TStroke (visto che si perde parte
// dell'aderenza originale
// al tratto). Si specifica un numero di 'punti di assaggio' della spezzata
// equidistanti tra loro,
// su cui viene prelevato il valore dell'ink del pixel corrispondente. Se si
// identifica un cambio
// di colore, viene lanciata la procedura di spezzamento della sequenza: si
// identifica il punto
// di spezzamento, e la sequenza s viene bloccata li'; si costruisce una nuova
// sequenza newSeq e
// viene rilanciata sampleColor(ras,newSeq,sOpposite). Le sequenze tra due punti
// di spezzamento
// vengono inserite nel vector 'globals->singleSequences'.
// Nel caso di sequenze circolari c'e' una piccola modifica: il primo punto di
// spezzamento
//*ridefinisce solo* il nodo-raccordo di s, senza introdurre nuove sequenze.
// La sequenza sOpposite, 'inversa' di s, rimane e diventa 'forward-oriented'
// previo aggiornamento
// della coda.
// Osservare che i nodi di spezzamento vengono inseriti con la signature
// 'SAMPLECOLOR_SIGN'.
// NOTA: La struttura a grafo J-S 'superiore' non viene alterata qui dentro.
// Eventualm. da fare fuori.

static void sampleColor(const TRasterCM32P &ras, int threshold, Sequence &seq,
                        Sequence &seqOpposite, SequenceList &singleSequences) {
  SkeletonGraph *currGraph = seq.m_graphHolder;

  // Calculate sequence parametrization
  std::vector<unsigned int> nodes;
  std::vector<double> params;

  // Meanwhile, ensure each point belong to ras. Otherwise, typically an error
  // occurred in the thinning process and it's better avoid sampling procedure. 
  // Only exception, when a point has 
  // x==ras->getLx() || y==ras->getLy(); that is accepted.
  {
    const T3DPointD &headPos = *currGraph->getNode(seq.m_head);

    if (!ras->getBounds().contains(TPoint(headPos.x, headPos.y))) {
      if (headPos.x < 0 || ras->getLx() < headPos.x || headPos.y < 0 ||
          ras->getLy() < headPos.y)
        return;
    }
  }

  unsigned int curr, currLink, next;
  double meanThickness = currGraph->getNode(seq.m_head)->z;

  params.push_back(0);
  nodes.push_back(seq.m_head);

  for (curr = seq.m_head, currLink = seq.m_headLink;
       curr != seq.m_tail || params.size() == 1; seq.next(curr, currLink)) {
    next = currGraph->getNode(curr).getLink(currLink).getNext();

    const T3DPointD &nextPos = *currGraph->getNode(next);
    if (!ras->getBounds().contains(TPoint(nextPos.x, nextPos.y))) {
      if (nextPos.x < 0 || ras->getLx() < nextPos.x || nextPos.y < 0 ||
          ras->getLy() < nextPos.y)
        return;
    }

    double distance =
        tdistance(*currGraph->getNode(next), *currGraph->getNode(curr));
    if (distance == 0.0) continue;

    params.push_back(params.back() + distance);
    nodes.push_back(next);

    meanThickness += currGraph->getNode(next)->z;
  }

  meanThickness /= params.size();

  // Exclude 0-length sequences
  if (params.back() < 0.01) {
    seq.m_color = pixel(*ras, currGraph->getNode(seq.m_head)->x,
                        currGraph->getNode(seq.m_head)->y)
                      .getInk();
    return;
  }

  // Prepare sampling procedure
  int paramCount = params.size(), paramMax = paramCount - 1;

  int sampleMax   = std::max(params.back() / std::max(meanThickness, 1.0),
                           3.0),  // Number of color samples depends on
      sampleCount = sampleMax + 1;  // the ratio params.back() / meanThickness

  std::vector<double> sampleParams(sampleCount);  // Sampling lengths
  std::vector<TPoint> samplePoints(
      sampleCount);  // Image points for color sampling
  std::vector<int> sampleSegments(
      sampleCount);  // Sequence segment index for the above

  // Sample colors
  for (int s = 0, j = 0; s != sampleCount; ++s) {
    double samplePar = params.back() * (s / double(sampleMax));

    while (j != paramMax &&
           params[j + 1] < samplePar)  // params[j] < samplePar <= params[j+1]
      ++j;

    double t = (samplePar - params[j]) / (params[j + 1] - params[j]);

    T3DPointD samplePoint(*currGraph->getNode(nodes[j]) * (1 - t) +
                          *currGraph->getNode(nodes[j + 1]) * t);

    sampleParams[s] = samplePar;
    samplePoints[s] = TPoint(
        std::min(samplePoint.x,
                 double(ras->getLx() - 1)),  // This deals with sample points at
        std::min(samplePoint.y,
                 double(ras->getLy() - 1)));  // the top/right raster border
    sampleSegments[s] = j;
  }

  // NOTE: Extremities of a sequence are considered unreliable: they typically
  // happen to be junction points shared between possibly different-colored
  // strokes.

  // Find first and last extremity-free sampled points
  T3DPointD first(*currGraph->getNode(seq.m_head));
  T3DPointD last(*currGraph->getNode(seq.m_tail));

  int i, k;

  for (i = 1;
       params.back() * i / double(sampleMax) <= first.z && i < sampleCount; ++i)
    ;
  for (k = sampleMax - 1;
       params.back() * (sampleMax - k) / double(sampleMax) <= last.z && k >= 0;
       --k)
    ;

  // Give s the first sampled ink color found

  // Initialize with a last-resort reasonable color - not just 0
  seq.m_color = seqOpposite.m_color =
      ras->pixels(samplePoints[0].y)[samplePoints[0].x].getInk();

  int l;

  for (l = i - 1; l >= 0; --l) {
    if (ras->pixels(samplePoints[l].y)[samplePoints[l].x].getTone() <
        threshold) {
      seq.m_color = seqOpposite.m_color =
          ras->pixels(samplePoints[l].y)[samplePoints[l].x].getInk();

      break;
    }
  }

  // Then, look for the first reliable ink
  for (l = i; l <= k; ++l) {
    if (ras->pixels(samplePoints[l].y)[samplePoints[l].x].getTone() <
        threshold) {
      seq.m_color = seqOpposite.m_color =
          ras->pixels(samplePoints[l].y)[samplePoints[l].x].getInk();

      break;
    }
  }

  if (i >= k)
    goto _getOut;  // No admissible segment found for splitting
                   // check.
  // Find color changes between sampled colors
  for (l = i; l < k; ++l) {
    const TPixelCM32
        &nextSample = ras->pixels(samplePoints[l + 1].y)[samplePoints[l + 1].x],
        &nextSample2 = ras->pixels(
            samplePoints[l + 2]
                .y)[samplePoints[l + 2].x];  // l < k < sampleMax - so +2 is ok

    if (nextSample.getTone() < threshold &&
        nextSample.getInk() != seq.m_color &&
        nextSample2.getTone() < threshold &&
        nextSample2.getInk() ==
            nextSample.getInk())  // Ignore single-sample color changes
    {
      // Found a color change - apply splitting procedure
      // NOTE: The function RETURNS BEFORE THE FOR IS CONTINUED!

      int nextColor = nextSample.getInk();

      // Identify split segment
      int u;

      for (u = sampleSegments[l]; u < sampleSegments[l + 1]; ++u) {
        const TPixelCM32 &pix = pixel(*ras, currGraph->getNode(nodes[u + 1])->x,
                                      currGraph->getNode(nodes[u + 1])->y);
        if (pix.getTone() < threshold && pix.getInk() != seq.m_color) break;
      }

      // Now u indicates the splitting segment. Search for splitting point by
      // binary subdivision.
      const T3DPointD &nodeStartPos = *currGraph->getNode(nodes[u]),
                      &nodeEndPos   = *currGraph->getNode(nodes[u + 1]);

      T3DPointD splitPoint =
          firstInkChangePosition(ras, nodeStartPos, nodeEndPos, threshold);

      if (splitPoint == TConsts::nap3d)
        splitPoint = 0.5 * (nodeStartPos +
                            nodeEndPos);  // A color change was found, but could
                                          // not be precisely located. Just take
                                          // a reasonable representant.
      // Insert a corresponding new node in basic graph structure.
      unsigned int splitNode = currGraph->newNode(splitPoint);

      unsigned int nodesLink =
          currGraph->getNode(nodes[u]).linkOfNode(nodes[u + 1]);
      currGraph->insert(splitNode, nodes[u], nodesLink);
      *currGraph->node(splitNode).link(0) =
          *currGraph->getNode(nodes[u]).getLink(nodesLink);

      nodesLink = currGraph->getNode(nodes[u + 1]).linkOfNode(nodes[u]);
      currGraph->insert(splitNode, nodes[u + 1], nodesLink);
      *currGraph->node(splitNode).link(1) =
          *currGraph->getNode(nodes[u + 1]).getLink(nodesLink);

      currGraph->node(splitNode).setAttribute(
          SAMPLECOLOR_SIGN);  // Sign all split-inserted nodes

      if (seq.m_head == seq.m_tail &&
          currGraph->getNode(seq.m_head).getLinksCount() == 2 &&
          !currGraph->getNode(seq.m_head).hasAttribute(SAMPLECOLOR_SIGN)) {
        // Circular case: we update s to splitNode and relaunch this very
        // procedure on it.
        seq.m_head = seq.m_tail = splitNode;
        sampleColor(ras, threshold, seq, seqOpposite, singleSequences);
      } else {
        // Update upper (Joint-Sequence) graph data
        Sequence newSeq;
        newSeq.m_graphHolder = currGraph;
        newSeq.m_head        = splitNode;
        newSeq.m_headLink    = 0;
        newSeq.m_tail        = seq.m_tail;
        newSeq.m_tailLink    = seq.m_tailLink;

        seq.m_tail     = splitNode;
        seq.m_tailLink = 1;  // (link from splitNode to nodes[u] inserted for
                             // second by 'insert')

        seqOpposite.m_graphHolder =
            seq.m_graphHolder;  // Inform that a split was found

        // NOTE: access on s terminates at newSeq's push_back, due to possible
        // reallocation of globals->singleSequences

        if ((!(seq.m_head == newSeq.m_tail &&
               currGraph->getNode(seq.m_head).getLinksCount() == 2)) &&
            currGraph->getNode(seq.m_head).hasAttribute(SAMPLECOLOR_SIGN))
          singleSequences.push_back(seq);

        sampleColor(ras, threshold, newSeq, seqOpposite, singleSequences);
      }

      return;
    }
  }

_getOut:

  // Color changes not found (and therefore no newSeq got pushed back); if a
  // split happened, update sOpposite.
  if (currGraph->getNode(seq.m_head).hasAttribute(SAMPLECOLOR_SIGN)) {
    seqOpposite.m_color    = seq.m_color;
    seqOpposite.m_head     = seq.m_tail;
    seqOpposite.m_headLink = seq.m_tailLink;
    seqOpposite.m_tail     = seq.m_head;
    seqOpposite.m_tailLink = seq.m_headLink;
  }
}

//--------------------------------------------------------------------------

// Take samples of image colors to associate each sequence to its corresponding
// palette color. Currently working on colormaps.
// void calculateSequenceColors(const TRasterP &ras)
void calculateSequenceColors(const TRasterP &ras, VectorizerCoreGlobals &g) {
  int threshold                           = g.currConfig->m_threshold;
  SequenceList &singleSequences           = g.singleSequences;
  JointSequenceGraphList &organizedGraphs = g.organizedGraphs;

  TRasterCM32P cm = ras;
  unsigned int i, j, k;
  int l;

  if (cm && g.currConfig->m_maxThickness > 0.0) {
    // singleSequence is traversed back-to-front because new, possibly splitted
    // sequences are inserted at back - and don't have to be re-sampled.
    for (l = singleSequences.size() - 1; l >= 0; --l) {
      Sequence rear;
      sampleColor(ras, threshold, singleSequences[l], rear, singleSequences);
      // If rear is built, a split occurred and the rear of this
      // single sequence has to be pushed back.
      if (rear.m_graphHolder) singleSequences.push_back(rear);
    }

    for (i = 0; i < organizedGraphs.size(); ++i)
      for (j = 0; j < organizedGraphs[i].getNodesCount(); ++j)
        if (!organizedGraphs[i].getNode(j).hasAttribute(
                JointSequenceGraph::ELIMINATED))  // due to junction recovery
          for (k = 0; k < organizedGraphs[i].getNode(j).getLinksCount(); ++k) {
            Sequence &s = *organizedGraphs[i].node(j).link(k);
            if (s.isForward() &&
                !s.m_graphHolder->getNode(s.m_tail).hasAttribute(
                    SAMPLECOLOR_SIGN)) {
              unsigned int next = organizedGraphs[i].node(j).link(k).getNext();
              unsigned int nextLink = organizedGraphs[i].tailLinkOf(j, k);

              Sequence &sOpposite =
                  *organizedGraphs[i].node(next).link(nextLink);
              sampleColor(cm, threshold, s, sOpposite, singleSequences);
            }
          }
  }
}

//==========================================================================

inline void applyStrokeIndices(VectorizerCoreGlobals *globals) {
  unsigned int i, j, k, n;
  unsigned int next, nextLink;

  for (i = 0; i < globals->singleSequences.size(); ++i)
    globals->singleSequences[i].m_strokeIndex = i;

  n = i;

  for (i = 0; i < globals->organizedGraphs.size(); ++i) {
    JointSequenceGraph *currJSGraph = &globals->organizedGraphs[i];
    for (j = 0; j < currJSGraph->getNodesCount(); ++j)
      if (!currJSGraph->getNode(j).hasAttribute(JointSequenceGraph::ELIMINATED))
        for (k = 0; k < currJSGraph->getNode(j).getLinksCount(); ++k) {
          Sequence &s = *currJSGraph->node(j).link(k);
          if (s.isForward()) {
            s.m_strokeIndex = n;

            if (!s.m_graphHolder->getNode(s.m_tail).hasAttribute(
                    SAMPLECOLOR_SIGN)) {
              next     = currJSGraph->getNode(j).getLink(k).getNext();
              nextLink = currJSGraph->tailLinkOf(j, k);
              currJSGraph->node(next).link(nextLink)->m_strokeIndex = n;
            }

            ++n;
          }
        }
  }
}

//==========================================================================

// Riassunto: Dato un grafo superiore, possiamo associare ad ogni nodo il colore
// del pixel associato a quel punto; se una sequenza e' nascosta, ha entrambi
// i nodi agli estremi di colore diverso, viceversa per sequenze esposte.
// Data una sequenza, a partire dai nodi superiori adiacenti possiamo stabilire
// un
// insieme di sequenze che gli stanno sotto, ed uno di seq. che gli stanno
// sopra.

// NOTA: Questo problema e' un caso particolare di 'graph labeling', di cui non
// ho ancora trovato soluzione. In rete qualcosa si trova...

// La seguente funzione fa qualcosa di piu' debole: ad ogni joint ed ogni
// Sequence
// viene assegnata una altezza (intero). Dato un Joint, le sequenze che lo hanno
// per estremo e che hanno lo stesso colore dell'immagine in quella posizione
// hanno
// un'altezza +1 rispetto al giunto, e viceversa altezza -1. Partendo da
// un giunto iniziale, quest'informazione viene propagata sul grafo; il problema
// sta ritornando ai giunti gia' percorsi...

//--------------------------------------------------------------------------

// Find predominant ink color in a circle of given radius and center
static int getInkPredominance(const TRasterCM32P &ras, TPalette *palette, int x,
                              int y, int radius, int threshold) {
  int i, j;
  int mx, my, Mx, My;
  std::vector<int> inksFound(palette->getStyleCount());

  radius = std::min(
      radius, 7);  // Restrict radius for a minimum significative neighbour

  mx = std::max(x - radius, 0);
  my = std::max(y - radius, 0);
  Mx = std::min(x + radius, ras->getLx() - 1);
  My = std::min(y + radius, ras->getLy() - 1);

  // Check square grid around (x,y)
  for (i = mx; i <= Mx; ++i)
    for (j = my; j <= My; ++j)
      if (sq(i) + sq(j) <= sq(radius) &&
          ras->pixels(j)[i].getTone() < threshold) {
        // Update color table
        inksFound[ras->pixels(j)[i].getInk()] +=
            255 - ras->pixels(j)[i].getTone();
      }

  // return the most found ink
  int maxCount = 0, mostFound = 0;
  for (i = 0; i < (int)inksFound.size(); ++i)
    if (inksFound[i] > maxCount) {
      maxCount  = inksFound[i];
      mostFound = i;
    }

  return mostFound;
}

//--------------------------------------------------------------------------

/*!
  \brief    Find the predominant color in sequences adjacent to the
            input graph node.
  \return   The predominant branch color if found, \p -1 otherwise.
*/
static int getBranchPredominance(const TRasterCM32P &ras, TPalette *palette,
                                 JointSequenceGraph::Node &node) {
  boost_c::flat_map<int, int> branchInksHistogram;

  UINT l, lCount = node.getLinksCount();

  for (l = 0; l != lCount; ++l) {
    int color = node.getLink(l)->m_color;
    if (color >= 0 && color <= palette->getStyleCount())
      ++branchInksHistogram[color];
  }

  // Return the most found ink, or -1 if a predominance color could not be found
  if (branchInksHistogram.empty()) return -1;

  typedef boost_c::flat_map<int, int>::iterator histo_it;

  const std::pair<histo_it, histo_it> &histoRange = std::minmax_element(
      branchInksHistogram.begin(), branchInksHistogram.end(),
      [](const std::pair<int, int> &a, const std::pair<int, int> &b) {
        return a.second < b.second;
      });

  return (histoRange.first->second == histoRange.second->second)
             ? -1
             : histoRange.second->first;
}

//--------------------------------------------------------------------------

// NOTA: Da implementare una versione in grado di ordinare *pienamente* la
// vector image.
static void sortJS(JointSequenceGraph *js,
                   std::vector<std::pair<int, TStroke *>> &toOrder,
                   const TRasterCM32P &ras, TPalette *palette) {
  enum { SORTED = 0x10 };

  std::vector<std::pair<unsigned int, int>> nodesToDo;
  unsigned int currNodeIdx, nextNodeIdx;
  int currColor, currHeight, nextColor, nextHeight;
  T3DPointD pD;
  TPoint p;

  SkeletonGraph *currGraph = js->getNode(0).getLink(0)->m_graphHolder;

  unsigned int n, nCount = js->getNodesCount();
  for (n = 0; n != nCount; ++n) {
    // Get the first non-ELIMINATED and non-already treated JS node
    if (!js->getNode(n).hasAttribute(JointSequenceGraph::ELIMINATED | SORTED)) {
      nodesToDo.push_back(std::make_pair(n, 0));

      while (!nodesToDo.empty()) {
        currNodeIdx = nodesToDo.back().first;
        currHeight  = nodesToDo.back().second;
        nodesToDo.pop_back();

        JointSequenceGraph::Node &currNode = js->node(currNodeIdx);

        // Sign current node
        currNode.setAttribute(SORTED);

        // Initialize this node infos
        pD = *currGraph->getNode(currNode.getLink(0)->m_head);
        p  = TPoint(pD.x, pD.y);

        if (!ras->getBounds().contains(p)) continue;

        // currColor = getInkPredominance(ras, palette, p.x, p.y, (int) pD.z);
        // //ras->pixels(p.y)[p.x].getInk();
        currColor = getBranchPredominance(ras, palette, currNode);
        if (currColor < 0) currColor = ras->pixels(p.y)[p.x].getInk();

        int l, lCount = currNode.getLinksCount();
        for (l = 0; l != lCount; ++l) {
          nextNodeIdx = currNode.getLink(l).getNext();
          Sequence &s = *currNode.link(l);

          // Check if outgoing sequence has current color (front) or not (back)
          toOrder[s.m_strokeIndex].first =
              (s.m_color == currColor) ? currHeight : currHeight - 1;

          if (!(currNode.getLink(l).getAccess() == SORTED)) {
            // Deal with this unchecked branch

            // If sequence was not split (due to color change)
            if (!currGraph->getNode(s.m_tail).hasAttribute(SAMPLECOLOR_SIGN)) {
              JointSequenceGraph::Node &nextNode = js->node(nextNodeIdx);

              // Then check nextNode
              pD = *currGraph->getNode(nextNode.getLink(0)->m_head);
              p  = TPoint(pD.x, pD.y);

              if (!ras->getBounds().contains(p)) continue;

              // If nextNode was not already inserted in ToDo vector, do it now.
              if (!nextNode.hasAttribute(SORTED)) {
                // nextColor = getInkPredominance(ras, palette, p.x, p.y, (int)
                // pD.z);
                nextColor = getBranchPredominance(ras, palette, nextNode);
                if (nextColor < 0) nextColor = ras->pixels(p.y)[p.x].getInk();

                nextHeight = (s.m_color == nextColor)
                                 ? toOrder[s.m_strokeIndex].first
                                 : toOrder[s.m_strokeIndex].first + 1;

                nodesToDo.push_back(std::make_pair(nextNodeIdx, nextHeight));
              }

              // Deny access to its inverse (already processed now)
              nextNode.link(js->tailLinkOf(currNodeIdx, l)).setAccess(SORTED);
            }
          }
        }
      }
    }
  }
}

//--------------------------------------------------------------------------

inline void orderColoredStrokes(JointSequenceGraphList &organizedGraphs,
                                std::vector<TStroke *> &strokes,
                                const TRasterCM32P &ras, TPalette *palette) {
  // Initialize ordering
  std::vector<std::pair<int, TStroke *>> strokesByHeight(
      strokes.size(),
      std::make_pair(-(std::numeric_limits<int>::max)(), (TStroke *)0));

  size_t s, sCount = strokes.size();
  for (s = 0; s != sCount; ++s) strokesByHeight[s].second = strokes[s];

  size_t og, ogCount = organizedGraphs.size();
  for (og = 0; og != ogCount; ++og)
    sortJS(&organizedGraphs[og], strokesByHeight, ras, palette);

  // Now, we have the order vector filled, apply sorting algorithm.
  std::sort(strokesByHeight.begin(), strokesByHeight.end());

  for (s = 0; s != sCount; ++s) strokes[s] = strokesByHeight[s].second;
}

//==========================================================================

// Take samples of image colors to associate each stroke to its corresponding
// palette color. Currently working on colormaps, closest-to-black strokes
// otherwise.
void applyStrokeColors(std::vector<TStroke *> &strokes, const TRasterP &ras,
                       TPalette *palette, VectorizerCoreGlobals &g) {
  JointSequenceGraphList &organizedGraphs = g.organizedGraphs;
  SequenceList &singleSequences           = g.singleSequences;

  TRasterCM32P cm = ras;
  unsigned int i, j, k, n;

  if (cm && g.currConfig->m_maxThickness > 0.0) {
    applyStrokeIndices(&g);

    // Treat single sequences before, like conversionToStrokes(..)
    for (i = 0; i < singleSequences.size(); ++i)
      strokes[i]->setStyle(singleSequences[i].m_color);

    // Then, treat remaining graph-strokes
    n = i;

    for (i = 0; i < organizedGraphs.size(); ++i)
      for (j = 0; j < organizedGraphs[i].getNodesCount(); ++j)
        if (!organizedGraphs[i].getNode(j).hasAttribute(
                JointSequenceGraph::ELIMINATED))  // due to junction recovery
          for (k = 0; k < organizedGraphs[i].getNode(j).getLinksCount(); ++k) {
            Sequence &s = *organizedGraphs[i].node(j).link(k);
            if (s.isForward()) {
              // vi->getStroke(n)->setStyle(s.m_color);
              strokes[n]->setStyle(s.m_color);
              ++n;
            }
          }

    // Order vector image according to actual color-coverings at junctions.
    orderColoredStrokes(organizedGraphs, strokes, cm, palette);
  } else {
    // Choose closest-to-black palette color
    int blackStyleId = palette->getClosestStyle(TPixel32::Black);

    unsigned int i;
    for (i = 0; i < strokes.size(); ++i) strokes[i]->setStyle(blackStyleId);
  }
}
