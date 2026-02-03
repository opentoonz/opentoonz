

#include "texternfx.h"
#include "tsystem.h"
#include "tdoubleparam.h"
#include "timage_io.h"
//#include "tfx.h"
#include "trasterfx.h"
//#include "tsystem.h"
//#include "tparamcontainer.h"
#include "tfxparam.h"
#include "tstream.h"

namespace {

TFilePath getExternFxPath() {
  return TSystem::getBinDir() + "plugins" + "externFxs";
}
}

//=========================================================

// not implemented yet
void TExternFx::getNames(std::vector<std::string> &names) {}

//=========================================================

TExternFx *TExternFx::create(std::string name) {
  TExternalProgramFx *fx = new TExternalProgramFx(name);
  return fx;
}

//=========================================================

TExternalProgramFx::TExternalProgramFx(std::string name)
    : m_externFxName(name) {
  initialize(name);
  setName(L"ExternalProgramFx");
  //    addInputPort("input2", m_input2);
}

//------------------------------------------------------------------

TExternalProgramFx::TExternalProgramFx() : m_externFxName() {
  setName(L"ExternalProgramFx");
  //    addInputPort("input2", m_input2);
}

//------------------------------------------------------------------

TExternalProgramFx::~TExternalProgramFx() {}

//------------------------------------------------------------------

void TExternalProgramFx::initialize(std::string name) {
  TFilePath fp = getExternFxPath() + (name + ".xml");
  TIStream is(fp);
  if (!is) return;

  std::string tagName;
  if (!is.matchTag(tagName) || tagName != "externFx") return;

  try {
    while (is.matchTag(tagName)) {
      if (tagName == "executable") {
        TFilePath executable = TFilePath(is.getTagAttribute("path"));
        std::string args     = is.getTagAttribute("args");
        if (executable == TFilePath())
          throw TException("missing executable path");
        if (args == "") throw TException("missing args string");
        setExecutable(executable, args);
      } else if (tagName == "inport" || tagName == "outport") {
        std::string portName = is.getTagAttribute("name");
        std::string ext      = is.getTagAttribute("ext");
        if (portName == "") throw TException("missing port name");
        if (ext == "") throw TException("missing port ext");
        addPort(portName, ext, tagName == "inport");
      } else if (tagName == "param") {
        std::string paramName = is.getTagAttribute("name");
        if (paramName == "") throw TException("missing param name");
        std::string type = is.getTagAttribute("type");
        if (type == "") throw TException("missing param type");
        if (type != "double")
          throw TException("param type not yet implemented");
        TDoubleParamP param = new TDoubleParam();
        param->setName(paramName);
        m_params.push_back(param);
      } else
        throw TException("unexpected tag " + tagName);
    }
    is.closeChild();

    for (int i = 0; i < (int)m_params.size(); i++)
      bindParam(this, m_params[i]->getName(), m_params[i]);

  } catch (...) {
  }
}

//------------------------------------------------------------------

void TExternalProgramFx::addPort(std::string portName, std::string ext,
                                 bool isInput) {
  if (isInput) {
    TRasterFxPort *port = new TRasterFxPort();
    m_ports[portName]   = Port(portName, ext, port);
    addInputPort(portName, *port);
  } else
    m_ports[portName] = Port(portName, ext, 0);
}

//------------------------------------------------------------------

// void TExternalProgramFx::addParam(string paramName, const TParamP &param)
//{
//  m_params[paramName] = param;
//  TFx::addParam(paramName, param);
//}

//------------------------------------------------------------------

