/**
*@brief socket�༰����������ʵ�ֺ���
*@author �ܽ�ƽ
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

///�����ջ�����
const int MAX_RCVBUF_SIZE = 4096;
///����ͻ�����
const int MAX_SNDBUF_SIZE = 4096;

/**
*@brief ���캯��
*@param[in] socktype: �����׽�����ʹ�õ�Э������
*/
Socket::Socket(PROTOCOL socktype):m_type(socktype)
{
	m_hSocket = INVALID_SOCKET;
	m_hServer = NULL;
}

/**
*@brief ���캯��
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
*@brief ��ȡ���һ�δ���
*
*��̬�������������һ�β����Ĵ�����
*@return ϵͳ������
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
*@brief �����׽���
*
*������Ĳ������ͣ���ʼ���׽���
*@param[in] af:Э���� 
*@param[in] type:�׽������ͣ���:SOCK_STREAM ����SOCK_DGRAM 
*@param[in] protocol:Э��
*@return ���ش����õ�socket�ļ������������ʧ�ܣ�����-1
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
*@brief ����CommServerָ��
*
*���ڽ�Socket���󸽼ӵ���Ӧ��CommServer����
*@param[in] pServer:CommServer�����ָ��
*/
void Socket::SetServer(CommServer *pServer)
{
	if(pServer)
	{
		m_hServer = pServer;
	}
}

/**
*@brief �󶨵�ip���˿ڵ�ַ��
*
*��Socket����󶨵�ָ����IP�Ͷ˿��ϣ�Ĭ�ϰ󶨵���������IP��
*@param[in] port:�˿ں�
*@param[in] address:IP��ַ��Ĭ��Ϊ�գ�����ָ��IP
*@return �ɹ�����true��ʧ�ܷ���false
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
*@brief �󶨵�ip���˿ڵ�ַ��
*
*��Socket����󶨵���SOCKADDR�ṹָ����IP�Ͷ˿��ϣ�
*@param[in] pSockAddr:��ַ�ṹ��Ϣ
*@param[in] addrlen:�ṹ��С
*@return �ɹ�����true��ʧ�ܷ���false
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
*@brief ����
*
*@param[in] backlog:��ѹ��������Ŀ
*@return �ɹ�����true��ʧ�ܷ���false
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
*@brief �����׽���ѡ��
*
*�μ�msdn��setsockopt������ϸ����
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
*@brief ��ȡ�׽���ѡ��
*
*�μ�msdn��getsockopt������ϸ����
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
*@brief �����׽���Ϊ������״̬
*
*@param[in] bNb: true��ʾ����Ϊ������false��ʾ���������־ 
*@return �ɹ�����true��ʧ�ܷ���false
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
*@brief �ر��׽���
*
*�ر��׽��֣������׽���ΪINVALID_SOCKET
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
*@brief ��ȫ�ر��׽���
*
*�ر��׽��֣������׽���ΪINVALID_SOCKET������ɾ��ע����¼���������
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
*@brief ������
*
*��socket�ж��¼��ϱ�ʱ���ص��ĺ���
*/
void Socket::OnRead()
{
}

/**
*@brief д����
*
*��socket��д�¼��ϱ�ʱ���ص��ĺ���
*/
void Socket::OnWrite()
{
}

/**
*@brief accept����
*
*��socket�ж��¼��ϱ�ʱ����socketΪ�����˿�ʱ�ص��ĺ���
*/
void Socket::OnAccept()
{
	
}

/**
*@brief �쳣����
*
*��socket���쳣�¼��ϱ�ʱ�ص��ĺ���
*/
void Socket::OnException()
{
	sLog->Info("Socket::OnException\n");
	Close();
	sLog->Info("Socket::OnException end\n");
	m_hServer->DelEvent(this);
	
}

/**
*@brief ������
*
*��socket�ж��¼��ϱ�ʱ���ص��ĺ�������Ϊ��Ϊ��������socket�����Ե���OnAccept����
*ʵ��Socket::OnAccept()
*/
void ListenSocket::OnRead()
{
	OnAccept();	
}

/**
*@brief accept����
*
*��OnRead�������ã�����һ���µ����ӣ������䵽�ͻ����ӱ���ע���д�¼�����ʼ���׽���ѡ��
*ʵ��Socket::OnAccept()
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
		if (fd == INVALID_SOCKET )   // ����linux��windowsƽ̨
		{
			int error = GetLastError();
			if(error == EINPROGRESS || error == EWOULDBLOCK || error == EAGAIN)
			{
				sLog->Debug("accept error,errno=%d\n",get_last_error());
				return;
			}
			continue;
		}
		//�������ƣ�����ر�
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
		pTmp->SetSockOpt(SOL_SOCKET,SO_KEEPALIVE,(char*)&optval,sizeof(optval));         //���ͱ�������
		pTmp->SetSockOpt(IPPROTO_TCP,TCP_NODELAY,(char*)&optval,sizeof(optval));         //��������ʱ�㷨 
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
*@brief ��������socket��
*
*������������·�Ƿ�ͨ��ʱʹ��:
*��������������·���˿ڣ��������������󣬼�accpet�������رա�
*�����ͻ���ͨ�����Ӹö˿ڣ��Ϳ���֪����·�Ƿ�ͨ����
*@return  ����ֵ����
*@note     ��������ڲ������ӵ�������;
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
*@brief ���캯��
*
*��ʼ�����պͷ��ͻ�����
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
*@brief ��������
*
*���ջ�����
*/
ClientSocket::~ClientSocket()
{
	m_rSize = 0;
	m_rOffset = 0;

	m_wSize = 0;
	m_wOffset = 0;
}

