

#include "ext/CompositeStatus.h"

#include "tgeometry.h"
#include "tvectorimage.h"

#include "ext/Types.h"
#include "ext/StrokeParametricDeformer.h"

#include <vector>

namespace ToonzExt {

//---------------------------------------------------------------------------
/**
   *@class CompositeStatus
   *@brief This class mantains interal data for Dragger manipulator.
   */
CompositeStatus::CompositeStatus()
    : dbImpl_(new std::map<std::string, CompositeStatus *>), db_(*dbImpl_) {}

//---------------------------------------------------------------------------

CompositeStatus::~CompositeStatus() {
  iterator it = db_.begin(), end = db_.end();

  while (it != end) {
    delete it->second;
    ++it;
  }

  delete dbImpl_;
}

//---------------------------------------------------------------------------

void CompositeStatus::add(CompositeStatus *status, const std::string &name) {
  if (!status) return;

  CompositeStatus *tmp = this->find(name);

  if (tmp) delete tmp;

  db_[name] = status;
}

//---------------------------------------------------------------------------

void CompositeStatus::remove(const std::string &name) {
  iterator found = db_.find(name), end = db_.end();

  if (found != end) {
    delete found->second;
    db_.erase(found);
  }
}

//---------------------------------------------------------------------------

CompositeStatus *CompositeStatus::find(const std::string &name) const {
  const_iterator found = db_.find(name), end = db_.end();

  if (found != end) return found->second;

  return 0;
}

//---------------------------------------------------------------------------
}
