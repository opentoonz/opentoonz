#pragma once

#ifndef TSTREAM_H
#define TSTREAM_H

#include <memory>

// TnzCore includes
#include "tpixel.h"

// Qt includes
#ifndef TNZCORE_LIGHT
#include <QString>
#endif

#undef DVAPI
#undef DVVAR
#ifdef TSTREAM_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//===================================================================

//    Forward declarations

class TPersist;
class TFilePath;

typedef std::pair<int, int>
    VersionNumber;  //!< Integer pair storing the major and minor
                    //!  application version numbers.

//===================================================================

/*!
  \brief Toonz's XML-like \a input file parser.

  This class is Toonz's standard \a input parser for simple XML files.
  It is specifically designed to interact with object types derived
  from the TPersist base class.
*/

class DVAPI TIStream {
  class Imp;
  std::unique_ptr<Imp> m_imp;

public:
  /*!
\warning  Stream construction <I> may throw </I> on files \b compressed using
        TOStream 's compression.

\warning  Even if construction does not throw, the stream could still be
        constructed with an invalid status. Remember to check the stream
        status using operator bool().
*/
  TIStream(const TFilePath &is);  //!< Opens the document at the specified path
  ~TIStream();                    //!< Destroys the stream

  //! \sa std::basic_istream::operator void*().
  operator bool() const;  //!< Returns whether the stream has a valid status.

  TIStream &operator>>(int &v);           //!< Reads an integer from the stream
  TIStream &operator>>(double &v);        //!< Reads a double from the stream
  TIStream &operator>>(std::string &v);   //!< Reads a string from the stream
  TIStream &operator>>(std::wstring &v);  //!< Reads a wstring from the stream
  TIStream &operator>>(TFilePath &v);     //!< Reads a TFilePath from the stream
  TIStream &operator>>(TPixel32 &v);      //!< Reads a TPixel32 from the stream
  TIStream &operator>>(TPixel64 &v);      //!< Reads a TPixel64 from the stream

#ifndef TNZCORE_LIGHT
  TIStream &operator>>(QString &v);  //!< Reads an integer from the stream
#endif

  /*! \detail
This function dispatches the loading process to the derived class'
reimplementation of the TPersist::loadData() function.

\note Unlinke operator>>(TPersist*&), this function \a requires that
    the passed object is of the \b correct type to read the current tag.
*/
  TIStream &operator>>(
      TPersist &v);  //!< Reads a TPersist derived object data from the stream.
  TIStream &operator>>(TPersist *&v);  //!< \a Allocates and reads a TPersist
                                       //! derived object data from the stream.
  //!  \sa operator>>(TPersist&)

  //!  \deprecated
  std::string
  getString();  //!< Returns the stream content as a string, up to the next tag.

  //!  \deprecated
  bool eos();  //!< \brief Returns \e true in case of end of string (a
               //! StreamTag::EndTag
  //!  is encountered or the string is empty).

  /*!
\param      tagName Output name of a matched tag.
\return     Whether a tag was found.
*/
  bool matchTag(
      std::string &tagName);  //!< Attempts matching a tag and returns its name.

  //! \return   Whether an end tag was found.
  bool matchEndTag();  //!< Attempts matching a StreamTag::EndTag.

  /*!
\brief Attempts retrieval of the value associated with the
     specified tag attribute in current tag.

\param paramName  Tag attribute name.
\param value      Output value of the attribute.
\return           Whether the tag attribute was found.
*/
  bool getTagParam(std::string paramName, std::string &value);
  bool getTagParam(std::string paramName,
                   int &value);  //!< \sa getTagParam(string, string&)

  bool isBeginEndTag();  //!< Returns whether current tag is of type
                         //! StreamTag::BeginEndTag.

  bool openChild(
      std::string &tagName);  //!< \deprecated Use matchTag(string&) instead.
  void closeChild();          //!< \deprecated Use matchEndTag() instead.

  bool match(char c) const;  //! \deprecated

  std::string getTagAttribute(
      std::string name) const;  //!< \sa getTagParam(string, string&),
  //!  TOStream::openChild(string, const map<std::string, string>&).

  TFilePath getFilePath();  //!< Returns the stream's path (i.e. the opened
                            //! filename associated to the input stream).
  TFilePath getRepositoryPath();  //!< \deprecated

  int getLine()
      const;  //!< Returns the line number of the stream <TT><B>+1</B></TT>.
              //!  \warning I've not idea why the +1, though.

  VersionNumber getVersion()
      const;  //!< Returns the currently stored version of the opened document.
              //!  \sa setVersion()

