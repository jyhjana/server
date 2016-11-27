/**
*@brief CommServer类的实现文件
*
*CommServer类是一个通用的服务器程序框架，支持linux下epoll和windows下select
*主要完成事件的管理，间隔定时器，socket管理，事件主循环等
*@warning  使用时请继承服务器框架类，并实现期虚函数，如:InitEvent,ProcessCmd等
*@author 曹建平
*@date 2011-04-12
*@version 版本 1.0.1
*@since 版本建立
*/
#ifdef WIN32
#include <process.h>
#else
#include <pthread.h>
#endif
#include "commserver.h"
#include "log.h"
#include <stdio.h>
#define MAX_FD 128

/**
*@brief 服务器运行状态
*
*该变量为静态类成员变量，状态为true时表示正在运行，false这停止运行状态
*如果需要使CommServer的主循环结束运行，需要将m_bIsRunning设置为false
*/
bool CommServer::m_bIsRunning = true;

#ifdef WIN32
void GlobalThreadFun(void *arg)
#else
void *GlobalThreadFun(void *arg)
#endif
{
	CommServer* pServer = (CommServer*) arg;
	pServer->ProcessTimer();


#ifdef WIN32
	return ;
#else
	return (void*)0;
#endif
}
/**
*@brief 构造函数
*
*初始化网络参数，如:在win32下初始化网络编程接口软件；
*linux下初始化事件
*/
CommServer::CommServer()
{
#ifdef WIN32
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;
	
	wVersionRequested = MAKEWORD( 2, 2 );
	
	err = WSAStartup( wVersionRequested, &wsaData );
	if ( err != 0 ) {
		/* Tell the user that we could not find a usable */
		/* WinSock DLL.                                  */
		exit(-1);
	}
	
	if ( LOBYTE( wsaData.wVersion ) != 2 ||
        HIBYTE( wsaData.wVersion ) != 2 ) {
		/* Tell the user that we could not find a usable */
		/* WinSock DLL.                                  */
		WSACleanup();
		exit(-1); 
	}
#else

#endif
	if(InitEvent(1024) < 0)
	{
		exit(-1); 
	}	

	
}

/**
*@brief 析构函数
*
*清除网络编程接口或关闭事件等，析构掉分配的资源
*/
CommServer::~CommServer()
{
	//clear all socket object
    for(socket_m::iterator iter =m_sockets.begin();iter!=m_sockets.end();iter++)
    {
		DelEvent(iter->second);
		delete(iter->second);
    }
	m_sockets.clear();

	//clear all timers
	for(timer_m::iterator iter=m_timers.begin();iter!=m_timers.end();iter++) 
	{
		delete(iter->second);
	}
	m_timers.clear();
	
#ifdef WIN32
	WSACleanup();
#else
	if(m_epollfd > 0)
	{
		close(m_epollfd);
	}
	m_epollfd = INVALID_SOCKET;
#endif

}

/**
*@brief 获取主机名
*
*/

void CommServer::GetHostName()
{
	char host[100];
	gethostname(host,sizeof(host));
	
	strupr(host);//把字符串转换成大写状态（2010.12.11 王庆）
	
	char *host1 = strchr(host,'.');
	if(host1!=NULL) *host1 = 0;
	if(strlen(host)>=HOSTNAMELEN) host[0] = 0;
	else strcpy(hostname,host);
}