TFx *TExternalProgramFx::clone(bool recursive) const {
  TExternalProgramFx *fx =
      dynamic_cast<TExternalProgramFx *>(TExternFx::create(m_externFxName));
  assert(fx);
  // new TExternalProgramFx();
  // fx->setExecutable(m_executablePath, m_args);

  // copia della time region
  fx->setActiveTimeRegion(getActiveTimeRegion());
  // fx->m_imp->m_activeTimeRegion = m_imp->m_activeTimeRegion;

  fx->getParams()->copy(getParams());

  assert(getInputPortCount() == fx->getInputPortCount());

  // std::map<std::string, Port>::const_iterator j;
  // for(j=m_ports.begin(); j!=m_ports.end(); ++j)
  //  fx->addPort(j->first, j->second.m_ext, j->second.m_port != 0);

  // copia ricorsiva sulle porte
  if (recursive) {
    for (int i = 0; i < getInputPortCount(); ++i) {
      TFxPort *port = getInputPort(i);
      if (port->getFx())
        fx->connect(getInputPortName(i), port->getFx()->clone(true));
    }
  }

  // std::map<std::string, TParamP>::const_iterator j;
  // for(j=m_params.begin(); j!=m_params.end(); ++j)
  //  fx->addParam(j->first, j->second->clone());

  return fx;
}

//------------------------------------------------------------------

bool TExternalProgramFx::doGetBBox(double frame, TRectD &bBox,
                                   const TRenderSettings &info) {
  // bBox = TRectD(-30,-30,30,30);
  //  return true;

  std::map<std::string, Port>::const_iterator portIt;
  for (portIt = m_ports.begin(); portIt != m_ports.end(); ++portIt) {
    if (portIt->second.m_port != 0) {
      TRasterFxPort *tmp;
      tmp = portIt->second.m_port;
      if (tmp->isConnected()) {
        TRectD tmpbBox;
        (*tmp)->doGetBBox(frame, tmpbBox, info);
        bBox += tmpbBox;
      }
    }
  }

  if (bBox.isEmpty()) {
    bBox = TRectD();
    return false;
  } else
    return true;
  /*
if(m_input1.isConnected() || m_input2.isConnected())
{
bool ret = m_input1->doGetBBox(frame, bBox) || m_input1->doGetBBox(frame, bBox);
return ret;
}
else
{
bBox = TRectD();
return false;
}
*/
}

//------------------------------------------------------------------

