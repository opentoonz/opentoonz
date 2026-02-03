

#include "toonz/observer.h"
// OBSOLETO??

TNotifier *TNotifier::instance() {
  static TNotifier theNotifier;
  return &theNotifier;
}

/*
void TNotifier::attach(TChangeObserver*observer)
{
  std::vector<TObserverList*>::iterator it;
  for(it = m_obsList.begin(); it != m_obsList.end(); ++it)
    (*it)->attach(observer);
  if(TGlobalObserver *go = dynamic_cast<TGlobalObserver *>(observer))
    {
     if(m_newSceneNotifiedObs.find(go) == m_newSceneNotifiedObs.end())
       {
        go->update(TGlobalChange(true));
        m_newSceneNotifiedObs.insert(go);
       }
    }
}


void TNotifier::detach(TChangeObserver*observer)
{
  std::vector<TObserverList*>::iterator it;
  for(it = m_obsList.begin(); it != m_obsList.end(); ++it)
    (*it)->detach(observer);
}
*/

void TNotifier::notify(const TGlobalChange &c) {
  m_globalObs.notify(c);
  if (c.isSceneChanged()) {
    m_newSceneNotifiedObs.clear();
    for (int i = 0; i < (int)m_globalObs.m_observers.size(); i++)
      m_newSceneNotifiedObs.insert(m_globalObs.m_observers[i]);
  }
}
