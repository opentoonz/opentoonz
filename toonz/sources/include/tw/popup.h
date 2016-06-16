#pragma once

#ifndef TW_POPUP_INCLUDED
#define TW_POPUP_INCLUDED

#include "tw/tw.h"

class DVAPI TPopup : public TWidget {
protected:
  bool m_isOpen;
  bool m_isMenu;
  bool m_hasCaption;
  bool m_isResizable;
  bool m_isTopMost;
  virtual void create();
  std::wstring m_caption;

public:
  TPopup(TWidget *parent, std::string name, bool hasCaption = true,
         bool isResizable = true, bool isTopMost = false);
  ~TPopup();

  virtual void openPopup();
  virtual void openPopup(const TPoint &pos);
  virtual void openPopup(const TDimension &size);
  virtual void closePopup();

  virtual void setGeometry(const TRect &rect);

  bool onNcPaint(bool is_active, const TDimension &window_size,
                 const TRect &caption_rect);

  // if return false, the popup does not close;
  virtual bool onClose() { return true; }
  virtual void onOpen() {}

  bool isOpen() { return m_isOpen; };

  bool isMenu() const { return m_isMenu; }

  bool hasCaption() const { return m_hasCaption; }

  bool isResizable() const { return m_isResizable; }
  bool isTopMost() const { return m_isTopMost; }

  void setCaption(const std::string &name);
  void setCaption(const std::wstring &name);

  std::wstring getCaption() const { return m_caption; };

  virtual TDimension getPreferredSize() const;
  virtual TDimension getMinimumSize() const { return TDimension(1, 1); };
  virtual TDimension getMaximumSize() const {
    return TDimension(32768, 32768);
  };
};

class DVAPI TModalPopup : public TPopup {
private:
  bool m_shellWasDisabled;
  bool m_doQuit;

protected:
  virtual void create();

public:
  TModalPopup(TWidget *parent, std::string name, bool hasCaption = true,
              bool isResizable = true, bool isTopMost = true);
  ~TModalPopup();

  // virtual void popup(const TPoint &pos);
  virtual void openPopup();
  virtual void openPopup(const TDimension &size);
  virtual void openPopup(const TPoint &pos);
  virtual void closePopup();

  static int getTitlebarHeight();
};

#endif
