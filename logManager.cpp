#include "logManager.h"
#include "xmlbase.h"
#include "log.h"

LogManager::LogManager()
{
	m_base = 0;
}

LogManager::~LogManager()
{
	if (m_base)
	{
		delete m_base;
		m_base = 0;
	}	
}

void LogManager::InitXmlBase()
{
	if (!m_base)
	{
		m_base = new XMLBase(LOG_FILE_PATH);
	}
}

void LogManager::SetLogFileInfo(LogInfo& _loginfo, const char* _filename)
{
	char *ca6100dir = getenv("CA6100DIR");
	if(ca6100dir == NULL)
	{
		return ;
	}
	memset(_loginfo.m_filepath, 0, MAX_LOGFILE_PATH);
	strncpy(_loginfo.m_filepath, ca6100dir, strlen(ca6100dir));
#if WIN32
	strcat(_loginfo.m_filepath, "\\log\\");
#else
	strcat(_loginfo.m_filepath, "/log/");
#endif
	strcat(_loginfo.m_filepath, _filename);
}

void LogManager::SetLevel(LogInfo& _loginfo)
{
	QString level = "";
	m_base->ReadXMLNode("LOGLEVEL", level);
	_loginfo.m_level = level.toInt();
}

void LogManager::SetInterval(LogInfo& _loginfo)
{
	QString interval = "";
	m_base->ReadXMLNode("LOGINTERVAL", interval);
	_loginfo.m_interval = interval.toInt();
}

void LogManager::InitLogInfo(LogInfo& _loginfo, const char* _filename)
{
	if (!m_base)
		return;	

	SetLevel(_loginfo);
	SetInterval(_loginfo);
	SetLogFileInfo(_loginfo, _filename);	
}

void LogManager::InitLog(const char* _filename)
{
	InitXmlBase();
	LogInfo info;
	InitLogInfo(info, _filename);
	sLog->InitLog(info.m_filepath, info.m_level, info.m_interval);
}
