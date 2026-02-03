#pragma once

#ifndef STUDIOPALETTE_INCLUDED
#define STUDIOPALETTE_INCLUDED

#include "tpalette.h"
#include "toonz/tproject.h"
// #include "tfilepath.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TPalette;
class TImage;
class TColorStyle;

class DVAPI StudioPalette
    : public TProjectManager::Listener {  // singleton; methods can throw
                                          // exceptions
public:
  class Listener {
  public:
    virtual void onStudioPaletteTreeChange() {}
    virtual void onStudioPaletteMove(const TFilePath &dstPath,
                                     const TFilePath &srcPath) {}
    virtual void onStudioPaletteChange(const TFilePath &palette) {}
    virtual ~Listener() {}
  };

  class Palette {  // serve per il drag&drop
    TFilePath m_palettePath;

  public:
    Palette(const TFilePath &path) : m_palettePath(path) {}
    const TFilePath &getPath() const { return m_palettePath; }
  };

  static StudioPalette *instance();

  ~StudioPalette();

  TPalette *getPalette(const TFilePath &path, bool loadRefImg = false);

  int getChildren(std::vector<TFilePath> &fps, const TFilePath &folderPath);
  int getChildCount(const TFilePath &folderPath);

  bool isFolder(const TFilePath &path);
  bool isPalette(const TFilePath &path);
  bool isReadOnly(const TFilePath &path);
  bool isLevelPalette(const TFilePath &path);
  bool hasGlobalName(const TFilePath &path);

  // ritorna il nome del folder creato
  TFilePath createFolder(const TFilePath &parentFolderPath);
  void createFolder(const TFilePath &parentFolderPath, std::wstring name);

  // ritorna il nome della palette creata;
  // se paletteName != "" prova ad assegnare quel nome. Se esiste gia'
  // aggiunge un suffisso
  TFilePath createPalette(const TFilePath &folderPath,
                          std::string paletteName = "");

  // DOESN'T get ownership
  void setPalette(const TFilePath &palettePath, const TPalette *palette,
                  bool notifyPaletteChanged);

  void deletePalette(const TFilePath &palettePath);
  void deleteFolder(const TFilePath &folderPath);

  TFilePath importPalette(const TFilePath &dstFolder, const TFilePath &srcPath);

  void movePalette(const TFilePath &dstPath, const TFilePath &srcPath);

  TFilePath getLevelPalettesRoot();
  TFilePath getProjectPalettesRoot();

  // TFilePath getRefImage(const TFilePath palettePath);

  bool updateLinkedColors(TPalette *palette);
  void setStylesGlobalNames(TPalette *palette);

  TFilePath getPalettePath(std::wstring paletteId);
  TPalette *getPalette(std::wstring paletteId);

  TColorStyle *getStyle(std::wstring styleId);

  // se lo stile ha un nome globale restituisce il nome della studio palette e
  // l'indice
  // dello stile linkato
  // (altrimenti restituisce (TFilePath(),-1))
  std::pair<TFilePath, int> getSourceStyle(TColorStyle *cs);

  void addListener(Listener *listener);
  void removeListener(Listener *listener);

  void notifyTreeChange();
  void notifyMove(const TFilePath &dstPath, const TFilePath &srcPath);
  void notifyPaletteChange(const TFilePath &name);

  // bruttacchietto.
  static bool isEnabled() { return m_enabled; };
  static void enable(bool enabled);

  void save(const TFilePath &path, TPalette *palette);

  void removeEntry(const std::wstring paletteId);
  void addEntry(const std::wstring paletteId, const TFilePath &path);

  // TProjectManager::Listener
  void onProjectSwitched() override;
  void onProjectChanged() override;

private:
  StudioPalette();
  TFilePath m_root;
  std::map<std::wstring, TFilePath> m_table;
  std::vector<Listener *> m_listeners;

  TPalette *load(const TFilePath &path);

  static bool m_enabled;
};

#endif
