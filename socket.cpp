/**
*@brief socket类及其相关子类的实现函数
*@author 曹建平
*@date 2011-04-19
*@version 1.0.1
*/
#include "socket.h"
#include "commserver.h"
#include "log.h"
#ifdef WIN32

#else
#include <errno.h>
#endif


int CloseSocket(SOCKET fd)
{
#ifdef WIN32
    return closesocket(fd);
#else
    return close(fd);
#endif
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

///最大接收缓存区
const int MAX_RCVBUF_SIZE = 4096;
///最大发送缓存区
const int MAX_SNDBUF_SIZE = 4096;

/**
*@brief 构造函数
*@param[in] socktype: 创建套接字所使用的协议类型
*/
Socket::Socket(PROTOCOL socktype):m_type(socktype)
{
	m_hSocket = INVALID_SOCKET;
	m_hServer = NULL;
}

/**
*@brief 构造函数
*/
Socket::~Socket()
{
	sLog->Debug("Socket::~Socket begin\n");
	Close();
	sLog->Debug("Socket::~Socket end\n");
	//m_hSocket = INVALID_SOCKET;
	m_hServer = NULL;
}

/**
*@brief 获取最后一次错误
*
*静态函数，返回最后一次操作的错误码
*@return 系统错误码
*/
int Socket::GetLastError()
{
#ifdef WIN32
	return ::GetLastError();
#else
	return errno;
#endif
}

/**
*@brief 创建套接字
*
*按传入的参数类型，初始化套接字
*@param[in] af:协议族 
*@param[in] type:套接字类型，如:SOCK_STREAM 或者SOCK_DGRAM 
*@param[in] protocol:协议
*@return 返回创建好的socket文件描述符，如果失败，返回-1
*/
SOCKET Socket::CreateSocket(int af,int type, const std::string& protocol)
{
	struct protoent *p = NULL;
	SOCKET s;
	
	if (protocol.size())
	{
		p = getprotobyname( protocol.c_str() );
		if (!p)
		{
			return INVALID_SOCKET;
		}
		if(protocol=="tcp")
		{
			m_type = TCP;
		}
		else if(protocol=="udp")
		{
			m_type = UDP;
		}
	}
	int protno = p ? p -> p_proto : 0;
	
	s = socket(af, type, protno);
	if (s == INVALID_SOCKET)
	{
		return INVALID_SOCKET;
	}
	m_hSocket = s;
	return s;
}

/**
*@brief 设置CommServer指针
*
*用于将Socket对象附加到对应的CommServer对象
*@param[in] pServer:CommServer对象的指针
*/
void Socket::SetServer(CommServer *pServer)
{
	if(pServer)
	{
		m_hServer = pServer;
	}
}

/**
*@brief 绑定到ip、端口地址对
*
*将Socket对象绑定到指定的IP和端口上，默认绑定到本机任意IP上
*@param[in] port:端口号
*@param[in] address:IP地址，默认为空，即不指定IP
*@return 成功返回true，失败返回false
*/
bool Socket::Bind(port_t port, const std::string& address)
{
	struct sockaddr_in addr;
    //bzero(&serv_addr,sizeof(struct sockaddr_in));
	memset(&addr,0,sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
	if(address=="")
	{
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
	}
	else
	{
		addr.sin_addr.s_addr = inet_addr(address.c_str());
	}

	if (bind(m_hSocket, (SOCKADDR *)&addr, sizeof(addr)) == SOCKET_ERROR)
	{
        CloseSocket(m_hSocket);
		return false;
	}
	return true;
	
}

/**
*@brief 绑定到ip、端口地址对
*
*将Socket对象绑定到由SOCKADDR结构指定的IP和端口上，
*@param[in] pSockAddr:地址结构信息
*@param[in] addrlen:结构大小
*@return 成功返回true，失败返回false
*/
bool Socket::Bind (const SOCKADDR* pSockAddr, int addrlen)
{
	//if (bind(m_hSocket, pSockAddr, sizeof(SOCKADDR)) == -1)
	if (bind(m_hSocket, pSockAddr, addrlen) == SOCKET_ERROR)
	{
        CloseSocket(m_hSocket);
		return false;
	}
	return true;
}


/**
*@brief 监听
*
*@param[in] backlog:积压的连接数目
*@return 成功返回true，失败返回false
*/
bool Socket::Listen(int backlog)
{
	if (listen(m_hSocket, backlog) == SOCKET_ERROR)
	{
        CloseSocket(m_hSocket);
		return false;
	}
	return true;
}

/**
*@brief 设置套接字选项
*
*参见msdn的setsockopt函数详细描述
*@param[in] level:Level at which the option is defined; the supported levels include SOL_SOCKET and IPPROTO_TCP. 
*See Windows Sockets 2 Protocol-Specific Annex for more information on protocol-specific levels. 
* 
*@param[in] optname:Socket option for which the value is to be set. 
*@param[in] optval:Pointer to the buffer in which the value for the requested option is supplied. 
*@param[in] optlen:Size of the optval buffer
*@return If no error occurs, setsockopt returns zero. Otherwise, a value of SOCKET_ERROR is returned, 
*and a specific error code can be retrieved by calling GetLastError.
*/
int Socket::SetSockOpt(int level, int optname, const char *optval, socklen_t optlen)
{
	return setsockopt(m_hSocket, level, optname, optval, optlen);
}


/**
*@brief 获取套接字选项
*
*参见msdn的getsockopt函数详细描述
*@param[in] level: Level at which the option is defined; the supported levels include SOL_SOCKET and IPPROTO_TCP. 
*See Windows Sockets 2 Protocol-Specific Annex for more information on protocol-specific levels. 
*@param[in] optname: Socket option for which the value is to be retrieved. 
*@param[out] optval: Pointer to the buffer in which the value for the requested option is to be returned. 
*@param[in, out] optlen: Pointer to the size of the optval buffer. 
*@return If no error occurs, getsockopt returns zero. Otherwise, a value of SOCKET_ERROR is returned,
*and a specific error code can be retrieved by calling GetLastError.
*/
int Socket::GetSockOpt(int level, int optname, char *optval,socklen_t *optlen)
{
	int ret;
    ret = getsockopt(m_hSocket,level,optname,optval,optlen);
	return ret;
}


/**
*@brief 设置套接字为非阻塞状态
*
*@param[in] bNb: true表示设置为阻塞，false表示清空阻塞标志 
*@return 成功返回true，失败返回false
*/
bool Socket::SetNonblocking(bool bNb)
{
#ifdef WIN32
	unsigned long l = bNb ? 1 : 0;
	int n = ioctlsocket(m_hSocket, FIONBIO, &l);
	if (n != 0)
	{
		return false;
	}
	return true;
#else
	if (bNb)
	{
		if (fcntl(m_hSocket, F_SETFL, O_NONBLOCK) == -1)
		{
			return false;
		}
	}
	else
	{
		if (fcntl(m_hSocket, F_SETFL, 0) == -1)
		{
			return false;
		}
	}
	return true;
#endif
}


/**
*@brief 关闭套接字
*
*关闭套接字，并置套接字为INVALID_SOCKET
*/
void Socket::Close()
{
	if(m_hSocket != INVALID_SOCKET)
	{
		sLog->Info("Socket::Close fd=%d\n",m_hSocket);
		CloseSocket(m_hSocket);
		sLog->Info("Socket::Close fd=%d\n end",m_hSocket);
		m_hSocket=INVALID_SOCKET;
	}
}

/**
*@brief 安全关闭套接字
*
*关闭套接字，并置套接字为INVALID_SOCKET，并做删除注册的事件等清理工作
*/
void Socket::SafeClose()
{
	m_hServer->DelEvent(this);
	m_hServer->DelSocket(this);
	sLog->Debug("SafeClose begin\n");
	Close();	
	sLog->Debug("SafeClose end\n");
}

/**
*@brief 读函数
*
*当socket有读事件上报时，回调的函数
*/
void Socket::OnRead()
{
}

/**
*@brief 写函数
*
*当socket有写事件上报时，回调的函数
*/
void Socket::OnWrite()
{
}

/**
*@brief accept函数
*
*当socket有读事件上报时，且socket为监听端口时回调的函数
*/
void Socket::OnAccept()
{
	
}

/**
*@brief 异常函数
*
*当socket有异常事件上报时回调的函数
*/
void Socket::OnException()
{
	sLog->Info("Socket::OnException\n");
	Close();
	sLog->Info("Socket::OnException end\n");
	m_hServer->DelEvent(this);
	
}

/**
*@brief 读函数
*
*当socket有读事件上报时，回调的函数，因为其为监听类型socket，所以调用OnAccept函数
*实现Socket::OnAccept()
*/
void ListenSocket::OnRead()
{
	OnAccept();	
}

/**
*@brief accept函数
*
*被OnRead函数调用，接收一个新的连接，并分配到客户连接表，并注册读写事件，初始化套接字选项
*实现Socket::OnAccept()
*/
void ListenSocket::OnAccept()
{
	while(true)
	{
	    struct sockaddr_in client;
	    socklen_t len;
	    len = sizeof(client);
		
		int fd;
		fd = accept(m_hSocket, (struct sockaddr *) &client, &len);  
		if (fd == INVALID_SOCKET )   // 兼容linux和windows平台
		{
			int error = GetLastError();
			if(error == EINPROGRESS || error == EWOULDBLOCK || error == EAGAIN)
			{
				sLog->Debug("accept error,errno=%d\n",get_last_error());
				return;
			}
			continue;
		}
		//超出限制，必须关闭
		#ifdef WIN32
		#else
		if(fd > FD_SETSIZE)
		{
			CloseSocket(fd);
			return;
		}
		#endif
		Socket* pTmp = new ClientSocket;
		pTmp->SetSocket((SOCKET)fd);
		pTmp->SetNonblocking(true);
		///set socket option
		int optval=1;
		pTmp->SetSockOpt(SOL_SOCKET,SO_KEEPALIVE,(char*)&optval,sizeof(optval));         //发送保持连接
		pTmp->SetSockOpt(IPPROTO_TCP,TCP_NODELAY,(char*)&optval,sizeof(optval));         //不采用延时算法 
	    linger lng;
	    lng.l_linger=0;
	    lng.l_onoff=1;
	    pTmp->SetSockOpt(SOL_SOCKET,SO_LINGER,(char*)&lng,sizeof(lng));
		///set socket buffer
		int temp;
		temp = RXTXBUFSIZE;
		pTmp->SetSockOpt(SOL_SOCKET,SO_SNDBUF,(char*)&temp,sizeof(temp));
		pTmp->SetSockOpt(SOL_SOCKET,SO_RCVBUF,(char*)&temp,sizeof(temp));	
		
		///set server
		pTmp->SetServer(m_hServer);
		pTmp->SetPort(m_port);   ///< attach the port
		///add event
		m_hServer->AddEvent(pTmp,FD_STAT_READ|FD_STAT_EXCEPT|FD_STAT_ET );   // FD_STAT_ALL|FD_STAT_ET);
		///add to active sockets map
		m_hServer->AddSocket(pTmp);
		sLog->Info("accept new fd=%d from listen fd=%d\n",fd,m_hSocket);
	}
		
}

/**
*@brief 测试连接socket类
*
*仅用来测试链路是否通畅时使用:
*服务器监听在链路检查端口，当有连接上来后，即accpet后立即关闭。
*这样客户端通过连接该端口，就可以知道链路是否通畅。
*@return  返回值描述
*@note     该类仅用于测试连接的特殊用途
*@see ListenSocket::OnAccept() 
*/
void TestListenSocket::OnAccept()
{
    struct sockaddr_in client;
    socklen_t len;
    len = sizeof(client);
	
	int fd;
	fd = accept(m_hSocket, (struct sockaddr *) &client, &len);  
	if (fd == INVALID_SOCKET)
	{
		return;
	}
	#if 0
	string msg="connect success\n";
	send(fd,msg.c_str(),msg.length(),0);
	delay_t(1);
	#endif
	CloseSocket(fd);
	
}

/**
*@brief 构造函数
*
*初始化接收和发送缓存区
*/
ClientSocket::ClientSocket()
{
	m_rBuffer.alloc(MAX_RCVBUF_SIZE);          // read buffer
	m_wBuffer.alloc(MAX_SNDBUF_SIZE);          // write buffer

	if(m_rBuffer == NULL || m_wBuffer == NULL)
	{
		exit(-1);
	}
	m_rSize = MAX_RCVBUF_SIZE;              // read buffer total size
	m_rOffset = 0;            				// read buffer offset
        
	m_wSize = MAX_SNDBUF_SIZE;              // write buffer total size
	m_wOffset = 0;            				// write buffer offset
	
}

/**
*@brief 析构函数
*
*回收缓存区
*/
ClientSocket::~ClientSocket()
{
	m_rSize = 0;
	m_rOffset = 0;

	m_wSize = 0;
	m_wOffset = 0;
}

/**
*@brief 读取数据
*
*从读缓存区中读取到用户缓存区
*@param[in,out] 用户缓存区指针，请保证其不为空
*@param[in] 用户缓存区大小
*@return 本次读取操作的返回情况
*@retval < 0 说明错误发生，请检查传入参数
*@retval >= 0 返回拷贝到用户缓存区的实际数目，建议上层检查该值，决定是否重新读取
*/
int ClientSocket::ReadData(char* buf, int bufsize)
{
	if(!buf)
	{
		return -1;
	}
	if(bufsize > m_rOffset)  // buffer not read ready, for next try
	{
		return 0;  // read nothing // indicate reader to try again
	}

	memcpy(buf,m_rBuffer,bufsize);
	memmove(m_rBuffer,m_rBuffer+bufsize,m_rOffset-bufsize);
	m_rOffset-=bufsize;
	return bufsize;
}


/**
*@brief 发送数据
*
*从用户缓存区拷贝到写缓存区，如果写缓存区不够，则先试着强制发送写缓存中数据；
*如果缓存区足够，则直接拷贝至写缓存，等待下一次写事件到来时发送
*@param[in] 用户缓存区指针，请保证其不为空
*@param[in] 用户缓存区大小
*@return 本次写操作的返回情况
*@return  = 0 说明参数错误或者无法分配内存，导致实际发送数据为0，建议上层废弃该报文的发送
*@return  < 0 说明发送错误，上层需检查并回收该对象(如果是new出来的对象，需删除)
*@return  > 0 返回拷贝到发送缓存区的实际数目，建议上层检查该值，判断是否需要重发
*/
int ClientSocket::WriteDate(const char* buf, int bufsize)
{
	if(!buf || bufsize<=0)
	{
		return 0;
	}
	int num_allocs = 0;   ///< 最多分配3次，过多还不够,说明是个大包，得要try again了
	int wbufsize = m_wSize;
	int wsize = 0;

	///buffer is going to full, kick to send now
	if(bufsize > wbufsize-m_wOffset)
	{
		OnWrite();   ///< must check the socket state because of writting failure
		if(IsClosed())
		{
			return -1;
		}	
	}
	else  ///< enough
	{
		memcpy(m_wBuffer+m_wOffset,buf,bufsize);
		m_wOffset+=bufsize;	
		return bufsize;
	}

	///rellocate memory
	while((bufsize > wbufsize-m_wOffset) && num_allocs++ < 4)
	{
		wbufsize*=2;   ///< 指数增长
	}
	if(num_allocs == 4)  ///< too big packet
	{
		wsize = wbufsize - m_wOffset;  ///< send partially
	}
	else
	{
		wsize = bufsize;
	}
	m_wBuffer.realloc(wbufsize);
    if (!m_wBuffer) 
	{
		m_wSize = 0;
        return 0;
    }
	m_wSize = wbufsize;   ///< new buffer size
	memcpy(m_wBuffer+m_wOffset,buf,wsize);
	m_wOffset+=wsize;
	return wsize;

}

/**
*@brief 接收数据
*
*当socket有读事件发生时回调的函数，读取系统TCP缓存区的数据到读缓存区
*实现Socket::OnRead()
*/
void ClientSocket::OnRead()
{
	//modify by caojianping 2011-5-31 begin ***
	m_hServer->ProcessCmd(this);   // 回调应用层处理，与原代码框架基本一致
	return;

	// 以下逻辑都不走了，等待以后第二版优化处理。 目前，由应用层直接操作系统缓存区，而不是socket封装的缓冲区。
	// 特此说明 !!!!!!!!!!!!!!!!!!   // comment by caojianping 2011-5-31
	//modify by caojianping 2011-5-31 end ***
	
	sLog->Info("ClientSocket::OnRead\n");
    int res;
    int num_allocs = 0;   // 最多分配3次，过多还不够,说明是个大包，得要try again了

    while (true) 
	{

		//m_hServer->UpdateEvent(this,FD_STAT_ALL|FD_STAT_ET);
		if (m_rOffset >= m_rSize)   // 偏移量已经大于缓存区大小了
		{
            if (num_allocs == 4) 
			{
				if(m_hServer->UpdateEvent(this, FD_STAT_ALL) < 0)
				{
					SafeClose();
				}
                return; 
            }
            ++num_allocs;
			m_rBuffer.realloc(m_rSize*2);
            if (!m_rBuffer) 
			{
                //m_rSize = 0; 
				m_rOffset = 0;/* ignore what we read */
                return;
            }
            m_rSize *= 2;  // offset not changed
        }

        int avail = m_rSize - m_rOffset;
        res = recv(m_hSocket, (char*)m_rBuffer + m_rOffset, avail,0);
        if (res > 0) 
		{
			m_rOffset += res;
            if (res == avail)  // maybe not complete ! to be continued ...
			{
                continue;
            } 
			else 
			{
				//to do working here ...
				//echo here
				//just move to send buffer
				m_hServer->ProcessCmd(this);   // process cmd
				//memcpy(m_wBuffer,m_rBuffer,m_rOffset); //just for echo test
				//m_wOffset = m_rOffset;
				//m_rOffset = 0;
				//
                break;   // read completed
            }
        }
        if (res == 0)   // close normally
		{
			SafeClose();
			return;
        }
        if (res == SOCKET_ERROR )   // 跨平台的兼容性
		{
			sLog->Debug("fd =%d;recv err =%d\n",m_hSocket,GetLastError() );
			//no more data to read
            //if (errno == EAGAIN || errno == EWOULDBLOCK) 
            if ( GetLastError() == EINPROGRESS || GetLastError() == EWOULDBLOCK || GetLastError() == EAGAIN) 
			{
                break;
            }

			SafeClose();
			return;
			
        }
    }	
}

/**
*@brief 写数据
*
*当socket有写事件发生时回调的函数，将读缓存区中待发送的数据拷贝到系统TCP缓存区，驱动真正的网络发送
*实现Socket::OnWrite()
*/
void ClientSocket::OnWrite()
{
	//sLog->Info("ClientSocket::OnWrite\n");
	if(m_wOffset == 0)
	{
		return;   // nothing to snd  // maybe LT mode use this
	}
	sLog->Info("ClientSocket::OnWrite\n");
	int size;
	int sndsize=0;

	while(true)
	{
		size = m_wOffset;
        sndsize = send(m_hSocket,m_wBuffer,size,0);
		//socket buffer not enough
		if(sndsize > 0)
		{
			if(sndsize >= size)
			{
				m_wOffset = 0;   // write complete
				m_hServer->ModEvent(this,(FD_STAT_ALL&~FD_STAT_WRITE)|FD_STAT_ET);  // 去掉写事件，已经发完
				return;
			}
			else
			{
				memmove(m_wBuffer,m_wBuffer+sndsize,m_wOffset-sndsize);  // remain buffer
				m_wOffset -= sndsize;
				continue;
			}
		}
		if (sndsize == SOCKET_ERROR)
		{
			if( GetLastError()== EINPROGRESS || GetLastError() == EWOULDBLOCK || GetLastError() == EAGAIN  ) 
			{
				m_hServer->ModEvent(this,FD_STAT_ALL|FD_STAT_ET);  // 添加写事件，等待下次再发
				break;
	        }
			SafeClose();   // 异常关闭
		}
		if(sndsize == 0)   // 正常关闭
		{
			SafeClose();
			return;
		}
		
	}
}

/**
*@brief accept函数
*
*当socket有读事件上报时，且socket为监听端口时回调的函数
*实现Socket::OnAccept()
*/
void ClientSocket::OnAccept()
{
	sLog->Info("ClientSocket::OnAccept\n");
}

/**
*@brief 异常函数
*
*当socket有异常事件上报时回调的函数
*实现Socket::OnException()
*/
void ClientSocket::OnException()
{
	sLog->Info("ClientSocket::OnException\n");
	Close();
	sLog->Info("ClientSocket::OnException end\n");
	m_hServer->DelEvent(this);
}

/**
*@brief 读函数
*
*当socket有读事件上报时，回调的函数
*实现Socket::OnRead()
*/
void UdpListenSocket::OnRead()
{
	//sLog->Info("UdpListenSocket::OnRead\n");
	//通知应用层从网络缓冲区直接取
	m_hServer->ProcessCmd(this); 
/*
	char buf[1024];
	struct sockaddr_in from;
	int fromlen = sizeof(sockaddr_in);
	int n=recvfrom (
		m_hSocket,                   
		buf,              
		sizeof(buf),                    
		0,                  
		(struct sockaddr *) &from,  
		&fromlen            
		);*/
	//sLog->Info("read byte:%d\n",n);

}