#if 0
//返回参数为bind成功后对应的fd，出错则-1
int CommServer::Open(port_t port,std::string protocol,std::string ip)
{
	std::map<port_t,std::string>::iterator iter;
	iter = acceptor.find(port);
	if(iter != acceptor.end())   // port already open 
	{
		return -1;
	}
	Socket* pAcceptor;
	
	if(protocol=="tcp")
	{
		if(port == LINKPORT )
		{
			pAcceptor = new TestListenSocket;
		}
		else
		{
			pAcceptor = new ListenSocket;
		}
		
		if ( pAcceptor->CreateSocket(AF_INET, SOCK_STREAM, "tcp") == INVALID_SOCKET)
		{
			return -1;
		}

		if(!pAcceptor->Bind(port,ip))
		{
			return -1;
		}
		int on=1;
		pAcceptor->SetSockOpt(SOL_SOCKET, SO_REUSEADDR, (char *) &on, sizeof(on));
		pAcceptor->SetNonblocking(true);
		if(!pAcceptor->Listen())
		{
			return -1;
		}
	}
	else if(protocol == "udp")
	{
		pAcceptor = new UdpListenSocket;
		
		if ( pAcceptor->CreateSocket(AF_INET, SOCK_DGRAM, "udp") == INVALID_SOCKET)
		{
			return -1;
		}

		if(!pAcceptor->Bind(port,ip))
		{
			return -1;
		}
		int temp;
		temp = RXTXBUFSIZE;
		pAcceptor->SetSockOpt(SOL_SOCKET,SO_SNDBUF,(char*)&temp,sizeof(temp));
		pAcceptor->SetSockOpt(SOL_SOCKET,SO_RCVBUF,(char*)&temp,sizeof(temp));
		temp = 1;
		pAcceptor->SetSockOpt(SOL_SOCKET,SO_BROADCAST,(char*)&temp,sizeof(temp));		
	}
		
	pAcceptor->SetServer(this);  // must
	pAcceptor->SetPort(port);    // set the bind port

	acceptor[port] = ip;  
	//add to event poll
 	AddEvent(pAcceptor,FD_STAT_READ|FD_STAT_EXCEPT|FD_STAT_ET);
 	AddSocket(pAcceptor);

	sLog->Info("bind port:%d ok\n",port);
	
	return (int)pAcceptor->GetSocket();	
}
#endif

/**
*@brief server在某个IP和端口上建立监听socket
*
*该函数用来绑定到指定IP地址和监听端口，如果为tcp服务器则进入listen状态。
*@param[in,out] sock:操作的Socket对象的指针,该参数值结果型参数
*@param[in] port:指定的端口
*@param[in] port:指定需要绑定的IP地址，默认为空，表示随机绑定本机的一个IP地址
*@return  返回0表示成功，<0表示错误，具体见日志信息
*@see Socket 
*/
int CommServer::Open(Socket* sock,port_t port,std::string ip)
{
	if(!sock)
	{
		return -1;
	}

	if(!sock->Bind(port,ip))
	{
		sLog->Error("bind port:%d error\n",port);
		return -2;
	}	
	sLog->Info("bind port:%d ok\n",port);
	
	sock->SetServer(this);  // must
	sock->SetPort(port);    // set the bind port
	
	///add to event poll

 	if (0 != AddEvent(sock,FD_STAT_READ|FD_STAT_EXCEPT|FD_STAT_ET))
		return -1;


	///tcp端口绑定后需listen
	if(sock->GetType() == Socket::TCP)
	{
		if(!sock->Listen())
		{
			return -1;
		}		
	}
	// 完全成功后才添加 hubaihua
	AddSocket(sock);	
	return 0;
}
/**
*@brief 初始化服务器
*
*该函数为虚函数，给子类完成初始化特定服务器的机会
*@return  返回0表示成功
*@warning 子类必须实现该虚函数，否则CommServer基类默认什么也不做
*/
int CommServer::InitServer()
{
	return 0;
}

/**
*@brief 初始化事件
*
*初始化等待事件的fd(linux)或者读、写、异常等的fdset
*@return  返回0表示成功，<0出错
*@param[in] maxfd:用于指定最大可以等待的描述符(linux下)
*/
int CommServer::InitEvent(int maxfd)
{
#ifdef WIN32
	FD_ZERO(&m_rfds);
	FD_ZERO(&m_wfds);
	FD_ZERO(&m_efds);
	maxfd = 0;
	m_maxsock = maxfd;
#else
    m_epollfd=epoll_create(maxfd);
    if(m_epollfd < 0)
    {
		return -1;
    }
#endif
	return 0;
}

/**
*@brief 初始化timer
*
*创建处理timer时间的线程
*@param[in] threadnum:处理timer时间的线程数目，默认一个
*@return  返回0表示成功，<0出错
*/
int CommServer::InitTimer(int threadnum)
{

	for(int i=0;i<threadnum;i++)
	{
#ifdef WIN32
		_beginthread(GlobalThreadFun,0,this);
#else
		pthread_t tid;
		pthread_create(&tid ,NULL,GlobalThreadFun,this);
#endif
	}
	return 0;
}