void TExternalProgramFx::doCompute(TTile &tile, double frame,
                                   const TRenderSettings &ri) {
  TRaster32P ras = tile.getRaster();
  if (!ras) return;
  std::string args           = m_args;
  std::string executablePath = ::to_string(m_executablePath);
  std::map<std::string, TFilePath> tmpFiles;  // portname --> file
  TFilePath outputTmpFile;

  std::map<std::string, Port>::const_iterator portIt;
  for (portIt = m_ports.begin(); portIt != m_ports.end(); ++portIt) {
    TFilePath fp            = TSystem::getUniqueFile("externfx");
    fp                      = fp.withType(portIt->second.m_ext);
    tmpFiles[portIt->first] = fp;
    if (portIt->second.m_port == 0)  // solo una porta e' di output
      outputTmpFile = fp;
    else {
      TRasterFxPort *tmp;
      tmp = portIt->second.m_port;
      if (tmp->isConnected()) {
        (*tmp)->compute(tile, frame, ri);
        TImageWriter::save(fp, ras);
      }
    }
  }

  // args e' della forma "$src $ctrl -o $out -v $value"
  // sostituisco le variabili
  int i = 0;
  for (;;) {
    i = args.find('$', i);
    if (i == (int)std::string::npos) break;
    int j   = i + 1;
    int len = args.length();
    while (j < len && isalnum(args[j])) j++;
    // un '$' non seguito da caratteri alfanumerici va ignorato
    if (j == i + 1) {
      // la sequenza '$$' diventa '$'
      if (j < len && args[j] == '$') args.replace(i, 2, "$");
      i++;
      continue;
    }
    // ho trovato una variabile
    int m            = j - i - 1;
    std::string name = args.substr(i + 1, m);
    // calcolo il valore.
    std::string value;

    std::map<std::string, TFilePath>::const_iterator it;
    it = tmpFiles.find(name);
    if (it != tmpFiles.end()) {
      // e' una porta. il valore e' il nome del
      // file temporaneo
      value = "\"" + ::to_string(it->second.getWideString()) + "\"";
    } else {
      // e' un parametro
      // se il nome non viene riconosciuto sostituisco la stringa nulla
      TDoubleParamP param = TParamP(getParams()->getParam(name));
      if (param) value    = std::to_string(param->getValue(frame));
    }

    args.replace(i, m + 1, value);
  }
  args = " " + args;  // aggiungo uno spazio per sicurezza
  // ofstream os("C:\\temp\\butta.txt");
  // os << args << endl;

  // bisognerebbe calcolare le immagini dalla/e porta/e di input
  // scrivere il/i valore/i nei files temporanei/o
  // chiamare "m_executablePath args"
  // e leggere l'immagine scritta in outputTmpFile
  // poi cancellare tutto
  std::string expandedargs;
  char buffer[1024];
#ifdef _WIN32
  ExpandEnvironmentStrings(args.c_str(), buffer, 1024);

  STARTUPINFO si;
  PROCESS_INFORMATION pinfo;

  GetStartupInfo(&si);

  BOOL ret = CreateProcess(
      (char *)executablePath.c_str(),           // name of executable module
      buffer,                                   // command line string
      NULL,                                     // SD
      NULL,                                     // SD
      TRUE,                                     // handle inheritance option
      CREATE_NO_WINDOW, /*CREATE_NEW_CONSOLE*/  // creation flags
      NULL,                                     // new environment block
      NULL,                                     // current directory name
      &si,                                      // startup information
      &pinfo                                    // process information
      );

  if (!ret) DWORD err = GetLastError();

  // aspetta che il processo termini
  WaitForSingleObject(pinfo.hProcess, INFINITE);

  DWORD exitCode;
  ret = GetExitCodeProcess(pinfo.hProcess,  // handle to the process
                           &exitCode);      // termination status

#else
  std::string cmdline = executablePath + buffer;
  //    int exitCode =
  system(cmdline.c_str());
#endif
  /*
string name = m_executablePath.getName();
TPixel32 color;
if(name == "saturate") color = TPixel32::Magenta;
else if(name == "over") color = TPixel32::Green;
else color = TPixel32::Red;
for(int iy=0;iy<ras->getLy();iy++)
{
TPixel32 *pix = ras->pixels(iy);
TPixel32 *endPix = pix + ras->getLx();
double x = tile.m_pos.x;
double y = tile.m_pos.y + iy;
while(pix<endPix)
 {
  if(x*x+y*y<900) *pix = color;
  else *pix = TPixel32(0,0,0,0);
  ++pix;
  x+=1.0;
 }
}
*/

  try {
    TRasterP ras = tile.getRaster();
    TImageReader::load(outputTmpFile, ras);
  } catch (...) {
  }

  // butto i file temporanei creati
  std::map<std::string, TFilePath>::const_iterator fileIt;
  for (fileIt = tmpFiles.begin(); fileIt != tmpFiles.end(); ++fileIt) {
    if (TFileStatus(fileIt->second).doesExist() == true) try {
        TSystem::deleteFile(fileIt->second);
      } catch (...) {
      }
  }
  if (TFileStatus(outputTmpFile).doesExist() == true) try {
      TSystem::deleteFile(outputTmpFile);
    } catch (...) {
    }
}

//------------------------------------------------------------------

void TExternalProgramFx::loadData(TIStream &is) {
  std::string tagName;
  while (is.openChild(tagName)) {
    if (tagName == "path") {
      is >> m_executablePath;
    } else if (tagName == "args") {
      is >> m_args;
    } else if (tagName == "name") {
      is >> m_externFxName;
      // initialize(m_externFxName);
    } else if (tagName == "params") {
      while (is.matchTag(tagName)) {
        if (tagName == "param") {
          // assert(0);
          std::string paramName = is.getTagAttribute("name");
          TDoubleParamP param   = new TDoubleParam();
          param->setName(paramName);
          m_params.push_back(param);
        } else
          throw TException("unexpected tag " + tagName);
      }
      for (int i = 0; i < (int)m_params.size(); i++)
        bindParam(this, m_params[i]->getName(), m_params[i]);
    } else if (tagName == "ports") {
      while (is.matchTag(tagName)) {
        if (tagName == "port") {
          std::string name = is.getTagAttribute("name");
          std::string ext  = is.getTagAttribute("ext");
          addPort(name, ext, true);
        } else if (tagName == "outport") {
          std::string name = is.getTagAttribute("name");
          std::string ext  = is.getTagAttribute("ext");
          addPort(name, ext, false);
        } else
          throw TException("unexpected tag " + tagName);
      }
    } else if (tagName == "super") {
      TExternFx::loadData(is);
    } else
      throw TException("unexpected tag " + tagName);
    is.closeChild();
  }
}

