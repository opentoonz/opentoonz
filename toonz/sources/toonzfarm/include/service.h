#ifndef SERVICE_H
#define SERVICE_H

#include <memory>

class TFilePath;

#include <string>
#include <QString>
using namespace std;

//------------------------------------------------------------------------------

#ifdef TFARMAPI
#undef TFARMAPI
#endif

#ifdef WIN32
#ifdef TFARM_EXPORTS
#define TFARMAPI __declspec(dllexport)
#else
#define TFARMAPI __declspec(dllimport)
#endif
#else
#define TFARMAPI
#endif

//------------------------------------------------------------------------------

bool ReportStatusToSCMgr(long currentState,
						 long win32ExitCode,
						 long WaitHint);

void AddToMessageLog(char *msg);

//------------------------------------------------------------------------------

TFARMAPI string getLastErrorText();

//------------------------------------------------------------------------------

class TFARMAPI TService
{
public:
	TService(const string &name, const string &displayName);
	virtual ~TService();

	static TService *instance();

	enum Status {
		Stopped = 1,
		StartPending,
		StopPending,
		Running,
		ContinuePending,
		PausePending,
		Paused
	};

	void setStatus(Status status, long exitCode, long waitHint);

	string getName() const;
	string getDisplayName() const;

	void run(int argc, char *argv[], bool console = false);

	virtual void onStart(int argc, char *argv[]) = 0;
	virtual void onStop() = 0;

	bool isRunningAsConsoleApp() const;

	static void start(const string &name);
	static void stop(const string &name);

	static void install(
		const string &name,
		const string &displayName,
		const TFilePath &appPath);

	static void remove(const string &name);

	static void addToMessageLog(const string &msg);
	static void addToMessageLog(const QString &msg);

private:
	class Imp;
	std::unique_ptr<Imp> m_imp;

	static TService *m_instance;
};

#endif
