

#include "toonz/tvectorimageutils.h"
#include "tpalette.h"

//-------------------------------------------------------------------------------------

void getGroupsList(const TVectorImageP &vi, std::vector<TVectorImageP> &list) {
  // Scan vi's strokes
  unsigned int i, j, strokeCount = vi->getStrokeCount();
  for (i = 0; i < strokeCount;) {
    std::vector<int> indexes;
    // Find the group interval
    for (j = i;
         j < strokeCount && vi->areDifferentGroup(i, false, j, false) == -1;
         ++j)
      indexes.push_back(j);

    // Fill a new list item with the strokes
    TVectorImageP item(vi->splitImage(indexes, false));
    if (item->getPalette() == 0) item->setPalette(new TPalette);
    list.push_back(item);

    i = j;
  }
}
