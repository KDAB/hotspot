#include <QLoggingCategory>

// If the value of the argument to 'verbose' option is set to 'warning'
// display warnings on the console.
void messageToConsole_W(QtMsgType type, const QMessageLogContext &context, const QString &msg);
// If the value of the argument to 'verbose' option is set to 'debug'
// display debugging information on the console.
void messageToConsole_D(QtMsgType type, const QMessageLogContext &context, const QString &msg);
// If the value of the argument to 'verbose' option is set to 'all'
// display warnings and debugging information on the console.
void messageToConsole_A(QtMsgType type, const QMessageLogContext &context, const QString &msg);
// By default, the verbose option is omitted. Any warnings and debug information are suppressed
void messageSuppress(QtMsgType type, const QMessageLogContext &context, const QString &msg);

