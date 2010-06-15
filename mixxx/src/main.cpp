/***************************************************************************
                          main.cpp  -  description
                             -------------------
    begin                : Mon Feb 18 09:48:17 CET 2002
    copyright            : (C) 2002 by Tue and Ken Haste Andersen
    email                :
***************************************************************************/

/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

#include <qapplication.h>
#include <qfont.h>
#include <qstring.h>
#include <qtextcodec.h>
#include <qtranslator.h>
#include <qmessagebox.h>
#include <qiodevice.h>
#include <qfile.h>
#include <q3textstream.h>
#include <qstringlist.h>
#include <stdio.h>
#include <math.h>
#include "portaudio.h"
#include "mixxx.h"
#include "qpixmap.h"
#include "qsplashscreen.h"
#include "errordialog.h"
#include "defs_audiofiles.h"

#ifdef __WINDOWS__
#ifdef DEBUGCONSOLE
#include <io.h> // Debug Console
#include <windows.h>

void InitDebugConsole() { // Open a Debug Console so we can printf
    int fd;
    FILE * fp;

    FreeConsole();
    if (AllocConsole()) {
        SetConsoleTitleA("Mixxx Debug Messages");

        fd = _open_osfhandle( (long)GetStdHandle( STD_OUTPUT_HANDLE ), 0);
        fp = _fdopen( fd, "w" );

        *stdout = *fp;
        setvbuf( stdout, NULL, _IONBF, 0 );

        fd = _open_osfhandle( (long)GetStdHandle( STD_ERROR_HANDLE ), 0);
        fp = _fdopen( fd, "w" );

        *stderr = *fp;
        setvbuf( stderr, NULL, _IONBF, 0 );
    }
}
#endif // DEBUGCONSOLE
#endif // __WINDOWS__

QApplication * a;

QStringList plugin_paths; //yes this is global. sometimes global is good.
ErrorDialog *dialogHelper; //= new ErrorDialog();   // allows threads to show error dialogs

void qInitImages_mixxx();

void MessageOutput( QtMsgType type, const char * msg )
{
    static QMutex mutex;
    QMutexLocker locker(&mutex);

    switch ( type ) {
    case QtDebugMsg:
#ifdef __WINDOWS__
        if (strstr(msg, "doneCurrent")) {
            break;
        }
#endif
        fprintf( stderr, "Debug: %s\n", msg );
        break;
    case QtWarningMsg:
        fprintf( stderr, "Warning: %s\n", msg);
        break;
    case QtCriticalMsg:
        fprintf( stderr, "Critical: %s\n", msg );
        QMessageBox::warning(0, "Mixxx", msg);
        exit(-1);
        break;
    case QtFatalMsg:
        fprintf( stderr, "Fatal: %s\n", msg );
        QMessageBox::warning(0, "Mixxx", msg);
        abort();
    }
}

QFile Logfile; // global logfile variable


void MessageToLogfile( QtMsgType type, const char * msg )
{
    static QMutex mutex;
    QMutexLocker locker(&mutex);
    
    Q3TextStream Log( &Logfile );
    switch ( type ) {
    case QtDebugMsg:
        Log << "Debug: " << msg << "\n";
        break;
    case QtWarningMsg:
        Log << "Warning: " << msg << "\n";
        //a->lock(); //this doesn't do anything in Qt4
        QMessageBox::warning(0, "Mixxx", msg);
        //a->unlock();
        break;
    case QtCriticalMsg:
        fprintf( stderr, "Critical: %s\n", msg );
        QMessageBox::warning(0, "Mixxx", msg);
        exit(-1);
        break;
    case QtFatalMsg:
        fprintf( stderr, "Fatal: %s\n", msg );
        QMessageBox::warning(0, "Mixxx", msg);
        abort();
    }
    Logfile.flush();
}


/* Debug message handler which outputs to both a logfile and a
 * and prepends the thread the message came from too.
 *
 *
 */
void MessageHandler( QtMsgType type, const char * input )
{
    static QMutex mutex;
    QMutexLocker locker(&mutex);
    QString tmp = QString("[%1]: %2").arg(QThread::currentThread()->objectName()).arg(input);
    QByteArray ba = tmp.toLocal8Bit(); //necessary inner step to avoid memory corruption (otherwise the QByteArray is destroyed at the end of the next line which is BAD NEWS BEARS)
    const char* s = ba.constData();
    
    
    if(!Logfile.isOpen())
    {
    Logfile.setFileName("mixxx.log"); //XXX will there ever be a case that we can't write to our current working directory?
     
#ifdef QT3_SUPPORT
    Logfile.open(QIODevice::WriteOnly | QIODevice::Text);
#else
    Logfile.open(IO_WriteOnly | IO_Translate);
#endif
    }
      
    Q3TextStream Log( &Logfile );	
    
    switch ( type ) {
    case QtDebugMsg:
#ifdef __WINDOWS__  //wtf? -kousu 2/2009
        if (strstr(input, "doneCurrent")) {
            break;
        }
#endif
        fprintf( stderr, "Debug: %s\n", s );
        Log << "Debug: " << s << "\n";
        break;
    case QtWarningMsg:
        fprintf( stderr, "Warning: %s\n", s);
        Log << "Warning: " << s << "\n";
        //QMessageBox::warning(0, "Mixxx", input);
        //dialogHelper->requestErrorDialog(0,input);
        //I will break your legs if you re-enable the above lines of code.
        //You shouldn't be using qWarning for reporting user-facing errors.
        //Implement your own error message box...
        // - Albert (March 11, 2010)
        break;
    case QtCriticalMsg:
        fprintf( stderr, "Critical: %s\n", s );
        Log << "Critical: " << s << "\n";
         //QMessageBox::critical(0, "Mixxx", input);
        dialogHelper->requestErrorDialog(1,input);
//         exit(-1);
        break; //NOTREACHED(?)
    case QtFatalMsg:
        fprintf( stderr, "Fatal: %s\n", s );
        Log << "Fatal: " << s << "\n";
        //QMessageBox::critical(0, "Mixxx", input);
        dialogHelper->requestErrorDialog(1,input);
        abort();
        break; //NOTREACHED
    }
    Logfile.flush();
}


