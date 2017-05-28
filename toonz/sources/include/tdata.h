#pragma once

#ifndef T_DATA_INCLUDED
#define T_DATA_INCLUDED

#include "tsmartpointer.h"
#include "tfilepath.h"

#undef DVAPI
#undef DVVAR
#ifdef TNZCORE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//-------------------------------------------------------------------

class TData;
#ifdef _WIN32
template class DVAPI TSmartPointerT<TData>;
#endif

typedef TSmartPointerT<TData> TDataP;

//-------------------------------------------------------------------
class DVAPI TData : public TSmartObject {
  DECLARE_CLASS_CODE

protected:
  TData() : TSmartObject(m_classCode) {}

public:
  virtual TDataP clone() const = 0;
};

//-------------------------------------------------------------------

class DVAPI TTextData final : public TData {
  TString m_text;

public:
  TTextData(TString text) : m_text(text) {}
  TTextData(std::string text);

  TDataP clone() const override;

  TString getText() const { return m_text; }
};

//-------------------------------------------------------------------

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

class DVAPI TFilePathListData final : public TData {
  std::vector<TFilePath> m_filePaths;

public:
  TFilePathListData(const std::vector<TFilePath> &filePaths)
      : m_filePaths(filePaths) {}
  TDataP clone() const override;

  int getFilePathCount() const { return m_filePaths.size(); }
  TFilePath getFilePath(int i) const;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