/**
*@brief 添加事件
*
*该函数用来添加指定的事件，并关联到指定的Socket对象
*@param[in,out] sock:操作的Socket对象的指针,该参数值结果型参数
*@param[in] eventtype:事件类型，参见FD_STAT宏定义
*@return  返回0表示成功，<0表示错误，具体见日志信息
*@see FD_STAT 
*/
int CommServer::AddEvent(Socket* sock,int eventtype)
{
	if(sock->GetSocket()==INVALID_SOCKET)
	{
		return -1;
	}
	SOCKET s = sock->GetSocket();

	bool bRead,bWrite,bException;
	if(eventtype == FD_STAT_ALL)
	{
		bRead = bWrite = bException = true;
	}
	else
	{
		bRead = (eventtype & FD_STAT_READ) ? true:false;
		bWrite = (eventtype & FD_STAT_WRITE) ? true:false;
		bException = (eventtype & FD_STAT_EXCEPT)? true:false;
	}

#ifdef WIN32	
	if (bRead)
	{
		if (!FD_ISSET(s, &m_rfds))
		{
			FD_SET(s, &m_rfds);
		}
	}
	else
	{
		FD_CLR(s, &m_rfds);
	}
	if (bWrite)
	{
		if (!FD_ISSET(s, &m_wfds))
		{
			FD_SET(s, &m_wfds);
		}
	}
	else
	{
		FD_CLR(s, &m_wfds);
	}
	if (bException)
	{
		if (!FD_ISSET(s, &m_efds))
		{
			FD_SET(s, &m_efds);
		}
	}
	else
	{
		FD_CLR(s, &m_efds);
	}
	if(sock->GetSocket() > m_maxsock)
	{
		m_maxsock = sock->GetSocket();
	}
	return 0;	
#else
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    
    ev.data.fd = s;
	ev.events = 0;

	if(bRead)
	{
		ev.events = ev.events | EPOLLIN | EPOLLPRI;
	}
	if(bWrite)
	{
		ev.events = ev.events | EPOLLOUT;
	}
	if(bException)
	{
		//ev.events = ev.events | EPOLLERR | EPOLLRDHUP | EPOLLHUP;
		ev.events = ev.events | EPOLLERR | EPOLLHUP;   // some system not surport
	}
	if(eventtype & FD_STAT_ET)
	{
		// comment by caojianping 2011-09-02 // ET模式不适合目前的移植程序框架，待改造时恢复
		//ev.events = ev.events | EPOLLET;   // edge trigger
	}
	
    if(epoll_ctl(m_epollfd,EPOLL_CTL_ADD,s,&ev) < 0)
    {
		sLog->Error("%s:%d->epoll add event error,errno=%d:%s\n",__FILE__,__LINE__,get_last_error(),strerror(get_last_error()));
        return  -1;
    }
	sLog->Info("%s:%d->epoll add event fd=%d\n",__FILE__,__LINE__,s);
    return 0;	

#endif
}


/**
*@brief 修改事件
*
*该函数用来修改指定的事件，并关联到指定的Socket对象
*@param[in,out] sock:操作的Socket对象的指针,该参数值结果型参数
*@param[in] eventtype:事件类型，参见FD_STAT宏定义
*@return  返回0表示成功，<0表示错误，具体见日志信息
*@see FD_STAT 
*/
int CommServer::ModEvent(Socket* sock,int eventtype)
{
	if(sock->GetSocket()==INVALID_SOCKET)
	{
		return -1;
	}
	SOCKET s = sock->GetSocket();

	bool bRead,bWrite,bException;
	if(eventtype == FD_STAT_ALL)
	{
		bRead = bWrite = bException = true;
	}
	else
	{
		bRead = (eventtype & FD_STAT_READ) ? true:false;
		bWrite = (eventtype & FD_STAT_WRITE) ? true:false;
		bException = (eventtype & FD_STAT_EXCEPT)? true:false;
	}

#ifdef WIN32	
	if (bRead)
	{
		if (!FD_ISSET(s, &m_rfds))
		{
			FD_SET(s, &m_rfds);
		}
	}
	else
	{
		FD_CLR(s, &m_rfds);
	}
	if (bWrite)
	{
		if (!FD_ISSET(s, &m_wfds))
		{
			FD_SET(s, &m_wfds);
		}
	}
	else
	{
		FD_CLR(s, &m_wfds);
	}
	if (bException)
	{
		if (!FD_ISSET(s, &m_efds))
		{
			FD_SET(s, &m_efds);
		}
	}
	else
	{
		FD_CLR(s, &m_efds);
	}
	if(sock->GetSocket() > m_maxsock)
	{
		m_maxsock = sock->GetSocket();
	}
	return 0;	
#else
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    
    ev.data.fd = s;
	ev.events = 0;

	if(bRead)
	{
		ev.events = ev.events | EPOLLIN | EPOLLPRI;
	}
	if(bWrite)
	{
		ev.events = ev.events | EPOLLOUT;
	}
	if(bException)
	{
		//ev.events = ev.events | EPOLLERR | EPOLLRDHUP | EPOLLHUP;
		ev.events = ev.events | EPOLLERR | EPOLLHUP;   // some system not surport
	}
	if(eventtype & FD_STAT_ET)
	{
		// comment by caojianping 2011-09-02 // ET模式不适合目前的移植程序框架，待改造时恢复
		//ev.events = ev.events | EPOLLET;   // edge trigger
	}
	
    if(epoll_ctl(m_epollfd,EPOLL_CTL_MOD,s,&ev) < 0)
    {
		sLog->Error("%s:%d->epoll modify event error,errno=%d:%s\n",__FILE__,__LINE__,get_last_error(),strerror(get_last_error()));
        return  -1;
    }
	sLog->Info("%s:%d->epoll modify event fd=%d\n",__FILE__,__LINE__,s);
    return 0;	

#endif
}



