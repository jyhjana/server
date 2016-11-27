#include "log.h"
#include <time.h>
#include <stdio.h>
#ifdef WIN32
#define snprintf _snprintf
#else
#endif


Log::Log()
:logfile(NULL),level(LOG_ALL),interval(0),next_rotate(0)
{
}

Log::Log(string fname,int loglevel)
:filename(fname),level(loglevel),interval(0),next_rotate(0)
{
	if(logfile)
	{
		fclose(logfile);
	}
	logfile =fopen(filename.c_str(),"w+");
	
}


Log::~Log()
{
	if(logfile)
	{
		fclose(logfile);
		logfile = NULL;
	}
}

int Log::InitLog(const char* logfile,int loglevel,int loginterval)
{
	
	int ret;
	string realname;
	
	filename = logfile;
	
    //IceUtil::Mutex::Lock lock(*this);
    Lock();
	realname = filename + "." + FormatDate();	
	ret = Initialize(realname.c_str());
	if(ret < 0)
	{
		Unlock();
        return ret;
	}
	level = loglevel;
	interval = loginterval;
	//用于换日志
	struct tm nextday;

	time_t now = time(NULL);
	struct tm *psttm;
	psttm = localtime(&now);
	nextday = *psttm;
	nextday.tm_hour = 0;
	nextday.tm_min = 0;
	nextday.tm_sec = 0;
	
	// 获得下一个日志轮转时间点零时，方便比较	
	next_rotate = mktime(&nextday); 
	next_rotate +=  interval*3600*24; //下一个间隔的零时开始轮转
	
	Unlock();
	return 0;
}
int Log::Initialize(const char* filename)
{
	if(logfile)
	{
		fclose(logfile);
	}
	logfile =fopen(filename,"a+");
	if(!logfile)
	{
		printf("can't open logfile: %s\n",filename);
		return -1;
	}
	return 0;
}

void Log::CheckLog(time_t timenow)
{
	//判断是否有需要改变日志文件
	if(timenow > next_rotate)  
	{
		string fname;
		fname = filename + "." + FormatDate();
		Initialize(fname.c_str());

		next_rotate = next_rotate + interval*3600*24;// 加上日志间隔
		
	}	
}

//输出时间戳
void Log::OutTimestamp(time_t timenow)
{
    tm* aTm = localtime(&timenow);
    //       YYYY   year
    //       MM     month (2 digits 01-12)
    //       DD     day (2 digits 01-31)
    //       HH     hour (2 digits 00-23)
    //       MM     minutes (2 digits 00-59)
    //       SS     seconds (2 digits 00-59)
    fprintf(logfile,"%-4d-%02d-%02d %02d:%02d:%02d ",aTm->tm_year+1900,aTm->tm_mon+1,aTm->tm_mday,aTm->tm_hour,aTm->tm_min,aTm->tm_sec);
}


string Log::FormatDate()
{
    time_t t = time(NULL);
    tm* aTm = localtime(&t);
	char buf[256];
	snprintf(buf,256,"%4d%02d%02d",aTm->tm_year+1900,aTm->tm_mon+1,aTm->tm_mday);
	return string(buf);
}




void Log::OutLog(const char * str,LOG_LEVEL loglevel, va_list args )
{
    if( !str ) return;

    if(logfile &&  level <= loglevel )
    {
		time_t t = time(NULL);
		
		Lock();	
		CheckLog(t);
		OutTimestamp(t);
        vfprintf(logfile, str, args);
        fflush(logfile);
		Unlock();
    }
}

/*
void Log::Debug(const char * str, ... )
{
    if( !str ) return;
    va_list ap;
    if( level <= LOG_DEBUG )
    {
        va_start(ap, str);
        vprintf( str, ap );
        va_end(ap);
    }
    if(logfile)
    {
		Lock();
        outTimestamp(logfile);
        va_start(ap, str);
        vfprintf(logfile, str, ap);
        va_end(ap);
        fflush(logfile);
		UnLock();
    }
}

void Log::Info(const char * str, ... )
{
    if( !str ) return;
    va_list ap;
    if( level <= LOG_INFO )
    {
        va_start(ap, str);
        vprintf( str, ap );
        va_end(ap);
    }
    if(logfile)
    {
        outTimestamp(logfile);
        va_start(ap, str);
        vfprintf(logfile, str, ap);
        va_end(ap);
        fflush(logfile);
    }
}

void Log::Warn(const char * str, ... )
{
    if( !str ) return;
    va_list ap;
    if( level <= LOG_WARN )
    {
        va_start(ap, str);
        vprintf( str, ap );
        va_end(ap);
    }
    if(logfile)
    {
        outTimestamp(logfile);
        va_start(ap, str);
        vfprintf(logfile, str, ap);
        va_end(ap);
        fflush(logfile);
    }
}

void Log::Error(const char * str, ... )
{
    if( !str ) return;
    va_list ap;
    if( level <= LOG_ERROR)
    {
        va_start(ap, str);
        vprintf( str, ap );
        va_end(ap);
    }
    if(logfile)
    {
        outTimestamp(logfile);
        va_start(ap, str);
        vfprintf(logfile, str, ap);
        va_end(ap);
        fflush(logfile);
    }
}

void Log::Fatal(const char * str, ... )
{
    if( !str ) return;
    va_list ap;
    if( level <= LOG_FATAL)
    {
        va_start(ap, str);
        vprintf( str, ap );
        va_end(ap);
    }
    if(logfile)
    {
        outTimestamp(logfile);
        va_start(ap, str);
        vfprintf(logfile, str, ap);
        va_end(ap);
        fflush(logfile);
    }
}
*/