/**
*@brief ��ȡ����
*
*�Ӷ��������ж�ȡ���û�������
*@param[in,out] �û�������ָ�룬�뱣֤�䲻Ϊ��
*@param[in] �û���������С
*@return ���ζ�ȡ�����ķ������
*@retval < 0 ˵�������������鴫�����
*@retval >= 0 ���ؿ������û���������ʵ����Ŀ�������ϲ����ֵ�������Ƿ����¶�ȡ
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
*@brief ��������
*
*���û�������������д�����������д��������������������ǿ�Ʒ���д���������ݣ�
*����������㹻����ֱ�ӿ�����д���棬�ȴ���һ��д�¼�����ʱ����
*@param[in] �û�������ָ�룬�뱣֤�䲻Ϊ��
*@param[in] �û���������С
*@return ����д�����ķ������
*@return  = 0 ˵��������������޷������ڴ棬����ʵ�ʷ�������Ϊ0�������ϲ�����ñ��ĵķ���
*@return  < 0 ˵�����ʹ����ϲ����鲢���ոö���(�����new�����Ķ�����ɾ��)
*@return  > 0 ���ؿ��������ͻ�������ʵ����Ŀ�������ϲ����ֵ���ж��Ƿ���Ҫ�ط�
*/
int ClientSocket::WriteDate(const char* buf, int bufsize)
{
	if(!buf || bufsize<=0)
	{
		return 0;
	}
	int num_allocs = 0;   ///< ������3�Σ����໹����,˵���Ǹ��������Ҫtry again��
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
		wbufsize*=2;   ///< ָ������
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
*@brief ��������
*
*��socket�ж��¼�����ʱ�ص��ĺ�������ȡϵͳTCP�����������ݵ���������
*ʵ��Socket::OnRead()
*/
void ClientSocket::OnRead()
{
	//modify by caojianping 2011-5-31 begin ***
	m_hServer->ProcessCmd(this);   // �ص�Ӧ�ò㴦����ԭ�����ܻ���һ��
	return;

	// �����߼��������ˣ��ȴ��Ժ�ڶ����Ż����� Ŀǰ����Ӧ�ò�ֱ�Ӳ���ϵͳ��������������socket��װ�Ļ�������
	// �ش�˵�� !!!!!!!!!!!!!!!!!!   // comment by caojianping 2011-5-31
	//modify by caojianping 2011-5-31 end ***
	
	sLog->Info("ClientSocket::OnRead\n");
    int res;
    int num_allocs = 0;   // ������3�Σ����໹����,˵���Ǹ��������Ҫtry again��

    while (true) 
	{

		//m_hServer->UpdateEvent(this,FD_STAT_ALL|FD_STAT_ET);
		if (m_rOffset >= m_rSize)   // ƫ�����Ѿ����ڻ�������С��
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
        if (res == SOCKET_ERROR )   // ��ƽ̨�ļ�����
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
*@brief д����
*
*��socket��д�¼�����ʱ�ص��ĺ����������������д����͵����ݿ�����ϵͳTCP���������������������緢��
*ʵ��Socket::OnWrite()
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
				m_hServer->ModEvent(this,(FD_STAT_ALL&~FD_STAT_WRITE)|FD_STAT_ET);  // ȥ��д�¼����Ѿ�����
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
				m_hServer->ModEvent(this,FD_STAT_ALL|FD_STAT_ET);  // ���д�¼����ȴ��´��ٷ�
				break;
	        }
			SafeClose();   // �쳣�ر�
		}
		if(sndsize == 0)   // �����ر�
		{
			SafeClose();
			return;
		}
		
	}
}

/**
*@brief accept����
*
*��socket�ж��¼��ϱ�ʱ����socketΪ�����˿�ʱ�ص��ĺ���
*ʵ��Socket::OnAccept()
*/
void ClientSocket::OnAccept()
{
	sLog->Info("ClientSocket::OnAccept\n");
}

/**
*@brief �쳣����
*
*��socket���쳣�¼��ϱ�ʱ�ص��ĺ���
*ʵ��Socket::OnException()
*/
void ClientSocket::OnException()
{
	sLog->Info("ClientSocket::OnException\n");
	Close();
	sLog->Info("ClientSocket::OnException end\n");
	m_hServer->DelEvent(this);
}

/**
*@brief ������
*
*��socket�ж��¼��ϱ�ʱ���ص��ĺ���
*ʵ��Socket::OnRead()
*/
void UdpListenSocket::OnRead()
{
	//sLog->Info("UdpListenSocket::OnRead\n");
	//֪ͨӦ�ò�����绺����ֱ��ȡ
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