/**
*@brief 添加事件
*
*该函数用来删除关联到指定的Socket对象上的所有事件
*@param[in,out] sock:操作的Socket对象的指针,该参数值结果型参数
*@return  返回0表示成功，<0表示错误，具体见日志信息
*@see FD_STAT 
*/
int CommServer::DelEvent(Socket* sock)
{
	if(sock->GetSocket()==INVALID_SOCKET)
	{
		return -1;
	}
	SOCKET s = sock->GetSocket();
#ifdef WIN32
	FD_CLR(s, &m_rfds);
	FD_CLR(s, &m_wfds);
	FD_CLR(s, &m_efds);

	return 0;
#else
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    
    if(epoll_ctl(m_epollfd,EPOLL_CTL_DEL,s,NULL) < 0)
    {
        return  -1;
    }
    
    return 0;	
#endif
}


/**
*@brief 更新事件
*
*该函数用来重新注册并关联事件到指定的Socket对象上
*具体实现是，先删除原来事件，在重新添加新事件
*@param[in,out] sock:操作的Socket对象的指针,该参数值结果型参数
*@param[in] eventtype:事件类型，参见FD_STAT宏定义
*@return  返回0表示成功，<0表示错误，具体见日志信息
*@see FD_STAT 
*/
int CommServer::UpdateEvent(Socket* sock,int eventtype)
{
	int ret = 0;
	ret = DelEvent(sock);
	if(ret < 0)
	{
		return -1;
	}
	ret = AddEvent(sock,eventtype);
	if(ret < 0)
	{
		return -1;
	}
	return ret;
}

/**
*@brief 添加Socket对象
*
*将Socket对象添加到服务器的对象池
*@param[in] sock:操作的Socket对象的指针
*@return  返回0表示成功，<0表示错误，具体见日志信息
*/
int CommServer::AddSocket(Socket* sock)
{
	if(m_sockets.find(sock->GetSocket())==m_sockets.end())
	{
		m_sockets[sock->GetSocket()] = sock;
	}
	return 0;
}


/**
*@brief 删除Socket对象
*
*将Socket对象从服务器的对象池中删除
*@param[in] sock:操作的Socket对象的指针
*@return  返回0表示成功，<0表示错误，具体见日志信息
*/
int CommServer::DelSocket(Socket* sock)
{
	if(m_sockets.find(sock->GetSocket())!=m_sockets.end())
	{
		m_sockets.erase(sock->GetSocket());
	}
	return 0;
}

/**
*@brief 获取Socket对象指针
*
*将Socket对象从服务器的对象池中取出
*@param[in] fd: 关联到Socket对象的套接字
*@return  返回Socket对象的指针，NULL表示未找到该对象
*/
Socket* CommServer::GetSocket(SOCKET fd)
{
	if(m_sockets.find(fd)!=m_sockets.end())
	{
		return m_sockets[fd];
	}
	return NULL;
}


