# mychatserver
基于mymuduo网络库的集群聊天服务器

服务器1启动command: export LD_LIBRARY_PATH=/usr/local/lib:\$LD_LIBRARY_PATH
                    						./bin/ChatServer 127.0.0.1 6000
					
服务器2启动command: export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
                    						./bin/ChatServer 127.0.0.1 6002

客户端启动command: ./bin/ChatClient 127.0.0.1 8000

#### nginx tcp 负载均衡配置

	# nginx tcp loadbalance config
	stream{
		upstream MyServer{
			server 127.0.0.1:6000 weight=1 max_fails=3 fail_timeout=30s;
			server 127.0.0.1:6002 weight=1 max_fails=3 fail_timeout=30s;
		}
	
		server {
			proxy_connect_timeout 1s;
			listen 8000;
			proxy_pass MyServer;
			tcp_nodelay on;
		}
	}