void TExternalProgramFx::saveData(TOStream &os) {
  os.child("name") << m_externFxName;
  os.child("path") << m_executablePath;
  os.child("args") << m_args;
  os.openChild("params");
  for (int i = 0; i < getParams()->getParamCount(); i++) {
    std::map<std::string, std::string> attr;
    attr["name"] = getParams()->getParamName(i);
    attr["type"] = "double";
    os.openCloseChild("param", attr);
  }
  os.closeChild();
  os.openChild("ports");
  std::map<std::string, Port>::iterator portIt;
  for (portIt = m_ports.begin(); portIt != m_ports.end(); ++portIt) {
    std::map<std::string, std::string> attr;
    attr["name"]        = portIt->second.m_name;
    attr["ext"]         = portIt->second.m_ext;
    std::string tagName = portIt->second.m_port == 0 ? "outport" : "port";
    os.openCloseChild(tagName, attr);
  }
  os.closeChild();
  os.openChild("super");
  TExternFx::saveData(os);
  os.closeChild();
}

//------------------------------------------------------------------

#ifdef CICCIO
//-------------------------------------------------------------------

void ExternalProgramFx::doCompute(TTile &tile, double frame,
                                  const TRenderSettings &ri) {
  if (!m_input1.isConnected() || !m_input2.isConnected()) {
    tile.getRaster()->clear();
    return;
  }
  std::string name1("C:\\temp\\uno..tif");
  std::string name2("C:\\temp\\due..tif");
  std::string outname("C:\\temp\\outfile.0001.jpg");
  std::string program("C:\\temp\\xdissolve.exe");
  std::string extension(".jpg");
  TFilePath programpath(program);

  m_input1->compute(tile, frame, ri);
  TFilePath fname1(name1);
  TFilePath fname2(name2);
  std::string tmp1 = fname1.getName() + extension;
  std::string tmp2 = fname2.getName() + extension;

  TFilePath tmpname1(fname1.getParentDir() + tmp1);
  TFilePath tmpname2(fname2.getParentDir() + tmp2);
  TFilePath out(outname);
  TImageWriter::save(tmpname1, tile.getRaster());
  m_input2->compute(tile, frame, ri);
  TImageWriter::save(tmpname2, tile.getRaster());

  std::string arglist = " -range1 1 1 -start2 1 -startout 1 ";
  arglist += toString(tmpname1.getWideString());
  arglist += " " + toString(tmpname2.getWideString());
  arglist += " " + outname;
  std::string cmdline = program + arglist;

#ifdef _WIN32
  STARTUPINFO si;
  PROCESS_INFORMATION pinfo;

  GetStartupInfo(&si);

  BOOL ret =
      CreateProcess(NULL,                     // name of executable module
                    (char *)cmdline.c_str(),  // command line string
                    NULL,                     // SD
                    NULL,                     // SD
                    TRUE,                     // handle inheritance option
                    CREATE_NO_WINDOW, /*CREATE_NEW_CONSOLE*/  // creation flags
                    NULL,   // new environment block
                    NULL,   // current directory name
                    &si,    // startup information
                    &pinfo  // process information
                    );

  if (!ret) DWORD err = GetLastError();

  // aspetta che il processo termini
  WaitForSingleObject(pinfo.hProcess, INFINITE);

  DWORD exitCode;
  ret = GetExitCodeProcess(pinfo.hProcess,  // handle to the process
                           &exitCode);      // termination status

#else
  int exitCode = system(cmdline.c_str());
#endif
  TImageReader::load(out, tile.getRaster());

  TSystem::deleteFile(tmpname1);
  TSystem::deleteFile(tmpname2);
  TSystem::deleteFile(out);
}

FX_PLUGIN_IDENTIFIER(ExternalProgramFx, "externalProgramFx");
* /

#endif
    FX_IDENTIFIER(TExternalProgramFx, "externalProgramFx")