/**
*@brief 设置一个新的间隔定时器
*
*如果原定时器timeid已经存在，则更新定时器计时间隔；否则，这生成新定时器
*@param[in] timeid: timer唯一标识，视具体的应用而定
*@param[in] interval: timer的时间间隔，单位为毫秒(目前精度最小为10ms)
*@return  返回0表示添加成功；其他为错误
*/
int CommServer::SetTimer(unsigned int timeid,int interval)
{
	m_timerLock.Lock();
	if(m_timers.find(timeid)==m_timers.end())
	{
		IntervalTimer* pNewTimer = new IntervalTimer;
		pNewTimer->SetTimerId(timeid);
		pNewTimer->SetInterval(interval);
		m_timers[timeid] = pNewTimer;
	}
	m_timers[timeid]->SetInterval(interval);
	m_timerLock.Unlock();
	return 0;
}

/**
*@brief 删除一个间隔定时器
*
*删除指定timerid的定时器，如果timerid不存在则直接返回
*@param[in] timeid: timer唯一标识，视具体的应用而定
*@return  返回0表示删除成功；其他为错误
*/
int CommServer::DelTimer(unsigned int timeid)
{	
	m_timerLock.Lock();
	if(m_timers.find(timeid)!=m_timers.end())
	{
		delete m_timers[timeid];
		m_timers.erase(timeid);
	}
	m_timerLock.Unlock();
	return 0;
}



/**
*@brief 获取间隔定时器对象
*
*以timeid为索引查找间隔定时器对象
*@param[in] timeid: timer唯一标识，视具体的应用而定
*@return  返回间隔定时器对象的指针；找不到返回NULL
*/
IntervalTimer* CommServer::GetTimer(unsigned int timeid)
{
	if(m_timers.find(timeid)==m_timers.end())
	{
		return NULL;
	}
	return m_timers[timeid];
}


/**
*@brief 事件主循环
*
*事件主循环中处理网络事件、timer事件、网络收发的事务；
*当m_bIsRunning设置为false时推出主循环
*@return  0表示正常返回，-1表示异常
*/
int CommServer::Run()
{
	//初始化时间差
	realCurrTime = 0;
    realPrevTime = getMSTime();

	//进入事件循环
	while(m_bIsRunning)
	{
		if(WaitEvent()<0)
		{
			sLog->Error("server stop running for event error!\n");
			return -1;
		}
		
		//ProcessSend(); 
		//process timer
		ProcessTimer();
	}
	sLog->Info("server stop running normally!\n");
	return 0;
}

/**
*@brief 等待网络事件
*
*被事件主循环调用，处理网络事件回调相应读写操作
*当m_bIsRunning设置为false时推出主循环
*@return  0表示正常返回，-1表示异常
*/
int CommServer::WaitEvent()
{
#ifdef WIN32
	fd_set rfds = m_rfds;
	fd_set wfds = m_wfds;
	fd_set efds = m_efds;

	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 10000;   // microseconds  10ms 精度
	int n;
	n = select(m_maxsock,&rfds,&wfds,&efds,&tv);
	if(n == -1)
	{
		for (SOCKET i = 0; i <= m_maxsock; i++)
		{
			bool t = false;
			FD_ZERO(&rfds);
			FD_ZERO(&wfds);
			FD_ZERO(&efds);
			if (FD_ISSET(i, &m_rfds))
			{
				FD_SET(i, &rfds);
				t = true;
			}
			if (FD_ISSET(i, &m_wfds))
			{
				FD_SET(i, &wfds);
				t = true;
			}
			if (FD_ISSET(i, &m_efds))
			{
				FD_SET(i, &efds);
				t = true;
			}
			if (t && m_sockets.find(i) == m_sockets.end())
			{
				fprintf(stderr, "Bad fd in fd_set: %d\n", i);
			}
		}
	}
	else if(n==0)
	{
		// time out

	}
	else if(n > 0)
	{
		for (socket_m::iterator it = m_sockets.begin(); it != m_sockets.end(); )
		{
			socket_m::iterator iterNext = it;  ///< 记下下一个位置，因为中间有可能被删除
			iterNext++;
			
			SOCKET i = it->first;
			Socket *pSock = it->second;
			if (FD_ISSET(i, &rfds))
			{
				pSock->OnRead();
			}
			if (FD_ISSET(i, &wfds))
			{
				pSock->OnWrite();
			}
			if (FD_ISSET(i, &efds))
			{
				pSock->OnException();
			}
			if(pSock)
			{
				if(pSock->IsClosed())
				{
					ProcessClose(pSock);   // 回调上层接口，以提供cleanup的手段
					delete pSock;
					it = iterNext;   // 如果元素被删除，找到下一个位置
				}
				else
				{
					it++;
				}
				
			}
		}
	}
	return 0;
	
#else
	struct epoll_event events[MAX_FD];  
	int nfds;
	nfds=epoll_wait(m_epollfd,events,MAX_FD,5);  //millisecond 

	for(int i=0;i<nfds;++i) 
	{
		Socket* pSocket = NULL;
		if((events[i].events& EPOLLIN) 
			||(events[i].events& EPOLLPRI))   // read or accept
		{ 
			pSocket = GetSocket((SOCKET)events[i].data.fd);
			if(pSocket)
			{
				pSocket->OnRead();
			}
		}
		if(events[i].events& EPOLLOUT )   // write
		{ 
			pSocket = GetSocket((SOCKET)events[i].data.fd);
			if(pSocket)
			{
				pSocket->OnWrite();
			}
		}
		if((events[i].events& EPOLLERR) 
			//||(events[i].events& EPOLLRDHUP)     // some system not surport
			||(events[i].events& EPOLLHUP))   // exception or error
		{ 
			pSocket = GetSocket((SOCKET)events[i].data.fd);
			if(pSocket)
			{
				pSocket->OnException();
			}
		}
		if(pSocket)
		{
			if(pSocket->IsClosed())
			{
				ProcessClose(pSocket);   // 回调上层接口，以提供cleanup的手段
				delete pSocket;
			}
		}
    }
	return 0;
#endif

}

