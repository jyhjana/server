/**
*@brief CommServer���ʵ���ļ�
*
*CommServer����һ��ͨ�õķ����������ܣ�֧��linux��epoll��windows��select
*��Ҫ����¼��Ĺ��������ʱ����socket�����¼���ѭ����
*@warning  ʹ��ʱ��̳з���������࣬��ʵ�����麯������:InitEvent,ProcessCmd��
*@author �ܽ�ƽ
*@date 2011-04-12
*@version �汾 1.0.1
*@since �汾����
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
*@brief ����������״̬
*
*�ñ���Ϊ��̬���Ա������״̬Ϊtrueʱ��ʾ�������У�false��ֹͣ����״̬
*�����ҪʹCommServer����ѭ���������У���Ҫ��m_bIsRunning����Ϊfalse
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
*@brief ���캯��
*
*��ʼ�������������:��win32�³�ʼ�������̽ӿ������
*linux�³�ʼ���¼�
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
*@brief ��������
*
*��������̽ӿڻ�ر��¼��ȣ��������������Դ
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
*@brief ��ȡ������
*
*/

void CommServer::GetHostName()
{
	char host[100];
	gethostname(host,sizeof(host));
	
	strupr(host);//���ַ���ת���ɴ�д״̬��2010.12.11 ���죩
	
	char *host1 = strchr(host,'.');
	if(host1!=NULL) *host1 = 0;
	if(strlen(host)>=HOSTNAMELEN) host[0] = 0;
	else strcpy(hostname,host);
}

#if 0
//���ز���Ϊbind�ɹ����Ӧ��fd��������-1
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
*@brief server��ĳ��IP�Ͷ˿��Ͻ�������socket
*
*�ú��������󶨵�ָ��IP��ַ�ͼ����˿ڣ����Ϊtcp�����������listen״̬��
*@param[in,out] sock:������Socket�����ָ��,�ò���ֵ����Ͳ���
*@param[in] port:ָ���Ķ˿�
*@param[in] port:ָ����Ҫ�󶨵�IP��ַ��Ĭ��Ϊ�գ���ʾ����󶨱�����һ��IP��ַ
*@return  ����0��ʾ�ɹ���<0��ʾ���󣬾������־��Ϣ
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


	///tcp�˿ڰ󶨺���listen
	if(sock->GetType() == Socket::TCP)
	{
		if(!sock->Listen())
		{
			return -1;
		}		
	}
	// ��ȫ�ɹ������� hubaihua
	AddSocket(sock);	
	return 0;
}
/**
*@brief ��ʼ��������
*
*�ú���Ϊ�麯������������ɳ�ʼ���ض��������Ļ���
*@return  ����0��ʾ�ɹ�
*@warning �������ʵ�ָ��麯��������CommServer����Ĭ��ʲôҲ����
*/
int CommServer::InitServer()
{
	return 0;
}

