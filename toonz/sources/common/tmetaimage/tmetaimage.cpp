

#include <tmetaimage.h>


//---------------------------------------------------------

TMetaObjectType::TMetaObjectType(const TStringId &name): name(name)
  { registerAlias(name); }

TMetaObjectType::~TMetaObjectType() {
  // TMetaObject::unregisterType(*this);
}

void
TMetaObjectType::registerAlias(const TStringId &alias)
  { TMetaObject::registerType(alias, *this); }

void
TMetaObjectType::unregisterAlias(const TStringId &alias) {
  TMetaObject::Registry::const_iterator i = TMetaObject::getRegistry().find(alias);
  if (i == TMetaObject::getRegistry().end() || i->second != this) {
    std::cerr << "warning: trying to unregister not-registered alias (" << alias.str()
              << ") of type of TMetaObject (" << name.str() << ") " << name.str() << std::endl;
  } else {
    TMetaObject::unregisterType(alias);
  }
}

//---------------------------------------------------------

TMetaObject::TMetaObject(const TMetaObject &other):
  m_typeLink(linkedMap().end()),
  m_previous(),
  m_next(),
  m_typeDesc(),
  m_handler(),
  m_data(*this, other.data())
    { linkToType(TStringId()); setType(other.getType()); }

TMetaObject::TMetaObject(const TStringId &typeName, const TVariant &data):
  m_typeLink(linkedMap().end()),
  m_previous(),
  m_next(),
  m_typeDesc(),
  m_handler(),
  m_data(*this, data)
    { linkToType(TStringId()); setType(typeName); }

TMetaObject::TMetaObject(const std::string &typeName, const TVariant &data):
  m_typeLink(linkedMap().end()),
  m_previous(),
  m_next(),
  m_typeDesc(),
  m_handler(),
  m_data(*this, data)
    { linkToType(TStringId()); setType(typeName); }

TMetaObject::~TMetaObject()
  { resetType(); unlinkFromType(); }

TMetaObject::LinkedMap&
TMetaObject::linkedMap()
  { static LinkedMap linkedMap; return linkedMap; }

void
TMetaObject::linkToType(const TStringId &type) {
  m_typeLink = linkedMap().insert( LinkedMap::value_type(type, LinkedList()) ).first;
  m_previous = m_typeLink->second.last;
  m_next = 0;
  (m_previous ? m_previous->m_next : m_typeLink->second.first) = this;
  m_typeLink->second.last = this;
}

void
TMetaObject::unlinkFromType() {
  (m_previous ? m_previous->m_next : m_typeLink->second.first) = m_next;
  (m_next     ? m_next->m_previous : m_typeLink->second.last ) = m_previous;
  m_typeLink = linkedMap().end();
  m_previous = m_next = 0;
}

void
TMetaObject::rewrap(const TStringId &type) {
  const TMetaObjectType *typeDesc = findType(type);
  if (typeDesc == m_typeDesc) return;
  if (m_handler) delete m_handler;
  m_typeDesc = typeDesc;
  m_handler  = m_typeDesc ? m_typeDesc->createHandler(*this) : 0;
  onVariantChanged(m_data);
}

void
TMetaObject::rewrapAll(const TStringId &type) {
  LinkedMap::const_iterator it = linkedMap().find(type);
  if (it != linkedMap().end())
    for(TMetaObject *i = it->second.first; i; i = i->m_next)
      i->rewrap(it->first);
}

void
TMetaObject::setType(const TStringId &name) {
  if (m_typeLink->first != name) {
    unlinkFromType();
    linkToType(name);
    rewrap(name);
  }
}

void
TMetaObject::onVariantChanged(const TVariant &value)
  { if (m_handler) m_handler->dataChanged(value); }

void
TMetaObject::setDefaults()
  { m_data.reset(); if (m_handler) m_handler->setDefaults(); }

TMetaObject*
TMetaObject::clone() const
  { return new TMetaObject(*this); }

TMetaObject::Registry&
TMetaObject::registry()
  { static Registry registry; return registry; }

void
TMetaObject::registerType(const TStringId &name, const TMetaObjectType &type) {
  if (registry().count(name))
    std::cerr << "warning: type of TMetaObject are already registered: " << name.str() << std::endl;
  registry()[name] = &type;

  LinkedMap::const_iterator it = linkedMap().find(name);
  if (it != linkedMap().end())
    for(TMetaObject *i = it->second.first; i; i = i->m_next)
      i->rewrap(name);
  rewrapAll(name);
}

void
TMetaObject::unregisterType(const TStringId &name) {
  if (!registry().count(name))
    std::cerr << "warning: trying to unregister non-registered alias of type of TMetaObject: " << name.str() << std::endl;
  registry().erase(name);
  rewrapAll(name);
}

void
TMetaObject::unregisterType(const TMetaObjectType &type) {
  Registry &r = registry();
  size_t s = r.size();
  for(Registry::iterator i = r.begin(); i != r.end();)
    if (i->second == &type)
      { r.erase(i++); rewrapAll(i->first); } else ++i;
  if (s == r.size())
    std::cerr << "warning: trying to unregister non-registered type of TMetaObject: " << type.name.str() << std::endl;
}

const TMetaObjectType*
TMetaObject::findType(const TStringId &name) {
  const Registry &r = getRegistry();
  Registry::const_iterator i = r.find(name);
  return i == r.end() ? 0 : i->second;
}

//---------------------------------------------------------

TMetaImage::TMetaImage()
  { }

TMetaImage::TMetaImage(const TMetaImage &other) {
  Reader reader(other);
  m_objects.reserve(reader->size());
  for(TMetaObjectListCW::iterator i = reader->begin(); i != reader->end(); ++i)
    if (*i)
      m_objects.push_back( TMetaObjectP((*i)->clone()) );
}

TMetaImage::~TMetaImage()
  { }

TImage*
TMetaImage::cloneImage() const
  { return new TMetaImage(*this); }

TRectD
TMetaImage::getBBox() const
  { return TRectD(); }

//---------------------------------------------------------