/**
*@brief 网络命令处理
*
*该函数被子类实现，用来处理具体的网络命令等应用层协议需要完成的任务
*@param[in] sock: 待处理的Socket对象
*@return  0表示正常返回，<0表示异常
*/
int CommServer::ProcessCmd(Socket * /*sock*/)
{
	return 0;
}

/**
*@brief 套接字关闭处理
*
*该函数被子类实现，用来处理某个套接字被关闭时，应用层的一些清理工作
*@param[in] sock: 待处理的Socket对象
*@return  0表示正常返回，<0表示异常
*/
int CommServer::ProcessClose(Socket * /*sock*/)
{
	return 0;
}



/**
*@brief 网络发送
*
*将发送缓存区的数据透过网络接口发送
*@return  0表示正常返回，<0表示异常
*/
int CommServer::ProcessSend()
{
	socket_m::iterator iter;
	socket_m::iterator iterNext;
	Socket* pSocket;
	for(iter=m_sockets.begin();iter!=m_sockets.end();)
	{
		iterNext = iter;
		iterNext++;    ///< 下一个位置
		
		pSocket = iter->second;
		if(!pSocket)
		{
			continue;  // that's impossible!! here
		}
		pSocket->OnWrite();   // deal send
		if(pSocket->IsClosed())   // if close when writting
		{
			delete pSocket;
			pSocket=NULL;
			iter = iterNext;
		}
		else
		{
			iter++;
		}
	}
	return 0;
}


/**
*@brief 处理定时器
*
*处理定时器，查看定时器是否time out，如果timeout回调对应的timer到时处理
*@return  0表示正常返回，<0表示异常
*/
int CommServer::ProcessTimer()
{
    realCurrTime = getMSTime();
    unsigned int diff;
    if (realPrevTime > realCurrTime) // getMSTime() have limited data range and this is case when it overflow in this tick   
        diff = 0xFFFFFFFF - (realPrevTime - realCurrTime);
    else
        diff = realCurrTime - realPrevTime;
	m_timerLock.Lock();
	if(diff > 100)   // 100ms  accuracy > 100ms
	{
		timer_m::iterator iter;
		
		for(iter=m_timers.begin();iter!=m_timers.end();iter++)
		{
			if(iter->second->GetCurrent()>=0)
			{
				iter->second->Update(diff);
			}
			else 
			{
				iter->second->SetCurrent(0);		
			}
		}
		
		realPrevTime = realCurrTime;	
	}
	//process timer if someone timeout 
	OnTimer();
	m_timerLock.Unlock();
	return 0;
}



