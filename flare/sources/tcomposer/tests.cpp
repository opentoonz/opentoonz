

#include "tsystem.h"
#include "tenv.h"
#include "tproject.h"
#include "tconvert.h"
#include <iostream.h>

TEnv::RootSystemVar systemVar(
    TFilePath("SOFTWARE\\Digital Video\\Toonz\\5.0\\ToonzRoot"));

ostream &operator<<(ostream &out, TProject *project) {
  if (project == 0) {
    return out << "<null>";
  }
  out << "'" << toString(project->getName().getWideString()) << "'";
  out << " {" << toString(project->getPath().getWideString()) << "}";
  return out;
}
extern void test();

int main() {
  /*
cout << "current: " << TProject::getCurrent() << endl;
TProjectManager *m = TProjectManager::instance();
for(int i=0;i<m->getProjectCount();i++)
{
cout << "  " << m->getProject(i) << endl;
}
TProject *project = new TProject(TFilePath("funziona"));
project->setFolder(TProject::Extras, TFilePath("C:\\butta"));
m->addProject(project);
*/

  test();
  return 0;
}
