CUR_DIR := $(CURDIR)
TARGET = pizzahunt_script.out
pizzahunt_script.out: main.cpp md5.cpp
	@g++ $^ -o $@ -std=c++11 -ljsoncpp -lcurl -lpthread -lssl -lcrypto -lz || \
	{ \
		echo  "\033[31m编译失败，检查语法错误或者是因为未安装 libcurl 或 jsoncpp 开发库\033[0m"; \
		echo  "\033[31m安装方法: sudo apt install libcurl4-openssl-dev libjsoncpp-dev\033[0m"; \
		exit 1; \
	}
	@ls pizzahunt_script.out
clean:
	@rm -f $(TARGET)

run:
	@touch log.txt
	@touch may_be_true_passwd.txt
	@daemonize -o $(CUR_DIR)/log.txt -e $(CUR_DIR)/log.txt -c $(CUR_DIR) $(CUR_DIR)/$(TARGET)|| \
	{ \
		echo  "\033[31m运行失败,可能是因为未安装daemonize软件\033[0m"; \
		echo  "\033[31m安装方法: sudo apt install daemonize\033[0m"; \
		exit 1; \
	}
	
	@ps ajx | head -1 && ps ajx | grep pizzahunt_script | grep -v grep
	@echo "\033[32m成功运行守护进程,如果是在云服务器上运行的,现在可以关闭远程连接,脚本会在后台自动执行\033[0m"

view_tail_log:
	@cat log.txt | tail -10
view_process:
	@ps ajx | head -1 && ps ajx | grep pizzahunt_script | grep -v grep