int main(int argc, char * argv[])
{
    // Check if an instance of Mixxx is already running


//it seems like this code should be inline in MessageHandler() but for some reason having it there corrupts the messages sometimes -kousu 2/2009
	

#ifdef __WINDOWS__
  #ifdef DEBUGCONSOLE
    InitDebugConsole();
  #endif
#endif
   dialogHelper = new ErrorDialog(); 

    qInstallMsgHandler( MessageHandler );

    QThread::currentThread()->setObjectName("Main");
    a = new QApplication(argc, argv);

    QTranslator tor( 0 );
    // set the location where your .qm files are in load() below as the last parameter instead of "."
    // for development, use "/" to use the english original as
    // .qm files are stored in the base project directory.
    tor.load( QString("mixxx.") + QLocale::system().name(), "." );
    a->installTranslator( &tor );

    // Check if one of the command line arguments is "--no-visuals"
//    bool bVisuals = true;
//    for (int i=0; i<argc; ++i)
//        if(QString("--no-visuals")==argv[i])
//            bVisuals = false;

    // Construct a list of strings based on the command line arguments
    struct CmdlineArgs args;
    args.bStartInFullscreen = false; //Initialize vars
    
    // Only match supported file types since command line options are also parsed elsewhere
    QRegExp fileRx(MIXXX_SUPPORTED_AUDIO_FILETYPES_REGEX, Qt::CaseInsensitive);

    for (int i=0; i<argc; ++i)
    {
        if (argv[i]==QString("-h") || argv[i]==QString("--h") || argv[i]==QString("--help")) {
            printf("Mixxx digital DJ software - command line options");
            printf("\n(These are case-sensitive.)\n\n\
    [FILE]                  Load the specified music file(s) at start-up.\n\
                            Each must be one of the following file types:\n\
                            ");
            printf(MIXXX_SUPPORTED_AUDIO_FILETYPES);
            printf("\n\n");
            printf("\
                            Each file you specify will be loaded into the\n\
                            next virtual deck.\n\
\n\
    --resourcePath PATH     Top-level directory where Mixxx should look\n\
                            for its resource files such as MIDI mappings,\n\
                            overriding the default installation location.\n\
\n\
    --midiDebug             Causes Mixxx to display/log all of the MIDI\n\
                            messages it receives and script functions it loads\n\
\n\
    -f, --fullScreen        Starts Mixxx in full-screen mode\n\
\n\
    -h, --help              Display this help message and exit");
    
            printf("\n\n(For more information, see http://mixxx.org/wiki/doku.php/command_line_options)\n");
            return(0);
        }
        
        if (argv[i]==QString("-f").toLower() || argv[i]==QString("--f") || argv[i]==QString("--fullScreen"))
        {
            args.bStartInFullscreen = true;
        }
        else if (fileRx.indexIn(argv[i]) != -1)
            args.qlMusicFiles += argv[i];
    }
    
    
#ifdef __APPLE__
     qDebug() << "setting Qt's plugin seach path (on OS X)";
     QDir dir(QApplication::applicationDirPath());
     dir.cdUp();
     dir.cd("PlugIns");
     //For some reason we need to do setLibraryPaths() and not addLibraryPath().
     //The latter causes weird problems once the binary is bundled (happened with 1.7.2 when Brian packaged it up).
     QApplication::setLibraryPaths(QStringList(dir.absolutePath()));
#endif

    MixxxApp * mixxx=new MixxxApp(a, args);

    //a->setMainWidget(mixxx);
    a -> connect( a, SIGNAL(lastWindowClosed()), a, SLOT(quit()) );
    
    int result = -1;

    if (!dialogHelper->checkError()) {
        qDebug() << "Displaying mixxx";
        mixxx->show();
    
        qDebug() << "Running Mixxx";
        result = a->exec();
    }
    
    delete mixxx;
    
	qDebug() << "Mixxx shutdown complete.";

	// Don't make any more output after this
	//	or mixxx.log will get clobbered!
    if(Logfile.isOpen())
	Logfile.close();
    
    //delete plugin_paths;
    return result;
}

