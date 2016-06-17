#pragma once

#ifndef CURRENTIMAGE_INCLUDED
#define CURRENTIMAGE_INCLUDED

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class CurrentImageListener {
public:
  virtual void onImageChange() = 0;
  virtual ~CurrentImageListener() {}
};

DVAPI void addCurrentImageListener(CurrentImageListener *listener);
DVAPI int getCurrentImageType();

#endif