/**
*@brief ��ʼ���¼�
*
*��ʼ���ȴ��¼���fd(linux)���߶���д���쳣�ȵ�fdset
*@return  ����0��ʾ�ɹ���<0����
*@param[in] maxfd:����ָ�������Եȴ���������(linux��)
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
*@brief ��ʼ��timer
*
*��������timerʱ����߳�
*@param[in] threadnum:����timerʱ����߳���Ŀ��Ĭ��һ��
*@return  ����0��ʾ�ɹ���<0����
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
*@brief ����¼�
*
*�ú����������ָ�����¼�����������ָ����Socket����
*@param[in,out] sock:������Socket�����ָ��,�ò���ֵ����Ͳ���
*@param[in] eventtype:�¼����ͣ��μ�FD_STAT�궨��
*@return  ����0��ʾ�ɹ���<0��ʾ���󣬾������־��Ϣ
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
		// comment by caojianping 2011-09-02 // ETģʽ���ʺ�Ŀǰ����ֲ�����ܣ�������ʱ�ָ�
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
*@brief �޸��¼�
*
*�ú��������޸�ָ�����¼�����������ָ����Socket����
*@param[in,out] sock:������Socket�����ָ��,�ò���ֵ����Ͳ���
*@param[in] eventtype:�¼����ͣ��μ�FD_STAT�궨��
*@return  ����0��ʾ�ɹ���<0��ʾ���󣬾������־��Ϣ
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
		// comment by caojianping 2011-09-02 // ETģʽ���ʺ�Ŀǰ����ֲ�����ܣ�������ʱ�ָ�
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
*@brief ����¼�
*
*�ú�������ɾ��������ָ����Socket�����ϵ������¼�
*@param[in,out] sock:������Socket�����ָ��,�ò���ֵ����Ͳ���
*@return  ����0��ʾ�ɹ���<0��ʾ���󣬾������־��Ϣ
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
*@brief �����¼�
*
*�ú�����������ע�Ტ�����¼���ָ����Socket������
*����ʵ���ǣ���ɾ��ԭ���¼���������������¼�
*@param[in,out] sock:������Socket�����ָ��,�ò���ֵ����Ͳ���
*@param[in] eventtype:�¼����ͣ��μ�FD_STAT�궨��
*@return  ����0��ʾ�ɹ���<0��ʾ���󣬾������־��Ϣ
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
*@brief ���Socket����
*
*��Socket������ӵ��������Ķ����
*@param[in] sock:������Socket�����ָ��
*@return  ����0��ʾ�ɹ���<0��ʾ���󣬾������־��Ϣ
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
*@brief ɾ��Socket����
*
*��Socket����ӷ������Ķ������ɾ��
*@param[in] sock:������Socket�����ָ��
*@return  ����0��ʾ�ɹ���<0��ʾ���󣬾������־��Ϣ
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
*@brief ��ȡSocket����ָ��
*
*��Socket����ӷ������Ķ������ȡ��
*@param[in] fd: ������Socket������׽���
*@return  ����Socket�����ָ�룬NULL��ʾδ�ҵ��ö���
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
*@brief ����һ���µļ����ʱ��
*
*���ԭ��ʱ��timeid�Ѿ����ڣ�����¶�ʱ����ʱ����������������¶�ʱ��
*@param[in] timeid: timerΨһ��ʶ���Ӿ����Ӧ�ö���
*@param[in] interval: timer��ʱ��������λΪ����(Ŀǰ������СΪ10ms)
*@return  ����0��ʾ��ӳɹ�������Ϊ����
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
*@brief ɾ��һ�������ʱ��
*
*ɾ��ָ��timerid�Ķ�ʱ�������timerid��������ֱ�ӷ���
*@param[in] timeid: timerΨһ��ʶ���Ӿ����Ӧ�ö���
*@return  ����0��ʾɾ���ɹ�������Ϊ����
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
*@brief ��ȡ�����ʱ������
*
*��timeidΪ�������Ҽ����ʱ������
*@param[in] timeid: timerΨһ��ʶ���Ӿ����Ӧ�ö���
*@return  ���ؼ����ʱ�������ָ�룻�Ҳ�������NULL
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
*@brief �¼���ѭ��
*
*�¼���ѭ���д��������¼���timer�¼��������շ�������
*��m_bIsRunning����Ϊfalseʱ�Ƴ���ѭ��
*@return  0��ʾ�������أ�-1��ʾ�쳣
*/
int CommServer::Run()
{
	//��ʼ��ʱ���
	realCurrTime = 0;
    realPrevTime = getMSTime();

	//�����¼�ѭ��
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
*@brief �ȴ������¼�
*
*���¼���ѭ�����ã����������¼��ص���Ӧ��д����
*��m_bIsRunning����Ϊfalseʱ�Ƴ���ѭ��
*@return  0��ʾ�������أ�-1��ʾ�쳣
*/
int CommServer::WaitEvent()
{
#ifdef WIN32
	fd_set rfds = m_rfds;
	fd_set wfds = m_wfds;
	fd_set efds = m_efds;

	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 10000;   // microseconds  10ms ����
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
			socket_m::iterator iterNext = it;  ///< ������һ��λ�ã���Ϊ�м��п��ܱ�ɾ��
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
					ProcessClose(pSock);   // �ص��ϲ�ӿڣ����ṩcleanup���ֶ�
					delete pSock;
					it = iterNext;   // ���Ԫ�ر�ɾ�����ҵ���һ��λ��
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
				ProcessClose(pSocket);   // �ص��ϲ�ӿڣ����ṩcleanup���ֶ�
				delete pSocket;
			}
		}
    }
	return 0;
#endif

}

/**
*@brief ���������
*
*�ú���������ʵ�֣����������������������Ӧ�ò�Э����Ҫ��ɵ�����
*@param[in] sock: �������Socket����
*@return  0��ʾ�������أ�<0��ʾ�쳣
*/
int CommServer::ProcessCmd(Socket * /*sock*/)
{
	return 0;
}

/**
*@brief �׽��ֹرմ���
*
*�ú���������ʵ�֣���������ĳ���׽��ֱ��ر�ʱ��Ӧ�ò��һЩ������
*@param[in] sock: �������Socket����
*@return  0��ʾ�������أ�<0��ʾ�쳣
*/
int CommServer::ProcessClose(Socket * /*sock*/)
{
	return 0;
}



/**
*@brief ���緢��
*
*�����ͻ�����������͸������ӿڷ���
*@return  0��ʾ�������أ�<0��ʾ�쳣
*/
int CommServer::ProcessSend()
{
	socket_m::iterator iter;
	socket_m::iterator iterNext;
	Socket* pSocket;
	for(iter=m_sockets.begin();iter!=m_sockets.end();)
	{
		iterNext = iter;
		iterNext++;    ///< ��һ��λ��
		
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
*@brief ����ʱ��
*
*����ʱ�����鿴��ʱ���Ƿ�time out�����timeout�ص���Ӧ��timer��ʱ����
*@return  0��ʾ�������أ�<0��ʾ�쳣
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



