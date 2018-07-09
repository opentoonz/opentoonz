#ifdef _WIN32

#include <QAbstractNativeEventFilter>

class TabletSupportWinInk : public QAbstractNativeEventFilter
{
	Q_DISABLE_COPY(TabletSupportWinInk)

public:
	static bool isAvailable();
	static bool isPenDeviceAvailable();

	TabletSupportWinInk() = default;
	~TabletSupportWinInk() = default;

	bool init();
	// void registerPointerDeviceNotifications();
	virtual bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) override;
};

#endif