  void setVersion(const VersionNumber &version);  //!< Returns the currently
                                                  //! stored version of the
  //! opened document.
  //!  \sa setVersion()
  /*!
\note After skipping the tag content, the stream is positioned immediately
    after the end tag.
*/
  void skipCurrentTag();  //!< Silently ignores the content of currently opened
                          //! tag up to its end.

  std::string getCurrentTagName();

private:
  // Not copyable
  TIStream(const TIStream &);             //!< Not implemented
  TIStream &operator=(const TIStream &);  //!< Not implemented
};

//-------------------------------------------------------------------

template <class T>
TIStream &operator>>(TIStream &is, T *&v) {
  TPersist *persist = 0;
  is >> persist;
  v = persist ? dynamic_cast<T *>(persist) : 0;
  return is;
}

//===================================================================

/*!
  \brief Toonz's XML-like \a output file parser.

  This class is Toonz's standard \a output parser for simple XML files.
  It is specifically designed to interact with object types derived
  from the TPersist base class.
*/

class DVAPI TOStream {
  class Imp;
  std::shared_ptr<Imp> m_imp;

private:
  explicit TOStream(std::shared_ptr<Imp> imp);  //!< deprecated

  TOStream(TOStream &&);
  TOStream &operator=(TOStream &&);

public:
  /*!
\param fp           Output file path
\param compressed   Enables compression of the whole file

\note     Stream construction <I> does not throw </I>. However, the stream
        status could be invalid. Remeber to check the stream validity using
        operator bool().

\warning  Stream compression has been verified to be unsafe.
        Please consider it \a deprecated.
*/
  TOStream(const TFilePath &fp,
           bool compressed = false);  //!< Opens the specified file for write
  ~TOStream();  //!< Closes the file and destroys the stream

  //! \sa std::basic_ostream::operator void*().
  operator bool() const;  //!< Returns whether the stream has a valid status.

  TOStream &operator<<(int v);           //!< Writes an int to the stream.
  TOStream &operator<<(double v);        //!< Writes a double to the stream.
  TOStream &operator<<(std::string v);   //!< Writes a string to the stream.
  TOStream &operator<<(std::wstring v);  //!< Writes a wstring to the stream.
  TOStream &operator<<(
      const TFilePath &v);  //!< Writes a TFilePath to the stream.
  TOStream &operator<<(
      const TPixel32 &v);  //!< Writes a TPixel32 to the stream.
  TOStream &operator<<(
      const TPixel64 &v);  //!< Writes a TPixel64 to the stream.

#ifndef TNZCORE_LIGHT
  TOStream &operator<<(QString v);  //!< Writes a QString to the stream.
#endif

  TOStream &operator<<(
      TPersist *v);  //!< deprecated Use operator<<(TPersist&) instead.
  TOStream &operator<<(TPersist &v);  //!< Saves data to the stream according
  //!  to the reimplemented TPersist::saveData.

  //! \deprecated Use openChild(string) instead
  TOStream child(std::string tagName);

  void openChild(std::string tagName);  //!< Writes a <tagName> to the stream,
                                        //! opening a tag.
  void openChild(
      std::string tagName,
      const std::map<std::string, std::string>
          &attributes);  //!< \brief Writes a <tagName attribute1="value1" ..>
  //!  to the stream, opening a tag with embedded attributes.
  void openCloseChild(std::string tagName,
                      const std::map<std::string, std::string>
                          &attributes);  //!< \brief Writes a tag <tagName
                                         //! attribute1="value1" ../>
  //!  to the stream, opening a tag with embedded attributes
  //!  which is immediately closed.

  void closeChild();  //!< Closes current tag, writing </currentTagName> to the
                      //! stream.

  void cr();  //!< Writes carriage return to the stream. \deprecated

  void tab(int dt);  //!< \deprecated

  TFilePath getFilePath();  //!< Returns the file path of the file associated to
                            //! this output stream.
  TFilePath getRepositoryPath();  //!< \deprecated

  /*! \detail
This function is similar to operator bool(), but \b flushes the stream before
checking the status.

\return   Whether the stream is in a good state (no fails in writing to).
*/
  bool checkStatus() const;  //!< \b Flushes the stream and checks its validity.

  std::string getCurrentTagName();

private:
  // Not copyable
  TOStream(const TOStream &) = delete;             //!< Not implemented
  TOStream &operator=(const TOStream &) = delete;  //!< Not implemented
};

#endif  // TSTREAM_H
