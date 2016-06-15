

#include "permissionsmanager.h"

// TnzBase includes
#include "tenv.h"

// TnzCore includes
#include "tsystem.h"
#include "tstream.h"

//=========================================================

namespace {

class User {
public:
  std::string m_name;
  std::vector<std::string> m_svnUsernames;
  std::vector<std::string> m_svnPasswords;
  User(std::string name) : m_name(name) {}

  void addSvnUsername(std::string username) {
    m_svnUsernames.push_back(username);
  }
  void addSvnPassword(std::string password) {
    m_svnPasswords.push_back(password);
  }

  std::string getSvnUsername(int index) {
    if (index < 0 || index >= (int)m_svnUsernames.size()) return "";
    return m_svnUsernames.at(index);
  }

  std::string getSvnPassword(int index) {
    if (index < 0 || index >= (int)m_svnPasswords.size()) return "";
    return m_svnPasswords.at(index);
  }
};

}  // namespace

//---------------------------------------------------------

class PermissionsManager::Imp {
public:
  std::map<std::string, User *> m_users;

  // utente corrente
  User *m_user;

  Imp() : m_user(0) {
    loadPermissions();
    m_user              = findUser(TSystem::getUserName().toStdString(), false);
    if (!m_user) m_user = findUser("guest", false);
  }

  ~Imp() {
    for (std::map<std::string, User *>::iterator u = m_users.begin();
         u != m_users.end(); ++u)
      delete u->second;
  }

  User *findUser(std::string userName, bool create = true) {
    std::map<std::string, User *>::iterator i = m_users.find(userName);
    if (i != m_users.end()) return i->second;
    if (!create) return 0;
    User *user        = new User(userName);
    m_users[userName] = user;
    return user;
  }

  std::string getSVNUserName(int index) {
    User *user      = findUser(TSystem::getUserName().toStdString(), false);
    if (!user) user = findUser("guest", false);
    if (!user) return "";
    return user->getSvnUsername(index);
  }

  std::string getSVNPassword(int index) {
    User *user      = findUser(TSystem::getUserName().toStdString(), false);
    if (!user) user = findUser("guest", false);
    if (!user) return "";
    return user->getSvnPassword(index);
  }

  TFilePath getPermissionFile() {
    return TEnv::getConfigDir() + "permissions.xml";
  }
  void loadPermissions();
};

//---------------------------------------------------------

void PermissionsManager::Imp::loadPermissions() {
  TFilePath fp = getPermissionFile();
  TIStream is(fp);
  if (!is) return;

  std::string tagName;
  if (!is.matchTag(tagName) || tagName != "permissions") return;

  while (is.matchTag(tagName))
    if (tagName == "users") {
      while (is.matchTag(tagName)) {
        if (tagName != "user") return;
        std::string userName;
        is.getTagParam("name", userName);
        if (userName == "") return;
        User *user = findUser(userName);
        while (is.matchTag(tagName)) {
          if (tagName == "roles") {
            //  <roles> is no longer used
            is.skipCurrentTag();
          } else if (tagName == "svn") {
            std::string name;
            is.getTagParam("name", name);
            std::string password;
            is.getTagParam("password", password);
            user->addSvnUsername(name);
            user->addSvnPassword(password);
          } else
            return;
        }
        if (!is.matchEndTag()) return;  // </user>
      }
      if (!is.matchEndTag()) return;  // </users>
    } else if (tagName == "roles") {
      //  <roles> is no longer used
      is.skipCurrentTag();
    } else
      return;
}

//---------------------------------------------------------

//=========================================================

PermissionsManager::PermissionsManager() : m_imp(new Imp()) {}

//---------------------------------------------------------

PermissionsManager::~PermissionsManager() {}

//---------------------------------------------------------

PermissionsManager *PermissionsManager::instance() {
  static PermissionsManager _instance;
  return &_instance;
}

//---------------------------------------------------------

std::string PermissionsManager::getSVNUserName(int index) const {
  return m_imp->getSVNUserName(index);
}

//---------------------------------------------------------

std::string PermissionsManager::getSVNPassword(int index) const {
  return m_imp->getSVNPassword(index);
}
