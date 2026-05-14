#include"client.h"

// 确保包含 <arpa/inet.h> 用于 htonl 和 ntohl

// 辅助函数：严格读取指定长度的字节，防止 TCP 半包
int Client::Recv_N(int fd, char* buf, int exact_len)
{
    int left = exact_len;
    char* ptr = buf;
    while(left > 0)
    {
        int n = recv(fd, ptr, left, 0);
        if(n <= 0) return n; // 发生错误或对端关闭
        left -= n;
        ptr += n;
    }
    return exact_len;
}

// 客户端发指令专用：带 4 字节包头发送
void Client::Send_Framed(const std::string& msg)
{
    uint32_t net_len = htonl(msg.length());
    std::string packet;
    packet.append((char*)&net_len, 4);
    packet.append(msg);
    send(sockfd, packet.c_str(), packet.size(), 0);
}

// 客户端收指令/数据专用：安全拆解 4 字节包头
bool Client::Recv_Framed(std::string& out_msg)
{
    uint32_t net_len = 0;
    // 1. 先死等 4 个字节的包头
    if(Recv_N(sockfd, (char*)&net_len, 4) <= 0) return false;
    
    // 2. 解析包体长度
    uint32_t msg_len = ntohl(net_len);
    if(msg_len > 10 * 1024 * 1024) return false; // 防御恶意包
    
    // 3. 准备空间，死等 msg_len 个字节
    out_msg.resize(msg_len);
    if(Recv_N(sockfd, &out_msg[0], msg_len) <= 0) return false;
    
    return true;
}
bool Client::Connect_Ser()
{
	sockfd = socket(AF_INET,SOCK_STREAM,0);
	if(-1 == sockfd)
	{
		return false;
	}
	struct sockaddr_in saddr;
	memset(&saddr,0,sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(port);
	saddr.sin_addr.s_addr = inet_addr(ip.c_str());
	int res = connect(sockfd,(struct sockaddr *)&saddr,sizeof(saddr));
	if(-1 == res)
	{
		return false;
	}
	return true;
}
void Client::print()
{
	if(!flg)
	{
		cout<<"1 注册 2登录 3退出"<<endl;
		cout<<"请输入选择:"<<endl;
		cin>>op;
		if(op==3)
		{
			op=USEREXIT;
		}
	}
	else
	{
		cout<<"-----用户:"<<username<<"---------"<<endl;
			cout<<"1 查看服务器文件  2 下载  3 上传  4 新建目录"<<endl;
			cout<<"5 删除文件  6 重命名  7 进入目录  8 返回"<<endl;
			cout<<"9 移动文件  10 保存分享链接  11 分享文件链接 12 退出"<<endl;
			cout<<"请输入要执行的操作编号："<<endl;
			cin>>op;
			if(12==op)
			{
				op = USEREXIT;
				return;
			}
			op+=2;
	}
}

void Client::Register()
{
	cout<<"请输入手机号码"<<endl;
	cin>>usertel;
	cout<<"请输入用户名"<<endl;
	cin>>username;
	cout<<"请输入密码"<<endl;
	string pw1,pw2;
	cin>>pw1;
	cout<<"请再次输入密码"<<endl;
	cin>>pw2;
	if(usertel.empty()||username.empty())
	{
		cout<<"用户名或手机号码不能为空"<<endl;
		return;
	}
	if(pw1.empty()||pw2.empty())
	{
		cout<<"密码不能为空"<<endl;
		return;
	}else if(pw1!=pw2)
	{
		cout<<"两次密码不一致"<<endl;
		return;
	}
	Json::Value val;
	val["type"]=REGISTER;
	val["usertel"]=usertel;
	val["passwd"]=pw1;
	val["username"] = username;
	val["path"] = Path;
	
	Send_Framed(val.toStyledString());
	char buff[128] ={0};
	int n = recv(sockfd,buff,127,0);
	if(n<=0)
	{
		cout<<"ser close"<<endl;
		return;
	}
	Json::Value resval;
	Json::Reader Read;
	if(!Read.parse(buff,resval))
	{
		cout<<"Json解析失败"<<endl;
		return;
	}

	string s = resval["status"].asString();
	if(s!="OK")
	{
		cout<<"注册失败"<<endl;
	}
	cout<<"注册成功"<<endl;
	flg=true;
	return;
}


void Client::Login()
{
	string usertel;
	string passwd;
	cout<<"请输入手机号"<<endl;
	cin>>usertel;
	cout<<"请输入密码"<<endl;
	cin>>passwd;
	if(usertel.empty()||passwd.empty())
	{
		cout<<"手机号密码不为空"<<endl;
		return;
	}
	Json::Value val;
	val["type"] = LOGIN; 
	val["usertel"] = usertel; 
	val["passwd"] = passwd;
	val["path"] = Path;
	Send_Framed(val.toStyledString());
	string res_str;
    if(!Recv_Framed(res_str)) {
        cout << "服务器未响应或断开连接" << endl;
        return;
    }

	Json::Reader Read;
	Json::Value resval;
	if(!Read.parse(res_str, resval)) 
	{
		cout<<"json无法解析"<<endl;
		return;
	}
	string s = resval["status"].asString();
	if(s!="OK")
	{
		cout<<"登陆失败"<<endl;
		return;
	}
	cout<<"登陆成功"<<endl;
	username = resval["username"].asString();
	usertel1 = usertel;
	flg = true;
	return;

}

void Client::ShowFiles()
{
	Json::Value val;
	val["type"] = SHOWFILES;
	Send_Framed(val.toStyledString());
	char buff[4096] = {0};
	int n = recv(sockfd,buff,4096,0);
	if(n<=0)
	{
		cout<<"ser close"<<endl;
		return;
	}
	Json::Value resval;
	Json::Reader Read;
	if(!Read.parse(buff,resval))
	{
		cout<<"json解析失败"<<endl;
		return;
	}
	string s= resval["status"].asString();
	if(s != "OK")
	{
		cout<<"查看失败"<<endl;
		return;
	}
	int ndirs = resval["ndirs"].asInt();
	cout<<"目录文件:"<<endl;
	for(int i =0;i<ndirs;i++)
	{
		cout<<i<<". "<<resval["arrdir"][i]["filename"].asString()<<endl;
	}

	cout<<"普通文件:"<<endl;
	int nfiles = resval["nfiles"].asInt();
	for(int i = 0;i<nfiles;i++)
	{
		cout<<i<<". "<<resval["arrfile"][i]["filename"].asString()<<endl;
	}
}

void print_info(int percentage)
{
	const int barwidth = 30;
	std::cout<<"\r";
	std::cout<<"[";
	int pos = barwidth * percentage /100;
	for(int i =0;i<barwidth;i++)
	{
		if(i<pos)
		{
			std::cout<<"=";
		}else if(i==pos){
			std::cout<<">";
		}else{
			std::cout<<" ";
		}
	}
	std::cout<<"]"<<percentage<<"%";
	if(percentage==100)
	{
		std::cout<<std::endl;
	}
	std::cout.flush();
}

void Client::GetFile()
{
    string fname;
    cout << "请输入要下载的文件" << endl;
    cin >> fname;
    if(fname.empty()) {
        cout << "文件名不能为空" << endl;
        return;
    }

    // 1. 发送带包头的请求指令
    string q_str = "get start " + fname;
    Send_Framed(q_str);

    // 2. 接收带包头的响应指令
    string res_str;
    if(!Recv_Framed(res_str)) {
        cout << "服务器断开连接" << endl;
        return;
    }

    if(res_str == "ERR") {
        cout << "无法下载，文件可能不存在" << endl;
        return;
    }

    // 解析格式："OK [filesize]"
    if(res_str.substr(0, 2) != "OK") {
        cout << "报文解析错误" << endl;
        return;
    }

    // 提取文件大小 (跳过 "OK ")
    long long filesize = atoll(res_str.c_str() + 3);
    cout << "文件大小:" << filesize << " 字节" << endl;
    cout << "是否下载？(y/n)" << endl;
    string sel;
    cin >> sel;

    // 加入 O_TRUNC，如果是重新下载，清空原来的旧文件数据
    int fd = open(fname.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0664);
    if(fd == -1 || sel == "n") {
        Send_Framed("get stop");
        return;
    }

    long long cur_filesize = 0;
    cout << ".........开始下载........" << endl;

    // 3. 循环拉取数据块
    while(true)
    {
        Send_Framed("get continue");
        
        string chunk;
        if(!Recv_Framed(chunk)) {
            cout << "\n接受数据错误或者服务器关闭" << endl;
            Send_Framed("get stop");
            break;
        }

        // 检查状态指令
        if(chunk == "EOF") {
            cout << "\n下载完成!" << endl;
            break;
        }
        if(chunk == "ERR") {
            cout << "\n服务器读取文件出错" << endl;
            break;
        }

        // 把拆包后的纯二进制数据写入本地文件
        write(fd, chunk.data(), chunk.size());
        cur_filesize += chunk.size();
        
        // 更新进度条
        int r = static_cast<int>((cur_filesize * 100.0) / filesize);
        print_info(r);
    }
    close(fd);
}

string CalcFileMD5(const std::string& path) {
    unsigned char c[MD5_DIGEST_LENGTH];
    char buf[512];
    MD5_CTX ctx;
    MD5_Init(&ctx);

    std::ifstream file(path, std::ios::binary);
    while (file.read(buf, sizeof(buf)))
        MD5_Update(&ctx, buf, file.gcount());
    if (file.gcount() > 0)
        MD5_Update(&ctx, buf, file.gcount());

    MD5_Final(c, &ctx);
    char md5str[33];
    for (int i = 0; i < 16; ++i)
        sprintf(&md5str[i * 2], "%02x", c[i]);
    return std::string(md5str);
}


void Client::UpFile()
{
    string fname;
    cout << "请输入要上传的文件名" << endl;
    cin >> fname;

    int fd = open(fname.c_str(), O_RDONLY);
    if (fd == -1) {
        cout << "无法打开文件" << endl;
        return;
    }

    long long filesize = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    string filemd5 = CalcFileMD5(fname);

    // 1. 发送带包头的文件信息命令
    string up_cmd = "up " + fname + " " + to_string(filesize) + " " + filemd5 + " " + usertel1;
    Send_Framed(up_cmd);

    // 2. 接收服务器对于该文件的状态判定
    string res_str;
    if (!Recv_Framed(res_str)) {
        cout << "服务器拒绝上传或已断开" << endl;
        close(fd);
        return;
    }
    
    // 秒传判断
    if (res_str == "EXIST") {
        cout << "相同文件已存在，【秒传成功】" << endl;
        close(fd);
        return;
    }
    if (res_str == "ERR") {
        cout << "服务器发生错误，终止上传" << endl;
        close(fd);
        return;
    }
    
    // 提取断点续传的位置 (此时 res_str 里面存的就是纯数字的 string)
    long long resume_pos = atoll(res_str.c_str());
    if (resume_pos > 0) {
        cout << "触发断点续传，继续位置：" << resume_pos << " 字节" << endl; 
    }
    lseek(fd, resume_pos, SEEK_SET);

    cout << ".........开始上传........" << endl;

    // 3. 进入纯二进制传输模式：扩大每次读取的缓冲区，提升性能
    char buff[4096]; 
    int num = 0;
    long long sent = resume_pos;

    while ((num = read(fd, buff, sizeof(buff))) > 0) 
    {
        // 核心注意：这里绝对不能用 Send_Framed！必须发纯粹的原生字节流！
        int n = send(sockfd, buff, num, 0);
        if(n <= 0) {
            cout << "\n网络异常，上传中断" << endl;
            break;
        }
        sent += n;

        int percent = static_cast<int>((sent * 100.0) / filesize);
        print_info(percent);
    }

    close(fd);
    
    if (sent < filesize) {
        cout << "\n上传未完成，已发送：" << sent << " / " << filesize << " 字节" << endl;
    } else {
        cout << "\n上传完成：" << fname << " (" << sent << " 字节)" << endl;
    }
}

void Client::MkDir()
{
    cout << "请输入新建目录的名称: " << endl;
    string dname;
    cin >> dname;
    if(dname.empty())
    {
        cout << "目录名不能为空！" << endl;
        return;
    }
    
    Json::Value val;
    val["type"] = MKDIR;
    val["dirname"] = dname;
    
    // 1. 发送：你已经改对啦！
    Send_Framed(val.toStyledString());
    
    // ==========================================
    // 2. 接收：告别原生 recv 和 char 数组，使用拆包函数！
    // ==========================================
    string res_str;
    if(!Recv_Framed(res_str))
    {
        cout << "服务器未响应或断开连接" << endl;
        return;
    }

    // 3. 解析：直接解析干净的 res_str 字符串
    Json::Value rval;
    Json::Reader Read;
    if(!Read.parse(res_str, rval))
    {
        cout << "Json 无法解析！" << endl;
        return;
    }
    
    // 4. 判断结果
    string s = rval["status"].asString();
    if(s != "OK")
    {
        cout << "创建失败" << endl;
        return;
    }
    cout << "创建成功" << endl;
    return;
}

void Client::RmFile()
{
	cout<<"请输入要删除的文件: "<<endl;
	string fname;
	cin>>fname;
	if(fname.empty())
	{
		cout<<"要删除的文件不能为空！"<<endl;
		return;
	}
	Json::Value val;
	val["type"] = RMFILE;
	val["filename"] = fname;       
	Send_Framed(val.toStyledString());

	string res_str;
    if(!Recv_Framed(res_str))
    {
        cout<<"服务器未响应或断开连接"<<endl;
        return;
    }
	Json::Reader Read;
	Json::Value rval;
	if(!Read.parse(res_str,rval))//这个if的语句如果可以解析，那么这句话是否已经执行，是否后面的内容是已经解析过的？答:已经执行过
	{
		cout<<"Json 无法解析！"<<endl;
		return;
	}
	string s = rval["status"].asString();
	if(s!="OK")
	{
		cout<<"删除失败！"<<endl;
		return;
	}
	cout<<"删除成功"<<endl;
	return;


}

void Client::ReName()
{
	while(true){
	cout<<"请输入要修改的文件名"<<endl;
	string sname;
	if (!(cin >> sname)) {
		cin.clear(); // 恢复流状态
		cin.ignore(10000, '\n'); // 清空缓冲区里的垃圾
		cout << "输入异常，退出重命名！" << endl;
		break; 
	}
	cout<<"请输入新名字"<<endl;
	string tname;
	if (!(cin >> tname)) {
		cin.clear();
		cin.ignore(10000, '\n');
		cout << "输入异常，退出重命名！" << endl;
		break;
	}

	Json::Value val;
	val["type"] = MVNAME;
	val["sname"] = sname;
	val["tname"] = tname;
	Send_Framed(val.toStyledString());
	string res_str;
    if(!Recv_Framed(res_str))
    {
        cout<<"服务器未响应或断开连接"<<endl;
        return;
    }
	Json::Value rval;
	Json::Reader Read;
	if(!Read.parse(res_str,rval))
	{
		cout<<"Json解析失败"<<endl;
		return;
	}
	
	string s = rval["status"].asString();
	if(s=="OK")
	{
		cout<<"修改成功！"<<endl;
		break;
	}else if(s=="EXIT")
	{
		cout<<"目标文件名已经存在，请重新输入"<<endl;
		continue;

	}else{
		cout<<"错误！"<<endl;
		break;
	}
	
	}
	return;
}


void Client::ChDir()
{
	cout<<"请输入要进入的目录名:"<<endl;
	string dname;
	cin>>dname;
	Json::Value val;
	val["type"] = CHDIR;
	val["dirname"] = dname;
	Send_Framed(val.toStyledString());

	string res_str;
    if(!Recv_Framed(res_str))
    {
        cout<<"服务器未响应或断开连接"<<endl;
        return;
    }
	Json::Value rval;
	Json::Reader Read;
	if(!Read.parse(res_str,rval))
	{
		cout<<"Json 无法解析！"<<endl;
		return;
	}
	string s = rval["status"].asString();
	if(s!="OK")
	{
		cout<<"切换路径失败"<<endl;
		return;
	}
	cout<<"已经进入"<<dname<<"目录"<<endl;
}
void Client::Ret()
{
	Json::Value val;
	val["type"] = RET;
	Send_Framed(val.toStyledString());
	string res_str;
    if(!Recv_Framed(res_str))
    {
        cout<<"服务器未响应或断开连接"<<endl;
        return;
    }
	Json::Value rval;
	Json::Reader Read;
	if(!Read.parse(res_str,rval))
	{
		cout<<"Json解析失败"<<endl;
		return;
	}

	string s = rval["status"].asString();
	if(s!="OK")
	{
		cout<<"返回失败"<<endl;
		return;
	}
	cout<<"返回成功"<<endl;
	return;
}
void Client::MvFile()
{
	while(true)
	{
		cout<<"请输入要移动的文件或目录的路径(含文件名，从当前路径开始算):"<<endl;
		string src;
		cin>>src;
		cout<<"请输入目标路径(含文件名，从当前路径开始算):"<<endl;
		string dst;
		cin>>dst;
		if(src.empty()||dst.empty())
		{
			cout<<"文件名或者路径不能为空，清重新输入！"<<endl;
			continue;
		}
		Json::Value val;
		val["type"] = MVFILE;
		val["src"] = src;
		val["dst"] = dst;

		Send_Framed(val.toStyledString());
		string res_str;
		if(!Recv_Framed(res_str))
		{
			cout<<"服务器未响应或断开连接"<<endl;
			return;
		}

		Json::Value rval;
		Json::Reader Read;
		if(!Read.parse(res_str,rval))
		{
			cout<<"Json 解析失败！"<<endl;
			return;

		}
		string s = rval["status"].asString();
		if(s=="OK")
		{
			cout<<"移动成功"<<endl;
			break;
		}else if(s=="EXITS")
		{
			cout<<"文件当目标路径下已经存在，是否移动到其他路径？(y/n)"<<endl;
			string tptr;
			cin>>tptr;
			if(tptr=="y")
			{
				continue;
			}else{
				break;
			}
			
		}else if(s == "NOFILE"){
			cout<<"源文件不存在，是否重新输入？(y/n)"<<endl;
			string tptr;
			cin>>tptr;
			if(tptr=="y")
			{
				continue;
			}else{
				break;
			}
		}else{
			cout<<"移动失败！"<<endl;
			break;
		}


	}
	return;
	
}
void Client::ShareFile()
{
    cout << "请输入要分享的文件名: ";
    string fname;
    cin >> fname;

    Json::Value val;
    val["type"] = SHARE_FILE; // 你需要在协议头文件里加上这个枚举
    val["filename"] = fname;
    Send_Framed(val.toStyledString());

    string res_str;
    if(!Recv_Framed(res_str)) return;

    Json::Reader Read; 
	Json::Value rval;
    Read.parse(res_str, rval);

    if (rval["status"].asString() == "OK") {
        cout << "\n分享成功！" << endl;
        cout << "分享链接 (Share ID): " << rval["share_id"].asString() << endl;
        cout << "提取码 (Code): " << rval["code"].asString() << endl;
        cout << "有效期: 7天" << endl << endl;
    } else {
        cout << "分享失败，请检查文件是否存在！" << endl;
    }
}
void Client::SaveShare()
{
    cout << "请输入分享链接 (Share ID): ";
    string share_id;
    cin >> share_id;
    
    cout << "请输入 4 位提取码: ";
    string code;
    cin >> code;

    Json::Value val;
    val["type"] = SAVE_SHARE; // 协议头文件里加上这个枚举
    val["share_id"] = share_id;
    val["code"] = code;
    Send_Framed(val.toStyledString());

    string res_str;
    if(!Recv_Framed(res_str)) return;

    Json::Reader Read; Json::Value rval;
    Read.parse(res_str, rval);

    string s = rval["status"].asString();
    if (s == "OK") {
        cout << " 转存成功！文件已保存到您的网盘。" << endl;
    } else if (s == "INVALID") {
        cout << " 转存失败：提取码错误或链接已过期！" << endl;
    } else if (s == "MISSING") {
        cout << " 转存失败：该文件已被原作者彻底删除！" << endl;
    } else {
        cout << "服务器异常！" << endl;
    }
}
void Client::Run()
{
    bool r = true;
    while(r)
    {
        print();
        switch(op)
        {
            case REGISTER:
				Register();
				break;
			case LOGIN:
				Login();
				break;
			case SHOWFILES:
				ShowFiles();
				break;
			case GET:
				GetFile();
				break;
			case POST:
				UpFile();
				break;
			case MKDIR:
				MkDir();
				break;
			case RMFILE:
				RmFile();
				break;
			case MVNAME:
				ReName();
				break;
			case CHDIR:
				ChDir();
				break;
			case RET:
				Ret();
				break;
			case MVFILE:
				MvFile();
				break;
			case SAVE_SHARE:
				SaveShare();
				break;	
			case SHARE_FILE:
				ShareFile();
				break;
			case USEREXIT:
				r =false;
				break;
			default:
				break;
            
        }

    }
}


int main()
{
    Client cli;
    if (!cli.Connect_Ser())
    {
        cout<<"连接服务器失败"<<endl;
        return 1;
    }

    cli.Run();

    return 0;
}